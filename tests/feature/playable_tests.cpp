/* Feature tests of M9 (game-setup-and-modes.md, pk-officer-training.md,
   events.md; F-009, F-060, F-061, F-131 to F-135), through engine_api
   with the real engine and the sample save. */

/* What a test compares after a save and a load: the date, the turn, the
   ledger, and the gold, grain and public order of every city. */
struct feature_game_digest
{
    engine_date date;
    u32         turn;
    u32         ledger;
    u32         gold[SAVE_CITY_COUNT];
    u32         grain[SAVE_CITY_COUNT];
    u32         order[SAVE_CITY_COUNT];
    u8          acting;
};

internal feature_game_digest
feature_digest(engine_api *api)
{
    void *e = api->engine;
    feature_game_digest digest = {};
    digest.date   = api->date(e);
    digest.turn   = api->turn(e);
    digest.ledger = api->ledger_count(e);
    digest.acting = api->whose_turn(e);
    for (u32 index = 0; index < api->city_count(e) && index < SAVE_CITY_COUNT; ++index)
    {
        engine_city city = api->city_at(e, index);
        engine_city_economy economy = api->city_economy(e, city.city_id);
        digest.gold[index]  = city.gold;
        digest.grain[index] = city.food;
        digest.order[index] = economy.public_order;
    }
    return digest;
}

internal b32
feature_digest_equal(feature_game_digest *a, feature_game_digest *b)
{
    return a->date.days_since_start == b->date.days_since_start && a->turn == b->turn && a->ledger == b->ledger && a->acting == b->acting &&
           memcmp(a->gold, b->gold, sizeof(a->gold)) == 0 && memcmp(a->grain, b->grain, sizeof(a->grain)) == 0 &&
           memcmp(a->order, b->order, sizeof(a->order)) == 0;
}

/* ---- F-009: save and load ---------------------------------------------------------- */

internal void
feature_save_load_tests(test_counts *counts)
{
    engine_fixture empty;
    engine_fixture_init(&empty, 1, 65536);
    TEST_CHECK(counts, empty.api.save_game(empty.api.engine, 0, "x") == ENGINE_SAVE_NO_GAME, "refused: no game to save");
    TEST_CHECK(counts, empty.api.save_game(empty.api.engine, ENGINE_SAVE_SLOTS, "x") == ENGINE_SAVE_BAD_SLOT &&
                       empty.api.load_game(empty.api.engine, ENGINE_SAVE_SLOTS) == ENGINE_SAVE_BAD_SLOT,
               "refused: a slot after the 32 slots");
    engine_fixture_destroy(&empty);

    feature_city_setup setup;
    feature_city_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u16 ids[3];
    feature_take_officers(api, setup.city, ids, 1);
    api->command_patrol(e, setup.city, ids);
    feature_next_own_turn(api, setup.force);
    feature_game_digest saved = feature_digest(api);
    TEST_CHECK(counts, api->save_game(e, 2, "before the war") == ENGINE_SAVE_OK, "save_game writes slot 3");
    engine_save_slot slot = api->save_slot(e, 2);
    TEST_CHECK(counts, slot.used && strcmp(slot.comment, "before the war") == 0 && slot.year == saved.date.year && slot.month == saved.date.month &&
                       slot.player == setup.force && slot.turn == saved.turn,
               "the slot shows the comment, the date, the faction and the turn");
    TEST_CHECK(counts, api->ledger_count(e) == saved.ledger, "a save ledgers nothing");
    feature_next_own_turn(api, setup.force);
    feature_next_own_turn(api, setup.force);
    feature_game_digest later = feature_digest(api);

    /* A second engine loads the slot and plays the same two turns. */
    engine_fixture other;
    engine_fixture_init(&other, 99, 65536);
    engine_api *api_b = &other.api;
    api_b->refresh_save_slots(api_b->engine);
    TEST_CHECK(counts, api_b->save_slot(api_b->engine, 2).used && strcmp(api_b->save_slot(api_b->engine, 2).comment, "before the war") == 0,
               "refresh_save_slots reads the slots from the folder");
    TEST_CHECK(counts, api_b->load_game(api_b->engine, 2) == ENGINE_SAVE_OK, "load_game reads slot 3");
    feature_game_digest loaded = feature_digest(api_b);
    TEST_CHECK(counts, feature_digest_equal(&saved, &loaded), "after the load the date, the turn, the ledger and every city are as when saved");
    feature_next_own_turn(api_b, setup.force);
    feature_next_own_turn(api_b, setup.force);
    feature_game_digest replay = feature_digest(api_b);
    TEST_CHECK(counts, feature_digest_equal(&later, &replay), "the loaded game plays on as the saved game did: the seed is in the save");

    /* A damaged file and an empty slot. */
    char junk[64] = "not a save";
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "test_saves\\slot_%02u.hxs", 31);
    test_platform_write_entire_file(path, junk, sizeof(junk));
    char exe_dir[MAX_PATH], empty_path[MAX_PATH];
    test_exe_dir(exe_dir, MAX_PATH);
    snprintf(empty_path, sizeof(empty_path), "%s\\test_saves\\slot_32.hxs", exe_dir);
    DeleteFileA(empty_path);
    feature_game_digest before = feature_digest(api_b);
    TEST_CHECK(counts, api_b->load_game(api_b->engine, 30) == ENGINE_SAVE_BAD_FILE && api_b->load_game(api_b->engine, 31) == ENGINE_SAVE_NO_FILE,
               "refused: a file that is not a save, and an empty slot");
    feature_game_digest after = feature_digest(api_b);
    TEST_CHECK(counts, feature_digest_equal(&before, &after), "a refused load changes nothing");
    engine_fixture_destroy(&other);
    engine_fixture_destroy(&setup.fixture);
}

/* ---- F-132: the settings of the start ------------------------------------------------- */

/* The sum of the commands that the computer factions gave in one turn. */
internal u32
feature_ai_commands_in_a_turn(u8 difficulty)
{
    feature_city_setup setup;
    feature_city_setup_init(&setup);
    engine_api *api = setup.api;
    engine_settings settings = api->settings(api->engine);
    settings.ai         = true;
    settings.difficulty = difficulty;
    api->set_settings(api->engine, settings);
    u32 mark = api->ledger_count(api->engine);
    feature_next_own_turn(api, setup.force);
    u32 total = 0;
    for (u32 index = mark; index < api->ledger_count(api->engine); ++index)
    {
        sim_event event = api->ledger_at(api->engine, index);
        if (event.kind == SIM_EVENT_AI_PLANNED) total += (u32)event.delta;
    }
    engine_fixture_destroy(&setup.fixture);
    return total;
}

internal void
feature_settings_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_city_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    engine_settings settings = api->settings(e);
    TEST_CHECK(counts, settings.female_officers && settings.strong_plots && settings.relations && !settings.long_lifespan &&
                       settings.duel_mode == DUEL_MODE_MANUAL && settings.difficulty == DIFFICULTY_NORMAL,
               "a new game has female officers, the strong plots and the relations, manual duels and the normal level");

    /* 血緣・關係 off clears liked, disliked, spouse and sworn brothers. */
    u16 related = ENGINE_NO_OFFICER;
    for (u32 index = 0; index < api->officer_count(e) && related == ENGINE_NO_OFFICER; ++index)
    {
        engine_officer officer = api->officer_at(e, index);
        engine_officer_detail detail = api->officer_detail(e, officer.officer_id);
        if (detail.liked[0] != ENGINE_NO_OFFICER || officer.spouse != ENGINE_NO_OFFICER) related = officer.officer_id;
    }
    /* 女武將 off hides a free female officer (the editor makes one). */
    u16 free_officer = ENGINE_NO_OFFICER;
    for (u32 index = 0; index < api->officer_count(e) && free_officer == ENGINE_NO_OFFICER; ++index)
    {
        engine_officer officer = api->officer_at(e, index);
        if (officer.status == OFFICER_STATUS_FREE_OR_NOT_APPEARED) free_officer = officer.officer_id;
    }
    engine_officer_edit edit = feature_edit_of(api, free_officer);
    edit.female = true;
    api->set_officer(e, free_officer, &edit);
    settings.relations       = false;
    settings.female_officers = false;
    api->set_settings(e, settings);
    engine_officer_detail detail = api->officer_detail(e, related);
    engine_officer officer = {};
    feature_officer_by_id(api, related, &officer);
    TEST_CHECK(counts, related != ENGINE_NO_OFFICER && detail.liked[0] == ENGINE_NO_OFFICER && officer.spouse == ENGINE_NO_OFFICER &&
                       officer.sworn_brother_group_head == ENGINE_NO_OFFICER,
               "血緣・關係 off: the officers have no liked officer, no spouse and no sworn brothers");
    feature_officer_by_id(api, free_officer, &officer);
    TEST_CHECK(counts, free_officer != ENGINE_NO_OFFICER && officer.status == OFFICER_STATUS_DEAD, "女武將 off: a free female officer never appears");

    /* 單挑・舌戰 automatic: a debate of the player ends at once. */
    settings.duel_mode = DUEL_MODE_AUTO;
    api->set_settings(e, settings);
    u16 ids[3];
    feature_take_officers(api, setup.city, ids, 2);
    u32 mark = api->ledger_count(e);
    engine_command_result debate = api->start_debate(e, ids[0], ids[1]);
    TEST_CHECK(counts, debate.refusal == SIM_REFUSAL_NONE && !api->debate_view(e).active && feature_count_events(api, SIM_EVENT_DEBATE_ENDED, mark) == 1,
               "單挑・舌戰 automatic: the computer plays the debate to its end and takes the first result");
    engine_fixture_destroy(&setup.fixture);

    u32 beginner = feature_ai_commands_in_a_turn(DIFFICULTY_BEGINNER), super = feature_ai_commands_in_a_turn(DIFFICULTY_SUPER);
    TEST_CHECK(counts, beginner < super, "遊戲等級: the computer factions give more commands at 超級 than at 初級 (assumed, Q-187)");
}

/* ---- F-060: new officers -------------------------------------------------------------- */

internal sim_new_officer_order
feature_new_officer(u8 city, u8 type)
{
    sim_new_officer_order order = {};
    order.family_name[0] = 0xB1; order.family_name[1] = 0x69;   /* 張 */
    order.given_name[0]  = 0xA4; order.given_name[1]  = 0x40;   /* 一 */
    order.type        = type;
    order.personality = PERSONALITY_CALM;
    order.city_id     = city;
    order.ancient     = SIM_NO_ANCIENT;
    return order;
}

/* The officer that the last OFFICER_CREATED event names. */
internal u16
feature_created_officer(engine_api *api, u32 mark)
{
    u16 found = ENGINE_NO_OFFICER;
    for (u32 index = 0; index < api->officer_count(api->engine); ++index)
    {
        engine_officer officer = api->officer_at(api->engine, index);
        if (api->officer_detail(api->engine, officer.officer_id).created) found = officer.officer_id;
    }
    (void)mark;
    return found;
}

internal void
feature_new_officer_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_city_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u32 mark = api->ledger_count(e);
    sim_new_officer_order order = feature_new_officer(setup.city, OFFICER_TYPE_ALL_ROUND);
    engine_command_result result = api->create_officer(e, &order);
    u16 created = feature_created_officer(api, mark);
    engine_officer officer = {};
    feature_officer_by_id(api, created, &officer);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && created != ENGINE_NO_OFFICER && api->new_officer_count(e) == 1 &&
                       officer.status == OFFICER_STATUS_FREE_OR_NOT_APPEARED && officer.current_position == setup.city &&
                       officer.leadership == 85 && officer.charm == 85 && feature_count_events(api, SIM_EVENT_OFFICER_CREATED, mark) == 1,
               "新武將 萬能: a free officer in the city with all five abilities high (85, assumed)");
    order = feature_new_officer(setup.city, OFFICER_TYPE_STRATEGIST);
    api->create_officer(e, &order);
    feature_officer_by_id(api, feature_created_officer(api, mark), &officer);
    TEST_CHECK(counts, officer.leadership == 65 && officer.intelligence == 85, "軍師: normal leadership and force, high intellect, politics and charm");
    order = feature_new_officer(setup.city, OFFICER_TYPE_COMMON);
    order.ancient = 0;
    api->create_officer(e, &order);
    feature_officer_by_id(api, feature_created_officer(api, mark), &officer);
    TEST_CHECK(counts, officer.leadership == api->ancient_at(e, 0).abilities[0] && api->ancient_count(e) == 16 &&
                       memcmp(officer.family_name, api->ancient_at(e, 0).family_name, 2) == 0,
               "an ancient officer joins with the name and the abilities of its row");
    order = feature_new_officer(setup.city, OFFICER_TYPE_GENERAL);
    order.found_force = true;
    TEST_CHECK(counts, api->create_officer(e, &order).refusal == SIM_REFUSAL_NO_CITY, "refused: a new faction needs an empty city");
    u8 empty = ENGINE_NO_CITY;
    for (u32 city = 0; city < api->city_count(e) && empty == ENGINE_NO_CITY; ++city) if (api->city_at(e, city).owner_force_id == ENGINE_NO_FACTION) empty = (u8)city;
    if (empty != ENGINE_NO_CITY)
    {
        order.city_id = empty;
        result = api->create_officer(e, &order);
        feature_officer_by_id(api, feature_created_officer(api, mark), &officer);
        TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && officer.status == OFFICER_STATUS_RULER && api->city_at(e, empty).owner_force_id == officer.force_id &&
                           feature_count_events(api, SIM_EVENT_FACTION_FOUNDED, mark) == 1,
                   "a new officer founds a faction in an empty city and rules it");
    }
    while (api->new_officer_count(e) < 8) { order = feature_new_officer(setup.city, OFFICER_TYPE_COMMON); api->create_officer(e, &order); }
    order = feature_new_officer(setup.city, OFFICER_TYPE_COMMON);
    TEST_CHECK(counts, api->create_officer(e, &order).refusal == SIM_REFUSAL_LIMIT_REACHED, "refused: at most 8 new officers join one game");
    engine_fixture_destroy(&setup.fixture);
}

/* ---- F-061: 研究能力 and 培育武將 --------------------------------------------------------- */

#define FEATURE_CHART_LEADERSHIP_5  0
#define FEATURE_CHART_LEADERSHIP_70 5
#define FEATURE_CHART_PIKE_B        8

internal void
feature_training_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_city_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 force = setup.force, city = setup.city;
    TEST_CHECK(counts, api->chart_count(e) == 18 && api->chart_row_at(e, FEATURE_CHART_LEADERSHIP_70).requires == 1u, "ability_chart.tsv: 18 rows; 統率70 needs 統率+5 first");
    TEST_CHECK(counts, api->command_research_ability(e, city, FEATURE_CHART_LEADERSHIP_70).refusal == SIM_REFUSAL_NEEDS_TECHNIQUE, "refused: the row before it on the arrow");
    u32 gold = api->city_at(e, city).gold, points = api->actions_left(e, force);
    engine_command_result research = api->command_research_ability(e, city, FEATURE_CHART_LEADERSHIP_5);
    engine_faction_chart chart = api->faction_chart(e, force);
    TEST_CHECK(counts, research.refusal == SIM_REFUSAL_NONE && api->city_at(e, city).gold == gold - 300 && api->actions_left(e, force) == points - 20 &&
                       chart.job_row == FEATURE_CHART_LEADERSHIP_5 && chart.turns_left == 9,
               "研究能力: 300 gold and 20 AP; 統率+5 takes 3 months (9 turns)");
    TEST_CHECK(counts, api->command_research_ability(e, city, 1).refusal == SIM_REFUSAL_LIMIT_REACHED, "refused: one research at a time");
    for (u32 turn = 0; turn < 10 && api->faction_chart(e, force).job_row != ENGINE_NO_ROW; ++turn) feature_next_own_turn(api, force);
    chart = api->faction_chart(e, force);
    TEST_CHECK(counts, chart.job_row == ENGINE_NO_ROW && chart.uses[FEATURE_CHART_LEADERSHIP_5] == 3 && (chart.known & 1u),
               "the research ends with 3 uses of 統率+5");

    api->set_actions(e, force, 255);
    u16 ids[3];
    feature_take_officers(api, city, ids, 2);
    engine_officer before = {};
    feature_officer_by_id(api, ids[0], &before);
    TEST_CHECK(counts, api->command_train_officer(e, ids[0], FEATURE_CHART_PIKE_B).refusal == SIM_REFUSAL_NO_USES, "refused: a row without uses");
    engine_command_result train = api->command_train_officer(e, ids[0], FEATURE_CHART_LEADERSHIP_5);
    engine_officer away = {};
    feature_officer_by_id(api, ids[0], &away);
    TEST_CHECK(counts, train.refusal == SIM_REFUSAL_NONE && api->officer_detail(e, ids[0]).training_turns == 3 && away.current_position >= SAVE_CITY_COUNT &&
                       api->faction_chart(e, force).uses[FEATURE_CHART_LEADERSHIP_5] == 2,
               "培育武將: 20 AP; the officer is away 30 days and the row has one use less");
    u16 alone[3] = { ids[0], ENGINE_NO_OFFICER, ENGINE_NO_OFFICER };
    TEST_CHECK(counts, api->command_patrol(e, city, alone).refusal == SIM_REFUSAL_OFFICER_NOT_IN_CITY, "refused: an officer in training takes no command");
    TEST_CHECK(counts, api->command_train_officer(e, ids[1], FEATURE_CHART_LEADERSHIP_5).refusal == SIM_REFUSAL_LIMIT_REACHED,
               "refused: one officer at a time trains a base ability");
    u32 mark = api->ledger_count(e);
    for (u32 turn = 0; turn < 3; ++turn) feature_next_own_turn(api, force);
    engine_officer after = {};
    feature_officer_by_id(api, ids[0], &after);
    TEST_CHECK(counts, api->officer_detail(e, ids[0]).training_turns == 0 && after.current_position == city &&
                       after.leadership == Minimum(before.leadership + 5u, 100u) && feature_count_events(api, SIM_EVENT_OFFICER_TRAINED, mark) == 1,
               "the officer returns after 30 days with 統率 +5");
    engine_fixture_destroy(&setup.fixture);
}

internal void
feature_playable_tests(test_counts *counts)
{
    feature_save_load_tests(counts);
    feature_settings_tests(counts);
    feature_new_officer_tests(counts);
    feature_training_tests(counts);
}
