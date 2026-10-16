/* Unit tests of code/sim/tables.h (the TSV parser) and code/sim/content.h
   (the facility, city and rule tables). The text is synthetic; the
   feature tests read the real files of the content pack. */

/* tsv_parse cuts the text in place, so each test copies its text first. */
internal char *
content_test_text(memory_arena *arena, const char *text)
{
    u64 size = strlen(text) + 1;
    char *copy = (char *)arena_push_size(arena, size);
    memcpy(copy, text, size);
    return copy;
}

internal void
unit_tables_tests(test_counts *counts, memory_arena *arena)
{
    tsv_table table = {};
    char *text = content_test_text(arena, "# a comment before the header\r\n"
                                          "id\tname\tvalue\r\n"
                                          "\r\n"
                                          "0\tfirst\t12\r\n"
                                          "# a comment between rows\n"
                                          "1\tsecond\n"
                                          "2\tthird\t4294967295");
    TEST_CHECK(counts, tsv_parse(text, arena, &table), "a table with a header parses");
    TEST_CHECK(counts, table.column_count == 3 && table.row_count == 3, "comments and empty lines are not rows");
    TEST_CHECK(counts, tsv_column(&table, "value") == 2 && tsv_column(&table, "missing") == -1, "columns are found by name");
    TEST_CHECK(counts, strcmp(tsv_cell(&table, 0, "name"), "first") == 0, "a cell is found by row and column");
    TEST_CHECK(counts, tsv_cell(&table, 1, "value")[0] == 0, "a missing cell is empty");
    TEST_CHECK(counts, tsv_cell(&table, 0, "missing")[0] == 0 && tsv_cell(&table, 9, "name")[0] == 0,
               "a missing column or row gives an empty cell");
    u32 value = 0;
    TEST_CHECK(counts, tsv_cell_u32(&table, 0, "value", &value) && value == 12, "a decimal cell parses");
    TEST_CHECK(counts, tsv_cell_u32(&table, 2, "value", &value) && value == U32_MAX, "U32_MAX parses");
    TEST_CHECK(counts, !tsv_parse_u32("4294967296", &value) && !tsv_parse_u32("12a", &value) &&
                       !tsv_parse_u32("-1", &value) && !tsv_parse_u32("", &value),
               "an overflow, a letter, a sign and an empty cell do not parse");

    tsv_table empty = {};
    TEST_CHECK(counts, !tsv_parse(content_test_text(arena, "# only a comment\n\n"), arena, &empty),
               "a text without a header line does not parse");
}

global_variable const char *content_test_facilities =
    "id\tkey\tcost\tdurability\tmin_days\tperiod_status\teffect\tvalue\tvalue_status\tlimit\tneeds_item\tadjacency_bonus\tnew_year_removal\tpk\n"
    "0\tmarket\t200\t500\t20\tdocumented\tgold\t50\tassumed\tnone\tno\tyes\tno\tno\n"
    "1\tmint\t400\t1000\t30\tdocumented\tmint\t0\tnone\tnone\tno\tno\tno\tno\n"
    "2\tblack_market\t50\t200\t20\tassumed\tgold\t40\tassumed\tnone\tno\tno\tyes\tyes\n"
    "3\ttally_office\t200\t450\t30\tassumed\taction_points\t0\tnone\tcity\tno\tno\tno\tyes\n";

internal void
unit_content_tests(test_counts *counts, memory_arena *arena)
{
    sim_content content = {};
    u32 bad_row = 0;
    tsv_table table = {};
    tsv_parse(content_test_text(arena, content_test_facilities), arena, &table);
    TEST_CHECK(counts, content_load_facilities(&content, &table, &bad_row) && content.facility_count == 4,
               "the facility table parses one kind for each row");
    facility_kind *market = content.facilities + 0;
    TEST_CHECK(counts, strcmp(market->key, "market") == 0 && market->cost == 200 && market->durability == 500 &&
                       market->min_days == 20, "a facility has its key, cost, durability and shortest period");
    TEST_CHECK(counts, market->effect == FACILITY_EFFECT_GOLD && market->value == 50 && market->value_assumed &&
                       market->adjacency_bonus && !market->pk, "a facility has its effect, value and flags");
    TEST_CHECK(counts, content.facilities[1].effect == FACILITY_EFFECT_MINT && !content.facilities[1].value_assumed,
               "the mint has the mint effect");
    TEST_CHECK(counts, content.facilities[2].new_year_removal && content.facilities[2].period_assumed &&
                       content.facilities[2].pk, "the black market is removed on 1 January");
    TEST_CHECK(counts, content.facilities[3].limit == FACILITY_LIMIT_CITY, "the tally office has a limit of one in each city");

    tsv_table bad = {};
    tsv_parse(content_test_text(arena, "id\tkey\tcost\tdurability\tmin_days\teffect\tvalue\tlimit\n"
                                       "0\tmarket\t200\t500\t20\tgold\t50\tnone\n"
                                       "2\tfarm\t200\t500\t20\tgrain\t250\tnone\n"), arena, &bad);
    TEST_CHECK(counts, !content_load_facilities(&content, &bad, &bad_row) && bad_row == 1 && content.facility_count == 0,
               "an id out of order refuses the table and names the row");
    tsv_parse(content_test_text(arena, "id\tkey\tcost\tdurability\tmin_days\teffect\tvalue\tlimit\n"
                                       "0\tmarket\t200\t500\t20\tgold_coins\t50\tnone\n"), arena, &bad);
    TEST_CHECK(counts, !content_load_facilities(&content, &bad, &bad_row) && bad_row == 0,
               "an unknown effect refuses the table");

    tsv_table cities = {};
    tsv_parse(content_test_text(arena, "id\tspecialty\tslots\tport\n0\thorse\t12\tyes\n1\tbig_city\t20\tno\n"), arena, &cities);
    TEST_CHECK(counts, content_load_cities(&content, &cities, &bad_row) && content.city_count == 2,
               "the city table parses one city for each row");
    TEST_CHECK(counts, content.cities[0].specialty == CITY_SPECIALTY_HORSE && content.cities[0].slot_count == 12 &&
                       content.cities[0].has_port, "a city has its specialty, slots and port");
    TEST_CHECK(counts, content.cities[1].specialty == CITY_SPECIALTY_BIG_CITY && !content.cities[1].has_port,
               "a big city without a port");
    tsv_parse(content_test_text(arena, "id\tspecialty\tslots\n0\tsword\t12\n"), arena, &cities);
    TEST_CHECK(counts, !content_load_cities(&content, &cities, &bad_row) && content.city_count == 0,
               "an unknown specialty refuses the city table");

    tsv_table rules = {};
    tsv_parse(content_test_text(arena, "key\tvalue\tstatus\nap.cap\t255\tdocumented\nap.develop\t20\tdocumented\n"), arena, &rules);
    TEST_CHECK(counts, content_load_rules(&content, &rules, &bad_row) && content.rules.ap_cap == 255 &&
                       content.rules.ap_develop == 20 && content.rules.loaded_count == 2,
               "the rule table sets the named fields");
    TEST_CHECK(counts, content.rules.officer_salary == 0, "a rule without a row stays 0");
    tsv_parse(content_test_text(arena, "key\tvalue\nap.capp\t255\n"), arena, &rules);
    TEST_CHECK(counts, !content_load_rules(&content, &rules, &bad_row) && bad_row == 0 && content.rules.ap_cap == 0,
               "an unknown rule key refuses the table");
}
