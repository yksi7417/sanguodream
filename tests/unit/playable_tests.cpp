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

/* ---- E7: the new clauses of the Europe packs ------------------------------------------------- */

/* The officers of the clause tests have two ASCII letters for a name. The names of a row:
   0 R1, 1 R2, 2 F1, 3 M1, 4 B1 (the Big5 bytes are the same as ASCII). */
#define UNIT_E7_NAMES "5231|5232|4631|4D31|4231"

internal void
unit_name_officer(officer_component *officer, char family, char given)
{
    officer->family_name[0] = (u8)family;
    officer->given_name[0]  = (u8)given;
}

/* A small world in June 1250. Force 3 holds city 4, force 7 holds city 9, force 12 holds city 34 and
   city 5 has no owner. R1 rules force 3 with M1 and B1. R2 rules force 7. F1 is free since 1240. */
internal sim_state *
unit_history_world(memory_arena *arena)
{
    sim_state *sim = unit_turn_sim(arena);
    sim->has_content      = true;
    sim->turns.date       = calendar_make(1250, 6, 1);
    sim->turns.start_date = calendar_make(1200, 1, 1);
    sim->turn             = 100;
    sim_add_city(sim, unit_city(4, 3, 10000));
    sim_add_city(sim, unit_city(5, SIM_NO_FORCE, 0));
    sim_add_city(sim, unit_city(9, 7, 4000));
    sim_add_city(sim, unit_city(34, 12, 6000));
    officer_component ruler = unit_officer(10, 3, OFFICER_STATUS_RULER, 4, 70, 70, 70, 70, 100);
    unit_name_officer(&ruler, 'R', '1');
    sim_add_officer(sim, ruler);
    officer_component other_ruler = unit_officer(11, 7, OFFICER_STATUS_RULER, 9, 60, 60, 60, 60, 100);
    unit_name_officer(&other_ruler, 'R', '2');
    sim_add_officer(sim, other_ruler);
    officer_component free_officer = unit_officer(12, SIM_NO_FORCE, OFFICER_STATUS_FREE_OR_NOT_APPEARED, 4, 60, 80, 50, 50, 0);
    free_officer.appearance_year = 1240;
    unit_name_officer(&free_officer, 'F', '1');
    sim_add_officer(sim, free_officer);
    officer_component general = unit_officer(13, 3, OFFICER_STATUS_REGULAR_OFFICER, 4, 80, 90, 40, 40, 900);
    unit_name_officer(&general, 'M', '1');
    sim_add_officer(sim, general);
    officer_component follower = unit_officer(14, 3, OFFICER_STATUS_REGULAR_OFFICER, 4, 50, 50, 50, 50, 300);
    unit_name_officer(&follower, 'B', '1');
    sim_add_officer(sim, follower);
    return sim;
}

/* Loads the rows of history.tsv text into the content of the sim. The text has its header. */
internal b32
unit_history_rows(sim_state *sim, memory_arena *arena, const char *text)
{
    u64 mark = arena->used;
    tsv_table table = {};
    u32 bad_row = 0;
    b32 loaded = tsv_parse(content_test_text(arena, text), arena, &table) && content_load_history(&sim->content, &table, &bad_row);
    arena->used = mark;
    return loaded;
}

/* 1 if the conditions hold for the force, 0 if they do not, -1 if the row does not load. */
internal s32
unit_history_ready(sim_state *sim, memory_arena *arena, const char *conditions, u8 force)
{
    char rows[512];
    snprintf(rows, sizeof(rows), UNIT_HISTORY_HEADER "0\tone\tx\t100\t%s\t-\tx\t" UNIT_E7_NAMES "\tx\n", conditions);
    if (!unit_history_rows(sim, arena, rows)) return -1;
    return sim_history_ready(sim, 0, force) ? 1 : 0;
}

/* Fires a row with these effects for the force. Returns false if the row does not load. */
internal b32
unit_history_fire(sim_state *sim, memory_arena *arena, const char *effects, u8 force)
{
    char rows[512];
    snprintf(rows, sizeof(rows), UNIT_HISTORY_HEADER "0\tone\tx\t100\tdate_from:1200/1\t%s\tx\t" UNIT_E7_NAMES "\tx\n", effects);
    if (!unit_history_rows(sim, arena, rows)) return false;
    u64 mark = arena->used;
    sim_event_sink sink = sim_sink_from_arena(arena, 64);
    sim->history_turn[0] = 0;
    sim_fire_history(sim, 0, force, 0, &sink);
    arena->used = mark;
    return true;
}

internal void
unit_history_condition_tests(test_counts *counts, memory_arena *arena)
{
    sim_state *sim = unit_history_world(arena);
    TEST_CHECK(counts, unit_history_ready(sim, arena, "date_until:1250/6", 3) == 1 && unit_history_ready(sim, arena, "date_until:1260/1", 3) == 1,
               "date_until holds in the last month of the window and before it");
    TEST_CHECK(counts, unit_history_ready(sim, arena, "date_until:1250/5", 3) == 0 && unit_history_ready(sim, arena, "date_until:1249/12", 3) == 0,
               "date_until fails after the window");
    TEST_CHECK(counts, unit_history_ready(sim, arena, "date_from:1250/6;date_until:1250/6", 3) == 1, "date_from and date_until make a window of one month");

    /* at_war and allied: two factions fight unless they are allies or under a ceasefire. */
    TEST_CHECK(counts, unit_history_ready(sim, arena, "at_war:3:7", 3) == 1 && unit_history_ready(sim, arena, "allied:3:7", 3) == 0,
               "two factions with no relation are at war and not allied");
    sim->diplomacy.relation[3][7] = sim->diplomacy.relation[7][3] = RELATION_ALLIANCE;
    TEST_CHECK(counts, unit_history_ready(sim, arena, "at_war:3:7", 3) == 0 && unit_history_ready(sim, arena, "allied:3:7", 3) == 1 &&
                       unit_history_ready(sim, arena, "allied:7:3", 3) == 1,
               "allies are not at war, and allied has no order");
    sim->diplomacy.relation[3][7] = sim->diplomacy.relation[7][3] = RELATION_CEASEFIRE;
    TEST_CHECK(counts, unit_history_ready(sim, arena, "at_war:3:7", 3) == 0 && unit_history_ready(sim, arena, "allied:3:7", 3) == 0,
               "a ceasefire is no war and no alliance");
    TEST_CHECK(counts, unit_history_ready(sim, arena, "at_war:3:3", 3) == 0, "a faction is not at war with itself");

    /* owner: the owner of a city is in a list of factions, or "-" for no owner. */
    TEST_CHECK(counts, unit_history_ready(sim, arena, "owner:4:3", 7) == 1 && unit_history_ready(sim, arena, "owner:4:7", 7) == 0 &&
                       unit_history_ready(sim, arena, "owner:4:7|3", 7) == 1,
               "owner takes a force id or a list of force ids, whatever the faction of the row");
    TEST_CHECK(counts, unit_history_ready(sim, arena, "owner:5:-", 3) == 1 && unit_history_ready(sim, arena, "owner:4:-", 3) == 0 &&
                       unit_history_ready(sim, arena, "owner:5:3", 3) == 0 && unit_history_ready(sim, arena, "owner:5:3|-", 3) == 1,
               "a dash in the list stands for a city without an owner");
    TEST_CHECK(counts, unit_history_ready(sim, arena, "owner:200:3", 3) == 0, "a city that does not exist has no owner in the list");

    /* church: the church of the faction of the row. */
    sim->content.force_church[3] = CONTENT_CHURCH_MUSLIM;
    sim->content.force_church[7] = CONTENT_CHURCH_LATIN;
    TEST_CHECK(counts, unit_history_ready(sim, arena, "church:muslim", 3) == 1 && unit_history_ready(sim, arena, "church:latin", 3) == 0 &&
                       unit_history_ready(sim, arena, "church:latin", 7) == 1,
               "church tests the church of the faction that the row checks");
    TEST_CHECK(counts, unit_history_ready(sim, arena, "church:latin|greek", 7) == 1 && unit_history_ready(sim, arena, "church:greek|armenian", 7) == 0 &&
                       unit_history_ready(sim, arena, "church:greek", 12) == 0,
               "church takes a list, and a faction with no church matches none");
    TEST_CHECK(counts, unit_history_ready(sim, arena, "church:coptic", 3) == -1, "an unknown church is a bad row");

    /* after: another row of the table has started. */
    TEST_CHECK(counts, unit_history_rows(sim, arena, UNIT_HISTORY_HEADER
                                         "0\tfirst\tx\t100\tdate_from:1200/1\t-\tx\t-\tx\n"
                                         "1\tsecond\tx\t100\tafter:first\t-\tx\t-\tx\n"
                                         "2\tlost\tx\t100\tafter:nothing\t-\tx\t-\tx\n"),
               "a row can name another row by its key");
    sim->history_turn[0] = 0;
    TEST_CHECK(counts, !sim_history_ready(sim, 1, 3), "after fails while the first row has not started");
    unit_history_rows(sim, arena, UNIT_HISTORY_HEADER
                      "0\tfirst\tx\t100\tdate_from:1200/1\t-\tx\t-\tx\n"
                      "1\tsecond\tx\t100\tafter:first\t-\tx\t-\tx\n"
                      "2\tlost\tx\t100\tafter:nothing\t-\tx\t-\tx\n");
    sim->history_turn[0] = sim->turn + 1;
    TEST_CHECK(counts, sim_history_ready(sim, 1, 3) && !sim_history_ready(sim, 2, 3), "after holds after the first row started, and a key with no row never holds");
}

internal void
unit_history_effect_tests(test_counts *counts, memory_arena *arena)
{
    /* seize: an officer of the faction takes the rule, and the old ruler stays as an officer. */
    sim_state *sim = unit_history_world(arena);
    TEST_CHECK(counts, unit_history_fire(sim, arena, "seize:@3", 3) && sim_force_ruler(sim, 3) == 13 &&
                       sim_find_officer(sim, 10)->status == OFFICER_STATUS_REGULAR_OFFICER && sim_find_officer(sim, 10)->force_id == 3 &&
                       sim_find_officer(sim, 13)->force_id == 3,
               "seize gives the rule to the officer and keeps the old ruler in the faction");
    TEST_CHECK(counts, unit_history_fire(sim, arena, "seize:@2", 3) && sim_force_ruler(sim, 3) == 13,
               "seize does nothing for an officer who is not in the faction");
    TEST_CHECK(counts, unit_history_fire(sim, arena, "seize:@1", 3) && sim_force_ruler(sim, 7) == 11 && sim_force_ruler(sim, 3) == 13,
               "seize does nothing for the ruler of another faction");

    /* rename: derived from the rows that started, with no state of its own. */
    sim = unit_history_world(arena);
    sim_event_sink sink = sim_sink_from_arena(arena, 64);
    char key[32] = {};
    TEST_CHECK(counts, !sim_force_rename_key(sim, 3, key, sizeof(key)), "a faction keeps its name until a row renames it");
    TEST_CHECK(counts, unit_history_fire(sim, arena, "rename:@3:mamluk", 3) && sim_force_rename_key(sim, 3, key, sizeof(key)) && strcmp(key, "mamluk") == 0,
               "rename gives the key of the new name to the faction of the officer");
    TEST_CHECK(counts, !sim_force_rename_key(sim, 7, key, sizeof(key)), "rename leaves the other factions");
    sim_officer_dies(sim, sim_find_officer(sim, 13), 0, &sink);
    TEST_CHECK(counts, sim_force_rename_key(sim, 3, key, sizeof(key)), "the name stays after the officer dies");
    char small[4] = {};
    TEST_CHECK(counts, !sim_force_rename_key(sim, 3, small, sizeof(small)), "a key that does not fit gives no name");
    sim->history_turn[0] = 0;
    TEST_CHECK(counts, !sim_force_rename_key(sim, 3, key, sizeof(key)), "a row that has not started renames nothing");

    /* target: a city and a limit of months, from the turn of the row. */
    sim = unit_history_world(arena);
    TEST_CHECK(counts, sim_history_target(sim, 3) == SIM_NO_CITY, "no row means no target");
    TEST_CHECK(counts, unit_history_fire(sim, arena, "target:@4:34:6", 3) && sim_history_target(sim, 3) == 34 && sim_history_target(sim, 7) == SIM_NO_CITY,
               "target gives a city to the faction of the officer");
    u32 limit = 6 * CALENDAR_TURNS_PER_MONTH;
    sim->turn += limit - 1;
    TEST_CHECK(counts, sim_history_target(sim, 3) == 34, "the target stays in the last turn of its months");
    sim->turn += 1;
    TEST_CHECK(counts, sim_history_target(sim, 3) == SIM_NO_CITY, "the target ends after its months");
    sim->turn -= limit;
    sim_officer_dies(sim, sim_find_officer(sim, 14), 0, &sink);
    TEST_CHECK(counts, sim_history_target(sim, 3) == SIM_NO_CITY, "the target ends when its officer dies");

    /* city_loss: a percent of the walls and of the troops. */
    sim = unit_history_world(arena);
    city_component *city = sim_find_city(sim, 34);
    city->durability = 801;
    u32 durability = city->durability;
    TEST_CHECK(counts, unit_history_fire(sim, arena, "city_loss:34:50", 3) && city->troops == 3000 && city->durability == durability / 2,
               "city_loss takes a percent of the troops and of the walls");
    TEST_CHECK(counts, unit_history_fire(sim, arena, "city_loss:34:100", 3) && city->troops == 0 && city->durability == 0, "100 percent takes all");
    TEST_CHECK(counts, unit_history_fire(sim, arena, "city_loss:99:50", 3) && sim_find_city(sim, 4)->troops == 10000, "a city that does not exist loses nothing");
    TEST_CHECK(counts, !unit_history_fire(sim, arena, "city_loss:34:150", 3), "a percent above 100 is a bad row");

    /* order_all and morale_all: a negative value lowers the number and stops at zero. */
    sim = unit_history_world(arena);
    city_component *home = sim_find_city(sim, 4);
    home->public_order = 50;
    home->morale       = 40;
    TEST_CHECK(counts, unit_history_fire(sim, arena, "order_all:-20;morale_all:-15", 3) && home->public_order == 30 && home->morale == 25,
               "a negative value lowers the public order and the morale");
    TEST_CHECK(counts, unit_history_fire(sim, arena, "order_all:-90;morale_all:-90", 3) && home->public_order == 0 && home->morale == 0,
               "a negative value stops at zero");
    TEST_CHECK(counts, unit_history_fire(sim, arena, "order_all:10;morale_all:10", 3) && home->public_order == 10 && home->morale == 10,
               "a positive value still raises the numbers");
    TEST_CHECK(counts, sim_find_city(sim, 9)->public_order == unit_city(9, 7, 4000).public_order,
               "the effects leave the cities of another faction");
}

/* The AI of a faction with a target prefers that city to a nearer city at war. */
internal void
unit_history_target_ai_tests(test_counts *counts, memory_arena *arena)
{
    sim_state *sim = unit_history_world(arena);
    hex_offset unit_offset = { 10, 10 }, near_offset = { 12, 10 }, far_offset = { 40, 10 };
    sim->sites[4].hex  = hex_from_offset(unit_offset);
    sim->sites[9].hex  = hex_from_offset(near_offset);
    sim->sites[34].hex = hex_from_offset(far_offset);
    sim->unit_count = 1;
    sim->units[0] = {};
    sim->units[0].alive = true;
    sim->units[0].force_id = 3;
    sim->units[0].hex = sim->sites[4].hex;
    sim->units[0].soldiers = 10000;   /* U-012: the AI marches only on a city that its strength near it beats */
    sim->content.rules.ai_attack_troop_percent = 100;
    sim->content.rules.ai_target_hexes = 40;           /* the unit counts for the strength near each of the two cities */
    sim_ai_choice choices[AI_GOAL_COUNT];
    sim_ai_unit_goals(sim, 0, choices);
    TEST_CHECK(counts, choices[AI_GOAL_ATTACK_CITY].found && choices[AI_GOAL_ATTACK_CITY].city_id == 9,
               "without a target, the unit goes for the nearest city at war");
    sim_find_city(sim, 9)->troops = 20000;
    sim_ai_unit_goals(sim, 0, choices);
    TEST_CHECK(counts, !choices[AI_GOAL_ATTACK_CITY].found || choices[AI_GOAL_ATTACK_CITY].city_id != 9,
               "U-012: a city with more troops than the strength of the force near it is no goal of the unit");
    sim_find_city(sim, 9)->troops = 4000;
    TEST_CHECK(counts, unit_history_fire(sim, arena, "target:@4:34:6", 3), "the target row loads");
    sim_ai_unit_goals(sim, 0, choices);
    TEST_CHECK(counts, choices[AI_GOAL_ATTACK_CITY].found && choices[AI_GOAL_ATTACK_CITY].city_id == 34,
               "with a target, the unit goes for the target even if it is far");
    sim->turn += 6 * CALENDAR_TURNS_PER_MONTH;
    sim_ai_unit_goals(sim, 0, choices);
    TEST_CHECK(counts, choices[AI_GOAL_ATTACK_CITY].found && choices[AI_GOAL_ATTACK_CITY].city_id == 9,
               "after the months of the target, the unit goes for the nearest city again");
}

internal void
unit_playable_tests(test_counts *counts, memory_arena *arena)
{
    unit_history_condition_tests(counts, arena);
    unit_history_effect_tests(counts, arena);
    unit_history_target_ai_tests(counts, arena);
    unit_history_table_tests(counts, arena);
    unit_chart_table_tests(counts, arena);
    unit_ancient_table_tests(counts, arena);
}
