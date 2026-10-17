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
    /* M3 economy: the production at the calendar events */
    SIM_EVENT_GOLD_OVER_CAP,        /* subject = city; delta = the gold above the cap (negative) */
    SIM_EVENT_GRAIN_OVER_CAP,       /* subject = city; delta = the grain above the cap (negative) */
    SIM_EVENT_SALARIES_UNPAID,      /* subject = city; detail = the gold that the city could not pay; delta = 0 */
    SIM_EVENT_SOLDIERS_DESERTED,    /* subject = city; delta = soldiers out (negative): the city had no grain; assumed */
    SIM_EVENT_WEAPONS_FREE,         /* subject = city; detail = weapon; delta = the free weapons of a specialty city; assumed */
    SIM_EVENT_FACILITY_REPAIRED,    /* subject = city; detail = slot; delta = durability in; assumed */
    SIM_EVENT_FACILITY_DAMAGED,     /* subject = city; detail = slot; delta = durability out (negative) */
    SIM_EVENT_FACILITY_DESTROYED,   /* subject = city; detail = slot; delta = the facility id */
    SIM_EVENT_TECHNIQUE_GAINED,     /* faction; delta = technique points in; assumed */
    SIM_EVENT_MERIT_GAINED,         /* subject = officer; delta = merit in */
    /* M3 economy: the city commands */
    SIM_EVENT_PATROLLED,            /* subject = city; delta = public order in; assumed */
    SIM_EVENT_CONSCRIPTED,          /* subject = city; detail = slot of the barracks; delta = soldiers in; assumed */
    SIM_EVENT_ORDER_LOST,           /* subject = city; delta = public order out (negative); assumed */
    SIM_EVENT_MORALE_LOST,          /* subject = city; delta = morale out (negative); assumed */
    SIM_EVENT_TRAINED,              /* subject = city; delta = morale in; assumed */
    SIM_EVENT_GRAIN_BOUGHT,         /* subject = city; delta = grain in; assumed price */
    SIM_EVENT_GRAIN_SOLD,           /* subject = city; delta = grain out (negative) */
    SIM_EVENT_GOLD_EARNED,          /* subject = city; delta = gold in from a sale; assumed price */
    SIM_EVENT_PRODUCTION_STARTED,   /* subject = city; detail = weapon; delta = the period in turns */
    SIM_EVENT_WEAPONS_PRODUCED,     /* subject = city; detail = weapon; delta = weapons in; assumed quantity */
    /* M4: research, merge, disasters, the end of the game */
    SIM_EVENT_TECHNIQUE_SPENT,      /* faction; detail = technique; delta = technique points out (negative) */
    SIM_EVENT_RESEARCH_STARTED,     /* subject = city; faction; detail = technique; delta = the period in turns; assumed period */
    SIM_EVENT_TECHNIQUE_RESEARCHED, /* subject = city; faction; detail = technique; the faction has the technique */
    SIM_EVENT_MERGE_STARTED,        /* subject = city; detail = the absorber slot; delta = the period in turns; assumed period */
    SIM_EVENT_FACILITY_MERGED,      /* subject = city; detail = the absorber slot; delta = the new level; the absorbed slot is free */
    SIM_EVENT_DISASTER_STARTED,     /* subject = city; detail = disaster; delta = turns (0: no length); assumed chance */
    SIM_EVENT_DISASTER_ENDED,       /* subject = city; detail = disaster */
    SIM_EVENT_GRAIN_LOST,           /* subject = city; detail = disaster; delta = grain out (negative); assumed */
    SIM_EVENT_GRAIN_HARVEST,        /* subject = city; detail = disaster; delta = grain in from a good harvest; assumed */
    SIM_EVENT_SOLDIERS_LOST,        /* subject = city; detail = disaster; delta = soldiers out (negative); assumed */
    SIM_EVENT_GAME_WON,             /* faction; detail = sim_outcome_kind */
    SIM_EVENT_GAME_LOST,            /* faction; detail = sim_outcome_kind */
    /* M5: units. A unit event has detail = the unit id and subject = the city of the event, or 0. */
    SIM_EVENT_UNIT_SORTIED,         /* subject = city; detail = unit; delta = soldiers out of the city (negative) */
    SIM_EVENT_UNIT_SUPPLIES,        /* subject = city; detail = unit; delta = gold out (negative); the grain is in UNIT_GRAIN_TAKEN */
    SIM_EVENT_UNIT_GRAIN_TAKEN,     /* subject = city; detail = unit; delta = grain out (negative) */
    SIM_EVENT_UNIT_WEAPONS_TAKEN,   /* subject = city; detail = unit; delta = weapons out (negative) */
    SIM_EVENT_UNIT_MOVED,           /* detail = unit; delta = the movement points spent */
    SIM_EVENT_UNIT_SOLDIERS_LOST,   /* detail = unit; delta = soldiers out (negative): plank road, poison spring; assumed */
    SIM_EVENT_UNIT_WAITED,          /* detail = unit */
    SIM_EVENT_UNIT_ENTERED_CITY,    /* subject = city; detail = unit; delta = soldiers into the city */
    SIM_EVENT_UNIT_GOLD_RETURNED,   /* subject = city; detail = unit; delta = gold into the city */
    SIM_EVENT_UNIT_GRAIN_RETURNED,  /* subject = city; detail = unit; delta = grain into the city */
    SIM_EVENT_UNIT_RETREATED,       /* subject = the home city; detail = unit */
    SIM_EVENT_UNIT_SUPPLIED,        /* detail = the unit that receives; delta = soldiers; the gold and grain move too */
    SIM_EVENT_UNIT_GRAIN_EATEN,     /* detail = unit; delta = grain out (negative); assumed */
    SIM_EVENT_UNIT_DESERTED,        /* detail = unit; delta = soldiers out (negative): the unit had no grain; assumed */
    /* M6: combat. A unit event has detail = the unit that changes. */
    SIM_EVENT_UNIT_DAMAGED,         /* detail = unit; delta = soldiers out (negative); assumed formula */
    SIM_EVENT_COUNTERATTACK,        /* detail = the unit that counterattacks */
    SIM_EVENT_CRITICAL_HIT,         /* detail = the attacking unit */
    SIM_EVENT_TACTIC_USED,          /* detail = unit; delta = 1 success, 0 failure */
    SIM_EVENT_PLOT_USED,            /* detail = unit; delta = 1 success, 0 failure */
    SIM_EVENT_MORALE_CHANGED,       /* detail = unit; delta = morale in or out */
    SIM_EVENT_UNIT_PUSHED,          /* detail = unit; delta = hexes */
    SIM_EVENT_UNIT_STATE,           /* detail = unit; delta = the new sim_unit_state (0 = it ends) */
    SIM_EVENT_UNIT_DESTROYED,       /* detail = unit */
    SIM_EVENT_OFFICER_CAPTURED,     /* subject = officer; faction = the captor */
    SIM_EVENT_OFFICER_ESCAPED,      /* subject = officer */
    SIM_EVENT_FIRE_STARTED,         /* detail = the cell; delta = turns */
    SIM_EVENT_FIRE_ENDED,           /* detail = the cell */
    SIM_EVENT_BUILDING_STARTED,     /* detail = the building id; delta = the kind */
    SIM_EVENT_BUILDING_COMPLETED,   /* detail = the building id */
    SIM_EVENT_BUILDING_DAMAGED,     /* detail = the building id; delta = durability out (negative) */
    SIM_EVENT_BUILDING_DESTROYED,   /* detail = the building id */
    SIM_EVENT_TRAP_TRIGGERED,       /* detail = the building id */
    SIM_EVENT_BASE_DAMAGED,         /* subject = city; delta = durability out (negative) */
    SIM_EVENT_BASE_SOLDIERS_LOST,   /* subject = city; delta = soldiers out (negative) */
    SIM_EVENT_BASE_RECOVERED,       /* subject = city; delta = durability in; assumed */
    SIM_EVENT_BASE_CAPTURED,        /* subject = city; faction = the new owner; detail = the old owner */
    SIM_EVENT_UNIT_LOOT,            /* detail = unit; delta = the amount; the loot of a destroyed facility */
    SIM_EVENT_COUNT
};

/* The quantity that an event changes. The ledger filter of the UI uses it
   (sim_event_resource). */
enum sim_resource
{
    SIM_RESOURCE_NONE,
    SIM_RESOURCE_GOLD,
    SIM_RESOURCE_GRAIN,
    SIM_RESOURCE_SOLDIERS,
    SIM_RESOURCE_MORALE,
    SIM_RESOURCE_PUBLIC_ORDER,
    SIM_RESOURCE_WEAPONS,
    SIM_RESOURCE_DURABILITY,
    SIM_RESOURCE_TECHNIQUE,
    SIM_RESOURCE_ACTION_POINTS,
    SIM_RESOURCE_MERIT,
    SIM_RESOURCE_COUNT
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
    u32            detail;    /* a slot, a weapon or an amount; see the event kind */
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
    SIM_COMMAND_PATROL,          /* 巡察 */
    SIM_COMMAND_CONSCRIPT,       /* 徵兵 */
    SIM_COMMAND_TRAIN,           /* 訓練 */
    SIM_COMMAND_MERCHANT,        /* 商人 */
    SIM_COMMAND_PRODUCE,         /* 生產兵裝 */
    SIM_COMMAND_DAMAGE_FACILITY, /* a test and editor command until an attacker exists (M6) */
    SIM_COMMAND_RESEARCH,        /* 研究技巧 */
    SIM_COMMAND_MERGE,           /* 吸收合併 (PK) */
    SIM_COMMAND_START_DISASTER,  /* a test and editor command: the calendar starts disasters at random */
    SIM_COMMAND_SORTIE,          /* 出征 */
    SIM_COMMAND_TRANSPORT,       /* 輸送 */
    SIM_COMMAND_MOVE,            /* 移動: a unit moves */
    SIM_COMMAND_WAIT,            /* 待命 */
    SIM_COMMAND_RETREAT,         /* 退卻 */
    SIM_COMMAND_SUPPLY,          /* 補給: a transport unit gives to an adjacent unit */
    SIM_COMMAND_ATTACK,          /* 攻擊 */
    SIM_COMMAND_JOINT_ATTACK,    /* 齊攻 */
    SIM_COMMAND_TACTIC,          /* 戰法 */
    SIM_COMMAND_PLOT,            /* 計略 */
    SIM_COMMAND_BUILD,           /* 設置 */
    SIM_COMMAND_REPAIR,          /* 修補 */
    SIM_COMMAND_DISMANTLE,       /* 撤除 */
    SIM_COMMAND_COUNT
};

#define SIM_DEVELOP_OFFICERS 3
#define SIM_COMMAND_OFFICERS SIM_DEVELOP_OFFICERS

enum sim_merchant_mode
{
    SIM_MERCHANT_BUY,            /* spend gold, get grain */
    SIM_MERCHANT_SELL,           /* sell grain, get gold */
    SIM_MERCHANT_MODE_COUNT
};

/* The order of patrol, conscript, train, merchant and produce. */
struct sim_city_order
{
    u8  city_id;
    u8  weapon;                             /* produce: the row of weapons.tsv */
    u8  mode;                               /* merchant: sim_merchant_mode */
    u32 amount;                             /* merchant: the gold to spend or the grain to sell */
    u16 officers[SIM_COMMAND_OFFICERS];     /* SIM_NO_OFFICER for an empty place */
};

struct sim_damage_order
{
    u8  city_id;
    u8  slot;
    u32 amount;
};

struct sim_develop_order
{
    u8  city_id;
    u8  facility;
    u8  slot;                               /* SIM_ANY_SLOT: the first free slot */
    u16 officers[SIM_DEVELOP_OFFICERS];     /* SIM_NO_OFFICER for an empty place */
};

/* 研究技巧: up to three officers research a technique for the faction. */
struct sim_research_order
{
    u8  city_id;
    u8  technique;                          /* the row of techniques.tsv */
    u16 officers[SIM_COMMAND_OFFICERS];
};

/* 吸收合併: one officer merges the absorbed facility into the absorber. */
struct sim_merge_order
{
    u8  city_id;
    u8  absorber;                           /* the slot that keeps the facility and gains the level */
    u8  absorbed;                           /* the slot that becomes free; level 1 */
    u16 officer;
};

struct sim_disaster_order
{
    u8 city_id;
    u8 disaster;                            /* the row of disasters.tsv */
};

/* 出征 and 輸送: officers leave a city with soldiers, gold and grain. */
struct sim_sortie_order
{
    u8  city_id;
    u8  unit_type;                          /* the row of unit_types.tsv; the transport row for 輸送 */
    u16 officers[SIM_COMMAND_OFFICERS];     /* [0] is the commander */
    u32 soldiers;
    u32 gold;
    u32 grain;
    u8  destination;                        /* 輸送: the own city of the cargo */
    u32 cargo[CITY_EQUIPMENT_STOCK_COUNT];  /* 輸送: the weapons */
};

/* The orders of a unit: move, wait, retreat, supply. */
struct sim_unit_order
{
    u16       unit;
    hex_coord to;                           /* move */
    u16       target;                       /* supply: the unit that receives */
    u32       soldiers;                     /* supply */
    u32       gold;
    u32       grain;
    u8        kind;                         /* tactic, plot: the row of its table; build: the row of buildings.tsv */
};

struct sim_command
{
    sim_command_kind   kind;
    u32                id;        /* assigned by sim_make_command */
    sim_develop_order  develop;   /* SIM_COMMAND_DEVELOP */
    sim_city_order     city;      /* SIM_COMMAND_PATROL .. SIM_COMMAND_PRODUCE */
    sim_damage_order   damage;    /* SIM_COMMAND_DAMAGE_FACILITY */
    sim_research_order research;  /* SIM_COMMAND_RESEARCH */
    sim_merge_order    merge;     /* SIM_COMMAND_MERGE */
    sim_disaster_order disaster;  /* SIM_COMMAND_START_DISASTER */
    sim_sortie_order   sortie;    /* SIM_COMMAND_SORTIE, SIM_COMMAND_TRANSPORT */
    sim_unit_order     unit;      /* SIM_COMMAND_MOVE .. SIM_COMMAND_SUPPLY */
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
    SIM_REFUSAL_LIMIT_REACHED,         /* the command ran its limit of this turn (once, or once per facility) */
    SIM_REFUSAL_TOO_MANY_OFFICERS,     /* the command takes fewer officers (merchant: 1) */
    SIM_REFUSAL_NO_AMOUNT,             /* the merchant order has no amount */
    SIM_REFUSAL_NO_GRAIN,              /* the city has less grain than the order sells */
    SIM_REFUSAL_NEEDS_TECHNIQUE,       /* the faction lacks the earlier level, or the technique of the weapon */
    SIM_REFUSAL_NO_TECHNIQUE_POINTS,   /* the faction has fewer technique points than the research costs */
    SIM_REFUSAL_WEAPON_REPLACED,       /* the faction has the strong weapon: the weak one cannot be produced */
    SIM_REFUSAL_NOT_MERGEABLE,         /* the two slots are not two adjacent facilities of one kind that can merge */
    SIM_REFUSAL_GAME_OVER,             /* the game ended (victory or defeat) */
    SIM_REFUSAL_NO_DISASTER,           /* no such disaster, or it runs in the city */
    SIM_REFUSAL_NO_TECHNIQUE,          /* no such technique */
    SIM_REFUSAL_NO_UNIT,               /* no such unit, or the faction that acts does not own it */
    SIM_REFUSAL_UNIT_ACTED,            /* the unit moved or acted in this turn */
    SIM_REFUSAL_UNREACHABLE,           /* the hex is not in the move range of the unit */
    SIM_REFUSAL_NO_ROOM,               /* no free hex next to the city for the new unit */
    SIM_REFUSAL_NO_SOLDIERS,           /* no soldiers, or more than the city has or the commander can lead */
    SIM_REFUSAL_NO_WEAPONS,            /* the city has fewer weapons than the unit needs */
    SIM_REFUSAL_NOT_ADJACENT,          /* supply: the target is not a unit of the faction next to the transport unit */
    SIM_REFUSAL_UNIT_CONFUSED,         /* the unit is confused or under a false report */
    SIM_REFUSAL_NO_TARGET,             /* no valid target at the hex, or out of range */
    SIM_REFUSAL_NO_MORALE,             /* the unit has less morale than the tactic or the plot costs */
    SIM_REFUSAL_NO_APTITUDE,           /* the class, the aptitude or the skill does not allow it */
    SIM_REFUSAL_NO_PARTNER,            /* a joint attack needs two units or more */
    SIM_REFUSAL_BAD_TERRAIN,           /* the terrain of the unit or of the hex does not allow it */
    SIM_REFUSAL_NOT_BUILDABLE,         /* the building cannot stand on the hex */
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
    u32         value_before;   /* the value that the command changes: public order, soldiers, morale, grain or stock */
    u32         value_after;
    u32         quantity;       /* the amount that the command adds or moves */
    u32         gold_gain;      /* merchant sale: the gold that the city gets */
    b32         assumed;        /* the effect comes from an assumed rule */
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
    /* the state of the city commands (M3) */
    u8  morale;                          /* 氣力; the save field is not decoded: city.start_morale (assumed) */
    u32 patrolled_turn;                  /* sim turn + 1 of the last patrol; 0 = never */
    u32 trained_turn;                    /* sim turn + 1 of the last train */
    /* M4: the disasters that run in the city: turns left, 0 = none */
    u16 disaster_turns[CONTENT_DISASTER_CAPACITY];
    /* M6: the durability of the base (siege-and-bases.md); the maximum is durability_maximum */
    u32 durability;
};

#define SIM_STATUS_CAPTIVE 5   /* the status of a captive (assumed: status 5 of save-file.md, Q-176) */

#define SIM_DISASTER_OPEN 0xFFFF         /* a disaster without a length: it ends by a rule (bandits) */
#define SIM_JOB_RESEARCH  0xFE           /* officer_component.job_slot of an officer that researches */

/* ---- facility slots (cities-and-facilities.md) -------------------------- */

#define SIM_SLOT_CAPACITY 24     /* the largest city of the table has 22 slots */

enum sim_slot_state
{
    SIM_SLOT_EMPTY,
    SIM_SLOT_BUILDING,
    SIM_SLOT_BUILT,
    SIM_SLOT_MERGING,       /* the absorbed side of a merge: it gives nothing; the slot is free when the merge completes */
    SIM_SLOT_STATE_COUNT
};

struct sim_slot
{
    hex_coord hex;                                /* placeholder layout: see sim_city_slot_hex */
    u8        state;                              /* sim_slot_state */
    u8        facility;                           /* CONTENT_NO_FACILITY when empty */
    u8        level;                              /* 1; the PK merge raises it (M4); the editor sets it */
    u16       turns_left;                         /* while building, or while a production job runs */
    u16       officers[SIM_DEVELOP_OFFICERS];     /* while building, or while a production job runs */
    u32       durability;                         /* full when the develop starts (assumed) */
    u32       used_turn;                          /* sim turn + 1 of the last conscription or production */
    u8        job_weapon;                         /* the weapon of a production job; CONTENT_NO_WEAPON if none */
    u8        merge_slot;                         /* the absorber of a merge: the absorbed slot; SIM_ANY_SLOT if none */
};

struct sim_city_site
{
    hex_coord hex;                                /* the city on the placeholder map */
    u8        slot_count;
    sim_slot  slots[SIM_SLOT_CAPACITY];
};

/* A unit on the map (military-units-and-tactics.md). The id of a unit is
   its place in sim_state.units: it stays the same while the unit lives.
   An officer in a unit has the position SIM_UNIT_POSITION_BASE + the id
   (save-file.md, officer +100). A unit from place_unit (a test and editor
   call) has no type and no officers: it only blocks and exerts its zone of
   control. */
#define SIM_UNIT_CAPACITY      256
#define SIM_NO_UNIT            0xFFFF
#define SIM_UNIT_POSITION_BASE 87

struct sim_unit
{
    b32       alive;
    u8        force_id;
    hex_coord hex;
    u8        unit_type;                          /* CONTENT_NO_UNIT_TYPE for a placed test unit */
    u16       officers[SIM_COMMAND_OFFICERS];     /* [0] is the commander (主將), then the deputies (副將) */
    u32       soldiers;
    u32       gold;
    u32       grain;
    u32       morale;
    u32       weapons;                            /* the weapons of its class that it carries: one for each soldier, or 1 */
    u32       cargo[CITY_EQUIPMENT_STOCK_COUNT];  /* a transport unit: the weapons that it carries */
    u8        home_city;                          /* the base of departure */
    u8        destination;                        /* a transport unit: its city; SIM_NO_CITY */
    u32       moved_turn;                         /* sim turn + 1 of the last move; 0 = never */
    u32       acted_turn;                         /* sim turn + 1 of the last action (wait, supply) */
    u8        state;                              /* sim_unit_state (plots-and-states.md) */
    u8        state_turns;                        /* turns left of the state */
};

enum sim_unit_state
{
    UNIT_STATE_NONE,
    UNIT_STATE_CONFUSED,        /* 混亂: the unit cannot act */
    UNIT_STATE_FALSE_REPORT,    /* 偽報: the unit marches back to its home city by itself */
    UNIT_STATE_COUNT
};

/* A building on the map (military-facilities-and-traps.md): a military
   facility, an obstacle, a trap, or a natural trap of the map. */
#define SIM_BUILDING_CAPACITY 256
#define SIM_NO_BUILDING       0xFFFF
struct sim_building
{
    b32       alive;
    u8        kind;              /* the row of buildings.tsv */
    u8        force_id;          /* SIM_NO_FORCE for a trap or a natural trap: any faction starts it */
    hex_coord hex;
    u32       durability;
    b32       complete;          /* it works when its durability is full (manual p.16) */
    u8        city_id;           /* a dike: the city that it floods; SIM_NO_CITY */
};

/* The input of the combat_damage rule hook (docs/ROADMAP.md, "Rule hooks"). */
struct sim_combat_input
{
    u32 soldiers;                /* of the attacker */
    u32 attack;
    u32 defence;
    u32 percent;                 /* the product of the multipliers: tactic, triangle, critical, buildings */
};

struct sim_state;
typedef u32 sim_combat_damage_fn(sim_state *sim, sim_combat_input *input);

/* The rule hooks of a content pack. Each pointer defaults to the generic
   rule; a pack can set its own in code/sim/rules_<pack>.cpp. */
struct sim_ruleset
{
    sim_combat_damage_fn *combat_damage;
};

/* The five values of a unit (military-units-and-tactics.md, "Unit values"). */
struct sim_unit_values
{
    u32 attack;
    u32 defence;
    u32 intellect;
    u32 build;
    u32 move;
    u32 aptitude;      /* the best aptitude of the officers for the class: 0 C .. 3 S; 255 none */
    b32 ignores_zoc;   /* 飛將 or 遁走 on land, not for a siege weapon */
};

/* The hexes that a unit can reach in this turn, with the cost of each. */
struct sim_reach
{
    hex_coord *cells;
    u32       *costs;
    u32       *from;     /* the index in cells of the hex before; U32_MAX for the start */
    u32        count;
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

/* ---- M4: research, settings and the end of the game --------------------- */

/* The research of a faction (one at a time; assumed, Q-024). */
struct sim_research_job
{
    u8  technique;                          /* CONTENT_NO_TECHNIQUE when no research runs */
    u8  city_id;
    u16 turns_left;
    u16 officers[SIM_COMMAND_OFFICERS];
};

/* The game settings at the start (game-setup-and-modes.md; F-132 adds the rest in M9). */
struct sim_settings
{
    b32 disasters;          /* the calendar starts disasters; a new game has them */
};

enum sim_outcome_kind
{
    SIM_OUTCOME_NONE,
    SIM_OUTCOME_VICTORY,            /* the faction owns every city that has an owner */
    SIM_OUTCOME_DEFEAT_TIME,        /* the year became game.end_year (351) */
    SIM_OUTCOME_DEFEAT_NO_CITY,
    SIM_OUTCOME_DEFEAT_NO_OFFICER,
    SIM_OUTCOME_DEFEAT_SURRENDER,   /* the ruler accepted a surrender demand (M7 diplomacy) */
    SIM_OUTCOME_COUNT
};

/* The end of the game for the human factions (calendar-and-turn.md,
   "Victory and defeat"). The game is over when a human faction wins or
   when every human faction lost. */
struct sim_outcome
{
    sim_outcome_kind kind;          /* SIM_OUTCOME_NONE while the game runs */
    u8               force_id;      /* the faction of the outcome */
    b32              lost[SIM_FORCE_CAPACITY];
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
    u32            unit_count;      /* the places of units[] in use, alive or dead */
    memory_arena   scratch;         /* the working memory of a search; reset before each use */
    sim_building   buildings[SIM_BUILDING_CAPACITY];
    u32            building_count;  /* the places of buildings[] in use */
    u8            *fire;            /* the turns of fire left on each cell of the map; 0 = no fire */
    sim_ruleset    ruleset;
    u32            technique_points[SIM_FORCE_CAPACITY];   /* 技巧P of each faction; the save field is not decoded */
    u64            techniques[SIM_FORCE_CAPACITY];         /* bit t: the faction has technique t (CONTENT_TECHNIQUE_CAPACITY = 64) */
    sim_research_job research[SIM_FORCE_CAPACITY];
    sim_settings   settings;
    sim_outcome    outcome;
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
internal u16                sim_force_ruler(sim_state *sim, u8 force_id);   /* SIM_NO_OFFICER if none */

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
internal b32       sim_enemy_near(sim_state *sim, city_component *city);
internal sim_check sim_refuse(sim_check check, sim_refusal refusal);
internal void      sim_release_slot_officers(sim_state *sim, sim_slot *slot, u32 cause, sim_event_sink *events);

/* economy.cpp: the production at the calendar events, durability, technique
   points and merit (cities-and-facilities.md, calendar-and-turn.md) */
struct sim_income_forecast
{
    u32 gold_base;              /* the initial income of the city (assumed, Q-021) */
    u32 gold_facilities;        /* the sum of the facility outputs */
    u32 order_percent;          /* public order: high order raises the income (assumed rule) */
    u32 gold_skill_percent;     /* 富豪 */
    u32 gold_month;             /* the gold of one month */
    u32 gold_payment;           /* the gold of one payment */
    b32 gold_every_turn;        /* 徵稅: a payment each turn, each halved */
    u32 grain_base;
    u32 grain_facilities;
    u32 grain_skill_percent;    /* 米道 */
    u32 grain_season;           /* the grain of one season */
    u32 grain_payment;          /* the grain of one payment */
    b32 grain_every_month;      /* 徵收: a payment each month, each halved */
    u32 salaries;               /* the gold of the salaries each month */
    u32 grain_per_turn;         /* the grain that the soldiers eat each turn */
};

internal sim_resource        sim_event_resource(sim_event_kind kind);
internal const char         *sim_resource_name(sim_resource resource);
internal u32                 sim_percent_of(u32 amount, u32 percent);
internal u32                 sim_level_percent(u32 level, sim_rules *rules);
internal u32                 sim_facility_output(u32 value, u32 level, b32 adjacent_bonus, sim_rules *rules);
internal u32                 sim_order_percent(u32 public_order, sim_rules *rules);
internal b32                 sim_city_has_skill(sim_state *sim, u8 city_id, u32 skill);
internal sim_income_forecast sim_city_forecast(sim_state *sim, u8 city_id);
internal u32                 sim_city_salaries(sim_state *sim, city_component *city);
internal u32                 sim_city_soldier_cap(sim_state *sim, u8 city_id);
internal void                sim_reward_command(sim_state *sim, u8 force, const u16 *officers, b32 technique,
                                                u32 cause, sim_event_sink *events);
internal void                sim_emit_city(sim_state *sim, sim_event_kind kind, city_component *city, s32 delta, u32 detail,
                                           b32 assumed, u32 cause, sim_event_sink *events);
internal sim_refusal         sim_check_damage(sim_state *sim, sim_damage_order *order);
internal void                sim_run_production_month(sim_state *sim, u32 cause, sim_event_sink *events);
internal void                sim_run_production_season(sim_state *sim, u32 cause, sim_event_sink *events);
internal void                sim_run_production_turn(sim_state *sim, u32 cause, sim_event_sink *events);
internal void                sim_run_durability(sim_state *sim, u32 cause, sim_event_sink *events);
internal void                sim_damage_facility(sim_state *sim, sim_command command, sim_event_sink *events);

/* city_commands.cpp: patrol, conscript, train, merchant and produce
   (city-commands.md, weapons-and-production.md) */
internal u32       sim_patrol_rise(u32 leadership_sum, b32 enemy_near, sim_rules *rules);
internal u32       sim_conscript_soldiers(u32 charm_sum, u32 level, b32 enemy_near, sim_rules *rules);
internal u32       sim_train_rise(u32 force_sum, b32 drill_ground, sim_rules *rules);
internal u32       sim_merchant_percent(u32 politics, sim_rules *rules);
internal u32       sim_merchant_grain_for_gold(u32 gold, u32 politics, sim_rules *rules);
internal u32       sim_merchant_gold_for_grain(u32 grain, u32 politics, sim_rules *rules);
internal u32       sim_produce_quantity(u32 intellect_sum, u32 level, b32 skill, sim_rules *rules);
internal u32       sim_produce_days(u32 intellect_sum, u32 min_days, u32 max_days, b32 skill, sim_rules *rules);
internal u32       sim_weapon_cost(u32 cost, b32 specialty, sim_rules *rules);
internal sim_check sim_check_city_command(sim_state *sim, sim_command_kind kind, sim_city_order *order);
internal void      sim_city_command(sim_state *sim, sim_command command, sim_event_sink *events);

/* research.cpp: techniques, the research command and the effects of the
   techniques (research-techniques.md) */
internal b32       sim_has_technique(sim_state *sim, u8 force, u8 technique);
internal b32       sim_has_technique_effect(sim_state *sim, u8 force, technique_effect effect, content_unit_class unit_class);
internal u32       sim_technique_effect_value(sim_state *sim, u8 force, technique_effect effect);   /* 0 if the faction lacks it */
internal u8        sim_previous_technique(sim_content *content, u8 technique);   /* CONTENT_NO_TECHNIQUE for level 1 */
internal u32       sim_research_days(u32 ability_sum, u32 min_days, u32 max_days, b32 personnel_office, sim_rules *rules);
internal u32       sim_research_gold(u32 gold, b32 teaching, sim_rules *rules);
internal sim_check sim_check_research(sim_state *sim, sim_research_order *order);
internal void      sim_research(sim_state *sim, sim_command command, sim_event_sink *events);
internal void      sim_progress_research(sim_state *sim, u32 cause, sim_event_sink *events);
internal void      sim_grant_technique(sim_state *sim, u8 force, u8 technique);   /* setup: a scenario can start with techniques */
internal u32       sim_round_to_turns(u32 days);                                  /* up to whole turns, at least one */
internal u32       sim_city_morale_max(sim_state *sim, city_component *city);     /* morale.max, plus 熟練兵 */
internal u32       sim_order_loss(sim_state *sim, u8 force, u32 loss);            /* 整備政令 lowers the loss */
internal sim_refusal sim_check_weapon_technique(sim_state *sim, u8 force, weapon_kind *weapon);
internal b32       sim_city_has_built(sim_state *sim, u8 city_id, facility_effect effect);
internal u32       sim_count_built(sim_state *sim, u8 city_id, facility_effect effect);

/* merge.cpp: 吸收合併 (cities-and-facilities.md, "Merge") */
internal u32       sim_merge_days(u32 politics, sim_rules *rules);
internal sim_check sim_check_merge(sim_state *sim, sim_merge_order *order);
internal void      sim_merge(sim_state *sim, sim_command command, sim_event_sink *events);
internal void      sim_complete_merge(sim_state *sim, city_component *city, sim_slot *slot, u32 slot_index, u32 cause,
                                      sim_event_sink *events);

/* disasters.cpp: locusts, plague, bandits and the good harvest
   (calendar-and-turn.md, "Disasters") */
internal b32         sim_disaster_month(disaster_kind *kind, u32 month);
internal u32         sim_disaster_chance(disaster_kind *kind, b32 has_chance_skill);   /* percent */
internal void        sim_damage_slot(sim_state *sim, city_component *city, u32 slot_index, u32 amount, u32 cause,
                                     sim_event_sink *events);   /* economy.cpp: damage, and destroy at 0 */
internal b32         sim_disaster_active(city_component *city, u32 disaster);
internal sim_refusal sim_check_start_disaster(sim_state *sim, sim_disaster_order *order);
internal void        sim_start_disaster(sim_state *sim, city_component *city, u32 disaster, u32 cause, sim_event_sink *events);
internal void        sim_start_disaster_command(sim_state *sim, sim_command command, sim_event_sink *events);
internal void        sim_roll_disasters(sim_state *sim, u32 month, u32 cause, sim_event_sink *events);
internal void        sim_disaster_grain(sim_state *sim, city_component *city, u32 payment, u32 cause, sim_event_sink *events);
internal void        sim_run_disasters_turn(sim_state *sim, u32 cause, sim_event_sink *events);

/* units.cpp: units, the move range, the zone of control, sortie,
   transport, move, wait, retreat and supply (military-units-and-tactics.md,
   terrain-and-map.md) */
internal b32              sim_officer_has_skill(officer_component *officer, u32 skill);   /* economy.cpp; skills.md numbers */
internal sim_unit        *sim_find_unit(sim_state *sim, u32 unit_id);
internal u32              sim_unit_at(sim_state *sim, hex_coord hex);          /* SIM_NO_UNIT if none */
internal u8               sim_city_at(sim_state *sim, hex_coord hex);          /* SIM_NO_CITY if none */
internal sim_unit_values  sim_unit_values_of(sim_state *sim, u8 force, u8 unit_type, const u16 *officers);
internal u32              sim_unit_soldier_cap(sim_state *sim, u8 force);
internal terrain_kind    *sim_terrain_at(sim_state *sim, hex_coord hex);
internal u32              sim_entry_cost(sim_state *sim, u8 force, movement_class movement, hex_coord hex, u32 self);
internal b32              sim_in_enemy_zone(sim_state *sim, u8 force, hex_coord hex, u32 self);
internal sim_reach        sim_unit_reach(sim_state *sim, u32 unit_id, memory_arena *scratch);
internal u32              sim_reach_find(sim_reach *reach, hex_coord hex);
internal sim_check        sim_check_sortie(sim_state *sim, sim_sortie_order *order, b32 transport);
internal void             sim_sortie(sim_state *sim, sim_command command, sim_event_sink *events);
internal sim_check        sim_check_unit_order(sim_state *sim, sim_command_kind kind, sim_unit_order *order, memory_arena *scratch);
internal void             sim_move_unit(sim_state *sim, sim_command command, memory_arena *scratch, sim_event_sink *events);
internal void             sim_unit_action(sim_state *sim, sim_command command, memory_arena *scratch, sim_event_sink *events);
internal void             sim_run_units_turn(sim_state *sim, u32 cause, sim_event_sink *events);
internal sim_sortie_order sim_sortie_proposal(sim_state *sim, u8 city_id, u8 unit_type);
internal u32              sim_military_ap(sim_state *sim, city_component *city, u32 action_points);   /* city_commands.cpp */

/* combat.cpp and field.cpp: combat, plots, fire, buildings, siege (M6) */
internal u32       sim_rule_combat_damage(sim_state *sim, sim_combat_input *input);
internal u32       sim_building_at(sim_state *sim, hex_coord hex);   /* SIM_NO_BUILDING if none */
internal b32       sim_hex_is_water(sim_state *sim, hex_coord hex);
internal b32       sim_hex_burns(sim_state *sim, hex_coord hex);
internal u32       sim_city_durability_max(sim_state *sim, city_component *city);
internal sim_check sim_check_attack(sim_state *sim, sim_unit_order *order);
internal sim_check sim_check_joint_attack(sim_state *sim, sim_unit_order *order);
internal sim_check sim_check_tactic(sim_state *sim, sim_unit_order *order);
internal sim_check sim_check_plot(sim_state *sim, sim_unit_order *order);
internal sim_check sim_check_build(sim_state *sim, sim_command_kind kind, sim_unit_order *order);
internal void      sim_attack(sim_state *sim, sim_command command, sim_event_sink *events);
internal void      sim_joint_attack(sim_state *sim, sim_command command, sim_event_sink *events);
internal void      sim_use_tactic(sim_state *sim, sim_command command, sim_event_sink *events);
internal void      sim_use_plot(sim_state *sim, sim_command command, sim_event_sink *events);
internal void      sim_build(sim_state *sim, sim_command command, sim_event_sink *events);
internal u32       sim_add_building(sim_state *sim, u8 kind, u8 force, hex_coord hex, u32 durability, u8 city_id);
internal void      sim_run_field_turn(sim_state *sim, u32 cause, sim_event_sink *events);
internal b32       sim_building_available(sim_state *sim, u8 force, u32 kind_id);
internal b32       sim_can_place(sim_state *sim, building_kind *kind, hex_coord hex);

/* outcome.cpp: victory and defeat (calendar-and-turn.md) */
internal void sim_check_outcome(sim_state *sim, u32 cause, sim_event_sink *events);
internal b32  sim_game_over(sim_state *sim);
internal const char *sim_outcome_name(sim_outcome_kind kind);

#endif /* SIM_H */
