/* Feature tests of the develop command, the facility table, the slots and
   the governor rule (city-commands.md, cities-and-facilities.md,
   factions-corps-and-posts.md; F-020, F-021, F-022, F-057), through
   engine_api with the real engine. The numbers are the documented values
   of the spec. Where the rule is assumed, the check says so. */

#define FEATURE_DEVELOP_AP     20     /* city-commands.md: 開發 costs 20 */
#define FEATURE_MAX_DAYS       100    /* guide book p.9: periods up to 100 days */
#define FEATURE_FULL_POLITICS  100    /* assumed (Q-142) */
#define FEATURE_FACILITY_MARKET 0
#define FEATURE_FACILITY_FARM   1
#define FEATURE_FACILITY_MINT   7
#define FEATURE_FACILITY_TALLY  10
#define FEATURE_FACILITY_GREAT_MARKET 13
#define FEATURE_FACILITY_FISH_MARKET  18

struct feature_facility_row
{
    u8          id;
    const char *key;
    u32         cost;
    u32         durability;
    u32         min_days;   /* 0: the spec gives no period */
};

/* cities-and-facilities.md, "Domestic facilities" and "New facilities". */
global_variable feature_facility_row feature_facility_rows[] = {
    {  0, "market",               200,  500, 20 },
    {  1, "farm",                 200,  500, 20 },
    {  2, "barracks",             300,  800, 30 },
    {  3, "smithy",               300,  800, 30 },
    {  4, "stable",               300,  800, 30 },
    {  5, "workshop",             300,  800, 30 },
    {  6, "shipyard",             300,  800, 30 },
    {  7, "mint",                 400, 1000, 30 },
    {  8, "granary",              400, 1000, 30 },
    {  9, "bronze_sparrow_tower", 1500, 1300, 30 },
    { 10, "tally_office",         200,  450, 0 },
    { 11, "personnel_office",     200,  450, 0 },
    { 12, "plot_office",          200,  450, 0 },
    { 13, "great_market",         400,  750, 0 },
    { 14, "black_market",          50,  200, 0 },
    { 15, "diplomacy_office",     200,  450, 0 },
    { 16, "military_office",      200,  450, 0 },
    { 17, "drill_ground",         200,  450, 0 },
    { 18, "fish_market",          400,  750, 0 },
    { 19, "military_farm",        400,  750, 0 },
};

/* The active officers of the owner that stand in the city and have no job. */
internal u32
feature_free_officers(engine_api *api, u8 city_id, u16 *ids, u32 capacity)
{
    engine_city city = api->city_at(api->engine, city_id);
    u32 found = 0;
    u32 count = api->officer_count(api->engine);
    for (u32 index = 0; index < count && found < capacity; ++index)
    {
        engine_officer officer = api->officer_at(api->engine, index);
        if (officer.status <= 3 && officer.force_id == city.owner_force_id && officer.current_position == city_id &&
            officer.job_city == ENGINE_NO_CITY && !officer.acted)
        {
            ids[found++] = officer.officer_id;
        }
    }
    return found;
}

/* The owned city with the most free officers. */
internal u8
feature_busiest_city(engine_api *api)
{
    u8 best = ENGINE_NO_CITY;
    u32 best_count = 0;
    u16 ids[64];
    for (u32 city = 0; city < api->city_count(api->engine); ++city)
    {
        if (api->city_at(api->engine, city).owner_force_id == ENGINE_NO_FACTION) continue;
        u32 count = feature_free_officers(api, (u8)city, ids, ArrayCount(ids));
        if (count > best_count) { best = (u8)city; best_count = count; }
    }
    return best;
}

internal u32
feature_politics_sum(engine_api *api, const u16 *officers)
{
    u32 sum = 0;
    for (u32 index = 0; index < SIM_DEVELOP_OFFICERS; ++index)
    {
        engine_officer officer = {};
        if (officers[index] != ENGINE_NO_OFFICER && feature_officer_by_id(api, officers[index], &officer)) sum += officer.politics;
    }
    return sum;
}

/* The period in turns from the line between the manual's points (assumed, Q-142). */
internal u32
feature_expected_turns(u32 politics_sum, u32 min_days)
{
    u32 sum  = Minimum(politics_sum, (u32)FEATURE_FULL_POLITICS);
    u32 days = FEATURE_MAX_DAYS - (FEATURE_MAX_DAYS - min_days) * sum / FEATURE_FULL_POLITICS;
    return calendar_turns_for_days(days);
}

internal engine_command
feature_develop_command(u8 city_id, u8 facility, u8 slot, u16 first, u16 second, u16 third)
{
    engine_command command = {};
    command.kind        = ENGINE_COMMAND_DEVELOP;
    command.city_id     = city_id;
    command.facility    = facility;
    command.slot        = slot;
    command.officers[0] = first;
    command.officers[1] = second;
    command.officers[2] = third;
    return command;
}

internal sim_refusal
feature_check(engine_api *api, u8 city_id, u8 facility, u8 slot, u16 first, u16 second = ENGINE_NO_OFFICER,
              u16 third = ENGINE_NO_OFFICER)
{
    engine_command command = feature_develop_command(city_id, facility, slot, first, second, third);
    return api->can_issue(api->engine, &command).refusal;
}

internal engine_command_result
feature_develop(engine_api *api, u8 city_id, u8 facility, u8 slot, u16 first, u16 second = ENGINE_NO_OFFICER,
                u16 third = ENGINE_NO_OFFICER)
{
    u16 officers[SIM_DEVELOP_OFFICERS] = { first, second, third };
    return api->command_develop(api->engine, city_id, facility, officers, slot);
}

/* A free slot of the city at hex distance "distance" from slot "from". */
internal u8
feature_slot_at_distance(engine_api *api, u8 city_id, u8 from, s32 distance)
{
    engine_facility origin = api->facility_at(api->engine, city_id, from);
    engine_city_slots slots = api->city_slots(api->engine, city_id);
    for (u8 slot = 0; slot < slots.slot_count; ++slot)
    {
        engine_facility candidate = api->facility_at(api->engine, city_id, slot);
        if (candidate.state != SIM_SLOT_EMPTY || slot == from) continue;
        if (hex_distance(hex_make(candidate.q, candidate.r), hex_make(origin.q, origin.r)) == distance) return slot;
    }
    return ENGINE_ANY_SLOT;
}

internal void
feature_facility_table_tests(test_counts *counts, engine_api *api)
{
    void *e = api->engine;
    engine_content_status status = api->content_status(e);
    TEST_CHECK(counts, status.loaded && status.facility_count == 20 && status.city_count == SPEC_CITY_COUNT &&
                       status.rule_count >= 13, "the engine loads the facility, city and rule tables of the content pack");
    TEST_CHECK(counts, api->facility_kind_count(e) == ArrayCount(feature_facility_rows), "the table has the 10 base and the 10 PK facilities");
    b32 rows_agree = true;
    for (u32 index = 0; index < ArrayCount(feature_facility_rows); ++index)
    {
        feature_facility_row *row = feature_facility_rows + index;
        engine_facility_kind kind = api->facility_kind_at(e, index);
        if (kind.id != row->id || strcmp(kind.key, row->key) != 0 || kind.cost != row->cost || kind.durability != row->durability)
        {
            rows_agree = false;
        }
        if (row->min_days && (kind.min_days != row->min_days || kind.period_assumed)) rows_agree = false;
        if (!row->min_days && !kind.period_assumed) rows_agree = false;
        if ((index >= 10) != (kind.pk != 0)) rows_agree = false;
    }
    TEST_CHECK(counts, rows_agree, "each facility has the documented cost, durability and shortest period; PK periods are assumed");
    TEST_CHECK(counts, api->facility_kind_at(e, 99).id == ENGINE_NO_FACILITY, "facility_kind_at out of range is no facility");
    TEST_CHECK(counts, api->facility_kind_at(e, FEATURE_FACILITY_GREAT_MARKET).limit == FACILITY_LIMIT_BIG_CITY &&
                       api->facility_kind_at(e, FEATURE_FACILITY_FISH_MARKET).limit == FACILITY_LIMIT_PORT_CITY &&
                       api->facility_kind_at(e, 9).needs_item, "the limits of the great market, the fish market and 銅雀台");
}

internal void
feature_city_table_tests(test_counts *counts, engine_api *api)
{
    void *e = api->engine;
    engine_city_slots luoyang = api->city_slots(e, 15), changan = api->city_slots(e, 17), ye = api->city_slots(e, 6);
    engine_city_slots xiaopei = api->city_slots(e, 9), hanzhong = api->city_slots(e, 36);
    TEST_CHECK(counts, luoyang.slot_count == 22 && changan.slot_count == 22 && ye.slot_count == 20 &&
                       xiaopei.slot_count == 10 && hanzhong.slot_count == 15, "each city has the slot count of the guide book");
    TEST_CHECK(counts, ye.specialty == CITY_SPECIALTY_BIG_CITY && hanzhong.specialty == CITY_SPECIALTY_HALBERD &&
                       xiaopei.specialty == CITY_SPECIALTY_SIEGE, "each city has its specialty");
    TEST_CHECK(counts, luoyang.has_port && !hanzhong.has_port, "洛陽 has a port; 漢中 has none");
    TEST_CHECK(counts, api->city_slots(e, 200).city_id == ENGINE_NO_CITY, "city_slots of an unknown city is no city");
    TEST_CHECK(counts, hanzhong.used == 0 && api->facility_at(e, 36, 0).state == SIM_SLOT_EMPTY &&
                       api->facility_at(e, 36, 0).facility == ENGINE_NO_FACILITY,
               "a loaded city starts with empty slots (the save's facilities are not decoded)");
    TEST_CHECK(counts, api->facility_at(e, 36, 15).city_id == ENGINE_NO_CITY, "facility_at out of the slots is no slot");

    /* The governor (factions-corps-and-posts.md): ruler or viceroy first,
       then leadership, force, merit. The post is not decoded (Q-147). */
    b32 governors_agree = true;
    for (u32 city = 0; city < api->city_count(e); ++city)
    {
        engine_city info = api->city_at(e, city);
        u16 expected = ENGINE_NO_OFFICER;
        engine_officer best = {};
        for (u32 index = 0; index < api->officer_count(e); ++index)
        {
            engine_officer officer = api->officer_at(e, index);
            if (officer.status > 3 || officer.force_id != info.owner_force_id || officer.base_location != city) continue;
            u32 rank = officer.status <= 1 ? officer.status : 2, best_rank = best.status <= 1 ? best.status : 2;
            b32 better = expected == ENGINE_NO_OFFICER || rank < best_rank ||
                         (rank == best_rank && (officer.leadership > best.leadership ||
                          (officer.leadership == best.leadership && (officer.war > best.war ||
                           (officer.war == best.war && (officer.merit > best.merit ||
                            (officer.merit == best.merit && officer.officer_id < best.officer_id)))))));
            if (better) { expected = officer.officer_id; best = officer; }
        }
        /* M11: the governor that the city record names (太守) comes first. */
        engine_officer named = api->officer_by_id(e, info.commanding_officer_id);
        if (named.officer_id != ENGINE_NO_OFFICER && named.status <= 3 && named.force_id == info.owner_force_id && named.base_location == city)
        {
            expected = named.officer_id;
        }
        if (api->city_slots(e, (u8)city).governor_id != expected) governors_agree = false;
    }
    TEST_CHECK(counts, governors_agree, "the governor of each city is the one of the city record, else it follows the selection rule");
}

/* Every refusal reason, one at a time. */
internal void
feature_refusal_tests(test_counts *counts, engine_api *api, u8 city, u8 force)
{
    void *e = api->engine;
    u16 free_ids[8] = {};
    u32 free_count = feature_free_officers(api, city, free_ids, ArrayCount(free_ids));
    TEST_CHECK(counts, free_count >= 3, "the test city has at least three free officers");
    if (free_count < 3) return;
    u16 first = free_ids[0], second = free_ids[1];
    api->set_city_gold(e, city, 50000);
    api->set_actions(e, force, 255);

    TEST_CHECK(counts, feature_check(api, city, FEATURE_FACILITY_MARKET, ENGINE_ANY_SLOT, first) == SIM_REFUSAL_NONE,
               "a market with one free officer can run");
    TEST_CHECK(counts, feature_check(api, 200, FEATURE_FACILITY_MARKET, ENGINE_ANY_SLOT, first) == SIM_REFUSAL_NO_CITY,
               "refused: no city");
    u8 other_city = ENGINE_NO_CITY;
    for (u32 index = 0; index < api->city_count(e) && other_city == ENGINE_NO_CITY; ++index)
    {
        engine_city candidate = api->city_at(e, index);
        if (candidate.owner_force_id != force && candidate.owner_force_id != ENGINE_NO_FACTION) other_city = (u8)index;
    }
    TEST_CHECK(counts, feature_check(api, other_city, FEATURE_FACILITY_MARKET, ENGINE_ANY_SLOT, first) == SIM_REFUSAL_NOT_YOUR_TURN,
               "refused: the owner of the city does not act now");
    TEST_CHECK(counts, feature_check(api, city, 99, ENGINE_ANY_SLOT, first) == SIM_REFUSAL_NO_FACILITY,
               "refused: no facility with this id");
    TEST_CHECK(counts, feature_check(api, city, 9, ENGINE_ANY_SLOT, first) == SIM_REFUSAL_NO_FACILITY,
               "refused: 銅雀台 needs the item 銅雀 (items are not modelled)");
    engine_city_slots slots = api->city_slots(e, city);
    if (slots.specialty != CITY_SPECIALTY_BIG_CITY)
    {
        TEST_CHECK(counts, feature_check(api, city, FEATURE_FACILITY_GREAT_MARKET, ENGINE_ANY_SLOT, first) == SIM_REFUSAL_NO_FACILITY,
                   "refused: a great market needs a big city");
    }
    if (!slots.has_port)
    {
        TEST_CHECK(counts, feature_check(api, city, FEATURE_FACILITY_FISH_MARKET, ENGINE_ANY_SLOT, first) == SIM_REFUSAL_NO_FACILITY,
                   "refused: a fish market needs a city with a port");
    }
    TEST_CHECK(counts, feature_check(api, city, FEATURE_FACILITY_MARKET, slots.slot_count, first) == SIM_REFUSAL_SLOT_TAKEN,
               "refused: the slot is not in the city");
    TEST_CHECK(counts, feature_check(api, city, FEATURE_FACILITY_MARKET, ENGINE_ANY_SLOT, ENGINE_NO_OFFICER) == SIM_REFUSAL_NO_OFFICER,
               "refused: no officer");
    TEST_CHECK(counts, feature_check(api, city, FEATURE_FACILITY_MARKET, ENGINE_ANY_SLOT, first, first) == SIM_REFUSAL_DUPLICATE_OFFICER,
               "refused: the same officer two times");
    u16 stranger = ENGINE_NO_OFFICER;
    for (u32 index = 0; index < api->officer_count(e) && stranger == ENGINE_NO_OFFICER; ++index)
    {
        engine_officer officer = api->officer_at(e, index);
        if (officer.status <= 3 && officer.force_id != force) stranger = officer.officer_id;
    }
    TEST_CHECK(counts, feature_check(api, city, FEATURE_FACILITY_MARKET, ENGINE_ANY_SLOT, stranger) == SIM_REFUSAL_OFFICER_NOT_IN_CITY,
               "refused: an officer of another force");

    api->set_actions(e, force, FEATURE_DEVELOP_AP - 1);
    TEST_CHECK(counts, feature_check(api, city, FEATURE_FACILITY_MARKET, ENGINE_ANY_SLOT, first) == SIM_REFUSAL_NO_ACTION_POINTS,
               "refused: 19 action points are fewer than the 20 of a develop");
    api->set_actions(e, force, FEATURE_DEVELOP_AP);
    TEST_CHECK(counts, feature_check(api, city, FEATURE_FACILITY_MARKET, ENGINE_ANY_SLOT, first) == SIM_REFUSAL_NONE,
               "20 action points are sufficient");
    api->set_actions(e, force, 255);
    api->set_city_gold(e, city, 199);
    TEST_CHECK(counts, feature_check(api, city, FEATURE_FACILITY_MARKET, ENGINE_ANY_SLOT, first) == SIM_REFUSAL_NO_GOLD,
               "refused: 199 gold is less than the 200 of a market");
    api->set_city_gold(e, city, 200);
    TEST_CHECK(counts, feature_check(api, city, FEATURE_FACILITY_MARKET, ENGINE_ANY_SLOT, first) == SIM_REFUSAL_NONE,
               "200 gold is sufficient");
    api->set_city_gold(e, city, 50000);

    u32 ledger_before = api->ledger_count(e);
    engine_command_result refused = feature_develop(api, city, FEATURE_FACILITY_MARKET, ENGINE_ANY_SLOT, ENGINE_NO_OFFICER);
    TEST_CHECK(counts, refused.refusal == SIM_REFUSAL_NO_OFFICER && refused.command_id == 0 &&
                       api->ledger_count(e) == ledger_before && api->city_at(e, city).gold == 50000,
               "a refused develop changes nothing and ledgers nothing");

    /* The limit of one tally office in each city. */
    engine_command_result tally = feature_develop(api, city, FEATURE_FACILITY_TALLY, ENGINE_ANY_SLOT, first);
    TEST_CHECK(counts, tally.refusal == SIM_REFUSAL_NONE && tally.command_id != 0, "a tally office starts");
    TEST_CHECK(counts, feature_check(api, city, FEATURE_FACILITY_TALLY, ENGINE_ANY_SLOT, second) == SIM_REFUSAL_VALUE_AT_MAXIMUM,
               "refused: the city has its one tally office (under construction counts)");
    engine_facility building = {};
    for (u8 slot = 0; slot < slots.slot_count; ++slot)
    {
        engine_facility candidate = api->facility_at(e, city, slot);
        if (candidate.state == SIM_SLOT_BUILDING) building = candidate;
    }
    TEST_CHECK(counts, feature_check(api, city, FEATURE_FACILITY_MARKET, building.slot, second) == SIM_REFUSAL_SLOT_TAKEN,
               "refused: the slot has a facility under construction");
    TEST_CHECK(counts, feature_check(api, city, FEATURE_FACILITY_MARKET, ENGINE_ANY_SLOT, first) == SIM_REFUSAL_OFFICER_ALREADY_ACTED,
               "refused: the officer acted in this turn (one command per officer per turn)");
    TEST_CHECK(counts, feature_next_own_turn(api, force), "the faction acts again in the next turn");
    api->set_actions(e, force, 255);
    TEST_CHECK(counts, feature_check(api, city, FEATURE_FACILITY_MARKET, ENGINE_ANY_SLOT, first) == SIM_REFUSAL_OFFICER_BUSY,
               "refused: in the next turn the officer is busy until the facility is complete");
}

/* A develop that completes after the period of the politics sum. */
internal void
feature_develop_completion_tests(test_counts *counts, engine_api *api, u8 city, u8 force)
{
    void *e = api->engine;
    api->set_city_gold(e, city, 50000);
    api->set_actions(e, force, 255);
    u16 ids[3] = {};
    u32 found = feature_free_officers(api, city, ids, 1);
    TEST_CHECK(counts, found == 1, "one free officer for the completion test");
    if (!found) return;

    engine_command command = feature_develop_command(city, FEATURE_FACILITY_FARM, ENGINE_ANY_SLOT, ids[0], ENGINE_NO_OFFICER, ENGINE_NO_OFFICER);
    engine_check check = api->can_issue(e, &command);
    u32 politics = feature_politics_sum(api, command.officers);
    u32 turns = feature_expected_turns(politics, 20);
    TEST_CHECK(counts, check.refusal == SIM_REFUSAL_NONE && check.turns == turns && check.days == turns * 10 &&
                       check.gold == 200 && check.action_points == FEATURE_DEVELOP_AP,
               "can_issue gives the cost (200 gold, 20 points) and the period from the politics sum (assumed line)");

    u32 gold_before = api->city_at(e, city).gold;
    u32 points_before = api->actions_left(e, force);
    u32 mark = api->ledger_count(e);
    engine_city_slots site_before = api->city_slots(e, city);
    engine_command_result result = api->command_develop(e, city, FEATURE_FACILITY_FARM, command.officers, ENGINE_ANY_SLOT);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && result.command_id != 0, "the develop starts");
    TEST_CHECK(counts, api->city_at(e, city).gold == gold_before - 200 && api->actions_left(e, force) == points_before - FEATURE_DEVELOP_AP,
               "the develop pays its gold and its action points when it starts");
    engine_facility slot = api->facility_at(e, city, check.slot);
    TEST_CHECK(counts, slot.state == SIM_SLOT_BUILDING && slot.facility == FEATURE_FACILITY_FARM && slot.turns_left == turns &&
                       slot.officers[0] == ids[0], "the slot is under construction with the officer");
    sim_event started = {};
    TEST_CHECK(counts, feature_find_event(api, SIM_EVENT_DEVELOP_STARTED, mark, &started) && started.cause == result.command_id &&
                       started.delta == FEATURE_FACILITY_FARM && feature_count_events(api, SIM_EVENT_GOLD_SPENT, mark) == 1 &&
                       feature_count_events(api, SIM_EVENT_ACTION_POINTS_SPENT, mark) == 1,
               "the ledger has the start, the gold and the action points, with the command as the cause");
    engine_officer worker = {};
    TEST_CHECK(counts, feature_officer_by_id(api, ids[0], &worker) && worker.job_city == city && worker.acted,
               "the officer has the job and acted in this turn");
    TEST_CHECK(counts, api->city_slots(e, city).used >= 1, "the slot is in use");
    engine_city_slots site_started = api->city_slots(e, city);
    TEST_CHECK(counts, site_started.building == site_before.building + 1 && site_started.built == site_before.built &&
                       site_started.used == site_before.used + 1,
               "city_slots counts the build in progress apart from the built facilities");

    for (u32 turn = 1; turn < turns; ++turn)
    {
        feature_next_own_turn(api, force);
        TEST_CHECK(counts, api->facility_at(e, city, check.slot).state == SIM_SLOT_BUILDING &&
                           api->facility_at(e, city, check.slot).turns_left == turns - turn,
                   "the facility is under construction before the period ends");
    }
    mark = api->ledger_count(e);
    feature_next_own_turn(api, force);
    slot = api->facility_at(e, city, check.slot);
    TEST_CHECK(counts, slot.state == SIM_SLOT_BUILT && slot.turns_left == 0, "the facility is complete after the period");
    engine_city_slots site_finished = api->city_slots(e, city);
    TEST_CHECK(counts, site_finished.built >= site_started.built + 1 && site_finished.building <= site_started.building - 1 &&
                       site_finished.used == site_finished.built + site_finished.building,
               "city_slots moves the complete facility from building to built");
    sim_event completed = {}, released = {};
    TEST_CHECK(counts, feature_find_event(api, SIM_EVENT_FACILITY_COMPLETED, mark, &completed) &&
                       completed.delta == FEATURE_FACILITY_FARM && completed.cause != 0,
               "the completion is an event of the new turn, caused by the last proceed");
    TEST_CHECK(counts, feature_find_event(api, SIM_EVENT_OFFICER_RELEASED, mark, &released), "the officer is released");
    TEST_CHECK(counts, feature_officer_by_id(api, ids[0], &worker) && worker.job_city == ENGINE_NO_CITY && !worker.acted,
               "the officer is free again");
    TEST_CHECK(counts, slot.yield_effect == FACILITY_EFFECT_GRAIN && slot.yield == 250,
               "a complete farm gives its grain (assumed value)");
}

/* The mint bonus: a market next to a mint gives 1.5 times its gold. */
internal void
feature_adjacency_tests(test_counts *counts, engine_api *api, u8 city, u8 force)
{
    void *e = api->engine;
    api->set_city_gold(e, city, 50000);
    api->set_actions(e, force, 255);
    u16 ids[3] = {};
    u32 found = feature_free_officers(api, city, ids, 3);
    TEST_CHECK(counts, found == 3, "three free officers for the adjacency test");
    if (found < 3) return;

    u8 mint_slot = 0;
    for (u8 slot = 0; slot < api->city_slots(e, city).slot_count; ++slot)
    {
        if (api->facility_at(e, city, slot).state != SIM_SLOT_EMPTY) continue;
        if (feature_slot_at_distance(api, city, slot, 1) != ENGINE_ANY_SLOT && feature_slot_at_distance(api, city, slot, 3) != ENGINE_ANY_SLOT)
        {
            mint_slot = slot;
            break;
        }
    }
    u8 near_slot = feature_slot_at_distance(api, city, mint_slot, 1);
    u8 far_slot  = feature_slot_at_distance(api, city, mint_slot, 3);
    TEST_CHECK(counts, near_slot != ENGINE_ANY_SLOT && far_slot != ENGINE_ANY_SLOT, "the city has a slot next to the mint and one away");
    TEST_CHECK(counts, feature_develop(api, city, FEATURE_FACILITY_MINT, mint_slot, ids[0]).refusal == SIM_REFUSAL_NONE &&
                       feature_develop(api, city, FEATURE_FACILITY_MARKET, near_slot, ids[1]).refusal == SIM_REFUSAL_NONE &&
                       feature_develop(api, city, FEATURE_FACILITY_MARKET, far_slot, ids[2]).refusal == SIM_REFUSAL_NONE,
               "three officers start a mint and two markets in one turn");
    for (u32 turn = 0; turn < 12; ++turn)
    {
        if (api->facility_at(e, city, mint_slot).state == SIM_SLOT_BUILT && api->facility_at(e, city, near_slot).state == SIM_SLOT_BUILT &&
            api->facility_at(e, city, far_slot).state == SIM_SLOT_BUILT) break;
        feature_next_own_turn(api, force);
    }
    engine_facility near_market = api->facility_at(e, city, near_slot), far_market = api->facility_at(e, city, far_slot);
    u32 value = api->facility_kind_at(e, FEATURE_FACILITY_MARKET).value;
    TEST_CHECK(counts, near_market.state == SIM_SLOT_BUILT && far_market.state == SIM_SLOT_BUILT &&
                       api->facility_at(e, city, mint_slot).state == SIM_SLOT_BUILT, "the mint and the two markets are complete");
    TEST_CHECK(counts, far_market.yield == value && far_market.yield_effect == FACILITY_EFFECT_GOLD, "a market away from the mint gives its value");
    TEST_CHECK(counts, near_market.yield == value * 3 / 2, "a market next to the mint gives 1.5 times its value");

    /* The ledger shows the income on the next month start. */
    engine_city_slots slots = api->city_slots(e, city);
    entity_id subject = 0;
    u32 mark = api->ledger_count(e);
    for (u32 turn = 0; turn < 3 && api->date(e).day != 21; ++turn) feature_next_own_turn(api, force);
    slots = api->city_slots(e, city);
    mark = api->ledger_count(e);
    feature_next_own_turn(api, force);
    b32 found_income = false;
    for (u32 index = mark; index < api->ledger_count(e); ++index)
    {
        sim_event event = api->ledger_at(e, index);
        if (event.kind == SIM_EVENT_GOLD_INCOME && event.faction == force && event.delta == (s32)slots.gold_income) found_income = true;
    }
    (void)subject;
    TEST_CHECK(counts, found_income, "on day 1 the gold income event has the income with the mint bonus");
}

/* No slot is free: fill a city through the develop command. */
internal void
feature_no_slot_tests(test_counts *counts, engine_api *api, u8 city, u8 force)
{
    void *e = api->engine;
    for (u32 turn = 0; turn < 120; ++turn)
    {
        engine_city_slots slots = api->city_slots(e, city);
        if (slots.used == slots.slot_count) break;
        api->set_city_gold(e, city, 50000);
        api->set_actions(e, force, 255);
        u16 ids[16] = {};
        u32 found = feature_free_officers(api, city, ids, ArrayCount(ids));
        for (u32 index = 0; index < found; ++index) feature_develop(api, city, FEATURE_FACILITY_MARKET, ENGINE_ANY_SLOT, ids[index]);
        feature_next_own_turn(api, force);
    }
    engine_city_slots slots = api->city_slots(e, city);
    TEST_CHECK(counts, slots.used == slots.slot_count, "the develop command fills every slot of the city");
    u16 ids[1] = {};
    for (u32 turn = 0; turn < 12 && !feature_free_officers(api, city, ids, 1); ++turn) feature_next_own_turn(api, force);
    api->set_actions(e, force, 255);
    TEST_CHECK(counts, feature_check(api, city, FEATURE_FACILITY_MARKET, ENGINE_ANY_SLOT, ids[0]) == SIM_REFUSAL_NO_SLOT,
               "refused: the city has no free slot");
}

/* An enemy unit within 2 hexes blocks the develop command. */
internal void
feature_enemy_near_tests(test_counts *counts, engine_api *api, u8 city, u8 force)
{
    void *e = api->engine;
    api->set_actions(e, force, 255);
    u16 ids[1] = {};
    feature_free_officers(api, city, ids, 1);
    engine_city_slots slots = api->city_slots(e, city);
    u8 enemy = force == 1 ? 2 : 1;
    TEST_CHECK(counts, api->place_unit(e, force, slots.q + 1, slots.r) != ENGINE_NO_UNIT, "a unit of the own force stands next to the city");
    TEST_CHECK(counts, api->place_unit(e, enemy, slots.q + 3, slots.r) != ENGINE_NO_UNIT, "an enemy unit stands 3 hexes away");
    TEST_CHECK(counts, feature_check(api, city, FEATURE_FACILITY_MARKET, ENGINE_ANY_SLOT, ids[0]) != SIM_REFUSAL_ENEMY_NEAR,
               "an own unit and an enemy 3 hexes away do not block");
    api->place_unit(e, enemy, slots.q + 2, slots.r);
    TEST_CHECK(counts, feature_check(api, city, FEATURE_FACILITY_MARKET, ENGINE_ANY_SLOT, ids[0]) == SIM_REFUSAL_ENEMY_NEAR,
               "refused: an enemy unit within 2 hexes");
}

internal void
feature_develop_tests(test_counts *counts)
{
    engine_fixture f;
    engine_fixture_init(&f, 1, 65536);
    engine_api *api = &f.api;
    void *e = api->engine;

    feature_facility_table_tests(counts, api);
    TEST_CHECK(counts, api->city_slots(e, 0).city_id == ENGINE_NO_CITY, "before a load there are no city slots");
    engine_command early = feature_develop_command(0, 0, ENGINE_ANY_SLOT, 1, ENGINE_NO_OFFICER, ENGINE_NO_OFFICER);
    TEST_CHECK(counts, api->can_issue(e, &early).refusal == SIM_REFUSAL_NO_CITY, "before a load a develop has no city");

    api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
    feature_city_table_tests(counts, api);
    u8 city = feature_busiest_city(api);
    u8 force = api->city_at(e, city).owner_force_id;
    TEST_CHECK(counts, city != ENGINE_NO_CITY && feature_play_as(api, force), "the test plays the faction of the city with the most officers");

    feature_refusal_tests(counts, api, city, force);
    feature_develop_completion_tests(counts, api, city, force);
    feature_adjacency_tests(counts, api, city, force);
    feature_no_slot_tests(counts, api, city, force);
    feature_enemy_near_tests(counts, api, city, force);

    /* An engine without the content pack refuses the command. */
    memory_arena permanent = test_arena_create(Megabytes(8)), transient = test_arena_create(Megabytes(8));
    engine_config config = engine_default_config();
    config.content_folder = "no-such-folder";
    config.read_entire_file = test_platform_read_entire_file;
    engine_api bare = engine_make_api(engine_create(&permanent, &transient, config));
    bare.load_save(bare.engine, SAMPLE_SAVE_RELATIVE_PATH);
    TEST_CHECK(counts, !bare.content_status(bare.engine).loaded, "an engine without the tables reports it");
    engine_command command = feature_develop_command(36, 0, ENGINE_ANY_SLOT, 1, ENGINE_NO_OFFICER, ENGINE_NO_OFFICER);
    TEST_CHECK(counts, bare.can_issue(bare.engine, &command).refusal == SIM_REFUSAL_NO_CONTENT, "refused: no content pack");
    test_arena_destroy(&transient);
    test_arena_destroy(&permanent);

    engine_fixture_destroy(&f);
}
