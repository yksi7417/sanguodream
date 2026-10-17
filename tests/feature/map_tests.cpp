/* Feature tests of M10, the real map (formats/map-data.md,
   terrain-and-map.md; F-077, F-147 to F-151), through engine_api with the
   real engine and map\ of the pack: the counts of the map, the cities and
   their slots, the bases, the movement cost over real terrain, fire, the
   build permission, the natural traps, the bandit sites, the event sites,
   a few AI turns and a save on the real map. */

#define FEATURE_MAP_PIKE        1
#define FEATURE_MAP_CAVALRY     4
#define FEATURE_MAP_RAM         5
#define FEATURE_MAP_BOAT        9
#define FEATURE_MAP_BANDITS     2      /* the row of disasters.tsv */
#define FEATURE_MAP_CAMP        0      /* the rows of buildings.tsv */
#define FEATURE_MAP_FIRE_SHIP   17
#define FEATURE_MAP_DIKE        18
#define FEATURE_MAP_ROCKFALL    19
#define FEATURE_MAP_JIANYE      22     /* 建業: 6 flower sites (terrain-and-map.md) */
#define FEATURE_MAP_LUOYANG     15
#define FEATURE_MAP_CHANGAN     17

/* The setup of the city tests on the real map: the busiest city of the
   sample save, its faction plays, and it has gold, soldiers, grain and
   weapons for a sortie. */
internal void
feature_map_setup(feature_city_setup *setup)
{
    engine_fixture_init_real(&setup->fixture, 1, 65536);
    setup->api = &setup->fixture.api;
    engine_api *api = setup->api;
    void *e = api->engine;
    api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
    setup->city  = feature_busiest_city(api);
    setup->force = api->city_at(e, setup->city).owner_force_id;
    feature_play_as(api, setup->force);
    api->set_city_gold(e, setup->city, 50000);
    api->set_actions(e, setup->force, 255);
    api->set_city_resource(e, setup->city, SIM_RESOURCE_SOLDIERS, 0, 20000);
    api->set_city_resource(e, setup->city, SIM_RESOURCE_GRAIN, 0, 100000);
    api->set_city_resource(e, setup->city, SIM_RESOURCE_WEAPONS, 0, 10000);
}

internal hex_coord
feature_map_offset(s32 col, s32 row)
{
    hex_offset offset;
    offset.col = col;
    offset.row = row;
    return hex_from_offset(offset);
}

/* True if the hex is at least "apart" hexes from the centre of each city. */
internal b32
feature_map_apart(engine_api *api, hex_coord hex, s32 apart)
{
    for (u32 index = 0; index < api->city_count(api->engine); ++index)
    {
        engine_city_slots slots = api->city_slots(api->engine, api->city_at(api->engine, index).city_id);
        if (hex_distance(hex, hex_make(slots.q, slots.r)) < apart) return false;
    }
    return true;
}

/* The first hex of a terrain without a city, a base, a building or a unit,
   at least "apart" hexes from each city. */
internal b32
feature_map_find(engine_api *api, u8 terrain, s32 apart, hex_coord *out)
{
    void *e = api->engine;
    engine_map_size size = api->map_size(e);
    for (u32 row = 2; row + 2 < size.height; ++row)
    {
        for (u32 col = 2; col + 2 < size.width; ++col)
        {
            hex_coord hex = feature_map_offset((s32)col, (s32)row);
            engine_hex data = api->hex_at(e, hex.q, hex.r);
            if (data.terrain != terrain || data.city_id != ENGINE_NO_CITY || data.base_id != ENGINE_NO_BASE) continue;
            if (api->building_at_hex(e, hex.q, hex.r) != ENGINE_NO_BUILDING || api->unit_at_hex(e, hex.q, hex.r) != ENGINE_NO_UNIT) continue;
            if (!feature_map_apart(api, hex, apart)) continue;
            *out = hex;
            return true;
        }
    }
    return false;
}

internal void
feature_map_data_tests(test_counts *counts)
{
    engine_fixture f;
    engine_fixture_init_real(&f, 1, 65536);
    engine_api *api = &f.api;
    void *e = api->engine;
    engine_map_info info = api->map_info(e);
    TEST_CHECK(counts, info.real_map && strcmp(info.detail, "ok") == 0 && info.width == 200 && info.height == 200,
               "the engine loads map\\hexes.tsv of the pack: 200 x 200 hexes (F-077)");
    TEST_CHECK(counts, info.cities == 42 && info.gates == 10 && info.ports == 35 && info.city_hexes == 294 && info.port_hexes == 35,
               "42 cities of 7 hexes, 10 gates and 35 ports");
    TEST_CHECK(counts, info.gate_hexes == 10 && info.gate_side_cliffs == 28, "the side hexes of the gates are cliff; 10 gate hexes stay (F-151)");
    TEST_CHECK(counts, info.slots == 591 && info.port_region_slots == 50, "591 slots; 50 of them in port regions (F-151)");
    TEST_CHECK(counts, info.dikes == 4 && info.rockfalls == 18 && info.flood_hexes == 195 && info.bandit_sites == 190 && info.event_sites == 39 &&
                       info.unknown_flags == 396,
               "the sites of the map: 4 dikes, 18 rockfall sites, 195 flood hexes, 190 bandit sites, 39 event sites (F-150)");

    api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
    b32 flowers = api->city_count(e) == SAVE_CITY_COUNT, slots_ok = true;
    u32 slot_total = 0, port_slots = 0, flower_sites = 0, flower_cities = 0, jianye_sites = 0;
    for (u32 index = 0; index < api->city_count(e); ++index)
    {
        u8 city = api->city_at(e, index).city_id;
        engine_city_slots site = api->city_slots(e, city);
        engine_hex center = api->hex_at(e, site.q, site.r);
        if (center.city_id != city || center.region != city) flowers = false;
        for (u32 direction = 0; direction < HEX_DIRECTION_COUNT; ++direction)
        {
            hex_coord petal = hex_neighbor(hex_make(site.q, site.r), direction);
            if (api->hex_at(e, petal.q, petal.r).city_id != city) flowers = false;
        }
        u32 sites_here = 0;
        for (u32 slot = 0; slot < site.slot_count; ++slot)
        {
            engine_facility facility = api->facility_at(e, city, (u8)slot);
            engine_hex hex = api->hex_at(e, facility.q, facility.r);
            if (!hex.slot || hex.region_city != city) slots_ok = false;
            if (hex.region != city) port_slots++;
            if (facility.slot_neighbours == 6) sites_here++;
        }
        slot_total    += site.slot_count;
        flower_sites  += sites_here;
        flower_cities += sites_here > 0;
        if (city == FEATURE_MAP_JIANYE) jianye_sites = sites_here;
    }
    TEST_CHECK(counts, flowers, "each city of the save stands on its flower: the centre and the six neighbours are the city");
    TEST_CHECK(counts, slots_ok && slot_total == 591 && port_slots == 50,
               "each city has the slots of its region and of its port regions (F-151)");
    TEST_CHECK(counts, flower_sites == 38 && flower_cities == 26 && jianye_sites == 6,
               "the slot neighbours are the real ones: 38 flower sites in 26 cities, 6 in 建業 (terrain-and-map.md)");

    b32 bases_ok = api->base_count(e) == 45;
    u32 gate_cliffs = 0;
    u8 port = feature_terrain_id(api, "port"), gate = feature_terrain_id(api, "gate"), cliff = feature_terrain_id(api, "cliff");
    for (u32 index = 0; index < api->base_count(e); ++index)
    {
        engine_base base = api->base_at(e, index);
        engine_hex hex = api->hex_at(e, base.q, base.r);
        if (hex.base_id != base.id || hex.region != base.id || hex.terrain != (base.type == BASE_PORT ? port : gate)) bases_ok = false;
        for (u32 direction = 0; direction < HEX_DIRECTION_COUNT; ++direction)
        {
            hex_coord side = hex_neighbor(hex_make(base.q, base.r), direction);
            engine_hex next = api->hex_at(e, side.q, side.r);
            if (next.region == base.id && base.type != BASE_PORT && next.terrain == gate) bases_ok = false;   /* the sides are cliff now */
            if (next.region == base.id && base.type != BASE_PORT && next.terrain == cliff) gate_cliffs++;
        }
    }
    TEST_CHECK(counts, bases_ok && gate_cliffs >= 20, "each port and gate stands on its hex of the map; no gate hex is beside a gate hex, its sides are cliff");

    u32 dikes = 0, rockfalls = 0;
    b32 traps_ok = true;
    for (u32 id = 0; id < api->building_count(e); ++id)
    {
        engine_building building = api->building_at(e, id);
        if (building.id == ENGINE_NO_BUILDING) continue;
        engine_hex hex = api->hex_at(e, building.q, building.r);
        if (building.kind == FEATURE_MAP_DIKE)     { dikes++;     if (hex.trap != 1 || building.city_id != hex.region_city) traps_ok = false; }
        if (building.kind == FEATURE_MAP_ROCKFALL) { rockfalls++; if (hex.trap != 2) traps_ok = false; }
    }
    TEST_CHECK(counts, traps_ok && dikes == 4 && rockfalls == 18, "a dike stands on each dike hex and a rockfall on each rockfall site (F-150)");

    u32 temples = 0, ruins = 0;
    b32 relics_ok = api->relic_count(e) == SIM_RELIC_COUNT;
    for (u32 index = 0; index < api->relic_count(e); ++index)
    {
        engine_relic relic = api->relic_at(e, index);
        if (!relic.alive || !api->hex_at(e, relic.q, relic.r).event_site) relics_ok = false;
        for (u32 other = 0; other < index; ++other)
        {
            engine_relic before = api->relic_at(e, other);
            if (before.q == relic.q && before.r == relic.r) relics_ok = false;
        }
        temples += relic.kind == RELIC_TEMPLE;
        ruins   += relic.kind == RELIC_RUIN;
    }
    TEST_CHECK(counts, relics_ok && temples == 6 && ruins == 6, "the game picks 6 + 6 different event sites at the start (F-150, Q-193)");
    engine_fixture_destroy(&f);
}

internal void
feature_map_move_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_map_setup(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 force = setup.force;
    const char *keys[] = { "grass", "earth", "swamp", "forest", "stream", "river", "waste", "main_road", "plank_road", "cliff", "small_path" };
    hex_coord at[ArrayCount(keys)];
    b32 found = true;
    for (u32 index = 0; index < ArrayCount(keys); ++index) found &= feature_map_find(api, feature_terrain_id(api, keys[index]), 3, at + index);
    TEST_CHECK(counts, found, "the real map has each terrain of the test away from the cities");
    if (!found) { engine_fixture_destroy(&setup.fixture); return; }
    enum { GRASS, EARTH, SWAMP, FOREST, STREAM, RIVER, WASTE, ROAD, PLANK, CLIFF, PATH };
    u32 pike[ArrayCount(keys)], cavalry[ArrayCount(keys)], ram[ArrayCount(keys)], boat[ArrayCount(keys)];
    for (u32 index = 0; index < ArrayCount(keys); ++index)
    {
        pike[index]    = api->move_cost(e, FEATURE_MAP_PIKE, force, at[index].q, at[index].r);
        cavalry[index] = api->move_cost(e, FEATURE_MAP_CAVALRY, force, at[index].q, at[index].r);
        ram[index]     = api->move_cost(e, FEATURE_MAP_RAM, force, at[index].q, at[index].r);
        boat[index]    = api->move_cost(e, FEATURE_MAP_BOAT, force, at[index].q, at[index].r);
    }
    TEST_CHECK(counts, pike[GRASS] == 5 && pike[EARTH] == 5 && pike[SWAMP] == 8 && pike[FOREST] == 6 && pike[WASTE] == 6 && pike[ROAD] == 4 && pike[PLANK] == 6,
               "a pike unit pays the 槍 row of movement-costs.tsv: grass 5, earth 5, swamp 8, forest 6, waste 6, main road 4, plank road 6 (F-148)");
    TEST_CHECK(counts, cavalry[EARTH] == 4 && cavalry[FOREST] == 10 && cavalry[WASTE] == 7 && ram[FOREST] == 8 && ram[GRASS] == 6,
               "cavalry pays 4 on earth and 10 in a forest; a ram pays 8 in a forest and 6 on grass");
    TEST_CHECK(counts, pike[STREAM] == 0 && pike[CLIFF] == 0 && cavalry[STREAM] == 0 && boat[STREAM] == 0 && boat[CLIFF] == 0,
               "no unit enters 川 or 崖 (255 in the table)");
    TEST_CHECK(counts, pike[RIVER] == 4 && boat[RIVER] == 4 && ram[RIVER] == 0 && boat[GRASS] == 0,
               "on 河 a land unit is a 走舸 (4); a siege weapon cannot go on water; a ship cannot go on land");
    TEST_CHECK(counts, pike[PATH] == 0, "a small path is closed without 難所行軍");

    api->start_fire(e, at[GRASS].q, at[GRASS].r, 3);
    TEST_CHECK(counts, api->hex_at(e, at[GRASS].q, at[GRASS].r).fire_turns == 3 && api->move_cost(e, FEATURE_MAP_PIKE, force, at[GRASS].q, at[GRASS].r) == 20,
               "to enter a burning hex costs 4 times the cost of the table (5 x 4, F-148)");

    engine_city_slots luoyang = api->city_slots(e, FEATURE_MAP_LUOYANG), changan = api->city_slots(e, FEATURE_MAP_CHANGAN);
    u32 path = api->path_cost(e, luoyang.q, luoyang.r, changan.q, changan.r);
    TEST_CHECK(counts, path != HEX_NO_PATH && path >= 4 * (u32)hex_distance(hex_make(luoyang.q, luoyang.r), hex_make(changan.q, changan.r)),
               "a path leads over the real map from 洛陽 to 長安, through the passes");

    /* A sortie from the city and the move range on real terrain. */
    u32 unit_id = feature_sortie_unit(api, setup.city, 1);
    engine_unit unit = api->unit_at(e, unit_id);
    engine_hex beside = api->hex_at(e, unit.q, unit.r);
    TEST_CHECK(counts, unit_id != ENGINE_NO_UNIT && beside.city_id == ENGINE_NO_CITY && beside.on_map,
               "a sortie puts the unit on a hex outside the city flower");
    engine_reach_view reach = api->reachable(e, unit_id);
    b32 reach_ok = reach.count > 1;
    for (u32 index = 1; index < reach.count && reach_ok; ++index)
    {
        engine_hex hex = api->hex_at(e, reach.cells[index].q, reach.cells[index].r);
        engine_terrain_kind kind = api->terrain_kind_at(e, hex.terrain);
        if (reach.costs[index] > 22 || !kind.enterable || (hex.city_id != ENGINE_NO_CITY && api->city_at(e, hex.city_id).owner_force_id != force)) reach_ok = false;
    }
    TEST_CHECK(counts, reach_ok, "the move range of a pike unit (22) on the real map holds only hexes that it can enter");
    engine_fixture_destroy(&setup.fixture);
}

internal void
feature_map_fire_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_map_setup(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    hex_coord grass = {}, river = {}, path = {}, sand = {};
    b32 found = feature_map_find(api, feature_terrain_id(api, "grass"), 4, &grass) && feature_map_find(api, feature_terrain_id(api, "river"), 4, &river) &&
                feature_map_find(api, feature_terrain_id(api, "small_path"), 4, &path) && feature_map_find(api, feature_terrain_id(api, "sand"), 4, &sand);
    TEST_CHECK(counts, found, "the real map has grass, river, small path and sand away from the cities");
    if (!found) { engine_fixture_destroy(&setup.fixture); return; }

    api->start_fire(e, grass.q, grass.r, 7);
    api->start_fire(e, sand.q, sand.r, 9);
    api->start_fire(e, river.q, river.r, 3);
    api->start_fire(e, path.q, path.r, 3);
    TEST_CHECK(counts, api->hex_at(e, grass.q, grass.r).fire_turns == 7 && api->hex_at(e, sand.q, sand.r).fire_turns == 7,
               "the fire count of a hex is 0 to 7; sand burns (terrain-types.tsv, Q-198)");
    TEST_CHECK(counts, api->hex_at(e, river.q, river.r).fire_turns == 0 && api->hex_at(e, path.q, path.r).fire_turns == 0,
               "on an empty hex of 河 or 小徑 the fire stops at once (F-149)");
    hex_coord ship = river;
    api->place_unit(e, setup.force, ship.q, ship.r);
    api->start_fire(e, ship.q, ship.r, 3);
    TEST_CHECK(counts, api->hex_at(e, ship.q, ship.r).fire_turns == 3, "on 河 a fire starts on a hex with a unit");

    b32 decay = true;
    for (u32 turn = 1; turn <= 7; ++turn)
    {
        feature_next_turn(api);
        u32 left = api->hex_at(e, grass.q, grass.r).fire_turns;
        if (left != 7 - turn) decay = false;
    }
    TEST_CHECK(counts, decay, "each turn the fire count goes down by 1; at 0 the fire stops (F-149)");
    TEST_CHECK(counts, api->hex_at(e, ship.q, ship.r).fire_turns == 0, "the fire on the hex with the unit also ends after its turns");
    engine_fixture_destroy(&setup.fixture);
}

/* F-147: the build permission of each terrain, for a unit next to the hex. */
internal void
feature_map_build_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_map_setup(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u32 unit_id = feature_sortie_unit(api, setup.city, 1);
    u8 grass = feature_terrain_id(api, "grass");
    hex_coord target = {};
    b32 placed = false;
    hex_coord candidate = {};
    for (s32 attempt = 0; attempt < 8 && !placed && feature_map_find(api, grass, 6 + attempt, &candidate); ++attempt)
    {
        hex_coord next = hex_neighbor(candidate, 0);
        engine_hex data = api->hex_at(e, next.q, next.r);
        if (data.city_id != ENGINE_NO_CITY || data.base_id != ENGINE_NO_BASE || api->unit_at_hex(e, next.q, next.r) != ENGINE_NO_UNIT) continue;
        placed = api->set_unit_place(e, unit_id, setup.force, candidate.q, candidate.r);
        target = next;
    }
    TEST_CHECK(counts, unit_id != ENGINE_NO_UNIT && placed, "a pike unit stands on grass far from the cities");
    if (!placed) { engine_fixture_destroy(&setup.fixture); return; }

    const char *yes[] = { "grass", "earth", "sand", "main_road" };
    const char *no[]  = { "forest", "swamp", "waste", "ford", "plank_road", "river" };
    b32 allowed = true, refused = true;
    engine_command build = {};
    build.kind = ENGINE_COMMAND_BUILD;
    build.unit = unit_id;
    build.row  = FEATURE_MAP_CAMP;
    build.q    = target.q;
    build.r    = target.r;
    for (u32 index = 0; index < ArrayCount(yes); ++index)
    {
        api->set_cell_terrain(e, target.q, target.r, feature_terrain_id(api, yes[index]), 1);
        if (api->can_issue(e, &build).refusal != SIM_REFUSAL_NONE) allowed = false;
    }
    for (u32 index = 0; index < ArrayCount(no); ++index)
    {
        api->set_cell_terrain(e, target.q, target.r, feature_terrain_id(api, no[index]), 1);
        if (api->can_issue(e, &build).refusal != SIM_REFUSAL_NOT_BUILDABLE) refused = false;
    }
    TEST_CHECK(counts, allowed, "a camp can stand on 草地, 土, 砂地 and 主徑 (facility-types.tsv mask 0x407, F-147)");
    TEST_CHECK(counts, refused, "a camp cannot stand on 森, 濕地, 荒地, 渡所, 棧道 or 河");
    api->set_cell_terrain(e, target.q, target.r, feature_terrain_id(api, "river"), 0);
    build.row = FEATURE_MAP_FIRE_SHIP;
    b32 ship_on_river = api->can_issue(e, &build).refusal == SIM_REFUSAL_NONE;
    api->set_cell_terrain(e, target.q, target.r, grass, 1);
    TEST_CHECK(counts, ship_on_river && api->can_issue(e, &build).refusal == SIM_REFUSAL_NOT_BUILDABLE,
               "a fire ship stands only on 河 and 海 (mask 0x180)");
    engine_fixture_destroy(&setup.fixture);
}

/* F-150: a used rockfall comes back on its empty site at random; bandits
   use a bandit site of their city. */
internal void
feature_map_site_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_map_setup(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;

    u32 rockfall_id = ENGINE_NO_BUILDING;
    u32 unit_id = feature_sortie_unit(api, setup.city, 1);
    for (u32 id = 0; id < api->building_count(e) && rockfall_id == ENGINE_NO_BUILDING; ++id)
    {
        engine_building building = api->building_at(e, id);
        if (building.id == ENGINE_NO_BUILDING || building.kind != FEATURE_MAP_ROCKFALL) continue;
        for (u32 direction = 0; direction < HEX_DIRECTION_COUNT && rockfall_id == ENGINE_NO_BUILDING; ++direction)
        {
            hex_coord next = hex_neighbor(hex_make(building.q, building.r), direction);
            if (api->move_cost(e, FEATURE_MAP_PIKE, setup.force, next.q, next.r) && api->set_unit_place(e, unit_id, setup.force, next.q, next.r)) rockfall_id = id;
        }
    }
    TEST_CHECK(counts, unit_id != ENGINE_NO_UNIT && rockfall_id != ENGINE_NO_BUILDING, "a unit stands next to a rockfall site");
    if (rockfall_id != ENGINE_NO_BUILDING)
    {
        engine_building rockfall = api->building_at(e, rockfall_id);
        u32 mark = api->ledger_count(e);
        api->command_attack(e, unit_id, rockfall.q, rockfall.r);
        TEST_CHECK(counts, feature_count_events(api, SIM_EVENT_TRAP_TRIGGERED, mark) == 1 && !api->building_at(e, rockfall_id).complete,
                   "an attack on the rockfall makes it fall; its site is empty");
        mark = api->ledger_count(e);
        u32 turns = 0;
        while (turns < 120 && feature_count_events(api, SIM_EVENT_ROCKFALL_RETURNED, mark) == 0)
        {
            feature_next_own_turn(api, setup.force);
            turns++;
        }
        engine_building back = api->building_at(e, rockfall_id);
        TEST_CHECK(counts, feature_count_events(api, SIM_EVENT_ROCKFALL_RETURNED, mark) == 1 && back.complete && back.durability == back.durability_max,
                   "a new rockfall comes back on the empty site at a random check (10 percent each turn, Q-196)");
    }

    u8 city = ENGINE_NO_CITY;
    engine_bandit_sites sites = {};
    for (u32 index = 0; index < api->city_count(e) && city == ENGINE_NO_CITY; ++index)
    {
        u8 id = api->city_at(e, index).city_id;
        sites = api->bandit_sites(e, id);
        if (sites.count && api->city_at(e, index).owner_force_id != ENGINE_NO_FACTION) city = id;
    }
    b32 sites_ok = city != ENGINE_NO_CITY && sites.count <= ENGINE_BANDIT_SITE_CAPACITY;
    for (u32 index = 0; index < sites.count; ++index)
    {
        engine_hex hex = api->hex_at(e, sites.q[index], sites.r[index]);
        if (!hex.bandit_site || hex.region_city != city || hex.fire_turns) sites_ok = false;
    }
    TEST_CHECK(counts, sites_ok, "the bandit sites of a city: empty sites without fire in its regions, up to 10");
    if (city != ENGINE_NO_CITY)
    {
        u32 mark = api->ledger_count(e);
        engine_command_result started = api->start_disaster(e, city, FEATURE_MAP_BANDITS);
        sim_event event = {};
        b32 picked = feature_find_event(api, SIM_EVENT_BANDIT_SITE, mark, &event);
        b32 among = false;
        engine_map_size size = api->map_size(e);
        hex_coord site = feature_map_offset((s32)(event.detail % size.width), (s32)(event.detail / size.width));
        for (u32 index = 0; index < sites.count; ++index) among |= sites.q[index] == site.q && sites.r[index] == site.r;
        TEST_CHECK(counts, started.refusal == SIM_REFUSAL_NONE && picked && among && event.assumed,
                   "bandits come out of one of the bandit sites of the city (F-150)");
    }
    engine_fixture_destroy(&setup.fixture);
}

/* The computer factions play on the real map, and a save of it loads. */
internal void
feature_map_play_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_map_setup(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    engine_settings settings = api->settings(e);
    settings.ai     = true;
    settings.events = true;
    api->set_settings(e, settings);
    u32 mark = api->ledger_count(e);
    b32 turns = true;
    for (u32 turn = 0; turn < 3; ++turn) turns &= feature_next_own_turn(api, setup.force);
    TEST_CHECK(counts, turns && feature_count_events(api, SIM_EVENT_AI_PLANNED, mark) > 0, "three turns of the computer factions run on the real map");

    hex_coord probe = feature_map_offset(122, 11);   /* the north road of 薊 (map-data.md) */
    engine_hex before = api->hex_at(e, probe.q, probe.r);
    TEST_CHECK(counts, api->save_game(e, 0, "M10 real map") == ENGINE_SAVE_OK, "a game on the real map saves");
    TEST_CHECK(counts, api->load_game(e, 0) == ENGINE_SAVE_OK, "the save loads again");
    engine_hex after = api->hex_at(e, probe.q, probe.r);
    TEST_CHECK(counts, api->map_info(e).real_map && after.terrain == before.terrain && after.region == before.region && after.height == before.height &&
                       api->relic_count(e) == SIM_RELIC_COUNT,
               "after the load the map data is the map of the pack");
    engine_fixture_destroy(&setup.fixture);
}

internal void
feature_map_tests(test_counts *counts)
{
    feature_map_data_tests(counts);
    feature_map_move_tests(counts);
    feature_map_fire_tests(counts);
    feature_map_build_tests(counts);
    feature_map_site_tests(counts);
    feature_map_play_tests(counts);
}
