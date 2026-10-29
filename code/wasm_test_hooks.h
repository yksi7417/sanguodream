#ifndef WASM_TEST_HOOKS_H
#define WASM_TEST_HOOKS_H
/* The test hooks of the web build, for the Playwright smoke tests in
   tests/web (docs/WEB.md). Only "build_web.bat test-game" compiles them
   (HEXDOM_WEB_TEST). The game of the site has none of them.

   A browser test can only click pixels of the canvas. These hooks tell the
   test where to click:
   - The ImGui item hooks record the rect of each item with a stable id
     ("label###id") in each frame. wasm_test_item gives the rect of the
     last frame.
   - wasm_test_hex_screen gives the screen point of a hex.
   The hooks also make a known start for the battle test, through the
   editor functions of engine_api, as the native demos do. The test then
   moves and attacks through the real UI. */

#include <stdio.h>
#include <string.h>
#include "imgui_internal.h"   /* ImGuiContext, ImGuiWindow and the item hooks */

#define WASM_TEST_MAX_ITEMS  4096
#define WASM_TEST_ID_SIZE    64
#define WASM_TEST_REPLY_SIZE 4096
#define WASM_TEST_RECENT     64      /* the last item rects of ItemAdd */

struct wasm_test_item_rect
{
    char id[WASM_TEST_ID_SIZE];
    f32  x0, y0, x1, y1;
};

struct wasm_test_items
{
    wasm_test_item_rect rows[WASM_TEST_MAX_ITEMS];
    u32                 count;
};

global_variable wasm_test_items g_test_items[2];   /* the frame that ImGui builds, and the last frame */
global_variable u32             g_test_building;
global_variable char            g_test_reply[WASM_TEST_REPLY_SIZE];

struct wasm_test_recent
{
    ImGuiID id;
    ImRect  rect;            /* the part inside the clip rect of the window */
};

global_variable wasm_test_recent g_test_recent[WASM_TEST_RECENT];
global_variable u32              g_test_recent_next;

/* ---- the ImGui item hooks (IMGUI_ENABLE_TEST_ENGINE) ---------------------- */

/* ImGui calls ItemInfo after ItemAdd, but a tab adds other items between
   the two calls. Thus this hook keeps the last rects for ItemInfo. The
   rect is the part inside the clip rect of the window, because a list can
   scroll. */
void
ImGuiTestEngineHook_ItemAdd(ImGuiContext *ctx, ImGuiID id, const ImRect &bb, const ImGuiLastItemData *item_data)
{
    (void)item_data;
    if (id == 0) return;
    ImRect rect = bb;
    if (ctx->CurrentWindow) rect.ClipWith(ctx->CurrentWindow->ClipRect);
    wasm_test_recent *recent = &g_test_recent[g_test_recent_next++ % WASM_TEST_RECENT];
    recent->id   = id;
    recent->rect = rect;
}

/* Only a label with a stable id is kept. */
void
ImGuiTestEngineHook_ItemInfo(ImGuiContext *ctx, ImGuiID id, const char *label, ImGuiItemStatusFlags flags)
{
    (void)ctx; (void)flags;
    if (!label) return;
    const char *stable = strstr(label, "###");
    if (!stable) return;
    wasm_test_items *items = &g_test_items[g_test_building];
    if (items->count >= WASM_TEST_MAX_ITEMS) return;
    ImRect rect;
    b32 found = false;
    for (u32 back = 1; back <= WASM_TEST_RECENT && !found; ++back)
    {
        wasm_test_recent *recent = &g_test_recent[(g_test_recent_next - back) % WASM_TEST_RECENT];
        if (recent->id == id) { rect = recent->rect; found = true; }
    }
    if (!found) return;
    if (rect.GetWidth() <= 0.0f || rect.GetHeight() <= 0.0f) return;
    wasm_test_item_rect *row = &items->rows[items->count++];
    snprintf(row->id, sizeof(row->id), "%s", stable + 3);
    row->x0 = rect.Min.x; row->y0 = rect.Min.y;
    row->x1 = rect.Max.x; row->y1 = rect.Max.y;
}

void
ImGuiTestEngineHook_Log(ImGuiContext *ctx, const char *fmt, ...)
{
    (void)ctx; (void)fmt;
}

const char *
ImGuiTestEngine_FindItemDebugLabel(ImGuiContext *ctx, ImGuiID id)
{
    (void)ctx; (void)id;
    return 0;
}

/* Before NewFrame: ImGui calls the item hooks in this frame. */
internal void
wasm_test_begin_frame(ImGuiContext *imgui)
{
    imgui->TestEngineHookItems = true;
    g_test_items[g_test_building].count = 0;
}

/* After the frame: the new frame becomes the last frame. */
internal void
wasm_test_end_frame(void)
{
    g_test_building ^= 1;
}

internal game_state *
wasm_test_game(void)
{
    return (game_state *)g_wasm.memory.permanent;
}

/* ---- the functions of the tests ------------------------------------------- */

/* The rect of the item "...###id" in the last frame, as JSON, or "null". */
extern "C" EMSCRIPTEN_KEEPALIVE const char *
wasm_test_item(const char *id)
{
    wasm_test_items *items = &g_test_items[g_test_building ^ 1];
    for (u32 index = items->count; index-- > 0;)
    {
        wasm_test_item_rect *row = &items->rows[index];
        if (strcmp(row->id, id) == 0)
        {
            snprintf(g_test_reply, sizeof(g_test_reply), "{\"x0\":%.1f,\"y0\":%.1f,\"x1\":%.1f,\"y1\":%.1f}",
                     row->x0, row->y0, row->x1, row->y1);
            return g_test_reply;
        }
    }
    return "null";
}

/* The stable ids of the last frame that start with the prefix, as a JSON list. */
extern "C" EMSCRIPTEN_KEEPALIVE const char *
wasm_test_item_ids(const char *prefix)
{
    wasm_test_items *items = &g_test_items[g_test_building ^ 1];
    size_t prefix_length = strlen(prefix);
    u32 used = (u32)snprintf(g_test_reply, sizeof(g_test_reply), "[");
    for (u32 index = 0; index < items->count; ++index)
    {
        if (strncmp(items->rows[index].id, prefix, prefix_length) != 0) continue;
        if (used + WASM_TEST_ID_SIZE + 4 >= sizeof(g_test_reply)) break;
        used += (u32)snprintf(g_test_reply + used, sizeof(g_test_reply) - used, "%s\"%s\"", used > 1 ? "," : "", items->rows[index].id);
    }
    snprintf(g_test_reply + used, sizeof(g_test_reply) - used, "]");
    return g_test_reply;
}

/* 1 if the window "###name" showed in the last frame. */
extern "C" EMSCRIPTEN_KEEPALIVE int
wasm_test_window(const char *stable_name)
{
    char name[WASM_TEST_ID_SIZE + 4];
    snprintf(name, sizeof(name), "###%s", stable_name);
    ImGuiWindow *window = ImGui::FindWindowByName(name);
    return window && window->Active && !window->Hidden;
}

/* The camera goes to the hex, as ui_frame_hex does for a list. */
extern "C" EMSCRIPTEN_KEEPALIVE void
wasm_test_frame_hex(int q, int r)
{
    game_state *game = wasm_test_game();
    ui_frame_hex(&game->ui, &game->api, hex_make(q, r));
    game->ui.scene.cam.target.y = 0.0f;
}

/* The screen point of a hex, as JSON, or "null" if it is off the screen. */
extern "C" EMSCRIPTEN_KEEPALIVE const char *
wasm_test_hex_screen(int q, int r)
{
    game_state *game = wasm_test_game();
    ImVec2 display = ImGui::GetIO().DisplaySize;
    m4 view_projection = camera_view_projection(&game->ui.scene.cam, display.x, display.y);
    f32 x = 0.0f, y = 0.0f;
    if (!ui_scene_screen(&game->api, view_projection, hex_make(q, r), 0.0f, &x, &y)) return "null";
    snprintf(g_test_reply, sizeof(g_test_reply), "{\"x\":%.1f,\"y\":%.1f}", x, y);
    return g_test_reply;
}

/* The camera target, the selected city, the selected unit and the size of the map, as JSON. */
extern "C" EMSCRIPTEN_KEEPALIVE const char *
wasm_test_state(void)
{
    game_state *game = wasm_test_game();
    engine_api *api  = &game->api;
    engine_map_info map = api->map_info(api->engine);
    engine_multiplayer_info shared = api->multiplayer_info(api->engine);   /* MP3: the tests of a shared game */
    u64 humans = 0;                                                         /* MP4: a bit for each human faction */
    for (u32 index = 0; index < api->faction_count(api->engine); ++index)
    {
        engine_faction faction = api->faction_at(api->engine, index);
        if (faction.human && faction.force_id < 64) humans |= 1ull << faction.force_id;
    }
    snprintf(g_test_reply, sizeof(g_test_reply),
             "{\"camera_x\":%.2f,\"camera_z\":%.2f,\"city\":%u,\"unit\":%u,\"turn\":%u,\"whose_turn\":%u,\"map_width\":%u,\"map_height\":%u,"
             "\"shared\":%d,\"local_force\":%u,\"local_acts\":%d,\"steps\":%u,\"checksum\":\"%016llx\",\"net_error\":\"%s\","
             "\"humans\":\"%016llx\",\"timer\":%d,\"pack\":\"%s\"}",
             game->ui.scene.cam.target.x, game->ui.scene.cam.target.z, (u32)game->ui.city.selected,
             game->ui.units.selected, api->turn(api->engine), (u32)api->whose_turn(api->engine), map.width, map.height,
             shared.shared ? 1 : 0, (u32)shared.local_force, shared.local_acts ? 1 : 0, shared.step_count,
             (unsigned long long)api->state_checksum(api->engine), game->ui.net.error, (unsigned long long)humans,
             game->ui.net.timer_on ? (game->ui.net.timer_paused ? -1 : (int)game->ui.net.timer_seconds) : -2,
             api->content_pack(api->engine).pack);
    return g_test_reply;
}

/* MP3: the faction of the local seat ends its turn, as 進行 of the map menu
   does. The step goes to the room in the same frame (ui_multiplayer.cpp).
   The reply is the command id; 0 when the engine refused it. */
extern "C" EMSCRIPTEN_KEEPALIVE int
wasm_test_end_turn(void)
{
    game_state *game = wasm_test_game();
    return (int)game->api.end_turn(game->api.engine);
}

/* A unit of the player two hexes from an enemy base: the test moves it
   beside the base and attacks. The base gets 1 durability, so the attack
   takes it. The reply is JSON: the unit, the hex of the unit, the hex to
   move to and the base. */
extern "C" EMSCRIPTEN_KEEPALIVE const char *
wasm_test_battle_setup(void)
{
    game_state *game = wasm_test_game();
    engine_api *api  = &game->api;
    void       *e    = api->engine;
    u8 player = api->whose_turn(e);
    /* The first city of the player that can send a unit: a city needs an officer. */
    u32 refusal = SIM_REFUSAL_NO_CITY;
    for (u32 index = 0; index < api->city_count(e) && refusal != SIM_REFUSAL_NONE; ++index)
    {
        if (api->city_at(e, index).owner_force_id != player) continue;
        u8 city = (u8)index;
        api->set_city_resource(e, city, SIM_RESOURCE_SOLDIERS, 0, 20000);
        api->set_city_resource(e, city, SIM_RESOURCE_WEAPONS, 0, 20000);
        api->set_city_resource(e, city, SIM_RESOURCE_GRAIN, 0, 60000);
        engine_sortie_order order = api->sortie_proposal(e, city, 1);
        refusal = api->command_sortie(e, &order).refusal;
    }
    if (refusal != SIM_REFUSAL_NONE)
    {
        snprintf(g_test_reply, sizeof(g_test_reply), "{\"error\":\"no city of the player can send a unit: %u\"}", refusal);
        return g_test_reply;
    }
    u32 unit_id = ENGINE_NO_UNIT;
    for (u32 id = 0; id < api->unit_count(e); ++id)
    {
        engine_unit unit = api->unit_at(e, id);
        if (unit.id != ENGINE_NO_UNIT && unit.force_id == player && !unit.acted) unit_id = id;
    }
    if (unit_id == ENGINE_NO_UNIT) return "{\"error\":\"the sortie made no unit\"}";
    u32 enemy_bases = 0;

    for (u32 index = 0; index < api->base_count(e); ++index)
    {
        engine_base base = api->base_at(e, index);
        if (base.force == player || base.force == ENGINE_NO_FACTION) continue;
        ++enemy_bases;
        hex_coord target = hex_make(base.q, base.r);
        for (u32 direction = 0; direction < HEX_DIRECTION_COUNT; ++direction)
        {
            hex_coord beside = hex_neighbor(target, direction);
            hex_coord start  = hex_neighbor(beside, direction);
            if (api->unit_at_hex(e, beside.q, beside.r) != ENGINE_NO_UNIT) continue;
            if (api->base_at_hex(e, beside.q, beside.r) != ENGINE_NO_BASE) continue;
            if (api->base_at_hex(e, start.q, start.r) != ENGINE_NO_BASE) continue;
            /* A place on the hex beside the base tells that a unit can stand there. */
            if (!api->set_unit_place(e, unit_id, player, beside.q, beside.r)) continue;
            if (!api->set_unit_place(e, unit_id, player, start.q, start.r)) continue;
            api->set_base(e, index, base.force, 100, 1);
            snprintf(g_test_reply, sizeof(g_test_reply),
                     "{\"unit\":%u,\"start_q\":%d,\"start_r\":%d,\"beside_q\":%d,\"beside_r\":%d,"
                     "\"base\":%u,\"base_q\":%d,\"base_r\":%d,\"player\":%u}",
                     unit_id, start.q, start.r, beside.q, beside.r, index, base.q, base.r, (u32)player);
            return g_test_reply;
        }
    }
    snprintf(g_test_reply, sizeof(g_test_reply), "{\"error\":\"no free place beside %u enemy bases of %u\"}", enemy_bases, api->base_count(e));
    return g_test_reply;
}

/* A unit and a base, as JSON, for the checks of the battle test. */
extern "C" EMSCRIPTEN_KEEPALIVE const char *
wasm_test_battle_state(int unit_id, int base_index)
{
    game_state *game = wasm_test_game();
    engine_api *api  = &game->api;
    engine_unit unit = api->unit_at(api->engine, (u32)unit_id);
    engine_base base = api->base_at(api->engine, (u32)base_index);
    snprintf(g_test_reply, sizeof(g_test_reply),
             "{\"unit_q\":%d,\"unit_r\":%d,\"acted\":%u,\"base_force\":%u,\"base_durability\":%u}",
             unit.q, unit.r, (u32)unit.acted, (u32)base.force, base.durability);
    return g_test_reply;
}

#endif /* WASM_TEST_HOOKS_H */
