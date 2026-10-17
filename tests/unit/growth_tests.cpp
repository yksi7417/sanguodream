/* Unit tests of the M4 growth rules (research-techniques.md,
   calendar-and-turn.md "Disasters", cities-and-facilities.md "Merge"): the
   parsers of techniques.tsv and disasters.tsv, and the pure functions of
   the research period, the research gold, the merge period and the chance
   of a disaster. No engine, no files. */

#define UNIT_TECHNIQUE_HEADER "id\tkey\tline\tlevel\tpoints\tgold\tability\tmin_days\tmax_days\tperiod_status\teffect\tunit_class\tvalue\tvalue_status\n"
#define UNIT_DISASTER_HEADER  "id\tkey\tmonths\tchance_percent\tturns\tstart_order_loss\tturn_order_loss\tgrain_percent\tsoldier_percent\t" \
                              "facility_damage\tfacility_target\tlow_order_only\tends_at_order\tskill_block\tskill_chance\tskill_chance_percent\n"

internal void
unit_technique_table_tests(test_counts *counts, memory_arena *arena)
{
    sim_content content = {};
    tsv_table table = {};
    char *text = content_test_text(arena, UNIT_TECHNIQUE_HEADER
                                   "0\tpike_drill\tpike\t1\t1000\t1000\tleadership\t40\t60\tdocumented\ttactic_damage\tpike\t0\tnone\n"
                                   "1\tgrain_raid\tpike\t2\t2000\t2000\tleadership\t50\t70\tdocumented\traid_grain\tpike\t0\tnone\n"
                                   "2\tveterans\tdrill\t1\t1000\t1000\tforce\t40\t60\tdocumented\tmorale_cap\tnone\t20\tdocumented\n");
    u32 bad_row = 0;
    TEST_CHECK(counts, tsv_parse(text, arena, &table) && content_load_techniques(&content, &table, &bad_row) && content.technique_count == 3,
               "the technique table parses");
    technique_kind *raid = content.techniques + 1;
    TEST_CHECK(counts, raid->line == TECHNIQUE_LINE_PIKE && raid->level == 2 && raid->points == 2000 && raid->gold == 2000 &&
                       raid->ability == CONTENT_ABILITY_LEADERSHIP && raid->min_days == 50 && raid->max_days == 70 &&
                       raid->effect == TECHNIQUE_EFFECT_RAID_GRAIN && raid->unit_class == CONTENT_CLASS_PIKE,
               "a row has its line, level, cost, ability, period range, effect and class");
    TEST_CHECK(counts, content.techniques[2].value == 20 && content.techniques[2].ability == CONTENT_ABILITY_FORCE,
               "the veterans technique raises the morale cap by 20, shortened by force");
    TEST_CHECK(counts, sim_previous_technique(&content, 1) == 0 && sim_previous_technique(&content, 0) == CONTENT_NO_TECHNIQUE &&
                       sim_previous_technique(&content, 2) == CONTENT_NO_TECHNIQUE && sim_previous_technique(&content, 9) == CONTENT_NO_TECHNIQUE,
               "level 2 needs level 1 of its line; level 1 needs nothing");
    TEST_CHECK(counts, content_find_technique(&content, "veterans") == 2 && content_find_technique(&content, "magic") == CONTENT_NO_TECHNIQUE,
               "a technique has a key");

    tsv_table gap = {};
    char *gap_text = content_test_text(arena, UNIT_TECHNIQUE_HEADER
                                       "0\tgrain_raid\tpike\t2\t2000\t2000\tleadership\t50\t70\tdocumented\traid_grain\tpike\t0\tnone\n");
    TEST_CHECK(counts, tsv_parse(gap_text, arena, &gap) && !content_load_techniques(&content, &gap, &bad_row) && bad_row == 0 &&
                       content.technique_count == 0, "a level without the level below it in an earlier row is a bad row");
    tsv_table bad_line = {};
    char *bad_line_text = content_test_text(arena, UNIT_TECHNIQUE_HEADER
                                            "0\tx\tmagic\t1\t1000\t1000\tleadership\t40\t60\tdocumented\tnone\tnone\t0\tnone\n");
    TEST_CHECK(counts, tsv_parse(bad_line_text, arena, &bad_line) && !content_load_techniques(&content, &bad_line, &bad_row),
               "a line name that is not a line is a bad row");
    tsv_table bad_period = {};
    char *bad_period_text = content_test_text(arena, UNIT_TECHNIQUE_HEADER
                                              "0\tx\tpike\t1\t1000\t1000\tleadership\t60\t40\tdocumented\tnone\tnone\t0\tnone\n");
    TEST_CHECK(counts, tsv_parse(bad_period_text, arena, &bad_period) && !content_load_techniques(&content, &bad_period, &bad_row),
               "a shortest period above the longest is a bad row");
}

internal void
unit_disaster_table_tests(test_counts *counts, memory_arena *arena)
{
    TEST_CHECK(counts, content_month_mask_from_text("1|4|7") == ((1u << 1) | (1u << 4) | (1u << 7)) &&
                       content_month_mask_from_text("12") == (1u << 12),
               "a month list gives one bit for each month");
    TEST_CHECK(counts, content_month_mask_from_text("0") == 0 && content_month_mask_from_text("13") == 0 &&
                       content_month_mask_from_text("1||4") == 0 && content_month_mask_from_text("") == 0 &&
                       content_month_mask_from_text("a") == 0,
               "a month out of 1..12, an empty part or a letter gives no mask");

    sim_content content = {};
    tsv_table table = {};
    char *text = content_test_text(arena, UNIT_DISASTER_HEADER
                                   "0\tlocusts\t1|4|7\t5\t9\t10\t0\t50\t0\t200\tgrain\tno\tno\t97\t0\t100\n"
                                   "1\tbandits\t1|2|3\t10\t0\t0\t2\t0\t0\t300\tany\tyes\tyes\t0\t95\t50\n");
    u32 bad_row = 0;
    TEST_CHECK(counts, tsv_parse(text, arena, &table) && content_load_disasters(&content, &table, &bad_row) && content.disaster_count == 2,
               "the disaster table parses");
    disaster_kind *locusts = content.disasters + 0;
    disaster_kind *bandits = content.disasters + 1;
    TEST_CHECK(counts, sim_disaster_month(locusts, 4) && !sim_disaster_month(locusts, 10) && !sim_disaster_month(locusts, 0) &&
                       !sim_disaster_month(locusts, 13), "locusts can start in months 1, 4 and 7 only");
    TEST_CHECK(counts, locusts->facility_target == FACILITY_EFFECT_GRAIN && bandits->facility_target == CONTENT_TARGET_ANY &&
                       bandits->low_order_only && bandits->ends_at_order && !locusts->ends_at_order && locusts->skill_block == 97,
               "a row has its target, its order rules and its skills");
    TEST_CHECK(counts, sim_disaster_chance(locusts, false) == 5 && sim_disaster_chance(bandits, false) == 10 &&
                       sim_disaster_chance(bandits, true) == 5, "the intimidation skill halves the chance of bandits");
    disaster_kind harvest = {};
    harvest.chance_percent = 60;
    harvest.skill_chance = 98;
    harvest.skill_chance_percent = 200;
    TEST_CHECK(counts, sim_disaster_chance(&harvest, true) == 100 && sim_disaster_chance(&harvest, false) == 60,
               "the prayer skill doubles the chance of a harvest, up to 100");

    tsv_table bad = {};
    char *bad_text = content_test_text(arena, UNIT_DISASTER_HEADER
                                       "0\tx\t1\t101\t0\t0\t0\t0\t0\t0\tnone\tno\tno\t0\t0\t100\n");
    TEST_CHECK(counts, tsv_parse(bad_text, arena, &bad) && !content_load_disasters(&content, &bad, &bad_row) && content.disaster_count == 0,
               "a chance above 100 is a bad row");
    tsv_table bad_target = {};
    char *bad_target_text = content_test_text(arena, UNIT_DISASTER_HEADER
                                              "0\tx\t1\t5\t0\t0\t0\t0\t0\t0\twalls\tno\tno\t0\t0\t100\n");
    TEST_CHECK(counts, tsv_parse(bad_target_text, arena, &bad_target) && !content_load_disasters(&content, &bad_target, &bad_row),
               "a target that is not an effect, any or none is a bad row");
}

internal void
unit_growth_rule_tests(test_counts *counts)
{
    sim_rules rules = {};
    rules.research_full_ability      = 200;
    rules.research_personnel_percent = 75;
    rules.research_teaching_percent  = 50;
    rules.merge_max_days             = 60;
    rules.merge_min_days             = 20;
    rules.merge_full_politics        = 100;

    TEST_CHECK(counts, sim_research_days(0, 40, 60, false, &rules) == 60 && sim_research_days(200, 40, 60, false, &rules) == 40 &&
                       sim_research_days(400, 40, 60, false, &rules) == 40,
               "research: the longest period at ability 0, the shortest at the full sum and above (assumed line)");
    TEST_CHECK(counts, sim_research_days(100, 40, 60, false, &rules) == 50 && sim_research_days(100, 100, 120, false, &rules) == 110,
               "research: half the full sum gives the middle of the range");
    TEST_CHECK(counts, sim_research_days(150, 40, 60, false, &rules) == 50,
               "research: the period goes up to whole turns (45 days are 5 turns)");
    TEST_CHECK(counts, sim_research_days(200, 40, 60, true, &rules) == 30 && sim_research_days(0, 30, 50, true, &rules) == 40,
               "research: a personnel office gives 75 percent of the period, up to whole turns");
    TEST_CHECK(counts, sim_research_gold(5000, true, &rules) == 2500 && sim_research_gold(5000, false, &rules) == 5000,
               "research: the teaching skill halves the gold");
    TEST_CHECK(counts, sim_merge_days(0, &rules) == 60 && sim_merge_days(100, &rules) == 20 && sim_merge_days(50, &rules) == 40 &&
                       sim_merge_days(255, &rules) == 20, "merge: politics shortens the period from 60 to 20 days (assumed line)");
    sim_rules empty = {};
    TEST_CHECK(counts, sim_research_days(100, 40, 60, false, &empty) == 60 && sim_round_to_turns(0) == 10,
               "without the rules the period is the longest; a period is at least one turn");
}

internal void
unit_growth_tests(test_counts *counts, memory_arena *arena)
{
    unit_technique_table_tests(counts, arena);
    unit_disaster_table_tests(counts, arena);
    unit_growth_rule_tests(counts);
}
