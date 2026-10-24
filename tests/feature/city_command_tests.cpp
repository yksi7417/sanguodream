/* Feature tests of the five city commands of M3 (city-commands.md,
   weapons-and-production.md; F-025, F-026, F-027, F-029, F-030): 巡察
   patrol, 徵兵 conscript, 訓練 train, 商人 merchant and 生產兵裝 produce,
   through engine_api with the real engine. Each command: its effect, its
   costs, its ledger, and each refusal. The costs are documented; the
   effects are assumed (rules.tsv), and the ledger marks them. */

#define FEATURE_COMMAND_AP     20     /* city-commands.md: each of the five costs 20 */
#define FEATURE_PATROL_GOLD    100
#define FEATURE_CONSCRIPT_GOLD 300
#define FEATURE_WEAPON_PIKE    0
#define FEATURE_WEAPON_HORSE   3
#define FEATURE_WEAPON_RAM     4
#define FEATURE_WEAPON_WOOD_BEAST 5

struct feature_city_setup
{
    engine_fixture fixture;
    engine_api    *api;
    u8             city;
    u8             force;
};

/* Loads the sample and plays the faction of the city with the most free
   officers, with gold and action points. */
internal void
feature_city_setup_init(feature_city_setup *setup)
{
    engine_fixture_init(&setup->fixture, 1, 65536);
    setup->api = &setup->fixture.api;
    engine_api *api = setup->api;
    api->load_save(api->engine, SAMPLE_SAVE_RELATIVE_PATH);
    setup->city  = feature_busiest_city(api);
    setup->force = api->city_at(api->engine, setup->city).owner_force_id;
    feature_play_as(api, setup->force);
    api->set_city_gold(api->engine, setup->city, 50000);
    api->set_actions(api->engine, setup->force, 255);
}

internal engine_command
feature_city_command(engine_command_kind kind, u8 city, u16 first, u16 second = ENGINE_NO_OFFICER, u16 third = ENGINE_NO_OFFICER)
{
    engine_command command = {};
    command.kind        = kind;
    command.city_id     = city;
    command.officers[0] = first;
    command.officers[1] = second;
    command.officers[2] = third;
    return command;
}

internal u32
feature_officer_sum(engine_api *api, const u16 *officers, u32 ability)
{
    u32 sum = 0;
    for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place)
    {
        engine_officer officer = {};
        if (officers[place] == ENGINE_NO_OFFICER || !feature_officer_by_id(api, officers[place], &officer)) continue;
        u8 values[5] = { officer.leadership, officer.war, officer.intelligence, officer.politics, officer.charm };
        sum += values[ability];
    }
    return sum;
}

#define FEATURE_LEADERSHIP 0
#define FEATURE_FORCE      1
#define FEATURE_INTELLECT  2
#define FEATURE_POLITICS   3
#define FEATURE_CHARM      4

/* Takes the next free officers of the city into ids; returns how many. */
internal u32
feature_take_officers(engine_api *api, u8 city, u16 *ids, u32 count)
{
    for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place) ids[place] = ENGINE_NO_OFFICER;
    return feature_free_officers(api, city, ids, count);
}

internal void
feature_patrol_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_city_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 city = setup.city, force = setup.force;

    u16 ids[3];
    feature_take_officers(api, city, ids, 3);
    api->set_city_resource(e, city, SIM_RESOURCE_PUBLIC_ORDER, 0, 50);
    engine_command command = feature_city_command(ENGINE_COMMAND_PATROL, city, ids[0], ids[1], ids[2]);
    engine_check check = api->can_issue(e, &command);
    /* G3-013: the rise is 3 plus the best leadership of the officers over 40 (assumed from four observations). */
    u32 best_leadership = 0;
    for (u32 place = 0; place < 3; ++place)
    {
        engine_officer officer = api->officer_by_id(e, ids[place]);
        if (officer.officer_id != ENGINE_NO_OFFICER && officer.leadership > best_leadership) best_leadership = officer.leadership;
    }
    u32 rise = 3 + best_leadership / 40;
    TEST_CHECK(counts, check.refusal == SIM_REFUSAL_NONE && check.gold == FEATURE_PATROL_GOLD && check.action_points == FEATURE_COMMAND_AP &&
                       check.value_before == 50 && check.value_after == Minimum(100u, 50 + rise) && check.assumed,
               "patrol: can_issue gives 100 gold, 20 points, and public order now -> after (assumed rise)");

    u32 gold = api->city_at(e, city).gold, points = api->actions_left(e, force);
    u32 mark = api->ledger_count(e);
    engine_command_result result = api->command_patrol(e, city, ids);
    engine_city_economy economy = api->city_economy(e, city);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && result.command_id != 0 && economy.public_order == check.value_after,
               "patrol raises public order by the sum of leadership");
    TEST_CHECK(counts, api->city_at(e, city).gold == gold - FEATURE_PATROL_GOLD && api->actions_left(e, force) == points - FEATURE_COMMAND_AP,
               "patrol pays 100 gold and 20 action points");
    sim_event patrolled = {};
    TEST_CHECK(counts, feature_find_city_event(api, city, SIM_EVENT_PATROLLED, mark, &patrolled) && patrolled.delta == (s32)(check.value_after - 50) &&
                       patrolled.assumed && patrolled.cause == result.command_id &&
                       feature_count_events(api, SIM_EVENT_GOLD_SPENT, mark) == 1 && feature_count_events(api, SIM_EVENT_ACTION_POINTS_SPENT, mark) == 1 &&
                       feature_count_events(api, SIM_EVENT_MERIT_GAINED, mark) == 3 && feature_count_events(api, SIM_EVENT_TECHNIQUE_GAINED, mark) == 1,
               "the ledger has the order, the gold, the points, the merit of three officers and the technique points");
    engine_officer officer = {};
    TEST_CHECK(counts, feature_officer_by_id(api, ids[0], &officer) && officer.acted && economy.patrolled, "the officers acted; the city patrolled");

    u16 more[3];
    feature_take_officers(api, city, more, 1);
    TEST_CHECK(counts, api->command_patrol(e, city, more).refusal == SIM_REFUSAL_LIMIT_REACHED, "refused: patrol runs once per turn");
    TEST_CHECK(counts, api->command_patrol(e, city, ids).refusal == SIM_REFUSAL_LIMIT_REACHED, "the limit comes before the officer checks");

    feature_next_own_turn(api, force);
    api->set_actions(e, force, 255);
    api->set_city_gold(e, city, 50000);
    feature_take_officers(api, city, ids, 1);
    api->set_city_resource(e, city, SIM_RESOURCE_PUBLIC_ORDER, 0, 100);
    TEST_CHECK(counts, api->command_patrol(e, city, ids).refusal == SIM_REFUSAL_VALUE_AT_MAXIMUM, "refused: public order is at 100");
    api->set_city_resource(e, city, SIM_RESOURCE_PUBLIC_ORDER, 0, 99);
    command = feature_city_command(ENGINE_COMMAND_PATROL, city, ids[0]);
    TEST_CHECK(counts, api->can_issue(e, &command).value_after == 100, "public order stops at 100");
    TEST_CHECK(counts, api->city_economy(e, city).bandits_blocked, "at 80 or more public order the city has no bandits (a flag)");
    api->set_city_resource(e, city, SIM_RESOURCE_PUBLIC_ORDER, 0, 79);
    TEST_CHECK(counts, !api->city_economy(e, city).bandits_blocked, "below 80 bandits can appear");
    api->set_city_gold(e, city, FEATURE_PATROL_GOLD - 1);
    TEST_CHECK(counts, api->command_patrol(e, city, ids).refusal == SIM_REFUSAL_NO_GOLD, "refused: 99 gold is less than 100");
    api->set_city_gold(e, city, 50000);
    api->set_actions(e, force, FEATURE_COMMAND_AP - 1);
    TEST_CHECK(counts, api->command_patrol(e, city, ids).refusal == SIM_REFUSAL_NO_ACTION_POINTS, "refused: 19 action points");
    api->set_actions(e, force, 255);
    u16 none[3] = { ENGINE_NO_OFFICER, ENGINE_NO_OFFICER, ENGINE_NO_OFFICER };
    TEST_CHECK(counts, api->command_patrol(e, city, none).refusal == SIM_REFUSAL_NO_OFFICER, "refused: no officer");
    u16 twice[3] = { ids[0], ids[0], ENGINE_NO_OFFICER };
    TEST_CHECK(counts, api->command_patrol(e, city, twice).refusal == SIM_REFUSAL_DUPLICATE_OFFICER, "refused: one officer two times");
    TEST_CHECK(counts, api->command_patrol(e, 200, ids).refusal == SIM_REFUSAL_NO_CITY, "refused: no city");
    u32 ledger_before = api->ledger_count(e);
    api->command_patrol(e, city, none);
    TEST_CHECK(counts, api->ledger_count(e) == ledger_before, "a refused command ledgers nothing");

    /* An enemy within 2 hexes gives a smaller rise (assumed half). */
    api->set_city_resource(e, city, SIM_RESOURCE_PUBLIC_ORDER, 0, 20);
    engine_city_slots site = api->city_slots(e, city);
    api->place_unit(e, force == 1 ? 2 : 1, site.q + 1, site.r);
    command = feature_city_command(ENGINE_COMMAND_PATROL, city, ids[0]);
    check = api->can_issue(e, &command);
    u32 near_rise = Maximum(1u, feature_officer_sum(api, ids, FEATURE_LEADERSHIP) * 10 / 100) * 50 / 100;
    TEST_CHECK(counts, check.refusal == SIM_REFUSAL_NONE && check.value_after == 20 + Maximum(near_rise, 1u) ,
               "patrol: an enemy within 2 hexes gives half the rise (assumed); patrol still runs");
    engine_fixture_destroy(&setup.fixture);
}

internal void
feature_conscript_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_city_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 city = setup.city, force = setup.force;

    u16 ids[3];
    feature_take_officers(api, city, ids, 1);
    TEST_CHECK(counts, api->command_conscript(e, city, ids).refusal == SIM_REFUSAL_NO_FACILITY, "refused: conscription needs a barracks");
    u8 barracks = feature_build(api, city, FEATURE_FACILITY_BARRACKS, force);
    TEST_CHECK(counts, barracks != ENGINE_ANY_SLOT, "a barracks is built");
    api->set_city_gold(e, city, 50000);
    api->set_actions(e, force, 255);
    api->set_city_resource(e, city, SIM_RESOURCE_SOLDIERS, 0, 10000);
    api->set_city_resource(e, city, SIM_RESOURCE_PUBLIC_ORDER, 0, 60);
    api->set_city_resource(e, city, SIM_RESOURCE_MORALE, 0, 70);
    feature_take_officers(api, city, ids, 3);
    engine_command command = feature_city_command(ENGINE_COMMAND_CONSCRIPT, city, ids[0], ids[1], ids[2]);
    engine_check check = api->can_issue(e, &command);
    /* The run of 2026-10-22: F x (1000 + 6 x S x public order / 100); a new barracks is level 1 (F = 1.0), the order is 60. */
    u32 soldiers = (100000 + feature_officer_sum(api, ids, FEATURE_CHARM) * 6 * 60) / 100;
    TEST_CHECK(counts, check.refusal == SIM_REFUSAL_NONE && check.gold == FEATURE_CONSCRIPT_GOLD && check.action_points == FEATURE_COMMAND_AP &&
                       check.value_before == 10000 && check.value_after == 10000 + soldiers && check.slot == barracks,
               "conscript: 300 gold, 20 points, and soldiers now -> after: 1000 + 5.6 x the sum of charm (G2-011)");
    u32 mark = api->ledger_count(e);
    engine_command_result result = api->command_conscript(e, city, ids);
    engine_city_economy economy = api->city_economy(e, city);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && economy.soldiers == 10000 + soldiers &&
                       economy.public_order == 50 && economy.morale == 61 && api->city_at(e, city).gold == 50000 - FEATURE_CONSCRIPT_GOLD,
               "conscription adds soldiers, costs 300 gold, and takes 10 public order and 9 morale (pairs/conscript/001, G2-011)");
    sim_event conscripted = {}, order = {}, morale = {};
    TEST_CHECK(counts, feature_find_city_event(api, city, SIM_EVENT_CONSCRIPTED, mark, &conscripted) && conscripted.delta == (s32)soldiers &&
                       conscripted.detail == barracks && conscripted.assumed &&
                       feature_find_city_event(api, city, SIM_EVENT_ORDER_LOST, mark, &order) && order.delta == -10 &&
                       feature_find_city_event(api, city, SIM_EVENT_MORALE_LOST, mark, &morale) && morale.delta == -9,
               "the ledger has the soldiers with the barracks, the order and the morale");
    TEST_CHECK(counts, economy.barracks == 1 && economy.barracks_free == 0 && api->facility_at(e, city, barracks).used,
               "the barracks is used in this turn");
    u16 more[3];
    feature_take_officers(api, city, more, 1);
    TEST_CHECK(counts, api->command_conscript(e, city, more).refusal == SIM_REFUSAL_LIMIT_REACHED,
               "refused: one conscription per barracks per turn");

    u8 second = feature_build(api, city, FEATURE_FACILITY_BARRACKS, force);
    api->set_city_gold(e, city, 50000);
    api->set_actions(e, force, 255);
    feature_take_officers(api, city, ids, 2);
    u16 first_only[3]  = { ids[0], ENGINE_NO_OFFICER, ENGINE_NO_OFFICER };
    u16 second_only[3] = { ids[1], ENGINE_NO_OFFICER, ENGINE_NO_OFFICER };
    TEST_CHECK(counts, second != ENGINE_ANY_SLOT && api->command_conscript(e, city, first_only).refusal == SIM_REFUSAL_NONE &&
                       api->command_conscript(e, city, second_only).refusal == SIM_REFUSAL_NONE,
               "with two barracks, two conscriptions run in one turn");
    TEST_CHECK(counts, api->city_economy(e, city).barracks_free == 0, "both barracks are used");

    feature_next_own_turn(api, force);
    api->set_city_gold(e, city, 50000);
    api->set_actions(e, force, 255);
    feature_take_officers(api, city, ids, 1);
    u32 cap = api->city_economy(e, city).soldier_cap;
    TEST_CHECK(counts, cap == 100000 || cap == 150000, "the soldier cap is 100,000, or 150,000 for a big city");
    api->set_city_resource(e, city, SIM_RESOURCE_SOLDIERS, 0, cap);
    TEST_CHECK(counts, api->command_conscript(e, city, ids).refusal == SIM_REFUSAL_VALUE_AT_MAXIMUM, "refused: the soldiers are at the cap");
    api->set_city_resource(e, city, SIM_RESOURCE_SOLDIERS, 0, cap - 7);
    command = feature_city_command(ENGINE_COMMAND_CONSCRIPT, city, ids[0]);
    TEST_CHECK(counts, api->can_issue(e, &command).value_after == cap, "the soldiers stop at the cap");
    api->set_city_gold(e, city, FEATURE_CONSCRIPT_GOLD - 1);
    TEST_CHECK(counts, api->command_conscript(e, city, ids).refusal == SIM_REFUSAL_NO_GOLD, "refused: 299 gold is less than 300");
    api->set_city_gold(e, city, 50000);
    api->set_city_resource(e, city, SIM_RESOURCE_SOLDIERS, 0, 0);
    api->set_facility_level(e, city, barracks, 3);
    command = feature_city_command(ENGINE_COMMAND_CONSCRIPT, city, ids[0]);
    check = api->can_issue(e, &command);
    u16 single[3] = { ids[0], ENGINE_NO_OFFICER, ENGINE_NO_OFFICER };
    u32 order_now = api->city_economy(e, city).public_order;
    u32 level3 = (100000 + feature_officer_sum(api, single, FEATURE_CHARM) * 6 * order_now) * 150 / 10000;   /* floor(1.5 x (1000 + 6 x S x order / 100)) */
    TEST_CHECK(counts, check.slot == barracks && check.value_after == level3,
               "a level-3 barracks gives 1.5 times the soldiers and is used first (city-commands.md)");
    engine_city_slots site = api->city_slots(e, city);
    api->place_unit(e, force == 1 ? 2 : 1, site.q + 2, site.r);
    TEST_CHECK(counts, api->can_issue(e, &command).value_after == level3 / 2,
               "an enemy within 2 hexes gives fewer soldiers (assumed half); conscription still runs");
    engine_fixture_destroy(&setup.fixture);
}

internal void
feature_train_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_city_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 city = setup.city, force = setup.force;

    u16 ids[3];
    feature_take_officers(api, city, ids, 3);
    api->set_city_resource(e, city, SIM_RESOURCE_MORALE, 0, 40);
    engine_command command = feature_city_command(ENGINE_COMMAND_TRAIN, city, ids[0], ids[1], ids[2]);
    engine_check check = api->can_issue(e, &command);
    /* The run of 2026-10-22: floor(3 + S / 15) without a drill ground, floor(4 + 0.115 x S) with one; S is the first officer plus half of the others. */
    engine_officer first_officer = {}, second_officer = {}, third_officer = {};
    feature_officer_by_id(api, ids[0], &first_officer);
    feature_officer_by_id(api, ids[1], &second_officer);
    feature_officer_by_id(api, ids[2], &third_officer);
    u32 force_sum = first_officer.war + (second_officer.war + third_officer.war) / 2;
    b32 drill_ground = false;
    for (u32 slot = 0; slot < api->city_slots(e, city).slot_count; ++slot)
    {
        engine_facility built = api->facility_at(e, city, (u8)slot);
        if (built.state == SIM_SLOT_BUILT && built.facility == 17) drill_ground = true;   /* facilities.tsv: drill_ground */
    }
    u32 rise = drill_ground ? 4 + force_sum * 115 / 1000 : 3 + force_sum / 15;
    TEST_CHECK(counts, check.refusal == SIM_REFUSAL_NONE && check.gold == 0 && check.action_points == FEATURE_COMMAND_AP &&
                       check.value_before == 40 && check.value_after == Minimum(100u, 40 + rise),
               "train: no gold, 20 points, and morale now -> after from the sum of force (assumed)");
    u32 gold = api->city_at(e, city).gold;
    u32 mark = api->ledger_count(e);
    engine_command_result result = api->command_train(e, city, ids);
    sim_event trained = {};
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && api->city_economy(e, city).morale == check.value_after &&
                       api->city_at(e, city).gold == gold && api->city_economy(e, city).trained,
               "train raises the morale of the city and costs no gold");
    TEST_CHECK(counts, feature_find_city_event(api, city, SIM_EVENT_TRAINED, mark, &trained) && trained.assumed &&
                       trained.delta == (s32)(check.value_after - 40) && feature_count_events(api, SIM_EVENT_GOLD_SPENT, mark) == 0,
               "the ledger has the morale and no gold");
    u16 more[3];
    feature_take_officers(api, city, more, 1);
    TEST_CHECK(counts, api->command_train(e, city, more).refusal == SIM_REFUSAL_LIMIT_REACHED, "refused: train runs once per turn");

    feature_next_own_turn(api, force);
    api->set_actions(e, force, 255);
    feature_take_officers(api, city, ids, 1);
    api->set_city_resource(e, city, SIM_RESOURCE_MORALE, 0, 100);
    TEST_CHECK(counts, api->command_train(e, city, ids).refusal == SIM_REFUSAL_VALUE_AT_MAXIMUM, "refused: morale is at 100");
    api->set_city_resource(e, city, SIM_RESOURCE_MORALE, 0, 99);
    command = feature_city_command(ENGINE_COMMAND_TRAIN, city, ids[0]);
    TEST_CHECK(counts, api->can_issue(e, &command).value_after == 100, "morale stops at 100 (no 熟練兵 technique yet)");
    engine_fixture_destroy(&setup.fixture);
}

internal void
feature_merchant_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_city_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 city = setup.city, force = setup.force;

    u16 ids[3];
    feature_take_officers(api, city, ids, 3);
    engine_officer trader = {};
    feature_officer_by_id(api, ids[0], &trader);
    api->set_city_resource(e, city, SIM_RESOURCE_GRAIN, 0, 20000);
    engine_command command = feature_city_command(ENGINE_COMMAND_MERCHANT, city, ids[0], ids[1]);
    command.merchant_mode = SIM_MERCHANT_BUY;
    command.amount = 1000;
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_TOO_MANY_OFFICERS, "refused: the merchant takes one officer");
    command.officers[1] = ENGINE_NO_OFFICER;
    command.amount = 0;
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_NO_AMOUNT, "refused: no amount");
    command.amount = 1000;
    engine_check check = api->can_issue(e, &command);
    /* city-commands.md "Merchant rate": E = 400 / (450 - P); a purchase gives floor(G x R x E) grain. */
    u32 price = api->city_economy(e, city).grain_per_gold;
    u32 divisor = 450u - trader.politics;
    u32 grain = 1000 * price * 400 / divisor;
    TEST_CHECK(counts, check.refusal == SIM_REFUSAL_NONE && check.gold == 1000 && check.action_points == FEATURE_COMMAND_AP &&
                       check.quantity == grain && check.value_before == 20000 && check.value_after == 20000 + grain &&
                       check.effect_percent == 400 * 100 / divisor,
               "merchant: 1000 gold buys R grain for each gold times the trade effect of the officer (G2-025)");
    TEST_CHECK(counts, price >= 3 && price <= 7, "the city price is 3 to 7 grain for 1 gold (G2-013)");
    u32 mark = api->ledger_count(e);
    engine_command_result result = api->command_merchant(e, city, ids[0], SIM_MERCHANT_BUY, 1000);
    sim_event bought = {}, spent = {};
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && api->city_at(e, city).food == 20000 + grain && api->city_at(e, city).gold == 50000 - 1000,
               "the purchase moves the gold to grain");
    TEST_CHECK(counts, feature_find_city_event(api, city, SIM_EVENT_GRAIN_BOUGHT, mark, &bought) && bought.delta == (s32)grain && bought.assumed &&
                       feature_find_city_event(api, city, SIM_EVENT_GOLD_SPENT, mark, &spent) && spent.delta == -1000,
               "the ledger has the grain and the gold");
    TEST_CHECK(counts, feature_ledger_sum(api, city, SIM_RESOURCE_GOLD, mark) == -1000 && feature_ledger_sum(api, city, SIM_RESOURCE_GRAIN, mark) == (s64)grain,
               "the ledger filter by city and resource explains the purchase");

    engine_officer seller = {};
    feature_officer_by_id(api, ids[1], &seller);
    /* A sale of N grain gives floor(N x E x 0.8 / R) gold. */
    u32 gold = 6000 * 400 * 80 / ((450u - seller.politics) * price * 100);
    mark = api->ledger_count(e);
    result = api->command_merchant(e, city, ids[1], SIM_MERCHANT_SELL, 6000);
    sim_event sold = {}, earned = {};
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && api->city_at(e, city).food == 20000 + grain - 6000 &&
                       api->city_at(e, city).gold == 50000 - 1000 + gold,
               "the sale moves 6000 grain to gold at the city price, the trade effect and the loss of 20 percent (G2-025)");
    TEST_CHECK(counts, feature_find_city_event(api, city, SIM_EVENT_GRAIN_SOLD, mark, &sold) && sold.delta == -6000 &&
                       feature_find_city_event(api, city, SIM_EVENT_GOLD_EARNED, mark, &earned) && earned.delta == (s32)gold && earned.assumed,
               "the ledger has the sale and the gold");

    u32 food = api->city_at(e, city).food;
    TEST_CHECK(counts, api->command_merchant(e, city, ids[2], SIM_MERCHANT_SELL, food + 1).refusal == SIM_REFUSAL_NO_GRAIN,
               "refused: the city has less grain than the sale");
    TEST_CHECK(counts, api->command_merchant(e, city, ids[2], SIM_MERCHANT_BUY, api->city_at(e, city).gold + 1).refusal == SIM_REFUSAL_NO_GOLD,
               "refused: the city has less gold than the purchase");
    TEST_CHECK(counts, api->command_merchant(e, city, ids[2], 7, 100).refusal == SIM_REFUSAL_NO_AMOUNT, "refused: an unknown mode");
    api->set_city_resource(e, city, SIM_RESOURCE_GRAIN, 0, 1000000);
    TEST_CHECK(counts, api->command_merchant(e, city, ids[2], SIM_MERCHANT_BUY, 100).refusal == SIM_REFUSAL_VALUE_AT_MAXIMUM,
               "refused: the grain is at the cap");
    TEST_CHECK(counts, api->command_merchant(e, city, ids[0], SIM_MERCHANT_SELL, 100).refusal == SIM_REFUSAL_OFFICER_ALREADY_ACTED,
               "refused: the officer acted in this turn");
    engine_city_slots site = api->city_slots(e, city);
    api->place_unit(e, force == 1 ? 2 : 1, site.q, site.r + 1);
    TEST_CHECK(counts, api->command_merchant(e, city, ids[2], SIM_MERCHANT_SELL, 100).refusal == SIM_REFUSAL_ENEMY_NEAR,
               "refused: the merchant cannot run with an enemy within 2 hexes");
    engine_fixture_destroy(&setup.fixture);
}

internal void
feature_produce_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_city_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 city = setup.city, force = setup.force;

    TEST_CHECK(counts, api->weapon_kind_count(e) == 10 && api->weapon_kind_at(e, FEATURE_WEAPON_PIKE).cost == 700 &&
                       api->weapon_kind_at(e, FEATURE_WEAPON_RAM).cost == 1500 && api->weapon_kind_at(e, 9).cost == 2000 &&
                       api->weapon_kind_at(e, 99).id == ENGINE_NO_WEAPON,
               "the weapon table has the 10 weapons with the documented costs");
    u16 ids[3];
    feature_take_officers(api, city, ids, 1);
    TEST_CHECK(counts, api->command_produce(e, city, FEATURE_WEAPON_PIKE, ids).refusal == SIM_REFUSAL_NO_FACILITY, "refused: pikes need a smithy");
    TEST_CHECK(counts, api->command_produce(e, city, 99, ids).refusal == SIM_REFUSAL_NO_FACILITY, "refused: no such weapon");

    u8 smithy = feature_build(api, city, FEATURE_FACILITY_SMITHY, force);
    TEST_CHECK(counts, smithy != ENGINE_ANY_SLOT, "a smithy is built");
    api->set_city_gold(e, city, 50000);
    api->set_actions(e, force, 255);
    feature_take_officers(api, city, ids, 3);
    engine_command command = feature_city_command(ENGINE_COMMAND_PRODUCE, city, ids[0], ids[1], ids[2]);
    command.weapon = FEATURE_WEAPON_PIKE;
    engine_check check = api->can_issue(e, &command);
    engine_city_economy economy = api->city_economy(e, city);
    /* The run of 2026-10-22: 1000 + 10 x I at a level-1 smithy; I is the first officer plus half of the others. */
    engine_officer maker_a = {}, maker_b = {}, maker_c = {};
    feature_officer_by_id(api, ids[0], &maker_a);
    feature_officer_by_id(api, ids[1], &maker_b);
    feature_officer_by_id(api, ids[2], &maker_c);
    u32 quantity = 1000 + 10 * (maker_a.intelligence + (maker_b.intelligence + maker_c.intelligence) / 2);
    b32 specialty = api->city_slots(e, city).specialty == CITY_SPECIALTY_PIKE;
    TEST_CHECK(counts, check.refusal == SIM_REFUSAL_NONE && check.gold == (specialty ? 560u : 700u) && check.action_points == FEATURE_COMMAND_AP &&
                       check.days == 0 && check.slot == smithy && check.value_before == economy.stocks[0] &&
                       check.value_after == economy.stocks[0] + quantity,
               "produce: 700 gold, 20 points, no period, and the stock now -> after from the sum of intellect (assumed)");
    u32 mark = api->ledger_count(e);
    engine_command_result result = api->command_produce(e, city, FEATURE_WEAPON_PIKE, ids);
    sim_event produced = {};
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && api->city_economy(e, city).stocks[0] == economy.stocks[0] + quantity &&
                       api->city_at(e, city).gold == 50000 - check.gold, "the pikes arrive at once");
    TEST_CHECK(counts, feature_find_city_event(api, city, SIM_EVENT_WEAPONS_PRODUCED, mark, &produced) && produced.delta == (s32)quantity &&
                       produced.detail == FEATURE_WEAPON_PIKE && produced.assumed, "the ledger has the weapons");
    u16 more[3];
    feature_take_officers(api, city, more, 1);
    TEST_CHECK(counts, api->command_produce(e, city, 1, more).refusal == SIM_REFUSAL_LIMIT_REACHED,
               "refused: one production per smithy per turn, for each smithy weapon");
    TEST_CHECK(counts, api->command_produce(e, city, FEATURE_WEAPON_HORSE, more).refusal == SIM_REFUSAL_NO_FACILITY,
               "refused: war horses need a stable");
    TEST_CHECK(counts, api->command_produce(e, city, FEATURE_WEAPON_WOOD_BEAST, more).refusal == SIM_REFUSAL_NEEDS_TECHNIQUE,
               "refused: the wood beast needs the technique 開發木獸");

    feature_next_own_turn(api, force);
    api->set_city_gold(e, city, 50000);
    api->set_actions(e, force, 255);
    feature_take_officers(api, city, ids, 1);
    api->set_city_resource(e, city, SIM_RESOURCE_WEAPONS, 0, 100000);
    TEST_CHECK(counts, api->command_produce(e, city, FEATURE_WEAPON_PIKE, ids).refusal == SIM_REFUSAL_VALUE_AT_MAXIMUM,
               "refused: the stock is at the cap of 100,000");
    api->set_city_resource(e, city, SIM_RESOURCE_WEAPONS, 0, 99990);
    command = feature_city_command(ENGINE_COMMAND_PRODUCE, city, ids[0]);
    command.weapon = FEATURE_WEAPON_PIKE;
    TEST_CHECK(counts, api->can_issue(e, &command).value_after == 100000, "the stock stops at the cap");

    /* A siege weapon: one item after a period; the officers are busy. */
    u8 workshop = feature_build(api, city, FEATURE_FACILITY_WORKSHOP, force);
    TEST_CHECK(counts, workshop != ENGINE_ANY_SLOT, "a workshop is built");
    api->set_city_gold(e, city, 50000);
    api->set_actions(e, force, 255);
    feature_take_officers(api, city, ids, 1);
    command = feature_city_command(ENGINE_COMMAND_PRODUCE, city, ids[0]);
    command.weapon = FEATURE_WEAPON_RAM;
    check = api->can_issue(e, &command);
    u32 intellect = feature_officer_sum(api, ids, FEATURE_INTELLECT);
    u32 days = 90 - 60 * Minimum(intellect, 100u) / 100;
    u32 turns = calendar_turns_for_days(days);
    u32 rams = api->city_economy(e, city).stocks[api->weapon_kind_at(e, FEATURE_WEAPON_RAM).stock];
    TEST_CHECK(counts, check.refusal == SIM_REFUSAL_NONE && check.turns == turns && check.value_after == rams + 1 && check.slot == workshop,
               "produce: a ram takes a period from the sum of intellect (assumed line from 90 to 30 days)");
    mark = api->ledger_count(e);
    result = api->command_produce(e, city, FEATURE_WEAPON_RAM, ids);
    engine_officer worker = {};
    sim_event started = {};
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && feature_find_city_event(api, city, SIM_EVENT_PRODUCTION_STARTED, mark, &started) &&
                       started.delta == (s32)turns && started.detail == FEATURE_WEAPON_RAM &&
                       api->facility_at(e, city, workshop).job_weapon == FEATURE_WEAPON_RAM &&
                       feature_officer_by_id(api, ids[0], &worker) && worker.job_city == city,
               "the production starts: the workshop has the job and the officer is busy");
    u32 stock_index = api->weapon_kind_at(e, FEATURE_WEAPON_RAM).stock;
    for (u32 turn = 1; turn < turns; ++turn) feature_next_own_turn(api, force);
    TEST_CHECK(counts, api->city_economy(e, city).stocks[stock_index] == rams, "the ram is not there before the period ends");
    feature_take_officers(api, city, more, 1);
    TEST_CHECK(counts, api->command_produce(e, city, FEATURE_WEAPON_RAM, more).refusal == SIM_REFUSAL_LIMIT_REACHED,
               "refused: the workshop has a job (assumed)");
    mark = api->ledger_count(e);
    feature_next_own_turn(api, force);
    TEST_CHECK(counts, api->city_economy(e, city).stocks[stock_index] == rams + 1 &&
                       feature_find_city_event(api, city, SIM_EVENT_WEAPONS_PRODUCED, mark, &produced) && produced.delta == 1 &&
                       feature_officer_by_id(api, ids[0], &worker) && worker.job_city == ENGINE_NO_CITY &&
                       api->facility_at(e, city, workshop).job_weapon == ENGINE_NO_WEAPON,
               "after the period the city has one more ram and the officer is free");
    engine_fixture_destroy(&setup.fixture);
}

/* A specialty city pays less and gets free weapons each month (assumed amounts). */
internal void
feature_specialty_tests(test_counts *counts)
{
    engine_fixture f;
    engine_fixture_init(&f, 1, 65536);
    engine_api *api = &f.api;
    void *e = api->engine;
    api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
    u8 city = ENGINE_NO_CITY;
    u16 ids[8];
    for (u32 index = 0; index < api->city_count(e) && city == ENGINE_NO_CITY; ++index)
    {
        engine_city_slots slots = api->city_slots(e, (u8)index);
        if (slots.specialty < CITY_SPECIALTY_PIKE || slots.specialty > CITY_SPECIALTY_CROSSBOW) continue;
        if (api->city_at(e, index).owner_force_id == ENGINE_NO_FACTION) continue;
        if (feature_free_officers(api, (u8)index, ids, 3) >= 3) city = (u8)index;
    }
    TEST_CHECK(counts, city != ENGINE_NO_CITY, "the sample has a pike, halberd or crossbow city with three officers");
    if (city == ENGINE_NO_CITY) { engine_fixture_destroy(&f); return; }
    u8 force = api->city_at(e, city).owner_force_id;
    feature_play_as(api, force);
    u8 weapon = (u8)(api->city_slots(e, city).specialty - CITY_SPECIALTY_PIKE);
    u8 smithy = feature_build(api, city, FEATURE_FACILITY_SMITHY, force);
    api->set_city_gold(e, city, 50000);
    api->set_actions(e, force, 255);
    u16 officers[3];
    feature_take_officers(api, city, officers, 1);
    engine_command command = feature_city_command(ENGINE_COMMAND_PRODUCE, city, officers[0]);
    command.weapon = weapon;
    TEST_CHECK(counts, smithy != ENGINE_ANY_SLOT && api->can_issue(e, &command).gold == 560,
               "a specialty city pays 560 for its weapon (assumed 80 percent of 700)");
    command.weapon = (u8)((weapon + 1) % 3);
    TEST_CHECK(counts, api->can_issue(e, &command).gold == 700, "another smithy weapon costs 700");

    feature_walk_to_month_end(api, force);
    u32 stock = api->city_economy(e, city).stocks[weapon];
    u32 mark = api->ledger_count(e);
    feature_next_own_turn(api, force);
    sim_event free_weapons = {};
    TEST_CHECK(counts, feature_find_city_event(api, city, SIM_EVENT_WEAPONS_FREE, mark, &free_weapons) && free_weapons.assumed &&
                       free_weapons.detail == weapon && free_weapons.delta == 100 &&
                       api->city_economy(e, city).stocks[weapon] == stock + 100,
               "on day 1 a specialty city with its facility gets free weapons (assumed 100)");
    engine_fixture_destroy(&f);
}

/* The ledger filter is a query: it changes nothing, and it keeps only the
   events of the city and the resource. */
internal void
feature_ledger_filter_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_city_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 city = setup.city;
    u16 ids[3];
    feature_take_officers(api, city, ids, 1);
    engine_command_result patrol = api->command_patrol(e, city, ids);
    u32 count = api->ledger_count(e);
    engine_ledger_view all = api->ledger_filter(e, ENGINE_ANY_CITY, ENGINE_ANY_RESOURCE);
    TEST_CHECK(counts, all.count == count && api->ledger_count(e) == count, "no filter keeps every event and adds none");
    engine_ledger_view gold = api->ledger_filter(e, ENGINE_ANY_CITY, SIM_RESOURCE_GOLD);
    u32 gold_events = 0;
    for (u32 index = 0; index < count; ++index)
    {
        if (sim_event_resource(api->ledger_at(e, index).kind) == SIM_RESOURCE_GOLD) gold_events++;
    }
    b32 only_gold = gold.count == gold_events;
    for (u32 index = 0; index < gold.count; ++index)
    {
        if (sim_event_resource(api->ledger_at(e, gold.indices[index]).kind) != SIM_RESOURCE_GOLD) only_gold = false;
    }
    TEST_CHECK(counts, only_gold, "a resource filter keeps each event of the resource and no other");
    engine_ledger_view mine = api->ledger_filter(e, city, ENGINE_ANY_RESOURCE);
    b32 has_patrol = false, ordered = true;
    for (u32 index = 0; index < mine.count; ++index)
    {
        sim_event event = api->ledger_at(e, mine.indices[index]);
        if (event.kind == SIM_EVENT_PATROLLED && event.cause == patrol.command_id) has_patrol = true;
        if (event.kind == SIM_EVENT_ACTION_POINTS_SPENT || event.kind == SIM_EVENT_MERIT_GAINED) has_patrol = false;
        if (index > 0 && mine.indices[index] <= mine.indices[index - 1]) ordered = false;
    }
    TEST_CHECK(counts, has_patrol && ordered, "a city filter keeps the events of the city in ledger order, not the faction or officer events");
    TEST_CHECK(counts, api->ledger_filter(e, 200, ENGINE_ANY_RESOURCE).count == 0, "an unknown city keeps nothing");
    engine_fixture_destroy(&setup.fixture);
}

internal void
feature_city_command_tests(test_counts *counts)
{
    feature_patrol_tests(counts);
    feature_conscript_tests(counts);
    feature_train_tests(counts);
    feature_merchant_tests(counts);
    feature_produce_tests(counts);
    feature_specialty_tests(counts);
    feature_ledger_filter_tests(counts);
}
