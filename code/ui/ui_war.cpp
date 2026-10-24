/* The feedback of war (docs/tutorials/18-battle.md, "The feedback of war").
   The choice of the user (2026-10-22). A unit of the player walks hex by
   hex with a pause on each hex. Each class of unit has its own sprite. An
   attack shows an effect on the target hex for a few seconds. The change
   of the soldiers or the morale rises as a value over the unit and then
   marks the plate. The UI reads the changes from the ledger through
   engine_api. The sim stays instant.
   - The walk: ui_unit_click plans the path with path_to before its
     command_move and starts the walk after it. The scene and the plates
     draw the unit on the hex of the walk (ui_round_unit_hex).
   - A strike: ui_combat_click pushes one on the target hex of 攻擊, 齊攻,
     戰法, 計略 and 單挑. A hit of an enemy on a unit pushes one too.
   - A hit: each SIM_EVENT_UNIT_DAMAGED, UNIT_SOLDIERS_LOST, UNIT_DESERTED,
     UNIT_SUPPLIED and MORALE_CHANGED of the ledger. The show of the AI
     round raises the hits of the AI factions in its own time.
   Included by ui.cpp after ui_round.cpp. */

#include <string.h>   /* strcmp for the sprite of a unit class */

#define UI_HIT_RISE          48.0f   /* pixels that a value rises */
#define UI_HIT_STACK         18.0f   /* pixels between two values of one unit */
#define UI_STRIKE_MIN_RADIUS 10.0f
#define UI_STRIKE_MAX_RADIUS 120.0f

internal void
ui_war_init(ui_state *ui)
{
    ui->war = {};
    ui->war.walk.unit = ENGINE_NO_UNIT;
}

/* ---- the walk of a unit of the player ----------------------------------- */

/* Keeps the hex of the unit and the plan of path_to to the goal, before
   the command_move. ui_walk_begin trims the plan to the hex that the unit
   reached. */
internal void
ui_walk_plan(ui_state *ui, engine_api *api, u32 unit_id, s32 q, s32 r)
{
    engine_unit unit = api->unit_at(api->engine, unit_id);
    ui->war.walk.unit    = ENGINE_NO_UNIT;
    ui->war.walk.planned = false;
    ui->war.walk.count   = 0;
    if (unit.id == ENGINE_NO_UNIT) return;
    engine_path_view path = api->path_to(api->engine, unit_id, q, r);
    ui->war.walk.cells[0] = hex_make(unit.q, unit.r);
    ui->war.walk.count    = 1;
    for (u32 index = 0; index < path.count && ui->war.walk.count < UI_WALK_CAPACITY; ++index)
    {
        ui->war.walk.cells[ui->war.walk.count++] = path.cells[index];
    }
    ui->war.walk.unit    = unit_id;
    ui->war.walk.planned = true;
}

internal void
ui_walk_cancel(ui_state *ui)
{
    ui->war.walk.unit    = ENGINE_NO_UNIT;
    ui->war.walk.planned = false;
    ui->war.walk.count   = 0;
}

/* True while the walk of a unit shows. */
internal b32
ui_walk_active(ui_state *ui)
{
    return ui->war.walk.unit != ENGINE_NO_UNIT && !ui->war.walk.planned && ui->war.walk.count >= 2;
}

/* After a command_move: the walk goes from the start hex along the plan to
   the hex that the unit reached. Without a plan to that hex, the unit
   shows at its new hex with no walk. With open_menu, the command menu
   opens when the walk ends. */
internal void
ui_walk_begin(ui_state *ui, engine_api *api, u32 unit_id, bool open_menu)
{
    engine_unit unit = api->unit_at(api->engine, unit_id);
    b32 planned = ui->war.walk.planned && ui->war.walk.unit == unit_id && unit.id != ENGINE_NO_UNIT;
    u32 reached = 0;
    for (u32 index = 1; planned && index < ui->war.walk.count; ++index)
    {
        if (hex_equal(ui->war.walk.cells[index], hex_make(unit.q, unit.r))) reached = index;
    }
    if (!reached)
    {
        ui_walk_cancel(ui);
        if (open_menu) ui->show_unit_menu = unit.id != ENGINE_NO_UNIT;
        return;
    }
    ui->war.walk.count     = reached + 1;
    ui->war.walk.step      = 0;
    ui->war.walk.wait      = UI_WALK_HEX_SECONDS;
    ui->war.walk.planned   = false;
    ui->war.walk.open_menu = open_menu;
    ui->war.walk.has_strike = false;
}

/* The hex of the walk for a unit, or false when the unit does not walk. */
internal b32
ui_walk_hex(ui_state *ui, u32 unit_id, hex_coord *out)
{
    if (!ui_walk_active(ui) || ui->war.walk.unit != unit_id) return false;
    u32 step = ui->war.walk.step < ui->war.walk.count ? ui->war.walk.step : ui->war.walk.count - 1;
    *out = ui->war.walk.cells[step];
    return true;
}

internal void
ui_walk_update(ui_state *ui, engine_api *api, f32 dt_seconds)
{
    if (!ui_walk_active(ui)) return;
    ui->war.walk.wait -= dt_seconds;
    while (ui->war.walk.wait <= 0.0f)
    {
        ui->war.walk.step++;
        if (ui->war.walk.step + 1 >= ui->war.walk.count)
        {
            u32 unit_id = ui->war.walk.unit;
            bool open_menu = ui->war.walk.open_menu;
            bool has_strike = ui->war.walk.has_strike;
            u8 strike_kind = ui->war.walk.strike_kind;
            hex_coord strike_hex = ui->war.walk.strike_hex;
            ui_walk_cancel(ui);
            if (open_menu && ui->units.selected == unit_id) ui->show_unit_menu = api->unit_at(api->engine, unit_id).id != ENGINE_NO_UNIT;
            if (has_strike) ui_strike_push(ui, strike_kind, strike_hex);   /* G2-045: the action after the move */
            return;
        }
        ui->war.walk.wait += UI_WALK_HEX_SECONDS;
    }
}

/* ---- the strikes --------------------------------------------------------- */

/* A strike on a hex. A second strike on the same hex restarts it. */
internal void
ui_strike_push(ui_state *ui, u8 kind, hex_coord hex)
{
    for (u32 index = 0; index < ui->war.strike_count; ++index)
    {
        if (!hex_equal(ui->war.strikes[index].hex, hex)) continue;
        ui->war.strikes[index].kind    = kind;
        ui->war.strikes[index].seconds = UI_STRIKE_SECONDS;
        return;
    }
    if (ui->war.strike_count >= UI_STRIKE_CAPACITY)   /* the oldest goes */
    {
        for (u32 index = 1; index < ui->war.strike_count; ++index) ui->war.strikes[index - 1] = ui->war.strikes[index];
        ui->war.strike_count--;
    }
    u32 place = ui->war.strike_count++;
    ui->war.strikes[place].kind    = kind;
    ui->war.strikes[place].hex     = hex;
    ui->war.strikes[place].seconds = UI_STRIKE_SECONDS;
}

/* The strike of a pick of the unit menu, or UI_STRIKE_KIND_COUNT for a
   pick without one (設置, 修補, 撤除). */
internal u8
ui_strike_of_pick(u8 pick)
{
    switch (pick)
    {
        case UI_PICK_ATTACK: case UI_PICK_JOINT: case UI_PICK_DUEL: return (u8)UI_STRIKE_ATTACK;
        case UI_PICK_TACTIC: return (u8)UI_STRIKE_TACTIC;
        case UI_PICK_PLOT:   return (u8)UI_STRIKE_PLOT;
        default:             return (u8)UI_STRIKE_KIND_COUNT;
    }
}

/* ---- the hits ------------------------------------------------------------ */

internal void
ui_hit_push(ui_state *ui, u32 unit_id, u8 kind, s32 delta)
{
    if (!delta) return;
    if (ui->war.hit_count >= UI_HIT_CAPACITY)   /* the oldest goes */
    {
        for (u32 index = 1; index < ui->war.hit_count; ++index) ui->war.hits[index - 1] = ui->war.hits[index];
        ui->war.hit_count--;
    }
    u32 place = ui->war.hit_count++;
    ui->war.hits[place].unit    = unit_id;
    ui->war.hits[place].kind    = kind;
    ui->war.hits[place].delta   = delta;
    ui->war.hits[place].seconds = UI_HIT_FLOAT_SECONDS + UI_HIT_MARK_SECONDS;
}

/* The kind of hit of an event, or false for an event that is no hit. */
internal b32
ui_hit_kind_of_event(sim_event_kind kind, u8 *out)
{
    switch (kind)
    {
        case SIM_EVENT_UNIT_DAMAGED: case SIM_EVENT_UNIT_SOLDIERS_LOST: case SIM_EVENT_UNIT_DESERTED: case SIM_EVENT_UNIT_SUPPLIED:
            *out = (u8)UI_HIT_SOLDIERS; return true;
        case SIM_EVENT_MORALE_CHANGED:
            *out = (u8)UI_HIT_MORALE; return true;
        default:
            return false;
    }
}

/* One event of the ledger. A hit of a unit rises. The damage of an enemy
   on a unit shows a strike on its hex too. Returns true if the event gave
   a hit. */
internal b32
ui_war_take_event(ui_state *ui, engine_api *api, const sim_event *event, u8 player)
{
    u8 kind = 0;
    if (!ui_hit_kind_of_event(event->kind, &kind) || !event->delta) return false;
    ui_hit_push(ui, event->detail, kind, event->delta);
    if (event->kind == SIM_EVENT_UNIT_DAMAGED && event->faction != player)
    {
        engine_unit unit = api->unit_at(api->engine, event->detail);
        if (unit.id != ENGINE_NO_UNIT) ui_strike_push(ui, (u8)UI_STRIKE_ATTACK, hex_make(unit.q, unit.r));
    }
    return true;
}

/* The change that the plate of a unit marks now: the newest hit of the
   kind in its mark phase, else 0. */
internal s32
ui_hit_mark(ui_state *ui, u32 unit_id, u8 kind)
{
    s32 mark = 0;
    for (u32 index = 0; index < ui->war.hit_count; ++index)
    {
        if (ui->war.hits[index].unit != unit_id || ui->war.hits[index].kind != kind) continue;
        if (ui->war.hits[index].seconds <= UI_HIT_MARK_SECONDS) mark = ui->war.hits[index].delta;
    }
    return mark;
}

/* The colour of a marked value on a plate or a panel: red for a loss,
   green for a gain, else the given colour. */
internal ImU32
ui_hit_colour(s32 mark, ImU32 plain)
{
    if (mark < 0) return IM_COL32(255, 110, 90, 255);
    if (mark > 0) return IM_COL32(120, 230, 120, 255);
    return plain;
}

/* ---- the sprite of a unit ------------------------------------------------ */

/* True when the terrain of the hex carries ships and no foot soldier: a
   river or the sea (the rule of the sim for a 走舸). */
internal b32
ui_hex_is_water(engine_api *api, s32 q, s32 r)
{
    engine_hex hex = api->hex_at(api->engine, q, r);
    if (!hex.on_map) return false;
    engine_terrain_kind terrain = api->terrain_kind_at(api->engine, hex.terrain);
    return terrain.cost[MOVEMENT_NAVY] && !terrain.cost[MOVEMENT_FOOT];
}

/* The render_sprite of a unit from the key of its class (unit_types.tsv).
   A class without a picture of its own gets the figure of sprite 0. A land
   unit on water is a 走舸 (18-battle.md), so it shows the boat, not its
   weapon. */
internal u32
ui_unit_sprite(engine_api *api, engine_unit *unit)
{
    engine_unit_type type = api->unit_type_at(api->engine, unit->unit_type);
    if (type.id == ENGINE_NO_UNIT_TYPE) return RENDER_SPRITE_UNIT;
    if (type.movement != MOVEMENT_NAVY && ui_hex_is_water(api, unit->q, unit->r)) return RENDER_SPRITE_BOAT;
    if (!strcmp(type.key, "sword"))     return RENDER_SPRITE_SWORD;
    if (!strcmp(type.key, "pike"))      return RENDER_SPRITE_PIKE;
    if (!strcmp(type.key, "halberd"))   return RENDER_SPRITE_HALBERD;
    if (!strcmp(type.key, "crossbow"))  return RENDER_SPRITE_CROSSBOW;
    if (!strcmp(type.key, "cavalry"))   return RENDER_SPRITE_CAVALRY;
    if (!strcmp(type.key, "boat"))      return RENDER_SPRITE_BOAT;
    if (!strcmp(type.key, "transport")) return RENDER_SPRITE_TRANSPORT;
    if (!strcmp(type.key, "ram"))         return RENDER_SPRITE_RAM;
    if (!strcmp(type.key, "wood_beast"))  return RENDER_SPRITE_WOOD_BEAST;
    if (!strcmp(type.key, "tower"))       return RENDER_SPRITE_TOWER;
    if (!strcmp(type.key, "catapult"))    return RENDER_SPRITE_CATAPULT;
    if (!strcmp(type.key, "tower_ship"))  return RENDER_SPRITE_TOWER_SHIP;
    if (!strcmp(type.key, "battle_ship")) return RENDER_SPRITE_BATTLE_SHIP;
    return RENDER_SPRITE_UNIT;
}

/* ---- each frame ---------------------------------------------------------- */

/* The timers, and the new events of the ledger. During the show of the AI
   round, this reads only the events of the player. The show raises the
   hits of the AI factions in its own time (ui_round_play_event). */
internal void
ui_war_update(ui_state *ui, engine_api *api, f32 dt_seconds)
{
    ui_walk_update(ui, api, dt_seconds);
    u32 kept = 0;
    for (u32 index = 0; index < ui->war.strike_count; ++index)
    {
        ui->war.strikes[index].seconds -= dt_seconds;
        if (ui->war.strikes[index].seconds > 0.0f) ui->war.strikes[kept++] = ui->war.strikes[index];
    }
    ui->war.strike_count = kept;
    kept = 0;
    for (u32 index = 0; index < ui->war.hit_count; ++index)
    {
        ui->war.hits[index].seconds -= dt_seconds;
        if (ui->war.hits[index].seconds > 0.0f) ui->war.hits[kept++] = ui->war.hits[index];
    }
    ui->war.hit_count = kept;

    u32 count = api->ledger_count(api->engine);
    if (ui->war.ledger_mark > count) ui->war.ledger_mark = 0;   /* a load replaced the ledger */
    u8 player = ui_report_player(api);
    for (u32 index = ui->war.ledger_mark; index < count; ++index)
    {
        sim_event event = api->ledger_at(api->engine, index);
        if (ui->round.playing && event.faction != SIM_NO_FORCE && event.faction != player) continue;
        ui_war_take_event(ui, api, &event, player);
    }
    ui->war.ledger_mark = count;
}

/* ---- the draw ------------------------------------------------------------ */

/* The radius of a hex on the screen: half the distance to the centre of
   its first neighbour. */
internal f32
ui_hex_screen_radius(engine_api *api, m4 view_projection, hex_coord hex, f32 x, f32 y)
{
    ImGuiIO &io = ImGui::GetIO();
    v3 beside = ui_hex_world(api, hex_neighbor(hex, 0));
    beside.y += 0.3f;
    f32 bx = 0.0f, by = 0.0f;
    f32 radius = UI_STRIKE_MIN_RADIUS * 2.0f;
    if (camera_project(view_projection, beside, io.DisplaySize.x, io.DisplaySize.y, &bx, &by))
    {
        radius = sqrtf((bx - x) * (bx - x) + (by - y) * (by - y)) * 0.5f;
    }
    if (radius < UI_STRIKE_MIN_RADIUS) radius = UI_STRIKE_MIN_RADIUS;
    if (radius > UI_STRIKE_MAX_RADIUS) radius = UI_STRIKE_MAX_RADIUS;
    return radius;
}

/* One strike: a ring that grows and fades, and the mark of its kind. An
   attack shows three slashes that come one after the other. A tactic
   shows rays that turn. A plot shows two rings that pulse. */
internal void
ui_draw_strike(ImDrawList *draw, u8 kind, f32 x, f32 y, f32 radius, f32 progress)
{
    f32 fade = 1.0f - progress;
    ImU32 colours[UI_STRIKE_KIND_COUNT] = { IM_COL32(255, 70, 50, 255), IM_COL32(255, 190, 40, 255), IM_COL32(190, 90, 230, 255) };
    ImU32 colour = colours[kind < UI_STRIKE_KIND_COUNT ? kind : 0];
    ImU32 faded = (colour & 0x00FFFFFFu) | ((u32)(255.0f * fade) << 24);
    f32 ring = radius * (0.6f + 1.2f * progress);
    draw->AddCircle(ImVec2(x, y), ring, faded, 0, 2.5f);
    if (progress < 0.15f) draw->AddCircleFilled(ImVec2(x, y), radius * 0.9f, (colour & 0x00FFFFFFu) | ((u32)(140.0f * (1.0f - progress / 0.15f)) << 24));
    if (kind == UI_STRIKE_ATTACK)
    {
        for (u32 slash = 0; slash < 3; ++slash)
        {
            f32 start = 0.08f * (f32)slash;
            if (progress < start) continue;
            f32 part = (progress - start) / 0.25f;
            if (part > 1.0f) part = 1.0f;
            f32 angle = 0.9f + 0.5f * (f32)slash;
            f32 dx = cosf(angle) * radius * 1.1f, dy = sinf(angle) * radius * 1.1f;
            f32 ox = ((f32)slash - 1.0f) * radius * 0.35f;
            draw->AddLine(ImVec2(x - dx + ox, y - dy), ImVec2(x - dx + ox + 2.0f * dx * part, y - dy + 2.0f * dy * part), faded, 3.0f);
        }
    }
    else if (kind == UI_STRIKE_TACTIC)
    {
        for (u32 ray = 0; ray < 8; ++ray)
        {
            f32 angle = (f32)ray * 0.7854f + progress * 3.0f;
            f32 inner = radius * 0.4f, outer = radius * (0.9f + 0.6f * progress);
            draw->AddLine(ImVec2(x + cosf(angle) * inner, y + sinf(angle) * inner), ImVec2(x + cosf(angle) * outer, y + sinf(angle) * outer), faded, 2.0f);
        }
    }
    else
    {
        f32 pulse = 0.5f + 0.5f * sinf(progress * 12.0f);
        draw->AddCircle(ImVec2(x, y), radius * (0.3f + 0.4f * pulse), faded, 0, 2.0f);
        draw->AddCircle(ImVec2(x, y), radius * (0.9f - 0.3f * pulse), faded, 0, 1.5f);
    }
}

/* The hex where the scene draws a unit (the walk, then the show of the AI round). */
internal hex_coord ui_round_unit_hex(ui_state *ui, engine_unit *unit);

/* The strikes on their hexes and the values over the units, on top of the
   scene and the plates. */
internal void
ui_draw_war(ui_state *ui, engine_api *api)
{
    if (!ui->scene.enabled || !ui->scene.initialized) return;
    ImGuiIO &io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) return;
    m4 view_projection = camera_view_projection(&ui->scene.cam, io.DisplaySize.x, io.DisplaySize.y);
    ImDrawList *draw = ImGui::GetForegroundDrawList();
    for (u32 index = 0; index < ui->war.strike_count; ++index)
    {
        f32 x = 0.0f, y = 0.0f;
        hex_coord hex = ui->war.strikes[index].hex;
        if (!ui_scene_screen(api, view_projection, hex, 0.3f, &x, &y)) continue;
        f32 radius = ui_hex_screen_radius(api, view_projection, hex, x, y);
        ui_draw_strike(draw, ui->war.strikes[index].kind, x, y, radius, 1.0f - ui->war.strikes[index].seconds / UI_STRIKE_SECONDS);
    }
    for (u32 index = 0; index < ui->war.hit_count; ++index)
    {
        if (ui->war.hits[index].seconds <= UI_HIT_MARK_SECONDS) continue;   /* the mark phase: the plate shows it */
        engine_unit unit = api->unit_at(api->engine, ui->war.hits[index].unit);
        if (unit.id == ENGINE_NO_UNIT) continue;
        f32 x = 0.0f, y = 0.0f;
        if (!ui_scene_screen(api, view_projection, ui_round_unit_hex(ui, &unit), UI_UNIT_PLATE_LIFT, &x, &y)) continue;
        u32 stack = 0;   /* the earlier values of the same unit sit lower */
        for (u32 earlier = 0; earlier < index; ++earlier)
        {
            if (ui->war.hits[earlier].unit == unit.id && ui->war.hits[earlier].seconds > UI_HIT_MARK_SECONDS) stack++;
        }
        f32 progress = 1.0f - (ui->war.hits[index].seconds - UI_HIT_MARK_SECONDS) / UI_HIT_FLOAT_SECONDS;
        char value[64];
        snprintf(value, sizeof(value), "%+d %s", ui->war.hits[index].delta,
                 text_get(ui->text, ui->war.hits[index].kind == UI_HIT_MORALE ? "war.morale" : "war.soldiers"));
        ImVec4 colour = ui_report_colour((u8)(ui->war.hits[index].delta < 0 ? ENGINE_SIGN_BAD : ENGINE_SIGN_GOOD));
        colour.w = 1.0f - progress * 0.6f;
        f32 size = ImGui::GetFontSize() * 1.5f;
        ImVec2 extent = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, value);
        ImVec2 at(x - extent.x * 0.5f, y - 72.0f - progress * UI_HIT_RISE - UI_HIT_STACK * (f32)stack);
        draw->AddText(ImGui::GetFont(), size, ImVec2(at.x + 1.0f, at.y + 1.0f), IM_COL32(0, 0, 0, (int)(200.0f * colour.w)), value);
        draw->AddText(ImGui::GetFont(), size, at, ImGui::ColorConvertFloat4ToU32(colour), value);
    }
}
