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
    CONTENT_RULE("fire.cost_multiplier",  fire_cost_multiplier),
    CONTENT_RULE("fire.attack_turns_min", fire_attack_turns_min),
    CONTENT_RULE("fire.attack_turns_max", fire_attack_turns_max),
    CONTENT_RULE("fire.other_turns_min",  fire_other_turns_min),
    CONTENT_RULE("fire.other_turns_max",  fire_other_turns_max),
    CONTENT_RULE("rockfall.return_percent", rockfall_return_percent),
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
    CONTENT_RULE("confusion.recover_floor", confusion_recover_floor),
    CONTENT_RULE("confusion.recover_cap",   confusion_recover_cap),
    CONTENT_RULE("panic.soldiers",        panic_soldiers),
    CONTENT_RULE("panic.ratio",           panic_ratio),
    CONTENT_RULE("panic.chance",          panic_chance),
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
    /* M7 */
    CONTENT_RULE("ap.search", ap_search), CONTENT_RULE("ap.recruit", ap_recruit), CONTENT_RULE("ap.reward", ap_reward),
    CONTENT_RULE("ap.move", ap_move_officers), CONTENT_RULE("ap.summon", ap_summon),
    CONTENT_RULE("ap.give_item", ap_give_item), CONTENT_RULE("ap.diplomacy", ap_diplomacy), CONTENT_RULE("ap.faction_plot", ap_faction_plot),
    CONTENT_RULE("reward.gold", reward_gold), CONTENT_RULE("reward.loyalty", reward_loyalty),
    CONTENT_RULE("reward.compatibility_divisor", reward_compatibility_divisor),
    CONTENT_RULE("search.base", search_base), CONTENT_RULE("search.politics_percent", search_politics_percent),
    CONTENT_RULE("search.gold_chance", search_gold_chance), CONTENT_RULE("search.gold", search_gold),
    CONTENT_RULE("search.debate_intellect", search_debate_intellect),
    CONTENT_RULE("recruit.base", recruit_base), CONTENT_RULE("recruit.charm_percent", recruit_charm_percent),
    CONTENT_RULE("recruit.captive_bonus", recruit_captive_bonus), CONTENT_RULE("recruit.relation_bonus", recruit_relation_bonus),
    CONTENT_RULE("recruit.new_loyalty", recruit_new_loyalty), CONTENT_RULE("recruit.compatibility_divisor", recruit_compatibility_divisor),
    CONTENT_RULE("recruit.debate_intellect", recruit_debate_intellect),
    CONTENT_RULE("loyalty.start", loyalty_start), CONTENT_RULE("loyalty.monthly", loyalty_monthly),
    CONTENT_RULE("loyalty.ambition", loyalty_ambition), CONTENT_RULE("loyalty.unpaid", loyalty_unpaid),
    CONTENT_RULE("loyalty.leave", loyalty_leave), CONTENT_RULE("loyalty.leave_chance", loyalty_leave_chance),
    CONTENT_RULE("experience.command", experience_command), CONTENT_RULE("experience.level", experience_level),
    CONTENT_RULE("age.decline", age_decline), CONTENT_RULE("age.death_chance", death_chance),
    CONTENT_RULE("health.wound_turns", wound_turns), CONTENT_RULE("stamina.recovery", stamina_recovery),
    CONTENT_RULE("release.technique", release_technique), CONTENT_RULE("exile.technique", exile_technique),
    CONTENT_RULE("matchmaking.technique", matchmaking_technique), CONTENT_RULE("matchmaking.merit", matchmaking_merit),
    CONTENT_RULE("matchmaking.age", matchmaking_age),
    CONTENT_RULE("relation.liked_percent", relation_liked_percent), CONTENT_RULE("relation.disliked_intellect", relation_disliked_intellect),
    CONTENT_RULE("duel.damage", duel_damage), CONTENT_RULE("duel.attack_percent", duel_attack_percent),
    CONTENT_RULE("duel.defence_percent", duel_defence_percent), CONTENT_RULE("duel.spirit_rise", duel_spirit_rise),
    CONTENT_RULE("duel.spirit_hit", duel_spirit_hit), CONTENT_RULE("duel.critical_rounds", duel_critical_rounds),
    CONTENT_RULE("duel.max_rounds", duel_max_rounds), CONTENT_RULE("duel.capture_percent", duel_capture_percent),
    CONTENT_RULE("duel.merit", duel_merit), CONTENT_RULE("duel.technique", duel_technique),
    CONTENT_RULE("duel.force_chance", duel_force_chance), CONTENT_RULE("duel.morale_loss", duel_morale_loss),
    CONTENT_RULE("duel.accept_careful", duel_accept[0]), CONTENT_RULE("duel.accept_calm", duel_accept[1]),
    CONTENT_RULE("duel.accept_bold", duel_accept[2]), CONTENT_RULE("duel.accept_reckless", duel_accept[3]),
    CONTENT_RULE("duel.lower_force_percent", duel_lower_force_percent), CONTENT_RULE("duel.finisher", duel_finisher),
    CONTENT_RULE("duel.vital", duel_vital), CONTENT_RULE("duel.peerless", duel_peerless), CONTENT_RULE("duel.hidden", duel_hidden),
    CONTENT_RULE("duel.feint", duel_feint), CONTENT_RULE("duel.boost_percent", duel_boost_percent),
    CONTENT_RULE("duel.boost_rounds", duel_boost_rounds), CONTENT_RULE("duel.retreat_chance", duel_retreat_chance),
    CONTENT_RULE("duel.polearm_percent", duel_polearm_percent), CONTENT_RULE("duel.sky_halberd_percent", duel_sky_halberd_percent),
    CONTENT_RULE("duel.sword_percent", duel_sword_percent), CONTENT_RULE("duel.one_blow_chance", duel_one_blow_chance),
    CONTENT_RULE("duel.one_blow_percent", duel_one_blow_percent),
    CONTENT_RULE("debate.mind", debate_mind), CONTENT_RULE("debate.small", debate_card_damage[1]),
    CONTENT_RULE("debate.medium", debate_card_damage[2]), CONTENT_RULE("debate.large", debate_card_damage[3]),
    CONTENT_RULE("debate.topic_percent", debate_topic_percent), CONTENT_RULE("debate.shout", debate_shout),
    CONTENT_RULE("debate.ignore", debate_ignore), CONTENT_RULE("debate.anger_hit", debate_anger_hit),
    CONTENT_RULE("debate.anger_draw", debate_anger_draw), CONTENT_RULE("debate.anger_card", debate_anger_card),
    CONTENT_RULE("debate.calm_card", debate_calm_card), CONTENT_RULE("debate.rage_rounds", debate_rage_rounds),
    CONTENT_RULE("debate.topic_rounds", debate_topic_rounds), CONTENT_RULE("debate.instant_gap", debate_instant_gap),
    CONTENT_RULE("debate.merit", debate_merit), CONTENT_RULE("debate.technique", debate_technique),
    CONTENT_RULE("debate.press_chance", debate_press_chance), CONTENT_RULE("debate.skill_card_percent", debate_skill_card_percent),
    CONTENT_RULE("friendliness.start", friendliness_start), CONTENT_RULE("goodwill.gold", goodwill_gold),
    CONTENT_RULE("goodwill.base", goodwill_base), CONTENT_RULE("goodwill.politics_percent", goodwill_politics_percent),
    CONTENT_RULE("diplomacy.fail_loss", diplomacy_fail_loss), CONTENT_RULE("alliance.gold", alliance_gold),
    CONTENT_RULE("alliance.base", alliance_base), CONTENT_RULE("break.loss", break_loss), CONTENT_RULE("break.technique", break_technique),
    CONTENT_RULE("ceasefire.base", ceasefire_base), CONTENT_RULE("ceasefire.month_cost", ceasefire_month_cost),
    CONTENT_RULE("surrender.base", surrender_base), CONTENT_RULE("exchange.base", exchange_base), CONTENT_RULE("aid.base", aid_base),
    CONTENT_RULE("aid.turns", aid_turns), CONTENT_RULE("aid.soldiers", aid_soldiers), CONTENT_RULE("diplomacy.technique", diplomacy_technique),
    CONTENT_RULE("faction_plot.gold", faction_plot_gold), CONTENT_RULE("two_tigers.base", two_tigers_base),
    CONTENT_RULE("two_tigers.loss", two_tigers_loss), CONTENT_RULE("drive_tiger.base", drive_tiger_base),
    CONTENT_RULE("rumour.base", rumour_base), CONTENT_RULE("rumour.order_loss", rumour_order_loss),
    CONTENT_RULE("rumour.loyalty_loss", rumour_loyalty_loss),
    CONTENT_RULE("skill.search", skill_search), CONTENT_RULE("skill.debater", skill_debater),
    CONTENT_RULE("skill.benevolence", skill_benevolence), CONTENT_RULE("skill.wife_help", skill_wife_help),
    CONTENT_RULE("skill.guard", skill_guard), CONTENT_RULE("skill.fame", skill_fame), CONTENT_RULE("skill.fame_percent", skill_fame_percent),
    CONTENT_RULE("skill.morale_up", skill_morale_up), CONTENT_RULE("skill.double_attack", skill_double_attack),
    CONTENT_RULE("skill.heart_attack", skill_heart_attack), CONTENT_RULE("skill.unyielding", skill_unyielding),
    CONTENT_RULE("skill.diamond", skill_diamond), CONTENT_RULE("skill.iron_wall", skill_iron_wall), CONTENT_RULE("skill.fury", skill_fury),
    CONTENT_RULE("skill.plot_thrift", skill_plot_thrift), CONTENT_RULE("skill.heart_attack_percent", skill_heart_attack_percent), CONTENT_RULE("item.seal_technique", seal_technique),
    /* M8 */
    CONTENT_RULE("base.gold", base_gold), CONTENT_RULE("base.troop_cap", base_troop_cap), CONTENT_RULE("base.caps_percent", base_caps_percent),
    CONTENT_RULE("base.start_troops", base_start_troops), CONTENT_RULE("corps.commands", corps_commands), CONTENT_RULE("council.officers", council_officers),
    CONTENT_RULE("adviser.intellect", adviser_intellect), CONTENT_RULE("advice.per_intellect", advice_per_intellect),
    CONTENT_RULE("emperor.technique", emperor_technique), CONTENT_RULE("emperor.loyalty", emperor_loyalty), CONTENT_RULE("emperor.city", emperor_city),
    CONTENT_RULE("event.experience", event_experience), CONTENT_RULE("event.merit", event_merit),
    CONTENT_RULE("ai.urgency", ai_urgency), CONTENT_RULE("ai.sortie_soldiers", ai_sortie_soldiers), CONTENT_RULE("ai.sortie_reserve", ai_sortie_reserve),
    CONTENT_RULE("ai.goodwill_gold", ai_goodwill_gold), CONTENT_RULE("ai.commands", ai_commands), CONTENT_RULE("corps.ap_divisor", corps_ap_divisor),
    CONTENT_RULE("ai.target_hexes", ai_target_hexes), CONTENT_RULE("ai.free_city_bonus", ai_free_city_bonus),
    CONTENT_RULE("ai.defend_bonus", ai_defend_bonus), CONTENT_RULE("ai.defend_hexes", ai_defend_hexes), CONTENT_RULE("ai.march_hexes", ai_march_hexes),
    CONTENT_RULE("ai.grain_margin_turns", ai_grain_margin_turns), CONTENT_RULE("ai.resupply_score", ai_resupply_score),
    CONTENT_RULE("ai.attack_after_months", ai_attack_after_months),
    CONTENT_RULE("lifespan.long_years", lifespan_long_years), CONTENT_RULE("new_officer.max", new_officer_max),
    CONTENT_RULE("new_officer.high", new_officer_high), CONTENT_RULE("new_officer.normal", new_officer_normal), CONTENT_RULE("new_officer.low", new_officer_low),
    CONTENT_RULE("train.ap", train_ap), CONTENT_RULE("train.turns", train_turns),
    CONTENT_RULE("ability_research.ap", ability_research_ap), CONTENT_RULE("ability_research.gold", ability_research_gold),
    CONTENT_RULE("scenario.public_order", scenario_public_order),
};

/* Sets the rule field with this key. False if no field has the key. */
internal b32
content_set_rule(sim_rules *rules, const char *key, u32 value)
{
    for (u32 field = 0; field < ArrayCount(content_rule_fields); ++field)
    {
        if (strcmp(content_rule_fields[field].key, key) != 0) continue;
        *(u32 *)((u8 *)rules + content_rule_fields[field].offset) = value;
        return true;
    }
    return false;
}

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
    /* M10: terrain-types.tsv gives "enterable" and "burnable". A pack
       without the columns gets the rule of M6: a unit can enter a terrain
       with a cost, and every land terrain but sand burns. */
    b32 land = kind->cost[MOVEMENT_FOOT] || kind->cost[MOVEMENT_CAVALRY] || kind->cost[MOVEMENT_SIEGE];
    const char *enterable = tsv_cell(table, row, "enterable");
    const char *burnable  = tsv_cell(table, row, "burnable");
    kind->enterable = enterable[0] ? strcmp(enterable, "yes") == 0 : land || kind->cost[MOVEMENT_NAVY];
    kind->burnable  = burnable[0] ? strcmp(burnable, "yes") == 0 : kind->cost[MOVEMENT_FOOT] && strcmp(key, "sand") != 0;
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

/* movement_costs.tsv (M10, F-148): one row for each armament of
   movement-costs.tsv. The table has one column for each terrain key of
   terrain.tsv; 255 means that the armament cannot enter. Load it after
   terrain.tsv. */
internal b32
content_load_armaments(sim_content *content, tsv_table *table, u32 *bad_row)
{
    content->armament_count = 0;
    if (table->row_count > CONTENT_ARMAMENT_CAPACITY) { *bad_row = CONTENT_ARMAMENT_CAPACITY; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        armament_kind *kind = content->armaments + row;
        *kind = {};
        u32 id = 0;
        const char *key = tsv_cell(table, row, "key");
        b32 good = tsv_cell_u32(table, row, "id", &id) && id == row && key[0] && strlen(key) < CONTENT_KEY_SIZE &&
                   tsv_cell_u32(table, row, "move", &kind->move);
        for (u32 terrain = 0; good && terrain < content->terrain_count; ++terrain)
        {
            u32 cost = 0;
            good = tsv_cell_u32(table, row, content->terrains[terrain].key, &cost) && cost >= 1 && cost <= 255;
            kind->cost[terrain] = cost == 255 ? (u8)0 : (u8)cost;
        }
        if (!good)
        {
            *bad_row = row;
            content->armament_count = 0;
            return false;
        }
        kind->id = (u8)id;
        memcpy(kind->key, key, strlen(key) + 1);
    }
    content->armament_count = table->row_count;
    return true;
}

internal u8
content_find_armament(sim_content *content, const char *key)
{
    for (u32 index = 0; index < content->armament_count; ++index)
    {
        if (strcmp(content->armaments[index].key, key) == 0) return (u8)index;
    }
    return CONTENT_NO_ARMAMENT;
}

/* save_types.tsv (M11): one row for each facility type of the building
   array of a save. Load it after facilities.tsv and buildings.tsv: a row
   names a key of one of them. Types without a row stay SAVE_TYPE_NONE. */
internal b32
content_load_save_types(sim_content *content, tsv_table *table, u32 *bad_row)
{
    local_persist const char *kind_names[SAVE_TYPE_KIND_COUNT] = { "none", "base", "facility", "building", "natural", "temple", "ruin" };
    for (u32 type = 0; type < CONTENT_SAVE_TYPE_COUNT; ++type) content->save_types[type] = {};
    content->has_save_types = false;
    for (u32 row = 0; row < table->row_count; ++row)
    {
        u32 type = 0, level = 0;
        const char *key = tsv_cell(table, row, "key");
        u32 kind = content_find_name(kind_names, SAVE_TYPE_KIND_COUNT, tsv_cell(table, row, "kind"));
        b32 good = tsv_cell_u32(table, row, "save_type", &type) && type < CONTENT_SAVE_TYPE_COUNT && kind < SAVE_TYPE_KIND_COUNT &&
                   tsv_cell_u32(table, row, "level", &level) && level <= 3;
        save_type entry = {};
        entry.kind  = (u8)kind;
        entry.level = (u8)level;
        if (good && kind == SAVE_TYPE_FACILITY)
        {
            entry.row = CONTENT_NO_FACILITY;
            for (u32 index = 0; index < content->facility_count; ++index) if (strcmp(content->facilities[index].key, key) == 0) entry.row = (u8)index;
            good = entry.row != CONTENT_NO_FACILITY && level >= 1;
        }
        if (good && (kind == SAVE_TYPE_BUILDING || kind == SAVE_TYPE_NATURAL))
        {
            entry.row = CONTENT_NO_BUILDING;
            for (u32 index = 0; index < content->building_count; ++index) if (strcmp(content->buildings[index].key, key) == 0) entry.row = (u8)index;
            good = entry.row != CONTENT_NO_BUILDING;
        }
        if (!good)
        {
            *bad_row = row;
            return false;
        }
        content->save_types[type] = entry;
    }
    content->has_save_types = true;
    return true;
}

/* The armament of a cell of unit_types.tsv: a key of movement_costs.tsv,
   "-" for none, or an empty cell (a pack without the column). */
internal u8
content_armament_cell(sim_content *content, tsv_table *table, u32 row, const char *column, b32 *bad)
{
    const char *cell = tsv_cell(table, row, column);
    if (!cell[0] || strcmp(cell, "-") == 0) return CONTENT_NO_ARMAMENT;
    u8 armament = content_find_armament(content, cell);
    if (armament == CONTENT_NO_ARMAMENT) *bad = true;
    return armament;
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
    b32 bad = false;
    kind->armament = content_armament_cell(content, table, row, "armament", &bad);   /* M10 */
    kind->ship     = content_armament_cell(content, table, row, "ship", &bad);
    return !bad;
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

/* ---- skills.tsv, ranks.tsv, posts.tsv, items.tsv (M7) ------------------------ */

global_variable const char *content_skill_category_names[SKILL_CATEGORY_COUNT] = {
    "movement", "attack", "defence", "plot", "support", "domestic", "income", "disaster", "relation",
};
global_variable const char *content_item_type_names[ITEM_TYPE_COUNT] = {
    "horse", "bow", "sword", "book", "polearm", "seal", "hidden_weapon", "bronze", "other",
};
global_variable const char *content_polearm_names[POLEARM_COUNT] = { "plain", "sky_halberd", "snake_spear", "dragon_blade" };

/* One row of skills.tsv for each number 1 to 100, in order. A hooked skill
   names the rule hook "skill.<effect>"; the loader writes its number there. */
internal b32
content_load_skills(sim_content *content, tsv_table *table, u32 *bad_row)
{
    content->skill_count = 0;
    if (table->row_count != CONTENT_SKILL_COUNT) { *bad_row = table->row_count; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        skill_kind kind = {};
        u32 number = 0;
        const char *key = tsv_cell(table, row, "key"), *effect = tsv_cell(table, row, "effect");
        kind.category = (u8)content_find_name(content_skill_category_names, SKILL_CATEGORY_COUNT, tsv_cell(table, row, "category"));
        const char *applies = tsv_cell(table, row, "applies");
        b32 by_rule = strcmp(applies, "rule") == 0;
        kind.hooked   = by_rule || strcmp(applies, "table") == 0;
        b32 bad = !tsv_cell_u32(table, row, "number", &number) || number != row + 1 || !key[0] || strlen(key) >= CONTENT_KEY_SIZE ||
                  strlen(effect) >= CONTENT_KEY_SIZE || kind.category == SKILL_CATEGORY_COUNT ||
                  (!kind.hooked && strcmp(applies, "none") != 0);
        if (!bad && by_rule)
        {
            char rule[CONTENT_KEY_SIZE + 8] = "skill.";
            memcpy(rule + 6, effect, strlen(effect) + 1);
            bad = !content_set_rule(&content->rules, rule, number);
        }
        if (bad) { *bad_row = row; content->skill_count = 0; return false; }
        kind.number = (u8)number;
        memcpy(kind.key, key, strlen(key) + 1);
        memcpy(kind.effect, effect, strlen(effect) + 1);
        content->skills[row] = kind;
        content->skill_count = row + 1;
    }
    return true;
}

internal b32
content_load_ranks(sim_content *content, tsv_table *table, u32 *bad_row)
{
    content->rank_count = 0;
    if (table->row_count == 0 || table->row_count > CONTENT_RANK_CAPACITY) { *bad_row = 0; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        rank_kind kind = {};
        u32 id = 0;
        const char *key = tsv_cell(table, row, "key");
        if (!tsv_cell_u32(table, row, "id", &id) || id != row || !key[0] || strlen(key) >= CONTENT_KEY_SIZE ||
            !tsv_cell_u32(table, row, "soldiers", &kind.soldiers) || !tsv_cell_u32(table, row, "cities", &kind.cities) ||
            (row > 0 && kind.cities < content->ranks[row - 1].cities))
        {
            *bad_row = row;
            content->rank_count = 0;
            return false;
        }
        kind.id = (u8)id;
        memcpy(kind.key, key, strlen(key) + 1);
        kind.needs_emperor = tsv_cell_yes(table, row, "needs_emperor");
        content->ranks[row] = kind;
        content->rank_count = row + 1;
    }
    return true;
}

internal b32
content_load_posts(sim_content *content, tsv_table *table, u32 *bad_row)
{
    content->post_count = 0;
    if (table->row_count > CONTENT_POST_CAPACITY) { *bad_row = CONTENT_POST_CAPACITY; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        post_kind kind = {};
        u32 id = 0, rank = 0;
        const char *key = tsv_cell(table, row, "key"), *ability = tsv_cell(table, row, "ability");
        kind.ability = strcmp(ability, "-") == 0 ? (u8)CONTENT_ABILITY_COUNT : (u8)content_ability_from_name(ability);
        if (!tsv_cell_u32(table, row, "id", &id) || id != row || !key[0] || strlen(key) >= CONTENT_KEY_SIZE ||
            !tsv_cell_u32(table, row, "merit", &kind.merit) || !tsv_cell_u32(table, row, "soldiers", &kind.soldiers) ||
            !tsv_cell_u32(table, row, "bonus", &kind.bonus) || !tsv_cell_u32(table, row, "rank", &rank) || rank >= content->rank_count ||
            (strcmp(ability, "-") != 0 && kind.ability == CONTENT_ABILITY_COUNT))
        {
            *bad_row = row;
            content->post_count = 0;
            return false;
        }
        kind.id   = (u8)id;
        kind.rank = (u8)rank;
        memcpy(kind.key, key, strlen(key) + 1);
        content->posts[row] = kind;
        content->post_count = row + 1;
    }
    return true;
}

internal u32
content_hex_value(char c)
{
    if (c >= '0' && c <= '9') return (u32)(c - '0');
    if (c >= 'A' && c <= 'F') return (u32)(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (u32)(c - 'a' + 10);
    return 16;
}

internal b32
content_load_items(sim_content *content, tsv_table *table, u32 *bad_row)
{
    content->item_count = 0;
    if (table->row_count > CONTENT_ITEM_CAPACITY) { *bad_row = CONTENT_ITEM_CAPACITY; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        item_kind kind = {};
        u32 id = 0;
        const char *key = tsv_cell(table, row, "key"), *hex = tsv_cell(table, row, "big5");
        kind.type    = (u8)content_find_name(content_item_type_names, ITEM_TYPE_COUNT, tsv_cell(table, row, "type"));
        kind.polearm = (u8)content_find_name(content_polearm_names, POLEARM_COUNT, tsv_cell(table, row, "polearm"));
        u32 length = (u32)strlen(hex);
        b32 bad = !tsv_cell_u32(table, row, "id", &id) || id != row || !key[0] || strlen(key) >= CONTENT_KEY_SIZE ||
                  kind.type == ITEM_TYPE_COUNT || kind.polearm == POLEARM_COUNT || length == 0 || (length & 1) ||
                  length / 2 > CONTENT_BIG5_NAME_SIZE;
        for (u32 at = 0; !bad && at < length; at += 2)
        {
            u32 high = content_hex_value(hex[at]), low = content_hex_value(hex[at + 1]);
            if (high > 15 || low > 15) bad = true;
            else kind.big5[at / 2] = (u8)(high * 16 + low);
        }
        if (bad) { *bad_row = row; content->item_count = 0; return false; }
        kind.big5_size = length / 2;
        kind.id = (u8)id;
        memcpy(kind.key, key, strlen(key) + 1);
        content->items[row] = kind;
        content->item_count = row + 1;
    }
    return true;
}

/* The kind of an item by the Big5 bytes of its name (NUL padded). */
internal u8
content_find_item_kind(sim_content *content, const u8 *big5_name, u32 size)
{
    for (u32 index = 0; index < content->item_count; ++index)
    {
        item_kind *kind = content->items + index;
        if (kind->big5_size > size || memcmp(kind->big5, big5_name, kind->big5_size) != 0) continue;
        if (kind->big5_size == size || big5_name[kind->big5_size] == 0) return (u8)index;
    }
    return CONTENT_NO_ITEM_KIND;
}

/* ---- bases.tsv, events.tsv, ai_priorities.tsv (M8) ----------------------------- */

global_variable const char *content_base_type_names[BASE_TYPE_COUNT] = { "pass", "port" };
global_variable const char *content_event_check_names[EVENT_CHECK_COUNT] = { "quarter", "month", "search", "wait", "none" };
global_variable const char *content_ai_condition_names[AI_CONDITION_COUNT] = {
    "order_below", "troops_below_percent", "free_slot", "free_officer", "morale_below", "weapons_below_troops",
    "loyalty_below", "troops_above", "technique_above", "gold_above",
    "enemy_near", "free_city_near",
};

internal b32
content_load_bases(sim_content *content, tsv_table *table, u32 *bad_row)
{
    content->base_count = 0;
    if (table->row_count != CONTENT_BASE_COUNT) { *bad_row = table->row_count; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        base_kind kind = {};
        u32 id = 0, city = 0;
        const char *key = tsv_cell(table, row, "key");
        kind.type = (u8)content_find_name(content_base_type_names, BASE_TYPE_COUNT, tsv_cell(table, row, "kind"));
        if (!tsv_cell_u32(table, row, "id", &id) || id != CONTENT_FIRST_BASE + row || !key[0] || strlen(key) >= CONTENT_KEY_SIZE ||
            !tsv_cell_u32(table, row, "owner_city", &city) || city >= SAVE_CITY_COUNT || !tsv_cell_u32(table, row, "durability", &kind.durability) ||
            kind.type == BASE_TYPE_COUNT || kind.durability == 0)
        {
            *bad_row = row;
            content->base_count = 0;
            return false;
        }
        kind.id         = (u8)id;
        kind.owner_city = (u8)city;
        memcpy(kind.key, key, strlen(key) + 1);
        content->bases[row] = kind;
        content->base_count = row + 1;
    }
    return true;
}

/* "A8AAA8DF|B0A8...": the Big5 names of an event. */
internal b32
content_parse_event_names(event_kind *kind, const char *text)
{
    kind->name_count = 0;
    if (strcmp(text, "-") == 0) return true;
    u32 size = 0;
    for (const char *at = text;; ++at)
    {
        if (*at == '|' || *at == 0)
        {
            if (size == 0 || kind->name_count >= CONTENT_EVENT_NAMES) return false;
            kind->name_sizes[kind->name_count++] = (u8)size;
            size = 0;
            if (*at == 0) break;
            continue;
        }
        if (!at[1] || at[1] == '|') return false;
        u32 high = content_hex_value(at[0]), low = content_hex_value(at[1]);
        if (high > 15 || low > 15 || size >= sizeof(kind->names[0]) || kind->name_count >= CONTENT_EVENT_NAMES) return false;
        kind->names[kind->name_count][size++] = (u8)(high * 16 + low);
        ++at;
    }
    return true;
}

internal b32
content_load_events(sim_content *content, tsv_table *table, u32 *bad_row)
{
    content->event_count = 0;
    if (table->row_count > CONTENT_EVENT_CAPACITY) { *bad_row = CONTENT_EVENT_CAPACITY; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        event_kind kind = {};
        u32 id = 0;
        const char *key = tsv_cell(table, row, "key"), *months = tsv_cell(table, row, "months");
        kind.check = (u8)content_find_name(content_event_check_names, EVENT_CHECK_COUNT, tsv_cell(table, row, "check"));
        b32 bad = !tsv_cell_u32(table, row, "id", &id) || id != row || !key[0] || strlen(key) >= CONTENT_KEY_SIZE ||
                  !tsv_cell_u32(table, row, "min_days", &kind.min_days) || !tsv_cell_u32(table, row, "chance", &kind.chance) ||
                  kind.check == EVENT_CHECK_COUNT || kind.chance > 100 || !content_parse_event_names(&kind, tsv_cell(table, row, "names"));
        static const char *params[CONTENT_EVENT_PARAMS] = { "p1", "p2", "p3", "p4", "p5", "p6" };
        for (u32 index = 0; index < CONTENT_EVENT_PARAMS && !bad; ++index) bad = !tsv_cell_u32(table, row, params[index], kind.p + index);
        if (!bad && strcmp(months, "-") != 0)
        {
            kind.months = content_month_mask_from_text(months);
            bad = kind.months == 0;
        }
        if (bad) { *bad_row = row; content->event_count = 0; return false; }
        kind.id = (u8)id;
        memcpy(kind.key, key, strlen(key) + 1);
        content->events[row] = kind;
        content->event_count = row + 1;
    }
    return true;
}

internal u8
content_find_event(sim_content *content, const char *key)
{
    for (u32 index = 0; index < content->event_count; ++index)
    {
        if (strcmp(content->events[index].key, key) == 0) return (u8)index;
    }
    return CONTENT_EVENT_CAPACITY;
}

/* The command of a priority row by its key in ai_priorities.tsv. */
internal u8
content_ai_command(const char *name)
{
    static const char *names[]   = { "patrol", "conscript", "develop", "recruit", "train", "produce", "reward", "sortie", "research", "diplomacy" };
    static const u8 commands[]   = { SIM_COMMAND_PATROL, SIM_COMMAND_CONSCRIPT, SIM_COMMAND_DEVELOP, SIM_COMMAND_RECRUIT, SIM_COMMAND_TRAIN,
                                     SIM_COMMAND_PRODUCE, SIM_COMMAND_REWARD, SIM_COMMAND_SORTIE, SIM_COMMAND_RESEARCH, SIM_COMMAND_DIPLOMACY };
    for (u32 index = 0; index < ArrayCount(names); ++index) if (strcmp(names[index], name) == 0) return commands[index];
    return SIM_COMMAND_NONE;
}

internal b32
content_load_priorities(sim_content *content, tsv_table *table, u32 *bad_row)
{
    content->priority_count = 0;
    if (table->row_count > CONTENT_PRIORITY_CAPACITY) { *bad_row = CONTENT_PRIORITY_CAPACITY; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        ai_priority kind = {};
        u32 id = 0;
        const char *key = tsv_cell(table, row, "key");
        kind.command   = content_ai_command(tsv_cell(table, row, "command"));
        kind.condition = (u8)content_find_name(content_ai_condition_names, AI_CONDITION_COUNT, tsv_cell(table, row, "condition"));
        if (!tsv_cell_u32(table, row, "id", &id) || id != row || !key[0] || strlen(key) >= CONTENT_KEY_SIZE ||
            !tsv_cell_u32(table, row, "priority", &kind.priority) || !tsv_cell_u32(table, row, "threshold", &kind.threshold) ||
            kind.command == SIM_COMMAND_NONE || kind.condition == AI_CONDITION_COUNT)
        {
            *bad_row = row;
            content->priority_count = 0;
            return false;
        }
        kind.id = (u8)id;
        memcpy(kind.key, key, strlen(key) + 1);
        content->priorities[row] = kind;
        content->priority_count = row + 1;
    }
    return true;
}

/* ---- M9: the ability chart and the ancient officers ------------------------------ */

global_variable const char *content_chart_kind_names[CHART_KIND_COUNT] = { "plus", "floor", "aptitude", "skill" };
global_variable const char *content_chart_zone_names[CHART_ZONE_COUNT] = { "attack", "plot", "defence", "domestic" };
global_variable const char *content_unit_class_names[OFFICER_UNIT_APTITUDE_COUNT] = { "pike", "halberd", "crossbow", "cavalry", "siege", "navy" };

/* "0|3": the bits of the rows; "-": none. */
internal b32
content_parse_row_mask(const char *text, u32 limit, u32 *mask)
{
    *mask = 0;
    if (strcmp(text, "-") == 0) return true;
    u32 value = 0;
    b32 digits = false;
    for (const char *at = text;; ++at)
    {
        if (*at >= '0' && *at <= '9') { value = value * 10 + (u32)(*at - '0'); digits = true; continue; }
        if ((*at != '|' && *at != 0) || !digits || value >= limit) return false;
        *mask |= 1u << value;
        value  = 0;
        digits = false;
        if (*at == 0) return true;
    }
}

internal b32
content_load_chart(sim_content *content, tsv_table *table, u32 *bad_row)
{
    content->chart_count = 0;
    if (table->row_count > CONTENT_CHART_CAPACITY) { *bad_row = CONTENT_CHART_CAPACITY; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        chart_row kind = {};
        u32 id = 0, amount = 0, count = 0, months = 0, target = 0;
        const char *key = tsv_cell(table, row, "key"), *target_text = tsv_cell(table, row, "target");
        kind.kind = (u8)content_find_name(content_chart_kind_names, CHART_KIND_COUNT, tsv_cell(table, row, "kind"));
        kind.zone = (u8)content_find_name(content_chart_zone_names, CHART_ZONE_COUNT, tsv_cell(table, row, "zone"));
        b32 bad = !tsv_cell_u32(table, row, "id", &id) || id != row || !key[0] || strlen(key) >= CONTENT_KEY_SIZE ||
                  kind.kind == CHART_KIND_COUNT || kind.zone == CHART_ZONE_COUNT ||
                  !tsv_cell_u32(table, row, "amount", &amount) || !tsv_cell_u32(table, row, "count", &count) || !tsv_cell_u32(table, row, "months", &months) ||
                  amount > 100 || count == 0 || count > 255 || months == 0 || months > 24 ||
                  !content_parse_row_mask(tsv_cell(table, row, "requires"), row, &kind.requires);
        if (!bad)
        {
            switch (kind.kind)
            {
                case CHART_PLUS:
                case CHART_FLOOR:    target = content_find_name(content_ability_names, CONTENT_ABILITY_COUNT, target_text); bad = target == CONTENT_ABILITY_COUNT; break;
                case CHART_APTITUDE: target = content_find_name(content_unit_class_names, OFFICER_UNIT_APTITUDE_COUNT, target_text); bad = target == OFFICER_UNIT_APTITUDE_COUNT || amount > 3; break;
                default:             bad = !tsv_cell_u32(table, row, "target", &target) || target == 0 || target > 100; break;
            }
        }
        if (bad) { *bad_row = row; content->chart_count = 0; return false; }
        kind.id     = (u8)id;
        kind.target = (u8)target;
        kind.amount = (u8)amount;
        kind.count  = (u8)count;
        kind.months = (u8)months;
        memcpy(kind.key, key, strlen(key) + 1);
        content->chart[row] = kind;
        content->chart_count = row + 1;
    }
    return true;
}

/* The Big5 hex of one name into a NUL-padded field. */
internal b32
content_parse_big5_field(const char *text, u8 *out, u32 size)
{
    for (u32 index = 0; index < size; ++index) out[index] = 0;
    u32 used = 0;
    for (const char *at = text; *at; at += 2)
    {
        if (!at[1] || used >= size) return false;
        u32 high = content_hex_value(at[0]), low = content_hex_value(at[1]);
        if (high > 15 || low > 15) return false;
        out[used++] = (u8)(high * 16 + low);
    }
    return used > 0;
}

internal b32
content_load_ancients(sim_content *content, tsv_table *table, u32 *bad_row)
{
    static const char *personalities[PERSONALITY_COUNT] = { "careful", "calm", "bold", "reckless" };
    content->ancient_count = 0;
    if (table->row_count > CONTENT_ANCIENT_CAPACITY) { *bad_row = CONTENT_ANCIENT_CAPACITY; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        ancient_officer kind = {};
        u32 id = 0, skill = 0, value = 0;
        const char *key = tsv_cell(table, row, "key"), *sex = tsv_cell(table, row, "sex");
        kind.personality = (u8)content_find_name(personalities, PERSONALITY_COUNT, tsv_cell(table, row, "personality"));
        b32 bad = !tsv_cell_u32(table, row, "id", &id) || id != row || !key[0] || strlen(key) >= CONTENT_KEY_SIZE ||
                  !content_parse_big5_field(tsv_cell(table, row, "family"), kind.family_name, OFFICER_FAMILY_NAME_SIZE) ||
                  !content_parse_big5_field(tsv_cell(table, row, "given"), kind.given_name, OFFICER_GIVEN_NAME_SIZE) ||
                  !tsv_cell_u32(table, row, "skill", &skill) || skill > 100 || kind.personality == PERSONALITY_COUNT ||
                  (strcmp(sex, "male") != 0 && strcmp(sex, "female") != 0);
        for (u32 ability = 0; ability < CONTENT_ABILITY_COUNT && !bad; ++ability)
        {
            bad = !tsv_cell_u32(table, row, content_ability_names[ability], &value) || value == 0 || value > 120;
            kind.abilities[ability] = (u8)value;
        }
        if (bad) { *bad_row = row; content->ancient_count = 0; return false; }
        kind.id     = (u8)id;
        kind.skill  = (u8)skill;
        kind.female = strcmp(sex, "female") == 0;
        memcpy(kind.key, key, strlen(key) + 1);
        content->ancients[row] = kind;
        content->ancient_count = row + 1;
    }
    return true;
}
/* ---- M9: the historical events and the portraits --------------------------------- */

global_variable const char *content_history_conditions[] =
{
    "scenario", "opening", "start_before", "start_by", "start_from", "date_from", "days", "ruler", "faction_of", "ruler_family",
    "rules", "rules_any", "not_rules", "cities", "cities_at_most", "rank", "rank_below", "free", "free_any", "serves", "has",
    "unmarried", "captive", "in_other", "loyalty_below", "merit_at_least", "order_ruler", "order_ruler_below", "order_of",
    "emperor_held", "emperor_is", "no_emperor", "dead", "death_year", "adviser", "officers", "age", "faction_cities",
};

global_variable const char *content_history_effects[] =
{
    "join", "defect", "merit", "merit_like", "loyalty", "loyalty_all", "order_all", "morale_all", "marry", "sworn", "dies",
    "rank", "emperor", "friendliness_all", "friendliness_with", "raise", "adviser",
};

/* True if each clause names a known condition or effect, and each @i names
   an officer of the row. "-" is no clause. */
internal b32
content_history_clauses_valid(const char *text, b32 effects, u32 name_count)
{
    if (strcmp(text, "-") == 0) return effects;   /* a dialogue has no effect; an event without conditions is a bad row */
    const char **names = effects ? content_history_effects : content_history_conditions;
    u32 count = effects ? ArrayCount(content_history_effects) : ArrayCount(content_history_conditions);
    for (const char *at = text; *at;)
    {
        char name[32];
        u32 size = 0;
        while (*at && *at != ':' && *at != ';' && size < sizeof(name) - 1) name[size++] = *at++;
        name[size] = 0;
        if (content_find_name(names, count, name) == count) return false;
        while (*at && *at != ';')
        {
            if (*at == '@')
            {
                u32 index = 0;
                ++at;
                if (*at < '0' || *at > '9') return false;
                while (*at >= '0' && *at <= '9') index = index * 10 + (u32)(*at++ - '0');
                if (index >= name_count) return false;
                continue;
            }
            ++at;
        }
        if (*at == ';') ++at;
    }
    return true;
}

/* "A8E5ADB3|AE4CAB4AB1AC": the Big5 names of a row into fixed fields. */
internal b32
content_parse_names(const char *text, u8 (*names)[OFFICER_FAMILY_NAME_SIZE + OFFICER_GIVEN_NAME_SIZE], u8 *sizes, u32 capacity, u32 *count)
{
    *count = 0;
    if (strcmp(text, "-") == 0) return true;
    u32 size = 0;
    for (const char *at = text;; ++at)
    {
        if (*at == '|' || *at == 0)
        {
            if (size == 0 || *count >= capacity) return false;
            sizes[(*count)++] = (u8)size;
            size = 0;
            if (*at == 0) return true;
            continue;
        }
        if (!at[1] || at[1] == '|' || *count >= capacity || size >= OFFICER_FAMILY_NAME_SIZE + OFFICER_GIVEN_NAME_SIZE) return false;
        u32 high = content_hex_value(at[0]), low = content_hex_value(at[1]);
        if (high > 15 || low > 15) return false;
        names[*count][size++] = (u8)(high * 16 + low);
        ++at;
    }
}

internal b32
content_load_history(sim_content *content, tsv_table *table, u32 *bad_row)
{
    content->history_count = 0;
    if (table->row_count > CONTENT_HISTORY_CAPACITY) { *bad_row = CONTENT_HISTORY_CAPACITY; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        history_kind *kind = content->history + row;
        *kind = {};
        u32 id = 0;
        const char *key = tsv_cell(table, row, "key"), *conditions = tsv_cell(table, row, "conditions"), *effects = tsv_cell(table, row, "effects");
        b32 bad = !tsv_cell_u32(table, row, "id", &id) || id != row || !key[0] || strlen(key) >= CONTENT_KEY_SIZE ||
                  !tsv_cell_u32(table, row, "chance", &kind->chance) || kind->chance > 100 ||
                  strlen(conditions) >= CONTENT_HISTORY_TEXT || strlen(effects) >= CONTENT_HISTORY_TEXT ||
                  !content_parse_names(tsv_cell(table, row, "names"), kind->names, kind->name_sizes, CONTENT_HISTORY_NAMES, &kind->name_count);
        if (!bad) bad = !content_history_clauses_valid(conditions, false, kind->name_count) || !content_history_clauses_valid(effects, true, kind->name_count);
        if (bad) { *bad_row = row; content->history_count = 0; return false; }
        kind->id = (u8)id;
        memcpy(kind->key, key, strlen(key) + 1);
        memcpy(kind->conditions, conditions, strlen(conditions) + 1);
        memcpy(kind->effects, effects, strlen(effects) + 1);
        content->history_count = row + 1;
    }
    return true;
}

internal b32
content_load_portraits(sim_content *content, tsv_table *table, u32 *bad_row)
{
    content->portrait_count = 0;
    if (table->row_count > CONTENT_PORTRAIT_CAPACITY) { *bad_row = CONTENT_PORTRAIT_CAPACITY; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        portrait_kind *kind = content->portraits + row;
        *kind = {};
        u32 id = 0, age = 0, count = 0;
        u8 names[1][OFFICER_FAMILY_NAME_SIZE + OFFICER_GIVEN_NAME_SIZE] = {};
        u8 sizes[1] = {};
        if (!tsv_cell_u32(table, row, "id", &id) || id != row || !tsv_cell_u32(table, row, "age", &age) || age == 0 || age > 120 ||
            !content_parse_names(tsv_cell(table, row, "name"), names, sizes, 1, &count) || count != 1)
        {
            *bad_row = row;
            content->portrait_count = 0;
            return false;
        }
        memcpy(kind->name, names[0], sizeof(kind->name));
        kind->name_size = sizes[0];
        kind->age       = (u8)age;
        content->portrait_count = row + 1;
    }
    return true;
}