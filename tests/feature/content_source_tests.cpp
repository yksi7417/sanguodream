/* Feature tests of the install folder and content_source (M16), through
   engine_api with the real engine.

   The test makes a small install folder in build\test_install. It copies
   the two scenario files of spec-src/spec/fixtures/scenarios into
   media\scenario, the folder of formats/binary-identity.md. It also writes
   a short file media\msg\S11MSG00.s11. The engine only finds that file. It
   does not decode it, because the spec has no layout of the message files.

   Expected values come from formats/scenario-file.md: SCEN002 is scenario
   number 2, it starts in the year 194 in month 6, and its title is 群雄割據. */

#define CONTENT_TEST_INSTALL_FOLDER "test_install"

/* The Big5 bytes of 群雄割據, as in tests/unit/scenario_format_tests.cpp. */
global_variable const u8 content_scen002_title[] = { 0xB8, 0x73, 0xB6, 0xAF, 0xB3, 0xCE, 0xBE, 0xDA };

/* Makes <exe dir>\test_install with the copies. Writes the absolute path of
   the folder to out. Returns false if a copy failed. */
internal b32
content_make_test_install(char *out, u32 out_size)
{
    char exe_dir[MAX_PATH];
    test_exe_dir(exe_dir, MAX_PATH);
    snprintf(out, out_size, "%s\\%s", exe_dir, CONTENT_TEST_INSTALL_FOLDER);

    char folder[MAX_PATH];
    CreateDirectoryA(out, 0);
    snprintf(folder, MAX_PATH, "%s\\media", out);
    CreateDirectoryA(folder, 0);
    snprintf(folder, MAX_PATH, "%s\\media\\scenario", out);
    CreateDirectoryA(folder, 0);
    snprintf(folder, MAX_PATH, "%s\\media\\msg", out);
    CreateDirectoryA(folder, 0);

    static const char *files[] = { "Scenario.s11", "SCEN002.s11" };
    for (u32 index = 0; index < ArrayCount(files); ++index)
    {
        char source[MAX_PATH], target[MAX_PATH];
        char relative[64];
        snprintf(relative, sizeof(relative), "scenarios\\%s", files[index]);
        test_fixture_path(source, MAX_PATH, relative);
        snprintf(target, MAX_PATH, "%s\\media\\scenario\\%s", out, files[index]);
        if (!CopyFileA(source, target, FALSE)) return false;
    }

    char message[MAX_PATH];
    snprintf(message, MAX_PATH, "%s\\media\\msg\\S11MSG00.s11", out);
    FILE *file = fopen(message, "wb");
    if (!file) return false;
    fputs("not a message table", file);
    fclose(file);
    return true;
}

internal b32
content_title_is_scen002(const u8 *title)
{
    return memcmp(title, content_scen002_title, sizeof(content_scen002_title)) == 0 &&
           title[sizeof(content_scen002_title)] == 0;
}

/* Before the player names an install folder, each asset comes from the
   content pack or from nothing. */
internal void
feature_content_source_default_tests(test_counts *counts)
{
    engine_fixture f;
    engine_fixture_init(&f, 1, 64);
    engine_api *api = &f.api;
    void *e = api->engine;

    TEST_CHECK(counts, api->set_install_folder && api->content_source && api->install_scenario_count && api->install_scenario_at,
               "the api has the M16 functions");

    engine_content_source names = api->content_source(e, ENGINE_ASSET_CITY_NAMES);
    TEST_CHECK(counts, names.asset == ENGINE_ASSET_CITY_NAMES && names.source == ENGINE_SOURCE_NONE && names.count == 0,
               "a new game has no city names from any source");
    engine_content_source scenarios = api->content_source(e, ENGINE_ASSET_SCENARIOS);
    TEST_CHECK(counts, scenarios.source == ENGINE_SOURCE_PACK && scenarios.count == api->scenario_count(e) && scenarios.count > 0,
               "the scenario list comes from scenarios.tsv of the pack");
    TEST_CHECK(counts, api->content_source(e, ENGINE_ASSET_MESSAGES).source == ENGINE_SOURCE_PACK,
               "the messages come from the text files of the pack");
    TEST_CHECK(counts, api->content_source(e, ENGINE_ASSET_PORTRAITS).source == ENGINE_SOURCE_NONE &&
                       api->content_source(e, ENGINE_ASSET_MUSIC).source == ENGINE_SOURCE_NONE,
               "the engine has no portraits and no music");
    TEST_CHECK(counts, api->install_scenario_count(e) == 0 && api->install_folder(e)[0] == 0,
               "a new game has no install folder");
    engine_content_source bad = api->content_source(e, ENGINE_ASSET_KIND_COUNT);
    TEST_CHECK(counts, bad.asset == ENGINE_ASSET_KIND_COUNT && bad.source == ENGINE_SOURCE_NONE,
               "an asset kind out of range gives no source");
    TEST_CHECK(counts, api->install_scenario_at(e, 0).number == ENGINE_NO_ROW,
               "an install scenario out of range gives no row");

    TEST_CHECK(counts, api->load_name_index(e, NAME_INDEX_RELATIVE_PATH).status == ENGINE_LOAD_OK, "Scenario.s11 loads");
    names = api->content_source(e, ENGINE_ASSET_CITY_NAMES);
    TEST_CHECK(counts, names.source == ENGINE_SOURCE_PACK && names.count == SCENARIO_CITY_COUNT &&
                       strcmp(names.path, NAME_INDEX_RELATIVE_PATH) == 0,
               "a name index that the game loads gives the city names of the pack");

    u32 ledger = api->ledger_count(e);
    engine_install_result missing = api->set_install_folder(e, "no-such-install");
    TEST_CHECK(counts, !missing.found && missing.command_id == 0 && missing.scenario_count == 0,
               "a folder without the scenario files is not an install");
    TEST_CHECK(counts, api->ledger_count(e) == ledger && api->install_folder(e)[0] == 0 &&
                       api->content_source(e, ENGINE_ASSET_CITY_NAMES).source == ENGINE_SOURCE_PACK &&
                       api->content_source(e, ENGINE_ASSET_SCENARIOS).source == ENGINE_SOURCE_PACK,
               "a refused install folder changes nothing");
    engine_fixture_destroy(&f);
}

/* After the player names the install folder, the names and the scenarios
   come from the install. The messages, the portraits and the music do not,
   because the spec has no layout of their files. */
internal void
feature_content_source_install_tests(test_counts *counts)
{
    char install[MAX_PATH];
    TEST_CHECK(counts, content_make_test_install(install, MAX_PATH), "the test makes an install folder");

    engine_fixture f;
    engine_fixture_init(&f, 1, 64);
    engine_api *api = &f.api;
    void *e = api->engine;

    engine_install_result result = api->set_install_folder(e, install);
    TEST_CHECK(counts, result.found && result.names_loaded && result.command_id != 0 && result.scenario_count == 1,
               "the install folder gives the name index and one scenario");
    TEST_CHECK(counts, strcmp(api->install_folder(e), install) == 0, "the engine keeps the install folder");

    engine_content_source names = api->content_source(e, ENGINE_ASSET_CITY_NAMES);
    TEST_CHECK(counts, names.source == ENGINE_SOURCE_INSTALL && names.count == SCENARIO_CITY_COUNT &&
                       strstr(names.path, "media\\scenario\\Scenario.s11") != 0,
               "the city names come from Scenario.s11 of the install");

    engine_fixture pack;
    engine_fixture_init(&pack, 1, 64);
    pack.api.load_name_index(pack.api.engine, NAME_INDEX_RELATIVE_PATH);
    b32 same_names = true;
    for (u32 city = 0; city < SCENARIO_CITY_COUNT; ++city)
    {
        engine_city_name from_install = api->city_name_at(e, city);
        engine_city_name from_fixture = pack.api.city_name_at(pack.api.engine, city);
        if (from_install.city_id != city || memcmp(from_install.name, from_fixture.name, sizeof(from_install.name)) != 0)
            same_names = false;
    }
    TEST_CHECK(counts, same_names, "the names of the install are the names of the same file in the spec");
    engine_fixture_destroy(&pack);

    engine_content_source scenarios = api->content_source(e, ENGINE_ASSET_SCENARIOS);
    TEST_CHECK(counts, scenarios.source == ENGINE_SOURCE_INSTALL && scenarios.count == 1,
               "the scenario list comes from the install");
    engine_install_scenario scenario = api->install_scenario_at(e, 0);
    TEST_CHECK(counts, scenario.number == 2 && scenario.year == 194 && scenario.month == 6 &&
                       content_title_is_scen002(scenario.title),
               "the install scenario is 群雄割據 of 194, month 6");
    TEST_CHECK(counts, strstr(scenario.file, "media\\scenario\\SCEN002.s11") != 0, "the install scenario names its file");
    TEST_CHECK(counts, api->load_scenario(e, scenario.file).status == ENGINE_LOAD_OK,
               "load_scenario starts a game from the file of the install");

    engine_content_source messages = api->content_source(e, ENGINE_ASSET_MESSAGES);
    TEST_CHECK(counts, messages.source == ENGINE_SOURCE_PACK && messages.install_files == 1,
               "the engine finds one message file of the install, but the messages stay with the pack");
    TEST_CHECK(counts, api->content_source(e, ENGINE_ASSET_PORTRAITS).source == ENGINE_SOURCE_NONE &&
                       api->content_source(e, ENGINE_ASSET_MUSIC).source == ENGINE_SOURCE_NONE,
               "the install gives no portraits and no music");
    for (u8 asset = 0; asset < ENGINE_ASSET_KIND_COUNT; ++asset)
    {
        engine_content_source source = api->content_source(e, asset);
        TEST_CHECK(counts, source.detail && source.detail[0], "each asset kind says why it has its source");
    }

    u32 ledger = api->ledger_count(e);
    engine_install_result cleared = api->set_install_folder(e, "");
    TEST_CHECK(counts, !cleared.found && cleared.command_id == 0 && api->install_folder(e)[0] == 0,
               "an empty folder clears the install folder");
    TEST_CHECK(counts, api->content_source(e, ENGINE_ASSET_SCENARIOS).source == ENGINE_SOURCE_PACK &&
                       api->install_scenario_count(e) == 0 &&
                       api->content_source(e, ENGINE_ASSET_MESSAGES).install_files == 0,
               "without the install the scenario list comes from the pack again");
    TEST_CHECK(counts, api->content_source(e, ENGINE_ASSET_CITY_NAMES).source == ENGINE_SOURCE_INSTALL &&
                       api->ledger_count(e) == ledger,
               "the names of the game stay, and the clear ledgers nothing");
    engine_fixture_destroy(&f);
}

internal void
feature_content_source_tests(test_counts *counts)
{
    feature_content_source_default_tests(counts);
    feature_content_source_install_tests(counts);
}
