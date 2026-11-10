/* Unit tests of code/sim/turn.cpp: the action point pool, the turn order,
   and the rule that picks a leader (calendar-and-turn.md,
   factions-corps-and-posts.md). The sims are synthetic. */

internal sim_state *
unit_turn_sim(memory_arena *arena)
{
    sim_state *sim = PushStruct(arena, sim_state);
    sim_init(sim, arena, 1);
    return sim;
}

internal officer_component
unit_officer(u16 id, u8 force, u8 status, u16 city, u8 leadership, u8 war, u8 politics, u8 charm, u16 merit)
{
    officer_component officer = {};
    officer.officer_id       = id;
    officer.force_id         = force;
    officer.status           = status;
    officer.base_location    = city;
    officer.current_position = city;
    officer.leadership       = leadership;
    officer.war              = war;
    officer.politics         = politics;
    officer.charm            = charm;
    officer.merit            = merit;
    officer.job_city         = SIM_NO_CITY;
    return officer;
}

internal city_component
unit_city(u8 id, u8 owner, u32 troops)
{
    city_component city = {};
    city.city_id        = id;
    city.owner_force_id = owner;
    city.troops         = troops;
    city.commanding_officer_id = 0xFFFF;
    return city;
}

internal void
unit_turn_tests(test_counts *counts, memory_arena *arena)
{
    /* The pool (calendar-and-turn.md, "Action points"). */
    TEST_CHECK(counts, sim_recover_action_points(0, 70, 255) == 70, "the pool recovers by the recovery");
    TEST_CHECK(counts, sim_recover_action_points(250, 20, 255) == 255, "the pool has a cap of 255");
    TEST_CHECK(counts, sim_recover_action_points(255, 90, 255) == 255, "a full pool stays at the cap");

    /* The order: fewer cities first, then fewer officers, then fewer troops
       (knowledge/turn-structure.md, unverified), then the smaller force id (assumed). */
    sim_faction_size small = {}, large = {};
    small.faction.force_id = 7; small.faction.city_count = 1; small.faction.officer_count = 30; small.faction.troops = 90000;
    large.faction.force_id = 2; large.faction.city_count = 3; large.faction.officer_count = 5;  large.faction.troops = 1000;
    TEST_CHECK(counts, sim_faction_before(&small, &large) && !sim_faction_before(&large, &small),
               "a faction with fewer cities acts first");
    large.faction.city_count = 1;
    TEST_CHECK(counts, sim_faction_before(&large, &small), "with equal cities, fewer officers act first");
    large.faction.officer_count = 30; large.faction.troops = 90001;
    TEST_CHECK(counts, sim_faction_before(&small, &large), "with equal cities and officers, fewer troops act first");
    large.faction.troops = 90000;
    TEST_CHECK(counts, sim_faction_before(&large, &small) && !sim_faction_before(&small, &large),
               "with all equal, the smaller force id acts first");

    /* U-016: gold, then grain, come before the troops
       (knowledge/turn-structure.md, 「行動順序秘密」 0:26-1:54, 5:58-6:14). */
    large.gold = 400; small.gold = 300; large.faction.troops = 1;
    TEST_CHECK(counts, sim_faction_before(&small, &large) && !sim_faction_before(&large, &small),
               "with equal cities and officers, less gold acts first, before the troops count");
    large.gold = 300; large.food = 5000; small.food = 4000;
    TEST_CHECK(counts, sim_faction_before(&small, &large), "with equal gold, less grain acts first");
    large.food = 4000; large.faction.troops = 90000;

    sim_state *sim = unit_turn_sim(arena);
    sim_add_city(sim, unit_city(0, 4, 5000));
    sim_add_city(sim, unit_city(1, 4, 5000));
    sim_add_city(sim, unit_city(2, 9, 100));
    sim_add_city(sim, unit_city(3, 6, 100));
    sim_add_city(sim, unit_city(4, SIM_NO_FORCE, 0));
    sim_add_officer(sim, unit_officer(10, 4, 0, 0, 80, 70, 60, 90, 100));   /* ruler of 4 */
    sim_add_officer(sim, unit_officer(11, 9, 0, 2, 50, 50, 50, 40, 100));   /* ruler of 9 */
    sim_add_officer(sim, unit_officer(12, 9, 3, 2, 50, 50, 50, 40, 100));
    sim_add_officer(sim, unit_officer(13, 6, 0, 3, 60, 60, 60, 75, 100));   /* ruler of 6 */
    sim_add_officer(sim, unit_officer(14, 6, 6, 3, 60, 60, 60, 60, 100));   /* a free officer counts for no faction */
    TEST_CHECK(counts, !sim_add_officer(sim, unit_officer(10, 4, 3, 0, 1, 1, 1, 1, 0)), "an officer id cannot load two times");
    sim_compute_turn_order(sim);
    sim_turn_state *turns = &sim->turns;
    TEST_CHECK(counts, turns->order_count == 3, "each force that owns a city is a faction of the order");
    TEST_CHECK(counts, turns->order[0].force_id == 6 && turns->order[1].force_id == 9 && turns->order[2].force_id == 4,
               "the order: 6 (1 city, 1 officer), 9 (1 city, 2 officers), 4 (2 cities)");
    TEST_CHECK(counts, turns->order[2].city_count == 2 && turns->order[2].troops == 10000 && turns->order[2].officer_count == 1,
               "a faction counts its cities, troops and active officers");
    TEST_CHECK(counts, turns->order[0].leader_officer_id == 13, "the leader of corps 1 is the ruler");

    /* The recovery: the higher of leadership and charm of the leader
       (knowledge/turn-structure.md, unverified). Assumed (Q-019, Q-141). */
    TEST_CHECK(counts, sim_action_point_recovery(sim, &turns->order[2]) == 90, "the recovery is the higher of leadership and charm");
    TEST_CHECK(counts, sim_action_point_recovery(sim, &turns->order[0]) == 75, "a leader with more charm recovers by the charm");

    /* U-017: each city after the first adds action points, and from the
       seventh city no more (knowledge/turn-structure.md, 「委任軍團超詳解」
       0:44-0:50). Assumed amounts: ap.city_bonus, ap.city_bonus_cities. */
    TEST_CHECK(counts, sim_action_point_city_bonus(1, 20, 5) == 0, "one city adds nothing");
    TEST_CHECK(counts, sim_action_point_city_bonus(2, 20, 5) == 20, "the second city adds 20");
    TEST_CHECK(counts, sim_action_point_city_bonus(6, 20, 5) == 100, "the sixth city adds the fifth bonus");
    TEST_CHECK(counts, sim_action_point_city_bonus(12, 20, 5) == 100, "from the seventh city, no more");
    TEST_CHECK(counts, sim_action_point_city_bonus(0, 20, 5) == 0, "no city adds nothing");
    sim->content.rules.ap_city_bonus        = 20;
    sim->content.rules.ap_city_bonus_cities = 5;
    TEST_CHECK(counts, sim_action_point_recovery(sim, &turns->order[2]) == 110, "two cities: the leader and one city bonus");
    TEST_CHECK(counts, sim_action_point_recovery(sim, &turns->order[0]) == 75, "one city: the leader alone");
    sim->content.rules.ap_city_bonus = 0;

    /* U-016: the order counts the gold and the grain of the cities and of
       the ports and passes, and the troops of both (「行動順序秘密」
       1:56-2:30, 4:48-5:14). */
    {
        sim_state *order_sim = unit_turn_sim(arena);
        city_component rich = unit_city(0, 1, 100); rich.gold = 500; rich.food = 100;
        city_component poor = unit_city(1, 2, 100); poor.gold = 400; poor.food = 100;
        sim_add_city(order_sim, rich);
        sim_add_city(order_sim, poor);
        sim_add_officer(order_sim, unit_officer(30, 1, 0, 0, 50, 50, 50, 50, 0));
        sim_add_officer(order_sim, unit_officer(31, 2, 0, 1, 50, 50, 50, 50, 0));
        sim_compute_turn_order(order_sim);
        TEST_CHECK(counts, order_sim->turns.order[0].force_id == 2, "less gold in the cities acts first");
        order_sim->bases[0].exists = true;
        order_sim->bases[0].force  = 2;
        order_sim->bases[0].troops = 700;
        order_sim->base_stocks[0].gold  = 200;
        order_sim->base_stocks[0].grain = 50;
        sim_compute_turn_order(order_sim);
        TEST_CHECK(counts, order_sim->turns.order[0].force_id == 1, "the gold of a port counts with the cities");
        TEST_CHECK(counts, order_sim->turns.order[1].troops == 800,
                   "a faction counts the troops of its ports and passes");
    }

    /* The leader rule (factions-corps-and-posts.md): ruler or viceroy first,
       then leadership, force and merit. The post is not decoded (Q-147). */
    sim_add_officer(sim, unit_officer(20, 4, 3, 1, 70, 80, 50, 50, 900));
    sim_add_officer(sim, unit_officer(21, 4, 3, 1, 70, 90, 50, 50, 100));
    sim_add_officer(sim, unit_officer(22, 4, 3, 1, 70, 90, 50, 50, 200));
    sim_add_officer(sim, unit_officer(23, 4, 3, 1, 65, 99, 99, 99, 999));
    u16 regulars[] = { 20, 21, 22, 23 };
    TEST_CHECK(counts, sim_select_leader(sim, regulars, 4) == 22, "higher leadership, then higher force, then higher merit");
    sim_add_officer(sim, unit_officer(24, 4, 1, 1, 10, 10, 10, 10, 0));
    u16 with_viceroy[] = { 20, 24, 22 };
    TEST_CHECK(counts, sim_select_leader(sim, with_viceroy, 3) == 24, "a viceroy comes before every regular officer");
    u16 with_ruler[] = { 24, 10 };
    TEST_CHECK(counts, sim_select_leader(sim, with_ruler, 2) == 10, "the ruler comes before the viceroy");
    TEST_CHECK(counts, sim_select_leader(sim, regulars, 0) == SIM_NO_OFFICER, "no officers give no leader");
    TEST_CHECK(counts, sim_city_governor(sim, 1) == 24, "the governor of a city is the leader of its active officers");
    TEST_CHECK(counts, sim_city_governor(sim, 4) == SIM_NO_OFFICER, "a city without officers has no governor");
}
