/* Behavioral tests of the backend through engine_api: what a front end can
   observe after each command, using the real engine. */

struct engine_fixture
{
    memory_arena permanent;
    memory_arena transient;
    engine      *backend;
    engine_api   api;
};

internal engine_fixture
engine_fixture_create(u64 seed, u32 ledger_capacity)
{
    engine_fixture f;
    f.permanent = test_arena_create(Megabytes(8));
    f.transient = test_arena_create(Megabytes(2));
    f.backend   = engine_create(&f.permanent, &f.transient, seed, ledger_capacity);
    f.api       = engine_make_api(f.backend);
    return f;
}

internal void
engine_fixture_destroy(engine_fixture *f)
{
    test_arena_destroy(&f->permanent);
    test_arena_destroy(&f->transient);
}

internal void
feature_engine_tests(test_counts *counts)
{
    engine_fixture f = engine_fixture_create(1, 16);
    engine_api *api = &f.api;
    void *e = api->engine;

    TEST_CHECK(counts, api->turn && api->entities_alive && api->entity_capacity && api->ledger_count &&
               api->ledger_at && api->memory && api->end_turn && api->advance_turns && api->ledger_clear,
               "api table is complete");

    TEST_CHECK(counts, api->turn(e) == 0, "new game starts at turn 0");
    TEST_CHECK(counts, api->ledger_count(e) == 0, "new game has an empty ledger");
    TEST_CHECK(counts, api->entities_alive(e) == 0 && api->entity_capacity(e) == SIM_ENTITY_CAPACITY, "entity counts");

    u32 command_id = api->end_turn(e);
    TEST_CHECK(counts, api->turn(e) == 1, "end_turn advances to turn 1");
    TEST_CHECK(counts, api->ledger_count(e) == 1, "end_turn ledgers one event");
    sim_event ev = api->ledger_at(e, 0);
    TEST_CHECK(counts, ev.kind == SIM_EVENT_TURN_ADVANCED && ev.turn == 1 && ev.cause == command_id,
               "ledgered event names its cause");

    u32 emitted = api->advance_turns(e, 12);
    TEST_CHECK(counts, api->turn(e) == 13, "advance_turns(12) reaches turn 13");
    TEST_CHECK(counts, emitted == 12 && api->ledger_count(e) == 13, "advance_turns reports and ledgers every event");

    sim_event out_of_range = api->ledger_at(e, 999);
    TEST_CHECK(counts, out_of_range.kind == SIM_EVENT_NONE, "ledger_at out of range is the none event");

    api->advance_turns(e, 10);
    TEST_CHECK(counts, api->ledger_count(e) == 16, "ledger stops at its capacity");
    TEST_CHECK(counts, api->turn(e) == 23, "turns keep advancing when the ledger is full");

    api->ledger_clear(e);
    TEST_CHECK(counts, api->ledger_count(e) == 0 && api->turn(e) == 23, "ledger_clear empties the ledger only");

    engine_memory_stats stats = api->memory(e);
    TEST_CHECK(counts, stats.permanent_used > 0 && stats.permanent_used <= stats.permanent_size &&
               stats.transient_used <= stats.transient_size, "memory stats are sane");

    u32 second_id = api->end_turn(e);
    TEST_CHECK(counts, second_id > command_id, "command ids keep increasing");

    /* Determinism: two engines fed the same commands agree on everything observable. */
    engine_fixture g = engine_fixture_create(7, 64);
    engine_fixture h = engine_fixture_create(7, 64);
    g.api.advance_turns(g.api.engine, 5);
    h.api.advance_turns(h.api.engine, 5);
    b32 agree = g.api.turn(g.api.engine) == h.api.turn(h.api.engine) &&
                g.api.ledger_count(g.api.engine) == h.api.ledger_count(h.api.engine);
    for (u32 i = 0; agree && i < g.api.ledger_count(g.api.engine); ++i)
    {
        sim_event x = g.api.ledger_at(g.api.engine, i), y = h.api.ledger_at(h.api.engine, i);
        if (memcmp(&x, &y, sizeof(x)) != 0) agree = false;
    }
    TEST_CHECK(counts, agree, "same seed and commands give the same ledger");

    engine_fixture_destroy(&h);
    engine_fixture_destroy(&g);
    engine_fixture_destroy(&f);
}
