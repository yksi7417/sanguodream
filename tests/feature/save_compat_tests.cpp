/* The saves of older builds (the play report of 2026-10-23). The player
   loaded a slot of the build of 2026-10-22 with the build of 2026-10-23.
   The header check passed, because sizeof(sim_state) was the same, but a
   new rule in sim_rules had moved every member after content by four
   bytes. The game then showed the year 12 and the wrong ruler.
   tests/fixtures/saves/slot_01.hxs is that slot: 197/10/1, the player is
   the faction 6. It must load with every later build, or the build must
   refuse it with ENGINE_SAVE_OTHER_VERSION. A load that gives another
   state is the failure that this file guards against. When a change of
   sim_state breaks this test, add a migration to engine_saves.cpp, or
   make the load refuse the old files. Never let the test go. */

#define FEATURE_COMPAT_SAVE_FOLDER "..\\tests\\fixtures\\saves"
#define FEATURE_LAYOUT_SLOT        29   /* slot_30.hxs of test_saves: the layout test writes it */

internal void
feature_save_compat_tests(test_counts *counts)
{
    engine_fixture fixture;
    feature_save_folder = FEATURE_COMPAT_SAVE_FOLDER;
    engine_fixture_init_real(&fixture, 1, 65536);
    feature_save_folder = FEATURE_TEST_SAVE_FOLDER;
    engine_api *api = &fixture.api;
    void *e = api->engine;
    api->refresh_save_slots(e);
    engine_save_slot slot = api->save_slot(e, 0);
    TEST_CHECK(counts, slot.used && slot.year == 197 && slot.month == 10 && slot.day == 1 && slot.player == 6,
               "the slot of the build of 2026-10-22 reads: 197/10/1, the faction 6");
    engine_content_status before = api->content_status(e);
    engine_save_status status = api->load_game(e, 0);
    engine_date date = api->date(e);
    engine_content_status after = api->content_status(e);
    char name[300];
    snprintf(name, sizeof(name), "the save of the build of 2026-10-22 loads (status %u) with its date 197/10/1 (got %u/%u/%u) and its player 6 (got %u)",
             status, date.year, date.month, date.day, api->whose_turn(e));
    TEST_CHECK(counts, status == ENGINE_SAVE_OK && date.year == 197 && date.month == 10 && date.day == 1 && api->whose_turn(e) == 6, name);
    TEST_CHECK(counts, after.rule_count == before.rule_count && after.tactic_count == before.tactic_count,
               "the tables and the rules come from the pack of this build, not from the file");
    u16 ruler = api->force_ruler(e, 6);
    TEST_CHECK(counts, ruler != ENGINE_NO_OFFICER && api->officer_by_id(e, ruler).force_id == 6 && api->city_count(e) == 42,
               "the player has its ruler, and the 42 cities are there");
    /* The members after content: no event waits, no duel runs, no debate runs, and the technique points are sane. */
    engine_pending_event pending = api->pending_event(e);
    engine_duel_view duel = api->duel_view(e);
    engine_debate_view debate = api->debate_view(e);
    u32 points = api->technique_points(e, 6);
    snprintf(name, sizeof(name), "the state after content reads right: no pending event (%u), no duel (%u), no debate (%u), technique points %u",
             pending.active, duel.active, debate.active, points);
    TEST_CHECK(counts, !pending.active && !duel.active && !debate.active && points < 100000, name);
    engine_unit first_unit = api->unit_at(e, 0);
    TEST_CHECK(counts, api->unit_count(e) < 200 && (first_unit.id == ENGINE_NO_UNIT || first_unit.soldiers < 100000),
               "the units after the load are sane");
    /* The slot was saved at the start of the sample: the ruler of the player and the officers of the cities are those of a fresh load. */
    engine_fixture sample;
    engine_fixture_init_real(&sample, 1, 65536);
    sample.api.load_save(sample.api.engine, SAMPLE_SAVE_RELATIVE_PATH);
    u16 fresh_ruler = sample.api.force_ruler(sample.api.engine, 6);
    u16 fresh_governor = sample.api.city_at(sample.api.engine, 0).commanding_officer_id;
    snprintf(name, sizeof(name), "the ruler of the player (%u) and the governor of the city 0 (%u) are those of a fresh load of the sample (%u, %u)",
             ruler, api->city_at(e, 0).commanding_officer_id, fresh_ruler, fresh_governor);
    TEST_CHECK(counts, ruler == fresh_ruler && api->city_at(e, 0).commanding_officer_id == fresh_governor, name);
    engine_fixture_destroy(&sample);
    engine_fixture_destroy(&fixture);

    /* A save of this build carries the layout of sim_state. A file with
       another layout does not load, also with the same struct sizes. */
    engine_fixture_init_real(&fixture, 1, 65536);
    api = &fixture.api;
    e = api->engine;
    api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
    engine_date saved = api->date(e);
    TEST_CHECK(counts, api->save_game(e, FEATURE_LAYOUT_SLOT, "layout") == ENGINE_SAVE_OK, "a game of this build saves");
    char exe_dir[MAX_PATH], path[MAX_PATH];
    test_exe_dir(exe_dir, MAX_PATH);
    snprintf(path, sizeof(path), "%s\\%s\\slot_%02u.hxs", exe_dir, FEATURE_TEST_SAVE_FOLDER, FEATURE_LAYOUT_SLOT + 1);
    FILE *file = fopen(path, "r+b");
    b32 flipped = false;
    if (file)
    {
        u8 byte = 0;
        fseek(file, 12 * 4, SEEK_SET);   /* the layout field, after the twelve counts of the header */
        flipped = fread(&byte, 1, 1, file) == 1;
        byte ^= 1;
        fseek(file, 12 * 4, SEEK_SET);
        flipped = flipped && fwrite(&byte, 1, 1, file) == 1;
        fclose(file);
    }
    status = api->load_game(e, FEATURE_LAYOUT_SLOT);
    date = api->date(e);
    TEST_CHECK(counts, flipped && status == ENGINE_SAVE_OTHER_VERSION && date.year == saved.year && date.month == saved.month,
               "a file with another layout of sim_state does not load, and the game stays as it was");
    engine_fixture_destroy(&fixture);
}
