/* Feature tests of load_save and the officer and city queries, through
   engine_api with the real engine and the real sample save.

   Expected values come from spec-src/spec/formats/save-file.md: the identity
   checks it gives for sample.s11, the slot table, the observed ranges, and
   the officer/city cross-checks. Three ranges of the spec do not cover the
   sample (QUESTIONS.md, Q-016); these tests do not assert them. */

#define SAMPLE_SAVE_RELATIVE_PATH "..\\spec-src\\spec\\fixtures\\saves\\sample.s11"
#define SAMPLE_README_RELATIVE_PATH "..\\spec-src\\spec\\fixtures\\saves\\README.md"

/* save-file.md, Officer array slots and City record. */
#define SPEC_BUILT_IN_OFFICERS    852
#define SPEC_OFFICER_SLOTS        1100
#define SPEC_CITY_COUNT           42

internal b32
feature_find_officer(engine_api *api, u16 officer_id, engine_officer *out)
{
    u32 count = api->officer_count(api->engine);
    for (u32 index = 0; index < count; ++index)
    {
        engine_officer officer = api->officer_at(api->engine, index);
        if (officer.officer_id == officer_id)
        {
            *out = officer;
            return true;
        }
    }
    return false;
}

internal b32
feature_name_is_set(const u8 *bytes, u32 size)
{
    for (u32 index = 0; index < size; ++index) if (bytes[index]) return true;
    return false;
}

internal void
feature_load_failure_tests(test_counts *counts)
{
    engine_fixture f;
    engine_fixture_init(&f, 1, 64);
    engine_api *api = &f.api;
    void *e = api->engine;

    TEST_CHECK(counts, api->officer_count(e) == 0 && api->city_count(e) == 0, "a new game has no officers and no cities");
    TEST_CHECK(counts, api->officer_at(e, 0).officer_id == ENGINE_NO_OFFICER, "officer_at out of range is no officer");
    TEST_CHECK(counts, api->city_at(e, 0).city_id == ENGINE_NO_CITY, "city_at out of range is no city");

    engine_load_result missing = api->load_save(e, "no-such-save.s11");
    TEST_CHECK(counts, missing.status == ENGINE_LOAD_FILE_NOT_FOUND && missing.command_id == 0, "a missing file is not found");
    engine_load_result not_a_save = api->load_save(e, SAMPLE_README_RELATIVE_PATH);
    TEST_CHECK(counts, not_a_save.status == ENGINE_LOAD_BAD_SAVE && not_a_save.command_id == 0 && not_a_save.detail != 0,
               "a file that is not a save is refused with a reason");
    TEST_CHECK(counts, strcmp(not_a_save.detail, "not a save") == 0, "a text file longer than the header is not a save");

    /* Files written next to the executable for this test, then deleted. */
    char exe_dir[MAX_PATH], path[MAX_PATH];
    test_exe_dir(exe_dir, MAX_PATH);
    snprintf(path, MAX_PATH, "%s\\load_save_test_oversized.tmp", exe_dir);
    FILE *file = fopen(path, "wb");
    u8 zeros[4096] = {};
    for (u32 block = 0; file && block < (u32)(Megabytes(5) / sizeof(zeros)); ++block) fwrite(zeros, 1, sizeof(zeros), file);
    if (file) fclose(file);
    engine_load_result oversized = api->load_save(e, "load_save_test_oversized.tmp");
    TEST_CHECK(counts, oversized.status == ENGINE_LOAD_FILE_NOT_FOUND && oversized.command_id == 0,
               "a file larger than the scratch arena is not read");
    remove(path);

    memory_arena copy_arena = test_arena_create(Megabytes(1));
    char sample_path[MAX_PATH];
    test_fixture_path(sample_path, MAX_PATH, "saves\\sample.s11");
    test_file sample = test_read_file(sample_path, &copy_arena);
    snprintf(path, MAX_PATH, "%s\\load_save_test_bad_magic.tmp", exe_dir);
    file = fopen(path, "wb");
    if (file && sample.contents)
    {
        sample.contents[0x008] = 'X';   /* save-file.md: magic text at 0x008 */
        fwrite(sample.contents, 1, (size_t)sample.size, file);
    }
    if (file) fclose(file);
    engine_load_result bad_magic = api->load_save(e, "load_save_test_bad_magic.tmp");
    TEST_CHECK(counts, bad_magic.status == ENGINE_LOAD_BAD_SAVE && strcmp(bad_magic.detail, "not a save") == 0,
               "a file of save size with a wrong magic text is not a save");
    remove(path);
    test_arena_destroy(&copy_arena);

    TEST_CHECK(counts, api->officer_count(e) == 0 && api->ledger_count(e) == 0 && api->entities_alive(e) == 0,
               "a refused load changes nothing");

    engine_fixture_destroy(&f);

    memory_arena permanent = test_arena_create(Megabytes(8));
    memory_arena transient = test_arena_create(Megabytes(8));
    engine_config config = engine_default_config();   /* no file reader */
    engine *backend = engine_create(&permanent, &transient, config);
    engine_api bare = engine_make_api(backend);
    TEST_CHECK(counts, bare.load_save(bare.engine, SAMPLE_SAVE_RELATIVE_PATH).status == ENGINE_LOAD_NO_FILE_READER,
               "an engine without a file reader refuses to load");
    test_arena_destroy(&transient);
    test_arena_destroy(&permanent);
}

internal void
feature_sample_identity_tests(test_counts *counts, engine_api *api)
{
    engine_officer officer = {};
    TEST_CHECK(counts, feature_find_officer(api, 343, &officer), "officer 343 is loaded");
    TEST_CHECK(counts, officer.leadership == 96 && officer.war == 72 && officer.intelligence == 91 &&
                       officer.politics == 94 && officer.charm == 96, "officer 343 has stats 96/72/91/94/96");
    TEST_CHECK(counts, officer.birth_year == 155 && officer.death_year == 220, "officer 343 was born 155 and died 220");
    TEST_CHECK(counts, officer.force_id == 10 && officer.status == 3, "officer 343 is in force 10 with status 3");
    TEST_CHECK(counts, feature_name_is_set(officer.family_name, sizeof(officer.family_name)) &&
                       feature_name_is_set(officer.courtesy_name, sizeof(officer.courtesy_name)),
               "officer 343 has a family name and a courtesy name");

    TEST_CHECK(counts, feature_find_officer(api, 442, &officer), "officer 442 is loaded");
    TEST_CHECK(counts, officer.status == 0 && officer.force_id == 9 && officer.base_location == 36,
               "officer 442 is the ruler of force 9 in city 36");

    TEST_CHECK(counts, feature_find_officer(api, 98, &officer), "officer 98 is loaded");
    TEST_CHECK(counts, officer.sworn_brother_group_head == 635, "officer 98 has sworn-brother head 635");
}

/* Observed ranges of the officer record table that the sample agrees with. */
internal void
feature_sample_officer_range_tests(test_counts *counts, engine_api *api)
{
    u32 count = api->officer_count(api->engine);
    b32 statuses_known = true, compatibility_in_range = true, skills_in_range = true;
    b32 locations_in_range = true, built_in_in_order = true, names_set = true;
    for (u32 index = 0; index < count; ++index)
    {
        engine_officer officer = api->officer_at(api->engine, index);
        if (index < SPEC_BUILT_IN_OFFICERS && officer.officer_id != index) built_in_in_order = false;
        if (officer.status > 8 && officer.status != 255) statuses_known = false;
        if (officer.compatibility > 149) compatibility_in_range = false;
        if (officer.skill_number > 99 && officer.skill_number != 255) skills_in_range = false;
        if (officer.base_location > 86 && officer.base_location != 0xFFFF) locations_in_range = false;
        if (!feature_name_is_set(officer.family_name, sizeof(officer.family_name))) names_set = false;
    }
    TEST_CHECK(counts, count >= SPEC_BUILT_IN_OFFICERS && count <= SPEC_OFFICER_SLOTS,
               "officer count: the 852 built-in officers plus at most the temporary slots");
    TEST_CHECK(counts, built_in_in_order, "the first 852 officers are ids 0..851 in slot order");
    TEST_CHECK(counts, names_set, "every loaded officer has a family name");
    TEST_CHECK(counts, statuses_known, "officer status is 0..8 or 255");
    TEST_CHECK(counts, compatibility_in_range, "officer compatibility is 0..149");
    TEST_CHECK(counts, skills_in_range, "officer skill is 0..99 or 255");
    TEST_CHECK(counts, locations_in_range, "officer base location is a city, gate or port, or none");
}

/* Observed ranges of the city record table that the sample agrees with,
   and the cross-checks: the commander is a ruler, viceroy or governor of
   the owner force in that city, and every officer with status 0..3 in an
   owned city belongs to the owner force. */
internal void
feature_sample_city_tests(test_counts *counts, engine_api *api)
{
    u32 count = api->city_count(api->engine);
    TEST_CHECK(counts, count == SPEC_CITY_COUNT, "42 cities");
    b32 ids_in_order = true, capacity_known = true, ownerless_empty = true, owned_in_range = true;
    b32 order_in_range = true, durability_known = true, commanders_agree = true, officers_agree = true;
    u32 officer_count = api->officer_count(api->engine);
    for (u32 index = 0; index < count; ++index)
    {
        engine_city city = api->city_at(api->engine, index);
        if (city.city_id != index) ids_in_order = false;
        if (city.capacity_constant != 100000 && city.capacity_constant != 150000) capacity_known = false;
        if (city.public_order < 30 || city.public_order > 97) order_in_range = false;
        if (city.durability_maximum < 4000 || city.durability_maximum > 8000 || city.durability_maximum % 1000) durability_known = false;
        if (city.owner_force_id == 255)
        {
            if (city.gold != 0 || city.troops != 0) ownerless_empty = false;
            continue;
        }
        if (city.gold < 3184 || city.gold > 82767 || city.troops < 31 || city.troops > 45395 ||
            city.food < 14192 || city.food > 233450) owned_in_range = false;

        engine_officer commander = {};
        if (city.commanding_officer_id != 0xFFFF &&
            (!feature_find_officer(api, city.commanding_officer_id, &commander) || commander.status > 2 ||
             commander.base_location != city.city_id || commander.force_id != city.owner_force_id))
        {
            commanders_agree = false;
        }
        for (u32 slot = 0; slot < officer_count; ++slot)
        {
            engine_officer officer = api->officer_at(api->engine, slot);
            if (officer.status <= 3 && officer.base_location == city.city_id && officer.force_id != city.owner_force_id)
            {
                officers_agree = false;
            }
        }
    }
    TEST_CHECK(counts, ids_in_order, "cities are ids 0..41 in order");
    TEST_CHECK(counts, capacity_known, "city capacity constant is 100,000 or 150,000");
    TEST_CHECK(counts, ownerless_empty, "ownerless cities have 0 gold and 0 troops");
    TEST_CHECK(counts, owned_in_range, "owned cities: gold, troops and food in the observed ranges");
    TEST_CHECK(counts, order_in_range, "city public order is 30..97");
    TEST_CHECK(counts, durability_known, "city durability maximum is 4,000..8,000 in steps of 1,000");
    TEST_CHECK(counts, commanders_agree, "each city commander is a ruler, viceroy or governor of the owner force in that city");
    TEST_CHECK(counts, officers_agree, "officers with status 0..3 in an owned city belong to the owner force");
}

internal void
feature_load_save_tests(test_counts *counts)
{
    feature_load_failure_tests(counts);

    engine_fixture f;
    engine_fixture_init(&f, 1, 64);
    engine_api *api = &f.api;
    void *e = api->engine;

    engine_load_result loaded = api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
    TEST_CHECK(counts, loaded.status == ENGINE_LOAD_OK && loaded.command_id != 0, "sample.s11 loads through the api");
    TEST_CHECK(counts, api->ledger_count(e) >= 1, "a load ledgers its event, then the start of the turn");
    sim_event event = api->ledger_at(e, 0);
    TEST_CHECK(counts, event.kind == SIM_EVENT_SAVE_LOADED && event.cause == loaded.command_id &&
                       event.delta == (s32)api->officer_count(e), "the event names its load and the officer count");
    TEST_CHECK(counts, api->turn(e) == 0, "a load does not change the turn");

    feature_sample_identity_tests(counts, api);
    feature_sample_officer_range_tests(counts, api);
    feature_sample_city_tests(counts, api);

    u32 officers = api->officer_count(e);
    TEST_CHECK(counts, api->entities_alive(e) == officers + api->city_count(e), "one entity per officer and per city");
    engine_load_result again = api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
    TEST_CHECK(counts, again.status == ENGINE_LOAD_OK && again.command_id > loaded.command_id, "a second load is a new command");
    TEST_CHECK(counts, api->officer_count(e) == officers && api->entities_alive(e) == officers + api->city_count(e),
               "a second load replaces the first and leaves no entities behind");

    api->end_turn(e);
    engine_officer before = {}, after = {};
    feature_find_officer(api, 343, &before);
    api->load_save(e, "no-such-save.s11");
    feature_find_officer(api, 343, &after);
    TEST_CHECK(counts, api->officer_count(e) == officers && memcmp(&before, &after, sizeof(before)) == 0,
               "a failed load keeps the loaded game");

    engine_fixture_destroy(&f);
}
