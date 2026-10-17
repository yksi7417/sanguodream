/* Unit tests of M8 rivals (sim/content.cpp, sim/ai.cpp): the loaders of
   bases.tsv, events.tsv and ai_priorities.tsv, the settings of a corps
   that filter the commands of the AI, and the conditions and urgency of a
   priority row. */

#define UNIT_BASE_HEADER     "id\tkey\tname_zh\tkind\towner_city\tdurability\tstatus\n"
#define UNIT_EVENT_HEADER    "id\tkey\tname_zh\tcheck\tmin_days\tmonths\tchance\tp1\tp2\tp3\tp4\tp5\tp6\tnames\tstatus\n"
#define UNIT_PRIORITY_HEADER "id\tkey\tcommand\tpriority\tcondition\tthreshold\tstatus\n"

internal void
unit_base_table_tests(test_counts *counts, memory_arena *arena)
{
    char *text = (char *)arena_push_size(arena, 8192);
    u32 used = (u32)snprintf(text, 8192, "%s", UNIT_BASE_HEADER);
    for (u32 row = 0; row < CONTENT_BASE_COUNT; ++row)
    {
        used += (u32)snprintf(text + used, 8192 - used, "%u\tb%u\tx\t%s\t%u\t%u\tx\n", CONTENT_FIRST_BASE + row, row, row < 10 ? "pass" : "port", row % SAVE_CITY_COUNT, 1000 + row);
    }
    sim_content content = {};
    tsv_table table = {};
    u32 bad_row = 0;
    TEST_CHECK(counts, tsv_parse(text, arena, &table) && content_load_bases(&content, &table, &bad_row) && content.base_count == CONTENT_BASE_COUNT &&
                       content.bases[0].id == 42 && content.bases[0].type == BASE_PASS && content.bases[10].type == BASE_PORT &&
                       content.bases[44].id == 86 && content.bases[44].durability == 1044,
               "bases.tsv: 45 rows, the location ids 42 to 86, the kind, the owner city and the durability");
    tsv_table short_table = {};
    char *short_text = content_test_text(arena, UNIT_BASE_HEADER "42\tb\tx\tpass\t0\t1000\tx\n");
    TEST_CHECK(counts, tsv_parse(short_text, arena, &short_table) && !content_load_bases(&content, &short_table, &bad_row) && content.base_count == 0,
               "a table without all 45 bases does not load");
}

internal void
unit_event_table_tests(test_counts *counts, memory_arena *arena)
{
    sim_content content = {};
    tsv_table table = {};
    u32 bad_row = 0;
    char *text = content_test_text(arena, UNIT_EVENT_HEADER
                                   "0\ttreason\tx\tquarter\t360\t1|4|7|10\t30\t8\t2\t5\t10\t20\t0\t-\tx\n"
                                   "1\tvisit_a\tx\tsearch\t0\t-\t100\t90\t100\t0\t0\t0\t0\tBADEE078|A4A8\tx\n");
    TEST_CHECK(counts, tsv_parse(text, arena, &table) && content_load_events(&content, &table, &bad_row) && content.event_count == 2 &&
                       content.events[0].check == EVENT_CHECK_QUARTER && content.events[0].months == ((1u << 1) | (1u << 4) | (1u << 7) | (1u << 10)) &&
                       content.events[0].p[4] == 20 && content.events[1].name_count == 2 && content.events[1].name_sizes[0] == 4 &&
                       content.events[1].names[0][0] == 0xBA && content.events[1].names[1][1] == 0xA8,
               "events.tsv: the check, the months of a quarter check, the six numbers and the Big5 names");
    TEST_CHECK(counts, content_find_event(&content, "visit_a") == 1 && content_find_event(&content, "none") == CONTENT_EVENT_CAPACITY, "an event is found by its key");
    tsv_table bad = {};
    char *bad_text = content_test_text(arena, UNIT_EVENT_HEADER "0\tx\tx\tmonth\t0\t-\t101\t0\t0\t0\t0\t0\t0\t-\tx\n");
    TEST_CHECK(counts, tsv_parse(bad_text, arena, &bad) && !content_load_events(&content, &bad, &bad_row) && bad_row == 0, "a chance above 100 is a bad row");
    tsv_table odd = {};
    char *odd_text = content_test_text(arena, UNIT_EVENT_HEADER "0\tx\tx\tsearch\t0\t-\t100\t0\t0\t0\t0\t0\t0\tBAD\tx\n");
    TEST_CHECK(counts, tsv_parse(odd_text, arena, &odd) && !content_load_events(&content, &odd, &bad_row), "a Big5 name with an odd number of hex digits is a bad row");
}

internal void
unit_priority_table_tests(test_counts *counts, memory_arena *arena)
{
    sim_content content = {};
    tsv_table table = {};
    u32 bad_row = 0;
    char *text = content_test_text(arena, UNIT_PRIORITY_HEADER "0\tpatrol\tpatrol\t60\torder_below\t70\tx\n1\tgoodwill\tdiplomacy\t10\tgold_above\t8000\tx\n");
    TEST_CHECK(counts, tsv_parse(text, arena, &table) && content_load_priorities(&content, &table, &bad_row) && content.priority_count == 2 &&
                       content.priorities[0].command == SIM_COMMAND_PATROL && content.priorities[0].condition == AI_ORDER_BELOW &&
                       content.priorities[0].threshold == 70 && content.priorities[1].command == SIM_COMMAND_DIPLOMACY,
               "ai_priorities.tsv: the command, the priority and the condition with its threshold");
    tsv_table bad = {};
    char *bad_text = content_test_text(arena, UNIT_PRIORITY_HEADER "0\tx\tdance\t60\torder_below\t70\tx\n");
    TEST_CHECK(counts, tsv_parse(bad_text, arena, &bad) && !content_load_priorities(&content, &bad, &bad_row) && bad_row == 0, "a command that the planner does not know is a bad row");
}

/* The settings of a corps and the conditions of the planner. */
internal void
unit_planner_tests(test_counts *counts, memory_arena *arena)
{
    TEST_CHECK(counts, sim_ai_allowed(SIM_COMMAND_PATROL, 0) && !sim_ai_allowed(SIM_COMMAND_SORTIE, 0) && sim_ai_allowed(SIM_COMMAND_SORTIE, CORPS_FLAG_ATTACK) &&
                       !sim_ai_allowed(SIM_COMMAND_RECRUIT, CORPS_FLAG_ALL & ~CORPS_FLAG_RECRUIT) && !sim_ai_allowed(SIM_COMMAND_DEVELOP, CORPS_FLAG_SAVE_GOLD),
               "a corps setting filters the commands: no 攻擊 without the flag, no 開發 while it saves gold; 巡察 always");
    sim_state *sim = PushStruct(arena, sim_state);
    *sim = {};
    sim->content.rules.ai_urgency = 20;
    city_component city = {};
    city.public_order = 35;
    city.troops = 1000;
    city.morale = 70;
    city.gold   = 9000;
    ai_priority row = {};
    row.condition = AI_ORDER_BELOW;
    row.threshold = 70;
    u32 urgency = 0;
    TEST_CHECK(counts, sim_ai_condition(sim, &city, &row, &urgency) && urgency == 10, "public order 35 under 70: the condition holds with half the urgency (10 of 20)");
    city.public_order = 70;
    TEST_CHECK(counts, !sim_ai_condition(sim, &city, &row, &urgency), "public order at the threshold: no proposal");
    row.condition = AI_MORALE_BELOW;
    TEST_CHECK(counts, !sim_ai_condition(sim, &city, &row, &urgency), "morale 70 is not below 70");
    city.troops = 0;
    city.morale = 0;
    TEST_CHECK(counts, !sim_ai_condition(sim, &city, &row, &urgency), "a city without soldiers does not train");
    row.condition = AI_GOLD_ABOVE;
    row.threshold = 8000;
    TEST_CHECK(counts, sim_ai_condition(sim, &city, &row, &urgency) && urgency == 0, "gold above the threshold: the condition holds without urgency");
}

internal void
unit_rivals_tests(test_counts *counts, memory_arena *arena)
{
    unit_base_table_tests(counts, arena);
    unit_event_table_tests(counts, arena);
    unit_priority_table_tests(counts, arena);
    unit_planner_tests(counts, arena);
}
