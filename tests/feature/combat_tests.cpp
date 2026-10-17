/* Feature tests of M6 combat (military-units-and-tactics.md, plots-and-states.md,
   military-facilities-and-traps.md, siege-and-bases.md; F-082 to F-093),
   through engine_api with the real engine: 攻擊 with the counterattack,
   齊攻, 戰法, 計略, fire, 設置 / 修補 / 撤除, the towers, the zone of control
   of a building, the siege, the fall of a base and the capture of officers.
   The rules are documented; every damage number is assumed (Q-172 to Q-176). */

#define FEATURE_UNIT_TOWER     7
#define FEATURE_STOCK_TOWER    5
#define FEATURE_TACTIC_PIERCE  0
#define FEATURE_TACTIC_TOWER   14
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
    battle->start = feature_unit_hex(api, battle->a);
    battle->d     = feature_corridor(api, battle->start);
    if (battle->d >= HEX_DIRECTION_COUNT) return false;
    battle->enemy = (u8)(force + 1);
    hex_coord first = feature_along(battle->start, battle->d, 1);
    hex_coord side  = hex_neighbor(first, (battle->d + 1) % HEX_DIRECTION_COUNT);
    api->set_cell_terrain(e, side.q, side.r, feature_terrain_id(api, "grass"), 1);
    hex_coord second = feature_along(battle->start, battle->d, 2);
    b32 placed = api->set_unit_place(e, battle->b, battle->enemy, first.q, first.r) &&
                 api->set_unit_place(e, battle->t, force, second.q, second.r) &&
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
    engine_command joint = {};
    joint.kind = ENGINE_COMMAND_JOINT_ATTACK;
    joint.unit = battle->a;
    joint.q = at_b.q; joint.r = at_b.r;
    engine_check check = api->can_issue(e, &joint);
    TEST_CHECK(counts, check.refusal == SIM_REFUSAL_NONE && check.quantity == 2, "齊攻: the two pike units next to the target join (the tower cannot)");
    mark = api->ledger_count(e);
    engine_command_result result = api->command_joint_attack(e, battle->a, at_b.q, at_b.r);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && feature_count_events(api, SIM_EVENT_UNIT_DAMAGED, mark) >= 2 &&
                       feature_count_events(api, SIM_EVENT_COUNTERATTACK, mark) == 0 && api->unit_at(e, battle->a).acted && api->unit_at(e, battle->c).acted,
               "each partner strikes; the target answers none of them (assumed); both units acted");

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
                       feature_count_events(api, SIM_EVENT_PLOT_USED, mark) == 1 && fire.turns[(u32)o.row * fire.width + (u32)o.col] == 3,
               "火計 on an empty hex always works; the fire lasts 3 turns (assumed)");
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

    /* An enemy arrow tower shoots the nearest unit in its range each turn. */
    u32 tower = api->place_building(e, FEATURE_BUILD_ARROW, battle->enemy, sixth.q, sixth.r);
    u32 before = api->unit_at(e, battle->a).soldiers;
    feature_next_own_turn(api, force);
    TEST_CHECK(counts, tower != ENGINE_NO_BUILDING && api->unit_at(e, battle->a).soldiers == before - FEATURE_TOWER_SHOT,
               "an enemy arrow tower takes 300 soldiers each turn (assumed)");

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
    u32 mark = api->ledger_count(e);
    engine_command_result result = api->command_attack(e, battle->c, at_b.q, at_b.r);
    u32 fates = feature_count_events(api, SIM_EVENT_OFFICER_CAPTURED, mark) + feature_count_events(api, SIM_EVENT_OFFICER_ESCAPED, mark);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && api->unit_at(e, battle->b).id == ENGINE_NO_UNIT &&
                       feature_count_events(api, SIM_EVENT_UNIT_DESTROYED, mark) == 1 && fates == officers && officers > 0,
               "a unit with no soldiers is destroyed; each officer is captured or escapes (assumed 50 percent)");
    TEST_CHECK(counts, feature_count_events(api, SIM_EVENT_TECHNIQUE_GAINED, mark) >= 1, "the victor gains technique points (assumed)");
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
    mark = api->ledger_count(e);
    result = api->command_attack(e, unit_id, site.q, site.r);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && feature_count_events(api, SIM_EVENT_BASE_CAPTURED, mark) == 1 && api->unit_at(e, unit_id).id == ENGINE_NO_UNIT,
               "the base falls when its soldiers end; the unit enters it");
    b32 owned = false;
    for (u32 index = 0; index < api->city_count(e); ++index)
    {
        if (api->city_at(e, index).city_id == target) owned = api->city_at(e, index).owner_force_id == force;
    }
    TEST_CHECK(counts, owned && api->city_siege(e, target).durability == siege.durability_max / 2, "the attacker owns the base, at half durability (assumed)");
    engine_fixture_destroy(&setup.fixture);
}

internal void
feature_combat_tests(test_counts *counts)
{
    feature_battle_tests(counts);
    feature_siege_tests(counts);
}
