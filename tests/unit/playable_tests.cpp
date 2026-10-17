/* Unit tests of M9 (sim/content.cpp): the loaders of ability_chart.tsv and
   ancient_officers.tsv. */

#define UNIT_CHART_HEADER   "id\tkey\tname_zh\tzone\tkind\ttarget\tamount\tcount\tmonths\trequires\tstatus\n"
#define UNIT_ANCIENT_HEADER "id\tkey\tname_zh\tfamily\tgiven\tsex\tleadership\tforce\tintellect\tpolitics\tcharm\tskill\tpersonality\tstatus\n"

internal void
unit_chart_table_tests(test_counts *counts, memory_arena *arena)
{
    sim_content content = {};
    tsv_table table = {};
    u32 bad_row = 0;
    char *text = content_test_text(arena, UNIT_CHART_HEADER
                                   "0\tl5\tx\tattack\tplus\tleadership\t5\t3\t3\t-\tx\n"
                                   "1\tpike_b\tx\tattack\taptitude\tpike\t1\t5\t3\t-\tx\n"
                                   "2\twall\tx\tdefence\tskill\t47\t0\t1\t6\t0|1\tx\n");
    TEST_CHECK(counts, tsv_parse(text, arena, &table) && content_load_chart(&content, &table, &bad_row) && content.chart_count == 3 &&
                       content.chart[0].kind == CHART_PLUS && content.chart[0].target == CONTENT_ABILITY_LEADERSHIP && content.chart[0].count == 3 &&
                       content.chart[1].kind == CHART_APTITUDE && content.chart[1].target == 0 && content.chart[1].amount == 1 &&
                       content.chart[2].target == 47 && content.chart[2].requires == 3u && content.chart[2].zone == CHART_ZONE_DEFENCE,
               "ability_chart.tsv: the kind, the target by name or number, the count and the rows on the arrows");
    tsv_table forward = {};
    char *forward_text = content_test_text(arena, UNIT_CHART_HEADER "0\tx\tx\tattack\tplus\tforce\t5\t3\t3\t1\tx\n");
    TEST_CHECK(counts, tsv_parse(forward_text, arena, &forward) && !content_load_chart(&content, &forward, &bad_row) && bad_row == 0,
               "a row can only need rows before it");
    tsv_table grade = {};
    char *grade_text = content_test_text(arena, UNIT_CHART_HEADER "0\tx\tx\tattack\taptitude\tcavalry\t4\t5\t3\t-\tx\n");
    TEST_CHECK(counts, tsv_parse(grade_text, arena, &grade) && !content_load_chart(&content, &grade, &bad_row), "an aptitude above S is a bad row");
}

internal void
unit_ancient_table_tests(test_counts *counts, memory_arena *arena)
{
    sim_content content = {};
    tsv_table table = {};
    u32 bad_row = 0;
    char *text = content_test_text(arena, UNIT_ANCIENT_HEADER "0\txiang_yu\tx\tB6B5\tA6D0\tmale\t100\t100\t55\t40\t85\t11\treckless\tx\n"
                                                           "1\txi_shi\tx\tA6E8\tAC49\tfemale\t40\t20\t70\t75\t100\t0\tcareful\tx\n");
    TEST_CHECK(counts, tsv_parse(text, arena, &table) && content_load_ancients(&content, &table, &bad_row) && content.ancient_count == 2 &&
                       content.ancients[0].family_name[0] == 0xB6 && content.ancients[0].given_name[1] == 0xD0 && content.ancients[0].abilities[1] == 100 &&
                       content.ancients[0].personality == PERSONALITY_RECKLESS && content.ancients[1].female && content.ancients[1].skill == 0,
               "ancient_officers.tsv: the Big5 name, the five abilities, the skill, the sex and the personality");
    tsv_table bad = {};
    char *bad_text = content_test_text(arena, UNIT_ANCIENT_HEADER "0\tx\tx\tB6B\tA6D0\tmale\t1\t1\t1\t1\t1\t0\tcalm\tx\n");
    TEST_CHECK(counts, tsv_parse(bad_text, arena, &bad) && !content_load_ancients(&content, &bad, &bad_row), "a name with an odd number of hex digits is a bad row");
}

#define UNIT_HISTORY_HEADER "id\tkey\tname_zh\tchance\tconditions\teffects\tnames_zh\tnames\tstatus\n"

internal void
unit_history_table_tests(test_counts *counts, memory_arena *arena)
{
    sim_content content = {};
    tsv_table table = {};
    u32 bad_row = 0;
    char *text = content_test_text(arena, UNIT_HISTORY_HEADER
                                   "0\tzhao_yun\tx\t100\tdays:30;ruler:@0;free:@1;order_ruler:100\tjoin:@1:100:4000\tx\tBC42B3C6|BBAFB6B3\tx\n"
                                   "1\topening\tx\t100\tscenario:1;opening\t-\tx\t-\tx\n");
    TEST_CHECK(counts, tsv_parse(text, arena, &table) && content_load_history(&content, &table, &bad_row) && content.history_count == 2 &&
                       content.history[0].name_count == 2 && content.history[0].name_sizes[1] == 4 && content.history[0].names[1][0] == 0xBB &&
                       strcmp(content.history[0].effects, "join:@1:100:4000") == 0 && content.history[1].name_count == 0,
               "history.tsv: the clauses stay text for the interpreter; the names are Big5 bytes");
    TEST_CHECK(counts, content_history_clauses_valid("rules:22|23;has:@1", false, 2) && !content_history_clauses_valid("dance:3", false, 2) &&
                       !content_history_clauses_valid("has:@2", false, 2) && !content_history_clauses_valid("ruler:@0", true, 1) &&
                       content_history_clauses_valid("-", true, 0) && !content_history_clauses_valid("-", false, 0),
               "a clause must be a known condition or effect, and each @i an officer of the row");
}

internal void
unit_playable_tests(test_counts *counts, memory_arena *arena)
{
    unit_history_table_tests(counts, arena);
    unit_chart_table_tests(counts, arena);
    unit_ancient_table_tests(counts, arena);
}
