/* Feature tests of the queries that give names to the id columns of the
   panels: officer_by_id, force_ruler and location_name_at, through
   engine_api with the real engine, sample.s11 and Scenario.s11.

   Expected values come from spec-src/spec/formats/save-file.md (officer 442
   is the ruler of force 9; status 0 is the ruler; the tribal templates of
   forces 42..46 have no ruler) and scenario-file.md (Scenario.s11: the
   gates and ports are entries 42..86 after the 42 cities). */

#define SPEC_TRIBAL_FORCE_ID        42     /* save-file.md: tribal templates use forces 42..46 */
#define SPEC_FIRST_GATE_ID          42     /* scenario-file.md: the first gate */
#define SPEC_LAST_GATE_ID           86     /* scenario-file.md: the last port */
#define SPEC_HULAO_GATE_ID          43     /* a gate with a name of three characters */
#define SPEC_FIRST_PORT_ID          51     /* the first port, a name of three characters */

/* A new game has no officers and no names: each query gives "none". */
internal void
feature_display_names_empty_tests(test_counts *counts)
{
    engine_fixture f;
    engine_fixture_init(&f, 1, 64);
    engine_api *api = &f.api;
    TEST_CHECK(counts, api->force_ruler(api->engine, SPEC_ZHANG_LU_FORCE_ID) == ENGINE_NO_OFFICER,
               "a new game has no ruler");
    TEST_CHECK(counts, api->officer_by_id(api->engine, SPEC_ZHANG_LU_OFFICER_ID).officer_id == ENGINE_NO_OFFICER,
               "a new game has no officer");
    TEST_CHECK(counts, api->location_name_at(api->engine, SPEC_HANZHONG_CITY_ID).location == ENGINE_NO_LOCATION,
               "a new game has no location names");
    engine_fixture_destroy(&f);
}

/* officer_by_id gives the same record as officer_at for each loaded officer. */
internal void
feature_officer_by_id_tests(test_counts *counts, engine_api *api)
{
    b32 all_agree = true;
    u32 count = api->officer_count(api->engine);
    for (u32 index = 0; index < count; ++index)
    {
        engine_officer by_index = api->officer_at(api->engine, index);
        engine_officer by_id    = api->officer_by_id(api->engine, by_index.officer_id);
        if (memcmp(&by_index, &by_id, sizeof(by_id)) != 0) all_agree = false;
    }
    TEST_CHECK(counts, count > 0 && all_agree, "officer_by_id(id) is officer_at of the row with that id, for each officer");
    TEST_CHECK(counts, api->officer_by_id(api->engine, SPEC_OFFICER_SLOTS).officer_id == ENGINE_NO_OFFICER,
               "an id past the officer slots is no officer");
    TEST_CHECK(counts, api->officer_by_id(api->engine, ENGINE_NO_OFFICER).officer_id == ENGINE_NO_OFFICER,
               "the none id is no officer");
}

/* force_ruler: the officer with status 0 of the force. */
internal void
feature_force_ruler_tests(test_counts *counts, engine_api *api)
{
    TEST_CHECK(counts, api->force_ruler(api->engine, SPEC_ZHANG_LU_FORCE_ID) == SPEC_ZHANG_LU_OFFICER_ID,
               "the ruler of force 9 is officer 442");
    TEST_CHECK(counts, api->force_ruler(api->engine, ENGINE_NO_FACTION) == ENGINE_NO_OFFICER, "force 255 (none) has no ruler");
    TEST_CHECK(counts, api->force_ruler(api->engine, SPEC_TRIBAL_FORCE_ID) == ENGINE_NO_OFFICER,
               "a tribal force has no ruler: its leader has status 1");

    b32 rulers_agree = true, rulers_distinct = true, owners_led = true;
    u32 owners_without_ruler = 0;
    u16 seen[256];
    u32 seen_count = 0;
    for (u32 force = 0; force < ENGINE_NO_FACTION; ++force)
    {
        u16 ruler_id = api->force_ruler(api->engine, (u8)force);
        if (ruler_id == ENGINE_NO_OFFICER) continue;
        engine_officer ruler = api->officer_by_id(api->engine, ruler_id);
        if (ruler.status != 0 || ruler.force_id != force) rulers_agree = false;
        for (u32 index = 0; index < seen_count; ++index) if (seen[index] == ruler_id) rulers_distinct = false;
        seen[seen_count++] = ruler_id;
    }
    TEST_CHECK(counts, seen_count > 0 && rulers_agree, "each ruler has status 0 and the force id that it rules");
    TEST_CHECK(counts, rulers_distinct, "no officer rules two forces");

    /* sample.s11: two forces own cities with a status 1 leader and no
       status 0 officer (save-file.md names such leaders; Q-149). */
    b32 leaders_agree = true;
    u32 factions = api->faction_count(api->engine);
    for (u32 index = 0; index < factions; ++index)
    {
        engine_faction faction = api->faction_at(api->engine, index);
        u16 ruler = api->force_ruler(api->engine, faction.force_id);
        if (ruler != ENGINE_NO_OFFICER)
        {
            if (faction.leader_officer_id != ruler) leaders_agree = false;
            continue;
        }
        owners_without_ruler++;
        engine_officer leader = api->officer_by_id(api->engine, faction.leader_officer_id);
        if (leader.status != 1 || leader.force_id != faction.force_id) owners_led = false;
    }
    TEST_CHECK(counts, factions > 0 && leaders_agree, "the leader of a faction with a ruler is its ruler");
    TEST_CHECK(counts, owners_without_ruler == 2 && owners_led,
               "the two factions without a ruler have a leader with status 1 (viceroy)");
}

/* location_name_at: the city entries for 0..41, the gate and port entries
   for 42..86, the bytes that name_index_load decodes. */
internal void
feature_location_name_tests(test_counts *counts, engine_api *api)
{
    memory_arena arena = test_arena_create(Megabytes(1));
    char path[MAX_PATH];
    test_fixture_path(path, MAX_PATH, NAME_INDEX_FIXTURE);
    test_file file = test_read_file(path, &arena);
    name_index expected = {};
    TEST_CHECK(counts, file.contents && name_index_load(file.contents, file.size, &expected) == SCENARIO_LOAD_OK,
               "the test reads Scenario.s11 for the expected bytes");

    b32 cities_agree = true;
    for (u32 city = 0; city < SCENARIO_CITY_COUNT; ++city)
    {
        engine_location_name location = api->location_name_at(api->engine, (u16)city);
        engine_city_name name = api->city_name_at(api->engine, city);
        if (location.location != city || memcmp(location.name, name.name, sizeof(name.name)) != 0) cities_agree = false;
    }
    TEST_CHECK(counts, cities_agree, "locations 0..41 have the names of the cities");

    b32 gates_agree = true, gates_named = true;
    for (u32 gate = SPEC_FIRST_GATE_ID; gate <= SPEC_LAST_GATE_ID; ++gate)
    {
        engine_location_name location = api->location_name_at(api->engine, (u16)gate);
        const u8 *bytes = expected.gates_and_ports[gate - SPEC_FIRST_GATE_ID].name;
        if (location.location != gate || memcmp(location.name, bytes, sizeof(location.name)) != 0) gates_agree = false;
        if (fixture_name_length(location.name, sizeof(location.name)) == 0) gates_named = false;
    }
    TEST_CHECK(counts, gates_agree, "locations 42..86 have the bytes of the gate and port entries");
    TEST_CHECK(counts, gates_named, "each gate and port has a name");
    TEST_CHECK(counts, fixture_name_length(api->location_name_at(api->engine, SPEC_FIRST_GATE_ID).name, NAME_INDEX_GATE_ENTRY_SIZE) ==
                       2 * SPEC_BIG5_CHARACTER_BYTES, "location 42 has a name of two characters");
    TEST_CHECK(counts, fixture_name_length(api->location_name_at(api->engine, SPEC_HULAO_GATE_ID).name, NAME_INDEX_GATE_ENTRY_SIZE) ==
                       3 * SPEC_BIG5_CHARACTER_BYTES, "location 43 has a name of three characters");
    TEST_CHECK(counts, fixture_name_length(api->location_name_at(api->engine, SPEC_FIRST_PORT_ID).name, NAME_INDEX_GATE_ENTRY_SIZE) ==
                       3 * SPEC_BIG5_CHARACTER_BYTES, "location 51, the first port, has a name of three characters");
    TEST_CHECK(counts, api->location_name_at(api->engine, SPEC_LAST_GATE_ID + 1).location == ENGINE_NO_LOCATION,
               "location 87 (a unit) has no name");
    TEST_CHECK(counts, api->location_name_at(api->engine, ENGINE_NO_LOCATION).location == ENGINE_NO_LOCATION,
               "the none location has no name");
    test_arena_destroy(&arena);
}

internal void
feature_display_name_tests(test_counts *counts)
{
    feature_display_names_empty_tests(counts);

    engine_fixture f;
    engine_fixture_init(&f, 1, 64);
    engine_api *api = &f.api;
    void *e = api->engine;

    TEST_CHECK(counts, api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH).status == ENGINE_LOAD_OK, "sample.s11 loads");
    feature_officer_by_id_tests(counts, api);
    feature_force_ruler_tests(counts, api);
    TEST_CHECK(counts, api->location_name_at(e, SPEC_HANZHONG_CITY_ID).location == ENGINE_NO_LOCATION,
               "a save without a name index has no location names");

    TEST_CHECK(counts, api->load_name_index(e, NAME_INDEX_RELATIVE_PATH).status == ENGINE_LOAD_OK, "Scenario.s11 loads");
    feature_location_name_tests(counts, api);
    TEST_CHECK(counts, api->force_ruler(e, SPEC_ZHANG_LU_FORCE_ID) == SPEC_ZHANG_LU_OFFICER_ID,
               "a name index load keeps the rulers");

    engine_fixture_destroy(&f);
}
