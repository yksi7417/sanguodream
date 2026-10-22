/* Feature tests of the calendar, the turn order and the action points
   (calendar-and-turn.md, F-002, F-003, F-004), through engine_api with the
   real engine. The numbers of rules.tsv that the tests use: officer.salary
   10 and grain.soldiers 100 (both assumed). */

#define FEATURE_SALARY         10
#define FEATURE_GRAIN_SOLDIERS 40    /* grain.soldiers, observed (fixtures/grain-consumption.tsv) */
/* The header of sample.s11 names the player's ruler with the Big5 bytes
   BC42 AAED (劉表; save_dump.exe prints player_ruler_name_big5_hex). */
global_variable const u8 feature_player_family[] = { 0xBC, 0x42 };
global_variable const u8 feature_player_given[]  = { 0xAA, 0xED };

/* The force of the ruler with the name of the header, or ENGINE_NO_FACTION. */
internal u8
feature_player_force(engine_api *api)
{
    u32 count = api->officer_count(api->engine);
    for (u32 index = 0; index < count; ++index)
    {
        engine_officer officer = api->officer_at(api->engine, index);
        if (officer.status == 0 && memcmp(officer.family_name, feature_player_family, 2) == 0 && officer.family_name[2] == 0 &&
            memcmp(officer.given_name, feature_player_given, 2) == 0 && officer.given_name[2] == 0)
        {
            return officer.force_id;
        }
    }
    return ENGINE_NO_FACTION;
}

internal u32
feature_count_events(engine_api *api, sim_event_kind kind, u32 from)
{
    u32 found = 0;
    u32 count = api->ledger_count(api->engine);
    for (u32 index = from; index < count; ++index)
    {
        if (api->ledger_at(api->engine, index).kind == kind) found++;
    }
    return found;
}

/* The first event of a kind at or after "from". */
internal b32
feature_find_event(engine_api *api, sim_event_kind kind, u32 from, sim_event *found)
{
    u32 count = api->ledger_count(api->engine);
    for (u32 index = from; index < count; ++index)
    {
        sim_event event = api->ledger_at(api->engine, index);
        if (event.kind == kind)
        {
            *found = event;
            return true;
        }
    }
    return false;
}

internal b32
feature_date_is(engine_date date, u16 year, u8 month, u8 day)
{
    return date.year == year && date.month == month && date.day == day;
}

internal b32
feature_officer_by_id(engine_api *api, u16 officer_id, engine_officer *found)
{
    u32 count = api->officer_count(api->engine);
    for (u32 index = 0; index < count; ++index)
    {
        engine_officer officer = api->officer_at(api->engine, index);
        if (officer.officer_id == officer_id)
        {
            *found = officer;
            return true;
        }
    }
    return false;
}

/* The salaries of a city each month: its active officers of the owner. */
internal u32
feature_city_salaries(engine_api *api, u8 city_id)
{
    engine_city city = api->city_at(api->engine, city_id);
    u32 paid = 0;
    u32 count = api->officer_count(api->engine);
    for (u32 index = 0; index < count; ++index)
    {
        engine_officer officer = api->officer_at(api->engine, index);
        if (officer.status <= 3 && officer.force_id == city.owner_force_id && officer.base_location == city_id) paid++;
    }
    return paid * FEATURE_SALARY;
}

/* Ends faction turns until a new turn starts. Returns false if the date
   does not move in 300 commands. */
internal b32
feature_next_turn(engine_api *api)
{
    s64 before = api->date(api->engine).days_since_start;
    for (u32 step = 0; step < 300; ++step)
    {
        api->end_turn(api->engine);
        if (api->date(api->engine).days_since_start != before) return true;
    }
    return false;
}

/* Makes a force human and ends faction turns until it acts. */
internal b32
feature_play_as(engine_api *api, u8 force_id)
{
    api->set_faction_human(api->engine, force_id, true);
    for (u32 step = 0; step < 300; ++step)
    {
        if (api->whose_turn(api->engine) == force_id) return true;
        api->end_turn(api->engine);
    }
    return false;
}

/* After the faction acted: end turns until it acts again in the next turn. */
internal b32
feature_next_own_turn(engine_api *api, u8 force_id)
{
    return feature_next_turn(api) && feature_play_as(api, force_id);
}

internal void
feature_calendar_tests(test_counts *counts)
{
    engine_fixture f;
    engine_fixture_init(&f, 1, 4096);
    engine_api *api = &f.api;
    void *e = api->engine;

    engine_date date = api->date(e);
    TEST_CHECK(counts, feature_date_is(date, SIM_DEFAULT_START_YEAR, 1, 1) && date.season == CALENDAR_SPRING &&
                       date.days_since_start == 0, "a new game starts on 1 January of the default year, in spring");
    TEST_CHECK(counts, api->whose_turn(e) == ENGINE_NO_FACTION && api->faction_count(e) == 0,
               "a new game without a save has no factions");

    api->end_turn(e);
    date = api->date(e);
    TEST_CHECK(counts, feature_date_is(date, SIM_DEFAULT_START_YEAR, 1, 11) && date.days_since_start == 10,
               "without factions, end_turn moves the date one turn of 10 days");
    TEST_CHECK(counts, api->ledger_count(e) == 1 && api->ledger_at(e, 0).kind == SIM_EVENT_TURN_ADVANCED,
               "a turn inside a month ledgers only the turn");

    api->advance_turns(e, 2);
    date = api->date(e);
    sim_event month = {};
    TEST_CHECK(counts, feature_date_is(date, SIM_DEFAULT_START_YEAR, 2, 1) && date.days_since_start == 30,
               "three turns are one month");
    TEST_CHECK(counts, feature_find_event(api, SIM_EVENT_MONTH_STARTED, 0, &month) && month.delta == 2 && month.turn == 3,
               "the start of a month is a named event with the month");
    TEST_CHECK(counts, feature_count_events(api, SIM_EVENT_HOOK_BANDITS, 0) == 1, "each month start runs the bandit hook");

    api->ledger_clear(e);
    api->advance_turns(e, 33);
    date = api->date(e);
    TEST_CHECK(counts, feature_date_is(date, SIM_DEFAULT_START_YEAR + 1, 1, 1) && date.days_since_start == 360 &&
                       date.season == CALENDAR_SPRING, "36 turns are one year, and the year starts in spring");
    sim_event year = {}, season = {};
    TEST_CHECK(counts, feature_find_event(api, SIM_EVENT_YEAR_STARTED, 0, &year) && year.delta == SIM_DEFAULT_START_YEAR + 1,
               "the start of a year is a named event with the year");
    TEST_CHECK(counts, feature_find_event(api, SIM_EVENT_SEASON_STARTED, 0, &season) && season.delta == CALENDAR_SUMMER,
               "the first season start of the walk is summer (month 4)");
    TEST_CHECK(counts, feature_count_events(api, SIM_EVENT_MONTH_STARTED, 0) == 11 &&
                       feature_count_events(api, SIM_EVENT_SEASON_STARTED, 0) == 4,
               "from 1 February to 1 January: 11 month starts and 4 season starts");
    TEST_CHECK(counts, feature_count_events(api, SIM_EVENT_HOOK_LOCUSTS, 0) == 3 &&
                       feature_count_events(api, SIM_EVENT_HOOK_PLAGUE, 0) == 4 &&
                       feature_count_events(api, SIM_EVENT_HOOK_GOOD_HARVEST, 0) == 1,
               "locusts in months 1, 4, 7; plague in months 1, 4, 7, 10; a good harvest in month 7");
    TEST_CHECK(counts, feature_count_events(api, SIM_EVENT_HOOK_OFFICERS_AGE, 0) == 1 &&
                       feature_count_events(api, SIM_EVENT_HOOK_NEW_OFFICERS, 0) == 1,
               "month 1 runs the hooks of the officer age and the new officers");

    engine_fixture_destroy(&f);
}

/* The sample save: the date, the player and the factions. */
internal void
feature_turn_order_sample_tests(test_counts *counts)
{
    engine_fixture f;
    engine_fixture_init(&f, 1, 65536);
    engine_api *api = &f.api;
    void *e = api->engine;

    engine_load_result loaded = api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
    TEST_CHECK(counts, loaded.status == ENGINE_LOAD_OK, "the sample loads");
    engine_date date = api->date(e);
    TEST_CHECK(counts, feature_date_is(date, 197, 10, 1) && date.days_since_start == 1200 && date.season == CALENDAR_WINTER,
               "G2-001: a load sets the calendar to the scenario start plus the turn counter of the header");
    TEST_CHECK(counts, api->ledger_at(e, 0).kind == SIM_EVENT_SAVE_LOADED, "the first event of a load is save_loaded");

    u32 factions = api->faction_count(e);
    b32 sorted = factions > 1, counted = true;
    for (u32 index = 0; index < factions; ++index)
    {
        engine_faction faction = api->faction_at(e, index);
        if (faction.city_count == 0) counted = false;
        if (index > 0 && api->faction_at(e, index - 1).city_count > faction.city_count) sorted = false;
    }
    TEST_CHECK(counts, counted, "each faction of the order owns a city");
    TEST_CHECK(counts, sorted, "factions with fewer cities act first");
    TEST_CHECK(counts, api->faction_at(e, factions).force_id == ENGINE_NO_FACTION, "faction_at out of range is no faction");

    u8 player_force = feature_player_force(api);
    TEST_CHECK(counts, player_force != ENGINE_NO_FACTION, "the ruler that the header names is in the save");
    TEST_CHECK(counts, api->whose_turn(e) == player_force,
               "after a load, the faction of the ruler that the header names acts");
    u32 player_place = factions;
    for (u32 index = 0; index < factions; ++index)
    {
        if (api->faction_at(e, index).force_id == player_force) player_place = index;
    }
    TEST_CHECK(counts, player_place < factions && api->faction_at(e, player_place).human, "the player's faction is human");
    /* G2-003: the original saves in the turn of the player, so the load
       runs no AI command and recovers no action points. */
    TEST_CHECK(counts, feature_count_events(api, SIM_EVENT_AI_PASSED, 0) == 0 &&
                       feature_count_events(api, SIM_EVENT_ACTION_POINTS_RECOVERED, 0) == 0 &&
                       feature_count_events(api, SIM_EVENT_ACTION_POINTS_SPENT, 0) == 0,
               "G2-003: no faction acts and no pool recovers at the load");
    sim_event started = {};
    TEST_CHECK(counts, feature_find_event(api, SIM_EVENT_FACTION_TURN_STARTED, 0, &started) && started.faction == player_force &&
                       feature_count_events(api, SIM_EVENT_FACTION_TURN_STARTED, 0) == 1,
               "G2-003: the load starts the turn of the player at once");
    TEST_CHECK(counts, api->city_at(e, 31).troops == 7773 && api->city_at(e, 36).troops == 8925,
               "G2-003: the cities keep the soldiers of the file after the load");

    /* G2-002: the pool and the technique points of the save (table A +12, table B +61). */
    engine_faction player = api->faction_at(e, player_place);
    engine_officer ruler = {};
    TEST_CHECK(counts, feature_officer_by_id(api, player.leader_officer_id, &ruler) && ruler.status == 0 &&
                       ruler.force_id == player_force, "the leader of corps 1 is the ruler of the faction");
    TEST_CHECK(counts, api->actions_left(e, player_force) == 62 && player.action_points == 62 &&
                       api->technique_points(e, player_force) == 23,
               "G2-002: the pool and the technique points come from the save");

    /* G2-004: the acted flags of the officers (+148 bit 0) and of the units they lead. */
    engine_officer acted_officer = {}, free_officer = {};
    TEST_CHECK(counts, feature_officer_by_id(api, 110, &acted_officer) && acted_officer.acted &&
                       feature_officer_by_id(api, 636, &free_officer) && !free_officer.acted,
               "G2-004: an officer with the acted flag of the save has acted, the others have not");

    /* The recovery: the higher of leadership and charm of the ruler (assumed, Q-019, Q-141). */
    u32 recovery = Maximum(ruler.leadership, ruler.charm);
    u32 before_end = api->ledger_count(e);
    api->end_turn(e);
    sim_event ended = {}, recovered = {};
    TEST_CHECK(counts, feature_find_event(api, SIM_EVENT_FACTION_TURN_ENDED, before_end, &ended) &&
                       ended.faction == player_force, "proceed ends the turn of the player");
    TEST_CHECK(counts, api->date(e).days_since_start == 1210 && api->whose_turn(e) == player_force,
               "after the player and the AI factions, the next turn starts and the player acts again");
    TEST_CHECK(counts, api->actions_left(e, player_force) == Minimum(62 + recovery, 255u),
               "the pool recovers at the start of each turn by the higher of leadership and charm (assumed)");
    TEST_CHECK(counts, feature_find_event(api, SIM_EVENT_ACTION_POINTS_RECOVERED, before_end, &recovered) && recovered.assumed,
               "the ledger marks the recovery as assumed");
    for (u32 turn = 0; turn < 6; ++turn) api->end_turn(e);
    TEST_CHECK(counts, api->actions_left(e, player_force) == 255, "the pool stops at its cap of 255");

    /* Gold and grain on the calendar: walk to the last turn before a season
       starts, then compare the city before and after one more turn. */
    u32 steps = 0;
    while (!(api->date(e).day == 21 && api->date(e).month % 3 == 0) && steps++ < 100) api->end_turn(e);
    engine_date last = api->date(e);
    engine_city before = api->city_at(e, 36);
    engine_city_slots slots = api->city_slots(e, 36);
    u32 salaries = feature_city_salaries(api, 36);
    u32 mark = api->ledger_count(e);
    api->end_turn(e);
    engine_city after = api->city_at(e, 36);
    TEST_CHECK(counts, api->date(e).day == 1 && api->date(e).month == last.month % 12 + 1 &&
                       (api->date(e).month - 1) % 3 == 0, "the walk reaches day 1 of the first month of a season");
    TEST_CHECK(counts, (s64)after.gold - (s64)before.gold == (s64)slots.gold_income - (s64)salaries,
               "on day 1 the city gains its gold income and pays the salaries of its officers");
    TEST_CHECK(counts, (s64)after.food - (s64)before.food == (s64)slots.grain_income - (s64)(before.troops / FEATURE_GRAIN_SOLDIERS),
               "at a season start the city gains its grain income, and each turn it eats one grain for each 40 soldiers");
    sim_event income = {}, grain = {}, eaten = {}, paid = {};
    TEST_CHECK(counts, feature_find_event(api, SIM_EVENT_GOLD_INCOME, mark, &income) && income.assumed &&
                       feature_find_event(api, SIM_EVENT_SALARIES_PAID, mark, &paid) && paid.assumed && paid.delta < 0,
               "the ledger has the gold income and the salaries, marked as assumed");
    TEST_CHECK(counts, feature_find_event(api, SIM_EVENT_GRAIN_INCOME, mark, &grain) && grain.assumed &&
                       feature_find_event(api, SIM_EVENT_GRAIN_CONSUMED, mark, &eaten) && eaten.assumed && eaten.delta < 0,
               "the ledger has the grain income and the consumption, marked as assumed");

    engine_fixture_destroy(&f);
}

/* The turn order with three factions, set up through the editor commands. */
internal void
feature_turn_order_three_factions_tests(test_counts *counts)
{
    engine_fixture f;
    engine_fixture_init(&f, 1, 65536);
    engine_api *api = &f.api;
    void *e = api->engine;
    api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);

    /* Three forces that have a ruler: the first three of the order. */
    u8 big = api->faction_at(e, 0).force_id, middle = api->faction_at(e, 1).force_id, small = api->faction_at(e, 2).force_id;
    for (u32 city = 0; city < SPEC_CITY_COUNT; ++city)
    {
        u8 owner = city < 20 ? big : (city < 32 ? middle : small);
        api->set_city_owner(e, (u8)city, owner);
    }
    u32 factions_before = api->faction_count(e);
    TEST_CHECK(counts, factions_before > 3, "set_city_owner changes the order only at the next turn");
    for (u32 index = 0; index < factions_before; ++index) api->set_faction_human(e, api->faction_at(e, index).force_id, false);
    api->set_faction_human(e, big, true);
    api->set_faction_human(e, middle, true);
    api->set_faction_human(e, small, true);

    TEST_CHECK(counts, feature_next_turn(api), "the next turn starts");
    TEST_CHECK(counts, api->faction_count(e) == 3, "the next turn has the three factions that own cities");
    TEST_CHECK(counts, api->faction_at(e, 0).force_id == small && api->faction_at(e, 1).force_id == middle &&
                       api->faction_at(e, 2).force_id == big, "10 cities act before 12 cities, and 12 before 20");
    TEST_CHECK(counts, api->faction_at(e, 0).city_count == 10 && api->faction_at(e, 2).city_count == 20,
               "each faction counts its cities");

    u32 mark = api->ledger_count(e);
    TEST_CHECK(counts, api->whose_turn(e) == small, "the smallest faction acts first");
    api->end_turn(e);
    TEST_CHECK(counts, api->whose_turn(e) == middle && api->faction_at(e, 0).acted, "proceed passes the turn to the next faction");
    api->end_turn(e);
    TEST_CHECK(counts, api->whose_turn(e) == big, "the largest faction acts last");
    s64 days = api->date(e).days_since_start;
    api->end_turn(e);
    TEST_CHECK(counts, api->date(e).days_since_start == days + 10 && api->whose_turn(e) == small,
               "after the last faction, the date advances and the order starts again");
    TEST_CHECK(counts, feature_count_events(api, SIM_EVENT_FACTION_TURN_ENDED, mark) == 3 &&
                       feature_count_events(api, SIM_EVENT_FACTION_TURN_STARTED, mark) == 3,
               "two more faction turns start in this turn and one in the next; three end");
    TEST_CHECK(counts, feature_count_events(api, SIM_EVENT_AI_PASSED, mark) == 0, "human factions do not pass");

    /* A tie in cities: fewer officers act first (knowledge/turn-structure.md, unverified). */
    for (u32 city = 0; city < SPEC_CITY_COUNT; ++city)
    {
        u8 owner = city < 16 ? big : (city < 29 ? middle : small);
        api->set_city_owner(e, (u8)city, owner);
    }
    TEST_CHECK(counts, feature_next_turn(api), "the next turn starts after the change");
    engine_faction first = api->faction_at(e, 0), second = api->faction_at(e, 1), last = api->faction_at(e, 2);
    TEST_CHECK(counts, first.city_count == 13 && second.city_count == 13 && last.force_id == big,
               "two factions with 13 cities act before the faction with 16");
    TEST_CHECK(counts, first.officer_count < second.officer_count ||
                       (first.officer_count == second.officer_count &&
                        (first.troops < second.troops || (first.troops == second.troops && first.force_id < second.force_id))),
               "with equal cities, fewer officers act first, then fewer troops (then the smaller force id, assumed)");

    /* An AI faction passes without a command. */
    api->set_faction_human(e, first.force_id, false);
    mark = api->ledger_count(e);
    TEST_CHECK(counts, feature_next_turn(api), "the next turn starts");
    TEST_CHECK(counts, feature_count_events(api, SIM_EVENT_AI_PASSED, mark) == 1 && api->whose_turn(e) == second.force_id,
               "the AI faction passes, and the next human faction acts");

    engine_fixture_destroy(&f);
}

/* G2-028: the month start takes no soldiers and no public order from a
   city. The clone of the report rolled a plague at a wrong date (G2-001).
   From the load of the sample at 197/10/1, three turns reach 197/11/1.
   No disaster can start in month 11 (disasters.tsv). */
internal void
feature_month_start_sample_tests(test_counts *counts)
{
    engine_fixture f;
    engine_fixture_init(&f, 1, 4096);
    engine_api *api = &f.api;
    void *e = api->engine;
    api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
    u8 force = api->whose_turn(e);
    u8 jiangxia = 27;   /* 江夏, a city of 劉表 in the sample */
    engine_city_economy before = api->city_economy(e, jiangxia);
    u32 mark = api->ledger_count(e);
    b32 advanced = api->city_at(e, jiangxia).owner_force_id == force;
    for (u32 turn = 0; turn < 3 && advanced; ++turn) advanced = feature_next_own_turn(api, force);
    engine_date date = api->date(e);
    engine_city_economy after = api->city_economy(e, jiangxia);
    TEST_CHECK(counts, advanced && date.year == 197 && date.month == 11 && date.day == 1,
               "three turns after the load of the sample it is 197/11/1");
    TEST_CHECK(counts, after.soldiers == before.soldiers && after.public_order == before.public_order,
               "the month start of 197/11/1 keeps the soldiers and the public order of 江夏 (G2-028)");
    TEST_CHECK(counts, feature_count_events(api, SIM_EVENT_SOLDIERS_LOST, mark) == 0,
               "no disaster takes soldiers from a city in month 11");
    engine_fixture_destroy(&f);
}

internal void
feature_turn_tests(test_counts *counts)
{
    feature_calendar_tests(counts);
    feature_turn_order_sample_tests(counts);
    feature_turn_order_three_factions_tests(counts);
    feature_month_start_sample_tests(counts);
}
