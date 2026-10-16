#ifndef ENGINE_H
#define ENGINE_H
/* The line between the backend engine and whatever drives it.

   Everything a front end may do to the game goes through engine_api, a table
   of plain function pointers over an opaque engine. The UI never touches
   sim_state, the ECS or the ledger directly. Feature tests drive the real
   engine through this same table; UI tests substitute a recording double
   (tests/ui/engine_recorder.h) and assert which calls the UI made.

   Growing the game means growing this table: a new mechanic adds the query
   and command functions a front end needs, and nothing else. */

#include "base.h"
#include "sim/sim.h"

struct engine_memory_stats
{
    u64 permanent_used;
    u64 permanent_size;
    u64 transient_used;
    u64 transient_size;
};

struct engine_map_size
{
    u32 width;
    u32 height;
};

/* Queries */
typedef u32                 engine_turn_fn(void *engine);
typedef u32                 engine_entities_alive_fn(void *engine);
typedef u32                 engine_entity_capacity_fn(void *engine);
typedef u32                 engine_ledger_count_fn(void *engine);
typedef sim_event           engine_ledger_at_fn(void *engine, u32 index);
typedef engine_memory_stats engine_memory_fn(void *engine);
typedef engine_map_size     engine_map_size_fn(void *engine);
typedef u8                  engine_cell_cost_fn(void *engine, s32 q, s32 r);          /* 0 = impassable or off-map */
typedef u32                 engine_path_cost_fn(void *engine, s32 q0, s32 r0, s32 q1, s32 r1); /* HEX_NO_PATH if none */
/* Commands */
typedef u32                 engine_end_turn_fn(void *engine);                 /* returns the command id */
typedef u32                 engine_advance_turns_fn(void *engine, u32 count); /* returns events emitted */
typedef void                engine_ledger_clear_fn(void *engine);
typedef void                engine_set_cell_cost_fn(void *engine, s32 q, s32 r, u8 cost); /* editor/debug */

struct engine_api
{
    void *engine;
    engine_turn_fn            *turn;
    engine_entities_alive_fn  *entities_alive;
    engine_entity_capacity_fn *entity_capacity;
    engine_ledger_count_fn    *ledger_count;
    engine_ledger_at_fn       *ledger_at;
    engine_memory_fn          *memory;
    engine_map_size_fn        *map_size;
    engine_cell_cost_fn       *cell_cost;
    engine_path_cost_fn       *path_cost;
    engine_end_turn_fn        *end_turn;
    engine_advance_turns_fn   *advance_turns;
    engine_ledger_clear_fn    *ledger_clear;
    engine_set_cell_cost_fn   *set_cell_cost;
};

/* The real engine lives in code/engine/engine.cpp. */
struct engine;
struct engine_config
{
    u64 seed;
    u32 ledger_capacity;
    u32 map_width;        /* placeholder until the scenario format is in the spec */
    u32 map_height;
    u8  map_default_cost;
};
internal engine_config engine_default_config(void);
internal engine    *engine_create(memory_arena *permanent, memory_arena *transient, engine_config config);
internal engine_api engine_make_api(engine *e);

#endif /* ENGINE_H */
