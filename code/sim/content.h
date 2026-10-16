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
};

/* The numbers of rules.tsv. A missing row leaves 0. */
struct sim_rules
{
    u32 ap_cap;
    u32 ap_develop;
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
    u32 loaded_count;           /* rows that set a field */
};

struct sim_content
{
    facility_kind facilities[CONTENT_FACILITY_CAPACITY];
    u32           facility_count;
    city_kind     cities[SAVE_CITY_COUNT];
    u32           city_count;
    sim_rules     rules;
};

/* Each parser returns false and leaves its part of the content empty if a
   row is bad: an id out of order, a number that does not parse, or a name
   that is not in the enum. The bad row is in *bad_row. */
internal b32 content_load_facilities(sim_content *content, tsv_table *table, u32 *bad_row);
internal b32 content_load_cities(sim_content *content, tsv_table *table, u32 *bad_row);
internal b32 content_load_rules(sim_content *content, tsv_table *table, u32 *bad_row);

internal facility_effect content_effect_from_name(const char *name);
internal facility_limit  content_limit_from_name(const char *name);
internal city_specialty  content_specialty_from_name(const char *name);
internal const char     *content_effect_name(facility_effect effect);

#endif /* CONTENT_H */
