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
#include "hexdom.h"     /* platform_read_entire_file: the engine reads files only through it */
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

/* An officer of the loaded game, as a front end sees it. Names are the
   Big5 bytes of the save, verbatim and NUL padded: the spec does not name
   the Big5 variant (Q-013), so the engine keeps the bytes. A front end
   decodes them with code/text (text_name). */
struct engine_officer
{
    u16 officer_id;                 /* the save slot; ENGINE_NO_OFFICER for an index out of range */
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
    u8  force_id;                   /* 255 = none */
    u16 base_location;              /* city 0..41, gate or port 42..86, 0xFFFF = none */
    u16 current_position;           /* as base_location, or 87 + unit index */
    u8  status;                     /* 0 ruler, 1 viceroy, 2 governor, 3 regular, 6 free, 8 dead */
    u16 merit;
    u8  skill_number;               /* 255 = none */
    u8  compatibility;
    u16 father;
    u16 spouse;
    u16 sworn_brother_group_head;   /* 0xFFFF = none */
    u8  job_city;                   /* the city of a develop job; ENGINE_NO_CITY if free */
    b32 acted;                      /* the officer gave a command in this turn */
};

/* A city of the loaded game, as a front end sees it. */
struct engine_city
{
    u8  city_id;                    /* ENGINE_NO_CITY for an index out of range */
    u8  owner_force_id;             /* 255 = none */
    u16 commanding_officer_id;      /* 0xFFFF = none */
    u32 capacity_constant;
    u32 gold;
    u32 troops;
    u32 food;
    u32 equipment_stocks[CITY_EQUIPMENT_STOCK_COUNT];   /* spears, halberds, crossbows, war horses, rams, towers, catapults, 3 ships */
    u16 durability_maximum;         /* hypothesis in the spec */
    u8  public_order;               /* hypothesis in the spec */
};

/* The name of a city from the name index (Scenario.s11), as a front end
   sees it: the Big5 bytes of the entry, verbatim and NUL padded. */
struct engine_city_name
{
    u8 city_id;                           /* ENGINE_NO_CITY if no name index is loaded or the id is out of range */
    u8 name[NAME_INDEX_CITY_NAME_SIZE];
};

/* The name of a location from the name index (Scenario.s11): a city 0..41,
   or a gate or a port 42..86. The Big5 bytes of the entry, NUL padded. */
struct engine_location_name
{
    u16 location;                         /* ENGINE_NO_LOCATION if no name index is loaded or the id is out of range */
    u8  name[NAME_INDEX_GATE_ENTRY_SIZE];
};

#define ENGINE_NO_LOCATION 0xFFFF
#define ENGINE_NO_OFFICER  0xFFFF
#define ENGINE_NO_CITY     0xFF
#define ENGINE_NO_FACTION  0xFF
#define ENGINE_NO_FACILITY 0xFF
#define ENGINE_ANY_SLOT    0xFF
#define ENGINE_NO_UNIT     U32_MAX

/* The date of the game (calendar-and-turn.md). */
struct engine_date
{
    u16 year;
    u8  month;              /* 1..12 */
    u8  day;                /* 1, 11 or 21: the first day of the turn */
    u8  season;             /* calendar_season: 0 spring, 1 summer, 2 autumn, 3 winter */
    s64 days_since_start;   /* from the start date of the game or of the loaded save */
};

/* A faction in the order of this turn. */
struct engine_faction
{
    u8  force_id;           /* ENGINE_NO_FACTION for an index out of range */
    u16 city_count;
    u16 officer_count;      /* active officers */
    u32 troops;
    u16 leader_officer_id;  /* the leader of corps 1: the ruler */
    u32 action_points;
    b32 human;              /* a player gives its commands; else the AI placeholder passes */
    b32 acted;              /* it pressed "proceed" (or passed) in this turn */
};

/* A command that a front end can check before it issues it. */
enum engine_command_kind
{
    ENGINE_COMMAND_DEVELOP,
    ENGINE_COMMAND_KIND_COUNT
};

struct engine_command
{
    engine_command_kind kind;
    u8  city_id;
    u8  facility;                           /* ENGINE_COMMAND_DEVELOP */
    u8  slot;                               /* ENGINE_ANY_SLOT: the first free slot */
    u16 officers[SIM_DEVELOP_OFFICERS];     /* ENGINE_NO_OFFICER for an empty place */
};

/* The answer of can_issue: the refusal reason, or SIM_REFUSAL_NONE with
   the cost and the period. */
typedef sim_check engine_check;

struct engine_command_result
{
    sim_refusal refusal;
    u32         command_id;    /* 0 when the command was refused; the game is then unchanged */
};

/* The development area of a city. */
struct engine_city_slots
{
    u8  city_id;            /* ENGINE_NO_CITY if the city is not loaded */
    u8  slot_count;
    u8  used;               /* slots with a facility, built or under construction */
    u8  specialty;          /* city_specialty */
    b32 has_port;
    s32 q, r;               /* the city on the placeholder map */
    u32 gold_income;        /* expected gold next month */
    u32 grain_income;       /* expected grain next season */
    u16 governor_id;        /* the governor by the selection rule; ENGINE_NO_OFFICER if none */
};

/* One slot of a city. */
struct engine_facility
{
    u8  city_id;            /* ENGINE_NO_CITY for a slot out of range */
    u8  slot;
    u8  state;              /* sim_slot_state */
    u8  facility;           /* ENGINE_NO_FACILITY for an empty slot */
    u8  level;
    u16 turns_left;         /* while building */
    s32 q, r;               /* the slot on the placeholder map */
    u32 yield;              /* gold each month or grain each season, with the adjacency bonus */
    u8  yield_effect;       /* facility_effect of the yield */
    u16 officers[SIM_DEVELOP_OFFICERS];
};

/* One row of the facility table of the content pack. */
struct engine_facility_kind
{
    u8   id;                /* ENGINE_NO_FACILITY for an index out of range */
    char key[CONTENT_KEY_SIZE];   /* the text id is "facility.<key>" */
    u32  cost;
    u32  durability;
    u32  min_days;
    u8   effect;            /* facility_effect */
    u32  value;
    u8   limit;             /* facility_limit */
    b32  pk;
    b32  needs_item;
    b32  value_assumed;
    b32  period_assumed;
};

/* The tables of the content pack that the engine loaded at start. */
struct engine_content_status
{
    b32         loaded;           /* all three tables loaded */
    u32         facility_count;
    u32         city_count;
    u32         rule_count;
    const char *detail;           /* a static string: the file that did not load, or "ok" */
};

enum engine_load_status
{
    ENGINE_LOAD_OK,
    ENGINE_LOAD_NO_FILE_READER,     /* the engine was created without read_entire_file */
    ENGINE_LOAD_FILE_NOT_FOUND,     /* the reader returned nothing */
    ENGINE_LOAD_BAD_SAVE,           /* the file is not a save the loader can read; see detail */
    ENGINE_LOAD_BAD_NAME_INDEX,     /* the file is not a name index the loader can read; see detail */
    ENGINE_LOAD_STATUS_COUNT
};

struct engine_load_result
{
    engine_load_status status;
    u32                command_id;  /* 0 when the load failed; the game is then unchanged */
    const char        *detail;      /* a static string: why the save was refused, or "ok" */
};

/* The terrain as the renderer needs it: one kind and one level per cell,
   row-major by offset. The pointers stay valid for the life of the engine;
   version changes whenever a cell changes. Placeholder data until the
   spec's map format exists. */
struct engine_terrain_view
{
    u32       width;
    u32       height;
    const u8 *kind;
    const u8 *level;
    u32       version;
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
typedef u32                 engine_officer_count_fn(void *engine);
typedef engine_officer      engine_officer_at_fn(void *engine, u32 index);   /* index < officer_count */
typedef u32                 engine_city_count_fn(void *engine);
typedef engine_city         engine_city_at_fn(void *engine, u32 index);      /* index < city_count */
typedef engine_city_name    engine_city_name_at_fn(void *engine, u32 index); /* index = city id 0..41 */
typedef engine_officer      engine_officer_by_id_fn(void *engine, u16 officer_id);   /* officer_id ENGINE_NO_OFFICER if not loaded */
typedef u16                 engine_force_ruler_fn(void *engine, u8 force_id);         /* ENGINE_NO_OFFICER if the force has no ruler */
typedef engine_location_name engine_location_name_at_fn(void *engine, u16 location);  /* location 0..86 */
typedef engine_terrain_view engine_terrain_view_fn(void *engine);
typedef engine_date         engine_date_fn(void *engine);
typedef u8                  engine_whose_turn_fn(void *engine);                /* ENGINE_NO_FACTION if every faction acted */
typedef u32                 engine_actions_left_fn(void *engine, u8 force_id); /* the action points of the corps */
typedef u32                 engine_faction_count_fn(void *engine);             /* factions in the order of this turn */
typedef engine_faction      engine_faction_at_fn(void *engine, u32 index);     /* index 0 acts first */
typedef engine_check        engine_can_issue_fn(void *engine, const engine_command *command);
typedef engine_city_slots   engine_city_slots_fn(void *engine, u8 city_id);
typedef engine_facility     engine_facility_at_fn(void *engine, u8 city_id, u8 slot);
typedef u32                 engine_facility_kind_count_fn(void *engine);
typedef engine_facility_kind engine_facility_kind_at_fn(void *engine, u32 index);
typedef engine_content_status engine_content_status_fn(void *engine);
/* Commands */
typedef u32                 engine_end_turn_fn(void *engine);                 /* returns the command id */
typedef u32                 engine_advance_turns_fn(void *engine, u32 count); /* returns events emitted */
typedef void                engine_ledger_clear_fn(void *engine);
typedef void                engine_set_cell_cost_fn(void *engine, s32 q, s32 r, u8 cost); /* editor/debug */
typedef void                engine_set_cell_terrain_fn(void *engine, s32 q, s32 r, u8 kind, u8 level); /* editor/debug */
/* Replaces the officers and cities with those of a .s11 save. A relative
   path is relative to the folder of the executable. */
typedef engine_load_result  engine_load_save_fn(void *engine, const char *path);
/* Replaces the city names with those of a name index (Scenario.s11). The
   officers and cities do not change. A relative path is relative to the
   folder of the executable. */
typedef engine_load_result  engine_load_name_index_fn(void *engine, const char *path);
/* 開發: officers develop a facility in a slot of the city. A refused
   command changes nothing and ledgers nothing. */
typedef engine_command_result engine_command_develop_fn(void *engine, u8 city_id, u8 facility,
                                                        const u16 officers[SIM_DEVELOP_OFFICERS], u8 slot);
/* Setup and editor commands (F-134). They ledger nothing. */
typedef void                engine_set_faction_human_fn(void *engine, u8 force_id, b32 human);
typedef void                engine_set_city_owner_fn(void *engine, u8 city_id, u8 force_id);   /* the order changes at the next turn */
typedef void                engine_set_city_gold_fn(void *engine, u8 city_id, u32 gold);
typedef void                engine_set_actions_fn(void *engine, u8 force_id, u32 action_points);
/* A placeholder unit until M5: it only blocks the commands of enemy
   cities within 2 hexes. Returns ENGINE_NO_UNIT if there is no room. */
typedef u32                 engine_place_unit_fn(void *engine, u8 force_id, s32 q, s32 r);

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
    engine_officer_count_fn   *officer_count;
    engine_officer_at_fn      *officer_at;
    engine_city_count_fn      *city_count;
    engine_city_at_fn         *city_at;
    engine_city_name_at_fn    *city_name_at;
    engine_end_turn_fn        *end_turn;
    engine_advance_turns_fn   *advance_turns;
    engine_ledger_clear_fn    *ledger_clear;
    engine_set_cell_cost_fn   *set_cell_cost;
    engine_terrain_view_fn    *terrain_view;
    engine_set_cell_terrain_fn *set_cell_terrain;
    engine_load_save_fn       *load_save;
    engine_load_name_index_fn *load_name_index;
    /* M1 develop and M2 turns */
    engine_date_fn            *date;
    engine_whose_turn_fn      *whose_turn;
    engine_actions_left_fn    *actions_left;
    engine_faction_count_fn   *faction_count;
    engine_faction_at_fn      *faction_at;
    engine_can_issue_fn       *can_issue;
    engine_city_slots_fn      *city_slots;
    engine_facility_at_fn     *facility_at;
    engine_facility_kind_count_fn *facility_kind_count;
    engine_facility_kind_at_fn    *facility_kind_at;
    engine_content_status_fn  *content_status;
    engine_command_develop_fn *command_develop;
    engine_set_faction_human_fn *set_faction_human;
    engine_set_city_owner_fn  *set_city_owner;
    engine_set_city_gold_fn   *set_city_gold;
    engine_set_actions_fn     *set_actions;
    engine_place_unit_fn      *place_unit;
    /* Names for the id and code columns of the panels */
    engine_officer_by_id_fn   *officer_by_id;
    engine_force_ruler_fn     *force_ruler;
    engine_location_name_at_fn *location_name_at;
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
    platform_read_entire_file *read_entire_file;   /* zero: the load commands refuse with ENGINE_LOAD_NO_FILE_READER */
    const char *content_folder;   /* the folder of facilities.tsv, cities.tsv and rules.tsv; relative to the executable */
};
internal engine_config engine_default_config(void);
internal engine    *engine_create(memory_arena *permanent, memory_arena *transient, engine_config config);
internal engine_api engine_make_api(engine *e);

#endif /* ENGINE_H */
