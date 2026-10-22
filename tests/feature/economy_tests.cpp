/* Feature tests of the production system (M3 economy; cities-and-facilities.md,
   calendar-and-turn.md; F-005, F-006, F-031), through engine_api with the
   real engine: a 12-month trace of one city that nobody touches, the caps,
   the level and adjacency multipliers, the durability of facilities, and
   the technique points and merit of a command. The amounts come from
   rules.tsv; most are assumed, and the ledger marks them. */

#define FEATURE_FACILITY_BARRACKS     2
#define FEATURE_FACILITY_SMITHY       3
#define FEATURE_FACILITY_STABLE       4
#define FEATURE_FACILITY_WORKSHOP     5
#define FEATURE_FACILITY_BLACK_MARKET 14
#define FEATURE_GOLD_CAP              100000    /* manual p.25 */
#define FEATURE_GRAIN_CAP             1000000   /* manual p.25 */
#define FEATURE_TECHNIQUE_PER_COMMAND 10        /* assumed */
#define FEATURE_MERIT_PER_COMMAND     50        /* save-file.md: +50 for each officer that does an order */
#define FEATURE_RECOVERY              100       /* assumed: durability each turn */

/* Develops a facility with up to three free officers, then ends turns until
   it is complete. Returns its slot, or ENGINE_ANY_SLOT if it did not start
   or did not complete in 12 turns. */
internal u8
feature_build(engine_api *api, u8 city, u8 facility, u8 force, u8 slot = ENGINE_ANY_SLOT)
{
    void *e = api->engine;
    api->set_city_gold(e, city, 50000);
    api->set_actions(e, force, 255);
    u16 ids[SIM_DEVELOP_OFFICERS] = { ENGINE_NO_OFFICER, ENGINE_NO_OFFICER, ENGINE_NO_OFFICER };
    feature_free_officers(api, city, ids, SIM_DEVELOP_OFFICERS);
    engine_command command = feature_develop_command(city, facility, slot, ids[0], ids[1], ids[2]);
    engine_check check = api->can_issue(e, &command);
    if (check.refusal != SIM_REFUSAL_NONE) return ENGINE_ANY_SLOT;
    api->command_develop(e, city, facility, ids, slot);
    for (u32 turn = 0; turn < 12 && api->facility_at(e, city, check.slot).state != SIM_SLOT_BUILT; ++turn)
    {
        feature_next_own_turn(api, force);
    }
    return api->facility_at(e, city, check.slot).state == SIM_SLOT_BUILT ? check.slot : ENGINE_ANY_SLOT;
}

/* Ends the turns of the faction until the next turn is day 1 of a month. */
internal void
feature_walk_to_month_end(engine_api *api, u8 force)
{
    for (u32 step = 0; step < 4 && api->date(api->engine).day != 21; ++step) feature_next_own_turn(api, force);
}

/* The sum of the deltas of the ledger events from "from" on that change a
   resource of the city. */
internal s64
feature_ledger_sum(engine_api *api, u8 city, u8 resource, u32 from)
{
    engine_ledger_view view = api->ledger_filter(api->engine, city, resource);
    s64 sum = 0;
    for (u32 index = 0; index < view.count; ++index)
    {
        if (view.indices[index] >= from) sum += api->ledger_at(api->engine, view.indices[index]).delta;
    }
    return sum;
}

/* The first event of a city with this kind from "from" on. */
internal b32
feature_find_city_event(engine_api *api, u8 city, sim_event_kind kind, u32 from, sim_event *found)
{
    engine_ledger_view view = api->ledger_filter(api->engine, city, ENGINE_ANY_RESOURCE);
    for (u32 index = 0; index < view.count; ++index)
    {
        if (view.indices[index] < from) continue;
        sim_event event = api->ledger_at(api->engine, view.indices[index]);
        if (event.kind == kind)
        {
            *found = event;
            return true;
        }
    }
    return false;
}

/* An owned city of an AI faction with officers and soldiers: nobody gives
   it a command, so only the calendar changes it. */
internal u8
feature_untouched_city(engine_api *api, u8 player)
{
    for (u32 city = 0; city < api->city_count(api->engine); ++city)
    {
        engine_city info = api->city_at(api->engine, city);
        if (info.owner_force_id == ENGINE_NO_FACTION || info.owner_force_id == player || info.troops < 1000) continue;
        if (feature_city_salaries(api, (u8)city) == 0) continue;
        return (u8)city;
    }
    return ENGINE_NO_CITY;
}

internal b32
feature_is_trace_kind(sim_event_kind kind)
{
    return kind == SIM_EVENT_GOLD_INCOME || kind == SIM_EVENT_SALARIES_PAID || kind == SIM_EVENT_GRAIN_INCOME ||
           kind == SIM_EVENT_GRAIN_CONSUMED;
}

/* The trace: 36 turns (12 months) of one city that nobody touches. Each
   turn has the monthly and seasonal events of the calendar in order, with
   the amounts of the forecast. The ledger explains each change of the gold
   and the grain, and the stocks never pass the caps. */
internal void
feature_trace_tests(test_counts *counts)
{
    engine_fixture f;
    engine_fixture_init(&f, 1, 65536);
    engine_api *api = &f.api;
    void *e = api->engine;
    api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
    u8 player = api->whose_turn(e);
    u8 city = feature_untouched_city(api, player);
    TEST_CHECK(counts, city != ENGINE_NO_CITY, "the sample has a city of an AI faction with officers and soldiers");
    if (city == ENGINE_NO_CITY) { engine_fixture_destroy(&f); return; }

    u32 start_mark = api->ledger_count(e);
    engine_city start = api->city_at(e, city);
    b32 order_ok = true, amounts_ok = true, explained = true, capped = true, forecast_ok = true;
    u32 month_payments = 0, season_payments = 0, consumptions = 0;
    for (u32 turn = 0; turn < 36; ++turn)
    {
        engine_city before = api->city_at(e, city);
        engine_income_forecast forecast = api->city_income_forecast(e, city);
        if (forecast.city_id != city || !forecast.assumed) forecast_ok = false;
        sim_income_forecast *amounts = &forecast.amounts;
        u32 mark = api->ledger_count(e);
        feature_next_turn(api);
        engine_date date = api->date(e);
        b32 month_start  = date.day == 1;
        b32 season_start = month_start && (date.month - 1) % 3 == 0;

        sim_event_kind expected[8];
        s32 expected_delta[8];
        u32 expected_count = 0;
        if (month_start && !amounts->gold_every_turn)    { expected[expected_count] = SIM_EVENT_GOLD_INCOME;    expected_delta[expected_count++] = (s32)amounts->gold_payment; }
        if (month_start && amounts->salaries)            { expected[expected_count] = SIM_EVENT_SALARIES_PAID;  expected_delta[expected_count++] = -(s32)amounts->salaries; }
        if (month_start && amounts->grain_every_month)   { expected[expected_count] = SIM_EVENT_GRAIN_INCOME;   expected_delta[expected_count++] = (s32)amounts->grain_payment; }
        if (season_start && !amounts->grain_every_month) { expected[expected_count] = SIM_EVENT_GRAIN_INCOME;   expected_delta[expected_count++] = (s32)amounts->grain_payment; }
        if (amounts->gold_every_turn)                    { expected[expected_count] = SIM_EVENT_GOLD_INCOME;    expected_delta[expected_count++] = (s32)amounts->gold_payment; }
        if (amounts->grain_per_turn)                     { expected[expected_count] = SIM_EVENT_GRAIN_CONSUMED; expected_delta[expected_count++] = -(s32)amounts->grain_per_turn; }

        engine_ledger_view view = api->ledger_filter(e, city, ENGINE_ANY_RESOURCE);
        u32 seen = 0;
        for (u32 index = 0; index < view.count; ++index)
        {
            if (view.indices[index] < mark) continue;
            sim_event event = api->ledger_at(e, view.indices[index]);
            if (!feature_is_trace_kind(event.kind)) continue;
            if (seen >= expected_count || event.kind != expected[seen]) order_ok = false;
            else if (event.delta != expected_delta[seen] || !event.assumed) amounts_ok = false;
            if (event.kind == SIM_EVENT_GOLD_INCOME && month_start) month_payments++;
            if (event.kind == SIM_EVENT_GRAIN_INCOME) season_payments++;
            if (event.kind == SIM_EVENT_GRAIN_CONSUMED) consumptions++;
            seen++;
        }
        if (seen != expected_count) order_ok = false;

        engine_city after = api->city_at(e, city);
        if ((s64)after.gold - (s64)before.gold != feature_ledger_sum(api, city, SIM_RESOURCE_GOLD, mark) ||
            (s64)after.food - (s64)before.food != feature_ledger_sum(api, city, SIM_RESOURCE_GRAIN, mark))
        {
            explained = false;
        }
        if (after.gold > FEATURE_GOLD_CAP || after.food > FEATURE_GRAIN_CAP) capped = false;
    }
    engine_city end = api->city_at(e, city);
    TEST_CHECK(counts, forecast_ok, "city_income_forecast answers for the city and marks the amounts as assumed");
    TEST_CHECK(counts, order_ok, "each turn has the gold income, the salaries, the grain income and the consumption in the calendar order");
    TEST_CHECK(counts, amounts_ok, "each income, salary and consumption has the amount of the forecast and the assumed mark");
    TEST_CHECK(counts, month_payments == 12 && season_payments == 4 && consumptions == 36,
               "12 months: 12 gold payments, 4 grain payments and 36 consumptions");
    TEST_CHECK(counts, explained, "the ledger explains each change of the gold and the grain of each turn");
    TEST_CHECK(counts, (s64)end.gold - (s64)start.gold == feature_ledger_sum(api, city, SIM_RESOURCE_GOLD, start_mark) &&
                       (s64)end.food - (s64)start.food == feature_ledger_sum(api, city, SIM_RESOURCE_GRAIN, start_mark),
               "the ledger explains the change of the gold and the grain over 12 months");
    TEST_CHECK(counts, capped, "the gold and the grain never pass the caps of 100,000 and 1,000,000");
    TEST_CHECK(counts, feature_count_events(api, SIM_EVENT_MONTH_STARTED, start_mark) == 12 &&
                       feature_count_events(api, SIM_EVENT_SEASON_STARTED, start_mark) == 4 &&
                       feature_count_events(api, SIM_EVENT_YEAR_STARTED, start_mark) == 1,
               "12 months have 12 month starts, 4 season starts and 1 year start");
    TEST_CHECK(counts, api->city_income_forecast(e, 200).city_id == ENGINE_NO_CITY, "the forecast of an unknown city is no city");

    engine_fixture_destroy(&f);
}

/* The caps of the gold and the grain, the soldiers that leave a city
   without grain, and the unpaid salaries. */
internal void
feature_cap_tests(test_counts *counts)
{
    engine_fixture f;
    engine_fixture_init(&f, 1, 65536);
    engine_api *api = &f.api;
    void *e = api->engine;
    api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
    u8 city = feature_busiest_city(api);
    u8 force = api->city_at(e, city).owner_force_id;
    feature_play_as(api, force);

    feature_walk_to_month_end(api, force);
    api->set_city_resource(e, city, SIM_RESOURCE_GOLD, 0, FEATURE_GOLD_CAP - 10);
    engine_income_forecast forecast = api->city_income_forecast(e, city);
    u32 mark = api->ledger_count(e);
    feature_next_own_turn(api, force);
    sim_event over = {};
    TEST_CHECK(counts, feature_find_city_event(api, city, SIM_EVENT_GOLD_OVER_CAP, mark, &over) &&
                       over.delta == -(s32)(forecast.amounts.gold_payment - 10), "the gold above the cap of 100,000 is lost, and the ledger says so");
    TEST_CHECK(counts, api->city_at(e, city).gold == FEATURE_GOLD_CAP - forecast.amounts.salaries,
               "the city has the cap of 100,000 gold, then it pays the salaries");

    for (u32 step = 0; step < 12 && !(api->date(e).day == 21 && api->date(e).month % 3 == 0); ++step) feature_next_own_turn(api, force);
    TEST_CHECK(counts, api->date(e).day == 21 && api->date(e).month % 3 == 0, "the walk reaches the last turn of a season");
    api->set_city_resource(e, city, SIM_RESOURCE_GRAIN, 0, FEATURE_GRAIN_CAP - 10);
    forecast = api->city_income_forecast(e, city);
    mark = api->ledger_count(e);
    feature_next_own_turn(api, force);
    TEST_CHECK(counts, feature_find_city_event(api, city, SIM_EVENT_GRAIN_OVER_CAP, mark, &over) && over.delta < 0,
               "the grain above the cap of 1,000,000 is lost, and the ledger says so");
    TEST_CHECK(counts, api->city_at(e, city).food == FEATURE_GRAIN_CAP - forecast.amounts.grain_per_turn,
               "the city has the cap of 1,000,000 grain, then the soldiers eat");

    /* No grain on day 11: soldiers leave (calendar-and-turn.md; the number is assumed). */
    api->set_city_resource(e, city, SIM_RESOURCE_GRAIN, 0, 0);
    api->set_city_resource(e, city, SIM_RESOURCE_SOLDIERS, 0, 5000);
    mark = api->ledger_count(e);
    feature_next_own_turn(api, force);
    sim_event deserted = {};
    TEST_CHECK(counts, feature_find_city_event(api, city, SIM_EVENT_SOLDIERS_DESERTED, mark, &deserted) && deserted.assumed &&
                       deserted.delta == -5000 && api->city_economy(e, city).soldiers == 0,
               "a city without grain loses the soldiers that the missing grain feeds (assumed rule)");

    /* No gold: the salaries are not paid in full. */
    feature_walk_to_month_end(api, force);
    api->set_city_resource(e, city, SIM_RESOURCE_GOLD, 0, 0);
    forecast = api->city_income_forecast(e, city);
    mark = api->ledger_count(e);
    feature_next_own_turn(api, force);
    sim_event unpaid = {};
    TEST_CHECK(counts, forecast.amounts.salaries > forecast.amounts.gold_payment, "the busiest city pays more salaries than its income");
    TEST_CHECK(counts, feature_find_city_event(api, city, SIM_EVENT_SALARIES_UNPAID, mark, &unpaid) &&
                       unpaid.detail == forecast.amounts.salaries - forecast.amounts.gold_payment && unpaid.delta == 0,
               "a city with too little gold pays what it has; the ledger names the unpaid gold");
    TEST_CHECK(counts, api->city_at(e, city).gold == 0, "the gold of the city does not go below 0");
    engine_fixture_destroy(&f);
}

/* The level of a merged facility and the PK markets (manual p.28, p.29). */
internal void
feature_multiplier_tests(test_counts *counts)
{
    engine_fixture f;
    engine_fixture_init(&f, 1, 65536);
    engine_api *api = &f.api;
    void *e = api->engine;
    api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
    u8 city = feature_busiest_city(api);
    u8 force = api->city_at(e, city).owner_force_id;
    feature_play_as(api, force);
    feature_next_own_turn(api, force);   /* G2-004: the save marks officers as acted; the builds below need a fresh turn */
    u32 market_value = api->facility_kind_at(e, FEATURE_FACILITY_MARKET).value;
    TEST_CHECK(counts, api->facility_kind_at(e, FEATURE_FACILITY_GREAT_MARKET).value == 3 * market_value &&
                       api->facility_kind_at(e, FEATURE_FACILITY_FISH_MARKET).value == 2 * market_value &&
                       api->facility_kind_at(e, FEATURE_FACILITY_BLACK_MARKET).value * 10 == market_value * 8,
               "the great market gives 3 times, the fish market 2 times and the black market 0.8 times a market");

    u8 mint = feature_build(api, city, FEATURE_FACILITY_MINT, force);
    TEST_CHECK(counts, mint != ENGINE_ANY_SLOT, "a mint is built");
    u8 near_slot = feature_slot_at_distance(api, city, mint, 1);
    u8 far_slot  = feature_slot_at_distance(api, city, mint, 3);
    u8 near_market = feature_build(api, city, FEATURE_FACILITY_MARKET, force, near_slot);
    u8 far_market  = feature_build(api, city, FEATURE_FACILITY_MARKET, force, far_slot);
    TEST_CHECK(counts, near_market != ENGINE_ANY_SLOT && far_market != ENGINE_ANY_SLOT, "a market next to the mint and one away are built");

    TEST_CHECK(counts, api->set_facility_level(e, city, far_market, 2) && api->facility_at(e, city, far_market).level == 2 &&
                       api->facility_at(e, city, far_market).yield == market_value * 6 / 5, "a level-2 market gives 1.2 times");
    TEST_CHECK(counts, api->set_facility_level(e, city, far_market, 3) && api->facility_at(e, city, far_market).yield == market_value * 3 / 2,
               "a level-3 market gives 1.5 times");
    TEST_CHECK(counts, api->set_facility_level(e, city, near_market, 3) &&
                       api->facility_at(e, city, near_market).yield == market_value * 225 / 100,
               "a level-3 market next to a mint gives 1.5 times 1.5");
    TEST_CHECK(counts, !api->set_facility_level(e, city, mint, 2) && !api->set_facility_level(e, city, far_market, 4) &&
                       !api->set_facility_level(e, city, 200, 2), "only a market, farm, barracks, smithy or stable has a level of 1 to 3");
    engine_income_forecast forecast = api->city_income_forecast(e, city);
    TEST_CHECK(counts, forecast.amounts.gold_facilities == market_value * 3 / 2 + market_value * 225 / 100,
               "the forecast adds the outputs of the facilities");
    TEST_CHECK(counts, forecast.amounts.gold_month == sim_percent_of(sim_percent_of(forecast.amounts.gold_base + forecast.amounts.gold_facilities,
                                                                                    forecast.amounts.order_percent),
                                                                     forecast.amounts.gold_skill_percent) + forecast.amounts.gold_bases,
               "the gold of a month is the initial income plus the facilities, times the order percent and the skill percent, plus the bases (M8)");
    TEST_CHECK(counts, api->city_slots(e, city).gold_income == forecast.amounts.gold_month, "the City panel shows the gold of the forecast");

    u8 black = feature_build(api, city, FEATURE_FACILITY_BLACK_MARKET, force);
    TEST_CHECK(counts, black != ENGINE_ANY_SLOT && api->facility_at(e, city, black).yield == market_value * 8 / 10,
               "a black market gives 0.8 times a market, with no level and no mint bonus");
    engine_fixture_destroy(&f);
}

/* Durability: recovery each turn with a governor and no enemy near;
   destruction at 0 (cities-and-facilities.md, F-031). */
internal void
feature_durability_tests(test_counts *counts)
{
    engine_fixture f;
    engine_fixture_init(&f, 1, 65536);
    engine_api *api = &f.api;
    void *e = api->engine;
    api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
    u8 city = feature_busiest_city(api);
    u8 force = api->city_at(e, city).owner_force_id;
    feature_play_as(api, force);

    u8 farm = feature_build(api, city, FEATURE_FACILITY_FARM, force);
    engine_facility slot = api->facility_at(e, city, farm);
    TEST_CHECK(counts, farm != ENGINE_ANY_SLOT && slot.durability == 500 && slot.durability_max == 500,
               "a complete farm has its durability of 500");
    u32 mark = api->ledger_count(e);
    engine_command_result damage = api->damage_facility(e, city, farm, 300);
    sim_event damaged = {};
    TEST_CHECK(counts, damage.refusal == SIM_REFUSAL_NONE && damage.command_id != 0 && api->facility_at(e, city, farm).durability == 200,
               "the test command takes 300 durability");
    TEST_CHECK(counts, feature_find_city_event(api, city, SIM_EVENT_FACILITY_DAMAGED, mark, &damaged) && damaged.delta == -300 &&
                       damaged.detail == farm && damaged.cause == damage.command_id, "the ledger has the damage with the slot");
    mark = api->ledger_count(e);
    feature_next_own_turn(api, force);
    sim_event repaired = {};
    TEST_CHECK(counts, api->facility_at(e, city, farm).durability == 200 + FEATURE_RECOVERY &&
                       feature_find_city_event(api, city, SIM_EVENT_FACILITY_REPAIRED, mark, &repaired) &&
                       repaired.delta == FEATURE_RECOVERY && repaired.assumed && repaired.detail == farm,
               "with a governor and no enemy near, the facility recovers durability each turn (amount assumed)");
    for (u32 turn = 0; turn < 3; ++turn) feature_next_own_turn(api, force);
    TEST_CHECK(counts, api->facility_at(e, city, farm).durability == 500, "the recovery stops at the durability of the facility");

    u32 grain_before = api->city_income_forecast(e, city).amounts.grain_facilities;
    u32 used_before  = api->city_slots(e, city).used;
    mark = api->ledger_count(e);
    damage = api->damage_facility(e, city, farm, 9999);
    sim_event destroyed = {};
    TEST_CHECK(counts, damage.refusal == SIM_REFUSAL_NONE && api->facility_at(e, city, farm).state == SIM_SLOT_EMPTY &&
                       api->facility_at(e, city, farm).facility == ENGINE_NO_FACILITY,
               "at durability 0 the facility is destroyed and the slot is empty");
    TEST_CHECK(counts, feature_find_city_event(api, city, SIM_EVENT_FACILITY_DESTROYED, mark, &destroyed) &&
                       destroyed.delta == FEATURE_FACILITY_FARM && destroyed.detail == farm, "the ledger has the destruction");
    TEST_CHECK(counts, api->city_slots(e, city).used == used_before - 1 &&
                       api->city_income_forecast(e, city).amounts.grain_facilities == grain_before - 250,
               "the destroyed farm gives no grain and frees its slot");
    TEST_CHECK(counts, api->damage_facility(e, city, farm, 10).refusal == SIM_REFUSAL_NO_FACILITY &&
                       api->damage_facility(e, 200, 0, 10).refusal == SIM_REFUSAL_NO_CITY,
               "the test command refuses an empty slot and an unknown city");

    /* A facility under construction: its officers are free when it is destroyed. */
    api->set_city_gold(e, city, 50000);
    api->set_actions(e, force, 255);
    u16 ids[SIM_DEVELOP_OFFICERS] = { ENGINE_NO_OFFICER, ENGINE_NO_OFFICER, ENGINE_NO_OFFICER };
    feature_free_officers(api, city, ids, 1);
    engine_command develop = feature_develop_command(city, FEATURE_FACILITY_MARKET, ENGINE_ANY_SLOT, ids[0], ENGINE_NO_OFFICER, ENGINE_NO_OFFICER);
    engine_check check = api->can_issue(e, &develop);
    api->command_develop(e, city, FEATURE_FACILITY_MARKET, ids, ENGINE_ANY_SLOT);
    api->damage_facility(e, city, check.slot, 9999);
    engine_officer worker = {};
    TEST_CHECK(counts, feature_officer_by_id(api, ids[0], &worker) && worker.job_city == ENGINE_NO_CITY &&
                       api->facility_at(e, city, check.slot).state == SIM_SLOT_EMPTY,
               "a destroyed facility under construction frees its officers");

    /* An enemy within 2 hexes stops the recovery. */
    u8 smithy = feature_build(api, city, FEATURE_FACILITY_SMITHY, force);
    api->damage_facility(e, city, smithy, 300);
    engine_city_slots site = api->city_slots(e, city);
    api->place_unit(e, force == 1 ? 2 : 1, site.q + 2, site.r);
    feature_next_own_turn(api, force);
    TEST_CHECK(counts, smithy != ENGINE_ANY_SLOT && api->facility_at(e, city, smithy).durability == 800 - 300,
               "an enemy within 2 hexes stops the recovery");
    engine_fixture_destroy(&f);

    /* A city without a governor does not recover. */
    engine_fixture g;
    engine_fixture_init(&g, 1, 65536);
    api = &g.api;
    e = api->engine;
    api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
    city = feature_busiest_city(api);
    force = api->city_at(e, city).owner_force_id;
    feature_play_as(api, force);
    u8 market = feature_build(api, city, FEATURE_FACILITY_MARKET, force);
    api->damage_facility(e, city, market, 200);
    u8 other = ENGINE_NO_FACTION;
    for (u32 index = 0; index < api->faction_count(e) && other == ENGINE_NO_FACTION; ++index)
    {
        if (api->faction_at(e, index).force_id != force) other = api->faction_at(e, index).force_id;
    }
    api->set_city_owner(e, city, other);
    TEST_CHECK(counts, api->city_slots(e, city).governor_id == ENGINE_NO_OFFICER, "a city of a force with no officer there has no governor");
    feature_next_turn(api);
    TEST_CHECK(counts, api->facility_at(e, city, market).durability == 300, "a city without a governor does not recover durability");
    engine_fixture_destroy(&g);
}

/* Each command gives technique points to the faction and merit to each
   officer (calendar-and-turn.md, save-file.md). */
internal void
feature_technique_merit_tests(test_counts *counts)
{
    engine_fixture f;
    engine_fixture_init(&f, 1, 65536);
    engine_api *api = &f.api;
    void *e = api->engine;
    api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
    u8 city = feature_busiest_city(api);
    u8 force = api->city_at(e, city).owner_force_id;
    feature_play_as(api, force);
    api->set_city_gold(e, city, 50000);
    api->set_actions(e, force, 255);

    u16 ids[SIM_DEVELOP_OFFICERS] = { ENGINE_NO_OFFICER, ENGINE_NO_OFFICER, ENGINE_NO_OFFICER };
    feature_free_officers(api, city, ids, 2);
    engine_officer first = {}, second = {};
    feature_officer_by_id(api, ids[0], &first);
    feature_officer_by_id(api, ids[1], &second);
    u32 points = api->technique_points(e, force);
    u32 mark = api->ledger_count(e);
    u16 develop_ids[SIM_DEVELOP_OFFICERS] = { ids[0], ENGINE_NO_OFFICER, ENGINE_NO_OFFICER };
    engine_command_result develop = api->command_develop(e, city, FEATURE_FACILITY_MARKET, develop_ids, ENGINE_ANY_SLOT);
    sim_event gained = {};
    TEST_CHECK(counts, develop.refusal == SIM_REFUSAL_NONE && api->technique_points(e, force) == points + FEATURE_TECHNIQUE_PER_COMMAND &&
                       feature_find_event(api, SIM_EVENT_TECHNIQUE_GAINED, mark, &gained) && gained.faction == force &&
                       gained.assumed && gained.cause == develop.command_id,
               "a develop gives technique points to the faction (amount assumed)");
    engine_officer after = {};
    TEST_CHECK(counts, feature_officer_by_id(api, ids[0], &after) && after.merit == first.merit + FEATURE_MERIT_PER_COMMAND &&
                       feature_count_events(api, SIM_EVENT_MERIT_GAINED, mark) == 1,
               "the officer of a command gains 50 merit (save-file.md)");

    points = api->technique_points(e, force);
    engine_command_result sale = api->command_merchant(e, city, ids[1], SIM_MERCHANT_BUY, 100);
    TEST_CHECK(counts, sale.refusal == SIM_REFUSAL_NONE && api->technique_points(e, force) == points,
               "the merchant gives no technique points (city-commands.md)");
    TEST_CHECK(counts, feature_officer_by_id(api, ids[1], &after) && after.merit == second.merit + FEATURE_MERIT_PER_COMMAND,
               "the merchant gives merit");
    TEST_CHECK(counts, api->technique_points(e, 200) == 0, "a force without points has 0");
    engine_fixture_destroy(&f);
}

internal void
feature_economy_tests(test_counts *counts)
{
    feature_trace_tests(counts);
    feature_cap_tests(counts);
    feature_multiplier_tests(counts);
    feature_durability_tests(counts);
    feature_technique_merit_tests(counts);
}
