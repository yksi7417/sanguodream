/* Behavioral tests of the backend through engine_api: what a front end can
   observe after each command, using the real engine. */

struct engine_fixture
{
    memory_arena permanent;
    memory_arena transient;
    engine      *backend;
    engine_api   api;
};

/* The save folder of the next fixture, relative to the executable. The
   tests write into test_saves; save_compat_tests.cpp reads the slots of
   older builds from the fixtures of the repository. */
#define FEATURE_TEST_SAVE_FOLDER "test_saves"
global_variable const char *feature_save_folder = FEATURE_TEST_SAVE_FOLDER;

/* Initialises in place: the engine keeps pointers to the fixture's arenas,
   so the fixture must live where the caller put it, never in a temporary. */
/* real_map: false keeps the placeholder map of map_width x map_height, the
   synthetic board of the tests of M0 to M9; true loads map\ of the pack (M10). */
internal void
engine_fixture_init_with(engine_fixture *f, u64 seed, u32 ledger_capacity, u32 map_width, u32 map_height, b32 real_map)
{
    f->permanent = test_arena_create(real_map ? Megabytes(48) : Megabytes(8));
    f->transient = test_arena_create(real_map ? Megabytes(16) : Megabytes(8));
    engine_config config = engine_default_config();
    config.seed            = seed;
    config.ledger_capacity = ledger_capacity;
    config.map_width       = map_width;
    config.map_height      = map_height;
    config.use_pack_map    = real_map;
    config.read_entire_file = test_platform_read_entire_file;
    config.write_entire_file = test_platform_write_entire_file;
    config.save_folder      = feature_save_folder;
    f->backend = engine_create(&f->permanent, &f->transient, config);
    f->api     = engine_make_api(f->backend);
    /* The random disasters change the numbers of the other tests: they are
       off unless a test turns them on (disaster_tests.cpp). */
    engine_settings settings = f->api.settings(f->api.engine);
    settings.disasters      = false;
    settings.ability_change = false;   /* M7: no growth with age and no deaths, except in the court tests */
    settings.lifespan       = false;
    settings.ai             = false;   /* M8: no computer commands and no events, except in the rivals tests */
    settings.events         = false;
    f->api.set_settings(f->api.engine, settings);
}

internal void
engine_fixture_init_map(engine_fixture *f, u64 seed, u32 ledger_capacity, u32 map_width, u32 map_height)
{
    engine_fixture_init_with(f, seed, ledger_capacity, map_width, map_height, false);
}

/* The placeholder map: 40 x 30 holds the 42 cities of the placeholder layout. */
internal void
engine_fixture_init(engine_fixture *f, u64 seed, u32 ledger_capacity)
{
    engine_fixture_init_map(f, seed, ledger_capacity, 40, 30);
}

/* M10: the real map of the pack (200 x 200, formats/map-data.md). */
internal void
engine_fixture_init_real(engine_fixture *f, u64 seed, u32 ledger_capacity)
{
    engine_fixture_init_with(f, seed, ledger_capacity, 40, 30, true);
}

internal void
engine_fixture_destroy(engine_fixture *f)
{
    test_arena_destroy(&f->permanent);
    test_arena_destroy(&f->transient);
}

/* The row-major index of a hex in a map of this width (odd-q offset). */
internal u32
hex_map_index_of(u32 width, hex_coord hex)
{
    hex_offset o = hex_to_offset(hex);
    return (u32)o.row * width + (u32)o.col;
}

internal void
feature_engine_tests(test_counts *counts)
{
    engine_fixture f;
    engine_fixture_init_map(&f, 1, 16, 16, 12);
    engine_api *api = &f.api;
    void *e = api->engine;

    TEST_CHECK(counts, api->turn && api->entities_alive && api->entity_capacity && api->ledger_count &&
               api->ledger_at && api->memory && api->map_size && api->cell_cost && api->path_cost &&
               api->end_turn && api->advance_turns && api->ledger_clear && api->set_cell_cost &&
               api->officer_count && api->officer_at && api->city_count && api->city_at && api->load_save &&
               api->city_name_at && api->load_name_index && api->date && api->whose_turn && api->actions_left &&
               api->faction_count && api->faction_at && api->can_issue && api->city_slots && api->facility_at &&
               api->facility_kind_count && api->facility_kind_at && api->content_status && api->command_develop &&
               api->set_faction_human && api->set_city_owner && api->set_city_gold && api->set_actions && api->place_unit,
               "api table is complete");

    /* The map through the api. */
    engine_map_size size = api->map_size(e);
    TEST_CHECK(counts, size.width == 16 && size.height == 12, "map size comes from the config");
    engine_terrain_view start = api->terrain_view(e);
    TEST_CHECK(counts, api->cell_cost(e, 3, 3) == api->terrain_kind_at(e, start.kind[hex_map_index_of(start.width, hex_make(3, 3))]).cost[MOVEMENT_FOOT],
               "a cell of the placeholder map costs the foot cost of its kind in terrain.tsv");
    TEST_CHECK(counts, api->cell_cost(e, -1, 0) == HEX_IMPASSABLE, "off-map cells are impassable");
    /* The placeholder landscape has water and hills; flatten it through the api first. */
    for (u32 row = 0; row < size.height; ++row)
    {
        for (u32 col = 0; col < size.width; ++col)
        {
            hex_offset o; o.col = (s32)col; o.row = (s32)row;
            hex_coord c = hex_from_offset(o);
            api->set_cell_terrain(e, c.q, c.r, 1, 1);
            api->set_cell_cost(e, c.q, c.r, 1);
        }
    }
    engine_terrain_view terrain = api->terrain_view(e);
    TEST_CHECK(counts, terrain.width == 16 && terrain.height == 12 && terrain.kind && terrain.level && terrain.version > 1,
               "terrain_view exposes the cells and a version that moved");
    TEST_CHECK(counts, api->cell_cost(e, 3, 3) == 1 && terrain.kind[0] == 1 && terrain.level[0] == 1, "set_cell_terrain and set_cell_cost are visible through the view");
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

    /* Turns 2 to 4 cross the start of February: the month start and its
       bandit hook are two more events (calendar-and-turn.md). */
    u32 emitted = api->advance_turns(e, 3);
    TEST_CHECK(counts, api->turn(e) == 4, "advance_turns(3) reaches turn 4");
    TEST_CHECK(counts, emitted == 5 && api->ledger_count(e) == 6, "advance_turns reports and ledgers every event");
    u32 turn_events = 0;
    for (u32 i = 0; i < api->ledger_count(e); ++i) if (api->ledger_at(e, i).kind == SIM_EVENT_TURN_ADVANCED) turn_events++;
    TEST_CHECK(counts, turn_events == 4, "one turn_advanced event for each turn");

    sim_event out_of_range = api->ledger_at(e, 999);
    TEST_CHECK(counts, out_of_range.kind == SIM_EVENT_NONE, "ledger_at out of range is the none event");

    api->advance_turns(e, 19);
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
