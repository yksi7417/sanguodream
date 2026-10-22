#ifndef CONTENT_H
#define CONTENT_H
/* The tables of the content pack that the turn rules and the develop
   command read: data/<pack>/facilities.tsv, cities.tsv and rules.tsv. The
   engine reads the files; this module parses the rows into fixed arrays.
   No mechanic reads a number that is not in one of these tables. */

#include "base.h"
#include "sim/tables.h"
#include "sim/save_format.h"

#define CONTENT_FACILITY_CAPACITY 32
#define CONTENT_KEY_SIZE          32
#define CONTENT_NO_FACILITY       0xFF

enum facility_effect
{
    FACILITY_EFFECT_NONE,
    FACILITY_EFFECT_GOLD,               /* gold income each month */
    FACILITY_EFFECT_GRAIN,              /* grain income each season */
    FACILITY_EFFECT_MINT,               /* adjacent gold facilities give more */
    FACILITY_EFFECT_GRANARY,            /* adjacent grain facilities give more */
    FACILITY_EFFECT_CONSCRIPTION,
    FACILITY_EFFECT_WEAPONS,
    FACILITY_EFFECT_HORSES,
    FACILITY_EFFECT_SIEGE,
    FACILITY_EFFECT_SHIPS,
    FACILITY_EFFECT_TECHNIQUE,
    FACILITY_EFFECT_ACTION_POINTS,
    FACILITY_EFFECT_PERSONNEL,
    FACILITY_EFFECT_PLOT,
    FACILITY_EFFECT_DIPLOMACY,
    FACILITY_EFFECT_MILITARY,
    FACILITY_EFFECT_DRILL,
    FACILITY_EFFECT_GRAIN_BY_SOLDIERS,  /* grain each season for each "value" soldiers */
    FACILITY_EFFECT_COUNT
};

enum facility_limit
{
    FACILITY_LIMIT_NONE,
    FACILITY_LIMIT_CITY,        /* one in each city */
    FACILITY_LIMIT_BIG_CITY,    /* one, and only in a big city */
    FACILITY_LIMIT_PORT_CITY,   /* one, and only in a city with a port */
    FACILITY_LIMIT_TERRITORY,   /* one in the whole territory of the faction */
    FACILITY_LIMIT_COUNT
};

struct facility_kind
{
    u8              id;
    char            key[CONTENT_KEY_SIZE];   /* the text id is "facility.<key>" */
    u32             cost;
    u32             durability;
    u32             min_days;                /* the shortest period */
    b32             period_assumed;
    facility_effect effect;
    u32             value;                   /* see facility_effect */
    b32             value_assumed;
    facility_limit  limit;
    b32             needs_item;
    b32             adjacency_bonus;         /* an adjacent mint or granary multiplies the value */
    b32             new_year_removal;        /* the game removes it on 1 January */
    b32             mergeable;               /* the PK merge can raise its level to 2 and 3 (manual p.28) */
    b32             pk;
};

enum city_specialty
{
    CITY_SPECIALTY_NONE,
    CITY_SPECIALTY_BIG_CITY,
    CITY_SPECIALTY_PIKE,
    CITY_SPECIALTY_HALBERD,
    CITY_SPECIALTY_CROSSBOW,
    CITY_SPECIALTY_HORSE,
    CITY_SPECIALTY_SIEGE,
    CITY_SPECIALTY_NAVY,
    CITY_SPECIALTY_COUNT
};

struct city_kind
{
    u8             slot_count;
    city_specialty specialty;
    b32            has_port;
    u8             price;          /* the start price of the merchant, R grain for 1 gold (G2-013) */
};

/* One weapon of weapons.tsv (weapons-and-production.md). */
#define CONTENT_WEAPON_CAPACITY 16
#define CONTENT_NO_WEAPON       0xFF

struct weapon_kind
{
    u8              id;
    char            key[CONTENT_KEY_SIZE];   /* the text id is "weapon.<key>" */
    u32             cost;                    /* the gold of one production command */
    facility_effect facility;                /* the effect of the facility that produces it */
    city_specialty  specialty;               /* the specialty that makes it cheaper */
    u32             stock;                   /* the index into the weapon stocks of a city */
    u32             min_days;                /* 0 for a weapon that arrives at once */
    u32             max_days;
    u8              technique;               /* the technique that the faction needs; CONTENT_NO_TECHNIQUE if none */
    u8              replaces;                /* the weak weapon that this one replaces; CONTENT_NO_WEAPON if none */
};

/* ---- techniques (research-techniques.md) -------------------------------- */

#define CONTENT_TECHNIQUE_CAPACITY 64
#define CONTENT_NO_TECHNIQUE       0xFF

enum technique_line
{
    TECHNIQUE_LINE_PIKE,
    TECHNIQUE_LINE_HALBERD,
    TECHNIQUE_LINE_CROSSBOW,
    TECHNIQUE_LINE_CAVALRY,
    TECHNIQUE_LINE_DRILL,
    TECHNIQUE_LINE_INVENTION,
    TECHNIQUE_LINE_DEFENCE,
    TECHNIQUE_LINE_FIRE,
    TECHNIQUE_LINE_PK,
    TECHNIQUE_LINE_COUNT
};

/* The ability that shortens a research (and the other commands). */
enum content_ability
{
    CONTENT_ABILITY_LEADERSHIP,
    CONTENT_ABILITY_FORCE,
    CONTENT_ABILITY_INTELLECT,
    CONTENT_ABILITY_POLITICS,
    CONTENT_ABILITY_CHARM,
    CONTENT_ABILITY_COUNT
};

/* The flag that a technique sets. The rules of later milestones read the
   flags (M5 movement, M6 combat); M4 applies the effects that have a rule
   now: the morale cap, the facility recovery, the weapons and the order. */
enum technique_effect
{
    TECHNIQUE_EFFECT_NONE,
    TECHNIQUE_EFFECT_TACTIC_DAMAGE,
    TECHNIQUE_EFFECT_RAID_GRAIN,
    TECHNIQUE_EFFECT_FOREST_NO_COUNTER,
    TECHNIQUE_EFFECT_ELITE,
    TECHNIQUE_EFFECT_ARROW_BLOCK,
    TECHNIQUE_EFFECT_ATTACK_BLOCK,
    TECHNIQUE_EFFECT_COUNTER_ARROW,
    TECHNIQUE_EFFECT_RANGE,
    TECHNIQUE_EFFECT_MOVEMENT,
    TECHNIQUE_EFFECT_MOUNTED_ARCHERY,
    TECHNIQUE_EFFECT_MORALE_CAP,
    TECHNIQUE_EFFECT_ROUGH_MARCH,
    TECHNIQUE_EFFECT_SOLDIER_CAP,
    TECHNIQUE_EFFECT_BASE_ATTACK,
    TECHNIQUE_EFFECT_SIEGE_SPEED,
    TECHNIQUE_EFFECT_STONE_BUILD,
    TECHNIQUE_EFFECT_CATAPULT,
    TECHNIQUE_EFFECT_THUNDER,
    TECHNIQUE_EFFECT_FACILITY_RECOVERY,
    TECHNIQUE_EFFECT_FORT_BUILD,
    TECHNIQUE_EFFECT_CITADEL,
    TECHNIQUE_EFFECT_BASE_COUNTER,
    TECHNIQUE_EFFECT_WOOD_BEAST,
    TECHNIQUE_EFFECT_FIRE_RANGE,
    TECHNIQUE_EFFECT_FIRE_SEED_2,
    TECHNIQUE_EFFECT_FIRE_SEED_3,
    TECHNIQUE_EFFECT_TRANSPORT_SPEED,
    TECHNIQUE_EFFECT_PORT_CAPS,
    TECHNIQUE_EFFECT_ORDER_KEEP,
    TECHNIQUE_EFFECT_LOYALTY_KEEP,
    TECHNIQUE_EFFECT_COUNT
};

/* The unit class that an effect changes. The classes of M5 use the same
   order (military-units-and-tactics.md). */
enum content_unit_class
{
    CONTENT_CLASS_NONE,
    CONTENT_CLASS_PIKE,
    CONTENT_CLASS_HALBERD,
    CONTENT_CLASS_CROSSBOW,
    CONTENT_CLASS_CAVALRY,
    CONTENT_CLASS_SIEGE,
    CONTENT_CLASS_COUNT
};

struct technique_kind
{
    u8                 id;
    char               key[CONTENT_KEY_SIZE];   /* the text id is "technique.<key>" */
    technique_line     line;
    u32                level;                   /* 1 to 4; level n needs level n - 1 of the line */
    u32                points;
    u32                gold;
    content_ability    ability;
    u32                min_days;
    u32                max_days;
    b32                period_assumed;
    technique_effect   effect;
    content_unit_class unit_class;
    u32                value;
    b32                value_assumed;
};

/* ---- terrain and unit types (terrain-and-map.md, military-units-and-tactics.md) */

enum movement_class
{
    MOVEMENT_FOOT,
    MOVEMENT_CAVALRY,
    MOVEMENT_SIEGE,
    MOVEMENT_NAVY,
    MOVEMENT_CLASS_COUNT
};

/* The bits of terrain_kind.tactics: the classes whose tactics work there. */
enum tactic_class
{
    TACTIC_PIKE,
    TACTIC_HALBERD,
    TACTIC_CROSSBOW,
    TACTIC_CAVALRY,
    TACTIC_SIEGE,
    TACTIC_NAVY,
    TACTIC_CLASS_COUNT
};

#define CONTENT_TERRAIN_CAPACITY 32
#define CONTENT_NO_TERRAIN       0xFF

struct terrain_kind
{
    u8   id;
    char key[CONTENT_KEY_SIZE];   /* the text id is "terrain.<key>" */
    u8   cost[MOVEMENT_CLASS_COUNT];   /* movement points to enter; 0 = the class cannot enter */
    u8   needs_technique;         /* CONTENT_NO_TECHNIQUE if none */
    u32  loss_percent;            /* soldiers lost on entry */
    u8   loss_waiver_technique;   /* CONTENT_NO_TECHNIQUE if none */
    u32  loss_waiver_skill;       /* a skills.md number; 0 = none */
    b32  buildable;               /* a military facility, an obstacle or a fire trap can stand here (F-147) */
    u32  tactics;                 /* bit tactic_class: the tactics of that class work here */
    u8   tile;                    /* the colour of the placeholder atlas */
    u8   level;                   /* the height of the placeholder map */
    b32  enterable;               /* a unit can stand on it (terrain-types.tsv); 川, 岸 and 崖 are not */
    b32  burnable;                /* a fire stays on it without a unit (F-149) */
};

/* One row of movement_costs.tsv (movement-costs.tsv of the spec): an
   armament, its movement allowance and the cost to enter each terrain. */
#define CONTENT_ARMAMENT_CAPACITY 16
#define CONTENT_NO_ARMAMENT       0xFF

/* One row of save_types.tsv (M11): what a facility type of the building
   array of a save (formats/save-file.md, fixtures/map/facility-types.tsv)
   is in the pack. */
#define CONTENT_SAVE_TYPE_COUNT 256
enum save_type_kind
{
    SAVE_TYPE_NONE,       /* the pack has nothing for it; the load skips it */
    SAVE_TYPE_BASE,       /* a city, a gate or a port: records 0..86 */
    SAVE_TYPE_FACILITY,   /* a domestic facility on a slot: row of facilities.tsv and level */
    SAVE_TYPE_BUILDING,   /* a military facility, an obstacle or a trap: row of buildings.tsv */
    SAVE_TYPE_NATURAL,    /* a dike or a rockfall of the map: row of buildings.tsv */
    SAVE_TYPE_TEMPLE,     /* an event site with a temple */
    SAVE_TYPE_RUIN,       /* an event site with a ruin */
    SAVE_TYPE_KIND_COUNT
};

struct save_type
{
    u8 kind;    /* save_type_kind */
    u8 row;     /* the row of facilities.tsv or buildings.tsv */
    u8 level;   /* a facility: 1, 2 or 3 */
};

struct armament_kind
{
    u8   id;
    char key[CONTENT_KEY_SIZE];
    u32  move;                                /* the movement allowance (stat_c) */
    u8   cost[CONTENT_TERRAIN_CAPACITY];      /* by terrain id; 0 = it cannot enter (255 in the spec) */
};

/* The grades of the manual: S > A > B > C > D > E; GRADE_NONE for "-". */
enum unit_grade
{
    GRADE_NONE,
    GRADE_S,
    GRADE_A,
    GRADE_B,
    GRADE_C,
    GRADE_D,
    GRADE_E,
    GRADE_COUNT
};

#define CONTENT_UNIT_TYPE_CAPACITY 16
#define CONTENT_NO_UNIT_TYPE       0xFF
#define CONTENT_APTITUDE_NONE      0xFF

struct unit_type_kind
{
    u8             id;
    char           key[CONTENT_KEY_SIZE];   /* the text id is "unit_type.<key>" */
    u8             weapon;                  /* the weapon that it carries; CONTENT_NO_WEAPON if none */
    u8             aptitude;                /* officer_unit_aptitude; CONTENT_APTITUDE_NONE */
    movement_class movement;
    u32            move;
    unit_grade     attack;
    unit_grade     defence;
    unit_grade     destroy;
    u32            range;
    b32            can_attack;
    b32            counterattack;
    b32            transport;
    u8             armament;                /* the row of movement_costs.tsv on land; CONTENT_NO_ARMAMENT: the class costs */
    u8             ship;                    /* the row on 河 and 海; CONTENT_NO_ARMAMENT: the class costs */
};

/* ---- combat (military-units-and-tactics.md, plots-and-states.md,
        military-facilities-and-traps.md) ---------------------------------- */

enum tactic_pattern { PATTERN_SINGLE, PATTERN_LINE, PATTERN_FRONT3, PATTERN_AROUND, PATTERN_SPLASH, PATTERN_COUNT };
enum tactic_effect  { TACTIC_NONE, TACTIC_KNOCK, TACTIC_PULL, TACTIC_ADVANCE, TACTIC_PASS, TACTIC_CONFUSE, TACTIC_FIRE, TACTIC_EFFECT_COUNT };

#define TARGET_UNITS     1u
#define TARGET_BUILDINGS 2u
#define TARGET_SHIPS     4u
#define CONTENT_TACTIC_CAPACITY   32
#define CONTENT_PLOT_CAPACITY     16
#define CONTENT_BUILDING_CAPACITY 32
#define CONTENT_CRITICAL_CAPACITY 32
#define CONTENT_NO_BUILDING       0xFF
#define CONTENT_ANY_APTITUDE      0xFF

struct tactic_kind
{
    u8             id;
    char           key[CONTENT_KEY_SIZE];   /* the text id is "tactic.<key>" */
    u32            unit_types;              /* bit t: unit type t uses it */
    u32            aptitude;                /* the aptitude that the unit needs: 0 any (C), 1 B, 2 A, 3 S */
    u32            morale;
    u32            range_min, range_max;
    tactic_pattern pattern;
    tactic_effect  effect;
    u32            value;
    u32            damage_percent;
    u32            targets;                 /* TARGET_* bits */
    b32            always_success;
};

enum plot_effect { PLOT_FIRE, PLOT_EXTINGUISH, PLOT_FALSE_REPORT, PLOT_CONFUSE, PLOT_CALM, PLOT_AMBUSH, PLOT_INFIGHTING,
                   PLOT_SORCERY, PLOT_LIGHTNING, PLOT_EFFECT_COUNT };
enum plot_target { PLOT_TARGET_ENEMY, PLOT_TARGET_OWN, PLOT_TARGET_HEX, PLOT_TARGET_COUNT };

struct plot_kind
{
    u8          id;
    char        key[CONTENT_KEY_SIZE];      /* the text id is "plot.<key>" */
    u32         morale;
    u32         range;
    b32         splash;                     /* the target and the six hexes around it */
    plot_target target;
    plot_effect effect;
    u32         turns;
    u32         needs_skill, needs_skill_2;   /* skills.md numbers; 0 = none */
    u8          from_terrain;               /* CONTENT_NO_TERRAIN if any */
    u32         not_unit_types;             /* bit t: unit type t cannot use it */
};

enum building_type { BUILDING_CAMP, BUILDING_TOWER, BUILDING_DRUM, BUILDING_MUSIC, BUILDING_MAZE, BUILDING_WALL,
                     BUILDING_SEED, BUILDING_BALL, BUILDING_SHIP, BUILDING_DIKE, BUILDING_ROCKFALL, BUILDING_TYPE_COUNT };

struct building_kind
{
    u8            id;
    char          key[CONTENT_KEY_SIZE];    /* the text id is "building.<key>" */
    building_type type;
    u32           durability;
    u32           gold;
    u8            technique;                /* CONTENT_NO_TECHNIQUE if none */
    u8            replaces;                 /* CONTENT_NO_BUILDING if none */
    u32           range_min, range_max;
    u32           value;
    b32           water;                    /* only on water (fire ship) */
    b32           natural;                  /* a natural trap of the map: no unit builds it */
};

enum critical_action    { CRITICAL_ATTACK = 1, CRITICAL_TACTIC = 2, CRITICAL_BOTH = 3 };
enum critical_condition { CRITICAL_ALWAYS, CRITICAL_LOWER_FORCE, CRITICAL_FOREST, CRITICAL_BUILDING, CRITICAL_MOUNTED, CRITICAL_CONDITION_COUNT };

struct critical_skill
{
    u32                skill;               /* skills.md number */
    u32                action;              /* critical_action bits */
    u32                aptitude;            /* the class by its aptitude (0 spear .. 5 navy); CONTENT_ANY_APTITUDE */
    critical_condition condition;
};

/* ---- disasters (calendar-and-turn.md, "Disasters") ---------------------- */

#define CONTENT_DISASTER_CAPACITY 8
#define CONTENT_NO_DISASTER       0xFF
#define CONTENT_TARGET_ANY        FACILITY_EFFECT_COUNT   /* facility_target "any" */

struct disaster_kind
{
    u8              id;
    char            key[CONTENT_KEY_SIZE];   /* the text id is "disaster.<key>" */
    u32             month_mask;              /* bit m set: the disaster can start on day 1 of month m (1..12) */
    u32             chance_percent;
    u32             turns;                   /* 0: no length (harvest, or bandits that end by public order) */
    u32             start_order_loss;
    u32             turn_order_loss;
    u32             grain_percent;
    u32             soldier_percent;
    u32             facility_damage;
    u32             facility_target;         /* a facility_effect, CONTENT_TARGET_ANY, or FACILITY_EFFECT_NONE */
    b32             low_order_only;
    b32             ends_at_order;
    u32             skill_block;             /* the skill that stops it in the city; 0 = none */
    u32             skill_chance;            /* the skill that changes the chance; 0 = none */
    u32             skill_chance_percent;    /* the chance with that skill, in percent of chance_percent */
};
/* A disaster with turns = 0 and ends_at_order = no acts at once (the good
   harvest): grain_percent adds grain. A disaster with turns > 0 runs for
   that many turns: grain_percent takes grain from each grain payment. */

/* ---- M7: skills, posts, ranks and items --------------------------------- */

#define CONTENT_SKILL_COUNT     100   /* skills.md: the numbers 1 to 100 */
#define CONTENT_POST_CAPACITY   96
#define CONTENT_RANK_CAPACITY   16
#define CONTENT_ITEM_CAPACITY   32
#define CONTENT_NO_POST         0xFF
#define CONTENT_NO_ITEM_KIND    0xFF
#define CONTENT_BIG5_NAME_SIZE  16    /* the Big5 bytes of an item name in items.tsv */

/* One row of skills.tsv: a skill of skills.md. The column applies is
   "rule" (the loader writes the number into the rule field
   "skill.<effect>"), "table" (another table of the pack names the number:
   critical_skills.tsv, terrain.tsv, plots.tsv, disasters.tsv) or "none"
   (no effect yet). hooked is true for rule and table. */
struct skill_kind
{
    u8   number;                      /* 1 .. 100 */
    u8   file_id;                     /* 0 .. 99: the skill byte of a scenario or save record (officer +124) */
    char key[CONTENT_KEY_SIZE];
    char effect[CONTENT_KEY_SIZE];
    u8   category;                    /* skill_category */
    b32  hooked;
};

enum skill_category
{
    SKILL_MOVEMENT, SKILL_ATTACK, SKILL_DEFENCE, SKILL_PLOT, SKILL_SUPPORT, SKILL_DOMESTIC, SKILL_INCOME, SKILL_DISASTER,
    SKILL_RELATION, SKILL_CATEGORY_COUNT
};

/* One row of posts.tsv (factions-corps-and-posts.md, 官職). */
struct post_kind
{
    u8   id;
    char key[CONTENT_KEY_SIZE];
    u32  merit;                       /* the merit that the post needs */
    u32  soldiers;                    /* the soldiers that the holder leads */
    u8   ability;                     /* content_ability that the post raises, or CONTENT_ABILITY_COUNT */
    u32  bonus;
    u8   rank;                        /* the lowest rank of the ruler (the row of ranks.tsv) that allows the post */
};

/* One row of ranks.tsv (爵位). */
struct rank_kind
{
    u8   id;
    char key[CONTENT_KEY_SIZE];
    u32  soldiers;                    /* the soldiers of the ruler */
    u32  cities;                      /* the cities that the faction needs */
    b32  needs_emperor;               /* the faction protects the Han emperor, or no emperor exists */
};

enum item_type
{
    ITEM_HORSE, ITEM_BOW, ITEM_SWORD, ITEM_BOOK, ITEM_POLEARM, ITEM_SEAL, ITEM_HIDDEN_WEAPON, ITEM_BRONZE, ITEM_OTHER,
    ITEM_TYPE_COUNT
};

enum item_polearm { POLEARM_PLAIN, POLEARM_SKY_HALBERD, POLEARM_SNAKE_SPEAR, POLEARM_DRAGON_BLADE, POLEARM_COUNT };

/* One row of items.tsv: the kind of an item of the save, found by the Big5
   bytes of its name (save-file.md, Item record). */
struct item_kind
{
    u8   id;
    char key[CONTENT_KEY_SIZE];
    u8   big5[CONTENT_BIG5_NAME_SIZE];
    u32  big5_size;
    u8   type;                        /* item_type */
    u8   polearm;                     /* item_polearm */
};

/* ---- M8: bases, events, the AI ------------------------------------------ */

#define CONTENT_BASE_COUNT        45    /* the 10 passes and 35 ports; location ids 42 to 86 */
#define CONTENT_FIRST_BASE        42
#define CONTENT_EVENT_CAPACITY    32
#define CONTENT_PRIORITY_CAPACITY 16
#define CONTENT_EVENT_NAMES       10
#define CONTENT_EVENT_PARAMS      6

enum base_type { BASE_PASS, BASE_PORT, BASE_TYPE_COUNT };

/* One row of bases.tsv (bases-ports-and-passes.md). */
struct base_kind
{
    u8   id;                          /* the location id, 42 .. 86 */
    char key[CONTENT_KEY_SIZE];       /* the text id is "base.<key>" */
    u8   type;                        /* base_type */
    u8   owner_city;
    u32  durability;
};

enum event_check { EVENT_CHECK_QUARTER, EVENT_CHECK_MONTH, EVENT_CHECK_SEARCH, EVENT_CHECK_WAIT, EVENT_CHECK_NONE, EVENT_CHECK_COUNT };

/* One row of events.tsv (events.md, generic events). The handler of the
   key reads p[0..5] in its own order. */
struct event_kind
{
    u8   id;
    char key[CONTENT_KEY_SIZE];       /* the text id is "event.<key>" */
    u8   check;                       /* event_check */
    u32  min_days;
    u32  months;                      /* bit m: the month m (1 .. 12); quarter checks only */
    u32  chance;
    u32  p[CONTENT_EVENT_PARAMS];
    u8   names[CONTENT_EVENT_NAMES][OFFICER_FAMILY_NAME_SIZE + OFFICER_GIVEN_NAME_SIZE];   /* Big5 family + given name */
    u8   name_sizes[CONTENT_EVENT_NAMES];
    u32  name_count;
};

enum ai_condition
{
    AI_ORDER_BELOW, AI_TROOPS_BELOW_PERCENT, AI_FREE_SLOT, AI_FREE_OFFICER, AI_MORALE_BELOW, AI_WEAPONS_BELOW_TROOPS,
    AI_LOYALTY_BELOW, AI_TROOPS_ABOVE, AI_TECHNIQUE_ABOVE, AI_GOLD_ABOVE,
    AI_ENEMY_NEAR, AI_FREE_CITY_NEAR,   /* M15 */
    AI_CONDITION_COUNT
};

/* One row of ai_priorities.tsv: a kind of proposal of the AI planner. */
struct ai_priority
{
    u8   id;
    char key[CONTENT_KEY_SIZE];
    u8   command;                     /* sim_command_kind */
    u32  priority;
    u8   condition;                   /* ai_condition */
    u32  threshold;
};

/* M9: one row of ability_chart.tsv, the 能力表 of the Power Up Kit
   (pk-officer-training.md). Research makes the row usable count times;
   training gives it to one officer. */
#define CONTENT_CHART_CAPACITY 32
#define CONTENT_NO_CHART_ROW   0xFF
enum chart_kind { CHART_PLUS, CHART_FLOOR, CHART_APTITUDE, CHART_SKILL, CHART_KIND_COUNT };
enum chart_zone { CHART_ZONE_ATTACK, CHART_ZONE_PLOT, CHART_ZONE_DEFENCE, CHART_ZONE_DOMESTIC, CHART_ZONE_COUNT };

struct chart_row
{
    u8   id;
    char key[CONTENT_KEY_SIZE];       /* the text id is "chart.<key>" */
    u8   zone;                        /* chart_zone */
    u8   kind;                        /* chart_kind */
    u8   target;                      /* plus, floor: content_ability; aptitude: the class 0..5; skill: the skill number */
    u8   amount;                      /* plus: the rise; floor: the value; aptitude: the grade after (0 C .. 3 S) */
    u8   count;                       /* the uses that one research gives */
    u8   months;                      /* the months of the research */
    u32  requires;                    /* bit r: the row r must be researched first */
};

/* M9: one row of ancient_officers.tsv: an officer of another era that a
   player can add as a new officer (game-setup-and-modes.md). */
#define CONTENT_ANCIENT_CAPACITY 16
struct ancient_officer
{
    u8   id;
    char key[CONTENT_KEY_SIZE];
    u8   family_name[OFFICER_FAMILY_NAME_SIZE];
    u8   given_name[OFFICER_GIVEN_NAME_SIZE];
    u8   abilities[CONTENT_ABILITY_COUNT];
    u8   skill;                       /* the skill number; 0 = none */
    u8   personality;                 /* sim_personality order: careful, calm, bold, reckless */
    b32  female;
};

/* M9: one row of history.tsv: a historical event of events.md as data. The
   engine has one interpreter for the clauses (history.cpp). */
#define CONTENT_HISTORY_CAPACITY 48
#define CONTENT_HISTORY_NAMES    8
#define CONTENT_HISTORY_TEXT     384
struct history_kind
{
    u8   id;
    char key[CONTENT_KEY_SIZE];       /* the text id is "history.<key>" */
    u32  chance;                      /* percent at each check */
    char conditions[CONTENT_HISTORY_TEXT];
    char effects[CONTENT_HISTORY_TEXT];
    u8   names[CONTENT_HISTORY_NAMES][OFFICER_FAMILY_NAME_SIZE + OFFICER_GIVEN_NAME_SIZE];
    u8   name_sizes[CONTENT_HISTORY_NAMES];
    u32  name_count;
};

/* M9: one row of portraits.tsv: the age of the second portrait. */
#define CONTENT_PORTRAIT_CAPACITY 32
struct portrait_kind
{
    u8 name[OFFICER_FAMILY_NAME_SIZE + OFFICER_GIVEN_NAME_SIZE];
    u8 name_size;
    u8 age;
};

/* The numbers of rules.tsv. A missing row leaves 0. */
struct sim_rules
{
    u32 ap_cap;
    u32 ap_develop;
    u32 ap_patrol;
    u32 ap_conscript;
    u32 ap_train;
    u32 ap_merchant;
    u32 ap_produce;
    u32 patrol_gold;
    u32 conscript_gold;
    u32 order_max;
    u32 order_bandit_threshold;
    u32 order_income_floor;
    u32 patrol_leadership_percent;
    u32 patrol_enemy_percent;
    u32 conscript_enemy_percent;
    u32 conscript_order_loss;
    u32 conscript_morale_loss;
    u32 city_soldier_cap;
    u32 city_big_soldier_cap;
    u32 city_weapon_cap;
    u32 city_start_morale;
    u32 morale_max;
    u32 train_force_percent;
    u32 train_drill_percent;
    u32 produce_intellect_quantity;
    u32 produce_full_intellect;
    u32 produce_specialty_percent;
    u32 produce_free_monthly;
    u32 produce_skill_percent;
    u32 produce_skill_period_percent;
    u32 level2_percent;
    u32 level3_percent;
    u32 skill_income_percent;
    u32 skill_rich;
    u32 skill_rice;
    u32 skill_tax;
    u32 skill_levy;
    u32 skill_capable;
    u32 skill_breeding;
    u32 skill_invention;
    u32 skill_shipbuilding;
    /* The technique points by action (G2-014; rules.tsv technique.*). */
    u32 technique_patrol, technique_train, technique_conscript_min, technique_conscript_max, technique_conscript_soldiers;
    u32 technique_develop, technique_merchant, technique_produce_weapon, technique_produce_siege, technique_produce_ship;
    u32 technique_facility_built, technique_merge, technique_facility_lost, technique_facility_lost_leveled;
    u32 technique_city_lost, technique_city_lost_facility, technique_tactic, technique_tactic_extra_unit;
    u32 technique_plot, technique_plot_strife, technique_goodwill, technique_alliance, technique_ceasefire;
    u32 technique_exchange_sender, technique_exchange_releaser, technique_aid, technique_surrender;
    u32 technique_two_tigers, technique_rumour, technique_drive_tiger, technique_search_found, technique_recruit;
    u32 unit_grain_soldiers;           /* G2-049: the soldiers that one grain feeds in a unit each turn */
    u32 sortie_grain_per_soldier;      /* G2-044: the grain that the preset proposes for each soldier */
    u32 merchant_price_min, merchant_price_max;   /* the city price (city-commands.md) */
    u32 merchant_effect_scale, merchant_effect_offset, merchant_sale_percent;   /* E = scale / (offset - politics); a sale gets sale_percent */
    u32 conscript_base_soldiers, conscript_charm_tenths;   /* city-commands.md: F x (1000 + 5.6 x S) */
    /* The rules of the run of 2026-10-22 (spec/experiments/gap2-answers-2026-10-22.md). */
    u32 conscript_charm_order;                          /* F x (1000 + 6 x S x order / 100) */
    u32 train_base, train_divisor, train_drill_base, train_drill_permille;
    u32 produce_base_quantity;
    u32 class_attack_pike, class_attack_halberd, class_attack_crossbow, class_attack_cavalry;
    u32 class_defence_pike, class_defence_halberd, class_defence_crossbow, class_defence_cavalry;
    u32 sword_percent, build_base, build_percent;
    u32 merit_attack, merit_arrow, merit_tactic, merit_destroy, merit_tactic_destroy;
    u32 experience_attack_war, experience_tactic_war, experience_destroy_leadership, experience_hit_leadership;
    u32 technique_drill_ground_turn;
    u32 merit_per_command;
    u32 facility_recovery;
    u32 develop_officers;
    u32 develop_max_days;
    u32 develop_full_politics;
    u32 enemy_block_hexes;
    u32 adjacency_percent;
    u32 city_gold_cap;
    u32 city_grain_cap;
    u32 city_gold_income;
    u32 city_grain_income;
    u32 officer_salary;
    u32 grain_soldiers;
    /* M4: research, merge, the PK facilities, the end of the game */
    u32 ap_research;
    u32 ap_merge;
    u32 merge_gold;
    u32 merge_max_days;
    u32 merge_min_days;
    u32 merge_full_politics;
    u32 research_full_ability;
    u32 research_personnel_percent;
    u32 research_teaching_percent;
    u32 skill_teaching;
    u32 tally_action_points;
    u32 military_office_ap_percent;
    u32 drill_ship_period_percent;
    u32 game_end_year;
    /* M5: units, movement and the map */
    u32 ap_sortie;
    u32 ap_transport;
    u32 unit_soldier_cap;
    u32 unit_deputy_percent;
    u32 grade_percent[GRADE_COUNT];     /* GRADE_NONE stays 0 */
    u32 aptitude_percent[4];            /* C, B, A, S: the save values 0..3 */
    u32 move_bonus;
    u32 skill_ignore_zoc;
    u32 skill_escape;
    u32 skill_forced_march;
    u32 skill_long_drive;
    u32 skill_haul;
    /* M6: combat, plots, buildings and siege */
    u32 combat_damage_percent, combat_min_damage, combat_counter_percent, combat_triangle_percent;
    u32 combat_critical_chance, combat_critical_percent, combat_morale_hit, combat_tactic_success;
    u32 combat_building_percent, combat_siege_percent, combat_base_counter;
    u32 combat_capture_percent, combat_halberd_capture, combat_destroy_technique;
    u32 plot_base_success;
    u32 fire_turns, fire_damage_percent, fire_building_damage, fire_extra_cost;
    u32 fire_cost_multiplier, fire_attack_turns_min, fire_attack_turns_max, fire_other_turns_min, fire_other_turns_max;   /* M10 */
    u32 rockfall_return_percent;   /* M10 */
    u32 build_durability_per_build;
    u32 siege_blockade_loss, siege_recovery, siege_default_durability, siege_capture_technique;
    u32 skill_capture, skill_escape_capture, skill_fortune, skill_subtle, skill_fire_god, skill_rattan;
    u32 skill_insight, skill_strategist, skill_feint, skill_calm, skill_mirror, skill_discipline, skill_music;
    u32 combat_collision_percent, combat_ambush_percent, combat_lightning_percent, combat_technique_percent;
    u32 maze_confuse_chance, camp_recover_chance, natural_recovery, loot_percent, ap_dismantle, dismantle_technique;
    u32 confusion_recover_floor, confusion_recover_cap;     /* the recovery from 混亂 each turn (assumed, Q-218) */
    u32 panic_soldiers, panic_ratio, panic_chance;          /* a small unit next to a large enemy confuses itself (assumed, Q-218) */
    u32 siege_ladder_percent, siege_defence_percent, siege_wall_percent;
    u32 skill_ambush_critical, skill_long_plot, skill_surprise, skill_sweep, skill_awe, skill_archer;
    /* M7: officers, personnel, posts, duel, debate, diplomacy, faction plots */
    u32 ap_search, ap_recruit, ap_reward, ap_give_item, ap_diplomacy, ap_faction_plot;
    u32 ap_move_officers, ap_summon;   /* 移動, 召喚 (travel.cpp) */
    u32 reward_gold, reward_loyalty, reward_compatibility_divisor;
    u32 search_base, search_politics_percent, search_gold_chance, search_gold, search_debate_intellect;
    u32 recruit_base, recruit_charm_percent, recruit_captive_bonus, recruit_relation_bonus, recruit_new_loyalty;
    u32 recruit_compatibility_divisor, recruit_debate_intellect;
    u32 loyalty_start, loyalty_monthly, loyalty_ambition, loyalty_unpaid, loyalty_leave, loyalty_leave_chance;
    u32 experience_command, experience_level, age_decline, death_chance, wound_turns, stamina_recovery;
    u32 death_month_chance;   /* G2-055: the chance at each month start past the death year */
    u32 release_technique, exile_technique, matchmaking_technique, matchmaking_merit, matchmaking_age;
    u32 relation_liked_percent, relation_disliked_intellect;
    u32 duel_damage, duel_attack_percent, duel_defence_percent, duel_spirit_rise, duel_spirit_hit, duel_critical_rounds;
    u32 duel_max_rounds, duel_capture_percent, duel_merit, duel_technique, duel_force_chance, duel_morale_loss;
    u32 duel_accept[4];                 /* careful, calm, bold, reckless */
    u32 duel_lower_force_percent, duel_finisher, duel_vital, duel_peerless, duel_hidden, duel_feint;
    u32 duel_boost_percent, duel_boost_rounds, duel_retreat_chance, duel_polearm_percent, duel_sky_halberd_percent;
    u32 duel_sword_percent, duel_one_blow_chance, duel_one_blow_percent;
    u32 debate_mind, debate_card_damage[4], debate_topic_percent, debate_shout, debate_ignore, debate_anger_hit;
    u32 debate_anger_draw, debate_anger_card, debate_calm_card, debate_rage_rounds, debate_topic_rounds, debate_instant_gap;
    u32 debate_merit, debate_technique, debate_press_chance, debate_skill_card_percent;
    u32 friendliness_start, goodwill_gold, goodwill_base, goodwill_politics_percent, diplomacy_fail_loss;
    u32 alliance_gold, alliance_base, break_loss, break_technique, ceasefire_base, ceasefire_month_cost;
    u32 surrender_base, exchange_base, aid_base, aid_turns, aid_soldiers;
    u32 faction_plot_gold, two_tigers_base, two_tigers_loss, drive_tiger_base, rumour_base, rumour_order_loss, rumour_loyalty_loss;
    u32 skill_search, skill_debater, skill_benevolence, skill_wife_help, skill_guard, skill_fame, skill_fame_percent;
    u32 skill_morale_up, skill_double_attack, skill_heart_attack, skill_unyielding, skill_diamond, skill_iron_wall, skill_fury;
    u32 skill_plot_thrift, skill_heart_attack_percent, seal_technique;
    /* M8: bases, corps, council, emperor, events, AI */
    u32 base_gold, base_troop_cap, base_caps_percent, base_start_troops;
    u32 corps_commands, council_officers, adviser_intellect, advice_per_intellect;
    u32 emperor_technique, emperor_loyalty, emperor_city;
    u32 event_experience, event_merit;
    u32 ai_urgency, ai_sortie_soldiers, ai_sortie_reserve, ai_goodwill_gold, ai_commands, corps_ap_divisor;
    u32 ai_target_hexes, ai_free_city_bonus, ai_defend_bonus, ai_defend_hexes, ai_march_hexes, ai_grain_margin_turns, ai_resupply_score, ai_attack_after_months;   /* M15 */
    /* M9 */
    u32 lifespan_long_years, new_officer_max, new_officer_high, new_officer_normal, new_officer_low;
    u32 train_ap, train_turns, ability_research_ap, ability_research_gold, scenario_public_order;
    u32 loaded_count;           /* rows that set a field */
};

struct sim_content
{
    facility_kind  facilities[CONTENT_FACILITY_CAPACITY];
    u32            facility_count;
    city_kind      cities[SAVE_CITY_COUNT];
    u32            city_count;
    weapon_kind    weapons[CONTENT_WEAPON_CAPACITY];
    u32            weapon_count;
    technique_kind techniques[CONTENT_TECHNIQUE_CAPACITY];
    u32            technique_count;
    disaster_kind  disasters[CONTENT_DISASTER_CAPACITY];
    u32            disaster_count;
    terrain_kind   terrains[CONTENT_TERRAIN_CAPACITY];
    u32            terrain_count;
    armament_kind  armaments[CONTENT_ARMAMENT_CAPACITY];   /* M10: movement_costs.tsv; none for a pack without it */
    u32            armament_count;
    save_type      save_types[CONTENT_SAVE_TYPE_COUNT];   /* M11: save_types.tsv; all SAVE_TYPE_NONE without it */
    b32            has_save_types;
    unit_type_kind unit_types[CONTENT_UNIT_TYPE_CAPACITY];
    u32            unit_type_count;
    tactic_kind    tactics[CONTENT_TACTIC_CAPACITY];
    u32            tactic_count;
    plot_kind      plots[CONTENT_PLOT_CAPACITY];
    u32            plot_count;
    building_kind  buildings[CONTENT_BUILDING_CAPACITY];
    u32            building_count;
    critical_skill criticals[CONTENT_CRITICAL_CAPACITY];
    u32            critical_count;
    skill_kind     skills[CONTENT_SKILL_COUNT];
    u32            skill_count;
    u8             skill_of_file_id[CONTENT_SKILL_COUNT];   /* the number - 1 for each file id; OFFICER_SKILL_NONE if no row */
    post_kind      posts[CONTENT_POST_CAPACITY];
    u32            post_count;
    rank_kind      ranks[CONTENT_RANK_CAPACITY];
    u32            rank_count;
    item_kind      items[CONTENT_ITEM_CAPACITY];
    u32            item_count;
    base_kind      bases[CONTENT_BASE_COUNT];
    u32            base_count;
    event_kind     events[CONTENT_EVENT_CAPACITY];
    u32            event_count;
    ai_priority    priorities[CONTENT_PRIORITY_CAPACITY];
    u32            priority_count;
    chart_row      chart[CONTENT_CHART_CAPACITY];          /* M9 */
    u32            chart_count;
    ancient_officer ancients[CONTENT_ANCIENT_CAPACITY];
    u32            ancient_count;
    history_kind   history[CONTENT_HISTORY_CAPACITY];
    u32            history_count;
    portrait_kind  portraits[CONTENT_PORTRAIT_CAPACITY];
    u32            portrait_count;
    sim_rules      rules;
};

/* Each parser returns false and leaves its part of the content empty if a
   row is bad: an id out of order, a number that does not parse, or a name
   that is not in the enum. The bad row is in *bad_row. */
internal b32 content_load_facilities(sim_content *content, tsv_table *table, u32 *bad_row);
internal b32 content_load_cities(sim_content *content, tsv_table *table, u32 *bad_row);
internal b32 content_load_rules(sim_content *content, tsv_table *table, u32 *bad_row);
internal b32 content_load_weapons(sim_content *content, tsv_table *table, u32 *bad_row);     /* after the techniques */
internal b32 content_load_techniques(sim_content *content, tsv_table *table, u32 *bad_row);
internal b32 content_load_disasters(sim_content *content, tsv_table *table, u32 *bad_row);
internal b32 content_load_terrain(sim_content *content, tsv_table *table, u32 *bad_row);      /* after the techniques */
internal b32 content_load_unit_types(sim_content *content, tsv_table *table, u32 *bad_row);   /* after the weapons */
internal u8  content_find_terrain(sim_content *content, const char *key);                      /* CONTENT_NO_TERRAIN if none */
internal u8  content_find_unit_type(sim_content *content, const char *key);                    /* CONTENT_NO_UNIT_TYPE if none */
internal const char *content_movement_name(movement_class movement);
internal b32 content_load_tactics(sim_content *content, tsv_table *table, u32 *bad_row);     /* after the unit types */
internal b32 content_load_plots(sim_content *content, tsv_table *table, u32 *bad_row);       /* after the unit types and the terrain */
internal b32 content_load_buildings(sim_content *content, tsv_table *table, u32 *bad_row);   /* after the techniques */
internal b32 content_load_criticals(sim_content *content, tsv_table *table, u32 *bad_row);
internal b32 content_load_skills(sim_content *content, tsv_table *table, u32 *bad_row);   /* after the rules: it sets the skill.* rules */
internal u8  content_skill_of_file_id(const sim_content *content, u8 file_id);           /* the skill byte of a record -> the number - 1 */
internal b32 content_load_ranks(sim_content *content, tsv_table *table, u32 *bad_row);
internal b32 content_load_posts(sim_content *content, tsv_table *table, u32 *bad_row);    /* after the ranks */
internal b32 content_load_items(sim_content *content, tsv_table *table, u32 *bad_row);
internal u8  content_find_item_kind(sim_content *content, const u8 *big5_name, u32 size);  /* CONTENT_NO_ITEM_KIND if none */
internal b32 content_load_bases(sim_content *content, tsv_table *table, u32 *bad_row);
internal b32 content_load_events(sim_content *content, tsv_table *table, u32 *bad_row);
internal b32 content_load_priorities(sim_content *content, tsv_table *table, u32 *bad_row);
internal u8  content_find_event(sim_content *content, const char *key);   /* CONTENT_EVENT_CAPACITY if none */
internal b32 content_load_chart(sim_content *content, tsv_table *table, u32 *bad_row);      /* M9 */
internal b32 content_load_ancients(sim_content *content, tsv_table *table, u32 *bad_row);
internal b32 content_load_history(sim_content *content, tsv_table *table, u32 *bad_row);
internal b32 content_load_portraits(sim_content *content, tsv_table *table, u32 *bad_row);
internal b32 content_history_clauses_valid(const char *text, b32 effects, u32 name_count);

internal facility_effect    content_effect_from_name(const char *name);
internal facility_limit     content_limit_from_name(const char *name);
internal city_specialty     content_specialty_from_name(const char *name);
internal const char        *content_effect_name(facility_effect effect);
internal technique_line     content_line_from_name(const char *name);
internal const char        *content_line_name(technique_line line);
internal content_ability    content_ability_from_name(const char *name);
internal technique_effect   content_technique_effect_from_name(const char *name);
internal const char        *content_technique_effect_name(technique_effect effect);
internal content_unit_class content_class_from_name(const char *name);
internal u32                content_month_mask_from_text(const char *text);   /* "1|4|7"; 0 if a part is bad */
internal u8                 content_find_technique(sim_content *content, const char *key);   /* CONTENT_NO_TECHNIQUE if none */

#endif /* CONTENT_H */
