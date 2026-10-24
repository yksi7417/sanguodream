/* Feature tests of the long orders of a unit (city-commands.md, "Long
   orders"; manual p.24; clone gap report 2: G2-052, G2-056 and G2-058),
   through engine_api with the real engine on the placeholder map: the
   plan to a far_hex hex, 移動 out of the range, the step at 進行 and at
   委任部隊行動, 殲滅, the follow order, and a river hex in the range. The
   path rule and the end of 驅趕 are assumed (Q-227). */

#define FEATURE_ORDER_GRASS_COST 5    /* the pike row of movement_costs.tsv: grass */
#define FEATURE_ORDER_RIVER_COST 4    /* the boat row: river */

/* A pike unit of the city at the start of a corridor of six grass hexes
   in the direction d, with cliffs around it. */
struct feature_order_world
{
    feature_city_setup setup;
    u32       unit;
    hex_coord start;
    u32       d;
};

internal b32
feature_order_world_init(feature_order_world *world)
{
    feature_march_setup(&world->setup);
    engine_api *api = world->setup.api;
    world->unit = feature_sortie_unit(api, world->setup.city, 1);
    if (world->unit == ENGINE_NO_UNIT) return false;
    world->d = feature_corridor_for_unit(api, world->unit, &world->start);
    api->set_unit(api->engine, world->unit, 3000, 80);
    return world->d < HEX_DIRECTION_COUNT;
}

internal b32
feature_unit_stands_at(engine_api *api, u32 unit_id, hex_coord hex)
{
    engine_unit unit = api->unit_at(api->engine, unit_id);
    return unit.id != ENGINE_NO_UNIT && unit.q == hex.q && unit.r == hex.r;
}

/* G2-058: a move to a hex out of the range is a long order. */
internal void
feature_far_move_tests(test_counts *counts)
{
    feature_order_world world;
    if (!feature_order_world_init(&world)) { TEST_CHECK(counts, false, "the order world has a unit on a corridor"); return; }
    engine_api *api = world.setup.api;
    void *e = api->engine;
    u8 force = world.setup.force;
    hex_coord near_hex = feature_along(world.start, world.d, 2), far_hex = feature_along(world.start, world.d, 6);
    hex_coord range_end = feature_along(world.start, world.d, 4);

    engine_path_view to_near = api->path_to(e, world.unit, near_hex.q, near_hex.r);
    TEST_CHECK(counts, to_near.found && to_near.in_reach && to_near.count == 2 && to_near.cost == 2 * FEATURE_ORDER_GRASS_COST && to_near.days == 10,
               "path_to a hex of the range: two hexes, in the reach, 10 days");
    engine_path_view to_far = api->path_to(e, world.unit, far_hex.q, far_hex.r);
    TEST_CHECK(counts, to_far.found && !to_far.in_reach && to_far.count == 6 && to_far.cost == 6 * FEATURE_ORDER_GRASS_COST && to_far.days == 20 &&
                       to_far.cells[5].q == far_hex.q && to_far.cells[5].r == far_hex.r,
               "path_to a far_hex hex: six hexes of 5, out of the reach, 20 days (two turns of 22)");
    TEST_CHECK(counts, to_far.grain_days == api->unit_at(e, world.unit).grain_turns * 10 && to_far.base_count == 0,
               "the plan gives the days of grain of the unit and the bases on the way");
    hex_coord off = hex_neighbor(far_hex, (world.d + 2) % HEX_DIRECTION_COUNT);
    TEST_CHECK(counts, !api->path_to(e, world.unit, off.q, off.r).found, "no path leads into the cliffs");

    engine_command command = {};
    command.kind = ENGINE_COMMAND_MOVE;
    command.unit = world.unit;
    command.q = far_hex.q; command.r = far_hex.r;
    engine_check check = api->can_issue(e, &command);
    TEST_CHECK(counts, check.refusal == SIM_REFUSAL_NONE && check.days == 20 && check.turns == 2, "can_issue of a far_hex move gives the days of the plan");
    command.q = off.q; command.r = off.r;
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_UNREACHABLE, "a move into the cliffs is unreachable");

    u32 mark = api->ledger_count(e);
    engine_command_result result = api->command_move(e, world.unit, far_hex.q, far_hex.r);
    engine_unit unit = api->unit_at(e, world.unit);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && feature_unit_stands_at(api, world.unit, range_end),
               "G2-058: the far_hex move runs, and the unit goes to the end of its range in the same turn");
    TEST_CHECK(counts, unit.order == UNIT_ORDER_MOVE && unit.order_q == far_hex.q && unit.order_r == far_hex.r && unit.moved && !unit.acted,
               "the unit keeps the order 移動 to the far_hex hex (the plate shows 委)");
    sim_event ordered = {};
    TEST_CHECK(counts, feature_find_event(api, SIM_EVENT_UNIT_ORDERED, mark, &ordered) && ordered.detail == world.unit && ordered.delta == UNIT_ORDER_MOVE,
               "the ledger has the order");
    TEST_CHECK(counts, api->command_move(e, world.unit, far_hex.q, far_hex.r).refusal == SIM_REFUSAL_UNIT_ACTED, "a unit that moved cannot move again in the turn");

    TEST_CHECK(counts, feature_next_own_turn(api, force), "the next turn of the player starts");
    TEST_CHECK(counts, feature_unit_stands_at(api, world.unit, range_end) && api->unit_at(e, world.unit).order == UNIT_ORDER_MOVE,
               "a unit that moved in the turn of the order does not move at that 進行; it waits with its order");
    mark = api->ledger_count(e);
    TEST_CHECK(counts, feature_next_own_turn(api, force), "the turn after starts");
    sim_event done = {};
    TEST_CHECK(counts, feature_unit_stands_at(api, world.unit, far_hex) && api->unit_at(e, world.unit).order == UNIT_ORDER_NONE,
               "at the next 進行 the unit moves on to the far_hex hex, and the order ends");
    TEST_CHECK(counts, feature_find_event(api, SIM_EVENT_UNIT_ORDER_DONE, mark, &done) && done.detail == world.unit, "the ledger has the end of the order");
    engine_fixture_destroy(&world.setup.fixture);
}

/* G2-052: 殲滅 marches on an enemy unit and attacks it until it is gone. */
internal void
feature_destroy_order_tests(test_counts *counts)
{
    feature_order_world world;
    if (!feature_order_world_init(&world)) { TEST_CHECK(counts, false, "the order world has a unit on a corridor"); return; }
    engine_api *api = world.setup.api;
    void *e = api->engine;
    u8 force = world.setup.force, enemy_force = (u8)(force + 1);
    hex_coord enemy_hex = feature_along(world.start, world.d, 5), beside = feature_along(world.start, world.d, 4);
    u32 enemy = feature_sortie_unit(api, world.setup.city, 1);
    TEST_CHECK(counts, enemy != ENGINE_NO_UNIT && api->set_unit_place(e, enemy, enemy_force, enemy_hex.q, enemy_hex.r), "an enemy unit stands five hexes away");
    api->set_unit(e, enemy, 300, 80);
    u32 friend_unit = feature_sortie_unit(api, world.setup.city, 1);
    hex_coord behind = feature_along(world.start, world.d, 6);
    TEST_CHECK(counts, friend_unit != ENGINE_NO_UNIT && api->set_unit_place(e, friend_unit, force, behind.q, behind.r),
               "an own unit stands behind the enemy, out of the way");

    TEST_CHECK(counts, api->command_destroy(e, world.unit, world.unit).refusal == SIM_REFUSAL_NO_TARGET, "a unit cannot destroy itself");
    TEST_CHECK(counts, api->command_destroy(e, world.unit, friend_unit).refusal == SIM_REFUSAL_NO_TARGET, "an own unit is no target of 殲滅");
    TEST_CHECK(counts, api->command_follow(e, world.unit, enemy).refusal == SIM_REFUSAL_NO_TARGET, "an enemy unit is no target of the follow order");
    engine_command command = {};
    command.kind = ENGINE_COMMAND_DESTROY;
    command.unit = world.unit;
    command.target_unit = enemy;
    engine_check check = api->can_issue(e, &command);
    TEST_CHECK(counts, check.refusal == SIM_REFUSAL_NONE && check.days == 10 && check.assumed, "can_issue of 殲滅 gives the days to the hex beside the target");

    u32 mark = api->ledger_count(e);
    engine_command_result result = api->command_destroy(e, world.unit, enemy);
    engine_unit unit = api->unit_at(e, world.unit), target = api->unit_at(e, enemy);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && feature_unit_stands_at(api, world.unit, beside),
               "G2-052: 殲滅 moves the unit next to the target in the same turn");
    TEST_CHECK(counts, target.soldiers < 300 && unit.acted && feature_count_events(api, SIM_EVENT_UNIT_DAMAGED, mark) >= 1,
               "and attacks it in the same turn");
    /* The first attack of the order can use a tactic (run of 2026-10-22). A target that it destroys ends the order. */
    TEST_CHECK(counts, target.id == ENGINE_NO_UNIT ? unit.order == UNIT_ORDER_NONE : (unit.order == UNIT_ORDER_DESTROY && unit.order_target == enemy),
               "the unit keeps the order 殲滅 on the target while the target lives");

    u32 turns = 0;
    while (turns < 12 && api->unit_at(e, enemy).id != ENGINE_NO_UNIT && feature_next_own_turn(api, force)) turns++;
    TEST_CHECK(counts, api->unit_at(e, enemy).id == ENGINE_NO_UNIT, "at each 進行 the unit attacks again, until the target is gone");
    TEST_CHECK(counts, api->unit_at(e, world.unit).id != ENGINE_NO_UNIT && api->unit_at(e, world.unit).order == UNIT_ORDER_NONE,
               "the order ends with the target");
    engine_fixture_destroy(&world.setup.fixture);
}

/* G2-052: the follow order keeps the unit next to an own unit; 委任部隊行動
   runs the step now. */
internal void
feature_follow_order_tests(test_counts *counts)
{
    feature_order_world world;
    if (!feature_order_world_init(&world)) { TEST_CHECK(counts, false, "the order world has a unit on a corridor"); return; }
    engine_api *api = world.setup.api;
    void *e = api->engine;
    u8 force = world.setup.force;
    hex_coord leader_hex = feature_along(world.start, world.d, 5), beside = feature_along(world.start, world.d, 4);
    hex_coord further = feature_along(world.start, world.d, 6);
    u32 leader = feature_sortie_unit(api, world.setup.city, 1);
    TEST_CHECK(counts, leader != ENGINE_NO_UNIT && api->set_unit_place(e, leader, force, leader_hex.q, leader_hex.r), "an own unit stands five hexes away");

    engine_command_result result = api->command_follow(e, world.unit, leader);
    engine_unit unit = api->unit_at(e, world.unit);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && feature_unit_stands_at(api, world.unit, beside) && unit.order == UNIT_ORDER_FOLLOW &&
                       unit.order_target == leader, "the follow order moves the unit next to the leader and stays");
    TEST_CHECK(counts, feature_next_own_turn(api, force), "the next turn of the player starts");
    TEST_CHECK(counts, api->set_unit_place(e, leader, force, further.q, further.r), "the leader moves one hex on");
    u32 mark = api->ledger_count(e);
    TEST_CHECK(counts, api->command_delegate(e).refusal == SIM_REFUSAL_NONE, "委任部隊行動 runs the long orders now");
    TEST_CHECK(counts, feature_unit_stands_at(api, world.unit, leader_hex) && api->unit_at(e, world.unit).order == UNIT_ORDER_FOLLOW,
               "the unit follows to the hex next to the leader and keeps the order");
    TEST_CHECK(counts, feature_count_events(api, SIM_EVENT_UNIT_MOVED, mark) == 1, "the step is one move");
    TEST_CHECK(counts, api->command_delegate(e).refusal == SIM_REFUSAL_NONE && feature_count_events(api, SIM_EVENT_UNIT_MOVED, mark) == 1,
               "a second 委任部隊行動 in the turn moves nothing: the unit moved");
    engine_fixture_destroy(&world.setup.fixture);
}

/* G2-056: a land unit enters a river hex as a 走舸 (manual p.12). */
internal void
feature_river_reach_tests(test_counts *counts)
{
    feature_order_world world;
    if (!feature_order_world_init(&world)) { TEST_CHECK(counts, false, "the order world has a unit on a corridor"); return; }
    engine_api *api = world.setup.api;
    void *e = api->engine;
    hex_coord river = feature_along(world.start, world.d, 1), beyond = feature_along(world.start, world.d, 2);
    api->set_cell_terrain(e, river.q, river.r, feature_terrain_id(api, "river"), 1);
    engine_reach_view reach = api->reachable(e, world.unit);
    u32 river_cost = 0, beyond_cost = 0;
    for (u32 index = 0; index < reach.count; ++index)
    {
        if (reach.cells[index].q == river.q && reach.cells[index].r == river.r)   river_cost  = reach.costs[index];
        if (reach.cells[index].q == beyond.q && reach.cells[index].r == beyond.r) beyond_cost = reach.costs[index];
    }
    TEST_CHECK(counts, river_cost == FEATURE_ORDER_RIVER_COST, "G2-056: the river hex next to the unit is in its range, at the cost of the boat row");
    TEST_CHECK(counts, beyond_cost == FEATURE_ORDER_RIVER_COST + FEATURE_ORDER_GRASS_COST, "the grass beyond the river is in the range too");
    TEST_CHECK(counts, api->command_move(e, world.unit, river.q, river.r).refusal == SIM_REFUSAL_NONE && feature_unit_stands_at(api, world.unit, river),
               "the unit moves onto the river");
    engine_fixture_destroy(&world.setup.fixture);
}

/* G3-020 (observed 2026-10-23): the player orders an attack on an enemy
   facility out of the range. The unit marches at each turn and attacks the
   facility when it is in range, until the facility is gone. */
internal void
feature_facility_order_tests(test_counts *counts)
{
    /* The real map: the save gives the enemy cities their facilities (the placeholder board has none). */
    feature_city_setup setup;
    feature_map_setup(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u32 unit = feature_sortie_unit(api, setup.city, 1);
    /* A built facility of any enemy city. */
    u8 enemy_city = ENGINE_NO_CITY, enemy = ENGINE_NO_FACTION, slot_index = 0;
    engine_facility facility = {};
    facility.facility = ENGINE_NO_FACILITY;
    for (u32 city_index = 0; city_index < api->city_count(e) && facility.facility == ENGINE_NO_FACILITY; ++city_index)
    {
        engine_city city = api->city_at(e, (u8)city_index);
        if (city.owner_force_id == setup.force || city.owner_force_id == ENGINE_NO_FACTION) continue;
        for (u8 index = 0; index < api->city_slots(e, city.city_id).slot_count; ++index)
        {
            engine_facility candidate = api->facility_at(e, city.city_id, index);
            if (candidate.state != SIM_SLOT_BUILT || candidate.facility == ENGINE_NO_FACILITY) continue;
            facility = candidate; slot_index = index; enemy_city = city.city_id; enemy = city.owner_force_id;
            break;
        }
    }
    b32 ready = enemy != ENGINE_NO_FACTION && unit != ENGINE_NO_UNIT && facility.facility != ENGINE_NO_FACILITY;
    hex_coord slot = hex_make(facility.q, facility.r);
    /* The unit stands three hexes from the facility on a line of grass. */
    hex_coord at = slot;
    u32 d = 0;
    for (; ready && d < HEX_DIRECTION_COUNT; ++d)
    {
        hex_coord probe = slot;
        b32 clear = true;
        for (u32 step = 0; step < 3 && clear; ++step)
        {
            probe = hex_neighbor(probe, d);
            engine_hex data = api->hex_at(e, probe.q, probe.r);
            clear = data.on_map && data.city_id == ENGINE_NO_CITY && data.slot_city == ENGINE_NO_CITY && data.unit_id == ENGINE_NO_UNIT;
        }
        if (!clear) continue;
        probe = slot;
        for (u32 step = 0; step < 3; ++step) { probe = hex_neighbor(probe, d); api->set_cell_terrain(e, probe.q, probe.r, feature_terrain_id(api, "grass"), 1); }
        at = probe;
        break;
    }
    ready = ready && d < HEX_DIRECTION_COUNT && api->set_unit_place(e, unit, setup.force, at.q, at.r);
    if (ready) api->set_unit(e, unit, 3000, 80);
    char setup_name[200];
    snprintf(setup_name, sizeof(setup_name), "an enemy facility stands three grass hexes from the unit (enemy %u, city %u, unit %u, facility %u, direction %u, at %d,%d)",
             enemy, enemy_city, unit, facility.facility, d, at.q, at.r);
    TEST_CHECK(counts, ready && facility.durability > 0, setup_name);
    if (!ready) { engine_fixture_destroy(&setup.fixture); return; }
    engine_command probe_command = {};
    probe_command.kind = ENGINE_COMMAND_ATTACK_FACILITY;
    probe_command.unit = unit;
    probe_command.q = slot.q; probe_command.r = slot.r;
    engine_check check = api->can_issue(e, &probe_command);
    TEST_CHECK(counts, check.refusal == SIM_REFUSAL_NONE && check.days >= 10 && check.assumed, "the attack order on a facility is allowed, with the days of the march");
    hex_coord empty = feature_along(slot, d, 1);
    engine_command_result refused = api->command_attack_facility(e, unit, empty.q, empty.r);
    TEST_CHECK(counts, refused.refusal == SIM_REFUSAL_NO_TARGET, "refused: no enemy facility on the hex");
    u32 mark = api->ledger_count(e);
    engine_command_result result = api->command_attack_facility(e, unit, slot.q, slot.r);
    engine_unit after = api->unit_at(e, unit);
    engine_facility hit = api->facility_at(e, enemy_city, slot_index);
    b32 struck = hit.facility == ENGINE_NO_FACILITY || hit.durability < facility.durability;
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && feature_count_events(api, SIM_EVENT_UNIT_ORDERED, mark) == 1 &&
                       (after.order == UNIT_ORDER_ATTACK_FACILITY || struck) && hex_distance(hex_make(after.q, after.r), slot) <= 3,
               "the unit takes the order, marches toward the facility and strikes it when it is in range (G3-020)");
    for (u32 turn = 0; turn < 6 && api->facility_at(e, enemy_city, slot_index).facility != ENGINE_NO_FACILITY; ++turn) feature_next_own_turn(api, setup.force);
    hit = api->facility_at(e, enemy_city, slot_index);
    after = api->unit_at(e, unit);
    TEST_CHECK(counts, (hit.facility == ENGINE_NO_FACILITY && after.order == UNIT_ORDER_NONE) || hit.durability < facility.durability,
               "the order goes on at each turn until the facility falls, and then it ends");
    engine_fixture_destroy(&setup.fixture);
}

internal void
feature_order_tests(test_counts *counts)
{
    feature_far_move_tests(counts);
    feature_destroy_order_tests(counts);
    feature_follow_order_tests(counts);
    feature_river_reach_tests(counts);
    feature_facility_order_tests(counts);
}
