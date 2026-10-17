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

global_variable const char *content_line_names[TECHNIQUE_LINE_COUNT] = {
    "pike", "halberd", "crossbow", "cavalry", "drill", "invention", "defence", "fire", "pk",
};

global_variable const char *content_ability_names[CONTENT_ABILITY_COUNT] = {
    "leadership", "force", "intellect", "politics", "charm",
};

global_variable const char *content_technique_effect_names[TECHNIQUE_EFFECT_COUNT] = {
    "none", "tactic_damage", "raid_grain", "forest_no_counter", "elite", "arrow_block", "attack_block",
    "counter_arrow", "range", "movement", "mounted_archery", "morale_cap", "rough_march", "soldier_cap",
    "base_attack", "siege_speed", "stone_build", "catapult", "thunder", "facility_recovery", "fort_build",
    "citadel", "base_counter", "wood_beast", "fire_range", "fire_seed_2", "fire_seed_3", "transport_speed",
    "port_caps", "order_keep", "loyalty_keep",
};

global_variable const char *content_class_names[CONTENT_CLASS_COUNT] = {
    "none", "pike", "halberd", "crossbow", "cavalry", "siege",
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

internal technique_line
content_line_from_name(const char *name)
{
    return (technique_line)content_find_name(content_line_names, TECHNIQUE_LINE_COUNT, name);
}

internal const char *
content_line_name(technique_line line)
{
    return line < TECHNIQUE_LINE_COUNT ? content_line_names[line] : "?";
}

internal content_ability
content_ability_from_name(const char *name)
{
    return (content_ability)content_find_name(content_ability_names, CONTENT_ABILITY_COUNT, name);
}

internal technique_effect
content_technique_effect_from_name(const char *name)
{
    return (technique_effect)content_find_name(content_technique_effect_names, TECHNIQUE_EFFECT_COUNT, name);
}

internal const char *
content_technique_effect_name(technique_effect effect)
{
    return effect < TECHNIQUE_EFFECT_COUNT ? content_technique_effect_names[effect] : "?";
}

internal content_unit_class
content_class_from_name(const char *name)
{
    return (content_unit_class)content_find_name(content_class_names, CONTENT_CLASS_COUNT, name);
}

/* "1|4|7" gives the bits 1, 4 and 7. A part that is not a month 1..12
   gives 0: the row is bad. */
internal u32
content_month_mask_from_text(const char *text)
{
    u32 mask = 0, month = 0;
    b32 has_digit = false;
    for (const char *at = text;; ++at)
    {
        if (*at >= '0' && *at <= '9')
        {
            month = month * 10 + (u32)(*at - '0');
            has_digit = true;
            if (month > 12) return 0;
            continue;
        }
        if (*at != '|' && *at != 0) return 0;
        if (!has_digit || month < 1) return 0;
        mask |= 1u << month;
        month = 0;
        has_digit = false;
        if (*at == 0) break;
    }
    return mask;
}

internal u8
content_find_technique(sim_content *content, const char *key)
{
    for (u32 index = 0; index < content->technique_count; ++index)
    {
        if (strcmp(content->techniques[index].key, key) == 0) return (u8)index;
    }
    return CONTENT_NO_TECHNIQUE;
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
    kind->mergeable        = tsv_cell_yes(table, row, "merge");
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
    CONTENT_RULE("ap.patrol",             ap_patrol),
    CONTENT_RULE("ap.conscript",          ap_conscript),
    CONTENT_RULE("ap.train",              ap_train),
    CONTENT_RULE("ap.merchant",           ap_merchant),
    CONTENT_RULE("ap.produce",            ap_produce),
    CONTENT_RULE("patrol.gold",           patrol_gold),
    CONTENT_RULE("conscript.gold",        conscript_gold),
    CONTENT_RULE("order.max",             order_max),
    CONTENT_RULE("order.bandit_threshold", order_bandit_threshold),
    CONTENT_RULE("order.income_floor",    order_income_floor),
    CONTENT_RULE("patrol.leadership_percent", patrol_leadership_percent),
    CONTENT_RULE("patrol.enemy_percent",  patrol_enemy_percent),
    CONTENT_RULE("conscript.charm_soldiers", conscript_charm_soldiers),
    CONTENT_RULE("conscript.enemy_percent", conscript_enemy_percent),
    CONTENT_RULE("conscript.order_loss",  conscript_order_loss),
    CONTENT_RULE("conscript.morale_loss", conscript_morale_loss),
    CONTENT_RULE("city.soldier_cap",      city_soldier_cap),
    CONTENT_RULE("city.big_soldier_cap",  city_big_soldier_cap),
    CONTENT_RULE("city.weapon_cap",       city_weapon_cap),
    CONTENT_RULE("city.start_morale",     city_start_morale),
    CONTENT_RULE("morale.max",            morale_max),
    CONTENT_RULE("train.force_percent",   train_force_percent),
    CONTENT_RULE("train.drill_percent",   train_drill_percent),
    CONTENT_RULE("merchant.grain_per_gold", merchant_grain_per_gold),
    CONTENT_RULE("merchant.politics_divisor", merchant_politics_divisor),
    CONTENT_RULE("produce.intellect_quantity", produce_intellect_quantity),
    CONTENT_RULE("produce.full_intellect", produce_full_intellect),
    CONTENT_RULE("produce.specialty_percent", produce_specialty_percent),
    CONTENT_RULE("produce.free_monthly",  produce_free_monthly),
    CONTENT_RULE("produce.skill_percent", produce_skill_percent),
    CONTENT_RULE("produce.skill_period_percent", produce_skill_period_percent),
    CONTENT_RULE("level.2_percent",       level2_percent),
    CONTENT_RULE("level.3_percent",       level3_percent),
    CONTENT_RULE("skill.income_percent",  skill_income_percent),
    CONTENT_RULE("skill.rich",            skill_rich),
    CONTENT_RULE("skill.rice",            skill_rice),
    CONTENT_RULE("skill.tax",             skill_tax),
    CONTENT_RULE("skill.levy",            skill_levy),
    CONTENT_RULE("skill.capable",         skill_capable),
    CONTENT_RULE("skill.breeding",        skill_breeding),
    CONTENT_RULE("skill.invention",       skill_invention),
    CONTENT_RULE("skill.shipbuilding",    skill_shipbuilding),
    CONTENT_RULE("technique.per_command", technique_per_command),
    CONTENT_RULE("merit.per_command",     merit_per_command),
    CONTENT_RULE("facility.recovery",     facility_recovery),
    CONTENT_RULE("ap.research",           ap_research),
    CONTENT_RULE("ap.merge",              ap_merge),
    CONTENT_RULE("merge.gold",            merge_gold),
    CONTENT_RULE("merge.max_days",        merge_max_days),
    CONTENT_RULE("merge.min_days",        merge_min_days),
    CONTENT_RULE("merge.full_politics",   merge_full_politics),
    CONTENT_RULE("research.full_ability", research_full_ability),
    CONTENT_RULE("research.personnel_percent", research_personnel_percent),
    CONTENT_RULE("research.teaching_percent",  research_teaching_percent),
    CONTENT_RULE("skill.teaching",        skill_teaching),
    CONTENT_RULE("tally.action_points",   tally_action_points),
    CONTENT_RULE("military_office.ap_percent", military_office_ap_percent),
    CONTENT_RULE("drill.ship_period_percent",  drill_ship_period_percent),
    CONTENT_RULE("game.end_year",         game_end_year),
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

/* Parses one row of weapons.tsv. Returns false if a cell is bad. */
internal b32
content_parse_weapon(sim_content *content, tsv_table *table, u32 row, weapon_kind *kind)
{
    u32 id = 0;
    const char *key = tsv_cell(table, row, "key");
    if (!tsv_cell_u32(table, row, "id", &id) || id != row || !key[0] || strlen(key) >= CONTENT_KEY_SIZE) return false;
    if (!tsv_cell_u32(table, row, "cost", &kind->cost) || !tsv_cell_u32(table, row, "stock", &kind->stock) ||
        !tsv_cell_u32(table, row, "min_days", &kind->min_days) || !tsv_cell_u32(table, row, "max_days", &kind->max_days))
    {
        return false;
    }
    kind->facility  = content_effect_from_name(tsv_cell(table, row, "facility"));
    kind->specialty = content_specialty_from_name(tsv_cell(table, row, "specialty"));
    b32 producer = kind->facility == FACILITY_EFFECT_WEAPONS || kind->facility == FACILITY_EFFECT_HORSES ||
                   kind->facility == FACILITY_EFFECT_SIEGE || kind->facility == FACILITY_EFFECT_SHIPS;
    if (!producer || kind->specialty == CITY_SPECIALTY_COUNT || kind->stock >= CITY_EQUIPMENT_STOCK_COUNT ||
        kind->min_days > kind->max_days)
    {
        return false;
    }
    kind->id = (u8)id;
    memcpy(kind->key, key, strlen(key) + 1);
    /* "-" names no technique. Another key must be a row of techniques.tsv. */
    const char *technique = tsv_cell(table, row, "technique");
    kind->technique = CONTENT_NO_TECHNIQUE;
    if (strcmp(technique, "-") != 0)
    {
        kind->technique = content_find_technique(content, technique);
        if (kind->technique == CONTENT_NO_TECHNIQUE) return false;
    }
    return true;
}

/* The weapon that each row replaces: a key of an earlier or a later row,
   or "-". */
internal b32
content_resolve_replaces(sim_content *content, tsv_table *table, u32 *bad_row)
{
    for (u32 row = 0; row < content->weapon_count; ++row)
    {
        const char *replaces = tsv_cell(table, row, "replaces");
        weapon_kind *kind = content->weapons + row;
        kind->replaces = CONTENT_NO_WEAPON;
        if (strcmp(replaces, "-") == 0) continue;
        for (u32 other = 0; other < content->weapon_count; ++other)
        {
            if (strcmp(content->weapons[other].key, replaces) == 0) kind->replaces = (u8)other;
        }
        if (kind->replaces == CONTENT_NO_WEAPON || kind->replaces == row)
        {
            *bad_row = row;
            return false;
        }
    }
    return true;
}

internal b32
content_load_weapons(sim_content *content, tsv_table *table, u32 *bad_row)
{
    content->weapon_count = 0;
    if (table->row_count > CONTENT_WEAPON_CAPACITY) { *bad_row = CONTENT_WEAPON_CAPACITY; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        weapon_kind kind = {};
        if (!content_parse_weapon(content, table, row, &kind))
        {
            *bad_row = row;
            return false;
        }
        content->weapons[row] = kind;
    }
    content->weapon_count = table->row_count;
    if (!content_resolve_replaces(content, table, bad_row))
    {
        content->weapon_count = 0;
        return false;
    }
    return true;
}

/* Parses one row of techniques.tsv. Returns false if a cell is bad. */
internal b32
content_parse_technique(tsv_table *table, u32 row, technique_kind *kind)
{
    u32 id = 0;
    const char *key = tsv_cell(table, row, "key");
    if (!tsv_cell_u32(table, row, "id", &id) || id != row || !key[0] || strlen(key) >= CONTENT_KEY_SIZE) return false;
    if (!tsv_cell_u32(table, row, "level", &kind->level) || !tsv_cell_u32(table, row, "points", &kind->points) ||
        !tsv_cell_u32(table, row, "gold", &kind->gold) || !tsv_cell_u32(table, row, "min_days", &kind->min_days) ||
        !tsv_cell_u32(table, row, "max_days", &kind->max_days) || !tsv_cell_u32(table, row, "value", &kind->value))
    {
        return false;
    }
    kind->line       = content_line_from_name(tsv_cell(table, row, "line"));
    kind->ability    = content_ability_from_name(tsv_cell(table, row, "ability"));
    kind->effect     = content_technique_effect_from_name(tsv_cell(table, row, "effect"));
    kind->unit_class = content_class_from_name(tsv_cell(table, row, "unit_class"));
    if (kind->line == TECHNIQUE_LINE_COUNT || kind->ability == CONTENT_ABILITY_COUNT || kind->effect == TECHNIQUE_EFFECT_COUNT ||
        kind->unit_class == CONTENT_CLASS_COUNT || kind->level < 1 || kind->min_days > kind->max_days)
    {
        return false;
    }
    kind->id = (u8)id;
    memcpy(kind->key, key, strlen(key) + 1);
    kind->period_assumed = strcmp(tsv_cell(table, row, "period_status"), "assumed") == 0;
    kind->value_assumed  = strcmp(tsv_cell(table, row, "value_status"), "assumed") == 0;
    return true;
}

/* Level n of a line needs level n - 1 of the same line in an earlier row. */
internal b32
content_technique_has_previous(sim_content *content, technique_kind *kind)
{
    if (kind->level == 1) return true;
    for (u32 index = 0; index < content->technique_count; ++index)
    {
        technique_kind *other = content->techniques + index;
        if (other->line == kind->line && other->level == kind->level - 1) return true;
    }
    return false;
}

internal b32
content_load_techniques(sim_content *content, tsv_table *table, u32 *bad_row)
{
    content->technique_count = 0;
    if (table->row_count > CONTENT_TECHNIQUE_CAPACITY) { *bad_row = CONTENT_TECHNIQUE_CAPACITY; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        technique_kind kind = {};
        if (!content_parse_technique(table, row, &kind) || !content_technique_has_previous(content, &kind))
        {
            *bad_row = row;
            content->technique_count = 0;
            return false;
        }
        content->techniques[row] = kind;
        content->technique_count = row + 1;
    }
    return true;
}

/* Parses one row of disasters.tsv. Returns false if a cell is bad. */
internal b32
content_parse_disaster(tsv_table *table, u32 row, disaster_kind *kind)
{
    u32 id = 0;
    const char *key = tsv_cell(table, row, "key");
    if (!tsv_cell_u32(table, row, "id", &id) || id != row || !key[0] || strlen(key) >= CONTENT_KEY_SIZE) return false;
    if (!tsv_cell_u32(table, row, "chance_percent", &kind->chance_percent) || !tsv_cell_u32(table, row, "turns", &kind->turns) ||
        !tsv_cell_u32(table, row, "start_order_loss", &kind->start_order_loss) ||
        !tsv_cell_u32(table, row, "turn_order_loss", &kind->turn_order_loss) ||
        !tsv_cell_u32(table, row, "grain_percent", &kind->grain_percent) ||
        !tsv_cell_u32(table, row, "soldier_percent", &kind->soldier_percent) ||
        !tsv_cell_u32(table, row, "facility_damage", &kind->facility_damage) ||
        !tsv_cell_u32(table, row, "skill_block", &kind->skill_block) || !tsv_cell_u32(table, row, "skill_chance", &kind->skill_chance) ||
        !tsv_cell_u32(table, row, "skill_chance_percent", &kind->skill_chance_percent))
    {
        return false;
    }
    kind->month_mask = content_month_mask_from_text(tsv_cell(table, row, "months"));
    const char *target = tsv_cell(table, row, "facility_target");
    kind->facility_target = strcmp(target, "any") == 0 ? (u32)CONTENT_TARGET_ANY : (u32)content_effect_from_name(target);
    if (!kind->month_mask || kind->chance_percent > 100 || kind->facility_target > CONTENT_TARGET_ANY) return false;
    kind->id = (u8)id;
    memcpy(kind->key, key, strlen(key) + 1);
    kind->low_order_only = tsv_cell_yes(table, row, "low_order_only");
    kind->ends_at_order  = tsv_cell_yes(table, row, "ends_at_order");
    return true;
}

internal b32
content_load_disasters(sim_content *content, tsv_table *table, u32 *bad_row)
{
    content->disaster_count = 0;
    if (table->row_count > CONTENT_DISASTER_CAPACITY) { *bad_row = CONTENT_DISASTER_CAPACITY; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        disaster_kind kind = {};
        if (!content_parse_disaster(table, row, &kind))
        {
            *bad_row = row;
            return false;
        }
        content->disasters[row] = kind;
    }
    content->disaster_count = table->row_count;
    return true;
}
