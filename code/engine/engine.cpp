/* The real backend engine: owns the simulation, applies commands to it and
   keeps the event ledger. Exposed to front ends only through engine_api. */

#include "engine/engine.h"

#define ENGINE_SCRATCH_SIZE Megabytes(4)
#define ENGINE_SINK_CAPACITY 4096

struct engine
{
    memory_arena *permanent;
    memory_arena *transient;
    memory_arena  scratch;         /* per-call working memory; reset before every command or search */
    sim_state    *sim;
    sim_event    *ledger;          /* every event the sim has emitted this session */
    u32           ledger_count;
    u32           ledger_capacity;
};

internal engine_config
engine_default_config(void)
{
    engine_config c;
    c.seed             = 0x5EEDull;
    c.ledger_capacity  = 65536;
    c.map_width        = 40;    /* placeholder map until spec/formats/scenario-file.md */
    c.map_height       = 30;
    c.map_default_cost = 1;
    return c;
}

internal engine *
engine_create(memory_arena *permanent, memory_arena *transient, engine_config config)
{
    engine *e = PushStruct(permanent, engine);
    e->permanent = permanent;
    e->transient = transient;
    e->scratch   = arena_sub(transient, ENGINE_SCRATCH_SIZE);
    e->sim       = PushStruct(permanent, sim_state);
    sim_init(e->sim, permanent, config.seed);
    sim_init_map(e->sim, permanent, config.map_width, config.map_height, config.map_default_cost);
    e->ledger_capacity = config.ledger_capacity;
    e->ledger_count    = 0;
    e->ledger          = PushArray(permanent, config.ledger_capacity, sim_event);
    return e;
}

internal void
engine_ledger_append(engine *e, sim_event_sink *sink)
{
    for (u32 i = 0; i < sink->count; ++i)
    {
        if (e->ledger_count < e->ledger_capacity)
        {
            e->ledger[e->ledger_count++] = sink->events[i];
        }
    }
}

/* Runs one command against the sim and ledgers what it emitted. */
internal sim_command
engine_run_command(engine *e, sim_command_kind kind, u32 *events_emitted)
{
    arena_reset(&e->scratch);
    sim_event_sink sink = sim_sink_from_arena(&e->scratch, ENGINE_SINK_CAPACITY);
    sim_command command = sim_make_command(e->sim, kind);
    sim_apply(e->sim, command, &sink);
    engine_ledger_append(e, &sink);
    if (events_emitted) *events_emitted += sink.count;
    return command;
}

/* ---- engine_api implementation ------------------------------------------ */

internal u32
engine_api_turn(void *opaque)
{
    return ((engine *)opaque)->sim->turn;
}

internal u32
engine_api_entities_alive(void *opaque)
{
    return ((engine *)opaque)->sim->entities.alive_count;
}

internal u32
engine_api_entity_capacity(void *opaque)
{
    return ((engine *)opaque)->sim->entities.capacity;
}

internal u32
engine_api_ledger_count(void *opaque)
{
    return ((engine *)opaque)->ledger_count;
}

internal sim_event
engine_api_ledger_at(void *opaque, u32 index)
{
    engine *e = (engine *)opaque;
    sim_event none = {};
    return index < e->ledger_count ? e->ledger[index] : none;
}

internal engine_memory_stats
engine_api_memory(void *opaque)
{
    engine *e = (engine *)opaque;
    engine_memory_stats stats;
    stats.permanent_used = e->permanent->used;
    stats.permanent_size = e->permanent->size;
    stats.transient_used = e->transient->used;
    stats.transient_size = e->transient->size;
    return stats;
}

internal engine_map_size
engine_api_map_size(void *opaque)
{
    engine *e = (engine *)opaque;
    engine_map_size size;
    size.width  = e->sim->map.width;
    size.height = e->sim->map.height;
    return size;
}

internal u8
engine_api_cell_cost(void *opaque, s32 q, s32 r)
{
    return hex_map_cost(&((engine *)opaque)->sim->map, hex_make(q, r));
}

internal u32
engine_api_path_cost(void *opaque, s32 q0, s32 r0, s32 q1, s32 r1)
{
    engine *e = (engine *)opaque;
    arena_reset(&e->scratch);
    hex_path path = hex_find_path(&e->sim->map, hex_make(q0, r0), hex_make(q1, r1), &e->scratch);
    return path.total_cost;
}

internal u32
engine_api_end_turn(void *opaque)
{
    sim_command command = engine_run_command((engine *)opaque, SIM_COMMAND_END_TURN, 0);
    return command.id;
}

internal u32
engine_api_advance_turns(void *opaque, u32 count)
{
    u32 emitted = 0;
    for (u32 i = 0; i < count; ++i)
    {
        engine_run_command((engine *)opaque, SIM_COMMAND_END_TURN, &emitted);
    }
    return emitted;
}

internal void
engine_api_ledger_clear(void *opaque)
{
    ((engine *)opaque)->ledger_count = 0;
}

internal void
engine_api_set_cell_cost(void *opaque, s32 q, s32 r, u8 cost)
{
    hex_map_set_cost(&((engine *)opaque)->sim->map, hex_make(q, r), cost);
}

internal engine_api
engine_make_api(engine *e)
{
    engine_api api;
    api.engine          = e;
    api.turn            = engine_api_turn;
    api.entities_alive  = engine_api_entities_alive;
    api.entity_capacity = engine_api_entity_capacity;
    api.ledger_count    = engine_api_ledger_count;
    api.ledger_at       = engine_api_ledger_at;
    api.memory          = engine_api_memory;
    api.map_size        = engine_api_map_size;
    api.cell_cost       = engine_api_cell_cost;
    api.path_cost       = engine_api_path_cost;
    api.end_turn        = engine_api_end_turn;
    api.advance_turns   = engine_api_advance_turns;
    api.ledger_clear    = engine_api_ledger_clear;
    api.set_cell_cost   = engine_api_set_cell_cost;
    return api;
}
