/* Feature tests of load_name_index and city_name_at, through engine_api with
   the real engine and the real Scenario.s11.

   Expected values come from spec-src/spec/formats/scenario-file.md
   (Scenario.s11): 42 city entries, and the entry number is the city id.
   The name lengths come from spec_city_name_characters in
   scenario_fixture_tests.cpp. */

#define NAME_INDEX_RELATIVE_PATH "..\\spec-src\\spec\\fixtures\\scenarios\\Scenario.s11"
#define SCEN002_RELATIVE_PATH    "..\\spec-src\\spec\\fixtures\\scenarios\\SCEN002.s11"

internal b32
feature_no_city_names(engine_api *api)
{
    for (u32 city = 0; city < SCENARIO_CITY_COUNT; ++city)
    {
        if (api->city_name_at(api->engine, city).city_id != ENGINE_NO_CITY) return false;
    }
    return true;
}

internal void
feature_name_index_failure_tests(test_counts *counts)
{
    engine_fixture f;
    engine_fixture_init(&f, 1, 64);
    engine_api *api = &f.api;
    void *e = api->engine;

    TEST_CHECK(counts, feature_no_city_names(api), "a new game has no city names");

    engine_load_result missing = api->load_name_index(e, "no-such-index.s11");
    TEST_CHECK(counts, missing.status == ENGINE_LOAD_FILE_NOT_FOUND && missing.command_id == 0,
               "a missing name index is not found");
    engine_load_result scenario = api->load_name_index(e, SCEN002_RELATIVE_PATH);
    TEST_CHECK(counts, scenario.status == ENGINE_LOAD_BAD_NAME_INDEX && scenario.command_id == 0 &&
                       strcmp(scenario.detail, "wrong kind") == 0,
               "a scenario file is refused as a name index of the wrong kind");
    engine_load_result save = api->load_name_index(e, SAMPLE_SAVE_RELATIVE_PATH);
    TEST_CHECK(counts, save.status == ENGINE_LOAD_BAD_NAME_INDEX && save.command_id == 0,
               "a save file is refused as a name index");
    engine_load_result readme = api->load_name_index(e, SAMPLE_README_RELATIVE_PATH);
    TEST_CHECK(counts, readme.status == ENGINE_LOAD_BAD_NAME_INDEX && strcmp(readme.detail, "file too small") == 0,
               "a short text file is refused as too small");
    TEST_CHECK(counts, feature_no_city_names(api) && api->ledger_count(e) == 0, "a refused name index changes nothing");
    engine_fixture_destroy(&f);

    memory_arena permanent = test_arena_create(Megabytes(8));
    memory_arena transient = test_arena_create(Megabytes(8));
    engine *backend = engine_create(&permanent, &transient, engine_default_config());   /* no file reader */
    engine_api bare = engine_make_api(backend);
    TEST_CHECK(counts, bare.load_name_index(bare.engine, NAME_INDEX_RELATIVE_PATH).status == ENGINE_LOAD_NO_FILE_READER,
               "an engine without a file reader refuses to load a name index");
    test_arena_destroy(&transient);
    test_arena_destroy(&permanent);
}

/* The names through the api are the bytes that name_index_load decodes. */
internal void
feature_city_name_tests(test_counts *counts, engine_api *api)
{
    memory_arena arena = test_arena_create(Megabytes(1));
    char path[MAX_PATH];
    test_fixture_path(path, MAX_PATH, NAME_INDEX_FIXTURE);
    test_file file = test_read_file(path, &arena);
    name_index expected = {};
    if (file.contents) name_index_load(file.contents, file.size, &expected);

    b32 ids_agree = true, bytes_agree = true, lengths_agree = true;
    for (u32 city = 0; city < SCENARIO_CITY_COUNT; ++city)
    {
        engine_city_name name = api->city_name_at(api->engine, city);
        if (name.city_id != city) ids_agree = false;
        if (memcmp(name.name, expected.cities[city].name, sizeof(name.name)) != 0) bytes_agree = false;
        u32 length = fixture_name_length(name.name, sizeof(name.name));
        if (length != SPEC_BIG5_CHARACTER_BYTES * spec_city_name_characters[city]) lengths_agree = false;
    }
    TEST_CHECK(counts, ids_agree, "city_name_at(index) names city id index for 0..41");
    TEST_CHECK(counts, bytes_agree, "the name bytes are the bytes of the name index entry");
    TEST_CHECK(counts, lengths_agree, "each name has two bytes for each character of its spec name");
    TEST_CHECK(counts, api->city_name_at(api->engine, SCENARIO_CITY_COUNT).city_id == ENGINE_NO_CITY,
               "city_name_at out of range is no city");
    test_arena_destroy(&arena);
}

internal void
feature_name_index_tests(test_counts *counts)
{
    feature_name_index_failure_tests(counts);

    engine_fixture f;
    engine_fixture_init(&f, 1, 64);
    engine_api *api = &f.api;
    void *e = api->engine;

    engine_load_result loaded = api->load_name_index(e, NAME_INDEX_RELATIVE_PATH);
    TEST_CHECK(counts, loaded.status == ENGINE_LOAD_OK && loaded.command_id != 0 && strcmp(loaded.detail, "ok") == 0,
               "Scenario.s11 loads through the api");
    TEST_CHECK(counts, api->ledger_count(e) == 1, "a name index load ledgers one event");
    sim_event event = api->ledger_at(e, 0);
    TEST_CHECK(counts, event.kind == SIM_EVENT_NAME_INDEX_LOADED && event.cause == loaded.command_id &&
                       event.delta == SCENARIO_CITY_COUNT, "the event names its load and the city count");
    TEST_CHECK(counts, api->turn(e) == 0 && api->entities_alive(e) == 0 && api->city_count(e) == 0,
               "a name index adds names, not cities or entities");
    feature_city_name_tests(counts, api);

    engine_city_name hanzhong = api->city_name_at(e, SPEC_HANZHONG_CITY_ID);
    engine_load_result save = api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
    engine_city_name after_save = api->city_name_at(e, SPEC_HANZHONG_CITY_ID);
    TEST_CHECK(counts, save.status == ENGINE_LOAD_OK && memcmp(&hanzhong, &after_save, sizeof(hanzhong)) == 0,
               "a save load keeps the city names");
    TEST_CHECK(counts, api->city_at(e, SPEC_HANZHONG_CITY_ID).city_id == after_save.city_id,
               "the city at index 36 and the name at index 36 are the same city");

    api->load_name_index(e, SCEN002_RELATIVE_PATH);
    engine_city_name after_refusal = api->city_name_at(e, SPEC_HANZHONG_CITY_ID);
    TEST_CHECK(counts, memcmp(&hanzhong, &after_refusal, sizeof(hanzhong)) == 0, "a refused name index keeps the old names");

    engine_load_result again = api->load_name_index(e, NAME_INDEX_RELATIVE_PATH);
    TEST_CHECK(counts, again.status == ENGINE_LOAD_OK && again.command_id > save.command_id,
               "a second name index load is a new command");
    feature_city_name_tests(counts, api);

    engine_fixture_destroy(&f);
}
