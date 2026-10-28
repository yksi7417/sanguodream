/* hexdom.dll: the game. Unity build of ecs/, sim/, engine/ and ui/. Owns no
   state outside the memory block handed in by the platform layer, so the
   platform can unload and reload this DLL while the game keeps running. */

#include "base.h"
#include "hexdom.h"
#include "imgui.h"

#include "ecs/ecs.h"
#include "ecs/ecs.cpp"
#include "sim/sim.h"
#include "sim/sim.cpp"
#include "engine/engine.h"
#include "engine/engine.cpp"
#include "text/text.h"
#include "text/text.cpp"
#include "ui/ui.h"
#include "ui/ui.cpp"

/* The locales of the front end. HEXDOM_DATA_ROOT and HEXDOM_CONTENT_PACK
   are in hexdom.h, because the platform layer loads the fonts from the
   same folder. */
#define HEXDOM_DEFAULT_LOCALE   "zh-TW"
#define HEXDOM_FALLBACK_LOCALE  "en"
#define HEXDOM_TEXT_ARENA_SIZE  Megabytes(4)

struct game_state
{
    memory_arena permanent;      /* everything below game_state in the permanent block */
    memory_arena transient;
    memory_arena engine_arena;   /* the rest of permanent after the text: the engine. A new game of another pack empties it (docs/MAPS.md) */
    engine      *backend;
    engine_api   api;            /* function pointers into THIS DLL image: rebuilt on reload */
    u32          reload_count;
    text_catalog text;           /* the UI strings and the names of the active locale */
    char         content_folder[260];   /* the folder of the rules and the map: game_memory.content_pack, or the default */
    ui_state     ui;
};

/* Creates the engine in its own arena with the rules and the map of a pack.
   pack "" is the default pack, HEXDOM_CONTENT_PACK. When the player chooses
   a scenario of another pack, the game calls it again: the old engine and
   its memory go away. */
internal void
game_create_engine(game_state *game, game_memory *memory, const char *pack)
{
    arena_reset(&game->engine_arena);
    arena_reset(&game->transient);
    engine_config config = engine_default_config();
    config.read_entire_file  = memory->platform.read_entire_file;
    config.write_entire_file = memory->platform.write_entire_file;
    if (pack && pack[0])
    {
        snprintf(game->content_folder, sizeof(game->content_folder), "%s/%s", HEXDOM_DATA_ROOT, pack);
        config.content_folder = game->content_folder;
    }
    game->backend = engine_create(&game->engine_arena, &game->transient, config);
    game->api     = engine_make_api(game->backend);
}

/* The UI at the title menu. A new engine starts a new UI too, because the
   views of the UI hold the ids and the ledger marks of the old engine. The
   font size of the player stays. */
internal void
game_start_ui(game_state *game)
{
    int font_size = game->ui.font_size;
    b32 had_ui    = font_size > 0;
    ui_init(&game->ui, &game->text);
    if (had_ui) game->ui.font_size = font_size;
    game->ui.show_title = true;   /* M9: the game starts at the title menu */
    game->ui.hud_mode   = true;   /* the game mode: no debug panels (F12 shows them) */
    game->ui.feel.original_timing = true;   /* M17: the rider, the load transition */
    game->ui.feel.turn_messages   = true;   /* M17: the message window and the turns of the AI */
    game->ui.show_officers = game->ui.show_cities = game->ui.show_city = false;   /* they open on demand */
}

HEXDOM_EXPORT
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    /* ImGui globals are per module: bind this DLL's copy to the exe's context
       and allocators before any ImGui call. */
    ImGui::SetCurrentContext(imgui);
    ImGui::SetAllocatorFunctions(memory->platform.imgui_alloc,
                                 memory->platform.imgui_free,
                                 memory->platform.imgui_alloc_user);

    Assert(sizeof(game_state) <= memory->permanent_size);
    game_state *game = (game_state *)memory->permanent;

    if (!memory->is_initialized)
    {
        arena_init(&game->permanent,
                   (u8 *)memory->permanent + sizeof(game_state),
                   memory->permanent_size - sizeof(game_state));
        arena_init(&game->transient, memory->transient, memory->transient_size);

        game->reload_count = 0;
        text_catalog_init(&game->text, arena_sub(&game->permanent, HEXDOM_TEXT_ARENA_SIZE),
                          memory->platform.read_entire_file, HEXDOM_DATA_ROOT, HEXDOM_CONTENT_PACK,
                          HEXDOM_FALLBACK_LOCALE);
        text_catalog_set_locale(&game->text, HEXDOM_DEFAULT_LOCALE);
        memory->platform.log(text_catalog_has_strings(&game->text) ? "hexdom: locale zh-TW loaded"
                                                                   : "hexdom: no strings file for zh-TW; the UI shows ids");
        game->engine_arena = arena_sub(&game->permanent, game->permanent.size - game->permanent.used);
        game_create_engine(game, memory, memory->content_pack);
        game_start_ui(game);   /* the platform gives a zeroed block: font_size is 0, so the UI takes its default */

        memory->platform.log("hexdom: game state initialized");
        memory->is_initialized = true;
    }

    if (input->dll_reloaded)
    {
        /* The old DLL image is gone; the api table pointed into it. */
        game->api = engine_make_api(game->backend);
        game->reload_count++;
        memory->platform.log("hexdom: dll reloaded");
    }

    ui_frame_info frame_info;
    frame_info.dt_seconds       = input->dt_seconds;
    frame_info.dll_reload_count = game->reload_count;
    frame_info.frame_arena_used = frame->used;
    frame_info.frame_arena_size = frame->size;
    frame_info.render           = input->render;
    frame_info.frame_arena      = frame;
    frame_info.portraits        = memory->platform.portraits;
    frame_info.portrait_texture = memory->platform.portrait_texture;
    frame_info.face_texture     = memory->platform.face_texture;
    frame_info.net_send         = memory->platform.net_send;
    frame_info.net_poll         = memory->platform.net_poll;
    ui_draw(&game->ui, &game->api, &frame_info);
    if (game->ui.game.pack_request[0])
    {
        /* The player chose a scenario of another pack: a new engine with its rules
           and its map, then the new UI loads the scenario (ui_start_pack_scenario). */
        char pack[CONTENT_KEY_SIZE], scenario[CONTENT_KEY_SIZE];
        snprintf(pack, sizeof(pack), "%s", game->ui.game.pack_request);
        snprintf(scenario, sizeof(scenario), "%s", game->ui.game.pack_scenario);
        game_create_engine(game, memory, pack);
        game_start_ui(game);
        snprintf(game->ui.game.pack_scenario, sizeof(game->ui.game.pack_scenario), "%s", scenario);
        memory->platform.log(game->api.map_info(game->api.engine).real_map ? "hexdom: the engine started again with another pack"
                                                                           : "hexdom: the other pack has no map");
    }
    /* The request lasts one frame. On the web the user can cancel it, and then ask again. */
    memory->quit_requested = game->ui.quit_requested;
    game->ui.quit_requested = false;
    memory->font_pixels    = (u32)game->ui.font_size;
    memory->permanent_used = sizeof(game_state) + HEXDOM_TEXT_ARENA_SIZE + game->engine_arena.used;
    memory->transient_used = game->transient.used;
}
