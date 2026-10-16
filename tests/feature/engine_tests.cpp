/* Behavioral tests of the backend through engine_api: what a front end can
   observe after each command, using the real engine. */

struct engine_fixture
{
    memory_arena permanent;
    memory_arena transient;
    engine      *backend;
    engine_api   api;
};

/* Initialises in place: the engine keeps pointers to the fixture's arenas,
   so the fixture must live where the caller put it, never in a temporary. */
internal void
engine_fixture_init(engine_fixture *f, u64 seed, u32 ledger_capacity)
{
    f->permanent = test_arena_create(Megabytes(8));
    f->transient = test_arena_create(Megabytes(8));
    engine_config config = engine_default_config();
    config.seed            = seed;
    config.ledger_capacity = ledger_capacity;
    config.map_width       = 16;
    config.map_height      = 12;
    config.read_entire_file = test_platform_read_entire_file;
    f->backend = engine_create(&f->permanent, &f->transient, config);
    f->api     = engine_make_api(f->backend);
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
    engine_fixture f;
    engine_fixture_init(&f, 1, 16);
    engine_api *api = &f.api;
    void *e = api->engine;

    TEST_CHECK(counts, api->turn && api->entities_alive && api->entity_capacity && api->ledger_count &&
               api->ledger_at && api->memory && api->map_size && api->cell_cost && api->path_cost &&
               api->end_turn && api->advance_turns && api->ledger_clear && api->set_cell_cost &&
               api->officer_count && api->officer_at && api->city_count && api->city_at && api->load_save &&
               api->city_name_at && api->load_name_index,
               "api table is complete");

    /* The map through the api. */
    engine_map_size size = api->map_size(e);
    TEST_CHECK(counts, size.width == 16 && size.height == 12, "map size comes from the config");
    TEST_CHECK(counts, api->cell_cost(e, 3, 3) == 1, "cells start at the default cost");
    TEST_CHECK(counts, api->cell_cost(e, -1, 0) == HEX_IMPASSABLE, "off-map cells are impassable");
    s32 far_q = 12, far_r = 4;
    u32 straight = api->path_cost(e, 2, 4, far_q, far_r);
    TEST_CHECK(counts, straight == (u32)hex_distance(hex_make(2, 4), hex_make(far_q, far_r)), "open-map path cost is the distance");
    for (s32 r = 0; r < 12; ++r)
    {
        hex_offset w; w.col = 7; w.row = r;
        hex_coord c = hex_from_offset(w);
        api->set_cell_cost(e, c.q, c.r, HEX_IMPASSABLE);
    }
    TEST_CHECK(counts, api->cell_cost(e, hex_from_offset(hex_offset{ 7, 4 }).q, 4) == HEX_IMPASSABLE, "set_cell_cost is visible to cell_cost");
    TEST_CHECK(counts, api->path_cost(e, 2, 4, far_q, far_r) == HEX_NO_PATH, "a full wall leaves no path");
    hex_coord gate = hex_from_offset(hex_offset{ 7, 6 });
    api->set_cell_cost(e, gate.q, gate.r, 2);
    u32 through = api->path_cost(e, 2, 4, far_q, far_r);
    TEST_CHECK(counts, through != HEX_NO_PATH && through > straight, "a gate in the wall makes a longer path");
    TEST_CHECK(counts, api->turn(e) == 0 && api->ledger_count(e) == 0, "map edits do not touch turn or ledger");

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
    engine_fixture g, h;
    engine_fixture_init(&g, 7, 64);
    engine_fixture_init(&h, 7, 64);
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
