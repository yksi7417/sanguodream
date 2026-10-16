#ifndef ENGINE_RECORDER_H
#define ENGINE_RECORDER_H
/* A recording double for engine_api. Every call the UI makes is appended to
   calls[] with its argument; replies are scripted so the UI has something to
   draw. UI tests assert on this record: the UI's whole job is to make the
   right API calls. */

#include "base.h"
#include "engine/engine.h"

enum recorder_call_kind
{
    RECORDER_TURN,
    RECORDER_ENTITIES_ALIVE,
    RECORDER_ENTITY_CAPACITY,
    RECORDER_LEDGER_COUNT,
    RECORDER_LEDGER_AT,
    RECORDER_MEMORY,
    RECORDER_MAP_SIZE,
    RECORDER_CELL_COST,
    RECORDER_PATH_COST,
    RECORDER_OFFICER_COUNT,
    RECORDER_OFFICER_AT,
    RECORDER_CITY_COUNT,
    RECORDER_CITY_AT,
    RECORDER_CITY_NAME_AT,
    RECORDER_END_TURN,        /* commands start here */
    RECORDER_ADVANCE_TURNS,
    RECORDER_LEDGER_CLEAR,
    RECORDER_SET_CELL_COST,
    RECORDER_LOAD_SAVE,
    RECORDER_LOAD_NAME_INDEX,
    RECORDER_CALL_KIND_COUNT
};

#define RECORDER_FIRST_COMMAND RECORDER_END_TURN
#define RECORDER_MAX_CALLS     65536
#define RECORDER_MAX_EVENTS    64
#define RECORDER_MAX_PATH      260

struct recorder_call
{
    recorder_call_kind kind;
    u32                arg;
};

struct engine_recorder
{
    recorder_call calls[RECORDER_MAX_CALLS];
    u32           call_count;
    u32           dropped;            /* calls beyond RECORDER_MAX_CALLS */
    /* scripted state the replies come from */
    u32           turn;
    u32           next_command_id;
    sim_event     events[RECORDER_MAX_EVENTS];
    u32           event_count;
    u32           officer_count;      /* scripted rows for the Officers and Cities panels */
    u32           city_count;
    char          last_load_path[RECORDER_MAX_PATH];
    b32           load_fails;         /* script a refused load */
    b32           has_names;          /* city_name_at answers only after a name index load */
    char          last_name_index_path[RECORDER_MAX_PATH];
};

internal void
recorder_record(engine_recorder *r, recorder_call_kind kind, u32 arg)
{
    if (r->call_count < RECORDER_MAX_CALLS)
    {
        r->calls[r->call_count].kind = kind;
        r->calls[r->call_count].arg  = arg;
        r->call_count++;
    }
    else
    {
        r->dropped++;
    }
}

/* Forgets recorded calls; scripted state stays. */
internal void
recorder_reset(engine_recorder *r)
{
    r->call_count = 0;
    r->dropped = 0;
}

internal u32
recorder_count(engine_recorder *r, recorder_call_kind kind)
{
    u32 n = 0;
    for (u32 i = 0; i < r->call_count; ++i) if (r->calls[i].kind == kind) n++;
    return n;
}

/* Number of command calls (anything that would change the game). */
internal u32
recorder_count_commands(engine_recorder *r)
{
    u32 n = 0;
    for (u32 i = 0; i < r->call_count; ++i) if (r->calls[i].kind >= RECORDER_FIRST_COMMAND) n++;
    return n;
}

/* The argument of the first call of a kind, or U32_MAX if none. */
internal u32
recorder_first_arg(engine_recorder *r, recorder_call_kind kind)
{
    for (u32 i = 0; i < r->call_count; ++i) if (r->calls[i].kind == kind) return r->calls[i].arg;
    return U32_MAX;
}

internal void
recorder_seed_events(engine_recorder *r, u32 count)
{
    r->event_count = 0;
    for (u32 i = 0; i < count && i < RECORDER_MAX_EVENTS; ++i)
    {
        sim_event ev = {};
        ev.kind  = SIM_EVENT_TURN_ADVANCED;
        ev.turn  = i + 1;
        ev.delta = 1;
        ev.cause = i + 1;
        r->events[r->event_count++] = ev;
    }
}

internal u32 recorder_turn(void *o)                { engine_recorder *r = (engine_recorder *)o; recorder_record(r, RECORDER_TURN, 0); return r->turn; }
internal u32 recorder_entities_alive(void *o)      { recorder_record((engine_recorder *)o, RECORDER_ENTITIES_ALIVE, 0); return 3; }
internal u32 recorder_entity_capacity(void *o)     { recorder_record((engine_recorder *)o, RECORDER_ENTITY_CAPACITY, 0); return 4096; }
internal u32 recorder_ledger_count(void *o)        { engine_recorder *r = (engine_recorder *)o; recorder_record(r, RECORDER_LEDGER_COUNT, 0); return r->event_count; }
internal sim_event recorder_ledger_at(void *o, u32 i)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_LEDGER_AT, i);
    sim_event none = {};
    return i < r->event_count ? r->events[i] : none;
}
internal engine_memory_stats recorder_memory(void *o)
{
    recorder_record((engine_recorder *)o, RECORDER_MEMORY, 0);
    engine_memory_stats s = { Megabytes(1), Megabytes(256), Megabytes(1), Megabytes(256) };
    return s;
}
internal u32 recorder_end_turn(void *o)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_END_TURN, 0);
    r->turn++;
    return r->next_command_id++;
}
internal u32 recorder_advance_turns(void *o, u32 count)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_ADVANCE_TURNS, count);
    r->turn += count;
    r->next_command_id += count;
    return count;
}
internal void recorder_ledger_clear(void *o)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_LEDGER_CLEAR, 0);
    r->event_count = 0;
}
internal engine_map_size recorder_map_size(void *o)
{
    recorder_record((engine_recorder *)o, RECORDER_MAP_SIZE, 0);
    engine_map_size s = { 8, 6 };
    return s;
}
internal u8  recorder_cell_cost(void *o, s32 q, s32 r)
{
    (void)q; (void)r;
    recorder_record((engine_recorder *)o, RECORDER_CELL_COST, 0);
    return 1;
}
internal u32 recorder_path_cost(void *o, s32 q0, s32 r0, s32 q1, s32 r1)
{
    (void)q0; (void)r0; (void)q1; (void)r1;
    recorder_record((engine_recorder *)o, RECORDER_PATH_COST, 0);
    return 3;
}
internal void recorder_set_cell_cost(void *o, s32 q, s32 r, u8 cost)
{
    (void)q; (void)r;
    recorder_record((engine_recorder *)o, RECORDER_SET_CELL_COST, cost);
}

internal u32 recorder_officer_count(void *o)  { engine_recorder *r = (engine_recorder *)o; recorder_record(r, RECORDER_OFFICER_COUNT, 0); return r->officer_count; }
internal u32 recorder_city_count(void *o)     { engine_recorder *r = (engine_recorder *)o; recorder_record(r, RECORDER_CITY_COUNT, 0); return r->city_count; }

/* Officer i of the script: id 100 + i, with a two-byte name that no
   character mapping is needed to check. */
internal engine_officer recorder_officer_at(void *o, u32 i)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_OFFICER_AT, i);
    engine_officer officer = {};
    officer.officer_id = ENGINE_NO_OFFICER;
    if (i >= r->officer_count) return officer;
    officer.officer_id     = (u16)(100 + i);
    officer.family_name[0] = 0xB1;
    officer.family_name[1] = (u8)(0x40 + i);
    officer.leadership     = 50;
    officer.force_id       = 1;
    officer.status         = 3;
    officer.base_location  = 0xFFFF;
    return officer;
}

internal engine_city recorder_city_at(void *o, u32 i)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_CITY_AT, i);
    engine_city city = {};
    city.city_id = ENGINE_NO_CITY;
    if (i >= r->city_count) return city;
    city.city_id               = (u8)i;
    city.owner_force_id        = 255;
    city.commanding_officer_id = 0xFFFF;
    city.gold                  = 1000 * (i + 1);
    return city;
}

/* City i of the script is named with the two bytes A4 (40 + i) after a name
   index load, and is no city before it. */
internal engine_city_name recorder_city_name_at(void *o, u32 i)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_CITY_NAME_AT, i);
    engine_city_name name = {};
    name.city_id = ENGINE_NO_CITY;
    if (!r->has_names || i >= SCENARIO_CITY_COUNT) return name;
    name.city_id = (u8)i;
    name.name[0] = 0xA4;
    name.name[1] = (u8)(0x40 + i);
    return name;
}

/* Records the path; the script then has city names. */
internal engine_load_result recorder_load_name_index(void *o, const char *path)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_LOAD_NAME_INDEX, 0);
    snprintf(r->last_name_index_path, RECORDER_MAX_PATH, "%s", path);
    r->has_names = true;
    engine_load_result result = {};
    result.status     = ENGINE_LOAD_OK;
    result.command_id = r->next_command_id++;
    result.detail     = "ok";
    return result;
}

/* Records the path; the script gains three officers and two cities. */
internal engine_load_result recorder_load_save(void *o, const char *path)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_LOAD_SAVE, 0);
    snprintf(r->last_load_path, RECORDER_MAX_PATH, "%s", path);
    if (r->load_fails)
    {
        engine_load_result refused = {};
        refused.status = ENGINE_LOAD_BAD_SAVE;
        refused.detail = "not a save";
        return refused;
    }
    r->officer_count = 3;
    r->city_count    = 2;
    engine_load_result result = {};
    result.status     = ENGINE_LOAD_OK;
    result.command_id = r->next_command_id++;
    result.detail     = "ok";
    return result;
}

internal engine_api
recorder_make_api(engine_recorder *r)
{
    r->next_command_id = r->next_command_id ? r->next_command_id : 1;
    engine_api api;
    api.engine          = r;
    api.turn            = recorder_turn;
    api.entities_alive  = recorder_entities_alive;
    api.entity_capacity = recorder_entity_capacity;
    api.ledger_count    = recorder_ledger_count;
    api.ledger_at       = recorder_ledger_at;
    api.memory          = recorder_memory;
    api.map_size        = recorder_map_size;
    api.cell_cost       = recorder_cell_cost;
    api.path_cost       = recorder_path_cost;
    api.officer_count   = recorder_officer_count;
    api.officer_at      = recorder_officer_at;
    api.city_count      = recorder_city_count;
    api.city_at         = recorder_city_at;
    api.city_name_at    = recorder_city_name_at;
    api.end_turn        = recorder_end_turn;
    api.advance_turns   = recorder_advance_turns;
    api.ledger_clear    = recorder_ledger_clear;
    api.set_cell_cost   = recorder_set_cell_cost;
    api.load_save       = recorder_load_save;
    api.load_name_index = recorder_load_name_index;
    return api;
}

#endif /* ENGINE_RECORDER_H */
