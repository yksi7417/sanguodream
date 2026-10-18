/* Feature tests of M11, the state of a save on the real map
   (formats/save-file.md: the building array at 0x0008A3 and the troop unit
   array at 0x086B71), through engine_api with the real engine, the map of
   the pack and sample.s11. City 26 (柴桑) is the check of the user: 12 of
   12 slots in use, five markets of level 3 around a mint. */

#define FEATURE_SAVE_CHAISANG     26
#define FEATURE_SAVE_UNITS        53     /* the units that officers ride in, in sample.s11 */
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
               "the ledger has the load of the map state: the facilities, the buildings and 53 units (their hexes assumed, Q-211)");

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
    TEST_CHECK(counts, bases_ok && buildings == FEATURE_SAVE_BUILDINGS && dikes == 4 && rockfalls == 18,
               "each base has its durability from the save; 30 military buildings and traps stand on their hexes; 4 dikes and 18 rockfalls stay");

    /* The units of the save. */
    u32 units = 0;
    b32 units_ok = true;
    for (u32 id = 0; id < api->unit_count(e); ++id)
    {
        engine_unit unit = api->unit_at(e, id);
        if (unit.id == ENGINE_NO_UNIT) continue;
        units++;
        engine_officer leader = api->officer_by_id(e, unit.officers[0]);
        engine_hex hex = api->hex_at(e, unit.q, unit.r);
        if (leader.officer_id == ENGINE_NO_OFFICER || leader.force_id != unit.force_id || leader.current_position != SAVE_UNIT_LOCATION_BASE + id ||
            !hex.on_map || hex.city_id != ENGINE_NO_CITY || hex.unit_id != id || unit.unit_type != ENGINE_NO_UNIT_TYPE)
        {
            units_ok = false;
        }
    }
    engine_unit first = api->unit_at(e, 0);
    TEST_CHECK(counts, units == FEATURE_SAVE_UNITS && units_ok,
               "53 units stand on the map; each has its leader and the force of the leader, and the id of its officer position");
    TEST_CHECK(counts, first.officers[0] == 158 && first.officers[1] == 552 && first.soldiers == 6000,
               "unit 0 has the leader 158, the deputy 552 and 6000 troops");
    feature_play_as(api, api->city_at(e, city).owner_force_id);
    feature_next_turn(api);
    TEST_CHECK(counts, api->unit_at(e, 0).soldiers == 6000, "a unit of the save does not starve: the save gives no grain (Q-211)");
    TEST_CHECK(counts, api->unit_at(e, 0).grain_turns == ENGINE_GRAIN_TURNS_UNKNOWN, "M12: the supply icon of a unit of the save shows that its grain is not known");
    engine_fixture_destroy(&f);
}
