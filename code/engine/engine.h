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
    u8  morale;                     /* 氣力; the start value is assumed (the save field is not decoded) */
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
#define ENGINE_NO_WEAPON   0xFF
#define ENGINE_ANY_CITY     ENGINE_NO_CITY   /* ledger_filter: no city filter */
#define ENGINE_ANY_RESOURCE 0xFF             /* ledger_filter: no resource filter */

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

/* The order of 出征 and 輸送 (city-commands.md). */
struct engine_sortie_order
{
    u8  city_id;
    u8  unit_type;          /* the transport row for 輸送 */
    u16 officers[SIM_COMMAND_OFFICERS];   /* [0] the commander; ENGINE_NO_OFFICER for an empty place */
    u32 soldiers, gold, grain;
    u8  destination;        /* 輸送 */
    u32 cargo[CITY_EQUIPMENT_STOCK_COUNT];   /* 輸送 */
};

/* A command that a front end can check before it issues it. */
enum engine_command_kind
{
    ENGINE_COMMAND_DEVELOP,
    ENGINE_COMMAND_PATROL,
    ENGINE_COMMAND_CONSCRIPT,
    ENGINE_COMMAND_TRAIN,
    ENGINE_COMMAND_MERCHANT,
    ENGINE_COMMAND_PRODUCE,
    ENGINE_COMMAND_RESEARCH,
    ENGINE_COMMAND_MERGE,
    ENGINE_COMMAND_SORTIE,
    ENGINE_COMMAND_TRANSPORT,
    ENGINE_COMMAND_MOVE,
    ENGINE_COMMAND_WAIT,
    ENGINE_COMMAND_RETREAT,
    ENGINE_COMMAND_SUPPLY,
    ENGINE_COMMAND_ATTACK,         /* M6: unit at the hex q, r */
    ENGINE_COMMAND_JOINT_ATTACK,
    ENGINE_COMMAND_TACTIC,         /* row: the row of tactic_kind_at */
    ENGINE_COMMAND_PLOT,           /* row: the row of plot_kind_at */
    ENGINE_COMMAND_BUILD,          /* row: the row of building_kind_at */
    ENGINE_COMMAND_REPAIR,
    ENGINE_COMMAND_DISMANTLE,
    ENGINE_COMMAND_SEARCH,         /* M7: court; the order is court */
    ENGINE_COMMAND_RECRUIT,
    ENGINE_COMMAND_REWARD,
    ENGINE_COMMAND_GIVE_ITEM,
    ENGINE_COMMAND_APPOINT,
    ENGINE_COMMAND_MATCHMAKE,
    ENGINE_COMMAND_RELEASE,
    ENGINE_COMMAND_EXILE,
    ENGINE_COMMAND_DIPLOMACY,
    ENGINE_COMMAND_FACTION_PLOT,
    ENGINE_COMMAND_DUEL,           /* unit at the hex q, r */
    ENGINE_COMMAND_DUEL_ACTION,    /* row: the action; value: amount */
    ENGINE_COMMAND_DEBATE_ACTION,
    ENGINE_COMMAND_KIND_COUNT
};

struct engine_command
{
    engine_command_kind kind;
    u8  city_id;
    u8  facility;                           /* ENGINE_COMMAND_DEVELOP */
    u8  slot;                               /* develop: ENGINE_ANY_SLOT is the first free slot; merge: the absorber */
    u16 officers[SIM_DEVELOP_OFFICERS];     /* ENGINE_NO_OFFICER for an empty place; merge: officers[0] */
    u8  weapon;                             /* ENGINE_COMMAND_PRODUCE: the row of weapon_kind_at */
    u8  merchant_mode;                      /* ENGINE_COMMAND_MERCHANT: sim_merchant_mode */
    u32 amount;                             /* ENGINE_COMMAND_MERCHANT: the gold to spend or the grain to sell */
    u8  technique;                          /* ENGINE_COMMAND_RESEARCH: the row of technique_kind_at */
    u8  absorbed;                           /* ENGINE_COMMAND_MERGE: the slot that becomes free */
    engine_sortie_order sortie;             /* ENGINE_COMMAND_SORTIE, ENGINE_COMMAND_TRANSPORT */
    u32 unit;                               /* ENGINE_COMMAND_MOVE .. ENGINE_COMMAND_SUPPLY */
    s32 q, r;                               /* ENGINE_COMMAND_MOVE: the hex */
    u32 target_unit;                        /* ENGINE_COMMAND_SUPPLY */
    u32 soldiers, gold, grain;              /* ENGINE_COMMAND_SUPPLY */
    u8  row;                                /* ENGINE_COMMAND_TACTIC, _PLOT, _BUILD: the row of its table; DUEL_ACTION, DEBATE_ACTION: the action */
    u8  value;                              /* ENGINE_COMMAND_DUEL_ACTION, DEBATE_ACTION: the policy, special, place or choice */
    sim_court_order court;                  /* ENGINE_COMMAND_SEARCH .. ENGINE_COMMAND_FACTION_PLOT */
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
    u32 yield;              /* gold each month or grain each season, with the level and the adjacency bonus */
    u8  yield_effect;       /* facility_effect of the yield */
    u16 officers[SIM_DEVELOP_OFFICERS];
    u32 durability;         /* current */
    u32 durability_max;     /* the durability of the facility table */
    u8  job_weapon;         /* a production job of a workshop or a shipyard; ENGINE_NO_WEAPON if none */
    b32 used;               /* a conscription or a production used it in this turn */
};

/* The state of a city that the city commands change (cities-and-facilities.md,
   "Data of a city"). */
struct engine_city_economy
{
    u8  city_id;            /* ENGINE_NO_CITY if the city is not loaded */
    u32 gold, gold_cap;
    u32 grain, grain_cap;
    u32 public_order, order_max;
    b32 bandits_blocked;    /* public order at the threshold (80) or more: no bandits; a flag only until F-007 */
    u32 morale, morale_max;
    u32 soldiers, soldier_cap;
    u32 weapon_cap;
    u32 stocks[CITY_EQUIPMENT_STOCK_COUNT];
    u32 grain_per_gold;     /* the merchant price before the politics bonus (assumed) */
    b32 patrolled;          /* a patrol ran in this turn */
    b32 trained;            /* a train ran in this turn */
    u32 barracks;           /* built barracks */
    u32 barracks_free;      /* built barracks that no conscription used in this turn */
    u32 technique_points;   /* of the owner */
};

/* The income of a city at the next calendar events, with each part. */
struct engine_income_forecast
{
    u8                  city_id;      /* ENGINE_NO_CITY if the city is not loaded */
    b32                 assumed;      /* the amounts come from assumed numbers (Q-021, Q-027, Q-119) */
    sim_income_forecast amounts;
};

/* One row of the weapon table of the content pack. */
struct engine_weapon_kind
{
    u8   id;                /* ENGINE_NO_WEAPON for an index out of range */
    char key[CONTENT_KEY_SIZE];   /* the text id is "weapon.<key>" */
    u32  cost;
    u8   facility;          /* facility_effect of the facility that produces it */
    u8   specialty;         /* city_specialty */
    u32  stock;             /* the index into engine_city_economy.stocks */
    u32  min_days;          /* 0: the weapons arrive at once */
    u32  max_days;
    u8   technique;         /* the technique that the faction needs; ENGINE_NO_TECHNIQUE if none */
    u8   replaces;          /* the weak weapon that this one replaces; ENGINE_NO_WEAPON if none */
};

#define ENGINE_NO_TECHNIQUE 0xFF
#define ENGINE_NO_DISASTER  0xFF

/* One row of the technique table (research-techniques.md). */
struct engine_technique_kind
{
    u8   id;                /* ENGINE_NO_TECHNIQUE for an index out of range */
    char key[CONTENT_KEY_SIZE];   /* the text id is "technique.<key>" */
    u8   line;              /* technique_line */
    u32  level;             /* 1 to 4 */
    u32  points;            /* technique points */
    u32  gold;
    u8   ability;           /* content_ability that shortens the period */
    u32  min_days;
    u32  max_days;
    b32  period_assumed;
    u8   effect;            /* technique_effect */
    u8   unit_class;        /* content_unit_class */
    u32  value;
    u8   previous;          /* the technique that this one needs; ENGINE_NO_TECHNIQUE for level 1 */
};

/* The techniques of a faction and its research. */
struct engine_faction_techniques
{
    u8  force_id;           /* ENGINE_NO_FACTION for a bad id */
    u64 owned;              /* bit t: the faction has technique t */
    u32 technique_points;
    u8  researching;        /* ENGINE_NO_TECHNIQUE if no research runs */
    u8  research_city;
    u16 turns_left;
    u16 officers[SIM_COMMAND_OFFICERS];
};

/* One row of the disaster table (calendar-and-turn.md, "Disasters"). */
struct engine_disaster_kind
{
    u8   id;                /* ENGINE_NO_DISASTER for an index out of range */
    char key[CONTENT_KEY_SIZE];   /* the text id is "disaster.<key>" */
    u32  month_mask;        /* bit m: it can start on day 1 of month m */
    u32  chance_percent;
    u32  turns;
};

/* The disasters that run in a city: the turns left of each row of the
   table, 0 = not running, ENGINE_DISASTER_OPEN = until a rule ends it. */
#define ENGINE_DISASTER_OPEN 0xFFFF
struct engine_city_disasters
{
    u8  city_id;            /* ENGINE_NO_CITY if the city is not loaded */
    u32 running;            /* the number of running disasters */
    u16 turns[CONTENT_DISASTER_CAPACITY];
};

/* The end of the game (calendar-and-turn.md, "Victory and defeat"). */
struct engine_outcome
{
    u8  kind;               /* sim_outcome_kind; SIM_OUTCOME_NONE while the game runs */
    u8  force_id;           /* the faction of the outcome */
    u32 end_year;           /* the year of the defeat by time (351) */
};

/* ---- M5: the map, the unit classes and the units ------------------------- */

/* One row of terrain.tsv (terrain-and-map.md). */
struct engine_terrain_kind
{
    u8   id;                /* ENGINE_NO_TERRAIN for an index out of range */
    char key[CONTENT_KEY_SIZE];   /* the text id is "terrain.<key>" */
    u8   cost[MOVEMENT_CLASS_COUNT];   /* 0: the class cannot enter */
    u8   needs_technique;   /* ENGINE_NO_TECHNIQUE if none */
    u32  loss_percent;
    b32  buildable;
    u32  tactics;           /* bit tactic_class */
    u8   tile;
    u8   level;
};

/* One row of unit_types.tsv (military-units-and-tactics.md). */
struct engine_unit_type
{
    u8   id;                /* ENGINE_NO_UNIT_TYPE for an index out of range */
    char key[CONTENT_KEY_SIZE];   /* the text id is "unit_type.<key>" */
    u8   weapon;            /* ENGINE_NO_WEAPON if none */
    u8   aptitude;          /* officer_unit_aptitude, or 255 */
    u8   movement;          /* movement_class */
    u32  move;
    u8   attack, defence, destroy;   /* unit_grade */
    u32  range;
    b32  can_attack;
    b32  counterattack;
    b32  transport;
};

#define ENGINE_NO_TERRAIN   0xFF
#define ENGINE_NO_UNIT_TYPE 0xFF

/* The five values of a unit (military-units-and-tactics.md). */
struct engine_unit_values
{
    u32 attack, defence, intellect, build, move;
    u32 aptitude;           /* 0 C .. 3 S; 255 none */
    b32 ignores_zoc;
    b32 assumed;            /* the formula is assumed (Q-170) */
};

/* A unit on the map. */
struct engine_unit
{
    u32 id;                 /* ENGINE_NO_UNIT if no unit lives there */
    u8  force_id;
    s32 q, r;
    u8  unit_type;          /* ENGINE_NO_UNIT_TYPE for a placed test unit */
    u16 officers[SIM_COMMAND_OFFICERS];   /* [0] the commander */
    u32 soldiers, gold, grain, morale, weapons;
    u32 cargo[CITY_EQUIPMENT_STOCK_COUNT];
    u8  home_city;
    u8  destination;        /* a transport unit: its city; ENGINE_NO_CITY */
    b32 moved;              /* it moved in this turn */
    b32 acted;              /* it waited, retreated, supplied or fought in this turn */
    engine_unit_values values;
    u8  state;              /* M6: sim_unit_state */
    u8  state_turns;
};

/* ---- M6: combat, plots, fire, buildings, siege -------------------------- */

/* One row of tactics.tsv (military-units-and-tactics.md). */
struct engine_tactic_kind
{
    u8   id;                /* ENGINE_NO_ROW for an index out of range */
    char key[CONTENT_KEY_SIZE];   /* the text id is "tactic.<key>" */
    u32  unit_types;        /* bit unit type */
    u8   aptitude;          /* the lowest aptitude: 0 C .. 3 S */
    u32  morale;            /* the morale cost (気力) */
    u32  range_min, range_max;
    u8   pattern;           /* tactic_pattern */
    u8   effect;            /* tactic_effect */
    u32  damage_percent;
    b32  always_success;
};

/* One row of plots.tsv (plots-and-states.md). */
struct engine_plot_kind
{
    u8   id;
    char key[CONTENT_KEY_SIZE];   /* the text id is "plot.<key>" */
    u32  morale;
    u32  range;
    u8   target;            /* plot_target */
    u8   effect;            /* plot_effect */
    u32  turns;
};

/* One row of buildings.tsv (military-facilities.md, obstacles-and-traps.md). */
struct engine_building_kind
{
    u8   id;
    char key[CONTENT_KEY_SIZE];   /* the text id is "building.<key>" */
    u8   type;              /* building_type */
    u32  durability;
    u32  gold;
    u8   technique;         /* ENGINE_NO_TECHNIQUE if none */
    u32  range_min, range_max;
    b32  natural;
};

/* A building, a trap or a natural trap on the map. */
struct engine_building
{
    u32 id;                 /* ENGINE_NO_BUILDING if none lives there */
    u8  kind;               /* the row of building_kind_at */
    u8  force_id;           /* ENGINE_NO_FACTION for a trap or a natural trap */
    s32 q, r;
    u32 durability, durability_max;
    b32 complete;
    u8  city_id;            /* a dike: the city it floods; ENGINE_NO_CITY */
};

/* The turns of fire of each cell, row-major by offset. The engine owns it. */
struct engine_fire_view
{
    u32       width, height;
    const u8 *turns;        /* 0: no fire */
};

/* The siege state of a city. */
struct engine_city_siege
{
    u8  city_id;            /* ENGINE_NO_CITY if the city is not loaded */
    u32 durability, durability_max;
    u32 soldiers;
    b32 surrounded;         /* enemy units on every free neighbour hex */
};

#define ENGINE_NO_ROW      0xFF
#define ENGINE_NO_BUILDING 0xFFFFFFFFu

/* ---- M8: bases, corps, council, adviser, emperor, events, AI ------------ */

struct engine_base
{
    u8   id;                        /* the location id 42 .. 86; ENGINE_NO_ROW for an index out of range */
    char key[CONTENT_KEY_SIZE];     /* the text id is "base.<key>" */
    u8   type;                      /* base_type */
    u8   owner_city;
    u8   force;                     /* ENGINE_NO_FACTION */
    u32  durability, durability_max;
    u32  troops, troop_cap;
    s32  q, r;
};

struct engine_corps
{
    b32 alive;
    u16 viceroy;
    u8  policy;                     /* sim_corps_policy */
    u8  target;
    u32 flags;                      /* sim_corps_flag */
    u8  transport_city;
    u32 action_points;
    u32 city_count;
};

/* One proposal of the planner: the AI, the council or the adviser. */
struct engine_proposal
{
    u8  kind;                       /* sim_command_kind */
    u8  city_id;
    u16 officer;                    /* the first officer of the command, or ENGINE_NO_OFFICER */
    u32 score;
    char key[CONTENT_KEY_SIZE];     /* the key of ai_priorities.tsv */
};

#define ENGINE_PLAN_CAPACITY 16
struct engine_plan_view
{
    sim_refusal     refusal;        /* the council: why no plan */
    u32             count;
    engine_proposal items[ENGINE_PLAN_CAPACITY];
};

struct engine_emperor
{
    u8 who;                         /* sim_emperor_kind */
    u8 city;
    u8 protector;                   /* ENGINE_NO_FACTION */
};

struct engine_relic
{
    b32 alive;
    u8  kind;                       /* sim_relic_kind */
    s32 q, r;
};

struct engine_event_kind
{
    u8   id;                        /* ENGINE_NO_ROW */
    char key[CONTENT_KEY_SIZE];     /* the text id is "event.<key>" */
    u8   check;                     /* event_check */
    u32  min_days, chance;
};

struct engine_pending_event
{
    b32 active;
    u8  event;                      /* the row of event_kind_at */
    u8  force;
    u16 officer_a, officer_b;
    u8  city;
    u8  choices;
};

/* ---- M9: save and load (manual p.6: 32 slots with a comment, no autosave) */

#define ENGINE_SAVE_SLOTS        32
#define ENGINE_SAVE_COMMENT_SIZE 64

enum engine_save_status
{
    ENGINE_SAVE_OK,
    ENGINE_SAVE_NO_WRITER,        /* the config has no file writer */
    ENGINE_SAVE_NO_READER,        /* the config has no file reader */
    ENGINE_SAVE_BAD_SLOT,         /* the slot is not 0 .. 31 */
    ENGINE_SAVE_NO_GAME,          /* no game is loaded */
    ENGINE_SAVE_NO_MEMORY,        /* the transient arena cannot hold the file */
    ENGINE_SAVE_WRITE_FAILED,
    ENGINE_SAVE_NO_FILE,          /* the slot is empty */
    ENGINE_SAVE_BAD_FILE,         /* not a hexdom save, or cut short */
    ENGINE_SAVE_OTHER_VERSION,    /* another build of the engine wrote it */
    ENGINE_SAVE_OTHER_PACK,       /* another content pack wrote it */
    ENGINE_SAVE_STATUS_COUNT
};

struct engine_save_slot
{
    b32  used;
    char comment[ENGINE_SAVE_COMMENT_SIZE];
    u16  year;
    u8   month, day;
    u8   player;                  /* the faction that acted when the game was saved */
    u32  turn;
};

/* ---- M7: officers, court, items, diplomacy, duel, debate ---------------- */

/* The order of the personnel, court, diplomacy and plot commands. */
typedef sim_court_order engine_court_order;

/* The M7 state of an officer (officers-and-relations.md). */
struct engine_officer_detail
{
    u16 officer_id;                 /* ENGINE_NO_OFFICER if not loaded */
    u8  loyalty, ambition, justice; /* ambition and justice: 0 .. 4, 2 normal */
    u8  personality;                /* sim_personality */
    u8  growth;                     /* sim_growth */
    u8  health;                     /* sim_health */
    u8  stamina;
    u8  post;                       /* the row of post_kind_at; ENGINE_NO_ROW */
    b32 female;
    b32 captive;
    u8  former_force;               /* a captive: its faction before */
    u16 liked[OFFICER_RELATION_SLOTS];
    u16 disliked[OFFICER_RELATION_SLOTS];
    u16 experience[CONTENT_ABILITY_COUNT];
    u32 age;
    u32 soldier_cap;                /* the soldiers that the officer leads */
    u32 recruit_chance;             /* the chance for the acting faction now, 0 if it cannot recruit the officer */
};

/* The editor fields of an officer that the save does not decode (Q-177). */
struct engine_officer_edit
{
    u8  loyalty, ambition, justice, personality, growth;
    b32 female;
    u16 merit;
    u8  captor;                     /* a faction: the officer becomes its captive; ENGINE_NO_FACTION: no change */
};

struct engine_skill_kind
{
    u8   number;                    /* 1 .. 100; 0 for an index out of range */
    char key[CONTENT_KEY_SIZE];     /* the text id is "skill.<key>" */
    char effect[CONTENT_KEY_SIZE];
    u8   category;                  /* skill_category */
    b32  hooked;                    /* the engine applies it */
};

struct engine_post_kind
{
    u8   id;                        /* ENGINE_NO_ROW for an index out of range */
    char key[CONTENT_KEY_SIZE];     /* the text id is "post.<key>" */
    u32  merit, soldiers, bonus;
    u8   ability;                   /* content_ability; CONTENT_ABILITY_COUNT for none */
    u8   rank;
};

struct engine_rank_kind
{
    u8   id;
    char key[CONTENT_KEY_SIZE];     /* the text id is "rank.<key>" */
    u32  soldiers, cities;
};

struct engine_item
{
    u8   id;                        /* the item id; ENGINE_NO_ROW if no item */
    u8   name[ITEM_NAME_SIZE];      /* Big5, NUL padded */
    char key[CONTENT_KEY_SIZE];     /* the key of items.tsv, or "" if the kind is not known */
    u8   type;                      /* item_type */
    u8   strength;
    u16  owner;                     /* ENGINE_NO_OFFICER */
    u8   city;                      /* ENGINE_NO_CITY */
};

struct engine_diplomacy
{
    u32 friendliness;               /* 0 .. 100 */
    u32 level;                      /* 0 險惡 .. 4 親密 */
    u8  relation;                   /* sim_relation */
    u32 months;                     /* a ceasefire */
};

struct engine_duel_side
{
    u16 officers[SIM_COMMAND_OFFICERS];
    u8  stamina[SIM_COMMAND_OFFICERS];
    u8  fighter;
    u32 spirit;
    u8  policy;
    u8  force;
    b32 boosted, guarded;           /* 集氣 or 堅守 runs; the defence critical runs */
};

struct engine_duel_view
{
    b32              active;
    u32              round;
    u8               result;        /* sim_duel_result after the end */
    u8               winner;        /* 0 or 1; 2 for a draw */
    engine_duel_side side[2];       /* 0 the challenger */
    b32              ready[DUEL_SPECIAL_COUNT];   /* the special commands of side 0 that can run now */
};

struct engine_debate_side
{
    u16      officer;
    sim_card hand[SIM_HAND_CAPACITY];
    u8       hand_count;
    s32      mind;
    u32      rocks;
    u32      anger;
    u32      rage_rounds;
};

struct engine_debate_view
{
    b32                active;
    b32                finished;
    u32                round;
    u8                 topic;       /* sim_topic */
    u8                 turn;        /* the side that speaks */
    u8                 winner;
    u8                 purpose;     /* sim_debate_purpose */
    engine_debate_side side[2];     /* 0 the officer of the acting faction */
    sim_card           last[2];     /* type CARD_TYPE_COUNT if none */
};

/* The hexes that a unit can reach now, with their costs. The engine owns
   the arrays; the next reachable call replaces them. */
struct engine_reach_view
{
    u32              unit;
    u32              count;     /* the first cell is the hex of the unit */
    const hex_coord *cells;
    const u32       *costs;
};

/* The game settings (game-setup-and-modes.md; F-132 adds the rest in M9). */
struct engine_settings
{
    b32 disasters;          /* the calendar starts disasters */
    b32 ability_change;     /* M7: 能力變動: abilities fall with age */
    b32 lifespan;           /* M7: officers die near their historical year */
    b32 ai;                 /* M8: the computer factions and the delegated corps act */
    b32 events;             /* M8: 事件 */
};

/* The indices of the ledger events that a filter keeps, in ledger order.
   The engine owns the array; the next ledger_filter call replaces it. */
struct engine_ledger_view
{
    u32        count;
    const u32 *indices;
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
    b32         loaded;           /* all four tables loaded */
    u32         facility_count;
    u32         city_count;
    u32         weapon_count;
    u32         rule_count;
    u32         technique_count;
    u32         disaster_count;
    u32         terrain_count;
    u32         unit_type_count;
    u32         tactic_count;     /* M6 */
    u32         plot_count;
    u32         building_count;
    u32         critical_count;
    u32         skill_count;      /* M7 */
    u32         rank_count;
    u32         post_count;
    u32         item_count;
    u32         base_count;       /* M8 */
    u32         event_count;
    u32         priority_count;
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
    const u8 *tile;     /* the colour of the atlas for each cell, from the terrain table; kind when no table loaded */
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
/* M3 economy queries */
typedef engine_city_economy    engine_city_economy_fn(void *engine, u8 city_id);
typedef engine_income_forecast engine_city_income_forecast_fn(void *engine, u8 city_id);
typedef u32                    engine_technique_points_fn(void *engine, u8 force_id);
typedef u32                    engine_weapon_kind_count_fn(void *engine);
typedef engine_weapon_kind     engine_weapon_kind_at_fn(void *engine, u32 index);
/* The ledger events of a city (or ENGINE_ANY_CITY) that change a resource
   (sim_resource, or ENGINE_ANY_RESOURCE). A query: it changes nothing. */
typedef engine_ledger_view     engine_ledger_filter_fn(void *engine, u8 city_id, u8 resource);
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
/* 巡察, 徵兵, 訓練, 商人 and 生產兵裝 (city-commands.md). A refused command
   changes nothing and ledgers nothing. The merchant takes one officer. */
typedef engine_command_result engine_command_city_fn(void *engine, u8 city_id, const u16 officers[SIM_DEVELOP_OFFICERS]);
typedef engine_command_result engine_command_merchant_fn(void *engine, u8 city_id, u16 officer, u8 mode, u32 amount);
typedef engine_command_result engine_command_produce_fn(void *engine, u8 city_id, u8 weapon,
                                                        const u16 officers[SIM_DEVELOP_OFFICERS]);
/* A test command until an attacker exists (M6): it takes durability from a
   facility and ledgers the damage. At 0 the facility is destroyed. */
typedef engine_command_result engine_damage_facility_fn(void *engine, u8 city_id, u8 slot, u32 amount);
/* Setup and editor commands (F-134). They ledger nothing. */
/* M4 queries */
typedef u32                       engine_technique_kind_count_fn(void *engine);
typedef engine_technique_kind     engine_technique_kind_at_fn(void *engine, u32 index);
typedef engine_faction_techniques engine_faction_techniques_fn(void *engine, u8 force_id);
typedef u32                       engine_disaster_kind_count_fn(void *engine);
typedef engine_disaster_kind      engine_disaster_kind_at_fn(void *engine, u32 index);
typedef engine_city_disasters     engine_city_disasters_fn(void *engine, u8 city_id);
typedef engine_outcome            engine_outcome_fn(void *engine);
typedef engine_settings           engine_settings_fn(void *engine);
/* M4 commands. 研究技巧: up to three officers research a technique. 吸收合併:
   one officer merges the absorbed slot into the absorber. A refused command
   changes nothing and ledgers nothing. */
typedef engine_command_result engine_command_research_fn(void *engine, u8 city_id, u8 technique, const u16 officers[SIM_DEVELOP_OFFICERS]);
typedef engine_command_result engine_command_merge_fn(void *engine, u8 city_id, u8 absorber, u8 absorbed, u16 officer);
/* A test and editor command: starts a disaster in a city with its start
   events. The calendar starts disasters at random. */
typedef engine_command_result engine_start_disaster_fn(void *engine, u8 city_id, u8 disaster);
/* Setup and editor commands. They ledger nothing. */
typedef void                engine_set_settings_fn(void *engine, engine_settings settings);
typedef void                engine_grant_technique_fn(void *engine, u8 force_id, u8 technique);   /* a scenario can start with techniques */
typedef void                engine_set_technique_points_fn(void *engine, u8 force_id, u32 points);
typedef void                engine_set_date_fn(void *engine, u16 year, u8 month, u8 day);   /* day 1, 11 or 21; the start date stays */
/* M5 queries */
typedef u32                 engine_terrain_kind_count_fn(void *engine);
typedef engine_terrain_kind engine_terrain_kind_at_fn(void *engine, u32 index);
typedef u32                 engine_unit_type_count_fn(void *engine);
typedef engine_unit_type    engine_unit_type_at_fn(void *engine, u32 index);
typedef u32                 engine_unit_count_fn(void *engine);                  /* the places of the units, alive or not */
typedef engine_unit         engine_unit_at_fn(void *engine, u32 unit_id);        /* id ENGINE_NO_UNIT if no unit lives there */
typedef u32                 engine_unit_at_hex_fn(void *engine, s32 q, s32 r);   /* ENGINE_NO_UNIT if none */
typedef engine_reach_view   engine_reachable_fn(void *engine, u32 unit_id);
typedef engine_unit_values  engine_unit_values_fn(void *engine, u8 force_id, u8 unit_type, const u16 officers[SIM_COMMAND_OFFICERS]);
typedef engine_sortie_order engine_sortie_proposal_fn(void *engine, u8 city_id, u8 unit_type);   /* 自動編制 */
/* M5 commands. A refused command changes nothing and ledgers nothing. */
typedef engine_command_result engine_command_sortie_fn(void *engine, const engine_sortie_order *order);
typedef engine_command_result engine_command_move_fn(void *engine, u32 unit_id, s32 q, s32 r);
typedef engine_command_result engine_command_unit_fn(void *engine, u32 unit_id);   /* 待命, 退卻 */
typedef engine_command_result engine_command_supply_fn(void *engine, u32 unit_id, u32 target_unit, u32 soldiers, u32 gold, u32 grain);
typedef b32                 engine_set_facility_level_fn(void *engine, u8 city_id, u8 slot, u8 level);  /* a built market, farm, barracks, smithy or stable; level 1 to 3 */
typedef void                engine_set_city_resource_fn(void *engine, u8 city_id, u8 resource, u32 index, u32 value); /* index: the weapon stock */
typedef void                engine_set_faction_human_fn(void *engine, u8 force_id, b32 human);
typedef void                engine_set_city_owner_fn(void *engine, u8 city_id, u8 force_id);   /* the order changes at the next turn */
typedef void                engine_set_city_gold_fn(void *engine, u8 city_id, u32 gold);
typedef void                engine_set_actions_fn(void *engine, u8 force_id, u32 action_points);
/* M6 queries */
typedef u32                  engine_tactic_kind_count_fn(void *engine);
typedef engine_tactic_kind   engine_tactic_kind_at_fn(void *engine, u32 index);
typedef u32                  engine_plot_kind_count_fn(void *engine);
typedef engine_plot_kind     engine_plot_kind_at_fn(void *engine, u32 index);
typedef u32                  engine_building_kind_count_fn(void *engine);
typedef engine_building_kind engine_building_kind_at_fn(void *engine, u32 index);
typedef u32                  engine_building_count_fn(void *engine);               /* the places of the buildings, alive or not */
typedef engine_building      engine_building_at_fn(void *engine, u32 building_id);
typedef u32                  engine_building_at_hex_fn(void *engine, s32 q, s32 r); /* ENGINE_NO_BUILDING if none */
typedef engine_fire_view     engine_fire_view_fn(void *engine);
typedef engine_city_siege    engine_city_siege_fn(void *engine, u8 city_id);
/* M6 commands. The unit acts on the hex q, r. A refused command changes
   nothing and ledgers nothing. */
typedef engine_command_result engine_command_target_fn(void *engine, u32 unit_id, s32 q, s32 r);          /* 攻擊, 共同攻擊, 修補, 撤去 */
typedef engine_command_result engine_command_row_fn(void *engine, u32 unit_id, u8 row, s32 q, s32 r);     /* 戰法, 計略, 建設 */
/* Test and editor commands. They ledger nothing. */
typedef u32                  engine_place_building_fn(void *engine, u8 kind, u8 force_id, s32 q, s32 r); /* a complete building; ENGINE_NO_BUILDING if refused */
typedef void                 engine_set_unit_fn(void *engine, u32 unit_id, u32 soldiers, u32 morale);
typedef b32                  engine_set_unit_place_fn(void *engine, u32 unit_id, u8 force_id, s32 q, s32 r);   /* false if the hex is not free */
typedef void                 engine_set_city_durability_fn(void *engine, u8 city_id, u32 durability);
/* M7 queries */
typedef engine_officer_detail engine_officer_detail_fn(void *engine, u16 officer_id);
typedef u32                   engine_skill_kind_count_fn(void *engine);
typedef engine_skill_kind     engine_skill_kind_at_fn(void *engine, u32 index);
typedef u32                   engine_post_kind_count_fn(void *engine);
typedef engine_post_kind      engine_post_kind_at_fn(void *engine, u32 index);
typedef u32                   engine_rank_kind_count_fn(void *engine);
typedef engine_rank_kind      engine_rank_kind_at_fn(void *engine, u32 index);
typedef u8                    engine_faction_rank_fn(void *engine, u8 force_id);
typedef u16                   engine_post_holder_fn(void *engine, u8 force_id, u8 post);   /* ENGINE_NO_OFFICER if free */
typedef u32                   engine_item_count_fn(void *engine);
typedef engine_item           engine_item_at_fn(void *engine, u32 index);
typedef engine_diplomacy      engine_diplomacy_fn(void *engine, u8 force_a, u8 force_b);
typedef engine_duel_view      engine_duel_view_fn(void *engine);
typedef engine_debate_view    engine_debate_view_fn(void *engine);
/* M7 commands. A refused command changes nothing and ledgers nothing. The
   court order names the city of the acting officer. kind: the
   ENGINE_COMMAND_SEARCH .. ENGINE_COMMAND_FACTION_PLOT command. */
typedef engine_command_result engine_command_court_fn(void *engine, engine_command_kind kind, const engine_court_order *order);
typedef engine_command_result engine_command_duel_fn(void *engine, u32 unit_id, s32 q, s32 r);          /* 單挑 */
typedef engine_command_result engine_command_mini_fn(void *engine, u8 action, u8 value);               /* sim_duel_action, sim_debate_action */
/* Test and editor commands. They ledger nothing, except the debate that
   they start. */
typedef engine_command_result engine_start_debate_fn(void *engine, u16 officer_a, u16 officer_b);
typedef void                  engine_set_officer_fn(void *engine, u16 officer_id, const engine_officer_edit *edit);
typedef void                  engine_set_item_owner_fn(void *engine, u8 item, u16 officer_id);
typedef void                  engine_set_diplomacy_fn(void *engine, u8 force_a, u8 force_b, u32 friendliness, u8 relation, u32 months);
typedef void                  engine_set_stamina_fn(void *engine, u16 officer_id, u8 stamina);
/* M8 queries */
typedef u32                  engine_base_count_fn(void *engine);
typedef engine_base          engine_base_at_fn(void *engine, u32 index);
typedef u32                  engine_base_at_hex_fn(void *engine, s32 q, s32 r);       /* the index, or ENGINE_NO_BASE */
typedef engine_corps         engine_corps_at_fn(void *engine, u8 force_id, u8 corps);
typedef u8                   engine_city_corps_fn(void *engine, u8 city_id);
typedef u16                  engine_adviser_fn(void *engine, u8 force_id);
typedef engine_plan_view     engine_advice_fn(void *engine, u8 force_id);
typedef engine_plan_view     engine_council_fn(void *engine, u8 force_id, u8 city_id);
typedef engine_plan_view     engine_ai_plan_fn(void *engine, u8 force_id);           /* what the AI would do now */
typedef engine_emperor       engine_emperor_fn(void *engine);
typedef u32                  engine_relic_count_fn(void *engine);
typedef engine_relic         engine_relic_at_fn(void *engine, u32 index);
typedef u32                  engine_event_kind_count_fn(void *engine);
typedef engine_event_kind    engine_event_kind_at_fn(void *engine, u32 index);
typedef engine_pending_event engine_pending_event_fn(void *engine);
/* M8 commands. A refused command changes nothing and ledgers nothing. */
typedef engine_command_result engine_command_corps_fn(void *engine, const sim_corps_order *order);
typedef engine_command_result engine_command_adviser_fn(void *engine, u8 force_id, u16 officer);
typedef engine_command_result engine_command_council_fn(void *engine, u8 force_id, u8 city_id, u8 proposal);
typedef engine_command_result engine_command_event_choice_fn(void *engine, u8 choice);
/* Test and editor commands. trigger_event fires an event now (unit: the
   unit of a wait event); the others ledger nothing. */
typedef engine_command_result engine_trigger_event_fn(void *engine, u8 event, u8 force_id, u8 city_id, u16 officer_or_unit);
typedef void                  engine_set_emperor_fn(void *engine, u8 who, u8 city_id);
typedef void                  engine_set_base_fn(void *engine, u32 index, u8 force_id, u32 troops, u32 durability);
/* M9 save and load. save_game writes the slot file and its comment; the
   ledger gets nothing. load_game replaces the game and the ledger with the
   slot; a refused load changes nothing. save_slot answers from the list
   that refresh_save_slots and save_game keep. */
typedef engine_save_status engine_save_game_fn(void *engine, u32 slot, const char *comment);
typedef engine_save_status engine_load_game_fn(void *engine, u32 slot);
typedef engine_save_slot   engine_save_slot_fn(void *engine, u32 slot);
typedef void               engine_refresh_save_slots_fn(void *engine);
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
    /* M3 economy */
    engine_city_economy_fn          *city_economy;
    engine_city_income_forecast_fn  *city_income_forecast;
    engine_technique_points_fn      *technique_points;
    engine_weapon_kind_count_fn     *weapon_kind_count;
    engine_weapon_kind_at_fn        *weapon_kind_at;
    engine_ledger_filter_fn         *ledger_filter;
    engine_command_city_fn          *command_patrol;
    engine_command_city_fn          *command_conscript;
    engine_command_city_fn          *command_train;
    engine_command_merchant_fn      *command_merchant;
    engine_command_produce_fn       *command_produce;
    engine_damage_facility_fn       *damage_facility;
    engine_set_facility_level_fn    *set_facility_level;
    engine_set_city_resource_fn     *set_city_resource;
    /* M4 growth: research, merge, disasters, the end of the game */
    engine_technique_kind_count_fn  *technique_kind_count;
    engine_technique_kind_at_fn     *technique_kind_at;
    engine_faction_techniques_fn    *faction_techniques;
    engine_disaster_kind_count_fn   *disaster_kind_count;
    engine_disaster_kind_at_fn      *disaster_kind_at;
    engine_city_disasters_fn        *city_disasters;
    engine_outcome_fn               *outcome;
    engine_settings_fn              *settings;
    engine_command_research_fn      *command_research;
    engine_command_merge_fn         *command_merge;
    engine_start_disaster_fn        *start_disaster;
    engine_set_settings_fn          *set_settings;
    engine_grant_technique_fn       *grant_technique;
    engine_set_technique_points_fn  *set_technique_points;
    engine_set_date_fn              *set_date;
    /* M5 march: the map, the units and their commands */
    engine_terrain_kind_count_fn    *terrain_kind_count;
    engine_terrain_kind_at_fn       *terrain_kind_at;
    engine_unit_type_count_fn       *unit_type_count;
    engine_unit_type_at_fn          *unit_type_at;
    engine_unit_count_fn            *unit_count;
    engine_unit_at_fn               *unit_at;
    engine_unit_at_hex_fn           *unit_at_hex;
    engine_reachable_fn             *reachable;
    engine_unit_values_fn           *unit_values;
    engine_sortie_proposal_fn       *sortie_proposal;
    engine_command_sortie_fn        *command_sortie;
    engine_command_sortie_fn        *command_transport;
    engine_command_move_fn          *command_move;
    engine_command_unit_fn          *command_wait;
    engine_command_unit_fn          *command_retreat;
    engine_command_supply_fn        *command_supply;
    /* M6 combat: attacks, tactics, plots, fire, buildings, siege */
    engine_tactic_kind_count_fn     *tactic_kind_count;
    engine_tactic_kind_at_fn        *tactic_kind_at;
    engine_plot_kind_count_fn       *plot_kind_count;
    engine_plot_kind_at_fn          *plot_kind_at;
    engine_building_kind_count_fn   *building_kind_count;
    engine_building_kind_at_fn      *building_kind_at;
    engine_building_count_fn        *building_count;
    engine_building_at_fn           *building_at;
    engine_building_at_hex_fn       *building_at_hex;
    engine_fire_view_fn             *fire_view;
    engine_city_siege_fn            *city_siege;
    engine_command_target_fn        *command_attack;
    engine_command_target_fn        *command_joint_attack;
    engine_command_row_fn           *command_tactic;
    engine_command_row_fn           *command_plot;
    engine_command_row_fn           *command_build;
    engine_command_target_fn        *command_repair;
    engine_command_target_fn        *command_dismantle;
    engine_place_building_fn        *place_building;
    engine_set_unit_fn              *set_unit;
    engine_set_unit_place_fn        *set_unit_place;
    engine_set_city_durability_fn   *set_city_durability;
    /* M7 court: officers, posts, items, diplomacy, duel, debate */
    engine_officer_detail_fn        *officer_detail;
    engine_skill_kind_count_fn      *skill_kind_count;
    engine_skill_kind_at_fn         *skill_kind_at;
    engine_post_kind_count_fn       *post_kind_count;
    engine_post_kind_at_fn          *post_kind_at;
    engine_rank_kind_count_fn       *rank_kind_count;
    engine_rank_kind_at_fn          *rank_kind_at;
    engine_faction_rank_fn          *faction_rank;
    engine_post_holder_fn           *post_holder;
    engine_item_count_fn            *item_count;
    engine_item_at_fn               *item_at;
    engine_diplomacy_fn             *diplomacy;
    engine_duel_view_fn             *duel_view;
    engine_debate_view_fn           *debate_view;
    engine_command_court_fn         *command_court;
    engine_command_duel_fn          *command_duel;
    engine_command_mini_fn          *command_duel_action;
    engine_command_mini_fn          *command_debate_action;
    engine_start_debate_fn          *start_debate;
    engine_set_officer_fn           *set_officer;
    engine_set_item_owner_fn        *set_item_owner;
    engine_set_diplomacy_fn         *set_diplomacy;
    engine_set_stamina_fn           *set_stamina;
    /* M8 rivals: bases, corps, council, adviser, emperor, events, AI */
    engine_base_count_fn            *base_count;
    engine_base_at_fn               *base_at;
    engine_base_at_hex_fn           *base_at_hex;
    engine_corps_at_fn              *corps_at;
    engine_city_corps_fn            *city_corps;
    engine_adviser_fn               *adviser;
    engine_advice_fn                *advice;
    engine_council_fn               *council;
    engine_ai_plan_fn               *ai_plan;
    engine_emperor_fn               *emperor;
    engine_relic_count_fn           *relic_count;
    engine_relic_at_fn              *relic_at;
    engine_event_kind_count_fn      *event_kind_count;
    engine_event_kind_at_fn         *event_kind_at;
    engine_pending_event_fn         *pending_event;
    engine_command_corps_fn         *command_corps;
    engine_command_adviser_fn       *command_adviser;
    engine_command_council_fn       *command_council;
    engine_command_event_choice_fn  *command_event_choice;
    engine_trigger_event_fn         *trigger_event;
    engine_set_emperor_fn           *set_emperor;
    engine_set_base_fn              *set_base;
    /* M9 save and load */
    engine_save_game_fn             *save_game;
    engine_load_game_fn             *load_game;
    engine_save_slot_fn             *save_slot;
    engine_refresh_save_slots_fn    *refresh_save_slots;
};

#define ENGINE_NO_BASE 0xFFFFFFFFu

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
    platform_write_entire_file *write_entire_file; /* M9: zero: save_game refuses with ENGINE_SAVE_NO_WRITER */
    const char *save_folder;      /* M9: the folder of the 32 save slots; relative to the executable */
    const char *content_folder;   /* the folder of facilities.tsv, cities.tsv and rules.tsv; relative to the executable */
};
internal engine_config engine_default_config(void);
internal engine    *engine_create(memory_arena *permanent, memory_arena *transient, engine_config config);
internal engine_api engine_make_api(engine *e);

#endif /* ENGINE_H */
