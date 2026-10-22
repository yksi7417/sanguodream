/* Unit tests of M7 (officers-and-relations.md, factions-corps-and-posts.md,
   skills.md, debate.md): the parsers of skills.tsv, ranks.tsv, posts.tsv and
   items.tsv, the distance of compatibility, the hand size and the strength
   order of the debate cards. No engine, no files. */

#define UNIT_SKILL_HEADER "number\tkey\tname_zh\tcategory\teffect\tapplies\tnote\tfile_id\n"
#define UNIT_RANK_HEADER  "id\tkey\tname_zh\tsoldiers\tcities\tneeds_emperor\tstatus\n"
#define UNIT_POST_HEADER  "id\tkey\tname_zh\tmerit\tsoldiers\tability\tbonus\trank\tstatus\n"
#define UNIT_ITEM_HEADER  "id\tkey\tname_zh\tbig5\ttype\tpolearm\tstatus\n"

/* 100 rows; row 19 is 捕縛 with the rule hook "capture", row 7 a table skill.
   The file id of the number n is 100 - n: the order of the file is not the
   order of the book. */
internal char *
unit_skill_text(memory_arena *arena, const char *bad_row_19)
{
    u32 size = 100 * 64 + 256;
    char *text = (char *)arena_push_size(arena, size);
    u32 used = (u32)snprintf(text, size, "%s", UNIT_SKILL_HEADER);
    for (u32 number = 1; number <= 100; ++number)
    {
        const char *row = "%u\tskill_%u\tx\tattack\t-\tnone\tx\t%u\n";
        if (number == 19) row = bad_row_19 ? bad_row_19 : "%u\tcapture_%u\tx\tattack\tcapture\trule\tx\t%u\n";
        if (number == 7)  row = "%u\ttrail_%u\tx\tmovement\t-\ttable\tx\t%u\n";
        used += (u32)snprintf(text + used, size - used, row, number, number, 100 - number);
    }
    return text;
}

internal void
unit_skill_table_tests(test_counts *counts, memory_arena *arena)
{
    sim_content content = {};
    tsv_table table = {};
    u32 bad_row = 0;
    TEST_CHECK(counts, tsv_parse(unit_skill_text(arena, 0), arena, &table) && content_load_skills(&content, &table, &bad_row) &&
                       content.skill_count == 100 && content.rules.skill_capture == 19 && content.skills[18].hooked &&
                       content.skills[6].hooked && !content.skills[0].hooked && content.skills[18].category == SKILL_ATTACK,
               "skills.tsv has the 100 skills; a rule skill writes its number into skill.<effect>");
    /* The skill byte of a scenario or a save record is the file id (scenario-file.md, officer +124). */
    TEST_CHECK(counts, content_skill_of_file_id(&content, 99) == 0 && content_skill_of_file_id(&content, 0) == 99 &&
                       content_skill_of_file_id(&content, 81) == 18 && content.skills[18].file_id == 81,
               "the file id of a row gives the skill number - 1 that the sim keeps");
    TEST_CHECK(counts, content_skill_of_file_id(&content, OFFICER_SKILL_NONE) == OFFICER_SKILL_NONE &&
                       content_skill_of_file_id(&content, 100) == OFFICER_SKILL_NONE,
               "the file id 255 and a file id above 99 give no skill");
    tsv_table bad = {};
    TEST_CHECK(counts, tsv_parse(unit_skill_text(arena, "%u\tx_%u\tx\tattack\tno_such_hook\trule\tx\t%u\n"), arena, &bad) &&
                       !content_load_skills(&content, &bad, &bad_row) && bad_row == 18,
               "a rule skill with an effect that names no rule is a bad row");
    tsv_table twice = {};
    TEST_CHECK(counts, tsv_parse(unit_skill_text(arena, "%u\tcapture_%u\tx\tattack\tcapture\trule\tx\t99\n"), arena, &twice) &&
                       !content_load_skills(&content, &twice, &bad_row) && bad_row == 18,
               "a file id that a row before uses is a bad row");
    tsv_table missing = {};
    TEST_CHECK(counts, tsv_parse(unit_skill_text(arena, "%u\tcapture_%u\tx\tattack\tcapture\trule\tx\t\n"), arena, &missing) &&
                       !content_load_skills(&content, &missing, &bad_row) && bad_row == 18,
               "a row without a file id is a bad row");
    sim_content empty = {};
    TEST_CHECK(counts, content_skill_of_file_id(&empty, 12) == 12,
               "a pack without skills.tsv keeps the skill byte as the number - 1");
}

internal void
unit_rank_and_post_tests(test_counts *counts, memory_arena *arena)
{
    sim_content content = {};
    tsv_table ranks = {}, posts = {};
    u32 bad_row = 0;
    char *rank_text = content_test_text(arena, UNIT_RANK_HEADER "0\tnone\tx\t10000\t0\tno\tx\n1\tinspector\tx\t11000\t2\tno\tx\n");
    char *post_text = content_test_text(arena, UNIT_POST_HEADER "0\tp0\tx\t36000\t15000\tpolitics\t5\t1\tx\n1\tp1\tx\t0\t6000\t-\t0\t0\tx\n");
    TEST_CHECK(counts, tsv_parse(rank_text, arena, &ranks) && content_load_ranks(&content, &ranks, &bad_row) && content.rank_count == 2 &&
                       content.ranks[1].soldiers == 11000 && content.ranks[1].cities == 2,
               "ranks.tsv: the soldiers of the ruler and the cities that each rank needs");
    TEST_CHECK(counts, tsv_parse(post_text, arena, &posts) && content_load_posts(&content, &posts, &bad_row) && content.post_count == 2 &&
                       content.posts[0].ability == CONTENT_ABILITY_POLITICS && content.posts[0].bonus == 5 && content.posts[1].ability == CONTENT_ABILITY_COUNT,
               "posts.tsv: merit, soldiers, the ability it raises and the rank it needs");
    tsv_table bad = {};
    char *bad_text = content_test_text(arena, UNIT_POST_HEADER "0\tp0\tx\t0\t5000\t-\t0\t7\tx\n");
    TEST_CHECK(counts, tsv_parse(bad_text, arena, &bad) && !content_load_posts(&content, &bad, &bad_row), "a post that needs a rank that is not in ranks.tsv is a bad row");
}

internal void
unit_item_table_tests(test_counts *counts, memory_arena *arena)
{
    sim_content content = {};
    tsv_table table = {};
    u32 bad_row = 0;
    char *text = content_test_text(arena, UNIT_ITEM_HEADER "0\tred_hare\tx\tA8AAA8DFB0A8\thorse\tplain\tx\n1\tsnake\tx\tB344A5D9\tpolearm\tsnake_spear\tx\n");
    TEST_CHECK(counts, tsv_parse(text, arena, &table) && content_load_items(&content, &table, &bad_row) && content.item_count == 2 &&
                       content.items[0].big5_size == 6 && content.items[0].big5[0] == 0xA8 && content.items[1].polearm == POLEARM_SNAKE_SPEAR,
               "items.tsv: the Big5 bytes of the name, the type and the polearm");
    u8 name[ITEM_NAME_SIZE] = { 0xB3, 0x44, 0xA5, 0xD9 };
    u8 longer[ITEM_NAME_SIZE] = { 0xB3, 0x44, 0xA5, 0xD9, 0xA4, 0x40 };
    TEST_CHECK(counts, content_find_item_kind(&content, name, ITEM_NAME_SIZE) == 1 && content_find_item_kind(&content, longer, ITEM_NAME_SIZE) == CONTENT_NO_ITEM_KIND,
               "a save item finds its kind by the whole Big5 name");
    tsv_table bad = {};
    char *bad_text = content_test_text(arena, UNIT_ITEM_HEADER "0\tx\tx\tA8A\thorse\tplain\tx\n");
    TEST_CHECK(counts, tsv_parse(bad_text, arena, &bad) && !content_load_items(&content, &bad, &bad_row), "a Big5 name with an odd number of hex digits is a bad row");
}

internal void
unit_relation_and_debate_tests(test_counts *counts)
{
    TEST_CHECK(counts, sim_compatibility_distance(0, 149) == 1 && sim_compatibility_distance(0, 75) == 75 && sim_compatibility_distance(10, 100) == 60 &&
                       sim_compatibility_distance(40, 40) == 0,
               "compatibility is a wheel of 150: the distance is at most 75");
    TEST_CHECK(counts, sim_debate_hand_size(69) == 3 && sim_debate_hand_size(70) == 4 && sim_debate_hand_size(85) == 5 && sim_debate_hand_size(90) == 6,
               "the hand size by intellect (guide book p.30)");
    sim_card ignore = sim_card_make(CARD_IGNORE, 0, 0), shout = sim_card_make(CARD_SHOUT, 0, 0), calm = sim_card_make(CARD_CALM, 0, 0);
    sim_card reason_small = sim_card_make(CARD_TOPIC, TOPIC_REASON, 1), story_large = sim_card_make(CARD_TOPIC, TOPIC_STORY, 3);
    sim_card reason_large = sim_card_make(CARD_TOPIC, TOPIC_REASON, 3);
    TEST_CHECK(counts, sim_card_compare(ignore, shout, TOPIC_REASON) > 0 && sim_card_compare(shout, reason_large, TOPIC_REASON) > 0 &&
                       sim_card_compare(calm, reason_small, TOPIC_REASON) < 0 && sim_card_compare(reason_small, calm, TOPIC_REASON) > 0,
               "the strength order: 無視 > 大喝 > topic cards > 鎮靜");
    TEST_CHECK(counts, sim_card_compare(reason_small, story_large, TOPIC_REASON) > 0 && sim_card_compare(reason_large, reason_small, TOPIC_REASON) > 0 &&
                       sim_card_compare(reason_small, reason_small, TOPIC_REASON) == 0,
               "on the topic 道理 a 道理 小 beats a 故事 大; on one topic 大 > 小; equal cards draw");
}

/* The rise of one 褒賞 (G2-040, assumed Q-028) and the bands of the clue of
   the adviser, from the rules alone. */
internal void
unit_reward_and_clue_tests(test_counts *counts)
{
    sim_rules rules = {};
    rules.reward_loyalty = 10;
    rules.reward_compatibility_divisor = 15;
    rules.reward_loyalty_min = 1;
    TEST_CHECK(counts, sim_reward_loyalty_rise(&rules, 0) == 10 && sim_reward_loyalty_rise(&rules, 14) == 10 && sim_reward_loyalty_rise(&rules, 15) == 9 &&
                       sim_reward_loyalty_rise(&rules, 75) == 5,
               "the rise of a reward is reward.loyalty less one for each reward.compatibility_divisor of the distance to the ruler");
    rules.reward_loyalty = 3;
    TEST_CHECK(counts, sim_reward_loyalty_rise(&rules, 75) == 1, "the rise is at least reward.loyalty_min");
    rules.reward_loyalty = 10;
    rules.reward_compatibility_divisor = 0;
    TEST_CHECK(counts, sim_reward_loyalty_rise(&rules, 75) == 10, "a divisor of 0 turns the penalty of the compatibility off");
    rules.clue_low_below = 34;
    rules.clue_high_from = 67;
    TEST_CHECK(counts, sim_chance_clue(&rules, 0) == SIM_CLUE_LOW && sim_chance_clue(&rules, 33) == SIM_CLUE_LOW &&
                       sim_chance_clue(&rules, 34) == SIM_CLUE_MIDDLE && sim_chance_clue(&rules, 66) == SIM_CLUE_MIDDLE &&
                       sim_chance_clue(&rules, 67) == SIM_CLUE_HIGH && sim_chance_clue(&rules, 100) == SIM_CLUE_HIGH,
               "the clue of the adviser: low below advice.low_below, high from advice.high_from, middle between them");
}

internal void
unit_court_tests(test_counts *counts, memory_arena *arena)
{
    unit_skill_table_tests(counts, arena);
    unit_rank_and_post_tests(counts, arena);
    unit_item_table_tests(counts, arena);
    unit_relation_and_debate_tests(counts);
    unit_reward_and_clue_tests(counts);
}
