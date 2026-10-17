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
    RECORDER_TECHNIQUE_KIND_COUNT,
    RECORDER_TECHNIQUE_KIND_AT,
    RECORDER_FACTION_TECHNIQUES,
    RECORDER_DISASTER_KIND_COUNT,
    RECORDER_DISASTER_KIND_AT,
    RECORDER_CITY_DISASTERS,
    RECORDER_OUTCOME,
    RECORDER_SETTINGS,
    RECORDER_TERRAIN_KIND_COUNT,
    RECORDER_TERRAIN_KIND_AT,
    RECORDER_UNIT_TYPE_COUNT,
    RECORDER_UNIT_TYPE_AT,
    RECORDER_UNIT_COUNT,
    RECORDER_UNIT_AT,
    RECORDER_UNIT_AT_HEX,
    RECORDER_REACHABLE,
    RECORDER_UNIT_VALUES,
    RECORDER_SORTIE_PROPOSAL,
    RECORDER_TACTIC_KIND_COUNT,
    RECORDER_TACTIC_KIND_AT,
    RECORDER_PLOT_KIND_COUNT,
    RECORDER_PLOT_KIND_AT,
    RECORDER_BUILDING_KIND_COUNT,
    RECORDER_BUILDING_KIND_AT,
    RECORDER_BUILDING_COUNT,
    RECORDER_BUILDING_AT,
    RECORDER_BUILDING_AT_HEX,
    RECORDER_FIRE_VIEW,
    RECORDER_CITY_SIEGE,
    RECORDER_OFFICER_DETAIL,
    RECORDER_SKILL_KIND_COUNT,
    RECORDER_SKILL_KIND_AT,
    RECORDER_POST_KIND_COUNT,
    RECORDER_POST_KIND_AT,
    RECORDER_RANK_KIND_COUNT,
    RECORDER_RANK_KIND_AT,
    RECORDER_FACTION_RANK,
    RECORDER_POST_HOLDER,
    RECORDER_ITEM_COUNT,
    RECORDER_ITEM_AT,
    RECORDER_DIPLOMACY,
    RECORDER_DUEL_VIEW,
    RECORDER_DEBATE_VIEW,
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
    RECORDER_COMMAND_RESEARCH,
    RECORDER_COMMAND_MERGE,
    RECORDER_START_DISASTER,
    RECORDER_SET_SETTINGS,
    RECORDER_GRANT_TECHNIQUE,
    RECORDER_SET_TECHNIQUE_POINTS,
    RECORDER_SET_DATE,
    RECORDER_COMMAND_SORTIE,
    RECORDER_COMMAND_TRANSPORT,
    RECORDER_COMMAND_MOVE,
    RECORDER_COMMAND_WAIT,
    RECORDER_COMMAND_RETREAT,
    RECORDER_COMMAND_SUPPLY,
    RECORDER_COMMAND_ATTACK,
    RECORDER_COMMAND_JOINT_ATTACK,
    RECORDER_COMMAND_TACTIC,
    RECORDER_COMMAND_PLOT,
    RECORDER_COMMAND_BUILD,
    RECORDER_COMMAND_REPAIR,
    RECORDER_COMMAND_DISMANTLE,
    RECORDER_PLACE_BUILDING,
    RECORDER_SET_UNIT,
    RECORDER_SET_UNIT_PLACE,
    RECORDER_SET_CITY_DURABILITY,
    RECORDER_COMMAND_COURT,       /* arg = the engine_command_kind */
    RECORDER_COMMAND_DUEL,
    RECORDER_COMMAND_DUEL_ACTION, /* arg = action | value << 8 */
    RECORDER_COMMAND_DEBATE_ACTION,
    RECORDER_START_DEBATE,
    RECORDER_SET_OFFICER,
    RECORDER_SET_ITEM_OWNER,
    RECORDER_SET_DIPLOMACY,
    RECORDER_SET_STAMINA,
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
    engine_command last_research;     /* the arguments of the last command_research */
    engine_command last_merge;        /* the arguments of the last command_merge (slot = absorber) */
    engine_settings settings;         /* the answer of settings; set_settings changes it */
    u8            outcome_kind;       /* script the answer of outcome */
    engine_sortie_order last_sortie;  /* the order of the last command_sortie or command_transport */
    engine_command last_unit_command; /* the last move, wait, retreat or supply: unit, q, r, target_unit, amounts */
    hex_coord     reach_cells[3];
    u32           reach_costs[3];
    u8            fire[64];           /* the fire_view answer: one burning cell */
    b32           duel_active;        /* M7: the duel_view and debate_view answers */
    b32           debate_active;
    engine_court_order last_court;    /* the order of the last command_court */
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
    if (slot == 2 || slot == 3)   /* two built markets: the merge dialog lists them */
    {
        facility.state    = SIM_SLOT_BUILT;
        facility.facility = 0;
        facility.level    = 1;
    }
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

/* ---- M4 growth: two lines of four techniques, four disasters ------------ */

global_variable const char *recorder_technique_keys[] = {
    "pike_drill", "grain_raid", "ambush", "elite_pike", "veterans", "rough_march", "army_reform", "ladders",
};
global_variable const char *recorder_disaster_keys[] = { "locusts", "plague", "bandits", "good_harvest" };

internal u32 recorder_technique_kind_count(void *o)
{
    recorder_record((engine_recorder *)o, RECORDER_TECHNIQUE_KIND_COUNT, 0);
    return ArrayCount(recorder_technique_keys);
}
internal engine_technique_kind recorder_technique_kind_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_TECHNIQUE_KIND_AT, index);
    engine_technique_kind kind = {};
    kind.id = ENGINE_NO_TECHNIQUE;
    if (index >= ArrayCount(recorder_technique_keys)) return kind;
    u32 levels_points[4] = { 1000, 2000, 3000, 5000 }, levels_gold[4] = { 1000, 2000, 5000, 10000 };
    kind.id       = (u8)index;
    snprintf(kind.key, sizeof(kind.key), "%s", recorder_technique_keys[index]);
    kind.line     = (u8)(index < 4 ? TECHNIQUE_LINE_PIKE : TECHNIQUE_LINE_DRILL);
    kind.level    = index % 4 + 1;
    kind.points   = levels_points[index % 4];
    kind.gold     = levels_gold[index % 4];
    kind.ability  = (u8)(index < 4 ? CONTENT_ABILITY_LEADERSHIP : CONTENT_ABILITY_FORCE);
    kind.min_days = 40;
    kind.max_days = 60;
    kind.previous = index % 4 ? (u8)(index - 1) : ENGINE_NO_TECHNIQUE;
    return kind;
}
internal engine_faction_techniques recorder_faction_techniques(void *o, u8 force)
{
    recorder_record((engine_recorder *)o, RECORDER_FACTION_TECHNIQUES, force);
    engine_faction_techniques techniques = {};
    techniques.force_id         = force;
    techniques.owned            = 1;   /* the first technique */
    techniques.technique_points = 4000;
    techniques.researching      = ENGINE_NO_TECHNIQUE;
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place) techniques.officers[place] = ENGINE_NO_OFFICER;
    return techniques;
}
internal u32 recorder_disaster_kind_count(void *o)
{
    recorder_record((engine_recorder *)o, RECORDER_DISASTER_KIND_COUNT, 0);
    return ArrayCount(recorder_disaster_keys);
}
internal engine_disaster_kind recorder_disaster_kind_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_DISASTER_KIND_AT, index);
    engine_disaster_kind kind = {};
    kind.id = ENGINE_NO_DISASTER;
    if (index >= ArrayCount(recorder_disaster_keys)) return kind;
    kind.id = (u8)index;
    snprintf(kind.key, sizeof(kind.key), "%s", recorder_disaster_keys[index]);
    kind.chance_percent = 5;
    kind.month_mask     = 1u << 1;
    return kind;
}
/* City 1 has locusts for 3 more turns; the other cities have none. */
internal engine_city_disasters recorder_city_disasters(void *o, u8 city)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_CITY_DISASTERS, city);
    engine_city_disasters disasters = {};
    disasters.city_id = city < r->city_count ? city : ENGINE_NO_CITY;
    if (city == 1)
    {
        disasters.turns[0] = 3;
        disasters.running  = 1;
    }
    return disasters;
}
internal engine_outcome recorder_outcome(void *o)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_OUTCOME, 0);
    engine_outcome outcome = {};
    outcome.kind     = r->outcome_kind;
    outcome.force_id = r->outcome_kind ? 0 : ENGINE_NO_FACTION;
    outcome.end_year = 351;
    return outcome;
}
internal engine_settings recorder_settings(void *o)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_SETTINGS, 0);
    return r->settings;
}
internal engine_command_result recorder_command_research(void *o, u8 city, u8 technique, const u16 officers[SIM_DEVELOP_OFFICERS])
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_RESEARCH, city);
    r->last_research = {};
    r->last_research.kind      = ENGINE_COMMAND_RESEARCH;
    r->last_research.city_id   = city;
    r->last_research.technique = technique;
    for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place) r->last_research.officers[place] = officers[place];
    engine_command_result result = {};
    result.refusal = r->can_issue_refusal;
    if (result.refusal == SIM_REFUSAL_NONE) result.command_id = r->next_command_id++;
    return result;
}
internal engine_command_result recorder_command_merge(void *o, u8 city, u8 absorber, u8 absorbed, u16 officer)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_MERGE, city);
    r->last_merge = {};
    r->last_merge.kind        = ENGINE_COMMAND_MERGE;
    r->last_merge.city_id     = city;
    r->last_merge.slot        = absorber;
    r->last_merge.absorbed    = absorbed;
    r->last_merge.officers[0] = officer;
    engine_command_result result = {};
    result.refusal = r->can_issue_refusal;
    if (result.refusal == SIM_REFUSAL_NONE) result.command_id = r->next_command_id++;
    return result;
}
internal engine_command_result recorder_start_disaster(void *o, u8 city, u8 disaster)
{
    (void)disaster;
    return recorder_city_command((engine_recorder *)o, RECORDER_START_DISASTER, ENGINE_COMMAND_KIND_COUNT, city, 0, ENGINE_NO_WEAPON, 0, 0);
}
internal void recorder_set_settings(void *o, engine_settings settings)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_SET_SETTINGS, settings.disasters ? 1 : 0);
    r->settings = settings;
}
internal void recorder_grant_technique(void *o, u8 force, u8 technique) { (void)technique; recorder_record((engine_recorder *)o, RECORDER_GRANT_TECHNIQUE, force); }
internal void recorder_set_technique_points(void *o, u8 force, u32 points) { (void)points; recorder_record((engine_recorder *)o, RECORDER_SET_TECHNIQUE_POINTS, force); }
internal void recorder_set_date(void *o, u16 year, u8 month, u8 day) { (void)month; (void)day; recorder_record((engine_recorder *)o, RECORDER_SET_DATE, year); }

/* ---- M5 march: two units of the acting force (1) on the 8 x 6 map ------- */
/* Unit 0: a pike unit at (1, 2) with officers 100 and 101; its range is
   (1, 2), (2, 2) and (3, 2). Unit 1: a transport unit at (1, 3). */

global_variable const char *recorder_unit_type_keys[] = {
    "sword", "pike", "halberd", "crossbow", "cavalry", "ram", "wood_beast", "tower", "catapult", "boat", "tower_ship", "battle_ship", "transport",
};

internal u32 recorder_terrain_kind_count(void *o) { recorder_record((engine_recorder *)o, RECORDER_TERRAIN_KIND_COUNT, 0); return 2; }
internal engine_terrain_kind recorder_terrain_kind_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_TERRAIN_KIND_AT, index);
    engine_terrain_kind kind = {};
    kind.id = index < 2 ? (u8)index : ENGINE_NO_TERRAIN;
    snprintf(kind.key, sizeof(kind.key), "%s", index == 0 ? "grass" : "forest");
    kind.cost[MOVEMENT_FOOT] = 4;
    return kind;
}
internal u32 recorder_unit_type_count(void *o) { recorder_record((engine_recorder *)o, RECORDER_UNIT_TYPE_COUNT, 0); return ArrayCount(recorder_unit_type_keys); }
internal engine_unit_type recorder_unit_type_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_UNIT_TYPE_AT, index);
    engine_unit_type type = {};
    type.id = ENGINE_NO_UNIT_TYPE;
    if (index >= ArrayCount(recorder_unit_type_keys)) return type;
    type.id = (u8)index;
    snprintf(type.key, sizeof(type.key), "%s", recorder_unit_type_keys[index]);
    type.move      = 22;
    type.transport = index == 12;
    type.weapon    = ENGINE_NO_WEAPON;
    return type;
}
internal u32 recorder_unit_count(void *o) { recorder_record((engine_recorder *)o, RECORDER_UNIT_COUNT, 0); return 2; }
internal engine_unit recorder_unit_at(void *o, u32 unit_id)
{
    recorder_record((engine_recorder *)o, RECORDER_UNIT_AT, unit_id);
    engine_unit unit = {};
    unit.id = unit_id < 2 ? unit_id : ENGINE_NO_UNIT;
    if (unit.id == ENGINE_NO_UNIT) return unit;
    unit.force_id    = 1;
    unit.q           = 1;
    unit.r           = unit_id == 0 ? 2 : 3;
    unit.unit_type   = unit_id == 0 ? 1 : 12;
    unit.officers[0] = unit_id == 0 ? 100 : 102;
    unit.officers[1] = unit_id == 0 ? 101 : ENGINE_NO_OFFICER;
    unit.officers[2] = ENGINE_NO_OFFICER;
    unit.soldiers = 1000; unit.gold = 500; unit.grain = 3000; unit.morale = 50;
    unit.home_city = 0; unit.destination = ENGINE_NO_CITY;
    unit.values.move = 22; unit.values.aptitude = 2;
    return unit;
}
internal u32 recorder_unit_at_hex(void *o, s32 q, s32 r)
{
    recorder_record((engine_recorder *)o, RECORDER_UNIT_AT_HEX, (u32)(q & 0xFFFF) | ((u32)(r & 0xFFFF) << 16));
    if (q == 1 && r == 2) return 0;
    if (q == 1 && r == 3) return 1;
    return ENGINE_NO_UNIT;
}
internal engine_reach_view recorder_reachable(void *o, u32 unit_id)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_REACHABLE, unit_id);
    r->reach_cells[0] = hex_make(1, 2); r->reach_cells[1] = hex_make(2, 2); r->reach_cells[2] = hex_make(3, 2);
    r->reach_costs[0] = 0; r->reach_costs[1] = 4; r->reach_costs[2] = 8;
    engine_reach_view view = {};
    view.unit  = unit_id;
    view.count = unit_id == 0 ? 3 : 1;
    view.cells = r->reach_cells;
    view.costs = r->reach_costs;
    return view;
}
internal engine_unit_values recorder_unit_values(void *o, u8 force, u8 unit_type, const u16 officers[SIM_COMMAND_OFFICERS])
{
    (void)force; (void)officers;
    recorder_record((engine_recorder *)o, RECORDER_UNIT_VALUES, unit_type);
    engine_unit_values values = {};
    values.attack = 70; values.defence = 70; values.intellect = 60; values.build = 50; values.move = 22; values.aptitude = 2;
    return values;
}
internal engine_sortie_order recorder_sortie_proposal(void *o, u8 city_id, u8 unit_type)
{
    recorder_record((engine_recorder *)o, RECORDER_SORTIE_PROPOSAL, city_id);
    engine_sortie_order order = {};
    order.city_id = city_id; order.unit_type = unit_type;
    order.officers[0] = 100; order.officers[1] = ENGINE_NO_OFFICER; order.officers[2] = ENGINE_NO_OFFICER;
    order.soldiers = 5000; order.grain = 1500; order.destination = ENGINE_NO_CITY;
    return order;
}
internal engine_command_result recorder_unit_result(engine_recorder *r)
{
    engine_command_result result = {};
    result.refusal = r->can_issue_refusal;
    if (result.refusal == SIM_REFUSAL_NONE) result.command_id = r->next_command_id++;
    return result;
}
internal engine_command_result recorder_command_sortie(void *o, const engine_sortie_order *order)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_SORTIE, order->city_id);
    r->last_sortie = *order;
    return recorder_unit_result(r);
}
internal engine_command_result recorder_command_transport(void *o, const engine_sortie_order *order)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_TRANSPORT, order->city_id);
    r->last_sortie = *order;
    return recorder_unit_result(r);
}
internal engine_command_result recorder_command_move(void *o, u32 unit_id, s32 q, s32 rr)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_MOVE, unit_id);
    r->last_unit_command = {};
    r->last_unit_command.kind = ENGINE_COMMAND_MOVE;
    r->last_unit_command.unit = unit_id; r->last_unit_command.q = q; r->last_unit_command.r = rr;
    return recorder_unit_result(r);
}
internal engine_command_result recorder_command_wait(void *o, u32 unit_id)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_WAIT, unit_id);
    return recorder_unit_result(r);
}
internal engine_command_result recorder_command_retreat(void *o, u32 unit_id)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_RETREAT, unit_id);
    return recorder_unit_result(r);
}
internal engine_command_result recorder_command_supply(void *o, u32 unit_id, u32 target, u32 soldiers, u32 gold, u32 grain)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_SUPPLY, unit_id);
    r->last_unit_command = {};
    r->last_unit_command.kind = ENGINE_COMMAND_SUPPLY;
    r->last_unit_command.unit = unit_id; r->last_unit_command.target_unit = target;
    r->last_unit_command.soldiers = soldiers; r->last_unit_command.gold = gold; r->last_unit_command.grain = grain;
    return recorder_unit_result(r);
}

/* M6: two tactics (突刺 for pikes, the tower arrow), two plots, two
   building kinds (a camp and a natural dike), one camp on the map at
   (2, 3) and one burning cell. */
internal u32 recorder_tactic_kind_count(void *o) { recorder_record((engine_recorder *)o, RECORDER_TACTIC_KIND_COUNT, 0); return 2; }
internal engine_tactic_kind recorder_tactic_kind_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_TACTIC_KIND_AT, index);
    engine_tactic_kind kind = {};
    kind.id = index < 2 ? (u8)index : ENGINE_NO_ROW;
    snprintf(kind.key, sizeof(kind.key), "%s", index == 0 ? "pierce" : "tower_arrow");
    kind.unit_types = index == 0 ? (1u << 1) : (1u << 7);
    kind.aptitude = index == 0 ? 1 : 0; kind.morale = index == 0 ? 15 : 10; kind.range_min = 1; kind.range_max = index == 0 ? 1 : 2;
    return kind;
}
internal u32 recorder_plot_kind_count(void *o) { recorder_record((engine_recorder *)o, RECORDER_PLOT_KIND_COUNT, 0); return 2; }
internal engine_plot_kind recorder_plot_kind_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_PLOT_KIND_AT, index);
    engine_plot_kind kind = {};
    kind.id = index < 2 ? (u8)index : ENGINE_NO_ROW;
    snprintf(kind.key, sizeof(kind.key), "%s", index == 0 ? "fire" : "confusion");
    kind.morale = index == 0 ? 10 : 15; kind.range = index == 0 ? 1 : 2;
    return kind;
}
internal u32 recorder_building_kind_count(void *o) { recorder_record((engine_recorder *)o, RECORDER_BUILDING_KIND_COUNT, 0); return 2; }
internal engine_building_kind recorder_building_kind_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_BUILDING_KIND_AT, index);
    engine_building_kind kind = {};
    kind.id = index < 2 ? (u8)index : ENGINE_NO_ROW;
    snprintf(kind.key, sizeof(kind.key), "%s", index == 0 ? "camp" : "dike");
    kind.durability = 1100; kind.gold = index == 0 ? 500 : 0; kind.natural = index == 1; kind.technique = ENGINE_NO_TECHNIQUE;
    return kind;
}
internal u32 recorder_building_count(void *o) { recorder_record((engine_recorder *)o, RECORDER_BUILDING_COUNT, 0); return 1; }
internal engine_building recorder_building_at(void *o, u32 building_id)
{
    recorder_record((engine_recorder *)o, RECORDER_BUILDING_AT, building_id);
    engine_building building = {};
    building.id = building_id == 0 ? 0 : ENGINE_NO_BUILDING;
    building.kind = 0; building.force_id = 1; building.q = 2; building.r = 3; building.durability = 600; building.durability_max = 1100;
    building.city_id = ENGINE_NO_CITY;
    return building;
}
internal u32 recorder_building_at_hex(void *o, s32 q, s32 r)
{
    recorder_record((engine_recorder *)o, RECORDER_BUILDING_AT_HEX, 0);
    return q == 2 && r == 3 ? 0 : ENGINE_NO_BUILDING;
}
internal engine_fire_view recorder_fire_view(void *o)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_FIRE_VIEW, 0);
    for (u32 i = 0; i < ArrayCount(r->fire); ++i) r->fire[i] = 0;
    r->fire[9] = 2;
    engine_fire_view view = {};
    view.width = 8; view.height = 8; view.turns = r->fire;
    return view;
}
internal engine_city_siege recorder_city_siege(void *o, u8 city_id)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_CITY_SIEGE, city_id);
    engine_city_siege siege = {};
    siege.city_id = city_id < r->city_count ? city_id : ENGINE_NO_CITY;
    siege.durability = 2000; siege.durability_max = 3000; siege.soldiers = 5000;
    return siege;
}
internal engine_command_result recorder_unit_target(engine_recorder *r, recorder_call_kind kind, engine_command_kind command, u32 unit_id, u8 row, s32 q, s32 rr)
{
    recorder_record(r, kind, unit_id);
    r->last_unit_command = {};
    r->last_unit_command.kind = command;
    r->last_unit_command.unit = unit_id; r->last_unit_command.row = row; r->last_unit_command.q = q; r->last_unit_command.r = rr;
    return recorder_unit_result(r);
}
internal engine_command_result recorder_command_attack(void *o, u32 unit_id, s32 q, s32 r)
{ return recorder_unit_target((engine_recorder *)o, RECORDER_COMMAND_ATTACK, ENGINE_COMMAND_ATTACK, unit_id, 0, q, r); }
internal engine_command_result recorder_command_joint_attack(void *o, u32 unit_id, s32 q, s32 r)
{ return recorder_unit_target((engine_recorder *)o, RECORDER_COMMAND_JOINT_ATTACK, ENGINE_COMMAND_JOINT_ATTACK, unit_id, 0, q, r); }
internal engine_command_result recorder_command_tactic(void *o, u32 unit_id, u8 row, s32 q, s32 r)
{ return recorder_unit_target((engine_recorder *)o, RECORDER_COMMAND_TACTIC, ENGINE_COMMAND_TACTIC, unit_id, row, q, r); }
internal engine_command_result recorder_command_plot(void *o, u32 unit_id, u8 row, s32 q, s32 r)
{ return recorder_unit_target((engine_recorder *)o, RECORDER_COMMAND_PLOT, ENGINE_COMMAND_PLOT, unit_id, row, q, r); }
internal engine_command_result recorder_command_build(void *o, u32 unit_id, u8 row, s32 q, s32 r)
{ return recorder_unit_target((engine_recorder *)o, RECORDER_COMMAND_BUILD, ENGINE_COMMAND_BUILD, unit_id, row, q, r); }
internal engine_command_result recorder_command_repair(void *o, u32 unit_id, s32 q, s32 r)
{ return recorder_unit_target((engine_recorder *)o, RECORDER_COMMAND_REPAIR, ENGINE_COMMAND_REPAIR, unit_id, 0, q, r); }
internal engine_command_result recorder_command_dismantle(void *o, u32 unit_id, s32 q, s32 r)
{ return recorder_unit_target((engine_recorder *)o, RECORDER_COMMAND_DISMANTLE, ENGINE_COMMAND_DISMANTLE, unit_id, 0, q, r); }
internal u32 recorder_place_building(void *o, u8 kind, u8 force_id, s32 q, s32 r)
{
    (void)force_id; (void)q; (void)r;
    recorder_record((engine_recorder *)o, RECORDER_PLACE_BUILDING, kind);
    return 0;
}
internal void recorder_set_unit(void *o, u32 unit_id, u32 soldiers, u32 morale)
{
    (void)soldiers; (void)morale;
    recorder_record((engine_recorder *)o, RECORDER_SET_UNIT, unit_id);
}
internal b32 recorder_set_unit_place(void *o, u32 unit_id, u8 force_id, s32 q, s32 r)
{
    (void)force_id; (void)q; (void)r;
    recorder_record((engine_recorder *)o, RECORDER_SET_UNIT_PLACE, unit_id);
    return true;
}
internal void recorder_set_city_durability(void *o, u8 city_id, u32 durability)
{
    (void)durability;
    recorder_record((engine_recorder *)o, RECORDER_SET_CITY_DURABILITY, city_id);
}

/* M7: officer 100 is the selected officer; two posts, one rank, one item;
   faction 2 is the other faction. */
internal engine_officer_detail recorder_officer_detail(void *o, u16 officer_id)
{
    recorder_record((engine_recorder *)o, RECORDER_OFFICER_DETAIL, officer_id);
    engine_officer_detail detail = {};
    detail.officer_id = officer_id >= 100 && officer_id < 103 ? officer_id : ENGINE_NO_OFFICER;
    detail.loyalty = 80; detail.ambition = 2; detail.justice = 2; detail.stamina = 100; detail.post = ENGINE_NO_ROW; detail.soldier_cap = 5000;
    detail.former_force = ENGINE_NO_FACTION; detail.age = 30;
    for (u32 slot = 0; slot < OFFICER_RELATION_SLOTS; ++slot) detail.liked[slot] = detail.disliked[slot] = ENGINE_NO_OFFICER;
    detail.recruit_chance = officer_id == 102 ? 60 : 0;
    return detail;
}
internal u32 recorder_skill_kind_count(void *o) { recorder_record((engine_recorder *)o, RECORDER_SKILL_KIND_COUNT, 0); return 0; }
internal engine_skill_kind recorder_skill_kind_at(void *o, u32 index) { recorder_record((engine_recorder *)o, RECORDER_SKILL_KIND_AT, index); engine_skill_kind kind = {}; return kind; }
internal u32 recorder_post_kind_count(void *o) { recorder_record((engine_recorder *)o, RECORDER_POST_KIND_COUNT, 0); return 2; }
internal engine_post_kind recorder_post_kind_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_POST_KIND_AT, index);
    engine_post_kind kind = {};
    kind.id = index < 2 ? (u8)index : ENGINE_NO_ROW;
    snprintf(kind.key, sizeof(kind.key), "post_%02u", index);
    kind.merit = index ? 4000 : 0; kind.soldiers = 6000; kind.ability = CONTENT_ABILITY_POLITICS; kind.bonus = 1;
    return kind;
}
internal u32 recorder_rank_kind_count(void *o) { recorder_record((engine_recorder *)o, RECORDER_RANK_KIND_COUNT, 0); return 1; }
internal engine_rank_kind recorder_rank_kind_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_RANK_KIND_AT, index);
    engine_rank_kind kind = {};
    kind.id = 0; snprintf(kind.key, sizeof(kind.key), "none"); kind.soldiers = 10000;
    return kind;
}
internal u8  recorder_faction_rank(void *o, u8 force) { recorder_record((engine_recorder *)o, RECORDER_FACTION_RANK, force); return 0; }
internal u16 recorder_post_holder(void *o, u8 force, u8 post) { (void)force; recorder_record((engine_recorder *)o, RECORDER_POST_HOLDER, post); return ENGINE_NO_OFFICER; }
internal u32 recorder_item_count(void *o) { recorder_record((engine_recorder *)o, RECORDER_ITEM_COUNT, 0); return 1; }
internal engine_item recorder_item_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_ITEM_AT, index);
    engine_item item = {};
    item.id = index == 0 ? 0 : ENGINE_NO_ROW;
    item.name[0] = 'A'; item.strength = 10; item.owner = 101; item.city = ENGINE_NO_CITY;
    return item;
}
internal engine_diplomacy recorder_diplomacy(void *o, u8 a, u8 b)
{
    (void)a;
    recorder_record((engine_recorder *)o, RECORDER_DIPLOMACY, b);
    engine_diplomacy state = {};
    state.friendliness = 50; state.level = 2;
    return state;
}
internal engine_duel_view recorder_duel_view(void *o)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_DUEL_VIEW, 0);
    engine_duel_view view = {};
    view.active = r->duel_active;
    for (u32 index = 0; index < 2; ++index)
    {
        view.side[index].officers[0] = (u16)(100 + index);
        view.side[index].officers[1] = view.side[index].officers[2] = ENGINE_NO_OFFICER;
        view.side[index].stamina[0]  = 80;
        view.side[index].spirit      = 120;
    }
    view.ready[DUEL_FINISHER] = r->duel_active;
    view.ready[DUEL_RETREAT]  = r->duel_active;
    return view;
}
internal engine_debate_view recorder_debate_view(void *o)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_DEBATE_VIEW, 0);
    engine_debate_view view = {};
    view.active = r->debate_active;
    for (u32 index = 0; index < 2; ++index)
    {
        view.side[index].officer    = (u16)(100 + index);
        view.side[index].hand_count = 3;
        view.side[index].mind       = 100;
        view.side[index].rocks      = 5;
        for (u32 place = 0; place < 3; ++place) view.side[index].hand[place] = sim_card_make(CARD_TOPIC, (u8)place, 2);
        view.last[index].type = CARD_TYPE_COUNT;
    }
    return view;
}
internal engine_command_result recorder_command_court(void *o, engine_command_kind kind, const engine_court_order *order)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_COURT, kind);
    r->last_court = *order;
    return recorder_unit_result(r);
}
internal engine_command_result recorder_command_duel(void *o, u32 unit_id, s32 q, s32 r)
{ return recorder_unit_target((engine_recorder *)o, RECORDER_COMMAND_DUEL, ENGINE_COMMAND_DUEL, unit_id, 0, q, r); }
internal engine_command_result recorder_command_duel_action(void *o, u8 action, u8 value)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_DUEL_ACTION, (u32)action | ((u32)value << 8));
    return recorder_unit_result(r);
}
internal engine_command_result recorder_command_debate_action(void *o, u8 action, u8 value)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_DEBATE_ACTION, (u32)action | ((u32)value << 8));
    return recorder_unit_result(r);
}
internal engine_command_result recorder_start_debate(void *o, u16 a, u16 b)
{
    (void)b;
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_START_DEBATE, a);
    return recorder_unit_result(r);
}
internal void recorder_set_officer(void *o, u16 officer_id, const engine_officer_edit *edit) { (void)edit; recorder_record((engine_recorder *)o, RECORDER_SET_OFFICER, officer_id); }
internal void recorder_set_item_owner(void *o, u8 item, u16 officer_id) { (void)officer_id; recorder_record((engine_recorder *)o, RECORDER_SET_ITEM_OWNER, item); }
internal void recorder_set_diplomacy(void *o, u8 a, u8 b, u32 friendliness, u8 relation, u32 months)
{
    (void)a; (void)friendliness; (void)relation; (void)months;
    recorder_record((engine_recorder *)o, RECORDER_SET_DIPLOMACY, b);
}
internal void recorder_set_stamina(void *o, u16 officer_id, u8 stamina) { (void)stamina; recorder_record((engine_recorder *)o, RECORDER_SET_STAMINA, officer_id); }

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
    api.technique_kind_count = recorder_technique_kind_count;
    api.technique_kind_at    = recorder_technique_kind_at;
    api.faction_techniques   = recorder_faction_techniques;
    api.disaster_kind_count  = recorder_disaster_kind_count;
    api.disaster_kind_at     = recorder_disaster_kind_at;
    api.city_disasters       = recorder_city_disasters;
    api.outcome              = recorder_outcome;
    api.settings             = recorder_settings;
    api.command_research     = recorder_command_research;
    api.command_merge        = recorder_command_merge;
    api.start_disaster       = recorder_start_disaster;
    api.set_settings         = recorder_set_settings;
    api.grant_technique      = recorder_grant_technique;
    api.set_technique_points = recorder_set_technique_points;
    api.set_date             = recorder_set_date;
    api.terrain_kind_count   = recorder_terrain_kind_count;
    api.terrain_kind_at      = recorder_terrain_kind_at;
    api.unit_type_count      = recorder_unit_type_count;
    api.unit_type_at         = recorder_unit_type_at;
    api.unit_count           = recorder_unit_count;
    api.unit_at              = recorder_unit_at;
    api.unit_at_hex          = recorder_unit_at_hex;
    api.reachable            = recorder_reachable;
    api.unit_values          = recorder_unit_values;
    api.sortie_proposal      = recorder_sortie_proposal;
    api.command_sortie       = recorder_command_sortie;
    api.command_transport    = recorder_command_transport;
    api.command_move         = recorder_command_move;
    api.command_wait         = recorder_command_wait;
    api.command_retreat      = recorder_command_retreat;
    api.command_supply       = recorder_command_supply;
    api.tactic_kind_count    = recorder_tactic_kind_count;
    api.tactic_kind_at       = recorder_tactic_kind_at;
    api.plot_kind_count      = recorder_plot_kind_count;
    api.plot_kind_at         = recorder_plot_kind_at;
    api.building_kind_count  = recorder_building_kind_count;
    api.building_kind_at     = recorder_building_kind_at;
    api.building_count       = recorder_building_count;
    api.building_at          = recorder_building_at;
    api.building_at_hex      = recorder_building_at_hex;
    api.fire_view            = recorder_fire_view;
    api.city_siege           = recorder_city_siege;
    api.command_attack       = recorder_command_attack;
    api.command_joint_attack = recorder_command_joint_attack;
    api.command_tactic       = recorder_command_tactic;
    api.command_plot         = recorder_command_plot;
    api.command_build        = recorder_command_build;
    api.command_repair       = recorder_command_repair;
    api.command_dismantle    = recorder_command_dismantle;
    api.place_building       = recorder_place_building;
    api.set_unit             = recorder_set_unit;
    api.set_unit_place       = recorder_set_unit_place;
    api.set_city_durability  = recorder_set_city_durability;
    api.officer_detail        = recorder_officer_detail;
    api.skill_kind_count      = recorder_skill_kind_count;
    api.skill_kind_at         = recorder_skill_kind_at;
    api.post_kind_count       = recorder_post_kind_count;
    api.post_kind_at          = recorder_post_kind_at;
    api.rank_kind_count       = recorder_rank_kind_count;
    api.rank_kind_at          = recorder_rank_kind_at;
    api.faction_rank          = recorder_faction_rank;
    api.post_holder           = recorder_post_holder;
    api.item_count            = recorder_item_count;
    api.item_at               = recorder_item_at;
    api.diplomacy             = recorder_diplomacy;
    api.duel_view             = recorder_duel_view;
    api.debate_view           = recorder_debate_view;
    api.command_court         = recorder_command_court;
    api.command_duel          = recorder_command_duel;
    api.command_duel_action   = recorder_command_duel_action;
    api.command_debate_action = recorder_command_debate_action;
    api.start_debate          = recorder_start_debate;
    api.set_officer           = recorder_set_officer;
    api.set_item_owner        = recorder_set_item_owner;
    api.set_diplomacy         = recorder_set_diplomacy;
    api.set_stamina           = recorder_set_stamina;
    return api;
}

#endif /* ENGINE_RECORDER_H */
