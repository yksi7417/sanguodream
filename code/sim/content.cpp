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
    CONTENT_RULE("ap.sortie",             ap_sortie),
    CONTENT_RULE("ap.transport",          ap_transport),
    CONTENT_RULE("unit.soldier_cap",      unit_soldier_cap),
    CONTENT_RULE("unit.deputy_percent",   unit_deputy_percent),
    CONTENT_RULE("grade.s",               grade_percent[GRADE_S]),
    CONTENT_RULE("grade.a",               grade_percent[GRADE_A]),
    CONTENT_RULE("grade.b",               grade_percent[GRADE_B]),
    CONTENT_RULE("grade.c",               grade_percent[GRADE_C]),
    CONTENT_RULE("grade.d",               grade_percent[GRADE_D]),
    CONTENT_RULE("grade.e",               grade_percent[GRADE_E]),
    CONTENT_RULE("aptitude.c",            aptitude_percent[0]),
    CONTENT_RULE("aptitude.b",            aptitude_percent[1]),
    CONTENT_RULE("aptitude.a",            aptitude_percent[2]),
    CONTENT_RULE("aptitude.s",            aptitude_percent[3]),
    CONTENT_RULE("move.bonus",            move_bonus),
    CONTENT_RULE("skill.ignore_zoc",      skill_ignore_zoc),
    CONTENT_RULE("skill.escape",          skill_escape),
    CONTENT_RULE("skill.forced_march",    skill_forced_march),
    CONTENT_RULE("skill.long_drive",      skill_long_drive),
    CONTENT_RULE("skill.haul",            skill_haul),
    CONTENT_RULE("combat.damage_percent", combat_damage_percent),
    CONTENT_RULE("combat.min_damage",     combat_min_damage),
    CONTENT_RULE("combat.counter_percent", combat_counter_percent),
    CONTENT_RULE("combat.triangle_percent", combat_triangle_percent),
    CONTENT_RULE("combat.critical_chance", combat_critical_chance),
    CONTENT_RULE("combat.critical_percent", combat_critical_percent),
    CONTENT_RULE("combat.morale_hit",     combat_morale_hit),
    CONTENT_RULE("combat.tactic_success", combat_tactic_success),
    CONTENT_RULE("combat.building_percent", combat_building_percent),
    CONTENT_RULE("combat.siege_percent",  combat_siege_percent),
    CONTENT_RULE("combat.base_counter",   combat_base_counter),
    CONTENT_RULE("combat.capture_percent", combat_capture_percent),
    CONTENT_RULE("combat.halberd_capture", combat_halberd_capture),
    CONTENT_RULE("combat.destroy_technique", combat_destroy_technique),
    CONTENT_RULE("plot.base_success",     plot_base_success),
    CONTENT_RULE("fire.turns",            fire_turns),
    CONTENT_RULE("fire.damage_percent",   fire_damage_percent),
    CONTENT_RULE("fire.building_damage",  fire_building_damage),
    CONTENT_RULE("fire.extra_cost",       fire_extra_cost),
    CONTENT_RULE("build.durability_per_build", build_durability_per_build),
    CONTENT_RULE("siege.blockade_loss",   siege_blockade_loss),
    CONTENT_RULE("siege.recovery",        siege_recovery),
    CONTENT_RULE("siege.default_durability", siege_default_durability),
    CONTENT_RULE("siege.capture_technique", siege_capture_technique),
    CONTENT_RULE("skill.capture",         skill_capture),
    CONTENT_RULE("skill.escape_capture",  skill_escape_capture),
    CONTENT_RULE("skill.fortune",         skill_fortune),
    CONTENT_RULE("skill.subtle",          skill_subtle),
    CONTENT_RULE("skill.fire_god",        skill_fire_god),
    CONTENT_RULE("skill.rattan",          skill_rattan),
    CONTENT_RULE("skill.insight",         skill_insight),
    CONTENT_RULE("skill.strategist",      skill_strategist),
    CONTENT_RULE("skill.feint",           skill_feint),
    CONTENT_RULE("skill.calm",            skill_calm),
    CONTENT_RULE("skill.mirror",          skill_mirror),
    CONTENT_RULE("skill.discipline",      skill_discipline),
    CONTENT_RULE("skill.music",           skill_music),
    CONTENT_RULE("combat.collision_percent", combat_collision_percent),
    CONTENT_RULE("combat.technique_percent", combat_technique_percent),
    CONTENT_RULE("combat.ambush_percent", combat_ambush_percent),
    CONTENT_RULE("combat.lightning_percent", combat_lightning_percent),
    CONTENT_RULE("maze.confuse_chance",   maze_confuse_chance),
    CONTENT_RULE("camp.recover_chance",   camp_recover_chance),
    CONTENT_RULE("natural.recovery",      natural_recovery),
    CONTENT_RULE("loot.percent",          loot_percent),
    CONTENT_RULE("ap.dismantle",          ap_dismantle),
    CONTENT_RULE("dismantle.technique",   dismantle_technique),
    CONTENT_RULE("siege.ladder_percent",  siege_ladder_percent),
    CONTENT_RULE("siege.defence_percent", siege_defence_percent),
    CONTENT_RULE("siege.wall_percent",    siege_wall_percent),
    CONTENT_RULE("skill.ambush_critical", skill_ambush_critical),
    CONTENT_RULE("skill.long_plot",       skill_long_plot),
    CONTENT_RULE("skill.surprise",        skill_surprise),
    CONTENT_RULE("skill.sweep",           skill_sweep),
    CONTENT_RULE("skill.awe",             skill_awe),
    CONTENT_RULE("skill.archer",          skill_archer),
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
    facility_effect effect = content_effect_from_name(target);
    b32 any = strcmp(target, "any") == 0;
    if (!any && effect == FACILITY_EFFECT_COUNT) return false;
    kind->facility_target = any ? (u32)CONTENT_TARGET_ANY : (u32)effect;
    if (!kind->month_mask || kind->chance_percent > 100) return false;
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

/* ---- terrain.tsv and unit_types.tsv -------------------------------------- */

global_variable const char *content_movement_names[MOVEMENT_CLASS_COUNT] = { "foot", "cavalry", "siege", "navy" };
global_variable const char *content_tactic_names[TACTIC_CLASS_COUNT] = { "pike", "halberd", "crossbow", "cavalry", "siege", "navy" };
global_variable const char *content_grade_names[GRADE_COUNT] = { "-", "S", "A", "B", "C", "D", "E" };
global_variable const char *content_aptitude_names[] = { "spear", "halberd", "crossbow", "cavalry", "siege", "navy" };

internal const char *
content_movement_name(movement_class movement)
{
    return movement < MOVEMENT_CLASS_COUNT ? content_movement_names[movement] : "?";
}

/* "pike|halberd" gives the bits of the tactic classes; "-" gives 0. A part
   that is not a class sets *bad. */
internal u32
content_tactics_from_text(const char *text, b32 *bad)
{
    if (strcmp(text, "-") == 0) return 0;
    u32 mask = 0;
    char part[CONTENT_KEY_SIZE];
    u32 length = 0;
    for (const char *at = text;; ++at)
    {
        if (*at != '|' && *at != 0)
        {
            if (length + 1 >= sizeof(part)) { *bad = true; return 0; }
            part[length++] = *at;
            continue;
        }
        part[length] = 0;
        u32 index = content_find_name(content_tactic_names, TACTIC_CLASS_COUNT, part);
        if (index == TACTIC_CLASS_COUNT) { *bad = true; return 0; }
        mask |= 1u << index;
        length = 0;
        if (*at == 0) break;
    }
    return mask;
}

/* A technique key, or "-" for none. A key that is not a technique sets *bad. */
internal u8
content_technique_cell(sim_content *content, tsv_table *table, u32 row, const char *column, b32 *bad)
{
    const char *key = tsv_cell(table, row, column);
    if (strcmp(key, "-") == 0) return CONTENT_NO_TECHNIQUE;
    u8 technique = content_find_technique(content, key);
    if (technique == CONTENT_NO_TECHNIQUE) *bad = true;
    return technique;
}

internal b32
content_parse_terrain(sim_content *content, tsv_table *table, u32 row, terrain_kind *kind)
{
    u32 id = 0, tile = 0, level = 0;
    const char *key = tsv_cell(table, row, "key");
    if (!tsv_cell_u32(table, row, "id", &id) || id != row || !key[0] || strlen(key) >= CONTENT_KEY_SIZE) return false;
    const char *cost_columns[MOVEMENT_CLASS_COUNT] = { "cost_foot", "cost_cavalry", "cost_siege", "cost_navy" };
    for (u32 movement = 0; movement < MOVEMENT_CLASS_COUNT; ++movement)
    {
        u32 cost = 0;
        if (!tsv_cell_u32(table, row, cost_columns[movement], &cost) || cost > 255) return false;
        kind->cost[movement] = (u8)cost;
    }
    if (!tsv_cell_u32(table, row, "loss_percent", &kind->loss_percent) || !tsv_cell_u32(table, row, "loss_waiver_skill", &kind->loss_waiver_skill) ||
        !tsv_cell_u32(table, row, "tile", &tile) || !tsv_cell_u32(table, row, "level", &level) || kind->loss_percent > 100 ||
        kind->loss_waiver_skill > 100 || tile > 255 || level > 255)
    {
        return false;
    }
    b32 bad = false;
    kind->needs_technique       = content_technique_cell(content, table, row, "needs_technique", &bad);
    kind->loss_waiver_technique = content_technique_cell(content, table, row, "loss_waiver_technique", &bad);
    kind->tactics               = content_tactics_from_text(tsv_cell(table, row, "tactics"), &bad);
    if (bad) return false;
    kind->id        = (u8)id;
    memcpy(kind->key, key, strlen(key) + 1);
    kind->buildable = tsv_cell_yes(table, row, "buildable");
    kind->tile      = (u8)tile;
    kind->level     = (u8)level;
    return true;
}

internal b32
content_load_terrain(sim_content *content, tsv_table *table, u32 *bad_row)
{
    content->terrain_count = 0;
    if (table->row_count > CONTENT_TERRAIN_CAPACITY) { *bad_row = CONTENT_TERRAIN_CAPACITY; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        terrain_kind kind = {};
        if (!content_parse_terrain(content, table, row, &kind))
        {
            *bad_row = row;
            return false;
        }
        content->terrains[row] = kind;
    }
    content->terrain_count = table->row_count;
    return true;
}

internal unit_grade
content_grade_from_name(const char *name)
{
    return (unit_grade)content_find_name(content_grade_names, GRADE_COUNT, name);
}

internal b32
content_parse_unit_type(sim_content *content, tsv_table *table, u32 row, unit_type_kind *kind)
{
    u32 id = 0;
    const char *key = tsv_cell(table, row, "key");
    if (!tsv_cell_u32(table, row, "id", &id) || id != row || !key[0] || strlen(key) >= CONTENT_KEY_SIZE) return false;
    if (!tsv_cell_u32(table, row, "move", &kind->move) || !tsv_cell_u32(table, row, "range", &kind->range) || kind->range < 1) return false;
    kind->movement = (movement_class)content_find_name(content_movement_names, MOVEMENT_CLASS_COUNT, tsv_cell(table, row, "movement_class"));
    kind->attack   = content_grade_from_name(tsv_cell(table, row, "attack"));
    kind->defence  = content_grade_from_name(tsv_cell(table, row, "defence"));
    kind->destroy  = content_grade_from_name(tsv_cell(table, row, "destroy"));
    if (kind->movement == MOVEMENT_CLASS_COUNT || kind->attack == GRADE_COUNT || kind->defence == GRADE_COUNT || kind->destroy == GRADE_COUNT)
    {
        return false;
    }
    const char *aptitude = tsv_cell(table, row, "aptitude");
    u32 aptitude_index = content_find_name(content_aptitude_names, ArrayCount(content_aptitude_names), aptitude);
    if (strcmp(aptitude, "none") == 0) kind->aptitude = CONTENT_APTITUDE_NONE;
    else if (aptitude_index < ArrayCount(content_aptitude_names)) kind->aptitude = (u8)aptitude_index;
    else return false;
    const char *weapon = tsv_cell(table, row, "weapon");
    kind->weapon = CONTENT_NO_WEAPON;
    if (strcmp(weapon, "-") != 0)
    {
        for (u32 index = 0; index < content->weapon_count; ++index)
        {
            if (strcmp(content->weapons[index].key, weapon) == 0) kind->weapon = (u8)index;
        }
        if (kind->weapon == CONTENT_NO_WEAPON) return false;
    }
    kind->id            = (u8)id;
    memcpy(kind->key, key, strlen(key) + 1);
    kind->can_attack    = tsv_cell_yes(table, row, "can_attack");
    kind->counterattack = tsv_cell_yes(table, row, "counterattack");
    kind->transport     = tsv_cell_yes(table, row, "transport");
    return true;
}

internal b32
content_load_unit_types(sim_content *content, tsv_table *table, u32 *bad_row)
{
    content->unit_type_count = 0;
    if (table->row_count > CONTENT_UNIT_TYPE_CAPACITY) { *bad_row = CONTENT_UNIT_TYPE_CAPACITY; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        unit_type_kind kind = {};
        if (!content_parse_unit_type(content, table, row, &kind))
        {
            *bad_row = row;
            return false;
        }
        content->unit_types[row] = kind;
    }
    content->unit_type_count = table->row_count;
    return true;
}

internal u8
content_find_terrain(sim_content *content, const char *key)
{
    for (u32 index = 0; index < content->terrain_count; ++index)
    {
        if (strcmp(content->terrains[index].key, key) == 0) return (u8)index;
    }
    return CONTENT_NO_TERRAIN;
}

internal u8
content_find_unit_type(sim_content *content, const char *key)
{
    for (u32 index = 0; index < content->unit_type_count; ++index)
    {
        if (strcmp(content->unit_types[index].key, key) == 0) return (u8)index;
    }
    return CONTENT_NO_UNIT_TYPE;
}

/* ---- tactics.tsv, plots.tsv, buildings.tsv, critical_skills.tsv ------------ */

global_variable const char *content_pattern_names[PATTERN_COUNT] = { "single", "line", "front3", "around", "splash" };
global_variable const char *content_tactic_effect_names[TACTIC_EFFECT_COUNT] = { "none", "knock", "pull", "advance", "pass", "confuse", "fire" };
global_variable const char *content_plot_effect_names[PLOT_EFFECT_COUNT] = {
    "fire", "extinguish", "false_report", "confuse", "calm", "ambush", "infighting", "sorcery", "lightning",
};
global_variable const char *content_plot_target_names[PLOT_TARGET_COUNT] = { "enemy", "own", "hex" };
global_variable const char *content_building_type_names[BUILDING_TYPE_COUNT] = {
    "camp", "tower", "drum", "music", "maze", "wall", "seed", "ball", "ship", "dike", "rockfall",
};
global_variable const char *content_condition_names[CRITICAL_CONDITION_COUNT] = { "always", "lower_force", "forest", "building", "mounted" };

/* "pike|halberd": the bits of the unit types with these keys; "-" gives 0.
   A key that is not a unit type sets *bad. */
internal u32
content_unit_type_mask(sim_content *content, const char *text, b32 *bad)
{
    if (strcmp(text, "-") == 0) return 0;
    u32 mask = 0;
    char part[CONTENT_KEY_SIZE];
    u32 length = 0;
    for (const char *at = text;; ++at)
    {
        if (*at != '|' && *at != 0)
        {
            if (length + 1 >= sizeof(part)) { *bad = true; return 0; }
            part[length++] = *at;
            continue;
        }
        part[length] = 0;
        u8 type = content_find_unit_type(content, part);
        if (type == CONTENT_NO_UNIT_TYPE) { *bad = true; return 0; }
        mask |= 1u << type;
        length = 0;
        if (*at == 0) break;
    }
    return mask;
}

/* "units|buildings|ships": the TARGET_* bits. */
internal u32
content_targets_from_text(const char *text, b32 *bad)
{
    u32 mask = 0;
    if (strstr(text, "units"))     mask |= TARGET_UNITS;
    if (strstr(text, "buildings")) mask |= TARGET_BUILDINGS;
    if (strstr(text, "ships"))     mask |= TARGET_SHIPS;
    if (!mask) *bad = true;
    return mask;
}

internal b32
content_parse_tactic(sim_content *content, tsv_table *table, u32 row, tactic_kind *kind)
{
    u32 id = 0;
    const char *key = tsv_cell(table, row, "key");
    if (!tsv_cell_u32(table, row, "id", &id) || id != row || !key[0] || strlen(key) >= CONTENT_KEY_SIZE) return false;
    if (!tsv_cell_u32(table, row, "morale", &kind->morale) || !tsv_cell_u32(table, row, "range_min", &kind->range_min) ||
        !tsv_cell_u32(table, row, "range_max", &kind->range_max) || !tsv_cell_u32(table, row, "value", &kind->value) ||
        !tsv_cell_u32(table, row, "damage_percent", &kind->damage_percent) || kind->range_min > kind->range_max || kind->range_min < 1)
    {
        return false;
    }
    b32 bad = false;
    kind->unit_types = content_unit_type_mask(content, tsv_cell(table, row, "unit_types"), &bad);
    kind->targets    = content_targets_from_text(tsv_cell(table, row, "targets"), &bad);
    kind->pattern    = (tactic_pattern)content_find_name(content_pattern_names, PATTERN_COUNT, tsv_cell(table, row, "pattern"));
    kind->effect     = (tactic_effect)content_find_name(content_tactic_effect_names, TACTIC_EFFECT_COUNT, tsv_cell(table, row, "effect"));
    const char *grade = tsv_cell(table, row, "grade");
    if      (strcmp(grade, "any") == 0) kind->aptitude = 0;
    else if (strcmp(grade, "B") == 0)   kind->aptitude = 1;
    else if (strcmp(grade, "A") == 0)   kind->aptitude = 2;
    else if (strcmp(grade, "S") == 0)   kind->aptitude = 3;
    else bad = true;
    if (bad || !kind->unit_types || kind->pattern == PATTERN_COUNT || kind->effect == TACTIC_EFFECT_COUNT) return false;
    kind->id = (u8)id;
    memcpy(kind->key, key, strlen(key) + 1);
    kind->always_success = tsv_cell_yes(table, row, "always_success");
    return true;
}

internal b32
content_load_tactics(sim_content *content, tsv_table *table, u32 *bad_row)
{
    content->tactic_count = 0;
    if (table->row_count > CONTENT_TACTIC_CAPACITY) { *bad_row = CONTENT_TACTIC_CAPACITY; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        tactic_kind kind = {};
        if (!content_parse_tactic(content, table, row, &kind)) { *bad_row = row; return false; }
        content->tactics[row] = kind;
    }
    content->tactic_count = table->row_count;
    return true;
}

internal b32
content_parse_plot(sim_content *content, tsv_table *table, u32 row, plot_kind *kind)
{
    u32 id = 0;
    const char *key = tsv_cell(table, row, "key");
    if (!tsv_cell_u32(table, row, "id", &id) || id != row || !key[0] || strlen(key) >= CONTENT_KEY_SIZE) return false;
    if (!tsv_cell_u32(table, row, "morale", &kind->morale) || !tsv_cell_u32(table, row, "range", &kind->range) ||
        !tsv_cell_u32(table, row, "turns", &kind->turns) || !tsv_cell_u32(table, row, "needs_skill", &kind->needs_skill) ||
        !tsv_cell_u32(table, row, "needs_skill_2", &kind->needs_skill_2))
    {
        return false;
    }
    b32 bad = false;
    const char *area = tsv_cell(table, row, "area");
    if (strcmp(area, "splash") == 0) kind->splash = true;
    else if (strcmp(area, "single") != 0) bad = true;
    kind->target = (plot_target)content_find_name(content_plot_target_names, PLOT_TARGET_COUNT, tsv_cell(table, row, "target"));
    kind->effect = (plot_effect)content_find_name(content_plot_effect_names, PLOT_EFFECT_COUNT, tsv_cell(table, row, "effect"));
    const char *terrain = tsv_cell(table, row, "from_terrain");
    kind->from_terrain = CONTENT_NO_TERRAIN;
    if (strcmp(terrain, "-") != 0)
    {
        kind->from_terrain = content_find_terrain(content, terrain);
        if (kind->from_terrain == CONTENT_NO_TERRAIN) bad = true;
    }
    kind->not_unit_types = content_unit_type_mask(content, tsv_cell(table, row, "not_unit_types"), &bad);
    if (bad || kind->target == PLOT_TARGET_COUNT || kind->effect == PLOT_EFFECT_COUNT) return false;
    kind->id = (u8)id;
    memcpy(kind->key, key, strlen(key) + 1);
    return true;
}

internal b32
content_load_plots(sim_content *content, tsv_table *table, u32 *bad_row)
{
    content->plot_count = 0;
    if (table->row_count > CONTENT_PLOT_CAPACITY) { *bad_row = CONTENT_PLOT_CAPACITY; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        plot_kind kind = {};
        if (!content_parse_plot(content, table, row, &kind)) { *bad_row = row; return false; }
        content->plots[row] = kind;
    }
    content->plot_count = table->row_count;
    return true;
}

internal b32
content_parse_building(sim_content *content, tsv_table *table, u32 row, building_kind *kind)
{
    u32 id = 0;
    const char *key = tsv_cell(table, row, "key");
    if (!tsv_cell_u32(table, row, "id", &id) || id != row || !key[0] || strlen(key) >= CONTENT_KEY_SIZE) return false;
    if (!tsv_cell_u32(table, row, "durability", &kind->durability) || !tsv_cell_u32(table, row, "gold", &kind->gold) ||
        !tsv_cell_u32(table, row, "range_min", &kind->range_min) || !tsv_cell_u32(table, row, "range_max", &kind->range_max) ||
        !tsv_cell_u32(table, row, "value", &kind->value) || kind->durability == 0)
    {
        return false;
    }
    b32 bad = false;
    kind->type      = (building_type)content_find_name(content_building_type_names, BUILDING_TYPE_COUNT, tsv_cell(table, row, "kind"));
    kind->technique = content_technique_cell(content, table, row, "technique", &bad);
    const char *replaces = tsv_cell(table, row, "replaces");
    kind->replaces = CONTENT_NO_BUILDING;
    if (strcmp(replaces, "-") != 0)
    {
        for (u32 index = 0; index < row; ++index)
        {
            if (strcmp(content->buildings[index].key, replaces) == 0) kind->replaces = (u8)index;
        }
        if (kind->replaces == CONTENT_NO_BUILDING) bad = true;
    }
    if (bad || kind->type == BUILDING_TYPE_COUNT) return false;
    kind->id = (u8)id;
    memcpy(kind->key, key, strlen(key) + 1);
    kind->water   = tsv_cell_yes(table, row, "water");
    kind->natural = tsv_cell_yes(table, row, "natural");
    return true;
}

/* The replaced building is an earlier row. */
internal b32
content_load_buildings(sim_content *content, tsv_table *table, u32 *bad_row)
{
    content->building_count = 0;
    if (table->row_count > CONTENT_BUILDING_CAPACITY) { *bad_row = CONTENT_BUILDING_CAPACITY; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        building_kind kind = {};
        if (!content_parse_building(content, table, row, &kind)) { *bad_row = row; content->building_count = 0; return false; }
        content->buildings[row] = kind;
        content->building_count = row + 1;
    }
    return true;
}

internal b32
content_load_criticals(sim_content *content, tsv_table *table, u32 *bad_row)
{
    content->critical_count = 0;
    if (table->row_count > CONTENT_CRITICAL_CAPACITY) { *bad_row = CONTENT_CRITICAL_CAPACITY; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        critical_skill kind = {};
        const char *action = tsv_cell(table, row, "action");
        const char *unit_class = tsv_cell(table, row, "unit_class");
        u32 id = 0;
        kind.action = strcmp(action, "attack") == 0 ? CRITICAL_ATTACK : strcmp(action, "tactic") == 0 ? CRITICAL_TACTIC :
                      strcmp(action, "both") == 0 ? CRITICAL_BOTH : 0;
        kind.condition = (critical_condition)content_find_name(content_condition_names, CRITICAL_CONDITION_COUNT, tsv_cell(table, row, "condition"));
        if      (strcmp(unit_class, "any") == 0)  kind.aptitude = CONTENT_ANY_APTITUDE;
        else if (strcmp(unit_class, "pike") == 0) kind.aptitude = 0;
        else
        {
            kind.aptitude = content_find_name(content_aptitude_names, ArrayCount(content_aptitude_names), unit_class);
            if (kind.aptitude == ArrayCount(content_aptitude_names)) kind.aptitude = 0xFFFFFFFFu;
        }
        if (!tsv_cell_u32(table, row, "id", &id) || id != row || !tsv_cell_u32(table, row, "skill", &kind.skill) || kind.skill < 1 ||
            kind.skill > 100 || !kind.action || kind.condition == CRITICAL_CONDITION_COUNT || kind.aptitude == 0xFFFFFFFFu)
        {
            *bad_row = row;
            content->critical_count = 0;
            return false;
        }
        content->criticals[row] = kind;
        content->critical_count = row + 1;
    }
    return true;
}
