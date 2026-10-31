/* Feature tests of a map from real geography (docs/MAPS.md): the pack
   data/medieval_europe has the map of misc/make_world_map.py. The same
   engine loads it, the bases stand where the tool put them, and the sample
   save plays a few turns on it. */

#define FEATURE_WORLD_PACK      HEXDOM_DATA_ROOT "/medieval_europe"
#define FEATURE_WORLD_MAX_HEXES 80000    /* max_hexes of map/source/region.tsv */
#define FEATURE_TERRAIN_RIVER   7        /* the ids of data/placeholder/terrain.tsv */
#define FEATURE_TERRAIN_SEA     8
#define FEATURE_TERRAIN_PORT    17
#define FEATURE_PORT_HEIGHT     8        /* a port stands this much above the level of its water (make_world_map.py) */

internal void
feature_world_map_init(engine_fixture *f, const char *content_folder = FEATURE_WORLD_PACK)
{
    f->permanent = test_arena_create(Megabytes(96));
    f->transient = test_arena_create(Megabytes(32));
    engine_config config = engine_default_config();
    config.seed              = 1;
    config.ledger_capacity   = 65536;
    config.read_entire_file  = test_platform_read_entire_file;
    config.write_entire_file = test_platform_write_entire_file;
    config.save_folder       = feature_save_folder;
    config.content_folder    = content_folder;
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
    /* The map left out the Brenner Pass: it had no purpose (docs/MAPS.md, "The purpose of a gate"). A port took its id. */
    TEST_CHECK(counts, info.cities == 42 && info.gates == 9 && info.ports == 36 && info.city_hexes == 294 && info.port_hexes == 36,
               "42 cities of 7 hexes, 9 gates and 36 ports: 45 passes and ports, as on the placeholder map");
    TEST_CHECK(counts, info.gate_hexes == 9 && info.gate_side_cliffs == 18, "the two side hexes of each gate become cliff (F-151)");
    TEST_CHECK(counts, info.slots >= 42 * 10 && info.bandit_sites > 0, "each city has development slots, and the map has bandit sites");

    TEST_CHECK(counts, api->load_scenario(e, api->scenario_at(e, 0).file).status == ENGINE_LOAD_OK,
               "the scenario of the pack loads with the map of the pack");
    TEST_CHECK(counts, feature_world_flowers(api), "each city stands on its flower and has slots on the new map");
    u8 city = feature_busiest_city(api);
    u8 force = api->city_at(e, city).owner_force_id;
    feature_play_as(api, force);
    TEST_CHECK(counts, feature_next_own_turn(api, force) && feature_next_own_turn(api, force),
               "the turns of the AI run on the new map");
    engine_fixture_destroy(&f);
}

/* ---- the west map and the east map (docs/EUROPE.md, E8) ------------------------------------------ */

struct feature_pack_city
{
    u8          city_id;
    const char *key;
    u8          owner;       /* the force id, or CITY_OWNER_NONE */
};

struct feature_pack_spec
{
    const char              *folder;
    u32                      year;
    u32                      factions;
    u32                      width;
    u32                      height;
    u32                      gates;       /* the gates with a purpose; ports take the other ids of the 45 */
    u32                      city_count;
    feature_pack_city        cities[8];
};

/* The heights of the water of a map from make_world_map.py (docs/MAPS.md, "The height of the water"). The sea has
   the height 0, a port stands at the level of its water, and the counts give the river hexes above the level of the sea. */
internal b32
feature_water_heights(engine_api *api, u32 *high_rivers)
{
    void *e = api->engine;
    engine_map_info info = api->map_info(e);
    b32 good = true;
    *high_rivers = 0;
    for (u32 row = 0; row < info.height; ++row)
    {
        for (u32 col = 0; col < info.width; ++col)
        {
            hex_offset offset;
            offset.col = (s32)col;
            offset.row = (s32)row;
            hex_coord hex = hex_from_offset(offset);
            engine_hex cell = api->hex_at(e, hex.q, hex.r);
            if (cell.terrain == FEATURE_TERRAIN_SEA && cell.height != 0) good = false;
            if (cell.terrain == FEATURE_TERRAIN_PORT && cell.height % SIM_MAP_HEIGHT_STEP != FEATURE_PORT_HEIGHT) good = false;
            if (cell.terrain == FEATURE_TERRAIN_RIVER && cell.height >= SIM_MAP_HEIGHT_STEP) (*high_rivers)++;
        }
    }
    return good;
}

/* One test for a pack of E8: the counts of the map, the keys of three cities, the scenario and two turns. */
internal void
feature_pack_map_tests(test_counts *counts, const feature_pack_spec *spec)
{
    engine_fixture f;
    feature_world_map_init(&f, spec->folder);
    engine_api *api = &f.api;
    void *e = api->engine;
    engine_map_info info = api->map_info(e);
    TEST_CHECK(counts, api->content_status(e).loaded && info.real_map && strcmp(info.detail, "ok") == 0,
               "the engine loads the content and the map of the pack");
    TEST_CHECK(counts, info.width == spec->width && info.height == spec->height && info.width * info.height <= FEATURE_WORLD_MAX_HEXES,
               "the map has the size that make_world_map.py wrote and fits the budget of the terrain mesh");
    TEST_CHECK(counts, info.cities == 42 && info.gates == spec->gates && info.gates + info.ports == 45 && info.city_hexes == 294 &&
                       info.port_hexes == info.ports && info.gate_hexes == spec->gates,
               "the map has 42 cities of 7 hexes and 45 passes and ports, with the gates of the pack");
    TEST_CHECK(counts, info.slots >= 42 * 10 && info.bandit_sites > 0, "each city has development slots, and the map has bandit sites");
    u32 high_rivers = 0;
    TEST_CHECK(counts, feature_water_heights(api, &high_rivers) && high_rivers > 0,
               "the sea is at level 0, each port stands at the level of its water, and the rivers in the hills stand higher");

    engine_scenario scenario = api->scenario_at(e, 0);
    TEST_CHECK(counts, scenario.kind == ENGINE_SCENARIO_PACK && scenario.available && scenario.year == spec->year && !scenario.pack[0],
               "the pack lists its own scenario first");
    TEST_CHECK(counts, api->load_scenario(e, scenario.file).status == ENGINE_LOAD_OK, "the scenario of the pack loads");
    TEST_CHECK(counts, feature_world_flowers(api), "each city stands on its flower and has slots");
    TEST_CHECK(counts, api->faction_count(e) == spec->factions, "each row of forces.tsv is a faction");

    for (u32 index = 0; index < spec->city_count; ++index)
    {
        const feature_pack_city *want = &spec->cities[index];
        engine_city_name city_name = api->city_name_at(e, want->city_id);
        u8 owner = ENGINE_NO_FACTION;
        for (u32 city_index = 0; city_index < api->city_count(e); ++city_index)
        {
            engine_city city = api->city_at(e, city_index);
            if (city.city_id == want->city_id) owner = city.owner_force_id;
        }
        TEST_CHECK(counts, strcmp(city_name.key, want->key) == 0 && owner == want->owner,
                   "the key of a city and its owner at the start are the ones of the scenario");
    }

    u8 force = api->city_at(e, feature_busiest_city(api)).owner_force_id;
    TEST_CHECK(counts, feature_play_as(api, force), "the player chooses a faction; the first turn of the scenario starts");
    TEST_CHECK(counts, feature_next_own_turn(api, force) && feature_next_own_turn(api, force),
               "the AI plays the other factions of the scenario for two turns");
    engine_fixture_destroy(&f);
}

internal void
feature_pack_map_tests_all(test_counts *counts)
{
    static const feature_pack_spec west =
    {
        HEXDOM_DATA_ROOT "/medieval_west", 1200, 17, 268, 282, 4, 3,
        { { 16, "paris", 0 }, { 30, "rouen", 1 }, { 32, "lille", 15 } }
    };
    /* The new factions of the east have the force ids 11 to 16: Jerusalem, Antioch, Cilicia, Cyprus, Georgia and the caliphate. */
    static const feature_pack_spec east =
    {
        HEXDOM_DATA_ROOT "/medieval_east", 1200, 17, 366, 217, 4, 8,
        {
            { 34, "antioch", 12 }, { 35, "aleppo", 3 }, { 39, "damascus", 3 }, { 40, "acre", 11 },
            { 38, "baghdad", 16 }, { 33, "sis", 13 }, { 41, "nicosia", 14 }, { 19, "tbilisi", 15 }
        }
    };
    /* The original Three Kingdoms pack of 196: Cao Cao holds Xuchang, Lü Bu holds Xiapi, Liu Bei holds Xiaopei, and Liucheng has no owner. */
    static const feature_pack_spec han =
    {
        HEXDOM_DATA_ROOT "/han_196", 196, 20, 280, 282, 10, 5,
        { { 13, "xuchang", 0 }, { 33, "xiapi", 4 }, { 34, "xiaopei", 5 }, { 21, "chengdu", 12 }, { 2, "liucheng", ENGINE_NO_FACTION } }
    };
    feature_pack_map_tests(counts, &west);
    feature_pack_map_tests(counts, &east);
    feature_pack_map_tests(counts, &han);
}

/* ---- the events of the packs (docs/EUROPE.md, E7) ---------------------------------------------- */

#define FEATURE_STORY_SLOT      29
#define FEATURE_STORY_CAIRO     4
#define FEATURE_STORY_AYYUBID   3
#define FEATURE_STORY_ROW_CAIRO 2

/* The key of the new name of a faction, or "" if the faction has none. */
internal const char *
feature_story_name_key(engine_api *api, u8 force_id, engine_faction *out)
{
    for (u32 index = 0; index < api->faction_count(api->engine); ++index)
    {
        engine_faction faction = api->faction_at(api->engine, index);
        if (faction.force_id == force_id) { *out = faction; return out->name_key; }
    }
    out->name_key[0] = 0;
    return out->name_key;
}

/* True if the engine reads each row of history.tsv and each officer that a row names has been found. */
internal b32
feature_story_rows_found(engine_api *api, u32 rows)
{
    void *e = api->engine;
    b32 found = api->history_count(e) == rows && api->content_status(e).history_count == rows;
    for (u32 row = 0; row < api->history_count(e); ++row)
    {
        engine_history_kind kind = api->history_at(e, row);
        found &= kind.names_found == kind.name_count;
    }
    return found;
}

internal void
feature_pack_story_tests(test_counts *counts)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/slot_%02u.hxs", feature_save_folder, FEATURE_STORY_SLOT + 1);
    remove(path);

    engine_fixture west;
    feature_world_map_init(&west, HEXDOM_DATA_ROOT "/medieval_west");
    TEST_CHECK(counts, west.api.history_count(west.api.engine) == 5, "the west pack has the 4 events of E7 and the interdict of E6");
    west.api.load_scenario(west.api.engine, west.api.scenario_at(west.api.engine, 0).file);
    TEST_CHECK(counts, feature_story_rows_found(&west.api, 5), "each officer that an event of the west pack names exists in the scenario");
    engine_fixture_destroy(&west);

    engine_fixture f;
    feature_world_map_init(&f, HEXDOM_DATA_ROOT "/medieval_east");
    engine_api *api = &f.api;
    void *e = api->engine;
    TEST_CHECK(counts, api->history_count(e) == 9, "the east pack has the 6 events of E7 and the 3 sees of E6");
    TEST_CHECK(counts, api->load_scenario(e, api->scenario_at(e, 0).file).status == ENGINE_LOAD_OK, "the scenario of the east pack loads");
    TEST_CHECK(counts, feature_story_rows_found(api, 9), "each officer that an event of the east pack names exists in the scenario");

    engine_faction faction = {};
    feature_story_name_key(api, FEATURE_STORY_AYYUBID, &faction);
    TEST_CHECK(counts, faction.force_id == FEATURE_STORY_AYYUBID && strcmp(faction.key, "ayyubid") == 0 && faction.name_key[0] == 0,
               "the Ayyubids of 1200 have their own name and no new name");
    TEST_CHECK(counts, !api->history_ready(e, FEATURE_STORY_ROW_CAIRO, FEATURE_STORY_AYYUBID),
               "the Mamluks do not take Cairo in 1200: the window of the row has not opened");

    api->set_date(e, 1250, 1, 1);
    TEST_CHECK(counts, api->history_ready(e, FEATURE_STORY_ROW_CAIRO, FEATURE_STORY_AYYUBID),
               "in 1250 the Ayyubids rule Cairo and the four Mamluk officers are free, so the row is ready");
    engine_command_result fired = api->trigger_history(e, FEATURE_STORY_ROW_CAIRO, FEATURE_STORY_AYYUBID);
    feature_story_name_key(api, FEATURE_STORY_AYYUBID, &faction);
    TEST_CHECK(counts, fired.refusal == SIM_REFUSAL_NONE && strcmp(faction.name_key, "mamluk") == 0 &&
                           api->history_at(e, FEATURE_STORY_ROW_CAIRO).fired_turn != 0,
               "the row fires: the faction has the new name key mamluk");

    TEST_CHECK(counts, api->save_game(e, FEATURE_STORY_SLOT, "mamluk") == ENGINE_SAVE_OK, "the game with the new name saves");
    api->load_scenario(e, api->scenario_at(e, 0).file);
    feature_story_name_key(api, FEATURE_STORY_AYYUBID, &faction);
    TEST_CHECK(counts, faction.name_key[0] == 0, "a new scenario clears the new name");
    TEST_CHECK(counts, api->load_game(e, FEATURE_STORY_SLOT) == ENGINE_SAVE_OK, "the save loads");
    feature_story_name_key(api, FEATURE_STORY_AYYUBID, &faction);
    TEST_CHECK(counts, strcmp(faction.name_key, "mamluk") == 0 && strcmp(faction.key, "ayyubid") == 0 &&
                           api->history_at(e, FEATURE_STORY_ROW_CAIRO).fired_turn != 0,
               "after the load the faction still has the name key mamluk, and the row stays fired");
    TEST_CHECK(counts, !api->history_ready(e, FEATURE_STORY_ROW_CAIRO, FEATURE_STORY_AYYUBID), "a row that fired does not fire again");
    engine_fixture_destroy(&f);
    remove(path);
}

/* ---- the scenario of the pack (docs/MAPS.md, step W2) ------------------------------------------ */

#define FEATURE_WORLD_FORCES     24     /* rows of scenarios/y1200/forces.tsv */
#define FEATURE_WORLD_PARIS      25
#define FEATURE_WORLD_LONDON     30
#define FEATURE_WORLD_BARQA      11     /* the three cities without an owner in 1200 */
#define FEATURE_WORLD_MILAN      24
#define FEATURE_WORLD_LYON       29
#define FEATURE_WORLD_RONCEVAUX  42
#define FEATURE_WORLD_FRANCE     0
#define FEATURE_WORLD_ANGEVIN    1
#define FEATURE_WORLD_PHILIP     0      /* the officer id of the ruler of France */

/* True if the Big5 name of the engine starts with these bytes and ends there. */
internal b32
feature_world_name_is(const u8 *name, u32 size, const u8 *expected, u32 expected_size)
{
    return expected_size < size && memcmp(name, expected, expected_size) == 0 && name[expected_size] == 0;
}

internal void
feature_pack_scenario_tests(test_counts *counts)
{
    engine_fixture f;
    feature_world_map_init(&f);
    engine_api *api = &f.api;
    void *e = api->engine;
    engine_scenario scenario = api->scenario_at(e, 0);
    TEST_CHECK(counts, api->scenario_count(e) == 3 && scenario.kind == ENGINE_SCENARIO_PACK && scenario.available && scenario.year == 1200 && !scenario.pack[0],
               "the pack lists one scenario of its own first: the kind pack, the year 1200, available");
    engine_scenario back = api->scenario_at(e, 1);
    TEST_CHECK(counts, strcmp(back.pack, "placeholder") == 0 && back.kind == ENGINE_SCENARIO_BASE && back.available,
               "the next rows lead back to the scenarios of the placeholder pack");
    engine_load_result missing = api->load_scenario(e, FEATURE_WORLD_PACK "/scenarios/no_such/scenario.tsv");
    TEST_CHECK(counts, missing.status != ENGINE_LOAD_OK && api->city_count(e) == 0, "a pack scenario that is not there does not load and changes nothing");

    TEST_CHECK(counts, api->load_scenario(e, scenario.file).status == ENGINE_LOAD_OK, "the pack scenario loads from its TSV files");
    TEST_CHECK(counts, api->city_count(e) == SAVE_CITY_COUNT, "the scenario has the 42 cities of the map");
    u32 rulers = 0;
    for (u32 index = 0; index < api->faction_count(e); ++index)
    {
        if (api->force_ruler(e, api->faction_at(e, index).force_id) != ENGINE_NO_OFFICER) rulers++;
    }
    TEST_CHECK(counts, rulers == FEATURE_WORLD_FORCES, "each row of forces.tsv is a faction with its ruler");
    TEST_CHECK(counts, api->force_ruler(e, FEATURE_WORLD_FRANCE) == FEATURE_WORLD_PHILIP, "the ruler column of forces.tsv makes the ruler");

    u8 paris = ENGINE_NO_FACTION, london = ENGINE_NO_FACTION, barqa = 0, milan = 0, lyon = 0;
    for (u32 index = 0; index < api->city_count(e); ++index)
    {
        engine_city city = api->city_at(e, index);
        if (city.city_id == FEATURE_WORLD_PARIS)  paris  = city.owner_force_id;
        if (city.city_id == FEATURE_WORLD_LONDON) london = city.owner_force_id;
        if (city.city_id == FEATURE_WORLD_BARQA)  barqa  = city.owner_force_id;
        if (city.city_id == FEATURE_WORLD_MILAN)  milan  = city.owner_force_id;
        if (city.city_id == FEATURE_WORLD_LYON)   lyon   = city.owner_force_id;
    }
    TEST_CHECK(counts, paris == FEATURE_WORLD_FRANCE && london == FEATURE_WORLD_ANGEVIN, "cities.tsv of the scenario gives each city its owner");
    TEST_CHECK(counts, barqa == CITY_OWNER_NONE && milan == CITY_OWNER_NONE && lyon == CITY_OWNER_NONE, "a city with the owner - has no owner");

    engine_officer philip = {};
    for (u32 index = 0; index < api->officer_count(e); ++index)
    {
        engine_officer officer = api->officer_at(e, index);
        if (officer.officer_id == FEATURE_WORLD_PHILIP) philip = officer;
    }
    static const u8 capet[] = { 0xA5, 0x64, 0xA8, 0xD8 };   /* 卡佩 */
    static const u8 felix[] = { 0xB5, 0xCC, 0xA4, 0x4F };   /* 腓力 */
    TEST_CHECK(counts, feature_world_name_is(philip.family_name, sizeof(philip.family_name), capet, sizeof(capet)) &&
                       feature_world_name_is(philip.given_name, sizeof(philip.given_name), felix, sizeof(felix)),
               "officers.tsv gives the Big5 name of each officer");
    TEST_CHECK(counts, philip.status == OFFICER_STATUS_RULER && philip.force_id == FEATURE_WORLD_FRANCE && philip.base_location == FEATURE_WORLD_PARIS &&
                       philip.birth_year == 1165 && philip.leadership == 82 && philip.intelligence == 90,
               "officers.tsv gives the status, the force, the city, the years and the abilities");

    static const u8 paris_name[]     = { 0xA4, 0xDA, 0xBE, 0xA4 };   /* 巴黎 */
    static const u8 roncevaux_name[] = { 0xB6, 0xA9, 0xB6, 0xEB, 0xB4, 0xB5, 0xA5, 0xCB, 0xAD, 0x43, 0xB4, 0xB5, 0xC3, 0xF6 };   /* 隆塞斯瓦耶斯關 */
    engine_city_name city_name = api->city_name_at(e, FEATURE_WORLD_PARIS);
    engine_location_name pass_name = api->location_name_at(e, FEATURE_WORLD_RONCEVAUX);
    TEST_CHECK(counts, city_name.city_id == FEATURE_WORLD_PARIS && feature_world_name_is(city_name.name, sizeof(city_name.name), paris_name, sizeof(paris_name)),
               "the cities.tsv of the pack names the cities");
    TEST_CHECK(counts, pass_name.location == FEATURE_WORLD_RONCEVAUX &&
                       feature_world_name_is(pass_name.name, sizeof(pass_name.name), roncevaux_name, sizeof(roncevaux_name)),
               "the bases.tsv of the pack names the passes and the ports");

    /* docs/EUROPE.md, E2: the English keys of the pack files, by the id of each record. */
    TEST_CHECK(counts, strcmp(philip.key, "philip_ii") == 0, "the key column of officers.tsv gives the English key of an officer");
    engine_faction france = {};
    for (u32 index = 0; index < api->faction_count(e); ++index)
    {
        if (api->faction_at(e, index).force_id == FEATURE_WORLD_FRANCE) france = api->faction_at(e, index);
    }
    TEST_CHECK(counts, strcmp(france.key, "france") == 0, "the key column of forces.tsv gives the English key of a faction");
    TEST_CHECK(counts, strcmp(city_name.key, "paris") == 0, "the key column of cities.tsv of the pack gives the English key of a city");

    TEST_CHECK(counts, feature_play_as(api, FEATURE_WORLD_FRANCE), "the player chooses France; the first turn of 1200 starts");
    TEST_CHECK(counts, feature_next_own_turn(api, FEATURE_WORLD_FRANCE) && feature_next_own_turn(api, FEATURE_WORLD_FRANCE),
               "the AI plays the other 23 factions of the scenario");
    engine_fixture_destroy(&f);
}

/* ---- the saves of a pack (docs/MAPS.md): slot_NN.meta names the pack ------------------------- */

#define FEATURE_WORLD_SAVE_SLOT  31     /* slot_32.hxs and slot_32.meta: a save of 1200 */
#define FEATURE_WORLD_PLAIN_SLOT 30     /* slot_31.hxs: a save of the placeholder pack without its .meta file */

/* The .meta file of a slot of the test save folder: the path, and its text in out ("" if there is no file). */
internal void
feature_world_meta(u32 slot, char *path, u32 path_size, char *out, u32 out_size)
{
    snprintf(path, path_size, "%s/slot_%02u.meta", feature_save_folder, slot + 1);
    out[0] = 0;
    FILE *file = fopen(path, "rb");
    if (!file) return;
    size_t size = fread(out, 1, out_size - 1, file);
    out[size] = 0;
    fclose(file);
}

internal void
feature_pack_save_tests(test_counts *counts)
{
    char path[512], meta[512], hxs[512];
    for (u32 slot = FEATURE_WORLD_PLAIN_SLOT; slot <= FEATURE_WORLD_SAVE_SLOT; ++slot)
    {
        feature_world_meta(slot, path, sizeof(path), meta, sizeof(meta));
        remove(path);
        snprintf(hxs, sizeof(hxs), "%s/slot_%02u.hxs", feature_save_folder, slot + 1);
        remove(hxs);
    }

    engine_fixture europe;
    feature_world_map_init(&europe);
    engine_api *api = &europe.api;
    void *e = api->engine;
    api->load_scenario(e, api->scenario_at(e, 0).file);
    feature_play_as(api, FEATURE_WORLD_FRANCE);
    TEST_CHECK(counts, api->save_game(e, FEATURE_WORLD_SAVE_SLOT, "europe") == ENGINE_SAVE_OK, "a game of 1200 saves");
    feature_world_meta(FEATURE_WORLD_SAVE_SLOT, path, sizeof(path), meta, sizeof(meta));
    TEST_CHECK(counts, strstr(meta, "\npack\tmedieval_europe\n") != 0, "the save writes slot_NN.meta with the pack medieval_europe");
    engine_fixture_destroy(&europe);

    engine_fixture three;
    feature_world_map_init(&three, HEXDOM_DATA_ROOT "/placeholder");
    api = &three.api;
    e = api->engine;
    api->refresh_save_slots(e);
    TEST_CHECK(counts, api->save_slot(e, FEATURE_WORLD_SAVE_SLOT).used && strcmp(api->slot_pack(e, FEATURE_WORLD_SAVE_SLOT).pack, "medieval_europe") == 0,
               "the engine of the placeholder pack lists the save of 1200 with its pack");
    TEST_CHECK(counts, api->load_game(e, FEATURE_WORLD_SAVE_SLOT) == ENGINE_SAVE_OTHER_PACK && api->city_count(e) == 0,
               "the engine of the placeholder pack does not load it and changes nothing");
    api->load_save(e, api->scenario_at(e, 0).file);
    TEST_CHECK(counts, api->save_game(e, FEATURE_WORLD_PLAIN_SLOT, "three kingdoms") == ENGINE_SAVE_OK, "a game of the placeholder pack saves");
    feature_world_meta(FEATURE_WORLD_PLAIN_SLOT, path, sizeof(path), meta, sizeof(meta));
    TEST_CHECK(counts, strstr(meta, "\npack\tplaceholder\n") != 0, "its slot_NN.meta names the pack placeholder");
    remove(path);   /* a save of an older build: no .meta file */
    engine_fixture_destroy(&three);

    engine_fixture again;
    feature_world_map_init(&again);
    api = &again.api;
    e = api->engine;
    api->refresh_save_slots(e);
    TEST_CHECK(counts, api->save_slot(e, FEATURE_WORLD_PLAIN_SLOT).used && strcmp(api->slot_pack(e, FEATURE_WORLD_PLAIN_SLOT).pack, HEXDOM_CONTENT_PACK) == 0,
               "a save without its .meta file is a save of the default pack");
    TEST_CHECK(counts, api->slot_pack(e, FEATURE_WORLD_SAVE_SLOT).pack[0] == 0 && api->load_game(e, FEATURE_WORLD_SAVE_SLOT) == ENGINE_SAVE_OK &&
                       api->city_count(e) == SAVE_CITY_COUNT,
               "the engine of medieval_europe loads its own save");
    engine_fixture_destroy(&again);
}
