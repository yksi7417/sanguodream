#ifndef SIM_H
#define SIM_H
/* Headless, deterministic simulation core. No Windows headers, no I/O, no
   ImGui. Everything lives in the permanent arena. Commands in, events out.

   Mechanics are implemented one at a time from spec-src/spec/mechanics/ with
   fixtures from spec-src/spec/fixtures/. Until a mechanic has a spec entry
   and fixtures, it does not exist here. */

#include "base.h"
#include "ecs/ecs.h"
#include "sim/hex.h"
#include "sim/inflate.h"

enum sim_event_kind
{
    SIM_EVENT_NONE,
    SIM_EVENT_TURN_ADVANCED,
    SIM_EVENT_COUNT
};

/* Every change the sim makes is an event with a cause, so a save-file diff
   from the original maps straight onto expected events. */
struct sim_event
{
    sim_event_kind kind;
    u32            turn;
    entity_id      subject;
    s32            delta;
    u32            cause;   /* id of the command that caused it, 0 for the turn itself */
};

struct sim_event_sink
{
    sim_event *events;
    u32        count;
    u32        capacity;
};

enum sim_command_kind
{
    SIM_COMMAND_NONE,
    SIM_COMMAND_END_TURN,
    SIM_COMMAND_COUNT
};

struct sim_command
{
    sim_command_kind kind;
    u32              id;      /* assigned by sim_make_command */
};

struct sim_state
{
    u64            rng;             /* seeded PRNG state; the only randomness source */
    u32            turn;
    u32            next_command_id;
    entity_manager entities;
    hex_map        map;             /* dimensions and costs come from the scenario; see sim_init_map */
    /* component tables are added mechanic by mechanic */
};

#define SIM_ENTITY_CAPACITY 4096

internal void        sim_init(sim_state *sim, memory_arena *arena, u64 seed);
internal void        sim_init_map(sim_state *sim, memory_arena *arena, u32 width, u32 height, u8 default_cost);
internal u32         sim_random_u32(sim_state *sim);
internal u32         sim_random_below(sim_state *sim, u32 bound);
internal sim_command sim_make_command(sim_state *sim, sim_command_kind kind);
internal void        sim_apply(sim_state *sim, sim_command command, sim_event_sink *events);
internal void        sim_advance_turn(sim_state *sim, sim_event_sink *events);
internal const char *sim_event_name(sim_event_kind kind);
internal const char *sim_command_name(sim_command_kind kind);

internal sim_event_sink sim_sink_from_arena(memory_arena *arena, u32 capacity);
internal void           sim_emit(sim_event_sink *sink, sim_event event);

#endif /* SIM_H */
