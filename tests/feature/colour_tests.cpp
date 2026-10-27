/* Feature tests of the flag colours of the factions (faction_colour,
   docs/GRAPHICS.md, "Faction colours"), through engine_api with the real
   engine. Each scenario of each content pack starts, and the test checks
   the colours of all factions of its turn order:
   - no two factions have the same colour;
   - two factions whose rulers have the same family name (the many 劉)
     have colours at least FACTION_COLOUR_SAME_NAME_DISTANCE apart.
   A faction keeps its colour from the new game of 群雄割據 to the save
   that started from it. */

/* The officer whose family name the flag shows. It is the ruler. A corps
   of a save has no ruler, so then it is the leader of corps 1. */
internal engine_officer
feature_flag_officer(engine_api *api, engine_faction *faction)
{
    u16 ruler = api->force_ruler(api->engine, faction->force_id);
    if (ruler == ENGINE_NO_OFFICER) ruler = faction->leader_officer_id;
    return api->officer_by_id(api->engine, ruler);
}

/* The RGBA8 word of faction_colour back to 0xRRGGBB. */
internal u32
feature_rgba_to_rgb(u32 rgba)
{
    return ((rgba & 0xFF) << 16) | (rgba & 0xFF00) | ((rgba >> 16) & 0xFF);
}

/* Checks the rule of the colours on the factions of the loaded game.
   *same_name_pairs gets the number of pairs with the same family name. */
internal void
feature_check_faction_colours(test_counts *counts, engine_api *api, const char *game, u32 *same_name_pairs)
{
    void *e = api->engine;
    u32 count = api->faction_count(e);
    b32 distinct = true, apart = true;
    u32 pairs = 0, nearest = 765;
    for (u32 index = 0; index < count; ++index)
    {
        engine_faction faction = api->faction_at(e, index);
        engine_officer officer = feature_flag_officer(api, &faction);
        u32 colour = api->faction_colour(e, faction.force_id);
        for (u32 before = 0; before < index; ++before)
        {
            engine_faction other = api->faction_at(e, before);
            engine_officer other_officer = feature_flag_officer(api, &other);
            u32 other_colour = api->faction_colour(e, other.force_id);
            if (colour == other_colour) distinct = false;
            if (!faction_same_family(officer.family_name, other_officer.family_name)) continue;
            u32 distance = faction_colour_distance(feature_rgba_to_rgb(colour), feature_rgba_to_rgb(other_colour));
            if (distance < nearest) nearest = distance;
            if (distance < FACTION_COLOUR_SAME_NAME_DISTANCE) apart = false;
            pairs++;
        }
    }
    char what[160];
    snprintf(what, sizeof(what), "%s: the %u factions have %u different colours", game, count, count);
    TEST_CHECK(counts, count > 1 && distinct, what);
    snprintf(what, sizeof(what), "%s: %u pairs of factions with the same family name, the nearest pair %u apart (at least %u)",
             game, pairs, pairs ? nearest : 0, FACTION_COLOUR_SAME_NAME_DISTANCE);
    TEST_CHECK(counts, apart, what);
    *same_name_pairs = pairs;
}

/* Starts each available scenario of the pack and checks its colours. */
internal void
feature_pack_colour_tests(test_counts *counts, const char *pack)
{
    engine_fixture fixture;
    fixture.permanent = test_arena_create(Megabytes(8));
    fixture.transient = test_arena_create(Megabytes(8));
    engine_config config = engine_default_config();
    config.read_entire_file = test_platform_read_entire_file;
    config.use_pack_map     = false;
    config.content_folder   = pack;
    fixture.backend = engine_create(&fixture.permanent, &fixture.transient, config);
    fixture.api     = engine_make_api(fixture.backend);
    engine_api *api = &fixture.api;
    void *e = api->engine;
    u32 checked = 0;
    for (u32 index = 0; index < api->scenario_count(e); ++index)
    {
        engine_scenario scenario = api->scenario_at(e, index);
        if (!scenario.available) continue;
        engine_load_result loaded = scenario.kind == ENGINE_SCENARIO_SAVE ? api->load_save(e, scenario.file) : api->load_scenario(e, scenario.file);
        char what[160];
        snprintf(what, sizeof(what), "%s: the scenario %s loads", pack, scenario.key);
        TEST_CHECK(counts, loaded.status == ENGINE_LOAD_OK, what);
        if (loaded.status != ENGINE_LOAD_OK) continue;
        snprintf(what, sizeof(what), "%s, %s", pack, scenario.key);
        u32 pairs = 0;
        feature_check_faction_colours(counts, api, what, &pairs);
        checked++;
    }
    char what[160];
    snprintf(what, sizeof(what), "%s: each available scenario has a check of its colours", pack);
    TEST_CHECK(counts, checked > 0, what);
    TEST_CHECK(counts, api->faction_colour(e, ENGINE_NO_FACTION) == faction_rgb_to_rgba(FACTION_COLOUR_NONE_RGB), "a place without a faction is grey");
    engine_fixture_destroy(&fixture);
}

/* The new game of 群雄割據 and the save that started from it: the factions
   of both keep their colours. The colour comes from the force id and the
   family names, so a load gives the colour again. */
internal void
feature_colour_save_tests(test_counts *counts)
{
    engine_fixture fixture;
    engine_fixture_init(&fixture, 1, 65536);
    engine_api *api = &fixture.api;
    void *e = api->engine;
    engine_scenario save = api->scenario_at(e, 0), warlords = api->scenario_at(e, 1);
    TEST_CHECK(counts, api->load_scenario(e, warlords.file).status == ENGINE_LOAD_OK, "the new game of 群雄割據 loads");
    u32 colours[256] = {};
    b32 in_new_game[256] = {};
    for (u32 index = 0; index < api->faction_count(e); ++index)
    {
        u8 force = api->faction_at(e, index).force_id;
        colours[force]     = api->faction_colour(e, force);
        in_new_game[force] = true;
    }
    u32 same_name_pairs = 0;
    feature_check_faction_colours(counts, api, "群雄割據, new game", &same_name_pairs);
    TEST_CHECK(counts, same_name_pairs >= 3, "群雄割據 has at least three pairs of rulers with the same family name (劉備, 劉表, 劉璋, 劉繇)");
    TEST_CHECK(counts, api->load_save(e, save.file).status == ENGINE_LOAD_OK, "the save of 群雄割據 loads");
    u32 kept = 0, shared = 0;
    for (u32 index = 0; index < api->faction_count(e); ++index)
    {
        u8 force = api->faction_at(e, index).force_id;
        if (!in_new_game[force]) continue;
        shared++;
        kept += api->faction_colour(e, force) == colours[force];
    }
    TEST_CHECK(counts, shared > 3 && kept == shared, "each faction of the save has the colour that it had in the new game");
    engine_fixture_destroy(&fixture);
}

internal void
feature_colour_tests(test_counts *counts)
{
    feature_pack_colour_tests(counts, HEXDOM_DATA_ROOT "/" HEXDOM_CONTENT_PACK);
    feature_pack_colour_tests(counts, HEXDOM_DATA_ROOT "/civil_war");
    feature_colour_save_tests(counts);
}
