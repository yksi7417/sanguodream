/* Unit tests for code/sim: PRNG, commands, events, sinks. */

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
}
