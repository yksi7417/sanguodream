/* Unit tests for code/sim: PRNG, commands, events, sinks, and sim_load_save. */

/* A decoded save with officers in slots 0, 2 and 5 (slot 1 and the rest
   empty) and two cities, built directly as records. */
internal void
unit_sim_build_save(save_file *save, officer_record *officers, u32 slot_count, city_record *cities)
{
    *save = {};
    memset(officers, 0, sizeof(officer_record) * slot_count);
    u16 named_slots[3] = { 0, 2, 5 };
    for (u32 index = 0; index < 3; ++index)
    {
        officer_record *officer = officers + named_slots[index];
        officer->family_name[0] = 0xB1;
        officer->family_name[1] = (u8)(0x40 + index);
        officer->leadership     = (u8)(60 + index);
        officer->force_id       = 9;
        officer->status         = OFFICER_STATUS_REGULAR_OFFICER;
    }
    for (u32 slot = 0; slot < slot_count; ++slot) officers[slot].id = (u16)slot;
    memset(cities, 0, sizeof(city_record) * 2);
    cities[0].id = 0;  cities[0].owner_force_id = CITY_OWNER_NONE; cities[0].commanding_officer_id = CITY_COMMANDER_NONE;
    cities[1].id = 1;  cities[1].owner_force_id = 9; cities[1].commanding_officer_id = 2; cities[1].gold = 5000;
    save->officers           = officers;
    save->officer_slot_count = slot_count;
    save->cities             = cities;
    save->city_count         = 2;
}

internal void
unit_sim_load_save_tests(test_counts *counts)
{
    memory_arena arena = test_arena_create(Megabytes(2));
    sim_state *sim = PushStruct(&arena, sim_state);
    sim_init(sim, &arena, 1);
    TEST_CHECK(counts, sim->officers.count == 0 && sim->cities.count == 0, "a fresh sim has no officers or cities");

    officer_record officers[8];
    city_record cities[2];
    save_file save;
    unit_sim_build_save(&save, officers, ArrayCount(officers), cities);

    sim_event_sink sink = sim_sink_from_arena(&arena, 4);
    sim_command load = sim_make_command(sim, SIM_COMMAND_LOAD_SAVE);
    sim_load_save(sim, &save, load, &sink);
    TEST_CHECK(counts, sim->officers.count == 3, "one officer per slot with a name");
    TEST_CHECK(counts, sim->officers.data[0].officer_id == 0 && sim->officers.data[1].officer_id == 2 &&
                       sim->officers.data[2].officer_id == 5, "officers keep their slot ids in slot order");
    TEST_CHECK(counts, sim->officers.data[1].leadership == 61 && sim->officers.data[1].family_name[1] == 0x41,
               "officer fields and name bytes are copied");
    TEST_CHECK(counts, sim->cities.count == 2 && sim->cities.data[1].city_id == 1 && sim->cities.data[1].gold == 5000 &&
                       sim->cities.data[1].commanding_officer_id == 2, "one city per city record");
    TEST_CHECK(counts, sim->entities.alive_count == 5, "one entity per officer and per city");
    TEST_CHECK(counts, sink.count == 1 && sink.events[0].kind == SIM_EVENT_SAVE_LOADED && sink.events[0].delta == 3 &&
                       sink.events[0].cause == load.id, "a load emits save_loaded with the officer count");

    officers[2].family_name[0] = 0;
    officers[2].family_name[1] = 0;
    sim_load_save(sim, &save, sim_make_command(sim, SIM_COMMAND_LOAD_SAVE), &sink);
    TEST_CHECK(counts, sim->officers.count == 2 && sim->entities.alive_count == 4, "a second load replaces the first");

    sim_unload_world(sim);
    TEST_CHECK(counts, sim->officers.count == 0 && sim->cities.count == 0 && sim->entities.alive_count == 0,
               "unload destroys every officer and city entity");
    TEST_CHECK(counts, strcmp(sim_event_name(SIM_EVENT_SAVE_LOADED), "save_loaded") == 0 &&
                       strcmp(sim_command_name(SIM_COMMAND_LOAD_SAVE), "load_save") == 0, "load names");
    test_arena_destroy(&arena);
}

internal void
unit_sim_tests(test_counts *counts)
{
    memory_arena arena = test_arena_create(Megabytes(2));

    sim_state *first  = PushStruct(&arena, sim_state);
    sim_state *second = PushStruct(&arena, sim_state);
    sim_state *third  = PushStruct(&arena, sim_state);
    sim_init(first,  &arena, 42);
    sim_init(second, &arena, 42);
    sim_init(third,  &arena, 43);
    TEST_CHECK(counts, first->turn == 0 && first->next_command_id == 1, "fresh sim starts at turn 0");

    b32 same = true, differs = false;
    for (u32 i = 0; i < 1000; ++i)
    {
        u32 x = sim_random_u32(first), y = sim_random_u32(second), z = sim_random_u32(third);
        if (x != y) same = false;
        if (x != z) differs = true;
    }
    TEST_CHECK(counts, same, "prng deterministic for equal seeds");
    TEST_CHECK(counts, differs, "prng differs for different seeds");

    b32 in_range = true;
    u32 histogram[7] = {};
    for (u32 i = 0; i < 7000; ++i)
    {
        u32 v = sim_random_below(first, 7);
        if (v >= 7) in_range = false; else histogram[v]++;
    }
    b32 spread = true;
    for (u32 i = 0; i < 7; ++i) if (histogram[i] < 700 || histogram[i] > 1300) spread = false;
    TEST_CHECK(counts, in_range, "random_below stays in range");
    TEST_CHECK(counts, spread, "random_below is roughly uniform");

    sim_state *zero_seed = PushStruct(&arena, sim_state);
    sim_init(zero_seed, &arena, 0);
    TEST_CHECK(counts, zero_seed->rng != 0, "seed 0 is replaced (xorshift must not start at 0)");

    sim_command c1 = sim_make_command(first, SIM_COMMAND_END_TURN);
    sim_command c2 = sim_make_command(first, SIM_COMMAND_NONE);
    TEST_CHECK(counts, c1.id == 1 && c2.id == 2, "command ids count up from 1");

    sim_event_sink sink = sim_sink_from_arena(&arena, 2);
    sim_apply(first, c1, &sink);
    TEST_CHECK(counts, first->turn == 1, "end_turn advances the turn");
    TEST_CHECK(counts, sink.count == 1 && sink.events[0].kind == SIM_EVENT_TURN_ADVANCED, "end_turn emits turn_advanced");
    TEST_CHECK(counts, sink.events[0].turn == 1 && sink.events[0].cause == c1.id, "event carries turn and causing command");

    sim_apply(first, c2, &sink);
    TEST_CHECK(counts, first->turn == 1 && sink.count == 1, "none command does nothing");

    sim_event extra = {};
    extra.kind = SIM_EVENT_TURN_ADVANCED;
    sim_emit(&sink, extra);
    sim_emit(&sink, extra);
    TEST_CHECK(counts, sink.count == 2, "a full sink drops events instead of overflowing");

    TEST_CHECK(counts, strcmp(sim_event_name(SIM_EVENT_TURN_ADVANCED), "turn_advanced") == 0, "event name");
    TEST_CHECK(counts, strcmp(sim_command_name(SIM_COMMAND_END_TURN), "end_turn") == 0, "command name");

    test_arena_destroy(&arena);
    unit_sim_load_save_tests(counts);
}
