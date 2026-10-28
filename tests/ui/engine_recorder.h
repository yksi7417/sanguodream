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
    RECORDER_REGION_VIEW,      /* M21 */
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
    RECORDER_FACTION_COLOUR,       /* a query: arg = the force */
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
    RECORDER_PATH_TO,              /* G2-052: the plan of a unit to a hex */
    RECORDER_TRAVEL_DAYS,          /* G2-036: the days between two bases */
    RECORDER_COURT_ADVICE,         /* the clue of the adviser; arg = the engine_command_kind */
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
    RECORDER_BASE_COUNT,       /* M8 queries */
    RECORDER_BASE_AT,
    RECORDER_BASE_AT_HEX,
    RECORDER_DEPOT_AT,             /* arg = the location id */
    RECORDER_CORPS_AT,         /* arg = force | corps << 8 */
    RECORDER_CITY_CORPS,
    RECORDER_ADVISER,
    RECORDER_ADVICE,
    RECORDER_COUNCIL,          /* arg = force | city << 8 */
    RECORDER_AI_PLAN,
    RECORDER_EMPEROR,
    RECORDER_RELIC_COUNT,
    RECORDER_RELIC_AT,
    RECORDER_EVENT_KIND_COUNT,
    RECORDER_EVENT_KIND_AT,
    RECORDER_PENDING_EVENT,
    RECORDER_SAVE_SLOT,        /* M9 queries */
    RECORDER_REFRESH_SAVE_SLOTS,
    RECORDER_CHART_COUNT,
    RECORDER_CHART_ROW_AT,
    RECORDER_FACTION_CHART,
    RECORDER_ANCIENT_COUNT,
    RECORDER_ANCIENT_AT,
    RECORDER_NEW_OFFICER_COUNT,
    RECORDER_HISTORY_COUNT,
    RECORDER_HISTORY_AT,
    RECORDER_HISTORY_READY,
    RECORDER_SCENARIO_COUNT,
    RECORDER_SCENARIO_AT,
    RECORDER_BATTLE_COUNT,
    RECORDER_BATTLE_AT,
    RECORDER_SUBJECT_OF,
    RECORDER_EVENT_REPORT,     /* M18: a query */
    /* M10 queries */
    RECORDER_MAP_INFO,
    RECORDER_HEX_AT,               /* arg = the col | the row << 16 of the hex */
    RECORDER_ARMAMENT_COUNT,
    RECORDER_ARMAMENT_AT,
    RECORDER_MOVE_COST,
    RECORDER_BANDIT_SITES,         /* arg = the city */
    RECORDER_REPLAY_PAIR_COUNT,    /* M13 */
    RECORDER_REPLAY_PAIR_AT,       /* arg = the row */
    RECORDER_REPLAY_DIFF_AT,       /* arg = the index */
    RECORDER_AI_EXPLAIN,           /* M15: arg = the faction */
    RECORDER_INSTALL_FOLDER,       /* M16 */
    RECORDER_CONTENT_SOURCE,       /* arg = the asset kind */
    RECORDER_INSTALL_SCENARIO_COUNT,
    RECORDER_INSTALL_SCENARIO_AT,  /* arg = the index */
    RECORDER_GAME_REPLAY_INFO,     /* the replay of a game. The view and the close change what shows, not the game. */
    RECORDER_GAME_REPLAY_STEP_AT,  /* arg = the index */
    RECORDER_GAME_REPLAY_VIEW,     /* arg = the step */
    RECORDER_GAME_REPLAY_CLOSE,
    RECORDER_GAME_REPLAY_TURN_STEP, /* arg = the turn */
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
    RECORDER_COMMAND_DESTROY,      /* G2-052: arg = unit | target << 16 */
    RECORDER_COMMAND_ATTACK_FACILITY, /* G3-020: the long order on a facility; last_unit_command holds the hex */
    RECORDER_UNIT_GOAL,            /* G3-009: arg = the unit */
    RECORDER_COMMAND_DRIVE,
    RECORDER_COMMAND_FOLLOW,
    RECORDER_COMMAND_DELEGATE,
    RECORDER_COMMAND_MOVE_OFFICERS, /* G2-036: arg = the city */
    RECORDER_COMMAND_SUMMON,        /* G2-037 */
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
    RECORDER_COMMAND_CAPTIVE,     /* 俘虜處置: arg = the officer */
    RECORDER_COMMAND_DUEL,
    RECORDER_COMMAND_DUEL_ACTION, /* arg = action | value << 8 */
    RECORDER_COMMAND_DEBATE_ACTION,
    RECORDER_START_DEBATE,
    RECORDER_SET_OFFICER,
    RECORDER_SET_ITEM_OWNER,
    RECORDER_SET_DIPLOMACY,
    RECORDER_SET_STAMINA,
    RECORDER_COMMAND_CORPS,        /* arg = the sim_corps_action */
    RECORDER_COMMAND_ADVISER,      /* arg = the officer */
    RECORDER_COMMAND_COUNCIL,      /* arg = proposal | city << 8 */
    RECORDER_COMMAND_EVENT_CHOICE, /* arg = the choice */
    RECORDER_TRIGGER_EVENT,        /* arg = the event */
    RECORDER_SET_EMPEROR,
    RECORDER_SET_BASE,
    RECORDER_SAVE_GAME,            /* M9: arg = the slot */
    RECORDER_LOAD_GAME,            /* arg = the slot */
    RECORDER_CREATE_OFFICER,       /* arg = the type | the city << 8 */
    RECORDER_COMMAND_RESEARCH_ABILITY, /* arg = the row | the city << 8 */
    RECORDER_COMMAND_TRAIN_OFFICER,    /* arg = the row | the officer << 8 */
    RECORDER_TRIGGER_HISTORY,      /* arg = the row */
    RECORDER_LOAD_SCENARIO,
    RECORDER_START_FIRE,           /* M10: arg = the turns */
    RECORDER_REPLAY_PAIR,          /* M13: arg = the command kind */
    RECORDER_SET_UNIT_GRAIN,       /* M15: arg = the unit */
    RECORDER_SET_INSTALL_FOLDER,   /* M16: it loads the names of the install as a command */
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
    /* M20: the AI round that end_turn adds to the ledger, and a move of unit 0 */
    sim_event     round_events[8];
    u32           round_event_count;
    s32           round_unit_shift;   /* end_turn moves unit 0 this many hexes along q */
    s32           unit_shift;
    b32           unit0_rival;        /* unit 0 belongs to force 2 */
    b32           unit1_rival;       /* G2-045: unit 1 at 1,3 belongs to force 2 */
    u16           captive_officer;   /* 俘虜處置: officer_detail says that this officer is a captive; 0 = none */
    b32           unit1_goal;        /* G3-009: unit_goal of the rival unit 1 is known */
    u8            unit1_goal_city;   /* G3-009: its target city */
    u8            last_captive_choice;   /* the choice of the last command_captive */
    b32           units_acted;        /* M22: both units acted in this turn */
    u32           officer_count;      /* scripted rows for the Officers and Cities panels */
    u32           city_count;
    b32           has_water;          /* hex_at reports the river kind (2) at water_q, water_r */
    s32           water_q, water_r;
    char          last_load_path[RECORDER_MAX_PATH];
    b32           load_fails;         /* script a refused load */
    b32           has_names;          /* city_name_at answers only after a name index load */
    u8            foreign_city;       /* this scripted city belongs to force 2, not to force 1; 0 = none (city 0 is always force 1) */
    b32           own_base_hex;       /* hex_at reports location 42 and base_at_hex reports index 0 (recorder_base_at: force 1) at the axial hex (3, 2) */
    b32           pack_base_keys;     /* recorder_base_at gives keys without a UI string: the label takes the name index (docs/MAPS.md, W2) */
    u8            pack_rows;          /* recorder_scenario_at row 1: 0 a PK scenario; 1 y1200 of the pack medieval_europe; 2 y1200 of this pack */
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
    hex_coord     path_cells[2];      /* the answer of path_to */
    u8            unit_order;         /* the long order of both units (sim_unit_order_kind) */
    engine_move_order last_move;      /* the last 移動 or 召喚 */
    u32           reach_costs[3];
    u8            fire[64];           /* the fire_view answer: one burning cell */
    u8            fire_cell;          /* the burning cell; 0: cell 9 */
    u8            unit0_state;        /* the sim_unit_state of unit 0 */
    b32           duel_active;        /* M7: the duel_view and debate_view answers */
    b32           debate_active;
    engine_court_order last_court;    /* the order of the last command_court */
    b32           pending_active;     /* M8: the pending_event answer */
    sim_corps_order last_corps;       /* the order of the last command_corps */
    char          last_save_comment[ENGINE_SAVE_COMMENT_SIZE];   /* M9: the comment of the last save_game */
    sim_new_officer_order last_newcomer;   /* the order of the last create_officer */
    char          last_replay_before[RECORDER_MAX_PATH];   /* M13: the paths of the last replay_pair */
    char          last_replay_after[RECORDER_MAX_PATH];
    char          last_install_folder[RECORDER_MAX_PATH];   /* M16: the folder of the last set_install_folder */
    char          last_scenario_path[RECORDER_MAX_PATH];    /* the path of the last load_scenario */
    u8            portrait_source;    /* script the source of content_source(ENGINE_ASSET_PORTRAITS) */
    u8            unit0_effective_type;   /* U-003: the class that unit 0 fights with; 0 = its unit_type (the pike) */
    b32           building_complete;  /* U-001: the camp at (2, 3) is complete; false = a build in progress */
    b32           replay_viewing;     /* the replay of a game: a view is open */
    u32           replay_view_step;
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

/* Number of calls of a kind with this argument. */
internal u32
recorder_count_arg(engine_recorder *r, recorder_call_kind kind, u32 arg)
{
    u32 n = 0;
    for (u32 i = 0; i < r->call_count; ++i) if (r->calls[i].kind == kind && r->calls[i].arg == arg) n++;
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
    for (u32 index = 0; index < r->round_event_count && r->event_count < RECORDER_MAX_EVENTS; ++index)
    {
        r->events[r->event_count++] = r->round_events[index];   /* M20: the AI factions act inside end_turn */
    }
    r->unit_shift += r->round_unit_shift;
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
    officer.charm          = (u8)(40 + 10 * i);   /* M19: the charm rises and the force falls, so that a proposal has an order */
    officer.war            = (u8)(70 - 10 * i);
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

/* Force 1 is blue, force 2 red, every other force grey. */
internal u32 recorder_faction_colour(void *o, u8 force_id)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_FACTION_COLOUR, force_id);
    if (force_id == 1) return faction_rgb_to_rgba(0x2F5FD0u);
    if (force_id == 2) return faction_rgb_to_rgba(0xD02828u);
    return faction_rgb_to_rgba(FACTION_COLOUR_NONE_RGB);
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
    city.owner_force_id        = (u8)(r->foreign_city && i == r->foreign_city ? 2 : 1);   /* the force of the scripted officers */
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

/* M21: the scripted board has no regions. */
internal engine_region_view
recorder_region_view(void *o)
{
    recorder_record((engine_recorder *)o, RECORDER_REGION_VIEW, 0);
    engine_region_view view = {};
    view.width = 8; view.height = 6;
    for (u32 region = 0; region < ENGINE_REGION_CAPACITY; ++region) view.region_city[region] = ENGINE_NO_CITY;
    return view;
}

internal engine_terrain_view
recorder_terrain_view(void *o)
{
    recorder_record((engine_recorder *)o, RECORDER_TERRAIN_VIEW, 0);
    for (u32 i = 0; i < 48; ++i) { recorder_terrain_kind[i] = (i % 8 == 3) ? 3 : 1; recorder_terrain_level[i] = (i % 8 == 3) ? 2 : 1; }
    engine_terrain_view v;
    v.width = 8; v.height = 6; v.kind = recorder_terrain_kind; v.level = recorder_terrain_level; v.version = 1;
    v.tile = recorder_terrain_kind;   /* M10: the Hex map panel draws the tile colours */
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
#define RECORDER_BUILDING_SLOT 9   /* the slot with the farm under construction */
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
    if (command->kind == ENGINE_COMMAND_RESEARCH)
    {
        /* The scripted techniques: the faction has technique 0; a level
           above 1 needs the level before it (recorder_technique_kind_at). */
        u8 technique = command->technique;
        if (technique == 0)                              check.refusal = SIM_REFUSAL_VALUE_AT_MAXIMUM;
        else if (technique % 4 != 0 && technique != 1)   check.refusal = SIM_REFUSAL_NEEDS_TECHNIQUE;
    }
    else if (command->kind == ENGINE_COMMAND_SORTIE)
    {
        /* The empty dialog, no commander and no soldiers, has a grey 決定 (G2-044). The probe of
           the city menu sends one soldier, and the scripted refusal rules it. */
        if (command->sortie.officers[0] == ENGINE_NO_OFFICER && command->sortie.soldiers == 0) check.refusal = SIM_REFUSAL_NO_OFFICER;
    }
    else if (command->kind == ENGINE_COMMAND_ATTACK || command->kind == ENGINE_COMMAND_JOINT_ATTACK || command->kind == ENGINE_COMMAND_TACTIC ||
             command->kind == ENGINE_COMMAND_PLOT || command->kind == ENGINE_COMMAND_DUEL)
    {
        /* A target command works on the hex of the rival unit 1 only. */
        b32 at_rival = r->unit1_rival && command->q == 1 && command->r == 3;
        if (!at_rival) check.refusal = SIM_REFUSAL_UNREACHABLE;
    }
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
    slots.built = 2; slots.building = 1; slots.used = 3;   /* two built markets and a farm under construction (facility_at) */
    slots.gold_income = 100; slots.grain_income = 500; slots.governor_id = 100;
    slots.q = (s32)city_id * 2 + 1;   /* M11: each city has its own centre, so a frame is visible */
    slots.r = 1;
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
    facility.q        = (s32)(slot % 4) + 2;   /* on the 8 x 6 map of the recorder, so the scene draws the slot */
    facility.r        = 1;
    if (slot == 2 || slot == 3)   /* two built markets: the merge dialog lists them */
    {
        facility.state    = SIM_SLOT_BUILT;
        facility.facility = 0;
        facility.level    = 1;
    }
    if (slot == RECORDER_BUILDING_SLOT)   /* a farm under construction: the counts and the scene show it */
    {
        facility.state      = SIM_SLOT_BUILDING;
        facility.facility   = 1;
        facility.level      = 1;
        facility.turns_left = 2;
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
    economy.stocks[0] = 3000;   /* pikes: the pike class and its preset are white */
    economy.stocks[1] = 1000;   /* halberds */
    economy.stocks[4] = 2;      /* the shared stock of the siege weapons and the ships (recorder_weapon_kind_at) */
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
/* Entity 1 + i is the scripted city i; 1000 + i is the scripted officer 100 + i. */
internal engine_subject recorder_subject_of(void *o, u32 entity)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_SUBJECT_OF, entity);
    engine_subject subject = {};
    if (entity >= 1 && entity <= r->city_count)                      { subject.kind = ENGINE_SUBJECT_CITY;    subject.id = (u16)(entity - 1); }
    else if (entity >= 1000 && entity < 1000 + r->officer_count)     { subject.kind = ENGINE_SUBJECT_OFFICER; subject.id = (u16)(entity - 1000 + 100); }
    return subject;
}
#define RECORDER_CITY_COL 2
#define RECORDER_CITY_ROW 2
#define RECORDER_SLOT_COL 6     /* M19: the hex of slot RECORDER_SLOT_INDEX of city 0 */
#define RECORDER_SLOT_ROW 4
#define RECORDER_SLOT_INDEX 4

/* M18: the facts of a scripted event. The speaker is the first scripted
   officer, the tone is "calm", and an event that a test marks with
   detail 1 is a report. The sign follows the amount. */
internal engine_event_report recorder_event_report(void *o, u32 index)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_EVENT_REPORT, index);
    engine_event_report report = {};
    report.ledger_index = ENGINE_NO_EVENT;
    report.speaker      = ENGINE_NO_OFFICER;
    report.faction      = ENGINE_NO_FACTION;
    memcpy(report.tone, "calm", 5);
    if (index >= r->event_count) return report;
    sim_event *event = r->events + index;
    report.ledger_index = index;
    report.kind         = (u8)event->kind;
    report.key          = sim_event_name(event->kind);
    report.resource     = (u8)sim_event_resource(event->kind);
    report.resource_key = sim_resource_name(sim_event_resource(event->kind));
    report.turn         = event->turn;
    report.date.year    = 194;
    report.date.month   = 6;
    report.date.day     = 1;
    report.faction      = event->faction == SIM_NO_FORCE ? ENGINE_NO_FACTION : event->faction;
    report.delta        = event->delta;
    report.detail       = event->detail;
    report.cause        = event->cause;
    report.subject.kind = ENGINE_SUBJECT_NONE;
    if (event->subject >= 1 && event->subject <= r->city_count) { report.subject.kind = ENGINE_SUBJECT_CITY; report.subject.id = (u16)(event->subject - 1); }
    report.speaker      = r->officer_count ? (u16)100 : ENGINE_NO_OFFICER;
    report.sign         = (u8)(event->delta > 0 ? ENGINE_SIGN_GOOD : event->delta < 0 ? ENGINE_SIGN_BAD : ENGINE_SIGN_NEUTRAL);
    report.report       = event->detail == 1;
    report.log          = true;
    /* The place: a facility event of a city is at its slot hex, another
       event of a city at the city hex, as in the real engine. */
    report.place.thing   = ENGINE_NO_ROW;
    report.place.force   = ENGINE_NO_FACTION;
    report.place.officer = ENGINE_NO_OFFICER;
    if (report.subject.kind == ENGINE_SUBJECT_CITY)
    {
        b32 facility = event->kind == SIM_EVENT_FACILITY_REPAIRED || event->kind == SIM_EVENT_FACILITY_DAMAGED || event->kind == SIM_EVENT_FACILITY_DESTROYED;
        hex_offset cell = { facility ? RECORDER_SLOT_COL : RECORDER_CITY_COL, facility ? RECORDER_SLOT_ROW : RECORDER_CITY_ROW };
        hex_coord hex = hex_from_offset(cell);
        report.place.kind   = (u8)(facility ? ENGINE_PLACE_FACILITY : ENGINE_PLACE_CITY);
        report.place.id     = report.subject.id;
        report.place.slot   = (u8)(facility ? RECORDER_SLOT_INDEX : 0);
        report.place.thing  = (u8)(facility ? 0 : ENGINE_NO_ROW);   /* facility kind 0 of recorder_facility_kind_at */
        report.place.on_map = true;
        report.place.q      = hex.q;
        report.place.r      = hex.r;
    }
    return report;
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
    kind.id = index < 3 ? (u8)index : ENGINE_NO_TERRAIN;
    snprintf(kind.key, sizeof(kind.key), "%s", index == 0 ? "grass" : index == 1 ? "forest" : "river");
    if (index == 2) kind.cost[MOVEMENT_NAVY] = 5;   /* water: no foot cost, a navy cost */
    else            kind.cost[MOVEMENT_FOOT] = 4;
    kind.burnable = index < 2;                      /* grass and forest burn; water and the unknown column 3 do not */
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
    /* The weapon row of the class (recorder_weapon_keys): the sword, the boat and the transport carry none. */
    type.weapon    = (u8)(index >= 1 && index <= 8 ? index - 1 : index == 10 || index == 11 ? index - 2 : ENGINE_NO_WEAPON);
    type.movement  = (u8)(index >= 9 && index <= 11 ? MOVEMENT_NAVY : index == 4 ? MOVEMENT_CAVALRY : index >= 5 && index <= 8 ? MOVEMENT_SIEGE : MOVEMENT_FOOT);
    return type;
}
internal u32 recorder_unit_count(void *o) { recorder_record((engine_recorder *)o, RECORDER_UNIT_COUNT, 0); return 2; }
internal engine_unit recorder_unit_at(void *o, u32 unit_id)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_UNIT_AT, unit_id);
    engine_unit unit = {};
    unit.id = unit_id < 2 ? unit_id : ENGINE_NO_UNIT;
    if (unit.id == ENGINE_NO_UNIT) return unit;
    unit.force_id    = (u8)((unit_id == 0 && r->unit0_rival) || (unit_id == 1 && r->unit1_rival) ? 2 : 1);
    unit.q           = 1 + (unit_id == 0 ? r->unit_shift : 0);
    unit.r           = unit_id == 0 ? 2 : 3;
    unit.unit_type   = unit_id == 0 ? 1 : 12;
    unit.officers[0] = unit_id == 0 ? 100 : 102;
    unit.officers[1] = unit_id == 0 ? 101 : ENGINE_NO_OFFICER;
    unit.officers[2] = ENGINE_NO_OFFICER;
    unit.soldiers = 1000; unit.gold = 500; unit.grain = 3000; unit.morale = 50;
    unit.home_city = 0; unit.destination = ENGINE_NO_CITY;
    unit.values.move = 22; unit.values.aptitude = 2;
    unit.grain_turns = 120;          /* M12: 3000 grain at 25 each turn */
    unit.ship_type   = ENGINE_NO_UNIT_TYPE;
    unit.effective_type = unit_id == 0 && r->unit0_effective_type ? r->unit0_effective_type : unit.unit_type;
    unit.state       = unit_id == 0 ? r->unit0_state : 0;
    unit.acted       = r->units_acted;
    unit.order       = r->unit_order;
    unit.order_target = ENGINE_NO_UNIT;
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
/* G2-052, G2-058: the plan to a hex is two hexes, 30 points, 20 days; the hex is never in the reach. */
internal engine_path_view recorder_path_to(void *o, u32 unit_id, s32 q, s32 rr)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_PATH_TO, unit_id);
    engine_path_view view = {};
    view.unit  = unit_id;
    view.found = unit_id < 2;
    r->path_cells[0] = hex_make(2, 2);
    r->path_cells[1] = hex_make(q, rr);
    view.count = 2;
    view.cells = r->path_cells;
    view.cost  = 30;
    view.days  = 20;
    view.grain_days = 1200;
    return view;
}
internal engine_command_result recorder_command_target(engine_recorder *r, recorder_call_kind kind, engine_command_kind command, u32 unit_id, u32 target)
{
    recorder_record(r, kind, (unit_id & 0xFFFF) | (target << 16));
    r->last_unit_command = {};
    r->last_unit_command.kind        = command;
    r->last_unit_command.unit        = unit_id;
    r->last_unit_command.target_unit = target;
    return recorder_unit_result(r);
}
internal engine_command_result recorder_command_destroy(void *o, u32 unit_id, u32 target)
{
    return recorder_command_target((engine_recorder *)o, RECORDER_COMMAND_DESTROY, ENGINE_COMMAND_DESTROY, unit_id, target);
}
internal engine_command_result recorder_command_drive(void *o, u32 unit_id, u32 target)
{
    return recorder_command_target((engine_recorder *)o, RECORDER_COMMAND_DRIVE, ENGINE_COMMAND_DRIVE, unit_id, target);
}
internal engine_command_result recorder_command_follow(void *o, u32 unit_id, u32 target)
{
    return recorder_command_target((engine_recorder *)o, RECORDER_COMMAND_FOLLOW, ENGINE_COMMAND_FOLLOW, unit_id, target);
}
internal engine_command_result recorder_command_delegate(void *o)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_DELEGATE, 0);
    return recorder_unit_result(r);
}
internal engine_command_result recorder_command_move(void *o, u32 unit_id, s32 q, s32 rr)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_MOVE, unit_id);
    r->last_unit_command = {};
    r->last_unit_command.kind = ENGINE_COMMAND_MOVE;
    r->last_unit_command.unit = unit_id; r->last_unit_command.q = q; r->last_unit_command.r = rr;
    if (unit_id == 0 && r->can_issue_refusal == SIM_REFUSAL_NONE) r->unit_shift = q - 1;   /* unit 0 stands on the goal: the walk of ui_war.cpp has an end */
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

/* M6: three tactics (突刺 for pikes, the tower arrow, the ship arrow of the
   navy classes 9 to 11), two plots, two building kinds (a camp and a
   natural dike), one camp on the map at (2, 3) and one burning cell. */
internal u32 recorder_tactic_kind_count(void *o) { recorder_record((engine_recorder *)o, RECORDER_TACTIC_KIND_COUNT, 0); return 3; }
internal engine_tactic_kind recorder_tactic_kind_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_TACTIC_KIND_AT, index);
    engine_tactic_kind kind = {};
    kind.id = index < 3 ? (u8)index : ENGINE_NO_ROW;
    snprintf(kind.key, sizeof(kind.key), "%s", index == 0 ? "pierce" : (index == 1 ? "tower_arrow" : "ship_arrow"));
    kind.unit_types = index == 0 ? (1u << 1) : (index == 1 ? (1u << 7) : ((1u << 9) | (1u << 10) | (1u << 11)));
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
    engine_recorder *r = (engine_recorder *)o;
    building.kind = 0; building.force_id = 1; building.q = 2; building.r = 3; building.durability = r->building_complete ? 1100 : 600; building.durability_max = 1100;
    building.complete = r->building_complete;
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
    r->fire[r->fire_cell ? r->fire_cell : 9] = 2;
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
    detail.captive        = ((engine_recorder *)o)->captive_officer == officer_id && officer_id != 0;
    detail.reward_rise    = officer_id == 101 ? 8 : (officer_id == 102 ? 10 : 0);   /* G2-040: the ruler 100 gets no reward */
    return detail;
}

/* The estimate of the adviser 100: 探索 is low, 登庸 of 102 is middle. */
internal engine_court_advice recorder_court_advice(void *o, engine_command_kind kind, const engine_court_order *order)
{
    recorder_record((engine_recorder *)o, RECORDER_COURT_ADVICE, kind);
    engine_court_advice advice = {};
    advice.adviser = ENGINE_NO_OFFICER;
    if (kind == ENGINE_COMMAND_SEARCH) { advice.chance = 20; advice.clue = SIM_CLUE_LOW; advice.adviser = 100; }
    if (kind == ENGINE_COMMAND_RECRUIT && order->target == 102) { advice.chance = 60; advice.clue = SIM_CLUE_MIDDLE; advice.adviser = 100; }
    return advice;
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
/* G2-036, G2-037: one command for the officers of the order; the days between two bases are 10. */
internal engine_command_result recorder_command_move_officers(void *o, const engine_move_order *order)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_MOVE_OFFICERS, order->city_id);
    r->last_move = *order;
    return recorder_unit_result(r);
}
internal engine_command_result recorder_command_summon(void *o, const engine_move_order *order)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_SUMMON, order->city_id);
    r->last_move = *order;
    return recorder_unit_result(r);
}
internal u32 recorder_travel_days(void *o, u8 from, u8 to)
{
    recorder_record((engine_recorder *)o, RECORDER_TRAVEL_DAYS, (u32)from | ((u32)to << 8));
    return from == to ? 0 : 10;
}
internal engine_command_result recorder_command_court(void *o, engine_command_kind kind, const engine_court_order *order)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_COURT, kind);
    r->last_court = *order;
    return recorder_unit_result(r);
}
internal engine_command_result recorder_command_captive(void *o, u16 officer_id, u8 choice)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_CAPTIVE, officer_id);
    r->last_captive_choice = choice;
    if (r->captive_officer == officer_id && choice != ENGINE_CAPTIVE_KEEP) r->captive_officer = 0;   /* the officer is a captive no more */
    return recorder_unit_result(r);
}
internal engine_command_result recorder_command_duel(void *o, u32 unit_id, s32 q, s32 r)
{ return recorder_unit_target((engine_recorder *)o, RECORDER_COMMAND_DUEL, ENGINE_COMMAND_DUEL, unit_id, 0, q, r); }
internal engine_command_result recorder_command_attack_facility(void *o, u32 unit_id, s32 q, s32 r)
{ return recorder_unit_target((engine_recorder *)o, RECORDER_COMMAND_ATTACK_FACILITY, ENGINE_COMMAND_ATTACK_FACILITY, unit_id, 0, q, r); }
internal engine_command_result recorder_command_duel_officer(void *o, u32 unit_id, s32 q, s32 r, u8 slot)
{ return recorder_unit_target((engine_recorder *)o, RECORDER_COMMAND_DUEL, ENGINE_COMMAND_DUEL, unit_id, slot, q, r); }
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

/* ---- M8: two bases, corps 1, an adviser, two council proposals, an emperor,
   and a treason event when pending_active ---------------------------------- */
internal u32 recorder_base_count(void *o) { recorder_record((engine_recorder *)o, RECORDER_BASE_COUNT, 0); return 2; }
internal engine_base recorder_base_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_BASE_AT, index);
    engine_base base = {};
    base.id = ENGINE_NO_ROW;
    base.force = ENGINE_NO_FACTION;
    if (index >= 2) return base;
    base.id = (u8)(42 + index * 10);
    if (((engine_recorder *)o)->pack_base_keys) snprintf(base.key, sizeof(base.key), "%s", index ? "pack_port" : "pack_pass");
    else                                        snprintf(base.key, sizeof(base.key), "%s", index ? "anping" : "hu_guan");
    base.type = (u8)index;
    base.owner_city = 0;
    base.force = 1;
    base.durability = base.durability_max = 1000;
    base.troops = 3000;
    base.troop_cap = 30000;
    base.q = 4 + (s32)index;
    base.r = 2;
    return base;
}
internal u32 recorder_base_at_hex(void *o, s32 q, s32 r)
{
    engine_recorder *rec = (engine_recorder *)o;
    recorder_record(rec, RECORDER_BASE_AT_HEX, 0);
    return rec->own_base_hex && q == 3 && r == 2 ? 0 : ENGINE_NO_BASE;
}
/* The two bases of recorder_base_at as bases of the spec: a garrison of
   3000, 800 gold, 6000 grain and 2000 pikes. */
internal engine_depot recorder_depot_at(void *o, u8 location)
{
    recorder_record((engine_recorder *)o, RECORDER_DEPOT_AT, location);
    engine_depot depot = {};
    depot.location = location;
    depot.force    = ENGINE_NO_FACTION;
    if (location != 42 && location != 52) return depot;
    depot.exists      = true;
    depot.is_base     = true;
    depot.force       = 1;
    depot.troops      = 3000;
    depot.soldier_cap = 30000;
    depot.gold        = 800;
    depot.grain       = 6000;
    depot.morale      = 70;
    depot.stocks[0]   = 2000;
    depot.q           = location == 42 ? 4 : 5;
    depot.r           = 2;
    return depot;
}
internal engine_corps recorder_corps_at(void *o, u8 force, u8 corps)
{
    recorder_record((engine_recorder *)o, RECORDER_CORPS_AT, (u32)force | ((u32)corps << 8));
    engine_corps view = {};
    view.viceroy = ENGINE_NO_OFFICER;
    view.transport_city = ENGINE_NO_CITY;
    if (corps != 1) return view;
    view.alive = true;
    view.viceroy = 101;
    view.flags = CORPS_FLAG_ALL;
    view.city_count = 1;
    view.action_points = 20;
    return view;
}
internal u8  recorder_city_corps(void *o, u8 city) { recorder_record((engine_recorder *)o, RECORDER_CITY_CORPS, city); return 0; }
internal u16 recorder_adviser(void *o, u8 force) { recorder_record((engine_recorder *)o, RECORDER_ADVISER, force); return 100; }
internal engine_plan_view recorder_plan(u32 count)
{
    engine_plan_view view = {};
    view.count = count;
    for (u32 index = 0; index < count; ++index)
    {
        view.items[index].kind = (u8)(index ? SIM_COMMAND_TRAIN : SIM_COMMAND_PATROL);
        view.items[index].city_id = 0;
        view.items[index].officer = 100;
        view.items[index].score = 80 - index * 20;
        snprintf(view.items[index].key, sizeof(view.items[index].key), "%s", index ? "train" : "patrol");
    }
    return view;
}
internal engine_plan_view recorder_advice(void *o, u8 force) { recorder_record((engine_recorder *)o, RECORDER_ADVICE, force); return recorder_plan(1); }
internal engine_plan_view recorder_council(void *o, u8 force, u8 city) { recorder_record((engine_recorder *)o, RECORDER_COUNCIL, (u32)force | ((u32)city << 8)); return recorder_plan(2); }
internal engine_plan_view recorder_ai_plan(void *o, u8 force) { recorder_record((engine_recorder *)o, RECORDER_AI_PLAN, force); return recorder_plan(2); }
/* G3-009: the rival unit 1 marches on the city unit1_goal_city when unit1_goal is set. */
internal engine_unit_goal recorder_unit_goal(void *o, u32 unit_id)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_UNIT_GOAL, unit_id);
    engine_unit_goal goal = {};
    goal.city_id = ENGINE_NO_CITY;
    if (unit_id == 1 && r->unit1_rival && r->unit1_goal) { goal.known = true; goal.city_id = r->unit1_goal_city; goal.q = 1; goal.r = 3; }
    return goal;
}
internal engine_emperor recorder_emperor(void *o)
{
    recorder_record((engine_recorder *)o, RECORDER_EMPEROR, 0);
    engine_emperor view = {};
    view.who = EMPEROR_XIAN;
    view.city = 0;
    view.protector = 1;
    return view;
}
internal u32 recorder_relic_count(void *o) { recorder_record((engine_recorder *)o, RECORDER_RELIC_COUNT, 0); return 0; }
internal engine_relic recorder_relic_at(void *o, u32 index) { recorder_record((engine_recorder *)o, RECORDER_RELIC_AT, index); engine_relic relic = {}; return relic; }
internal u32 recorder_event_kind_count(void *o) { recorder_record((engine_recorder *)o, RECORDER_EVENT_KIND_COUNT, 0); return 1; }
internal engine_event_kind recorder_event_kind_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_EVENT_KIND_AT, index);
    engine_event_kind kind = {};
    kind.id = (u8)index;
    snprintf(kind.key, sizeof(kind.key), "treason");
    kind.check = EVENT_CHECK_QUARTER;
    kind.chance = 30;
    return kind;
}
internal engine_pending_event recorder_pending_event(void *o)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_PENDING_EVENT, 0);
    engine_pending_event view = {};
    view.officer_a = view.officer_b = view.officer_c = ENGINE_NO_OFFICER;
    view.city = view.city_2 = ENGINE_NO_CITY;
    view.force = ENGINE_NO_FACTION;
    if (!r->pending_active) return view;
    view.active = true;
    view.force = 1;
    view.officer_a = 100;
    view.officer_b = 101;
    view.choices = 2;
    return view;
}
internal engine_command_result recorder_command_corps(void *o, const sim_corps_order *order)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_CORPS, order->action);
    r->last_corps = *order;
    return recorder_unit_result(r);
}
internal engine_command_result recorder_command_adviser(void *o, u8 force, u16 officer)
{
    (void)force;
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_ADVISER, officer);
    return recorder_unit_result(r);
}
internal engine_command_result recorder_command_council(void *o, u8 force, u8 city, u8 proposal)
{
    (void)force;
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_COUNCIL, (u32)proposal | ((u32)city << 8));
    return recorder_unit_result(r);
}
internal engine_command_result recorder_command_event_choice(void *o, u8 choice)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_EVENT_CHOICE, choice);
    r->pending_active = false;
    return recorder_unit_result(r);
}
internal engine_command_result recorder_trigger_event(void *o, u8 event, u8 force, u8 city, u16 officer)
{
    (void)force; (void)city; (void)officer;
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_TRIGGER_EVENT, event);
    return recorder_unit_result(r);
}
internal void recorder_set_emperor(void *o, u8 who, u8 city) { (void)city; recorder_record((engine_recorder *)o, RECORDER_SET_EMPEROR, who); }
internal void recorder_set_base(void *o, u32 index, u8 force, u32 troops, u32 durability) { (void)force; (void)troops; (void)durability; recorder_record((engine_recorder *)o, RECORDER_SET_BASE, index); }

/* ---- M9: slot 1 holds a save; the others are empty ---------------------- */
internal engine_save_status recorder_save_game(void *o, u32 slot, const char *comment)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_SAVE_GAME, slot);
    snprintf(r->last_save_comment, sizeof(r->last_save_comment), "%s", comment ? comment : "");
    return ENGINE_SAVE_OK;
}
internal engine_save_status recorder_load_game(void *o, u32 slot) { recorder_record((engine_recorder *)o, RECORDER_LOAD_GAME, slot); return ENGINE_SAVE_OK; }
internal engine_save_slot recorder_save_slot(void *o, u32 slot)
{
    recorder_record((engine_recorder *)o, RECORDER_SAVE_SLOT, slot);
    engine_save_slot info = {};
    info.player = ENGINE_NO_FACTION;
    if (slot != 1) return info;
    info.used = true;
    snprintf(info.comment, sizeof(info.comment), "autumn");
    info.year = 194; info.month = 9; info.day = 1; info.player = 1; info.turn = 25;
    return info;
}
internal void recorder_refresh_save_slots(void *o) { recorder_record((engine_recorder *)o, RECORDER_REFRESH_SAVE_SLOTS, 0); }

/* ---- M9: two chart rows (row 0 with 2 uses), one ancient officer, one
   history row, two scenarios (the save and an unavailable PK one), two
   battles ------------------------------------------------------------------ */
internal u32 recorder_chart_count(void *o) { recorder_record((engine_recorder *)o, RECORDER_CHART_COUNT, 0); return 2; }
internal engine_chart_row recorder_chart_row_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_CHART_ROW_AT, index);
    engine_chart_row row = {};
    row.id = index < 2 ? (u8)index : (u8)ENGINE_NO_ROW;
    snprintf(row.key, sizeof(row.key), "%s", index ? "force_5" : "leadership_5");
    row.kind = CHART_PLUS; row.target = (u8)index; row.amount = 5; row.count = 3; row.months = 3;
    return row;
}
internal engine_faction_chart recorder_faction_chart(void *o, u8 force)
{
    recorder_record((engine_recorder *)o, RECORDER_FACTION_CHART, force);
    engine_faction_chart chart = {};
    chart.uses[0] = 2;
    chart.known   = 1;
    chart.job_row = ENGINE_NO_ROW;
    chart.job_city = ENGINE_NO_CITY;
    return chart;
}
internal u32 recorder_ancient_count(void *o) { recorder_record((engine_recorder *)o, RECORDER_ANCIENT_COUNT, 0); return 1; }
internal engine_ancient recorder_ancient_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_ANCIENT_AT, index);
    engine_ancient row = {};
    row.id = index == 0 ? 0 : (u8)ENGINE_NO_ROW;
    snprintf(row.key, sizeof(row.key), "xiang_yu");
    row.family_name[0] = 0xB6; row.family_name[1] = 0xB5; row.given_name[0] = 0xA6; row.given_name[1] = 0xD0;
    for (u32 ability = 0; ability < CONTENT_ABILITY_COUNT; ++ability) row.abilities[ability] = 90;
    return row;
}
internal u32 recorder_new_officer_count(void *o) { recorder_record((engine_recorder *)o, RECORDER_NEW_OFFICER_COUNT, 0); return 0; }
internal u32 recorder_history_count(void *o) { recorder_record((engine_recorder *)o, RECORDER_HISTORY_COUNT, 0); return 1; }
internal engine_history_kind recorder_history_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_HISTORY_AT, index);
    engine_history_kind kind = {};
    kind.id = index == 0 ? 0 : (u8)ENGINE_NO_ROW;
    snprintf(kind.key, sizeof(kind.key), "zhao_yun");
    kind.chance = 100;
    return kind;
}
internal b32 recorder_history_ready(void *o, u32 row, u8 force) { (void)force; recorder_record((engine_recorder *)o, RECORDER_HISTORY_READY, row); return false; }
internal u32 recorder_scenario_count(void *o) { recorder_record((engine_recorder *)o, RECORDER_SCENARIO_COUNT, 0); return 2; }
internal engine_scenario recorder_scenario_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_SCENARIO_AT, index);
    engine_scenario scenario = {};
    scenario.id = index < 2 ? (u8)index : (u8)ENGINE_NO_ROW;
    snprintf(scenario.key, sizeof(scenario.key), "%s", index ? "lu_bu_campaign" : "sample_save");
    scenario.kind      = (u8)(index ? ENGINE_SCENARIO_PK : ENGINE_SCENARIO_SAVE);
    scenario.year      = (u16)(index ? 0 : 194);
    scenario.available = index == 0;
    if (index == 0) snprintf(scenario.file, sizeof(scenario.file), "sample.s11");
    u8 pack_rows = ((engine_recorder *)o)->pack_rows;
    if (index == 1 && pack_rows)
    {
        snprintf(scenario.key, sizeof(scenario.key), "y1200");
        snprintf(scenario.file, sizeof(scenario.file), "y1200/scenario.tsv");
        scenario.kind      = ENGINE_SCENARIO_PACK;
        scenario.year      = 1200;
        scenario.available = true;
        if (pack_rows == 1) snprintf(scenario.pack, sizeof(scenario.pack), "medieval_europe");
    }
    return scenario;
}
internal u32 recorder_battle_count(void *o) { recorder_record((engine_recorder *)o, RECORDER_BATTLE_COUNT, 0); return 2; }
internal engine_battle recorder_battle_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_BATTLE_AT, index);
    engine_battle battle = {};
    battle.id = index < 2 ? (u8)index : (u8)ENGINE_NO_ROW;
    snprintf(battle.key, sizeof(battle.key), "%s", index ? "rebels" : "hefei");
    battle.open_at_start = index == 0;
    return battle;
}
/* ---- M10: the real map. The scripted map has one city cell: offset (2, 2)
   is city 0. */

internal engine_map_info recorder_map_info(void *o)
{
    recorder_record((engine_recorder *)o, RECORDER_MAP_INFO, 0);
    engine_map_info info = {};
    info.real_map = true; info.detail = "ok"; info.width = 8; info.height = 6;
    return info;
}
internal engine_hex recorder_hex_at(void *o, s32 q, s32 r)
{
    hex_offset offset = hex_to_offset(hex_make(q, r));
    recorder_record((engine_recorder *)o, RECORDER_HEX_AT, (u32)(offset.col & 0xFFFF) | ((u32)(offset.row & 0xFFFF) << 16));
    engine_hex hex = {};
    hex.on_map      = offset.col >= 0 && offset.row >= 0 && offset.col < 8 && offset.row < 6;
    engine_recorder *rec = (engine_recorder *)o;
    hex.terrain     = (u8)(rec->has_water && q == rec->water_q && r == rec->water_r ? 2 : 1);
    hex.region      = ENGINE_NO_REGION;
    hex.region_city = ENGINE_NO_CITY;
    hex.city_id     = offset.col == RECORDER_CITY_COL && offset.row == RECORDER_CITY_ROW ? 0 : ENGINE_NO_CITY;
    hex.base_id     = rec->own_base_hex && q == 3 && r == 2 ? 42 : ENGINE_NO_BASE;   /* the location id, as the real engine: base_at_hex gives the index */
    hex.slot_city   = ENGINE_NO_CITY;    /* M11: nothing stands on the scripted hexes */
    if (offset.col == RECORDER_SLOT_COL && offset.row == RECORDER_SLOT_ROW)
    {
        hex.slot_city  = 0;              /* M19: one empty slot of city 0, for the placement mode */
        hex.slot_index = RECORDER_SLOT_INDEX;
    }
    hex.facility    = ENGINE_NO_FACILITY;
    if (rec->foreign_city && offset.col == 6 && offset.row == 4)
    {
        hex.slot_city  = (u8)rec->foreign_city;   /* G3-020: a built market (slot 2) of the foreign city, out of the range of unit 0 */
        hex.slot_index = 2;
        hex.facility   = 0;
    }
    hex.building_id = q == 2 && r == 3 ? 0 : ENGINE_NO_BUILDING;   /* U-001: the camp of recorder_building_at */
    hex.building_kind = q == 2 && r == 3 ? 0 : ENGINE_NO_ROW;
    hex.unit_id     = ENGINE_NO_UNIT;
    return hex;
}
internal u32 recorder_armament_count(void *o) { recorder_record((engine_recorder *)o, RECORDER_ARMAMENT_COUNT, 0); return 0; }
internal engine_armament recorder_armament_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_ARMAMENT_AT, index);
    engine_armament armament = {};
    armament.id = ENGINE_NO_ARMAMENT;
    return armament;
}
internal u32 recorder_move_cost(void *o, u8 unit_type, u8 force_id, s32 q, s32 r)
{
    (void)force_id; (void)q; (void)r;
    recorder_record((engine_recorder *)o, RECORDER_MOVE_COST, unit_type);
    return 4;
}
internal engine_bandit_sites recorder_bandit_sites(void *o, u8 city_id)
{
    recorder_record((engine_recorder *)o, RECORDER_BANDIT_SITES, city_id);
    engine_bandit_sites sites = {};
    return sites;
}
internal void recorder_start_fire(void *o, s32 q, s32 r, u32 turns)
{
    (void)q; (void)r;
    recorder_record((engine_recorder *)o, RECORDER_START_FIRE, turns);
}

/* M13: two scripted pairs; a replay answers one difference. */
internal u32 recorder_replay_pair_count(void *o)
{
    recorder_record((engine_recorder *)o, RECORDER_REPLAY_PAIR_COUNT, 0);
    return 2;
}
internal engine_replay_pair recorder_replay_pair_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_REPLAY_PAIR_AT, index);
    engine_replay_pair pair = {};
    snprintf(pair.rule, sizeof(pair.rule), "%s", index == 0 ? "develop" : "conscript");
    pair.number = 1;
    snprintf(pair.folder, sizeof(pair.folder), "pairs/%s/001", pair.rule);
    snprintf(pair.q_ids, sizeof(pair.q_ids), "Q-153");
    pair.command.kind    = (u8)(index == 0 ? ENGINE_REPLAY_DEVELOP : ENGINE_REPLAY_CONSCRIPT);
    pair.command.city_id = 27;
    for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place) pair.command.officers[place] = ENGINE_NO_OFFICER;
    return pair;
}
internal engine_replay_report recorder_replay_pair(void *o, const char *before, const engine_replay_command *command, const char *after)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_REPLAY_PAIR, command ? command->kind : 0);
    snprintf(r->last_replay_before, sizeof(r->last_replay_before), "%s", before ? before : "");
    snprintf(r->last_replay_after, sizeof(r->last_replay_after), "%s", after ? after : "");
    engine_replay_report report = {};
    report.fields = 10;
    report.differ = report.differ_verified = report.diff_count = 1;
    return report;
}
/* M15: a scripted record of an AI turn of faction 2. The first option is
   the chosen patrol of city 27; the second is the chosen march of unit 3
   to the free city 5 at hex (10, 12); the third is a goal that the unit
   did not choose. */
internal engine_ai_explain recorder_ai_explain(void *o, u8 force)
{
    recorder_record((engine_recorder *)o, RECORDER_AI_EXPLAIN, force);
    engine_ai_explain explain = {};
    explain.faction = force;
    if (force != 2) return explain;
    explain.turn  = 4;
    explain.count = 3;
    engine_ai_option *patrol = explain.options;
    patrol->goal = AI_GOAL_CITY_COMMAND; patrol->kind = SIM_COMMAND_PATROL; patrol->city_id = 27; patrol->chosen = true;
    patrol->unit = ENGINE_NO_UNIT; patrol->officer = 101; patrol->score = 80;
    snprintf(patrol->key, sizeof(patrol->key), "patrol");
    engine_ai_option *take = explain.options + 1;
    take->goal = AI_GOAL_TAKE_FREE_CITY; take->kind = SIM_COMMAND_MOVE; take->city_id = 5; take->chosen = true;
    take->unit = 3; take->officer = 102; take->score = 240; take->q = 10; take->r = 12;
    engine_ai_option *attack = explain.options + 2;
    *attack = *take;
    attack->goal = AI_GOAL_ATTACK_CITY; attack->city_id = 6; attack->chosen = false; attack->score = 180; attack->q = 14; attack->r = 9;
    return explain;
}
/* M16: one scripted scenario of the install, the SCEN002 of the spec. The
   portrait source is scripted; each other asset comes from the pack. */
#define RECORDER_INSTALL_SCENARIO_FILE "install\\media\\scenario\\SCEN002.s11"
internal engine_install_result recorder_set_install_folder(void *o, const char *folder)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_SET_INSTALL_FOLDER, 0);
    snprintf(r->last_install_folder, RECORDER_MAX_PATH, "%s", folder ? folder : "");
    engine_install_result result = {};
    result.found          = r->last_install_folder[0] != 0;
    result.names_loaded   = result.found;
    result.scenario_count = result.found ? 1 : 0;
    result.detail         = "ok";
    return result;
}
internal const char *recorder_install_folder(void *o)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_INSTALL_FOLDER, 0);
    return r->last_install_folder;
}
internal engine_content_source recorder_content_source(void *o, u8 asset)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_CONTENT_SOURCE, asset);
    engine_content_source source = {};
    source.asset  = asset;
    source.source = asset == ENGINE_ASSET_PORTRAITS ? r->portrait_source : (u8)ENGINE_SOURCE_PACK;
    source.count  = 1;
    source.detail = "scripted";
    return source;
}
internal u32 recorder_install_scenario_count(void *o)
{
    recorder_record((engine_recorder *)o, RECORDER_INSTALL_SCENARIO_COUNT, 0);
    return 1;
}
internal engine_install_scenario recorder_install_scenario_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_INSTALL_SCENARIO_AT, index);
    engine_install_scenario scenario = {};
    scenario.number = ENGINE_NO_ROW;
    if (index != 0) return scenario;
    static const u8 title[] = { 0xB8, 0x73, 0xB6, 0xAF, 0xB3, 0xCE, 0xBE, 0xDA };   /* the Big5 bytes of the SCEN002 title */
    scenario.number = 2;
    scenario.year   = 194;
    scenario.month  = 6;
    memcpy(scenario.title, title, sizeof(title));
    snprintf(scenario.file, sizeof(scenario.file), "%s", RECORDER_INSTALL_SCENARIO_FILE);
    return scenario;
}
/* The replay of a game: a scripted record of six steps in three turns.
   The steps of force 1 patrol city 0; the steps of force 2 end the turn.
   Step 1 has one event at ledger index 0, on the map at hex (2, 3). */
#define RECORDER_REPLAY_STEPS 6
internal engine_game_replay_info recorder_game_replay_info(void *o)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_GAME_REPLAY_INFO, 0);
    engine_game_replay_info info = {};
    info.status     = ENGINE_GAME_REPLAY_RECORDING;
    info.viewing    = r->replay_viewing;
    info.step_count = RECORDER_REPLAY_STEPS;
    info.view_step  = r->replay_viewing ? r->replay_view_step : RECORDER_REPLAY_STEPS;
    info.version    = 1;
    info.start_turn = 1;
    info.first_edit = ENGINE_NO_STEP;
    return info;
}
internal engine_game_replay_step recorder_game_replay_step_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_GAME_REPLAY_STEP_AT, index);
    engine_game_replay_step step = {};
    step.index = ENGINE_NO_STEP;
    if (index >= RECORDER_REPLAY_STEPS) return step;
    b32 patrol = index % 2 == 0;
    step.index       = index;
    step.kind        = ENGINE_STEP_COMMAND;
    step.command     = (u8)(patrol ? SIM_COMMAND_PATROL : SIM_COMMAND_END_TURN);
    step.key         = patrol ? "patrol" : "end_turn";
    step.force       = (u8)(patrol ? 1 : 2);
    step.turn        = 1 + index / 2;
    step.command_id  = 10 + index;
    step.officer     = ENGINE_NO_OFFICER;
    step.first_event = index == 1 ? 0 : ENGINE_NO_EVENT;
    step.event_count = index == 1 ? 1 : 0;
    step.place.kind  = ENGINE_PLACE_NONE;
    step.place.force = ENGINE_NO_FACTION;
    step.place.officer = ENGINE_NO_OFFICER;
    if (index == 1)
    {
        step.place.kind   = ENGINE_PLACE_HEX;
        step.place.on_map = true;
        step.place.q      = 2;
        step.place.r      = 3;
    }
    return step;
}
internal engine_game_replay_view recorder_game_replay_view(void *o, u32 step)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_GAME_REPLAY_VIEW, step);
    engine_game_replay_view view = {};
    view.step = step;
    if (step > RECORDER_REPLAY_STEPS) return view;
    r->replay_viewing   = true;
    r->replay_view_step = step;
    view.ok      = true;
    view.matches = true;
    return view;
}
internal void recorder_game_replay_close(void *o)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_GAME_REPLAY_CLOSE, 0);
    r->replay_viewing = false;
}
internal u32 recorder_game_replay_turn_step(void *o, u32 turn)
{
    recorder_record((engine_recorder *)o, RECORDER_GAME_REPLAY_TURN_STEP, turn);
    for (u32 index = 0; index < RECORDER_REPLAY_STEPS; ++index)
    {
        if (1 + index / 2 >= turn) return index;
    }
    return RECORDER_REPLAY_STEPS;
}
internal void recorder_set_unit_grain(void *o, u32 unit_id, u32 grain)
{
    (void)grain;
    recorder_record((engine_recorder *)o, RECORDER_SET_UNIT_GRAIN, unit_id);
}

internal engine_replay_diff recorder_replay_diff_at(void *o, u32 index)
{
    recorder_record((engine_recorder *)o, RECORDER_REPLAY_DIFF_AT, index);
    engine_replay_diff diff = {};
    snprintf(diff.path, sizeof(diff.path), "cities[30].troops");
    diff.field = ENGINE_REPLAY_CITY_TROOPS;
    diff.verified = true;
    diff.before = 12365; diff.replayed = 15935; diff.original = 15804;
    diff.rule = "conscription";
    diff.q_id = "Q-153";
    return diff;
}

internal engine_command_result recorder_create_officer(void *o, const sim_new_officer_order *order)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_CREATE_OFFICER, (u32)order->type | ((u32)order->city_id << 8));
    r->last_newcomer = *order;
    return recorder_unit_result(r);
}
internal engine_command_result recorder_command_research_ability(void *o, u8 city, u8 row)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_RESEARCH_ABILITY, (u32)row | ((u32)city << 8));
    return recorder_unit_result(r);
}
internal engine_command_result recorder_command_train_officer(void *o, u16 officer, u8 row)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_COMMAND_TRAIN_OFFICER, (u32)row | ((u32)officer << 8));
    return recorder_unit_result(r);
}
internal engine_command_result recorder_trigger_history(void *o, u32 row, u8 force)
{
    (void)force;
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_TRIGGER_HISTORY, row);
    return recorder_unit_result(r);
}
internal engine_load_result recorder_load_scenario(void *o, const char *path)
{
    engine_recorder *r = (engine_recorder *)o;
    recorder_record(r, RECORDER_LOAD_SCENARIO, 0);
    snprintf(r->last_scenario_path, RECORDER_MAX_PATH, "%s", path ? path : "");
    engine_load_result result = {};
    result.status = ENGINE_LOAD_OK;
    result.detail = "ok";
    return result;
}

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

/* G2-045: the check from a destination; the recorder has no map, so the
   check is the one of can_issue. */
internal engine_check recorder_can_issue_after_move(void *o, const engine_command *command, s32 q, s32 rr)
{
    (void)q; (void)rr;
    return recorder_can_issue(o, command);
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
    api.region_view     = recorder_region_view;   /* M21 */
    api.set_cell_terrain = recorder_set_cell_terrain;
    api.load_save       = recorder_load_save;
    api.load_name_index = recorder_load_name_index;
    api.date            = recorder_date;
    api.whose_turn      = recorder_whose_turn;
    api.actions_left    = recorder_actions_left;
    api.faction_count   = recorder_faction_count;
    api.faction_at      = recorder_faction_at;
    api.can_issue       = recorder_can_issue;
    api.can_issue_after_move = recorder_can_issue_after_move;
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
    api.faction_colour  = recorder_faction_colour;
    api.location_name_at = recorder_location_name_at;
    api.city_economy         = recorder_city_economy;
    api.city_income_forecast = recorder_city_income_forecast;
    api.technique_points     = recorder_technique_points;
    api.weapon_kind_count    = recorder_weapon_kind_count;
    api.weapon_kind_at       = recorder_weapon_kind_at;
    api.ledger_filter        = recorder_ledger_filter;
    api.subject_of           = recorder_subject_of;
    api.event_report         = recorder_event_report;
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
    api.path_to              = recorder_path_to;
    api.command_destroy      = recorder_command_destroy;
    api.command_attack_facility = recorder_command_attack_facility;
    api.command_drive        = recorder_command_drive;
    api.command_follow       = recorder_command_follow;
    api.command_delegate     = recorder_command_delegate;
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
    api.command_captive       = recorder_command_captive;
    api.command_move_officers = recorder_command_move_officers;
    api.command_summon        = recorder_command_summon;
    api.travel_days           = recorder_travel_days;
    api.court_advice          = recorder_court_advice;
    api.command_duel          = recorder_command_duel;
    api.command_duel_officer  = recorder_command_duel_officer;
    api.command_duel_action   = recorder_command_duel_action;
    api.command_debate_action = recorder_command_debate_action;
    api.start_debate          = recorder_start_debate;
    api.set_officer           = recorder_set_officer;
    api.set_item_owner        = recorder_set_item_owner;
    api.set_diplomacy         = recorder_set_diplomacy;
    api.set_stamina           = recorder_set_stamina;
    api.base_count            = recorder_base_count;
    api.base_at               = recorder_base_at;
    api.base_at_hex           = recorder_base_at_hex;
    api.depot_at              = recorder_depot_at;
    api.corps_at              = recorder_corps_at;
    api.city_corps            = recorder_city_corps;
    api.adviser               = recorder_adviser;
    api.advice                = recorder_advice;
    api.council               = recorder_council;
    api.ai_plan               = recorder_ai_plan;
    api.unit_goal             = recorder_unit_goal;
    api.emperor               = recorder_emperor;
    api.relic_count           = recorder_relic_count;
    api.relic_at              = recorder_relic_at;
    api.event_kind_count      = recorder_event_kind_count;
    api.event_kind_at         = recorder_event_kind_at;
    api.pending_event         = recorder_pending_event;
    api.command_corps         = recorder_command_corps;
    api.command_adviser       = recorder_command_adviser;
    api.command_council       = recorder_command_council;
    api.command_event_choice  = recorder_command_event_choice;
    api.trigger_event         = recorder_trigger_event;
    api.set_emperor           = recorder_set_emperor;
    api.set_base              = recorder_set_base;
    api.save_game             = recorder_save_game;
    api.load_game             = recorder_load_game;
    api.save_slot             = recorder_save_slot;
    api.refresh_save_slots    = recorder_refresh_save_slots;
    api.chart_count           = recorder_chart_count;
    api.chart_row_at          = recorder_chart_row_at;
    api.faction_chart         = recorder_faction_chart;
    api.ancient_count         = recorder_ancient_count;
    api.ancient_at            = recorder_ancient_at;
    api.new_officer_count     = recorder_new_officer_count;
    api.create_officer        = recorder_create_officer;
    api.command_research_ability = recorder_command_research_ability;
    api.command_train_officer = recorder_command_train_officer;
    api.history_count         = recorder_history_count;
    api.history_at            = recorder_history_at;
    api.history_ready         = recorder_history_ready;
    api.trigger_history       = recorder_trigger_history;
    api.scenario_count        = recorder_scenario_count;
    api.scenario_at           = recorder_scenario_at;
    api.load_scenario         = recorder_load_scenario;
    api.battle_count          = recorder_battle_count;
    api.battle_at             = recorder_battle_at;
    api.map_info              = recorder_map_info;   /* M10 */
    api.hex_at                = recorder_hex_at;
    api.armament_count        = recorder_armament_count;
    api.armament_at           = recorder_armament_at;
    api.move_cost             = recorder_move_cost;
    api.bandit_sites          = recorder_bandit_sites;
    api.start_fire            = recorder_start_fire;
    api.replay_pair_count     = recorder_replay_pair_count;   /* M13 */
    api.replay_pair_at        = recorder_replay_pair_at;
    api.replay_pair           = recorder_replay_pair;
    api.replay_diff_at        = recorder_replay_diff_at;
    api.ai_explain            = recorder_ai_explain;      /* M15 */
    api.set_unit_grain        = recorder_set_unit_grain;
    api.set_install_folder     = recorder_set_install_folder;   /* M16 */
    api.install_folder         = recorder_install_folder;
    api.content_source         = recorder_content_source;
    api.install_scenario_count = recorder_install_scenario_count;
    api.install_scenario_at    = recorder_install_scenario_at;
    api.game_replay_info       = recorder_game_replay_info;   /* the replay of a game */
    api.game_replay_step_at    = recorder_game_replay_step_at;
    api.game_replay_view       = recorder_game_replay_view;
    api.game_replay_close      = recorder_game_replay_close;
    api.game_replay_turn_step  = recorder_game_replay_turn_step;
    return api;
}

#endif /* ENGINE_RECORDER_H */
