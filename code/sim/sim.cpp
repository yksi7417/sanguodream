#include "sim/sim.h"

internal void
sim_init(sim_state *sim, memory_arena *arena, u64 seed)
{
    sim->rng = seed ? seed : 0x9E3779B97F4A7C15ull;
    sim->turn = 0;
    sim->next_command_id = 1;
    em_init(&sim->entities, arena, SIM_ENTITY_CAPACITY);
    sim->map.width = sim->map.height = 0;
    sim->map.cost = 0;
}

/* The real map's size and terrain come from spec-src/spec/formats/ once the
   scenario format is documented; until then callers pass a placeholder. */
internal void
sim_init_map(sim_state *sim, memory_arena *arena, u32 width, u32 height, u8 default_cost)
{
    hex_map_init(&sim->map, arena, width, height, default_cost);
}

/* xorshift64*: small, fast, deterministic across compilers. */
internal u32
sim_random_u32(sim_state *sim)
{
    u64 x = sim->rng;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    sim->rng = x;
    return (u32)((x * 0x2545F4914F6CDD1Dull) >> 32);
}

/* Unbiased integer in [0, bound). */
internal u32
sim_random_below(sim_state *sim, u32 bound)
{
    Assert(bound > 0);
    u32 threshold = (0u - bound) % bound;
    for (;;)
    {
        u32 r = sim_random_u32(sim);
        if (r >= threshold) return r % bound;
    }
}

internal sim_event_sink
sim_sink_from_arena(memory_arena *arena, u32 capacity)
{
    sim_event_sink sink;
    sink.events   = PushArray(arena, capacity, sim_event);
    sink.count    = 0;
    sink.capacity = capacity;
    return sink;
}

internal void
sim_emit(sim_event_sink *sink, sim_event event)
{
    /* A full sink drops the event; callers size sinks for the command they run. */
    if (sink->count < sink->capacity) sink->events[sink->count++] = event;
}

internal sim_command
sim_make_command(sim_state *sim, sim_command_kind kind)
{
    sim_command command;
    command.kind = kind;
    command.id   = sim->next_command_id++;
    return command;
}

internal void
sim_advance_turn(sim_state *sim, sim_event_sink *events)
{
    /* Per-turn systems run here in fixed order once they exist:
       TurnScheduler -> CommandResolution -> Production -> Movement -> Combat
       -> Diplomacy -> AIPlanning -> EventTriggers -> LedgerApply.
       Each is a free function taking sim_state* and an event sink. */
    sim->turn++;
    sim_event event = {};
    event.kind  = SIM_EVENT_TURN_ADVANCED;
    event.turn  = sim->turn;
    event.delta = 1;
    sim_emit(events, event);
}

internal void
sim_apply(sim_state *sim, sim_command command, sim_event_sink *events)
{
    switch (command.kind)
    {
        case SIM_COMMAND_NONE:
            break;
        case SIM_COMMAND_END_TURN:
        {
            u32 first = events->count;
            sim_advance_turn(sim, events);
            for (u32 i = first; i < events->count; ++i) events->events[i].cause = command.id;
            break;
        }
        default:
            InvalidCodePath;
            break;
    }
}

internal const char *
sim_event_name(sim_event_kind kind)
{
    switch (kind)
    {
        case SIM_EVENT_NONE:          return "none";
        case SIM_EVENT_TURN_ADVANCED: return "turn_advanced";
        default:                      return "?";
    }
}

internal const char *
sim_command_name(sim_command_kind kind)
{
    switch (kind)
    {
        case SIM_COMMAND_NONE:     return "none";
        case SIM_COMMAND_END_TURN: return "end_turn";
        default:                   return "?";
    }
}
