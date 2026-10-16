/* hexdom.dll: the game. Unity build of sim/, ecs/ and ui/. Owns no state
   outside the memory block handed in by the platform layer, so the platform
   can unload and reload this DLL while the game keeps running. */

#include <stdio.h>   /* snprintf for UI labels only */

#include "base.h"
#include "hexdom.h"
#include "imgui.h"

#include "ecs/ecs.h"
#include "ecs/ecs.cpp"
#include "sim/sim.h"
#include "sim/sim.cpp"
#include "ui/ui.h"

struct game_state
{
    memory_arena permanent;      /* everything below game_state in the permanent block */
    memory_arena transient;
    sim_state   *sim;
    sim_event   *ledger;         /* every event the sim has emitted this session */
    u32          ledger_count;
    u32          ledger_capacity;
    u32          reload_count;
    ui_state     ui;
};

internal void
ledger_append(game_state *game, sim_event_sink *sink)
{
    for (u32 i = 0; i < sink->count; ++i)
    {
        if (game->ledger_count < game->ledger_capacity)
        {
            game->ledger[game->ledger_count++] = sink->events[i];
        }
    }
}

#include "ui/ui.cpp"

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

        game->sim = PushStruct(&game->permanent, sim_state);
        sim_init(game->sim, &game->permanent, 0x5EEDull);

        game->ledger_capacity = 65536;
        game->ledger_count    = 0;
        game->ledger          = PushArray(&game->permanent, game->ledger_capacity, sim_event);

        game->reload_count = 0;
        ui_init(&game->ui);

        memory->platform.log("hexdom: game state initialized");
        memory->is_initialized = true;
    }

    if (input->dll_reloaded)
    {
        game->reload_count++;
        memory->platform.log("hexdom: dll reloaded");
    }

    ui_draw(game, input, frame);
}
