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

internal void
feature_playable_tests(test_counts *counts)
{
    feature_save_load_tests(counts);
}
