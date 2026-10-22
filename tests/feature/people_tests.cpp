/* Feature tests of the journeys of officers (city-commands.md, 移動 and
   召喚; clone gap report 2: G2-036, G2-037) and of the plots on the road
   (G2-065), through engine_api with the real engine on the sample save and
   the placeholder map. The days of a journey are assumed (Q-228). */

#define FEATURE_MOVE_AP 20   /* city-commands.md: 移動 and 召喚 */

/* Another city of the faction, or ENGINE_NO_CITY. */
internal u8
feature_other_own_city(engine_api *api, u8 force, u8 city)
{
    for (u32 index = 0; index < api->city_count(api->engine); ++index)
    {
        engine_city other = api->city_at(api->engine, index);
        if (other.owner_force_id == force && other.city_id != city) return other.city_id;
    }
    return ENGINE_NO_CITY;
}

/* A city of another faction, or ENGINE_NO_CITY. */
internal u8
feature_enemy_city(engine_api *api, u8 force)
{
    for (u32 index = 0; index < api->city_count(api->engine); ++index)
    {
        engine_city other = api->city_at(api->engine, index);
        if (other.owner_force_id != force && other.owner_force_id != ENGINE_NO_FACTION) return other.city_id;
    }
    return ENGINE_NO_CITY;
}

internal engine_move_order
feature_move_order(u8 city, u8 target, u16 officer)
{
    engine_move_order order = {};
    order.city_id     = city;
    order.target_city = target;
    for (u32 place = 0; place < SIM_MOVE_OFFICERS; ++place) order.officers[place] = ENGINE_NO_OFFICER;
    order.officers[0] = officer;
    return order;
}

internal void
feature_move_officers_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_rivals_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 city = setup.city, force = setup.force;
    u8 other = feature_other_own_city(api, force, city);
    TEST_CHECK(counts, other != ENGINE_NO_CITY, "the faction owns a second city");
    u32 days = api->travel_days(e, city, other);
    TEST_CHECK(counts, days >= 10 && days % 10 == 0 && api->travel_days(e, city, city) == 0 && api->travel_days(e, other, city) == days,
               "the days between two bases are a multiple of 10, 0 for the same base, and the same both ways (assumed, Q-228)");

    u16 ids[4];
    TEST_CHECK(counts, feature_take_officers(api, city, ids, 2) >= 1 && ids[0] != ENGINE_NO_OFFICER, "the city has a free officer");
    engine_move_order order = feature_move_order(city, other, ids[0]);
    engine_command command = {};
    command.kind = ENGINE_COMMAND_MOVE_OFFICERS;
    command.move = order;
    engine_check check = api->can_issue(e, &command);
    TEST_CHECK(counts, check.refusal == SIM_REFUSAL_NONE && check.action_points == FEATURE_MOVE_AP && check.days == days && check.quantity == 1,
               "G2-036: can_issue of a move gives 20 action points and the days of the journey");
    command.move = feature_move_order(city, city, ids[0]);
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_NO_CITY, "refused: the target is the city itself");
    command.move = feature_move_order(city, feature_enemy_city(api, force), ids[0]);
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_NO_CITY, "refused: the target belongs to another faction");
    command.move = feature_move_order(city, other, ENGINE_NO_OFFICER);
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_NO_OFFICER, "refused: no officer");

    u32 points = api->actions_left(e, force), mark = api->ledger_count(e);
    engine_command_result result = api->command_move_officers(e, &order);
    engine_officer moved = api->officer_by_id(e, ids[0]);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && api->actions_left(e, force) == points - FEATURE_MOVE_AP, "the move runs for 20 action points");
    TEST_CHECK(counts, moved.acted && moved.travel_city == other && moved.travel_days == days && moved.current_position == city,
               "the officer acted and is on the road to the target for the days of the journey");
    sim_event left = {};
    TEST_CHECK(counts, feature_find_event(api, SIM_EVENT_OFFICER_MOVED, mark, &left) && left.delta == (s32)days, "the ledger has the departure with the days");
    engine_command develop = {};
    develop.kind    = ENGINE_COMMAND_PATROL;
    develop.city_id = city;
    develop.officers[0] = ids[0];
    develop.officers[1] = develop.officers[2] = ENGINE_NO_OFFICER;
    TEST_CHECK(counts, api->can_issue(e, &develop).refusal != SIM_REFUSAL_NONE, "an officer on the road is busy for the commands of the city");

    u32 turns = 0;
    while (turns < 12 && api->officer_by_id(e, ids[0]).travel_days && feature_next_own_turn(api, force)) turns++;
    moved = api->officer_by_id(e, ids[0]);
    TEST_CHECK(counts, turns == days / 10 && moved.base_location == other && moved.current_position == other && !moved.travel_days,
               "at the turn start after the days, the officer is in the target city");
    sim_event arrived = {};
    TEST_CHECK(counts, feature_find_event(api, SIM_EVENT_OFFICER_ARRIVED, mark, &arrived) && arrived.detail == other, "the ledger has the arrival");

    /* 召喚: the officer comes back. */
    api->set_actions(e, force, 255);
    engine_move_order summon = feature_move_order(city, city, ids[0]);
    command.kind = ENGINE_COMMAND_SUMMON;
    command.move = summon;
    check = api->can_issue(e, &command);
    TEST_CHECK(counts, check.refusal == SIM_REFUSAL_NONE && check.action_points == FEATURE_MOVE_AP && check.days == days,
               "G2-037: can_issue of a summon gives 20 action points and the days from the base of the officer");
    TEST_CHECK(counts, api->command_summon(e, &summon).refusal == SIM_REFUSAL_NONE && api->officer_by_id(e, ids[0]).travel_city == city,
               "the summon runs, and the officer is on the road to the city");
    TEST_CHECK(counts, api->command_summon(e, &summon).refusal == SIM_REFUSAL_OFFICER_BUSY, "refused: the officer is on the road");
    turns = 0;
    while (turns < 12 && api->officer_by_id(e, ids[0]).travel_days && feature_next_own_turn(api, force)) turns++;
    TEST_CHECK(counts, api->officer_by_id(e, ids[0]).current_position == city, "the summoned officer arrives in the city");
    engine_fixture_destroy(&setup.fixture);
}

/* G2-065: a plot goes on the road and gives its result at the turn start of the arrival. */
internal void
feature_plot_mission_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_rivals_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 city = setup.city, force = setup.force;
    u8 target = feature_enemy_city(api, force);
    u16 ids[4];
    TEST_CHECK(counts, target != ENGINE_NO_CITY && feature_take_officers(api, city, ids, 1) >= 1, "an enemy city and a free officer exist");
    engine_court_order order = feature_court_order(city, ids[0], ENGINE_NO_OFFICER);
    order.kind        = SIM_PLOT_RUMOUR;
    order.target_city = target;
    order.force       = api->city_at(e, target).owner_force_id;
    u32 days = api->travel_days(e, city, target), mark = api->ledger_count(e);
    engine_command_result result = api->command_court(e, ENGINE_COMMAND_FACTION_PLOT, &order);
    sim_event started = {};
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && api->officer_by_id(e, ids[0]).acted, "the rumour runs and the officer acted");
    TEST_CHECK(counts, feature_find_event(api, SIM_EVENT_MISSION_STARTED, mark, &started) && started.delta == (s32)days && started.detail == target,
               "the plot goes on the road for the days of the journey");
    TEST_CHECK(counts, feature_count_events(api, SIM_EVENT_FACTION_PLOT, mark) == 0 && feature_count_events(api, SIM_EVENT_FACTION_PLOT_FAILED, mark) == 0,
               "the result does not come at the order");
    u32 turns = 0;
    while (turns < 12 && feature_count_events(api, SIM_EVENT_FACTION_PLOT, mark) + feature_count_events(api, SIM_EVENT_FACTION_PLOT_FAILED, mark) == 0 &&
           feature_next_own_turn(api, force)) turns++;
    sim_event done = {};
    b32 found = feature_find_event(api, SIM_EVENT_FACTION_PLOT, mark, &done) || feature_find_event(api, SIM_EVENT_FACTION_PLOT_FAILED, mark, &done);
    TEST_CHECK(counts, found && turns == days / 10 && done.cause == result.command_id && done.faction == force,
               "the result comes at the turn start of the arrival, with the command of the plot as its cause");
    engine_fixture_destroy(&setup.fixture);
}

/* The days on the real map against the observed dialog of the original
   (G2-036: 江陵 to 江夏 shows 10 days; Q-228). */
internal void
feature_travel_days_real_map_tests(test_counts *counts)
{
    engine_fixture f;
    engine_fixture_init_real(&f, 1, 64);
    engine_api *api = &f.api;
    void *e = api->engine;
    TEST_CHECK(counts, api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH).status == ENGINE_LOAD_OK, "the sample loads on the real map");
    u32 days = api->travel_days(e, 30, 27);
    printf("  travel days on the real map: 30 to 27 = %u\n", days);
    TEST_CHECK(counts, days == 10, "G2-036: the journey from city 30 to city 27 takes 10 days, as the dialog of the original shows (assumed rule, Q-228)");
    engine_fixture_destroy(&f);
}

/* G2-027: the transport of the sample (呂蒙隊, 任務 移動 to 柴桑) keeps a
   long order from the load, moves at 進行, delivers its cargo and asks
   where its officers go. Choice 0 keeps them in the city. Choice 1 sends
   them back to the home city on the road. */
internal void
feature_transport_arrival_tests(test_counts *counts)
{
    for (u8 choice = 0; choice < 2; ++choice)
    {
        engine_fixture f;
        engine_fixture_init_real(&f, 1, 4096);
        engine_api *api = &f.api;
        void *e = api->engine;
        api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
        u8 force = api->whose_turn(e);
        u32 transport = ENGINE_NO_UNIT;
        for (u32 unit_id = 0; unit_id < api->unit_count(e) && transport == ENGINE_NO_UNIT; ++unit_id)
        {
            engine_unit unit = api->unit_at(e, unit_id);
            if (unit.id != ENGINE_NO_UNIT && unit.force_id == force && unit.destination != ENGINE_NO_CITY && api->unit_type_at(e, unit.unit_type).transport) transport = unit_id;
        }
        TEST_CHECK(counts, transport != ENGINE_NO_UNIT, "the sample has a transport of the player on its way to a city");
        if (transport == ENGINE_NO_UNIT) { engine_fixture_destroy(&f); continue; }
        engine_unit before = api->unit_at(e, transport);
        u8 city = before.destination, home = before.home_city;
        u16 leader = before.officers[0];
        u32 soldiers = api->city_economy(e, city).soldiers;
        TEST_CHECK(counts, before.order == UNIT_ORDER_MOVE && home != ENGINE_NO_CITY && home != city,
                   "the transport of the save has the long order 移動 to its city");
        u32 mark = api->ledger_count(e);
        for (u32 turn = 0; turn < 12 && (api->unit_at(e, transport).id != ENGINE_NO_UNIT); ++turn) feature_next_own_turn(api, force);
        engine_pending_event pending = api->pending_event(e);
        engine_event_kind kind = api->event_kind_at(e, pending.event);
        TEST_CHECK(counts, !(api->unit_at(e, transport).id != ENGINE_NO_UNIT) && feature_count_events(api, SIM_EVENT_TRANSPORT_ARRIVED, mark) == 1 &&
                           api->city_economy(e, city).soldiers > soldiers,
                   "the transport reaches its city within 12 turns, and the city takes its soldiers and its cargo");
        TEST_CHECK(counts, pending.active && strcmp(kind.key, "transport_arrived") == 0 && pending.choices == 2 && pending.city == city &&
                           pending.city_2 == home && pending.officer_a == leader && pending.force == force,
                   "the question of the arrival waits for the player, with the city, the home city and the leader");
        TEST_CHECK(counts, api->command_event_choice(e, choice).refusal == SIM_REFUSAL_NONE && !api->pending_event(e).active,
                   "the choice closes the question");
        engine_officer officer = {};
        feature_officer_by_id(api, leader, &officer);
        if (choice == 0)
        {
            TEST_CHECK(counts, officer.current_position == city && officer.travel_city == ENGINE_NO_CITY, "choice 0: the officers stay in the city");
        }
        else
        {
            TEST_CHECK(counts, officer.travel_city == home && officer.travel_days > 0, "choice 1: the officers go back to the home city on the road");
        }
        engine_fixture_destroy(&f);
    }
}

internal void
feature_people_tests(test_counts *counts)
{
    feature_move_officers_tests(counts);
    feature_plot_mission_tests(counts);
    feature_travel_days_real_map_tests(counts);
    feature_transport_arrival_tests(counts);
}
