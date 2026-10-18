/* Feature tests of M11, the state of a save on the real map
   (formats/save-file.md: the building array at 0x0008A3 and the troop unit
   array at 0x086B71), through engine_api with the real engine, the map of
   the pack and sample.s11. City 26 (柴桑) is the check of the user: 12 of
   12 slots in use, five markets of level 3 around a mint. */

#define FEATURE_SAVE_CHAISANG     26
#define FEATURE_SAVE_UNITS        56     /* the units that officers ride in, in sample.s11 (records 0 to 56, spec 7fff546) */
#define FEATURE_SAVE_WATER_UNITS  12     /* the units on 河 or 海 (spec 7fff546) */
#define FEATURE_TERRAIN_RIVER     7      /* 河 */
#define FEATURE_TERRAIN_SEA       8      /* 海 */
#define FEATURE_SAVE_BUILDINGS    30     /* camps, forts, arrow towers, repeating towers, music towers, fire seeds */

internal u8
feature_facility_id(engine_api *api, const char *key)
{
    for (u32 index = 0; index < api->facility_kind_count(api->engine); ++index)
    {
        if (strcmp(api->facility_kind_at(api->engine, index).key, key) == 0) return (u8)index;
    }
    return ENGINE_NO_FACILITY;
}

/* The built facilities of a city of one kind and one level. */
internal u32
feature_count_facility(engine_api *api, u8 city, const char *key, u8 level)
{
    u8 kind = feature_facility_id(api, key);
    u32 found = 0;
    engine_city_slots site = api->city_slots(api->engine, city);
    for (u32 slot = 0; slot < site.slot_count; ++slot)
    {
        engine_facility facility = api->facility_at(api->engine, city, (u8)slot);
        if (facility.state == SIM_SLOT_BUILT && facility.facility == kind && facility.level == level) found++;
    }
    return found;
}

/* True if the unit type has the key; false for ENGINE_NO_UNIT_TYPE. */
internal b32
feature_type_is(engine_api *api, u8 type, const char *key)
{
    if (type == ENGINE_NO_UNIT_TYPE) return false;
    return strcmp(api->unit_type_at(api->engine, type).key, key) == 0;
}

/* The hex of save coordinates x (the column) and y (the row). */
internal hex_coord
feature_save_hex(u32 x, u32 y)
{
    hex_offset offset;
    offset.col = (s32)x;
    offset.row = (s32)y;
    return hex_from_offset(offset);
}

internal void
feature_save_map_tests(test_counts *counts)
{
    engine_fixture f;
    engine_fixture_init_real(&f, 1, 65536);
    engine_api *api = &f.api;
    void *e = api->engine;
    u32 mark = api->ledger_count(e);
    TEST_CHECK(counts, api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH).status == ENGINE_LOAD_OK, "sample.s11 loads on the real map");
    sim_event loaded = {};
    TEST_CHECK(counts, feature_find_event(api, SIM_EVENT_SAVE_MAP_LOADED, mark, &loaded) && loaded.delta > 500 && loaded.detail == FEATURE_SAVE_UNITS &&
                       loaded.assumed,
               "the ledger has the load of the map state: the facilities, the buildings and 56 units (the 走舸 on water assumed, Q-211)");
    TEST_CHECK(counts, loaded.subject == 0, "each unit stands on the hex of its record: no unit needs the fallback hex");

    /* 柴桑: the numbers of the city information screen of the game. */
    u8 city = FEATURE_SAVE_CHAISANG;
    engine_city_slots site = api->city_slots(e, city);
    TEST_CHECK(counts, site.slot_count == 12 && site.used == 12, "柴桑 has 12 slots and all 12 are in use");
    TEST_CHECK(counts, feature_count_facility(api, city, "market", 3) == 5 && feature_count_facility(api, city, "mint", 1) == 1 &&
                       feature_count_facility(api, city, "fish_market", 1) == 1 && feature_count_facility(api, city, "drill_ground", 1) == 1 &&
                       feature_count_facility(api, city, "military_farm", 1) == 1 && feature_count_facility(api, city, "barracks", 3) == 1 &&
                       feature_count_facility(api, city, "smithy", 3) == 1 && feature_count_facility(api, city, "shipyard", 1) == 1,
               "柴桑: 5 markets of level 3, a mint, a fish market, a drill ground, a military farm, barracks and a smithy of level 3, a shipyard");
    u8 mint = feature_facility_id(api, "mint"), market = feature_facility_id(api, "market");
    hex_coord mint_hex = {};
    for (u32 slot = 0; slot < site.slot_count; ++slot)
    {
        engine_facility facility = api->facility_at(e, city, (u8)slot);
        if (facility.facility == mint) mint_hex = hex_make(facility.q, facility.r);
    }
    u32 markets_beside = 0;
    for (u32 slot = 0; slot < site.slot_count; ++slot)
    {
        engine_facility facility = api->facility_at(e, city, (u8)slot);
        if (facility.facility == market && hex_distance(mint_hex, hex_make(facility.q, facility.r)) == 1) markets_beside++;
    }
    TEST_CHECK(counts, markets_beside == 5, "the mint stands in the middle of the flower of the five markets");
    engine_hex mint_data = api->hex_at(e, mint_hex.q, mint_hex.r);
    TEST_CHECK(counts, mint_data.slot_city == city && mint_data.facility == mint && mint_data.facility_level == 1,
               "hex_at tells the facility, its level and its city on a slot hex");
    engine_city record = api->city_at(e, city);
    TEST_CHECK(counts, record.owner_force_id == 6 && record.commanding_officer_id == 804 && site.governor_id == 804,
               "the owner and the governor of 柴桑 come from the city record (force 6, officer 804)");

    /* The bases and the buildings of the save. */
    b32 bases_ok = true;
    for (u32 index = 0; index < api->base_count(e); ++index)
    {
        engine_base base = api->base_at(e, index);
        if (base.durability == 0 || base.durability > 65535) bases_ok = false;
    }
    u32 buildings = 0, dikes = 0, rockfalls = 0;
    for (u32 id = 0; id < api->building_count(e); ++id)
    {
        engine_building building = api->building_at(e, id);
        if (building.id == ENGINE_NO_BUILDING) continue;
        engine_building_kind kind = api->building_kind_at(e, building.kind);
        if (kind.natural) { dikes += strcmp(kind.key, "dike") == 0; rockfalls += strcmp(kind.key, "rockfall") == 0; }
        else buildings++;
    }
    /* The gate and port records of the save (0x08428D, spec 9632e15): the owner and the soldiers. */
    engine_base hukou = api->base_at(e, 72 - CONTENT_FIRST_BASE), lukou = api->base_at(e, 74 - CONTENT_FIRST_BASE);
    engine_base poyang = api->base_at(e, 75 - CONTENT_FIRST_BASE), huguan = api->base_at(e, 42 - CONTENT_FIRST_BASE);
    TEST_CHECK(counts, hukou.id == 72 && hukou.troops == 6266, "皖口港 (base 72) has the 6266 soldiers that the game shows");
    TEST_CHECK(counts, lukou.id == 74 && lukou.troops == 100, "陸口港 (base 74) has the 100 soldiers of its name plate in the game");
    TEST_CHECK(counts, poyang.id == 75 && poyang.force == ENGINE_NO_FACTION && poyang.troops == 0, "鄱陽港 (base 75) has no owner and no soldiers");
    TEST_CHECK(counts, huguan.id == 42 && huguan.troops == 14534, "壺關 (base 42) has 14534 soldiers");
    u32 at_default = 0, without_owner = 0;
    for (u32 index = 0; index < api->base_count(e); ++index)
    {
        engine_base base = api->base_at(e, index);
        at_default    += base.troops == 3000;
        without_owner += base.force == ENGINE_NO_FACTION;
    }
    TEST_CHECK(counts, at_default == 3 && without_owner == 8,
               "the soldiers come from the records: only bases 53, 55 and 61 have 3000 in the save; 8 bases have no owner");
    TEST_CHECK(counts, bases_ok && buildings == FEATURE_SAVE_BUILDINGS && dikes == 4 && rockfalls == 18,
               "each base has its durability from the save; 30 military buildings and traps stand on their hexes; 4 dikes and 18 rockfalls stay");

    /* The units of the save, on the hexes of their records (+23 x, +25 y). */
    u32 units = 0, on_water = 0;
    b32 units_ok = true, enterable = true, classes_ok = true;
    for (u32 id = 0; id < api->unit_count(e); ++id)
    {
        engine_unit unit = api->unit_at(e, id);
        if (unit.id == ENGINE_NO_UNIT) continue;
        units++;
        engine_officer leader = api->officer_by_id(e, unit.officers[0]);
        engine_hex hex = api->hex_at(e, unit.q, unit.r);
        if (leader.officer_id == ENGINE_NO_OFFICER || leader.force_id != unit.force_id || leader.current_position != SAVE_UNIT_LOCATION_BASE + id ||
            !hex.on_map || hex.unit_id != id)
        {
            units_ok = false;
        }
        if (!api->terrain_kind_at(e, hex.terrain).enterable) enterable = false;
        b32 water = hex.terrain == FEATURE_TERRAIN_RIVER || hex.terrain == FEATURE_TERRAIN_SEA;
        on_water += water;
        if (unit.unit_type == ENGINE_NO_UNIT_TYPE) classes_ok = false;
    }
    engine_unit first = api->unit_at(e, 0);
    TEST_CHECK(counts, units == FEATURE_SAVE_UNITS && units_ok,
               "56 units stand on the map; each has its leader and the force of the leader, and the id of its officer position");
    TEST_CHECK(counts, enterable && on_water == FEATURE_SAVE_WATER_UNITS, "each unit stands on a terrain that a unit can enter; 12 stand on 河 or 海");
    TEST_CHECK(counts, classes_ok, "each unit has the land class of its record (+36)");

    /* The 部隊情報 screens of the game (spec b3ed57d): soldiers, morale, gold, grain, classes and 混亂. */
    engine_unit yan_xing = api->unit_at(e, 12), jiang_qin = api->unit_at(e, 22);
    TEST_CHECK(counts, yan_xing.soldiers == 3284 && yan_xing.morale == 100 && yan_xing.gold == 168 && yan_xing.grain == 6135 &&
                       feature_type_is(api, yan_xing.unit_type, "cavalry") && yan_xing.ship_type == ENGINE_NO_UNIT_TYPE &&
                       yan_xing.state == UNIT_STATE_CONFUSED,
               "unit 12 (閻行) at 49,67: 3284 soldiers, morale 100, gold 168, grain 6135, 軍馬 and 走舸, 混亂");
    TEST_CHECK(counts, jiang_qin.soldiers == 1478 && jiang_qin.morale == 100 && jiang_qin.gold == 200 && jiang_qin.grain == 3944 &&
                       feature_type_is(api, jiang_qin.unit_type, "halberd") && jiang_qin.ship_type == ENGINE_NO_UNIT_TYPE &&
                       jiang_qin.state == UNIT_STATE_CONFUSED,
               "unit 22 (蔣欽) at 98,106: 1478 soldiers, morale 100, gold 200, grain 3944, 戟 and 走舸, 混亂");

    /* The equipment list (Q-217): the two transports of the player's screens and their cargo. */
    engine_unit du_ji = api->unit_at(e, 52), lu_meng = api->unit_at(e, 27);
    hex_offset du_ji_hex = hex_to_offset(hex_make(du_ji.q, du_ji.r)), lu_meng_hex = hex_to_offset(hex_make(lu_meng.q, lu_meng.r));
    TEST_CHECK(counts, feature_type_is(api, du_ji.unit_type, "transport") && du_ji_hex.col == 69 && du_ji_hex.row == 98 &&
                       du_ji.soldiers == 6589 && du_ji.morale == 98 && du_ji.gold == 18556 && du_ji.grain == 91145,
               "unit 52 (杜畿) is a transport at 69,98: 6589 soldiers, morale 98, gold 18556, grain 91145");
    TEST_CHECK(counts, du_ji.cargo[CITY_STOCK_SPEARS] == 7568 && du_ji.cargo[CITY_STOCK_HALBERDS] == 4999 && du_ji.cargo[CITY_STOCK_CROSSBOWS] == 1559 &&
                       du_ji.cargo[CITY_STOCK_WAR_HORSES] == 0 && du_ji.cargo[CITY_STOCK_BATTERING_RAMS] == 2 && du_ji.cargo[CITY_STOCK_SIEGE_TOWERS] == 2 &&
                       du_ji.destination == 17,
               "杜畿 carries 7568 槍, 4999 戟, 1559 弩, 2 木獸 and 2 井闌, and moves to 長安 (17)");
    TEST_CHECK(counts, feature_type_is(api, lu_meng.unit_type, "transport") && lu_meng_hex.col == 115 && lu_meng_hex.row == 136 &&
                       lu_meng.soldiers == 14113 && lu_meng.morale == 92 && lu_meng.gold == 6064 && lu_meng.grain == 50835 &&
                       lu_meng.cargo[CITY_STOCK_CROSSBOWS] == 2120 && lu_meng.cargo[CITY_STOCK_SHIP_TYPE_1] == 5 && lu_meng.cargo[CITY_STOCK_SPEARS] == 0 &&
                       lu_meng.destination == 26,
               "unit 27 (呂蒙) is a transport at 115,136: 14113 soldiers, morale 92, gold 6064, grain 50835, 2120 弩 and 5 樓船, to 柴桑");
    u32 transports = 0;
    for (u32 id = 0; id < api->unit_count(e); ++id) transports += api->unit_at(e, id).id != ENGINE_NO_UNIT && feature_type_is(api, api->unit_at(e, id).unit_type, "transport");
    TEST_CHECK(counts, transports == 6, "the six units 13, 27, 31, 42, 52 and 53 are transports");
    TEST_CHECK(counts, feature_type_is(api, api->unit_at(e, 16).ship_type, "tower_ship") && feature_type_is(api, api->unit_at(e, 21).ship_type, "tower_ship") &&
                       feature_type_is(api, api->unit_at(e, 23).ship_type, "tower_ship") && api->unit_at(e, 12).ship_type == ENGINE_NO_UNIT_TYPE &&
                       api->unit_at(e, 22).ship_type == ENGINE_NO_UNIT_TYPE,
               "the units 16, 21 and 23 own a 樓船 (21 also on land); 12 and 22 own no ship and are a 走舸 on water");

    engine_unit sun_ce = api->unit_at(e, 16), zhou_tai = api->unit_at(e, 23);
    hex_offset sun_ce_hex = hex_to_offset(hex_make(sun_ce.q, sun_ce.r)), zhou_tai_hex = hex_to_offset(hex_make(zhou_tai.q, zhou_tai.r));
    TEST_CHECK(counts, sun_ce.officers[0] == 370 && sun_ce_hex.col == 135 && sun_ce_hex.row == 129 &&
                       api->hex_at(e, sun_ce.q, sun_ce.r).terrain == FEATURE_TERRAIN_RIVER,
               "unit 16 (孫策, officer 370) stands at 135,129 on 河, as the game shows");
    TEST_CHECK(counts, zhou_tai.officers[0] == 242 && zhou_tai_hex.col == 134 && zhou_tai_hex.row == 128 &&
                       api->hex_at(e, zhou_tai.q, zhou_tai.r).terrain == FEATURE_TERRAIN_RIVER,
               "unit 23 (周泰, officer 242) stands at 134,128 on 河, as the game shows");
    TEST_CHECK(counts, feature_type_is(api, sun_ce.unit_type, "pike") && feature_type_is(api, zhou_tai.unit_type, "halberd") &&
                       feature_type_is(api, sun_ce.ship_type, "tower_ship") && feature_type_is(api, zhou_tai.ship_type, "tower_ship") &&
                       sun_ce.state == UNIT_STATE_NONE && zhou_tai.state == UNIT_STATE_NONE,
               "孫策 (槍) and 周泰 (戟) ride 樓船 on the river and are not in 混亂");

    /* The per-hex block: the fire and the event sites (Q-200, Q-205). */
    TEST_CHECK(counts, api->hex_at(e, zhou_tai.q, zhou_tai.r).fire_turns == 1, "the hex of 周泰 (134,128) burns with 1 turn left, as the player saw");
    TEST_CHECK(counts, api->hex_at(e, yan_xing.q, yan_xing.r).fire_turns == 3 && api->hex_at(e, jiang_qin.q, jiang_qin.r).fire_turns == 4,
               "49,67 burns with 3 turns and 98,106 with 4 turns");
    u32 burning = 0;
    for (u32 cell = 0; cell < 200 * 200; ++cell)
    {
        hex_coord hex = feature_save_hex(cell % 200, cell / 200);
        burning += api->hex_at(e, hex.q, hex.r).fire_turns > 0;
    }
    u32 temples = 0, ruins = 0;
    for (u32 index = 0; index < api->relic_count(e); ++index)
    {
        engine_relic relic = api->relic_at(e, index);
        temples += relic.kind == RELIC_TEMPLE;
        ruins   += relic.kind == RELIC_RUIN;
    }
    TEST_CHECK(counts, burning == 3, "exactly 3 hexes of the save burn");
    TEST_CHECK(counts, api->relic_count(e) == 12 && temples == 6 && ruins == 6,
               "the 12 event sites of the save load: 6 temples (mark 16, assumed) and 6 ruins (mark 32), Q-205");

    /* 混亂 (Q-218): unit 12 stays confused at the load and cannot move, attack or use a plot. */
    feature_play_as(api, yan_xing.force_id);
    hex_coord beside = hex_neighbor(hex_make(yan_xing.q, yan_xing.r), 0);
    engine_command order = {};
    order.unit = 12; order.q = beside.q; order.r = beside.r; order.row = 0;
    order.kind = ENGINE_COMMAND_MOVE;
    b32 move_refused = api->can_issue(e, &order).refusal == SIM_REFUSAL_UNIT_CONFUSED;
    order.kind = ENGINE_COMMAND_ATTACK;
    b32 attack_refused = api->can_issue(e, &order).refusal == SIM_REFUSAL_UNIT_CONFUSED;
    order.kind = ENGINE_COMMAND_PLOT;
    b32 plot_refused = api->can_issue(e, &order).refusal == SIM_REFUSAL_UNIT_CONFUSED;
    TEST_CHECK(counts, api->unit_at(e, 12).state == UNIT_STATE_CONFUSED && move_refused && attack_refused && plot_refused,
               "a unit in 混亂 refuses a move, an attack and a plot");
    /* A move on the real map: the check and the move each build a reach of 40,000 cells. */
    feature_play_as(api, sun_ce.force_id);
    engine_reach_view reach = api->reachable(e, 16);
    hex_coord goal = hex_make(sun_ce.q, sun_ce.r);
    for (u32 index = reach.count; index-- > 1;)
    {
        engine_hex there = api->hex_at(e, reach.cells[index].q, reach.cells[index].r);
        if (there.city_id == ENGINE_NO_CITY && there.base_id == ENGINE_NO_BASE) { goal = reach.cells[index]; break; }   /* not into a base */
    }
    TEST_CHECK(counts, reach.count > 1 && api->command_move(e, 16, goal.q, goal.r).refusal == SIM_REFUSAL_NONE &&
                       api->unit_at(e, 16).q == goal.q && api->unit_at(e, 16).r == goal.r,
               "the 樓船 of 孫策 moves on the real map (the move gives back the scratch memory of its check)");
    TEST_CHECK(counts, first.officers[0] == 158 && first.officers[1] == 552 && first.soldiers == 6000,
               "unit 0 has the leader 158, the deputy 552 and 6000 troops");
    TEST_CHECK(counts, first.grain_turns == first.grain / (first.soldiers / 40) && first.grain_turns != ENGINE_GRAIN_TURNS_UNKNOWN,
               "the supply icon of a unit of the save uses the grain of the record (+13)");
    feature_play_as(api, api->city_at(e, city).owner_force_id);
    feature_next_turn(api);
    engine_unit fed = api->unit_at(e, 0);
    TEST_CHECK(counts, fed.soldiers == 6000 && fed.grain < first.grain && (first.grain - fed.grain) % (6000 / 40) == 0,
               "a unit of the save eats its grain: 150 each turn for 6000 soldiers");
    engine_fixture_destroy(&f);
}
