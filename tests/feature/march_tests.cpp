/* Feature tests of M5 march (terrain-and-map.md, military-units-and-tactics.md,
   city-commands.md; F-072 to F-076, F-080, F-081), through engine_api with the
   real engine: the tables, the placeholder map, 出征, the move range, the
   zone of control, the terrain losses, 待命, 退卻, 輸送, 補給 and the grain of
   a unit. The command costs and the class movement are documented; the
   terrain costs, the losses and the value formula are assumed. */

#define FEATURE_SORTIE_AP   10      /* city-commands.md */
#define FEATURE_UNIT_PIKE   1
#define FEATURE_UNIT_CAVALRY 4
#define FEATURE_UNIT_TRANSPORT 12
#define FEATURE_SOLDIER_CAP 5000    /* assumed, Q-169 */
#define FEATURE_TECH_ROUGH_MARCH_M5 17

internal u8
feature_terrain_id(engine_api *api, const char *key)
{
    for (u32 index = 0; index < api->terrain_kind_count(api->engine); ++index)
    {
        if (strcmp(api->terrain_kind_at(api->engine, index).key, key) == 0) return (u8)index;
    }
    return ENGINE_NO_TERRAIN;
}

internal u8
feature_terrain_kind_at(engine_api *api, hex_coord hex)
{
    engine_terrain_view view = api->terrain_view(api->engine);
    hex_offset o = hex_to_offset(hex);
    if (o.col < 0 || o.row < 0 || (u32)o.col >= view.width || (u32)o.row >= view.height) return ENGINE_NO_TERRAIN;
    return view.kind[(u32)o.row * view.width + (u32)o.col];
}

/* A pike sortie of the city setup: 1000 soldiers, 500 gold, 3000 grain. */
internal engine_sortie_order
feature_pike_order(engine_api *api, u8 city, u32 officers_wanted)
{
    engine_sortie_order order = {};
    order.city_id     = city;
    order.unit_type   = FEATURE_UNIT_PIKE;
    order.soldiers    = 1000;
    order.gold        = 500;
    order.grain       = 3000;
    order.destination = ENGINE_NO_CITY;
    feature_take_officers(api, city, order.officers, officers_wanted);
    return order;
}

internal void
feature_march_setup(feature_city_setup *setup)
{
    feature_city_setup_init(setup);
    engine_api *api = setup->api;
    void *e = api->engine;
    api->set_city_resource(e, setup->city, SIM_RESOURCE_SOLDIERS, 0, 20000);
    api->set_city_resource(e, setup->city, SIM_RESOURCE_GRAIN, 0, 100000);
    api->set_city_resource(e, setup->city, SIM_RESOURCE_WEAPONS, 0, 10000);   /* pikes */
}

internal void
feature_march_table_tests(test_counts *counts, engine_api *api)
{
    void *e = api->engine;
    TEST_CHECK(counts, api->terrain_kind_count(e) == 20 && api->content_status(e).terrain_count == 20, "terrain.tsv has the 20 terrains of the map (M10)");
    TEST_CHECK(counts, api->unit_type_count(e) == 13 && api->content_status(e).unit_type_count == 13, "unit_types.tsv has 12 classes and the transport unit");
    engine_unit_type pike = api->unit_type_at(e, FEATURE_UNIT_PIKE), cavalry = api->unit_type_at(e, FEATURE_UNIT_CAVALRY);
    engine_unit_type ram = api->unit_type_at(e, 5), crossbow = api->unit_type_at(e, 3);
    TEST_CHECK(counts, pike.move == 22 && cavalry.move == 28 && ram.move == 18, "movement: 22 foot, 28 cavalry, 18 siege (observed)");
    TEST_CHECK(counts, pike.attack == GRADE_A && pike.defence == GRADE_A && cavalry.attack == GRADE_S && api->unit_type_at(e, 2).defence == GRADE_S,
               "the grades of the manual: pike A/A, cavalry attack S, halberd defence S");
    TEST_CHECK(counts, !ram.can_attack && !ram.counterattack && crossbow.range == 2, "siege weapons do not attack or counterattack; crossbows reach 2 hexes");
    engine_terrain_kind small_path = api->terrain_kind_at(e, feature_terrain_id(api, "small_path"));
    engine_terrain_kind forest = api->terrain_kind_at(e, feature_terrain_id(api, "forest"));
    TEST_CHECK(counts, small_path.needs_technique == FEATURE_TECH_ROUGH_MARCH_M5 && !(forest.tactics & (1u << TACTIC_CAVALRY)) &&
                       api->terrain_kind_at(e, feature_terrain_id(api, "cliff")).cost[MOVEMENT_FOOT] == 0,
               "a small path needs 難所行軍; no cavalry tactics in a forest; nothing enters a cliff");
}

internal void
feature_march_map_tests(test_counts *counts, engine_api *api)
{
    void *e = api->engine;
    u8 city_kind = feature_terrain_id(api, "city");
    b32 cities_ok = true;
    for (u32 index = 0; index < api->city_count(e); ++index)
    {
        engine_city_slots slots = api->city_slots(e, api->city_at(e, index).city_id);
        if (feature_terrain_kind_at(api, hex_make(slots.q, slots.r)) != city_kind) cities_ok = false;
    }
    TEST_CHECK(counts, cities_ok, "each city stands on a city hex of the placeholder map");
    engine_city_slots a = api->city_slots(e, 0), b = api->city_slots(e, 1);
    TEST_CHECK(counts, api->path_cost(e, a.q, a.r, b.q, b.r) != HEX_NO_PATH, "a road links two neighbour cities");
    engine_city_slots hanzhong = api->city_slots(e, 36), zitong = api->city_slots(e, 37);
    hex_coord middle = hex_make((hanzhong.q + zitong.q) / 2, (hanzhong.r + zitong.r) / 2);
    TEST_CHECK(counts, feature_terrain_kind_at(api, middle) == feature_terrain_id(api, "plank_road"),
               "a plank road links 漢中 and 梓潼 (placeholder of terrain-and-map.md)");
}

internal void
feature_sortie_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_march_setup(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 city = setup.city, force = setup.force;
    feature_march_table_tests(counts, api);
    feature_march_map_tests(counts, api);

    engine_sortie_order proposal = api->sortie_proposal(e, city, FEATURE_UNIT_PIKE);
    TEST_CHECK(counts, proposal.officers[0] != ENGINE_NO_OFFICER && proposal.soldiers == FEATURE_SOLDIER_CAP && proposal.grain > 0,
               "自動編制 proposes officers, the soldiers that the commander can lead, and grain");

    engine_sortie_order order = feature_pike_order(api, city, 3);
    engine_command command = {};
    command.kind   = ENGINE_COMMAND_SORTIE;
    command.sortie = order;
    engine_check check = api->can_issue(e, &command);
    TEST_CHECK(counts, check.refusal == SIM_REFUSAL_NONE && check.action_points == FEATURE_SORTIE_AP && check.quantity == 1000,
               "sortie: can_issue gives 10 action points and the soldiers");
    command.sortie.soldiers = 0;
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_NO_SOLDIERS, "refused: no soldiers");
    command.sortie.soldiers = FEATURE_SOLDIER_CAP + 1;
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_NO_SOLDIERS, "refused: more soldiers than the commander leads (assumed 5000)");
    command.sortie.soldiers = 1000;
    command.sortie.unit_type = FEATURE_UNIT_CAVALRY;
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_NO_WEAPONS, "refused: cavalry needs a war horse for each soldier");
    command.sortie.unit_type = FEATURE_UNIT_PIKE;
    command.sortie.grain = 1000000;
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_NO_GRAIN, "refused: more grain than the city has");
    command.sortie.grain = 3000;
    command.sortie.officers[0] = ENGINE_NO_OFFICER;
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_NO_OFFICER, "refused: no commander");
    command.sortie = order;
    api->set_actions(e, force, FEATURE_SORTIE_AP - 1);
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_NO_ACTION_POINTS, "refused: 9 action points");
    api->set_actions(e, force, 255);

    engine_city before = api->city_at(e, city);
    u32 mark = api->ledger_count(e);
    engine_command_result result = api->command_sortie(e, &order);
    u32 unit_id = api->unit_at_hex(e, 0, 0);
    for (u32 id = 0; id < api->unit_count(e); ++id)
    {
        if (api->unit_at(e, id).id != ENGINE_NO_UNIT && api->unit_at(e, id).home_city == city) unit_id = id;
    }
    engine_unit unit = api->unit_at(e, unit_id);
    engine_city_slots site = api->city_slots(e, city);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && unit.id != ENGINE_NO_UNIT && unit.force_id == force &&
                       hex_distance(hex_make(unit.q, unit.r), hex_make(site.q, site.r)) == 1 && api->unit_at_hex(e, unit.q, unit.r) == unit_id,
               "the unit stands on a hex next to the city");
    engine_city after = api->city_at(e, city);
    TEST_CHECK(counts, unit.soldiers == 1000 && unit.gold == 500 && unit.grain == 3000 && unit.weapons == 1000 && unit.morale == api->city_economy(e, city).morale &&
                       after.troops == before.troops - 1000 && after.gold == before.gold - 500 && after.food == before.food - 3000 &&
                       after.equipment_stocks[0] == before.equipment_stocks[0] - 1000,
               "the soldiers, a pike for each, the gold and the grain leave the city; the unit has the morale of the city");
    TEST_CHECK(counts, feature_ledger_sum(api, city, SIM_RESOURCE_SOLDIERS, mark) == -1000 && feature_ledger_sum(api, city, SIM_RESOURCE_GOLD, mark) == -500 &&
                       feature_ledger_sum(api, city, SIM_RESOURCE_GRAIN, mark) == -3000,
               "the ledger of the city explains each change");
    engine_officer commander = {};
    TEST_CHECK(counts, feature_officer_by_id(api, order.officers[0], &commander) && commander.current_position == 87 + unit_id && commander.acted &&
                       unit.officers[0] == order.officers[0],
               "the officers are in the unit (position 87 + the unit) and acted");
    TEST_CHECK(counts, unit.values.move == 22 && unit.values.attack > 0 && unit.values.assumed, "the unit has its values (the formula is assumed)");
    engine_fixture_destroy(&setup.fixture);
}

/* Cliffs around the unit except a straight corridor of 6 hexes from it in a
   direction that stays on the map and away from cities. Returns the
   direction, or HEX_DIRECTION_COUNT. */
/* The first direction from start with 6 hexes on the map and away from
   every city, or HEX_DIRECTION_COUNT. */
internal u32
feature_corridor_direction(engine_api *api, hex_coord start)
{
    void *e = api->engine;
    engine_map_size size = api->map_size(e);
    for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d)
    {
        b32 ok = true;
        hex_coord cell = start;
        for (u32 step = 0; step < 6 && ok; ++step)
        {
            cell = hex_neighbor(cell, d);
            hex_offset o = hex_to_offset(cell);
            if (o.col < 1 || o.row < 1 || (u32)o.col + 1 >= size.width || (u32)o.row + 1 >= size.height) ok = false;
            for (u32 city = 0; city < api->city_count(e) && ok; ++city)
            {
                engine_city_slots slots = api->city_slots(e, api->city_at(e, city).city_id);
                if (hex_distance(cell, hex_make(slots.q, slots.r)) <= 1) ok = false;
            }
        }
        if (ok) return d;
    }
    return HEX_DIRECTION_COUNT;
}

internal u32
feature_corridor(engine_api *api, hex_coord start)
{
    void *e = api->engine;
    u8 cliff = feature_terrain_id(api, "cliff"), grass = feature_terrain_id(api, "grass");
    u32 d = feature_corridor_direction(api, start);
    if (d >= HEX_DIRECTION_COUNT) return HEX_DIRECTION_COUNT;
    for (s32 dq = -8; dq <= 8; ++dq)
    {
        for (s32 dr = -8; dr <= 8; ++dr)
        {
            hex_coord hex = hex_make(start.q + dq, start.r + dr);
            if (hex_distance(hex, start) > 8 || hex_equal(hex, start)) continue;
            b32 in_corridor = false;
            hex_coord along = start;
            for (u32 step = 0; step < 6; ++step) { along = hex_neighbor(along, d); in_corridor |= hex_equal(along, hex); }
            api->set_cell_terrain(e, hex.q, hex.r, in_corridor ? grass : cliff, 1);
        }
    }
    return d;
}

/* A corridor for a unit beside its city. On the flat-top grid (M10) only
   some hexes beside a city of the placeholder board have a straight
   corridor, so the editor moves the unit to the nearest free hex with
   one. start gets the hex of the unit. */
internal u32
feature_corridor_for_unit(engine_api *api, u32 unit_id, hex_coord *start)
{
    void *e = api->engine;
    engine_unit unit = api->unit_at(e, unit_id);
    *start = hex_make(unit.q, unit.r);
    if (feature_corridor_direction(api, *start) < HEX_DIRECTION_COUNT) return feature_corridor(api, *start);
    for (s32 dq = -2; dq <= 2; ++dq)
    {
        for (s32 dr = -2; dr <= 2; ++dr)
        {
            hex_coord hex = hex_make(unit.q + dq, unit.r + dr);
            if (hex_distance(hex, *start) > 2 || feature_corridor_direction(api, hex) >= HEX_DIRECTION_COUNT) continue;
            if (api->unit_at_hex(e, hex.q, hex.r) != ENGINE_NO_UNIT || api->hex_at(e, hex.q, hex.r).city_id != ENGINE_NO_CITY) continue;
            api->set_cell_terrain(e, hex.q, hex.r, feature_terrain_id(api, "grass"), 1);
            if (!api->set_unit_place(e, unit_id, unit.force_id, hex.q, hex.r)) continue;
            *start = hex;
            return feature_corridor(api, hex);
        }
    }
    return HEX_DIRECTION_COUNT;
}

internal hex_coord
feature_along(hex_coord start, u32 direction, u32 steps)
{
    hex_coord cell = start;
    for (u32 step = 0; step < steps; ++step) cell = hex_neighbor(cell, direction);
    return cell;
}

internal b32
feature_reaches(engine_api *api, u32 unit_id, hex_coord hex, u32 *cost)
{
    engine_reach_view view = api->reachable(api->engine, unit_id);
    for (u32 index = 0; index < view.count; ++index)
    {
        if (hex_equal(view.cells[index], hex))
        {
            if (cost) *cost = view.costs[index];
            return true;
        }
    }
    return false;
}

/* The first unit of the city setup after a sortie. */
internal u32
feature_sortie_unit(engine_api *api, u8 city, u32 officers_wanted, u8 unit_type = FEATURE_UNIT_PIKE)
{
    engine_sortie_order order = feature_pike_order(api, city, officers_wanted);
    order.unit_type = unit_type;
    if (api->command_sortie(api->engine, &order).refusal != SIM_REFUSAL_NONE) return ENGINE_NO_UNIT;
    u32 found = ENGINE_NO_UNIT;
    for (u32 id = 0; id < api->unit_count(api->engine); ++id)
    {
        engine_unit unit = api->unit_at(api->engine, id);
        if (unit.id != ENGINE_NO_UNIT && unit.officers[0] == order.officers[0]) found = id;
    }
    return found;
}

internal void
feature_move_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_march_setup(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 city = setup.city, force = setup.force;
    u32 unit_id = feature_sortie_unit(api, city, 1);
    hex_coord start = {};
    u32 d = feature_corridor_for_unit(api, unit_id, &start);
    engine_unit unit = api->unit_at(e, unit_id);
    TEST_CHECK(counts, unit_id != ENGINE_NO_UNIT && d < HEX_DIRECTION_COUNT, "a unit stands at the start of a corridor of grass");
    if (d >= HEX_DIRECTION_COUNT) { engine_fixture_destroy(&setup.fixture); return; }

    u32 cost = 0;
    TEST_CHECK(counts, feature_reaches(api, unit_id, feature_along(start, d, 4), &cost) && cost == 20 && !feature_reaches(api, unit_id, feature_along(start, d, 5), 0),
               "on grass (cost 5, movement-costs.tsv) a pike unit with movement 22 reaches 4 hexes and not 5");

    /* The zone of control: an enemy next to the second hex of the corridor. */
    hex_coord second = feature_along(start, d, 2);
    hex_coord enemy = hex_neighbor(second, (d + 1) % HEX_DIRECTION_COUNT);
    u32 enemy_id = api->place_unit(e, (u8)(force + 1), enemy.q, enemy.r);
    TEST_CHECK(counts, feature_reaches(api, unit_id, second, 0) && !feature_reaches(api, unit_id, feature_along(start, d, 3), 0),
               "a unit stops in the zone of control of an enemy unit (manual p.14)");
    engine_command move = {};
    move.kind = ENGINE_COMMAND_MOVE;
    move.unit = unit_id;
    hex_coord third = feature_along(start, d, 3);
    move.q = third.q; move.r = third.r;
    TEST_CHECK(counts, api->can_issue(e, &move).refusal == SIM_REFUSAL_UNREACHABLE, "refused: the hex is out of the range");
    TEST_CHECK(counts, api->command_move(e, unit_id, enemy.q, enemy.r).refusal == SIM_REFUSAL_UNREACHABLE, "refused: another unit stands there");
    (void)enemy_id;

    u32 mark = api->ledger_count(e);
    engine_command_result moved = api->command_move(e, unit_id, second.q, second.r);
    unit = api->unit_at(e, unit_id);
    TEST_CHECK(counts, moved.refusal == SIM_REFUSAL_NONE && unit.q == second.q && unit.r == second.r && unit.moved && !unit.acted &&
                       feature_count_events(api, SIM_EVENT_UNIT_MOVED, mark) == 1,
               "the unit moves to the hex and the ledger has the move");
    TEST_CHECK(counts, api->command_move(e, unit_id, start.q, start.r).refusal == SIM_REFUSAL_UNIT_ACTED, "refused: one move in each turn");
    TEST_CHECK(counts, api->command_wait(e, unit_id).refusal == SIM_REFUSAL_NONE && api->unit_at(e, unit_id).acted, "待命 ends the turn of the unit");
    TEST_CHECK(counts, api->command_retreat(e, unit_id).refusal == SIM_REFUSAL_UNIT_ACTED, "refused: the unit acted in this turn");
    u16 none[3] = { ENGINE_NO_OFFICER, ENGINE_NO_OFFICER, ENGINE_NO_OFFICER };
    (void)none;

    /* Grain each turn. */
    mark = api->ledger_count(e);
    feature_next_own_turn(api, force);
    sim_event eaten = {};
    TEST_CHECK(counts, feature_find_event(api, SIM_EVENT_UNIT_GRAIN_EATEN, mark, &eaten) && eaten.delta == -25 && eaten.detail == unit_id && eaten.assumed,
               "each turn a unit of 1000 soldiers eats 25 grain (the rate of a city, observed; assumed for a unit)");
    TEST_CHECK(counts, !api->unit_at(e, unit_id).moved && !api->unit_at(e, unit_id).acted, "a new turn lets the unit move and act again");
    engine_unit fed = api->unit_at(e, unit_id);
    TEST_CHECK(counts, fed.grain_turns == fed.grain / 25 && fed.grain_turns != ENGINE_GRAIN_TURNS_UNKNOWN,
               "M12: unit_at gives the turns that the grain lasts at 25 grain each turn (the supply icon)");

    /* 退卻: back to the home city with everything. */
    engine_city before = api->city_at(e, city);
    unit = api->unit_at(e, unit_id);
    engine_command_result retreat = api->command_retreat(e, unit_id);
    engine_city after = api->city_at(e, city);
    engine_officer commander = {};
    feature_officer_by_id(api, unit.officers[0], &commander);
    TEST_CHECK(counts, retreat.refusal == SIM_REFUSAL_NONE && api->unit_at(e, unit_id).id == ENGINE_NO_UNIT &&
                       after.troops == before.troops + unit.soldiers && after.food == before.food + unit.grain &&
                       after.equipment_stocks[0] == before.equipment_stocks[0] + unit.weapons && commander.current_position == city,
               "退卻 returns the unit to its city at once (assumed): soldiers, weapons, grain and officers");
    engine_fixture_destroy(&setup.fixture);
}

internal void
feature_terrain_rule_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_march_setup(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 city = setup.city, force = setup.force;
    u32 unit_id = feature_sortie_unit(api, city, 1);
    hex_coord start = {};
    u32 d = feature_corridor_for_unit(api, unit_id, &start);
    if (d >= HEX_DIRECTION_COUNT) { TEST_CHECK(counts, false, "a corridor for the terrain tests"); engine_fixture_destroy(&setup.fixture); return; }
    hex_coord first = feature_along(start, d, 1), second = feature_along(start, d, 2);

    api->set_cell_terrain(e, first.q, first.r, feature_terrain_id(api, "small_path"), 2);
    TEST_CHECK(counts, !feature_reaches(api, unit_id, first, 0) && !feature_reaches(api, unit_id, second, 0),
               "without 難所行軍 a unit cannot enter a small path");
    api->grant_technique(e, force, FEATURE_TECH_ROUGH_MARCH_M5);
    u32 cost = 0;
    TEST_CHECK(counts, feature_reaches(api, unit_id, second, &cost) && cost == 11, "with 難所行軍 the small path is open (cost 6 + 5, movement-costs.tsv)");

    /* A plank road takes 10 percent of the soldiers, but not with 難所行軍. */
    api->set_cell_terrain(e, first.q, first.r, feature_terrain_id(api, "plank_road"), 3);
    u32 mark = api->ledger_count(e);
    api->command_move(e, unit_id, second.q, second.r);
    TEST_CHECK(counts, feature_count_events(api, SIM_EVENT_UNIT_SOLDIERS_LOST, mark) == 0 && api->unit_at(e, unit_id).soldiers == 1000,
               "with 難所行軍 a plank road takes no soldiers");
    api->set_cell_terrain(e, start.q, start.r, feature_terrain_id(api, "poison_spring"), 1);
    feature_next_own_turn(api, force);
    mark = api->ledger_count(e);
    api->command_move(e, unit_id, start.q, start.r);
    sim_event lost = {};
    TEST_CHECK(counts, feature_find_event(api, SIM_EVENT_UNIT_SOLDIERS_LOST, mark, &lost) && lost.delta == -100 && lost.assumed &&
                       api->unit_at(e, unit_id).soldiers == 900,
               "a poison spring takes 10 percent of the soldiers (assumed)");
    engine_fixture_destroy(&setup.fixture);
}

internal void
feature_transport_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_march_setup(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 city = setup.city, force = setup.force;
    u8 other = (u8)(city == 0 ? 1 : 0);
    api->set_city_owner(e, other, force);

    engine_sortie_order order = {};
    order.city_id     = city;
    order.unit_type   = FEATURE_UNIT_TRANSPORT;
    order.soldiers    = 500;
    order.gold        = 1000;
    order.grain       = 5000;
    order.destination = city;
    order.cargo[0]    = 300;
    feature_take_officers(api, city, order.officers, 1);
    engine_command command = {};
    command.kind   = ENGINE_COMMAND_TRANSPORT;
    command.sortie = order;
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_NO_CITY, "refused: the destination must be another city of the faction");
    order.destination = other;
    command.sortie = order;
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_NONE && api->can_issue(e, &command).action_points == 10,
               "輸送 costs 10 action points");
    u32 pikes = api->city_at(e, city).equipment_stocks[0];
    TEST_CHECK(counts, api->command_transport(e, &order).refusal == SIM_REFUSAL_NONE, "the transport unit leaves");
    u32 transport_id = ENGINE_NO_UNIT;
    for (u32 id = 0; id < api->unit_count(e); ++id)
    {
        engine_unit unit = api->unit_at(e, id);
        if (unit.id != ENGINE_NO_UNIT && unit.unit_type == FEATURE_UNIT_TRANSPORT) transport_id = id;
    }
    engine_unit transport = api->unit_at(e, transport_id);
    TEST_CHECK(counts, transport.id != ENGINE_NO_UNIT && transport.destination == other && transport.cargo[0] == 300 &&
                       api->city_at(e, city).equipment_stocks[0] == pikes - 300,
               "it carries the soldiers, the gold, the grain and the weapons to its destination");

    /* 補給: the transport unit gives to an adjacent unit of the faction. */
    u32 pike_id = feature_sortie_unit(api, city, 1);
    engine_unit pike = api->unit_at(e, pike_id);
    TEST_CHECK(counts, pike.id != ENGINE_NO_UNIT && hex_distance(hex_make(pike.q, pike.r), hex_make(transport.q, transport.r)) == 1,
               "a pike unit stands next to the transport unit");
    engine_command supply = {};
    supply.kind = ENGINE_COMMAND_SUPPLY;
    supply.unit = pike_id;
    supply.target_unit = transport_id;
    supply.gold = 100;
    TEST_CHECK(counts, api->can_issue(e, &supply).refusal == SIM_REFUSAL_NO_UNIT, "refused: only a transport unit supplies");
    TEST_CHECK(counts, api->command_supply(e, transport_id, pike_id, 0, 0, 0).refusal == SIM_REFUSAL_NO_AMOUNT, "refused: nothing to give");
    TEST_CHECK(counts, api->command_supply(e, transport_id, pike_id, 0, 200, 1000).refusal == SIM_REFUSAL_NONE &&
                       api->unit_at(e, pike_id).gold == pike.gold + 200 && api->unit_at(e, pike_id).grain == pike.grain + 1000 &&
                       api->unit_at(e, transport_id).gold == 800 && api->unit_at(e, transport_id).acted,
               "補給 moves gold and grain to the adjacent unit, and the transport unit acted");

    /* A unit that enters an own city gives it everything. */
    engine_city before = api->city_at(e, city);
    engine_city_slots site = api->city_slots(e, city);
    engine_unit full = api->unit_at(e, pike_id);
    TEST_CHECK(counts, api->command_move(e, pike_id, site.q, site.r).refusal == SIM_REFUSAL_NONE && api->unit_at(e, pike_id).id == ENGINE_NO_UNIT &&
                       api->city_at(e, city).troops == before.troops + full.soldiers && api->city_at(e, city).gold == before.gold + full.gold,
               "a unit that moves into an own city leaves the map; its soldiers and gold go into the city");
    engine_fixture_destroy(&setup.fixture);
}

internal void
feature_march_tests(test_counts *counts)
{
    feature_sortie_tests(counts);
    feature_move_tests(counts);
    feature_terrain_rule_tests(counts);
    feature_transport_tests(counts);
}
