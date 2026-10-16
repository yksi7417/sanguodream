/* Parsers of the content tables. See content.h. */

#include "sim/content.h"

global_variable const char *content_effect_names[FACILITY_EFFECT_COUNT] = {
    "none", "gold", "grain", "mint", "granary", "conscription", "weapons", "horses", "siege", "ships",
    "technique", "action_points", "personnel", "plot", "diplomacy", "military", "drill", "grain_by_soldiers",
};

global_variable const char *content_limit_names[FACILITY_LIMIT_COUNT] = {
    "none", "city", "big_city", "port_city", "territory",
};

global_variable const char *content_specialty_names[CITY_SPECIALTY_COUNT] = {
    "none", "big_city", "pike", "halberd", "crossbow", "horse", "siege", "navy",
};

/* The index of name in names, or count if it is not there. */
internal u32
content_find_name(const char **names, u32 count, const char *name)
{
    for (u32 index = 0; index < count; ++index)
    {
        if (strcmp(names[index], name) == 0) return index;
    }
    return count;
}

internal facility_effect
content_effect_from_name(const char *name)
{
    return (facility_effect)content_find_name(content_effect_names, FACILITY_EFFECT_COUNT, name);
}

internal facility_limit
content_limit_from_name(const char *name)
{
    return (facility_limit)content_find_name(content_limit_names, FACILITY_LIMIT_COUNT, name);
}

internal city_specialty
content_specialty_from_name(const char *name)
{
    return (city_specialty)content_find_name(content_specialty_names, CITY_SPECIALTY_COUNT, name);
}

internal const char *
content_effect_name(facility_effect effect)
{
    return effect < FACILITY_EFFECT_COUNT ? content_effect_names[effect] : "?";
}

/* Parses one row of facilities.tsv. Returns false if a cell is bad. */
internal b32
content_parse_facility(tsv_table *table, u32 row, facility_kind *kind)
{
    u32 id = 0;
    const char *key = tsv_cell(table, row, "key");
    if (!tsv_cell_u32(table, row, "id", &id) || id != row || !key[0] || strlen(key) >= CONTENT_KEY_SIZE) return false;
    if (!tsv_cell_u32(table, row, "cost", &kind->cost) || !tsv_cell_u32(table, row, "durability", &kind->durability) ||
        !tsv_cell_u32(table, row, "min_days", &kind->min_days) || !tsv_cell_u32(table, row, "value", &kind->value))
    {
        return false;
    }
    kind->effect = content_effect_from_name(tsv_cell(table, row, "effect"));
    kind->limit  = content_limit_from_name(tsv_cell(table, row, "limit"));
    if (kind->effect == FACILITY_EFFECT_COUNT || kind->limit == FACILITY_LIMIT_COUNT) return false;
    kind->id = (u8)id;
    memcpy(kind->key, key, strlen(key) + 1);
    kind->period_assumed   = strcmp(tsv_cell(table, row, "period_status"), "assumed") == 0;
    kind->value_assumed    = strcmp(tsv_cell(table, row, "value_status"), "assumed") == 0;
    kind->needs_item       = tsv_cell_yes(table, row, "needs_item");
    kind->adjacency_bonus  = tsv_cell_yes(table, row, "adjacency_bonus");
    kind->new_year_removal = tsv_cell_yes(table, row, "new_year_removal");
    kind->pk               = tsv_cell_yes(table, row, "pk");
    return true;
}

internal b32
content_load_facilities(sim_content *content, tsv_table *table, u32 *bad_row)
{
    content->facility_count = 0;
    if (table->row_count > CONTENT_FACILITY_CAPACITY) { *bad_row = CONTENT_FACILITY_CAPACITY; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        facility_kind kind = {};
        if (!content_parse_facility(table, row, &kind))
        {
            *bad_row = row;
            return false;
        }
        content->facilities[row] = kind;
    }
    content->facility_count = table->row_count;
    return true;
}

internal b32
content_load_cities(sim_content *content, tsv_table *table, u32 *bad_row)
{
    content->city_count = 0;
    if (table->row_count > SAVE_CITY_COUNT) { *bad_row = SAVE_CITY_COUNT; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        u32 id = 0, slots = 0;
        city_specialty specialty = content_specialty_from_name(tsv_cell(table, row, "specialty"));
        if (!tsv_cell_u32(table, row, "id", &id) || id != row || !tsv_cell_u32(table, row, "slots", &slots) ||
            slots > 255 || specialty == CITY_SPECIALTY_COUNT)
        {
            *bad_row = row;
            content->city_count = 0;
            return false;
        }
        content->cities[row].slot_count = (u8)slots;
        content->cities[row].specialty  = specialty;
        content->cities[row].has_port   = tsv_cell_yes(table, row, "port");
    }
    content->city_count = table->row_count;
    return true;
}

struct content_rule_field
{
    const char *key;
    u32         offset;   /* of the field in sim_rules */
};

#define CONTENT_RULE(key, field) { key, (u32)offsetof(sim_rules, field) }
global_variable content_rule_field content_rule_fields[] = {
    CONTENT_RULE("ap.cap",                ap_cap),
    CONTENT_RULE("ap.develop",            ap_develop),
    CONTENT_RULE("develop.officers",      develop_officers),
    CONTENT_RULE("develop.max_days",      develop_max_days),
    CONTENT_RULE("develop.full_politics", develop_full_politics),
    CONTENT_RULE("enemy.block_hexes",     enemy_block_hexes),
    CONTENT_RULE("adjacency.percent",     adjacency_percent),
    CONTENT_RULE("city.gold_cap",         city_gold_cap),
    CONTENT_RULE("city.grain_cap",        city_grain_cap),
    CONTENT_RULE("city.gold_income",      city_gold_income),
    CONTENT_RULE("city.grain_income",     city_grain_income),
    CONTENT_RULE("officer.salary",        officer_salary),
    CONTENT_RULE("grain.soldiers",        grain_soldiers),
};

/* A key that is not in the list is a bad row: a typo must not leave a
   rule at 0 without a sign. */
internal b32
content_load_rules(sim_content *content, tsv_table *table, u32 *bad_row)
{
    sim_rules rules = {};
    for (u32 row = 0; row < table->row_count; ++row)
    {
        const char *key = tsv_cell(table, row, "key");
        u32 value = 0;
        u32 field = 0;
        while (field < ArrayCount(content_rule_fields) && strcmp(content_rule_fields[field].key, key) != 0) field++;
        if (field == ArrayCount(content_rule_fields) || !tsv_cell_u32(table, row, "value", &value))
        {
            *bad_row = row;
            content->rules = {};
            return false;
        }
        *(u32 *)((u8 *)&rules + content_rule_fields[field].offset) = value;
        rules.loaded_count++;
    }
    content->rules = rules;
    return true;
}
