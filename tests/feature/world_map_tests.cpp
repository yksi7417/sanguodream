/* Feature tests of a map from real geography (docs/MAPS.md): the pack
   data/medieval_europe has the map of misc/make_world_map.py. The same
   engine loads it, the bases stand where the tool put them, and the sample
   save plays a few turns on it. */

#define FEATURE_WORLD_PACK      HEXDOM_DATA_ROOT "/medieval_europe"
#define FEATURE_WORLD_MAX_HEXES 80000    /* max_hexes of map/source/region.tsv */

internal void
feature_world_map_init(engine_fixture *f)
{
    f->permanent = test_arena_create(Megabytes(96));
    f->transient = test_arena_create(Megabytes(32));
    engine_config config = engine_default_config();
    config.seed              = 1;
    config.ledger_capacity   = 65536;
    config.read_entire_file  = test_platform_read_entire_file;
    config.write_entire_file = test_platform_write_entire_file;
    config.save_folder       = feature_save_folder;
    config.content_folder    = FEATURE_WORLD_PACK;
    f->backend = engine_create(&f->permanent, &f->transient, config);
    f->api     = engine_make_api(f->backend);
    engine_settings settings = f->api.settings(f->api.engine);
    settings.disasters = false;
    settings.events    = false;
    f->api.set_settings(f->api.engine, settings);
}

/* True if each city of the save stands on its flower: the centre and its six neighbours. */
internal b32
feature_world_flowers(engine_api *api)
{
    void *e = api->engine;
    if (api->city_count(e) != SAVE_CITY_COUNT) return false;
    for (u32 index = 0; index < api->city_count(e); ++index)
    {
        u8 city = api->city_at(e, index).city_id;
        engine_city_slots site = api->city_slots(e, city);
        if (api->hex_at(e, site.q, site.r).city_id != city || site.slot_count == 0) return false;
        for (u32 direction = 0; direction < HEX_DIRECTION_COUNT; ++direction)
        {
            hex_coord petal = hex_neighbor(hex_make(site.q, site.r), direction);
            if (api->hex_at(e, petal.q, petal.r).city_id != city) return false;
        }
    }
    return true;
}

internal void
feature_world_map_tests(test_counts *counts)
{
    engine_fixture f;
    feature_world_map_init(&f);
    engine_api *api = &f.api;
    void *e = api->engine;
    engine_map_info info = api->map_info(e);
    TEST_CHECK(counts, api->content_status(e).loaded && info.real_map && strcmp(info.detail, "ok") == 0,
               "the engine loads the content and the map of the medieval_europe pack");
    TEST_CHECK(counts, info.width * info.height > 200 * 200 && info.width * info.height <= FEATURE_WORLD_MAX_HEXES,
               "the map is larger than the 200 x 200 map and fits the budget of the terrain mesh");
    TEST_CHECK(counts, info.cities == 42 && info.gates == 10 && info.ports == 35 && info.city_hexes == 294 && info.port_hexes == 35,
               "42 cities of 7 hexes, 10 gates and 35 ports, the same counts as the placeholder map");
    TEST_CHECK(counts, info.gate_hexes == 10 && info.gate_side_cliffs == 20, "the two side hexes of each gate become cliff (F-151)");
    TEST_CHECK(counts, info.slots >= 42 * 10 && info.bandit_sites > 0, "each city has development slots, and the map has bandit sites");

    TEST_CHECK(counts, api->load_save(e, api->scenario_at(e, 0).file).status == ENGINE_LOAD_OK,
               "the sample save loads with the map of the pack");
    TEST_CHECK(counts, feature_world_flowers(api), "each city stands on its flower and has slots on the new map");
    u8 city = feature_busiest_city(api);
    u8 force = api->city_at(e, city).owner_force_id;
    feature_play_as(api, force);
    TEST_CHECK(counts, feature_next_own_turn(api, force) && feature_next_own_turn(api, force),
               "the turns of the AI run on the new map");
    engine_fixture_destroy(&f);
}
