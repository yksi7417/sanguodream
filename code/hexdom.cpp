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
#include "ui/ui.h"
#include "ui/ui.cpp"

struct game_state
{
    memory_arena permanent;      /* everything below game_state in the permanent block */
    memory_arena transient;
    engine      *backend;
    engine_api   api;            /* function pointers into THIS DLL image: rebuilt on reload */
    u32          reload_count;
    ui_state     ui;
};

extern "C" __declspec(dllexport)
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

        engine_config config = engine_default_config();
        config.read_entire_file = memory->platform.read_entire_file;
        game->backend = engine_create(&game->permanent, &game->transient, config);
        game->api     = engine_make_api(game->backend);
        game->reload_count = 0;
        ui_init(&game->ui);

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
    ui_draw(&game->ui, &game->api, &frame_info);
}
