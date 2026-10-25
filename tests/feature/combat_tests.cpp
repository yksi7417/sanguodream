/* Feature tests of M6 combat (military-units-and-tactics.md, plots-and-states.md,
   military-facilities-and-traps.md, siege-and-bases.md; F-082 to F-093),
   through engine_api with the real engine: 攻擊 with the counterattack,
   齊攻, 戰法, 計略, fire, 設置 / 修補 / 撤除, the towers, the zone of control
   of a building, the siege, the fall of a base and the capture of officers.
   The rules are documented; every damage number is assumed (Q-172 to Q-176). */

#define FEATURE_UNIT_TOWER     7
#define FEATURE_STOCK_TOWER    5
#define FEATURE_UNIT_CROSSBOW  3
#define FEATURE_STOCK_CROSSBOW 2
#define FEATURE_TACTIC_PIERCE  0
#define FEATURE_TACTIC_TOWER   14
#define FEATURE_TACTIC_SHIP_ARROW 16   /* 火矢 of boat, tower_ship and battle_ship; grade B */
#define FEATURE_TACTIC_RAM_SHIP   17   /* 猛撞 of tower_ship and battle_ship; grade A */
#define FEATURE_UNIT_TOWER_SHIP   10
#define FEATURE_APTITUDE_A        2    /* 0 C .. 3 S */
#define FEATURE_PLOT_FIRE      0
#define FEATURE_PLOT_CONFUSION 3
#define FEATURE_PLOT_AMBUSH    5
#define FEATURE_BUILD_CAMP     0
#define FEATURE_BUILD_ARROW    3
#define FEATURE_BASE_COUNTER   300   /* combat.base_counter, assumed */
#define FEATURE_TOWER_SHOT     300   /* the value of an arrow tower, assumed */
#define FEATURE_DISMANTLE_AP   10    /* manual p.24 */

/* Four units of the city on a corridor of grass: A at the start, the enemy
   B next to it, the tower T behind B, and C on a side of B. */
struct feature_battle
{
    feature_city_setup setup;
    u32       a, b, c, t;
    hex_coord start;
    u32       d;
    u8        enemy;
};

internal hex_coord
feature_unit_hex(engine_api *api, u32 unit_id)
{
    engine_unit unit = api->unit_at(api->engine, unit_id);
    return hex_make(unit.q, unit.r);
}

internal b32
feature_battle_init(feature_battle *battle)
{
    feature_march_setup(&battle->setup);
    engine_api *api = battle->setup.api;
    void *e = api->engine;
    u8 city = battle->setup.city, force = battle->setup.force;
    api->set_city_resource(e, city, SIM_RESOURCE_WEAPONS, FEATURE_STOCK_TOWER, 5);
    battle->a = feature_sortie_unit(api, city, 1);
    battle->b = feature_sortie_unit(api, city, 1);
    battle->c = feature_sortie_unit(api, city, 1);
    battle->t = feature_sortie_unit(api, city, 1, FEATURE_UNIT_TOWER);
    if (battle->a == ENGINE_NO_UNIT || battle->b == ENGINE_NO_UNIT || battle->c == ENGINE_NO_UNIT || battle->t == ENGINE_NO_UNIT) return false;
    battle->d     = feature_corridor_for_unit(api, battle->a, &battle->start);
    if (battle->d >= HEX_DIRECTION_COUNT) return false;
    battle->enemy = (u8)(force + 1);
    hex_coord first = feature_along(battle->start, battle->d, 1);
    hex_coord side  = hex_neighbor(first, (battle->d + 1) % HEX_DIRECTION_COUNT);
    api->set_cell_terrain(e, side.q, side.r, feature_terrain_id(api, "grass"), 1);
    /* The tower stands 2 hexes from the enemy: an arrow tactic needs a target at 2 hexes (the player, 2026-10-24). */
    hex_coord third = feature_along(battle->start, battle->d, 3);
    api->set_cell_terrain(e, third.q, third.r, feature_terrain_id(api, "grass"), 1);
    b32 placed = api->set_unit_place(e, battle->b, battle->enemy, first.q, first.r) &&
                 api->set_unit_place(e, battle->t, force, third.q, third.r) &&
                 api->set_unit_place(e, battle->c, force, side.q, side.r);
    api->set_unit(e, battle->a, 1000, 80);
    api->set_unit(e, battle->b, 1000, 80);
    api->set_unit(e, battle->c, 1000, 80);
    api->set_unit(e, battle->t, 1000, 80);
    return placed;
}

internal void
feature_combat_table_tests(test_counts *counts, engine_api *api)
{
    void *e = api->engine;
    engine_content_status status = api->content_status(e);
    TEST_CHECK(counts, api->tactic_kind_count(e) == 19 && status.tactic_count == 19 && api->plot_kind_count(e) == 9 && status.plot_count == 9,
               "tactics.tsv has the 19 tactics and plots.tsv the 9 battle plots of the manual");
    TEST_CHECK(counts, api->building_kind_count(e) == 20 && status.critical_count == 21, "buildings.tsv has 18 buildings and 2 natural traps; 21 critical skills");
    engine_tactic_kind tower = api->tactic_kind_at(e, FEATURE_TACTIC_TOWER), pierce = api->tactic_kind_at(e, FEATURE_TACTIC_PIERCE);
    TEST_CHECK(counts, tower.always_success && tower.range_max == 2 && pierce.aptitude == 1 && pierce.morale == 15 && pierce.effect == TACTIC_KNOCK,
               "the tower arrow always hits; 突刺 needs aptitude B and 15 morale and knocks the target back");
    /* The player, 2026-10-24: no arrow and no stone at an adjacent hex. */
    const char *far_tactics[] = { "fire_arrow", "pierce_arrow", "volley", "tower_arrow", "stone", "ship_arrow", "ship_stone" };
    u32 far_count = 0;
    for (u32 row = 0; row < api->tactic_kind_count(e); ++row)
    {
        engine_tactic_kind kind = api->tactic_kind_at(e, row);
        for (u32 index = 0; index < ArrayCount(far_tactics); ++index)
        {
            if (strcmp(kind.key, far_tactics[index]) == 0 && kind.range_min == 2) ++far_count;
        }
    }
    TEST_CHECK(counts, far_count == ArrayCount(far_tactics), "the arrow tactics and the stone tactics need a target at 2 hexes or more");
    u32 natural = 0;
    for (u32 id = 0; id < api->building_count(e); ++id)
    {
        engine_building building = api->building_at(e, id);
        if (building.id != ENGINE_NO_BUILDING && api->building_kind_at(e, building.kind).natural && building.force_id == ENGINE_NO_FACTION) ++natural;
    }
    TEST_CHECK(counts, natural >= 4, "the placeholder map has dikes and rockfalls (placeholder, Q-034)");
}

internal void
feature_attack_tests(test_counts *counts, feature_battle *battle)
{
    engine_api *api = battle->setup.api;
    void *e = api->engine;
    hex_coord at_b = feature_unit_hex(api, battle->b);
    engine_command attack = {};
    attack.kind = ENGINE_COMMAND_ATTACK;
    attack.unit = battle->a;
    attack.q = at_b.q; attack.r = at_b.r;
    engine_check check = api->can_issue(e, &attack);
    TEST_CHECK(counts, check.refusal == SIM_REFUSAL_NONE && check.quantity > 0 && check.value_before == 1000 && check.assumed,
               "attack: can_issue gives the expected damage and the soldiers of the target (assumed rule)");
    hex_coord far_hex = feature_along(battle->start, battle->d, 3);
    TEST_CHECK(counts, api->command_attack(e, battle->a, far_hex.q, far_hex.r).refusal == SIM_REFUSAL_NO_TARGET, "refused: no enemy in range");

    u32 mark = api->ledger_count(e);
    engine_command_result result = api->command_attack(e, battle->a, at_b.q, at_b.r);
    engine_unit a = api->unit_at(e, battle->a), b = api->unit_at(e, battle->b);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && b.soldiers < 1000 && 1000 - b.soldiers >= check.quantity && a.acted,
               "the target loses at least the expected soldiers (more with a critical hit); the attacker acted");
    TEST_CHECK(counts, feature_count_events(api, SIM_EVENT_COUNTERATTACK, mark) == 1 && a.soldiers < 1000,
               "a pike unit answers a melee attack with a counterattack");
    TEST_CHECK(counts, b.morale < 80 && feature_count_events(api, SIM_EVENT_MORALE_CHANGED, mark) >= 1, "the target loses morale (assumed 5)");
    TEST_CHECK(counts, api->command_attack(e, battle->a, at_b.q, at_b.r).refusal == SIM_REFUSAL_UNIT_ACTED, "refused: one action in each turn");

    /* 齊攻 needs two units that can attack the hex. */
    engine_command joint = attack;
    joint.kind = ENGINE_COMMAND_JOINT_ATTACK;
    joint.unit = battle->c;
    TEST_CHECK(counts, api->can_issue(e, &joint).refusal == SIM_REFUSAL_NO_PARTNER, "refused: 齊攻 with no partner (the other unit acted)");
    TEST_CHECK(counts, api->command_attack(e, battle->t, at_b.q, at_b.r).refusal == SIM_REFUSAL_NO_APTITUDE, "refused: a siege weapon cannot attack");
}

internal void
feature_tactic_tests(test_counts *counts, feature_battle *battle)
{
    engine_api *api = battle->setup.api;
    void *e = api->engine;
    hex_coord at_b = feature_unit_hex(api, battle->b);
    engine_command tactic = {};
    tactic.kind = ENGINE_COMMAND_TACTIC;
    tactic.unit = battle->t;
    tactic.row  = FEATURE_TACTIC_TOWER;
    tactic.q = at_b.q; tactic.r = at_b.r;
    engine_check check = api->can_issue(e, &tactic);
    TEST_CHECK(counts, check.refusal == SIM_REFUSAL_NONE && check.quantity == 100 && check.action_points == 10,
               "戰法: can_issue gives the chance (100 for the tower) and the morale cost");
    TEST_CHECK(counts, api->command_tactic(e, battle->t, FEATURE_TACTIC_PIERCE, at_b.q, at_b.r).refusal == SIM_REFUSAL_NO_APTITUDE,
               "refused: a tower cannot use 突刺");
    hex_coord far_place = feature_unit_hex(api, battle->t), near_place = feature_along(battle->start, battle->d, 2);
    api->set_unit_place(e, battle->t, battle->setup.force, near_place.q, near_place.r);
    TEST_CHECK(counts, api->can_issue(e, &tactic).refusal == SIM_REFUSAL_NO_TARGET,
               "refused: the tower arrow on an adjacent enemy (the player, 2026-10-24: an arrow needs 2 hexes)");
    api->set_unit_place(e, battle->t, battle->setup.force, far_place.q, far_place.r);
    u32 soldiers = api->unit_at(e, battle->b).soldiers;
    u32 mark = api->ledger_count(e);
    engine_command_result result = api->command_tactic(e, battle->t, FEATURE_TACTIC_TOWER, at_b.q, at_b.r);
    sim_event used = {};
    engine_fire_view fire = api->fire_view(e);
    hex_offset o = hex_to_offset(at_b);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && feature_find_event(api, SIM_EVENT_TACTIC_USED, mark, &used) && used.delta == 1 &&
                       api->unit_at(e, battle->b).soldiers < soldiers && api->unit_at(e, battle->t).morale == 70,
               "the tower arrow hits, the target loses soldiers and the tower pays 10 morale");
    TEST_CHECK(counts, feature_count_events(api, SIM_EVENT_FIRE_STARTED, mark) >= 1 && fire.turns[(u32)o.row * fire.width + (u32)o.col] > 0 &&
                       feature_count_events(api, SIM_EVENT_COUNTERATTACK, mark) == 0,
               "a fire arrow sets the hex on fire; no counterattack answers a tactic");
}

internal void
feature_turn_two_tests(test_counts *counts, feature_battle *battle)
{
    engine_api *api = battle->setup.api;
    void *e = api->engine;
    u8 force = battle->setup.force;
    u32 before = api->unit_at(e, battle->b).soldiers;
    u32 mark = api->ledger_count(e);
    feature_next_own_turn(api, force);
    engine_unit b = api->unit_at(e, battle->b);
    TEST_CHECK(counts, b.soldiers == before - before * 10 / 100 && feature_count_events(api, SIM_EVENT_UNIT_DAMAGED, mark) >= 1,
               "a unit on a burning hex loses 10 percent of its soldiers each turn (assumed)");

    hex_coord at_b = hex_make(b.q, b.r);
    /* G2-045: the command menu opens at the destination before the move, so
       the engine checks a unit command as if the unit stood there. The
       query moves nothing. */
    {
        engine_command probe = {};
        probe.kind = ENGINE_COMMAND_ATTACK;
        probe.unit = battle->c;
        probe.q = at_b.q; probe.r = at_b.r;
        hex_coord far_hex = feature_along(battle->start, battle->d, 3);   /* two hexes from the target */
        hex_coord at_c = feature_unit_hex(api, battle->c);
        TEST_CHECK(counts, api->can_issue(e, &probe).refusal == SIM_REFUSAL_NONE &&
                           api->can_issue_after_move(e, &probe, far_hex.q, far_hex.r).refusal == SIM_REFUSAL_NO_TARGET &&
                           api->can_issue_after_move(e, &probe, at_c.q, at_c.r).refusal == SIM_REFUSAL_NONE &&
                           hex_equal(feature_unit_hex(api, battle->c), at_c),
                   "can_issue_after_move checks the attack from the destination, and the unit stays in place (G2-045)");
    }
    /* A crossbow unit two hexes from the target can attack it, but 齊攻 takes
       only the units next to the target (military-units-and-tactics.md). The
       crossbow unit keeps its turn. */
    hex_coord second   = feature_along(battle->start, battle->d, 2);
    hex_coord far_side = hex_neighbor(second, (battle->d + 1) % HEX_DIRECTION_COUNT);
    api->set_cell_terrain(e, far_side.q, far_side.r, feature_terrain_id(api, "grass"), 1);
    api->set_city_resource(e, battle->setup.city, SIM_RESOURCE_WEAPONS, FEATURE_STOCK_CROSSBOW, 2000);   /* one crossbow for each soldier */
    /* The corridor made every hex near the city a cliff. The sortie needs one grass hex next to the city. */
    hex_coord city_hex = battle->start;
    for (s32 dq = -3; dq <= 3; ++dq)
    {
        for (s32 dr = -3; dr <= 3; ++dr)
        {
            hex_coord hex = hex_make(battle->start.q + dq, battle->start.r + dr);
            if (api->hex_at(e, hex.q, hex.r).city_id == battle->setup.city) city_hex = hex;
        }
    }
    for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d)
    {
        hex_coord hex = hex_neighbor(city_hex, d);
        if (api->unit_at_hex(e, hex.q, hex.r) != ENGINE_NO_UNIT || api->hex_at(e, hex.q, hex.r).city_id != ENGINE_NO_CITY) continue;
        api->set_cell_terrain(e, hex.q, hex.r, feature_terrain_id(api, "grass"), 1);
        break;
    }
    engine_sortie_order bow_order = feature_pike_order(api, battle->setup.city, 1);
    bow_order.unit_type = FEATURE_UNIT_CROSSBOW;
    engine_command_result bow_result = api->command_sortie(e, &bow_order);
    u32 bow = ENGINE_NO_UNIT;
    for (u32 id = 0; id < api->unit_count(e); ++id)
    {
        engine_unit candidate = api->unit_at(e, id);
        if (candidate.id != ENGINE_NO_UNIT && candidate.officers[0] == bow_order.officers[0]) bow = id;
    }
    b32 bow_placed = bow != ENGINE_NO_UNIT && api->set_unit_place(e, bow, force, far_side.q, far_side.r);
    if (bow_placed) api->set_unit(e, bow, 1000, 80);
    engine_command bow_attack = {};
    bow_attack.kind = ENGINE_COMMAND_ATTACK;
    bow_attack.unit = bow;
    bow_attack.q = at_b.q; bow_attack.r = at_b.r;
    char bow_name[200];
    snprintf(bow_name, sizeof(bow_name), "a crossbow unit two hexes from the target can attack it (sortie refusal %u, officer %u, unit %u, placed %d, distance %d, refusal %u)",
             bow_result.refusal, bow_order.officers[0], bow, bow_placed, hex_distance(far_side, at_b), api->can_issue(e, &bow_attack).refusal);
    TEST_CHECK(counts, bow_placed && hex_distance(far_side, at_b) == 2 && api->can_issue(e, &bow_attack).refusal == SIM_REFUSAL_NONE, bow_name);
    engine_command joint = {};
    joint.kind = ENGINE_COMMAND_JOINT_ATTACK;
    joint.unit = battle->a;
    joint.q = at_b.q; joint.r = at_b.r;
    engine_check check = api->can_issue(e, &joint);
    TEST_CHECK(counts, check.refusal == SIM_REFUSAL_NONE && check.quantity == 2, "齊攻: the two pike units next to the target join (the tower and the far crossbow cannot)");
    mark = api->ledger_count(e);
    engine_command_result result = api->command_joint_attack(e, battle->a, at_b.q, at_b.r);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && feature_count_events(api, SIM_EVENT_UNIT_DAMAGED, mark) >= 2 &&
                       feature_count_events(api, SIM_EVENT_COUNTERATTACK, mark) == 0 && api->unit_at(e, battle->a).acted && api->unit_at(e, battle->c).acted,
               "each partner strikes; the target answers none of them (assumed); both units acted");
    TEST_CHECK(counts, bow_placed && !api->unit_at(e, bow).acted, "the crossbow unit two hexes away did not join and keeps its turn");

    /* 計略 from the tower. */
    hex_coord at_c = feature_unit_hex(api, battle->c);
    hex_coord empty = feature_along(battle->start, battle->d, 3);
    TEST_CHECK(counts, api->command_plot(e, battle->t, FEATURE_PLOT_CONFUSION, at_c.q, at_c.r).refusal == SIM_REFUSAL_NO_TARGET,
               "refused: 擾亂 on an own unit");
    TEST_CHECK(counts, api->command_plot(e, battle->t, FEATURE_PLOT_AMBUSH, at_b.q, at_b.r).refusal == SIM_REFUSAL_NO_APTITUDE,
               "refused: a siege weapon cannot lay an ambush");
    api->set_unit(e, battle->t, api->unit_at(e, battle->t).soldiers, 5);
    TEST_CHECK(counts, api->command_plot(e, battle->t, FEATURE_PLOT_FIRE, empty.q, empty.r).refusal == SIM_REFUSAL_NO_MORALE, "refused: 5 morale for 火計");
    api->set_unit(e, battle->t, api->unit_at(e, battle->t).soldiers, 80);
    engine_command plot = {};
    plot.kind = ENGINE_COMMAND_PLOT;
    plot.unit = battle->t;
    plot.row  = FEATURE_PLOT_FIRE;
    plot.q = empty.q; plot.r = empty.r;
    check = api->can_issue(e, &plot);
    mark = api->ledger_count(e);
    result = api->command_plot(e, battle->t, FEATURE_PLOT_FIRE, empty.q, empty.r);
    engine_fire_view fire = api->fire_view(e);
    hex_offset o = hex_to_offset(empty);
    TEST_CHECK(counts, check.refusal == SIM_REFUSAL_NONE && check.quantity == 100 && result.refusal == SIM_REFUSAL_NONE &&
                       feature_count_events(api, SIM_EVENT_PLOT_USED, mark) == 1 && fire.turns[(u32)o.row * fire.width + (u32)o.col] >= 1 &&
                       fire.turns[(u32)o.row * fire.width + (u32)o.col] <= 7,
               "火計 on an empty hex always works; the fire lasts 1 to 7 turns (terrain-and-map.md, F-149)");
    feature_next_own_turn(api, force);
}

/* A hex of the map 4 hexes or more from every city, unit and building,
   with grass on it and on the hexes within 2. */
internal hex_coord
feature_open_ground(engine_api *api)
{
    void *e = api->engine;
    engine_map_size size = api->map_size(e);
    u8 grass = feature_terrain_id(api, "grass");
    for (u32 row = 3; row + 3 < size.height; ++row)
    {
        for (u32 col = 3; col + 3 < size.width; ++col)
        {
            hex_offset offset = {};
            offset.col = (s32)col;
            offset.row = (s32)row;
            hex_coord hex = hex_from_offset(offset);
            b32 ok = true;
            for (u32 index = 0; index < api->city_count(e) && ok; ++index)
            {
                engine_city_slots slots = api->city_slots(e, api->city_at(e, index).city_id);
                if (hex_distance(hex, hex_make(slots.q, slots.r)) < 4) ok = false;
            }
            for (u32 id = 0; id < api->unit_count(e) && ok; ++id)
            {
                engine_unit unit = api->unit_at(e, id);
                if (unit.id != ENGINE_NO_UNIT && hex_distance(hex, hex_make(unit.q, unit.r)) < 4) ok = false;
            }
            for (u32 id = 0; id < api->building_count(e) && ok; ++id)
            {
                engine_building building = api->building_at(e, id);
                if (building.id != ENGINE_NO_BUILDING && hex_distance(hex, hex_make(building.q, building.r)) < 4) ok = false;
            }
            if (!ok) continue;
            for (s32 dq = -2; dq <= 2; ++dq)
            {
                for (s32 dr = -2; dr <= 2; ++dr)
                {
                    hex_coord cell = hex_make(hex.q + dq, hex.r + dr);
                    if (hex_distance(cell, hex) <= 2) api->set_cell_terrain(e, cell.q, cell.r, grass, 1);
                }
            }
            return hex;
        }
    }
    return hex_make(0, 0);
}

internal void
feature_building_tests(test_counts *counts, feature_battle *battle)
{
    engine_api *api = battle->setup.api;
    void *e = api->engine;
    u8 force = battle->setup.force;
    hex_coord fourth = feature_open_ground(api), fifth = hex_neighbor(fourth, 0);
    hex_coord sixth = hex_neighbor(hex_neighbor(fourth, 3), 3);
    TEST_CHECK(counts, api->set_unit_place(e, battle->a, force, fourth.q, fourth.r), "the editor puts a unit far from the city");
    TEST_CHECK(counts, api->command_build(e, battle->a, FEATURE_BUILD_ARROW, fifth.q, fifth.r).refusal == SIM_REFUSAL_NO_GOLD,
               "refused: an arrow tower costs 600 gold and the unit carries 500");
    engine_command build = {};
    build.kind = ENGINE_COMMAND_BUILD;
    build.unit = battle->a;
    build.row  = FEATURE_BUILD_CAMP;
    build.q = fifth.q; build.r = fifth.r;
    engine_check check = api->can_issue(e, &build);
    u32 mark = api->ledger_count(e);
    engine_command_result result = api->command_build(e, battle->a, FEATURE_BUILD_CAMP, fifth.q, fifth.r);
    u32 camp_id = api->building_at_hex(e, fifth.q, fifth.r);
    engine_building camp = api->building_at(e, camp_id);
    TEST_CHECK(counts, check.refusal == SIM_REFUSAL_NONE && check.gold == 500 && check.quantity > 0 && result.refusal == SIM_REFUSAL_NONE &&
                       camp.id != ENGINE_NO_BUILDING && camp.force_id == force && camp.durability == check.quantity && camp.durability_max == 1100 &&
                       api->unit_at(e, battle->a).gold == 0 && feature_count_events(api, SIM_EVENT_BUILDING_STARTED, mark) == 1,
               "設置: the unit pays the gold and builds the camp by its build value (assumed x 5)");
    /* The player, 2026-10-24: a building event named the commander of the unit with the index of the building. */
    engine_event_report started = {};
    for (u32 index = mark; index < api->ledger_count(e); ++index)
    {
        if (api->ledger_at(e, index).kind == SIM_EVENT_BUILDING_STARTED) started = api->event_report(e, index);
    }
    TEST_CHECK(counts, started.subject.kind == ENGINE_SUBJECT_NONE && started.faction == force && started.place.kind == ENGINE_PLACE_BUILDING &&
                       started.place.id == camp_id && started.place.thing == camp.kind && started.place.force == force &&
                       started.place.on_map && started.place.q == fifth.q && started.place.r == fifth.r,
               "a building event has the owner as its faction, no unit, and the building at its hex as its place");

    /* An enemy arrow tower shoots the nearest unit in its range each turn. */
    u32 tower = api->place_building(e, FEATURE_BUILD_ARROW, battle->enemy, sixth.q, sixth.r);
    u32 before = api->unit_at(e, battle->a).soldiers;
    mark = api->ledger_count(e);
    feature_next_own_turn(api, force);
    TEST_CHECK(counts, tower != ENGINE_NO_BUILDING && api->unit_at(e, battle->a).soldiers == before - FEATURE_TOWER_SHOT,
               "an enemy arrow tower takes 300 soldiers each turn (assumed)");
    engine_place shot = {};
    for (u32 index = mark; index < api->ledger_count(e); ++index)
    {
        sim_event event = api->ledger_at(e, index);
        if (event.kind == SIM_EVENT_UNIT_DAMAGED && event.detail == battle->a) shot = api->event_report(e, index).place;
    }
    TEST_CHECK(counts, shot.kind == ENGINE_PLACE_UNIT && shot.id == battle->a && shot.officer == api->unit_at(e, battle->a).officers[0] &&
                       shot.on_map && shot.q == fourth.q && shot.r == fourth.r,
               "the place of a unit event is the unit at its hex, with its commander");

    if (!api->building_at(e, camp_id).complete)
    {
        u32 durability = api->building_at(e, camp_id).durability;
        TEST_CHECK(counts, api->command_repair(e, battle->a, fifth.q, fifth.r).refusal == SIM_REFUSAL_NONE &&
                           api->building_at(e, camp_id).durability > durability,
                   "修補 adds durability to an own building that is not complete");
        feature_next_own_turn(api, force);
    }
    u32 points = api->actions_left(e, force);
    mark = api->ledger_count(e);
    TEST_CHECK(counts, api->command_dismantle(e, battle->a, fifth.q, fifth.r).refusal == SIM_REFUSAL_NONE &&
                       api->building_at_hex(e, fifth.q, fifth.r) == ENGINE_NO_BUILDING && api->actions_left(e, force) == points - FEATURE_DISMANTLE_AP &&
                       feature_count_events(api, SIM_EVENT_BUILDING_DESTROYED, mark) == 1,
               "撤除 removes an own building for 10 action points");
    TEST_CHECK(counts, api->command_dismantle(e, battle->a, sixth.q, sixth.r).refusal == SIM_REFUSAL_UNIT_ACTED,
               "refused: one action in each turn");
}

internal void
feature_capture_tests(test_counts *counts, feature_battle *battle)
{
    engine_api *api = battle->setup.api;
    void *e = api->engine;
    hex_coord at_b = feature_unit_hex(api, battle->b);
    api->set_unit(e, battle->b, 10, 50);
    engine_unit b = api->unit_at(e, battle->b);
    u32 officers = 0;
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place) officers += b.officers[place] != ENGINE_NO_OFFICER;
    engine_unit c_before = api->unit_at(e, battle->c);
    u32 mark = api->ledger_count(e);
    engine_command_result result = api->command_attack(e, battle->c, at_b.q, at_b.r);
    u32 fates = feature_count_events(api, SIM_EVENT_OFFICER_CAPTURED, mark) + feature_count_events(api, SIM_EVENT_OFFICER_ESCAPED, mark);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && api->unit_at(e, battle->b).id == ENGINE_NO_UNIT &&
                       feature_count_events(api, SIM_EVENT_UNIT_DESTROYED, mark) == 1 && fates == officers && officers > 0,
               "a unit with no soldiers is destroyed; each officer is captured or escapes (assumed 20 percent)");
    TEST_CHECK(counts, feature_count_events(api, SIM_EVENT_TECHNIQUE_GAINED, mark) >= 1, "the victor gains technique points (assumed)");
    /* G3-003 (observed 2026-10-23): the attacker takes half the gold and a quarter of the grain of the destroyed unit. */
    engine_unit c_after = api->unit_at(e, battle->c);
    TEST_CHECK(counts, b.gold > 0 && b.grain > 0 && c_after.gold == c_before.gold + b.gold / 2 && c_after.grain == c_before.grain + b.grain / 4 &&
                       feature_count_events(api, SIM_EVENT_UNIT_LOOT, mark) == 1 && feature_count_events(api, SIM_EVENT_UNIT_PLUNDER_GRAIN, mark) == 1,
               "the attacker plunders half the gold and a quarter of the grain of the destroyed unit, with two log lines (G3-003)");
}

internal void
feature_battle_tests(test_counts *counts)
{
    feature_battle battle;
    b32 ready = feature_battle_init(&battle);
    TEST_CHECK(counts, ready, "four units stand on a corridor of grass, one of them an enemy");
    if (ready)
    {
        feature_combat_table_tests(counts, battle.setup.api);
        feature_attack_tests(counts, &battle);
        feature_tactic_tests(counts, &battle);
        feature_turn_two_tests(counts, &battle);
        feature_building_tests(counts, &battle);
        feature_capture_tests(counts, &battle);
    }
    engine_fixture_destroy(&battle.setup.fixture);
}

/* The attack on a base: soldiers and durability fall, the walls answer, and
   the base falls when its soldiers end. */
internal void
feature_siege_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_march_setup(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 city = setup.city, force = setup.force, enemy = (u8)(force + 1);
    u32 unit_id = feature_sortie_unit(api, city, 1);
    u8 target = ENGINE_NO_CITY;
    for (u32 index = 0; index < api->city_count(e) && target == ENGINE_NO_CITY; ++index)
    {
        engine_city other = api->city_at(e, index);
        if (other.city_id != city && hex_distance(hex_make(api->city_slots(e, other.city_id).q, api->city_slots(e, other.city_id).r),
                                                  feature_unit_hex(api, unit_id)) > 4) target = other.city_id;
    }
    api->set_city_owner(e, target, enemy);
    api->set_city_resource(e, target, SIM_RESOURCE_SOLDIERS, 0, 5000);
    engine_city_slots site = api->city_slots(e, target);
    hex_coord next = hex_neighbor(hex_make(site.q, site.r), 0);
    for (u32 d = 0; d < HEX_DIRECTION_COUNT && !api->set_unit_place(e, unit_id, force, next.q, next.r); ++d) next = hex_neighbor(hex_make(site.q, site.r), d);
    api->set_unit(e, unit_id, 1000, 80);
    engine_city_siege siege = api->city_siege(e, target);
    /* G2-009 (Q-222): the maximum of the wall is the static city constant of the save, and the wall never goes above it. */
    b32 maxima_hold = true;
    for (u32 id = 0; id < api->city_count(e); ++id)
    {
        engine_city each = api->city_at(e, (u8)id);
        engine_city_siege wall = api->city_siege(e, (u8)id);
        if (each.city_id == ENGINE_NO_CITY || each.owner_force_id == ENGINE_NO_FACTION) continue;
        maxima_hold &= (!each.wall_maximum || wall.durability_max >= each.wall_maximum) && wall.durability <= wall.durability_max;
    }
    TEST_CHECK(counts, maxima_hold && api->city_at(e, target).wall_maximum != 0 && siege.durability_max == api->city_at(e, target).wall_maximum,
               "the maximum of the wall is the u16 at +82 of the save (G2-009, G3-011, Q-222: four cities), and no wall is above it");
    TEST_CHECK(counts, unit_id != ENGINE_NO_UNIT && siege.city_id == target && siege.durability == siege.durability_max && siege.durability_max > 0,
               "a base starts with its full durability");

    u32 mark = api->ledger_count(e);
    engine_command_result result = api->command_attack(e, unit_id, site.q, site.r);
    engine_city_siege after = api->city_siege(e, target);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && after.soldiers < 5000 && after.durability < siege.durability &&
                       feature_count_events(api, SIM_EVENT_BASE_SOLDIERS_LOST, mark) == 1 && feature_count_events(api, SIM_EVENT_BASE_DAMAGED, mark) == 1,
               "an attack on a base takes soldiers and durability");
    TEST_CHECK(counts, api->unit_at(e, unit_id).soldiers == 1000 - FEATURE_BASE_COUNTER, "the base answers a melee attack (assumed 300 soldiers)");
    TEST_CHECK(counts, feature_ledger_sum(api, target, SIM_RESOURCE_DURABILITY, mark) == (s32)after.durability - (s32)siege.durability,
               "the ledger of the city explains the durability");

    feature_next_own_turn(api, force);
    TEST_CHECK(counts, api->city_siege(e, target).durability == after.durability, "a base with an enemy next to it does not recover");
    api->set_city_resource(e, target, SIM_RESOURCE_SOLDIERS, 0, 1);
    /* G2-059: the target is the last city of its force: the force falls with the capture. */
    u8 loser = api->city_at(e, target).owner_force_id;
    for (u32 index = 0; index < api->city_count(e); ++index)
    {
        engine_city other_city = api->city_at(e, index);
        if (other_city.owner_force_id == loser && other_city.city_id != target) api->set_city_owner(e, other_city.city_id, force);
    }
    mark = api->ledger_count(e);
    result = api->command_attack(e, unit_id, site.q, site.r);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && feature_count_events(api, SIM_EVENT_BASE_CAPTURED, mark) == 1 && api->unit_at(e, unit_id).id == ENGINE_NO_UNIT,
               "the base falls when its soldiers end; the unit enters it");
    sim_event fell = {};
    TEST_CHECK(counts, feature_find_event(api, SIM_EVENT_FACTION_FELL, mark, &fell) && fell.faction == loser && fell.detail == force,
               "the force that loses its last city falls, and the ledger names the force that took it (G2-059)");
    b32 owned = false;
    for (u32 index = 0; index < api->city_count(e); ++index)
    {
        if (api->city_at(e, index).city_id == target) owned = api->city_at(e, index).owner_force_id == force;
    }
    TEST_CHECK(counts, owned && api->city_siege(e, target).durability == siege.durability_max / 2, "the attacker owns the base, at half durability (assumed)");
    engine_fixture_destroy(&setup.fixture);
}

/* A land unit on a river becomes a 走舸 (manual p.12): it can enter the
   water at the navy cost. */
internal void
feature_water_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_march_setup(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u32 unit_id = feature_sortie_unit(api, setup.city, 1);
    hex_coord start = {};
    u32 d = feature_corridor_for_unit(api, unit_id, &start);
    if (d >= HEX_DIRECTION_COUNT) { TEST_CHECK(counts, false, "a corridor for the water test"); engine_fixture_destroy(&setup.fixture); return; }
    u8 river = feature_terrain_id(api, "river");
    hex_coord first = feature_along(start, d, 1);
    api->set_cell_terrain(e, first.q, first.r, river, 0);
    u32 cost = 0;
    TEST_CHECK(counts, feature_reaches(api, unit_id, first, &cost) && cost == api->terrain_kind_at(e, river).cost[MOVEMENT_NAVY],
               "a pike unit enters a river at the navy cost: it is a boat on the water");
    engine_fixture_destroy(&setup.fixture);
}

/* U-003 (play report 2026-10-24): a land unit on a 樓船 on water fights as
   the ship. unit_at gives that class as effective_type, and the unit can
   use 火矢 and 猛撞 of the navy against an enemy ship. */
internal void
feature_ship_tactic_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_march_setup(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 city = setup.city, force = setup.force;
    engine_unit_type tower_ship = api->unit_type_at(e, FEATURE_UNIT_TOWER_SHIP);
    api->set_city_resource(e, city, SIM_RESOURCE_WEAPONS, api->weapon_kind_at(e, tower_ship.weapon).stock, 2);
    engine_sortie_order order = feature_pike_order(api, city, 1);
    order.ship = FEATURE_UNIT_TOWER_SHIP;
    engine_officer commander = api->officer_by_id(e, order.officers[0]);
    engine_officer_detail detail = api->officer_detail(e, order.officers[0]);
    engine_officer_edit edit = engine_officer_edit_of(&detail, &commander);
    edit.set_aptitudes = true;
    for (u32 index = 0; index < OFFICER_UNIT_APTITUDE_COUNT; ++index) edit.aptitudes[index] = FEATURE_APTITUDE_A;
    api->set_officer(e, order.officers[0], &edit);
    TEST_CHECK(counts, api->command_sortie(e, &order).refusal == SIM_REFUSAL_NONE, "a pike unit with a 樓船 marches out");
    u32 unit_id = ENGINE_NO_UNIT;
    for (u32 id = 0; id < api->unit_count(e); ++id)
    {
        if (api->unit_at(e, id).officers[0] == order.officers[0]) unit_id = id;
    }
    u32 enemy = feature_sortie_unit(api, city, 1);   /* before the corridor: a sortie takes a free hex next to the city */
    hex_coord start = {};
    u32 d = feature_corridor_for_unit(api, unit_id, &start);
    if (unit_id == ENGINE_NO_UNIT || d >= HEX_DIRECTION_COUNT) { TEST_CHECK(counts, false, "a corridor for the ship test"); engine_fixture_destroy(&setup.fixture); return; }
    TEST_CHECK(counts, api->unit_at(e, unit_id).effective_type == FEATURE_UNIT_PIKE, "on land the unit fights as its pike class");

    u8 river = feature_terrain_id(api, "river");
    hex_coord first = feature_along(start, d, 1), second = feature_along(start, d, 2);
    api->set_cell_terrain(e, first.q, first.r, river, 0);
    api->set_cell_terrain(e, second.q, second.r, river, 0);
    b32 placed = enemy != ENGINE_NO_UNIT && api->set_unit_place(e, unit_id, force, first.q, first.r) &&
                 api->set_unit_place(e, enemy, (u8)(force + 1), second.q, second.r);
    api->set_unit(e, unit_id, 1000, 80);
    api->set_unit(e, enemy, 1000, 80);
    TEST_CHECK(counts, placed, "the unit and an enemy stand on two hexes of the river");
    TEST_CHECK(counts, api->unit_at(e, unit_id).effective_type == FEATURE_UNIT_TOWER_SHIP,
               "on the river the unit fights as its 樓船: unit_at gives the ship class");

    engine_command tactic = {};
    tactic.kind = ENGINE_COMMAND_TACTIC;
    tactic.unit = unit_id;
    tactic.q = second.q; tactic.r = second.r;
    tactic.row = FEATURE_TACTIC_SHIP_ARROW;
    TEST_CHECK(counts, api->can_issue(e, &tactic).refusal == SIM_REFUSAL_NO_TARGET,
               "refused: 火矢 of a ship on an adjacent ship (the player, 2026-10-24: an arrow needs 2 hexes)");
    hex_coord third = feature_along(start, d, 3);
    api->set_cell_terrain(e, third.q, third.r, river, 0);
    api->set_unit_place(e, enemy, (u8)(force + 1), third.q, third.r);
    engine_command far_arrow = tactic;
    far_arrow.q = third.q; far_arrow.r = third.r;
    TEST_CHECK(counts, api->can_issue(e, &far_arrow).refusal == SIM_REFUSAL_NONE, "a unit on a 樓船 can use 火矢 of the navy against an enemy ship 2 hexes away");
    api->set_unit_place(e, enemy, (u8)(force + 1), second.q, second.r);
    tactic.row = FEATURE_TACTIC_RAM_SHIP;
    TEST_CHECK(counts, api->can_issue(e, &tactic).refusal == SIM_REFUSAL_NONE, "a unit on a 樓船 can use 猛撞 against an enemy ship");
    tactic.row = FEATURE_TACTIC_PIERCE;
    TEST_CHECK(counts, api->can_issue(e, &tactic).refusal == SIM_REFUSAL_NO_APTITUDE, "refused: a unit on a 樓船 cannot use 突刺 of its pike class");
    /* 猛撞 is not always a success (tactics.tsv), so the test checks the command and not the damage. */
    u32 mark = api->ledger_count(e);
    engine_command_result result = api->command_tactic(e, unit_id, FEATURE_TACTIC_RAM_SHIP, second.q, second.r);
    sim_event used = {};
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && api->unit_at(e, unit_id).morale == 70 && feature_find_event(api, SIM_EVENT_TACTIC_USED, mark, &used),
               "the engine accepts 猛撞 from the 樓船: the unit pays 10 morale and the ledger has the tactic");
    engine_fixture_destroy(&setup.fixture);
}

internal void
feature_combat_tests(test_counts *counts)
{
    feature_battle_tests(counts);
    feature_siege_tests(counts);
    feature_water_tests(counts);
    feature_ship_tactic_tests(counts);
}
