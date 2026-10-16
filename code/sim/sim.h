#ifndef SIM_H
#define SIM_H
/* Headless, deterministic simulation core. No Windows headers, no I/O, no
   ImGui. Everything lives in the permanent arena. Commands in, events out.

   Mechanics are implemented one at a time from spec-src/spec/mechanics/.
   The numbers come from the tables of the content pack (sim/content.h).
   Until a mechanic has a spec entry, it does not exist here. */

#include "base.h"
#include "ecs/ecs.h"
#include "sim/hex.h"
#include "sim/inflate.h"
#include "sim/save_format.h"
#include "sim/scenario_format.h"
#include "sim/calendar.h"
#include "sim/tables.h"
#include "sim/content.h"

enum sim_event_kind
{
    SIM_EVENT_NONE,
    SIM_EVENT_TURN_ADVANCED,     /* the date moved one turn; delta = 1 */
    SIM_EVENT_SAVE_LOADED,       /* delta = officers loaded; the old officers and cities are gone */
    SIM_EVENT_NAME_INDEX_LOADED, /* delta = city names loaded; the old names are gone */
    /* calendar-and-turn.md, "Calendar events" */
    SIM_EVENT_YEAR_STARTED,      /* delta = the year */
    SIM_EVENT_MONTH_STARTED,     /* delta = the month */
    SIM_EVENT_SEASON_STARTED,    /* delta = the season, 0 spring .. 3 winter */
    SIM_EVENT_GOLD_INCOME,       /* subject = city; delta = gold in; assumed amount */
    SIM_EVENT_SALARIES_PAID,     /* subject = city; delta = gold out (negative); assumed amount */
    SIM_EVENT_GRAIN_INCOME,      /* subject = city; delta = grain in; assumed amount */
    SIM_EVENT_GRAIN_CONSUMED,    /* subject = city; delta = grain out (negative); assumed amount */
    SIM_EVENT_HOOK_BANDITS,      /* the hooks of the disasters and the yearly changes: no logic yet */
    SIM_EVENT_HOOK_LOCUSTS,
    SIM_EVENT_HOOK_PLAGUE,
    SIM_EVENT_HOOK_GOOD_HARVEST,
    SIM_EVENT_HOOK_OFFICERS_AGE,
    SIM_EVENT_HOOK_NEW_OFFICERS,
    SIM_EVENT_BLACK_MARKET_REMOVED, /* subject = city; delta = the slot */
    /* the turn order and the action points */
    SIM_EVENT_FACTION_TURN_STARTED, /* faction; delta = its place in the order, 0 first */
    SIM_EVENT_FACTION_TURN_ENDED,   /* faction pressed "proceed" */
    SIM_EVENT_AI_PASSED,            /* faction; the AI placeholder gave no command */
    SIM_EVENT_ACTION_POINTS_RECOVERED, /* faction; delta = points in; assumed rule (Q-019, Q-141) */
    SIM_EVENT_ACTION_POINTS_SPENT,  /* faction; delta = points out (negative) */
    /* the develop command */
    SIM_EVENT_DEVELOP_STARTED,      /* subject = city; delta = the facility id */
    SIM_EVENT_GOLD_SPENT,           /* subject = city; delta = gold out (negative) */
    SIM_EVENT_FACILITY_COMPLETED,   /* subject = city; delta = the facility id */
    SIM_EVENT_OFFICER_RELEASED,     /* subject = officer; the job is complete */
    SIM_EVENT_COUNT
};

#define SIM_NO_FORCE   0xFF
#define SIM_NO_OFFICER 0xFFFF
#define SIM_NO_CITY    0xFF
#define SIM_ANY_SLOT   0xFF

/* Every change the sim makes is an event with a cause, so a save-file diff
   from the original maps straight onto expected events. */
struct sim_event
{
    sim_event_kind kind;
    u32            turn;
    entity_id      subject;
    s32            delta;
    u32            cause;     /* id of the command that caused it, 0 for the turn itself */
    u8             faction;   /* the force id, SIM_NO_FORCE if the event has no faction */
    b32            assumed;   /* the amount comes from an assumed number (DECISIONS.md) */
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
    SIM_COMMAND_END_TURN,        /* "proceed": the faction that acts ends its turn */
    SIM_COMMAND_LOAD_SAVE,       /* carries a decoded save: run it with sim_load_save, not sim_apply */
    SIM_COMMAND_LOAD_NAME_INDEX, /* carries a decoded name index: run it with sim_load_name_index */
    SIM_COMMAND_DEVELOP,         /* city-commands.md, 開發 */
    SIM_COMMAND_COUNT
};

#define SIM_DEVELOP_OFFICERS 3

struct sim_develop_order
{
    u8  city_id;
    u8  facility;
    u8  slot;                               /* SIM_ANY_SLOT: the first free slot */
    u16 officers[SIM_DEVELOP_OFFICERS];     /* SIM_NO_OFFICER for an empty place */
};

struct sim_command
{
    sim_command_kind  kind;
    u32               id;        /* assigned by sim_make_command */
    sim_develop_order develop;   /* SIM_COMMAND_DEVELOP */
};

/* Why the sim refuses a command (calendar-and-turn.md, "Reasons that block
   a base command", and city-commands.md, Limits). */
enum sim_refusal
{
    SIM_REFUSAL_NONE,                  /* the command can run */
    SIM_REFUSAL_NO_CONTENT,            /* the tables of the content pack did not load */
    SIM_REFUSAL_NO_CITY,               /* the city id is not a loaded city with an owner */
    SIM_REFUSAL_NOT_YOUR_TURN,         /* the owner of the city does not act now */
    SIM_REFUSAL_NO_FACILITY,           /* no such facility, or the city cannot have it */
    SIM_REFUSAL_ENEMY_NEAR,            /* an enemy unit is within 2 hexes of the city */
    SIM_REFUSAL_VALUE_AT_MAXIMUM,      /* the facility is at its limit (one in each city) */
    SIM_REFUSAL_NO_SLOT,               /* the city has no free slot */
    SIM_REFUSAL_SLOT_TAKEN,            /* the chosen slot is not free or not in the city */
    SIM_REFUSAL_NO_OFFICER,            /* the order names no officer */
    SIM_REFUSAL_OFFICER_NOT_IN_CITY,   /* an officer is not an active officer of the owner in the city */
    SIM_REFUSAL_DUPLICATE_OFFICER,     /* the order names an officer two times */
    SIM_REFUSAL_OFFICER_ALREADY_ACTED, /* one command per officer per turn */
    SIM_REFUSAL_OFFICER_BUSY,          /* the officer works on a job of several turns */
    SIM_REFUSAL_NO_ACTION_POINTS,      /* the corps has fewer points than the command costs */
    SIM_REFUSAL_NO_GOLD,               /* the city has less gold than the command costs */
    SIM_REFUSAL_COUNT
};

/* What a command costs and how long it takes, or why it cannot run. */
struct sim_check
{
    sim_refusal refusal;
    u32         gold;
    u32         action_points;
    u32         turns;       /* the period in turns */
    u32         days;        /* turns * 10 */
    u8          slot;        /* the slot that the command uses */
};

/* An officer of the loaded game: the fields of the officer record that the
   game uses (save-file.md, Officer record). Names are Big5 bytes, verbatim. */
struct officer_component
{
    u16 officer_id;                      /* the slot in the save */
    u8  family_name[OFFICER_FAMILY_NAME_SIZE];
    u8  given_name[OFFICER_GIVEN_NAME_SIZE];
    u8  courtesy_name[OFFICER_COURTESY_NAME_SIZE];
    u16 appearance_year;
    u16 birth_year;
    u16 death_year;
    u8  leadership;
    u8  war;
    u8  intelligence;
    u8  politics;
    u8  charm;
    u8  force_id;
    u16 base_location;
    u16 current_position;
    u8  status;
    u16 merit;
    u8  skill_number;
    u8  compatibility;
    u16 father;
    u16 spouse;
    u16 sworn_brother_group_head;
    u8  unit_aptitudes[OFFICER_UNIT_APTITUDE_COUNT];
    /* the state of the turn rules */
    u32 acted_turn;                      /* sim turn + 1 of the last command; 0 = never */
    u8  job_city;                        /* the city of the develop job, SIM_NO_CITY = free */
    u8  job_slot;
};

/* A city of the loaded game (save-file.md, City record). */
struct city_component
{
    u8  city_id;
    u8  owner_force_id;
    u16 commanding_officer_id;
    u32 capacity_constant;
    u32 gold;
    u32 troops;
    u32 food;
    u32 equipment_stocks[CITY_EQUIPMENT_STOCK_COUNT];
    u16 durability_maximum;              /* hypothesis */
    u8  public_order;                    /* hypothesis */
};

/* ---- facility slots (cities-and-facilities.md) -------------------------- */

#define SIM_SLOT_CAPACITY 24     /* the largest city of the table has 22 slots */

enum sim_slot_state
{
    SIM_SLOT_EMPTY,
    SIM_SLOT_BUILDING,
    SIM_SLOT_BUILT,
    SIM_SLOT_STATE_COUNT
};

struct sim_slot
{
    hex_coord hex;                                /* placeholder layout: see sim_city_slot_hex */
    u8        state;                              /* sim_slot_state */
    u8        facility;                           /* CONTENT_NO_FACILITY when empty */
    u8        level;                              /* 1; the PK merge raises it (M4) */
    u16       turns_left;                         /* while building */
    u16       officers[SIM_DEVELOP_OFFICERS];     /* while building */
};

struct sim_city_site
{
    hex_coord hex;                                /* the city on the placeholder map */
    u8        slot_count;
    sim_slot  slots[SIM_SLOT_CAPACITY];
};

/* A unit on the map. A placeholder until M5: units only block commands of
   enemy cities within 2 hexes. */
#define SIM_UNIT_CAPACITY 64
struct sim_unit
{
    u8        force_id;
    hex_coord hex;
};

/* ---- factions and the turn order (calendar-and-turn.md) ----------------- */

#define SIM_FORCE_CAPACITY 256

/* One faction in the order of a turn: its size when the turn started. */
struct sim_faction
{
    u8  force_id;
    u16 city_count;
    u16 officer_count;
    u32 troops;
    u16 leader_officer_id;    /* the ruler: the leader of corps 1 */
};

struct sim_turn_state
{
    calendar_date start_date;
    calendar_date date;
    sim_faction   order[SIM_FORCE_CAPACITY];
    u32           order_count;
    u32           acting;                        /* index into order; order_count = every faction acted */
    u32           action_points[SIM_FORCE_CAPACITY];   /* one pool for each faction (corps 1) */
    b32           human[SIM_FORCE_CAPACITY];
};

struct sim_state
{
    u64            rng;             /* seeded PRNG state; the only randomness source */
    u32            turn;
    u32            next_command_id;
    entity_manager entities;
    hex_map        map;             /* dimensions and costs come from the scenario; see sim_init_map */
    component_table<officer_component> officers;   /* one entity per officer of the loaded save */
    component_table<city_component>    cities;     /* one entity per city */
    name_index     names;           /* the names of Scenario.s11; the entry number is the city id */
    b32            has_names;       /* false until a name index loads */
    sim_content    content;         /* the tables of the content pack */
    b32            has_content;     /* false until the engine loads all three tables */
    sim_turn_state turns;
    sim_city_site  sites[SAVE_CITY_COUNT];
    sim_unit       units[SIM_UNIT_CAPACITY];
    u32            unit_count;
    u16            officer_index[SAVE_OFFICER_SLOTS];   /* officer id -> dense index, SIM_NO_OFFICER if none */
    u8             city_index[SAVE_CITY_COUNT];         /* city id -> dense index, SIM_NO_CITY if none */
};

#define SIM_ENTITY_CAPACITY  4096
#define SIM_OFFICER_CAPACITY SAVE_OFFICER_SLOTS
#define SIM_CITY_CAPACITY    SAVE_CITY_COUNT

/* The date of a new game until a save sets it from its header. The game
   date in the save is not known (Q-004). */
#define SIM_DEFAULT_START_YEAR  190
#define SIM_DEFAULT_START_MONTH 1

internal void        sim_init(sim_state *sim, memory_arena *arena, u64 seed);
internal void        sim_init_map(sim_state *sim, memory_arena *arena, u32 width, u32 height, u8 default_cost);
internal void        sim_set_content(sim_state *sim, sim_content *content);
internal u32         sim_random_u32(sim_state *sim);
internal u32         sim_random_below(sim_state *sim, u32 bound);
internal sim_command sim_make_command(sim_state *sim, sim_command_kind kind);
internal void        sim_apply(sim_state *sim, sim_command command, sim_event_sink *events);
internal void        sim_advance_turn(sim_state *sim, u32 cause, sim_event_sink *events);
internal b32         sim_load_save(sim_state *sim, save_file *save, sim_command command, sim_event_sink *events);
internal void        sim_unload_world(sim_state *sim);
internal b32         sim_add_officer(sim_state *sim, officer_component officer);   /* false if full or the id is taken */
internal b32         sim_add_city(sim_state *sim, city_component city);
internal void        sim_load_name_index(sim_state *sim, name_index *names, sim_command command, sim_event_sink *events);
internal const char *sim_event_name(sim_event_kind kind);
internal const char *sim_command_name(sim_command_kind kind);
internal const char *sim_refusal_name(sim_refusal refusal);

internal sim_event_sink sim_sink_from_arena(memory_arena *arena, u32 capacity);
internal void           sim_emit(sim_event_sink *sink, sim_event event);
internal sim_event      sim_event_make(sim_state *sim, sim_event_kind kind, u32 cause);

/* turn.cpp: the calendar events, the turn order and the action points */
internal void               sim_end_faction_turn(sim_state *sim, u32 cause, sim_event_sink *events);
internal void               sim_start_turn(sim_state *sim, u32 cause, sim_event_sink *events);
internal void               sim_run_calendar(sim_state *sim, u32 cause, sim_event_sink *events);
internal void               sim_compute_turn_order(sim_state *sim);
internal u32                sim_action_point_recovery(sim_state *sim, sim_faction *faction);
internal u32                sim_recover_action_points(u32 pool, u32 recovery, u32 cap);
internal u8                 sim_whose_turn(sim_state *sim);
internal officer_component *sim_find_officer(sim_state *sim, u16 officer_id);
internal city_component    *sim_find_city(sim_state *sim, u8 city_id);
internal entity_id          sim_city_entity(sim_state *sim, u8 city_id);
internal entity_id          sim_officer_entity(sim_state *sim, u16 officer_id);
internal b32                sim_officer_is_active(officer_component *officer);
internal b32                sim_faction_before(sim_faction *a, sim_faction *b);
internal b32                sim_officer_leads_before(officer_component *a, officer_component *b);
internal u16                sim_select_leader(sim_state *sim, const u16 *officer_ids, u32 count);
internal u16                sim_city_governor(sim_state *sim, u8 city_id);

/* develop.cpp: facility slots and the develop command */
internal hex_coord sim_city_slot_hex(u32 slot);
internal void      sim_init_sites(sim_state *sim);
internal u32       sim_develop_days(u32 politics_sum, u32 min_days, sim_rules *rules);
internal sim_check sim_check_develop(sim_state *sim, sim_develop_order *order);
internal void      sim_develop(sim_state *sim, sim_command command, sim_event_sink *events);
internal void      sim_progress_jobs(sim_state *sim, u32 cause, sim_event_sink *events);
internal u32       sim_slot_yield(sim_state *sim, u8 city_id, u32 slot, facility_effect *effect);
internal u32       sim_city_gold_income(sim_state *sim, u8 city_id);
internal u32       sim_city_grain_income(sim_state *sim, u8 city_id);
internal u32       sim_city_used_slots(sim_state *sim, u8 city_id);   /* built or under construction */

#endif /* SIM_H */
