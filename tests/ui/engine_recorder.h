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
    RECORDER_TERRAIN_VIEW,
    RECORDER_DATE,
    RECORDER_WHOSE_TURN,
    RECORDER_ACTIONS_LEFT,
    RECORDER_FACTION_COUNT,
    RECORDER_FACTION_AT,
    RECORDER_CAN_ISSUE,
    RECORDER_CITY_SLOTS,
    RECORDER_FACILITY_AT,
    RECORDER_FACILITY_KIND_COUNT,
    RECORDER_FACILITY_KIND_AT,
    RECORDER_CONTENT_STATUS,
    RECORDER_OFFICER_BY_ID,
    RECORDER_FORCE_RULER,
    RECORDER_LOCATION_NAME_AT,
    RECORDER_CITY_ECONOMY,
    RECORDER_CITY_INCOME_FORECAST,
    RECORDER_TECHNIQUE_POINTS,
    RECORDER_WEAPON_KIND_COUNT,
    RECORDER_WEAPON_KIND_AT,
    RECORDER_LEDGER_FILTER,   /* arg = city | resource << 8 */
    RECORDER_END_TURN,        /* commands start here */
    RECORDER_ADVANCE_TURNS,
    RECORDER_LEDGER_CLEAR,
    RECORDER_SET_CELL_COST,
    RECORDER_SET_CELL_TERRAIN,
    RECORDER_LOAD_SAVE,
    RECORDER_LOAD_NAME_INDEX,
    RECORDER_COMMAND_DEVELOP,
    RECORDER_SET_FACTION_HUMAN,
    RECORDER_SET_CITY_OWNER,
    RECORDER_SET_CITY_GOLD,
    RECORDER_SET_ACTIONS,
    RECORDER_PLACE_UNIT,
    RECORDER_COMMAND_PATROL,
    RECORDER_COMMAND_CONSCRIPT,
    RECORDER_COMMAND_TRAIN,
    RECORDER_COMMAND_MERCHANT,
    RECORDER_COMMAND_PRODUCE,
    RECORDER_DAMAGE_FACILITY,
    RECORDER_SET_FACILITY_LEVEL,
    RECORDER_SET_CITY_RESOURCE,
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
    sim_refusal   can_issue_refusal;  /* script the answer of can_issue and command_develop */
    engine_command last_develop;      /* the arguments of the last command_develop */
    engine_command last_city_command; /* the arguments of the last patrol, conscript, train, merchant or produce */
    u32           filter_indices[RECORDER_MAX_EVENTS];   /* the answer of ledger_filter: every scripted event */
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
internal engine_officer recorder_scripted_officer(engine_recorder *r, u32 i)
{
    engine_officer officer = {};
    officer.officer_id = ENGINE_NO_OFFICER;
    if (i >= r->officer_count) return officer;
    officer.officer_id     = (u16)(100 + i);
    officer.family_name[0] = 0xB1;
    officer.family_name[1] = (u8)(0x40 + i);
    officer.leadership     = 50;
    officer.force_id       = 1;
    officer.status         = 3;
    officer.base_location  = (u16)i;        /* officer i has its base in city i */
    officer.current_position = 0;        /* every scripted officer stands in city 0 */
    officer.politics       = 60;
    officer.job_city       = ENGINE_NO_CITY;
    return officer;
}

internal engine_officer recorder_officer_at(void *o, u32 i)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_OFFICER_AT, i);
    return recorder_scripted_officer(r, i);
}

internal engine_officer recorder_officer_by_id(void *o, u16 officer_id)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_OFFICER_BY_ID, officer_id);
    return recorder_scripted_officer(r, officer_id >= 100 ? officer_id - 100u : U32_MAX);
}

/* The first scripted officer (id 100) rules force 1; no other force has a ruler. */
internal u16 recorder_force_ruler(void *o, u8 force_id)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_FORCE_RULER, force_id);
    return force_id == 1 && r->officer_count > 0 ? 100 : ENGINE_NO_OFFICER;
}

/* After a name index load, location i is named A5 (40 + i) for a city and
   A6 (i) for a gate or a port. */
internal engine_location_name recorder_location_name_at(void *o, u16 location)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_LOCATION_NAME_AT, location);
    engine_location_name name = {};
    name.location = ENGINE_NO_LOCATION;
    if (!r->has_names || location >= NAME_INDEX_FIRST_GATE_ID + NAME_INDEX_GATE_COUNT) return name;
    name.location = location;
    name.name[0]  = location < SCENARIO_CITY_COUNT ? 0xA5 : 0xA6;
    name.name[1]  = (u8)(location < SCENARIO_CITY_COUNT ? 0x40 + location : location);
    return name;
}

internal engine_city recorder_city_at(void *o, u32 i)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_CITY_AT, i);
    engine_city city = {};
    city.city_id = ENGINE_NO_CITY;
    if (i >= r->city_count) return city;
    city.city_id               = (u8)i;
    city.owner_force_id        = 1;        /* the force of the scripted officers */
    city.commanding_officer_id = i == 0 ? 100 : 0xFFFF;   /* officer 100 commands city 0 */
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

/* The fake 8x6 map has flat plains with one hill column. */
global_variable u8 recorder_terrain_kind[8 * 6];
global_variable u8 recorder_terrain_level[8 * 6];

internal engine_terrain_view
recorder_terrain_view(void *o)
{
    recorder_record((engine_recorder *)o, RECORDER_TERRAIN_VIEW, 0);
    for (u32 i = 0; i < 48; ++i) { recorder_terrain_kind[i] = (i % 8 == 3) ? 3 : 1; recorder_terrain_level[i] = (i % 8 == 3) ? 2 : 1; }
    engine_terrain_view v;
    v.width = 8; v.height = 6; v.kind = recorder_terrain_kind; v.level = recorder_terrain_level; v.version = 1;
    return v;
}

internal void
recorder_set_cell_terrain(void *o, s32 q, s32 r, u8 kind, u8 level)
{
    (void)q; (void)r; (void)level;
    recorder_record((engine_recorder *)o, RECORDER_SET_CELL_TERRAIN, kind);
}

/* ---- M1 develop and M2 turns: one faction (force 1) acts; 10 empty slots
   in each city; 3 facility kinds; can_issue answers can_issue_refusal. */
#define RECORDER_SLOTS 10
global_variable const char *recorder_facility_keys[] = { "market", "farm", "mint" };

internal engine_date recorder_date(void *o)
{
    recorder_record((engine_recorder *)o, RECORDER_DATE, 0);
    engine_date date = {};
    date.year = 194; date.month = 6; date.day = 1; date.season = CALENDAR_SUMMER;
    return date;
}
internal u8  recorder_whose_turn(void *o)             { recorder_record((engine_recorder *)o, RECORDER_WHOSE_TURN, 0); return 1; }
internal u32 recorder_actions_left(void *o, u8 force) { recorder_record((engine_recorder *)o, RECORDER_ACTIONS_LEFT, force); return 100; }
internal u32 recorder_faction_count(void *o)          { recorder_record((engine_recorder *)o, RECORDER_FACTION_COUNT, 0); return 1; }
internal engine_faction recorder_faction_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_FACTION_AT, index);
    engine_faction faction = {};
    faction.force_id = index == 0 ? 1 : ENGINE_NO_FACTION;
    faction.city_count = 2; faction.action_points = 100; faction.human = true;
    return faction;
}
internal engine_check recorder_can_issue(void *o, const engine_command *command)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_CAN_ISSUE, command->facility);
    engine_check check = {};
    check.refusal = r->can_issue_refusal;
    if (check.refusal == SIM_REFUSAL_NONE) { check.gold = 200; check.action_points = 20; check.turns = 3; check.days = 30; check.slot = 0; }
    return check;
}
internal engine_city_slots recorder_city_slots(void *o, u8 city_id)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_CITY_SLOTS, city_id);
    engine_city_slots slots = {};
    slots.city_id = city_id < r->city_count ? city_id : ENGINE_NO_CITY;
    slots.slot_count = slots.city_id == ENGINE_NO_CITY ? 0 : RECORDER_SLOTS;
    slots.gold_income = 100; slots.grain_income = 500; slots.governor_id = 100;
    return slots;
}
internal engine_facility recorder_facility_at(void *o, u8 city_id, u8 slot)
{
    recorder_record((engine_recorder *)o, RECORDER_FACILITY_AT, slot);
    engine_facility facility = {};
    facility.city_id  = slot < RECORDER_SLOTS ? city_id : ENGINE_NO_CITY;
    facility.slot     = slot;
    facility.state    = SIM_SLOT_EMPTY;
    facility.facility = ENGINE_NO_FACILITY;
    return facility;
}
internal u32 recorder_facility_kind_count(void *o)
{
    recorder_record((engine_recorder *)o, RECORDER_FACILITY_KIND_COUNT, 0);
    return ArrayCount(recorder_facility_keys);
}
internal engine_facility_kind recorder_facility_kind_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_FACILITY_KIND_AT, index);
    engine_facility_kind kind = {};
    kind.id = ENGINE_NO_FACILITY;
    if (index >= ArrayCount(recorder_facility_keys)) return kind;
    kind.id = (u8)index;
    snprintf(kind.key, sizeof(kind.key), "%s", recorder_facility_keys[index]);
    kind.cost = 200 + 100 * index;
    return kind;
}
internal engine_content_status recorder_content_status(void *o)
{
    recorder_record((engine_recorder *)o, RECORDER_CONTENT_STATUS, 0);
    engine_content_status status = {};
    status.loaded = true; status.facility_count = ArrayCount(recorder_facility_keys); status.detail = "ok";
    return status;
}
internal engine_command_result recorder_command_develop(void *o, u8 city_id, u8 facility, const u16 officers[SIM_DEVELOP_OFFICERS], u8 slot)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_DEVELOP, facility);
    r->last_develop.kind     = ENGINE_COMMAND_DEVELOP;
    r->last_develop.city_id  = city_id;
    r->last_develop.facility = facility;
    r->last_develop.slot     = slot;
    for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place) r->last_develop.officers[place] = officers[place];
    engine_command_result result = {};
    result.refusal = r->can_issue_refusal;
    if (result.refusal == SIM_REFUSAL_NONE) result.command_id = r->next_command_id++;
    return result;
}
internal void recorder_set_faction_human(void *o, u8 force, b32 human) { (void)human; recorder_record((engine_recorder *)o, RECORDER_SET_FACTION_HUMAN, force); }
internal void recorder_set_city_owner(void *o, u8 city, u8 force)      { (void)force; recorder_record((engine_recorder *)o, RECORDER_SET_CITY_OWNER, city); }
internal void recorder_set_city_gold(void *o, u8 city, u32 gold)       { (void)gold; recorder_record((engine_recorder *)o, RECORDER_SET_CITY_GOLD, city); }
internal void recorder_set_actions(void *o, u8 force, u32 points)      { (void)points; recorder_record((engine_recorder *)o, RECORDER_SET_ACTIONS, force); }
internal u32  recorder_place_unit(void *o, u8 force, s32 q, s32 r)     { (void)q; (void)r; recorder_record((engine_recorder *)o, RECORDER_PLACE_UNIT, force); return 0; }

/* ---- M3 economy: each city has public order 60, morale 50, 8000
   soldiers, one barracks; the weapon table has 10 weapons. */
global_variable const char *recorder_weapon_keys[] = {
    "pike", "halberd", "crossbow", "horse", "ram", "wood_beast", "tower", "catapult", "tower_ship", "battle_ship",
};

internal engine_city_economy recorder_city_economy(void *o, u8 city_id)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_CITY_ECONOMY, city_id);
    engine_city_economy economy = {};
    economy.city_id = city_id < r->city_count ? city_id : ENGINE_NO_CITY;
    economy.gold = 1000 * (city_id + 1); economy.gold_cap = 100000;
    economy.grain = 20000; economy.grain_cap = 1000000;
    economy.public_order = 60; economy.order_max = 100;
    economy.morale = 50; economy.morale_max = 100;
    economy.soldiers = 8000; economy.soldier_cap = 100000; economy.weapon_cap = 100000;
    economy.grain_per_gold = 6; economy.barracks = 1; economy.barracks_free = 1;
    return economy;
}
internal engine_income_forecast recorder_city_income_forecast(void *o, u8 city_id)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_CITY_INCOME_FORECAST, city_id);
    engine_income_forecast forecast = {};
    forecast.city_id = city_id < r->city_count ? city_id : ENGINE_NO_CITY;
    forecast.assumed = true;
    forecast.amounts.gold_base = 100; forecast.amounts.gold_month = 120; forecast.amounts.gold_payment = 120;
    forecast.amounts.order_percent = 80; forecast.amounts.gold_skill_percent = 100; forecast.amounts.grain_skill_percent = 100;
    forecast.amounts.grain_base = 500; forecast.amounts.grain_season = 400; forecast.amounts.grain_payment = 400;
    forecast.amounts.salaries = 30; forecast.amounts.grain_per_turn = 80;
    return forecast;
}
internal u32 recorder_technique_points(void *o, u8 force) { recorder_record((engine_recorder *)o, RECORDER_TECHNIQUE_POINTS, force); return 40; }
internal u32 recorder_weapon_kind_count(void *o)
{
    recorder_record((engine_recorder *)o, RECORDER_WEAPON_KIND_COUNT, 0);
    return ArrayCount(recorder_weapon_keys);
}
internal engine_weapon_kind recorder_weapon_kind_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_WEAPON_KIND_AT, index);
    engine_weapon_kind kind = {};
    kind.id = ENGINE_NO_WEAPON;
    if (index >= ArrayCount(recorder_weapon_keys)) return kind;
    kind.id = (u8)index;
    snprintf(kind.key, sizeof(kind.key), "%s", recorder_weapon_keys[index]);
    kind.cost  = index < 4 ? 700 : 1500 + 100 * index;
    kind.stock = index < 4 ? index : 4;
    kind.min_days = index < 4 ? 0 : 30;
    kind.max_days = index < 4 ? 0 : 90;
    return kind;
}
internal engine_ledger_view recorder_ledger_filter(void *o, u8 city_id, u8 resource)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_LEDGER_FILTER, (u32)city_id | ((u32)resource << 8));
    for (u32 index = 0; index < r->event_count; ++index) r->filter_indices[index] = index;
    engine_ledger_view view = {};
    view.count   = r->event_count;
    view.indices = r->filter_indices;
    return view;
}

internal engine_command_result
recorder_city_command(engine_recorder *r, recorder_call_kind call, engine_command_kind kind, u8 city_id,
                      const u16 *officers, u8 weapon, u8 mode, u32 amount)
{
    recorder_record(r, call, city_id);
    r->last_city_command = {};
    r->last_city_command.kind          = kind;
    r->last_city_command.city_id       = city_id;
    r->last_city_command.weapon        = weapon;
    r->last_city_command.merchant_mode = mode;
    r->last_city_command.amount        = amount;
    for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place) r->last_city_command.officers[place] = officers ? officers[place] : ENGINE_NO_OFFICER;
    engine_command_result result = {};
    result.refusal = r->can_issue_refusal;
    if (result.refusal == SIM_REFUSAL_NONE) result.command_id = r->next_command_id++;
    return result;
}
internal engine_command_result recorder_command_patrol(void *o, u8 city, const u16 officers[SIM_DEVELOP_OFFICERS])
{
    return recorder_city_command((engine_recorder *)o, RECORDER_COMMAND_PATROL, ENGINE_COMMAND_PATROL, city, officers, ENGINE_NO_WEAPON, 0, 0);
}
internal engine_command_result recorder_command_conscript(void *o, u8 city, const u16 officers[SIM_DEVELOP_OFFICERS])
{
    return recorder_city_command((engine_recorder *)o, RECORDER_COMMAND_CONSCRIPT, ENGINE_COMMAND_CONSCRIPT, city, officers, ENGINE_NO_WEAPON, 0, 0);
}
internal engine_command_result recorder_command_train(void *o, u8 city, const u16 officers[SIM_DEVELOP_OFFICERS])
{
    return recorder_city_command((engine_recorder *)o, RECORDER_COMMAND_TRAIN, ENGINE_COMMAND_TRAIN, city, officers, ENGINE_NO_WEAPON, 0, 0);
}
internal engine_command_result recorder_command_merchant(void *o, u8 city, u16 officer, u8 mode, u32 amount)
{
    u16 officers[SIM_DEVELOP_OFFICERS] = { officer, ENGINE_NO_OFFICER, ENGINE_NO_OFFICER };
    return recorder_city_command((engine_recorder *)o, RECORDER_COMMAND_MERCHANT, ENGINE_COMMAND_MERCHANT, city, officers, ENGINE_NO_WEAPON, mode, amount);
}
internal engine_command_result recorder_command_produce(void *o, u8 city, u8 weapon, const u16 officers[SIM_DEVELOP_OFFICERS])
{
    return recorder_city_command((engine_recorder *)o, RECORDER_COMMAND_PRODUCE, ENGINE_COMMAND_PRODUCE, city, officers, weapon, 0, 0);
}
internal engine_command_result recorder_damage_facility(void *o, u8 city, u8 slot, u32 amount)
{
    (void)slot; (void)amount;
    return recorder_city_command((engine_recorder *)o, RECORDER_DAMAGE_FACILITY, ENGINE_COMMAND_KIND_COUNT, city, 0, ENGINE_NO_WEAPON, 0, 0);
}
internal b32  recorder_set_facility_level(void *o, u8 city, u8 slot, u8 level) { (void)slot; (void)level; recorder_record((engine_recorder *)o, RECORDER_SET_FACILITY_LEVEL, city); return true; }
internal void recorder_set_city_resource(void *o, u8 city, u8 resource, u32 index, u32 value) { (void)resource; (void)index; (void)value; recorder_record((engine_recorder *)o, RECORDER_SET_CITY_RESOURCE, city); }

/* The command kind of the recorder call of a city command. */
internal recorder_call_kind
recorder_call_of(engine_command_kind kind)
{
    switch (kind)
    {
        case ENGINE_COMMAND_DEVELOP:   return RECORDER_COMMAND_DEVELOP;
        case ENGINE_COMMAND_PATROL:    return RECORDER_COMMAND_PATROL;
        case ENGINE_COMMAND_CONSCRIPT: return RECORDER_COMMAND_CONSCRIPT;
        case ENGINE_COMMAND_TRAIN:     return RECORDER_COMMAND_TRAIN;
        case ENGINE_COMMAND_MERCHANT:  return RECORDER_COMMAND_MERCHANT;
        case ENGINE_COMMAND_PRODUCE:   return RECORDER_COMMAND_PRODUCE;
        default:                       return RECORDER_CALL_KIND_COUNT;
    }
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
    api.terrain_view    = recorder_terrain_view;
    api.set_cell_terrain = recorder_set_cell_terrain;
    api.load_save       = recorder_load_save;
    api.load_name_index = recorder_load_name_index;
    api.date            = recorder_date;
    api.whose_turn      = recorder_whose_turn;
    api.actions_left    = recorder_actions_left;
    api.faction_count   = recorder_faction_count;
    api.faction_at      = recorder_faction_at;
    api.can_issue       = recorder_can_issue;
    api.city_slots      = recorder_city_slots;
    api.facility_at     = recorder_facility_at;
    api.facility_kind_count = recorder_facility_kind_count;
    api.facility_kind_at    = recorder_facility_kind_at;
    api.content_status  = recorder_content_status;
    api.command_develop = recorder_command_develop;
    api.set_faction_human = recorder_set_faction_human;
    api.set_city_owner  = recorder_set_city_owner;
    api.set_city_gold   = recorder_set_city_gold;
    api.set_actions     = recorder_set_actions;
    api.place_unit      = recorder_place_unit;
    api.officer_by_id   = recorder_officer_by_id;
    api.force_ruler     = recorder_force_ruler;
    api.location_name_at = recorder_location_name_at;
    api.city_economy         = recorder_city_economy;
    api.city_income_forecast = recorder_city_income_forecast;
    api.technique_points     = recorder_technique_points;
    api.weapon_kind_count    = recorder_weapon_kind_count;
    api.weapon_kind_at       = recorder_weapon_kind_at;
    api.ledger_filter        = recorder_ledger_filter;
    api.command_patrol       = recorder_command_patrol;
    api.command_conscript    = recorder_command_conscript;
    api.command_train        = recorder_command_train;
    api.command_merchant     = recorder_command_merchant;
    api.command_produce      = recorder_command_produce;
    api.damage_facility      = recorder_damage_facility;
    api.set_facility_level   = recorder_set_facility_level;
    api.set_city_resource    = recorder_set_city_resource;
    return api;
}

#endif /* ENGINE_RECORDER_H */
