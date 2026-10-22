/* Feature tests of M4 growth (research-techniques.md, cities-and-facilities.md
   "Merge" and "New facilities", weapons-and-production.md,
   calendar-and-turn.md "Disasters" and "Victory and defeat"; F-007, F-008,
   F-023, F-024, F-028, F-040 to F-043), through engine_api with the real
   engine. The costs are documented; the periods, the disaster numbers and
   some effects are assumed, and the ledger marks them. */

#define FEATURE_RESEARCH_AP     50      /* city-commands.md */
#define FEATURE_MERGE_AP        10      /* city-commands.md */
#define FEATURE_MERGE_GOLD      100     /* cities-and-facilities.md */
#define FEATURE_TECH_PIKE_DRILL 0
#define FEATURE_TECH_GRAIN_RAID 1
#define FEATURE_TECH_VETERANS   16
#define FEATURE_TECH_ROUGH_MARCH 17
#define FEATURE_TECH_CATAPULTS  22
#define FEATURE_TECH_ENGINEERS  24
#define FEATURE_TECH_WOOD_BEASTS 28
#define FEATURE_TECH_DECREES    34
#define FEATURE_FACILITY_SHIPYARD     6
#define FEATURE_FACILITY_PERSONNEL    11
#define FEATURE_FACILITY_MILITARY     16
#define FEATURE_FACILITY_DRILL_GROUND 17
#define FEATURE_WEAPON_TOWER      6
#define FEATURE_WEAPON_CATAPULT   7
#define FEATURE_WEAPON_TOWER_SHIP 8
#define FEATURE_DISASTER_LOCUSTS 0
#define FEATURE_DISASTER_PLAGUE  1
#define FEATURE_DISASTER_BANDITS 2
#define FEATURE_DISASTER_HARVEST 3

internal engine_command
feature_research_command(u8 city, u8 technique, const u16 *officers)
{
    engine_command command = {};
    command.kind      = ENGINE_COMMAND_RESEARCH;
    command.city_id   = city;
    command.technique = technique;
    for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place) command.officers[place] = officers[place];
    return command;
}

internal engine_command
feature_merge_command(u8 city, u8 absorber, u8 absorbed, u16 officer)
{
    engine_command command = {};
    command.kind        = ENGINE_COMMAND_MERGE;
    command.city_id     = city;
    command.slot        = absorber;
    command.absorbed    = absorbed;
    command.officers[0] = officer;
    command.officers[1] = ENGINE_NO_OFFICER;
    command.officers[2] = ENGINE_NO_OFFICER;
    return command;
}

/* The period in days of the assumed line of a research (Q-161). */
internal u32
feature_research_days(u32 ability_sum, u32 min_days, u32 max_days)
{
    u32 sum  = Minimum(ability_sum, 200u);
    u32 days = max_days - (max_days - min_days) * sum / 200;
    return Maximum(1u, calendar_turns_for_days(days)) * CALENDAR_DAYS_PER_TURN;
}

/* ---- the technique table and the research command ----------------------- */

internal void
feature_technique_table_tests(test_counts *counts, engine_api *api)
{
    void *e = api->engine;
    TEST_CHECK(counts, api->technique_kind_count(e) == 36 && api->content_status(e).technique_count == 36,
               "the content pack has 32 techniques in 8 lines and the 4 techniques of the PK");
    u32 costs_ok = 1;
    u32 points[5] = { 0, 1000, 2000, 3000, 5000 }, gold[5] = { 0, 1000, 2000, 5000, 10000 };
    for (u32 index = 0; index < 36; ++index)
    {
        engine_technique_kind kind = api->technique_kind_at(e, index);
        if (kind.level < 1 || kind.level > 4 || kind.points != points[kind.level] || kind.gold != gold[kind.level]) costs_ok = 0;
        if ((kind.level == 1) != (kind.previous == ENGINE_NO_TECHNIQUE)) costs_ok = 0;
    }
    TEST_CHECK(counts, costs_ok, "each technique has the documented cost of its level, and each level above 1 needs the level below");
    engine_technique_kind veterans = api->technique_kind_at(e, FEATURE_TECH_VETERANS);
    TEST_CHECK(counts, veterans.effect == TECHNIQUE_EFFECT_MORALE_CAP && veterans.value == 20 && veterans.ability == CONTENT_ABILITY_FORCE &&
                       veterans.min_days == 40 && veterans.max_days == 60, "the veterans technique: morale cap +20, force, 40 to 60 days");
    TEST_CHECK(counts, api->technique_kind_at(e, 99).id == ENGINE_NO_TECHNIQUE, "a row out of the table is no technique");
    engine_weapon_kind wood_beast = api->weapon_kind_at(e, FEATURE_WEAPON_WOOD_BEAST);
    TEST_CHECK(counts, wood_beast.technique == FEATURE_TECH_WOOD_BEASTS && wood_beast.replaces == FEATURE_WEAPON_RAM &&
                       api->weapon_kind_at(e, FEATURE_WEAPON_CATAPULT).replaces == FEATURE_WEAPON_TOWER &&
                       api->weapon_kind_at(e, FEATURE_WEAPON_PIKE).technique == ENGINE_NO_TECHNIQUE,
               "the wood beast and the catapult name their technique and the weapon that they replace");
}

internal void
feature_research_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_city_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 city = setup.city, force = setup.force;
    feature_technique_table_tests(counts, api);

    api->set_technique_points(e, force, 10000);
    u16 ids[3];
    feature_take_officers(api, city, ids, 3);
    engine_command command = feature_research_command(city, FEATURE_TECH_PIKE_DRILL, ids);
    engine_check check = api->can_issue(e, &command);
    u32 days = feature_research_days(feature_officer_sum(api, ids, FEATURE_LEADERSHIP), 40, 60);
    TEST_CHECK(counts, check.refusal == SIM_REFUSAL_NONE && check.gold == 1000 && check.action_points == FEATURE_RESEARCH_AP &&
                       check.quantity == 1000 && check.value_before == 10000 && check.value_after == 9000 && check.days == days && check.assumed,
               "research: can_issue gives 1000 gold, 50 points, 1000 technique points and the period by leadership (assumed line)");

    command.technique = FEATURE_TECH_GRAIN_RAID;
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_NEEDS_TECHNIQUE, "refused: level 2 needs level 1 of the line");
    command.technique = 200;
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_NO_TECHNIQUE, "refused: no such technique");
    command.technique = FEATURE_TECH_PIKE_DRILL;
    api->set_technique_points(e, force, 999);
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_NO_TECHNIQUE_POINTS, "refused: 999 technique points are less than 1000");
    api->set_technique_points(e, force, 10000);
    api->set_actions(e, force, FEATURE_RESEARCH_AP - 1);
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_NO_ACTION_POINTS, "refused: 49 action points");
    api->set_actions(e, force, 255);
    api->set_city_gold(e, city, 999);
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_NO_GOLD, "refused: 999 gold");
    api->set_city_gold(e, city, 50000);
    u16 none[3] = { ENGINE_NO_OFFICER, ENGINE_NO_OFFICER, ENGINE_NO_OFFICER };
    TEST_CHECK(counts, api->command_research(e, city, FEATURE_TECH_PIKE_DRILL, none).refusal == SIM_REFUSAL_NO_OFFICER, "refused: no officer");

    u32 mark = api->ledger_count(e);
    u32 gold = api->city_at(e, city).gold, points = api->actions_left(e, force);
    engine_command_result result = api->command_research(e, city, FEATURE_TECH_PIKE_DRILL, ids);
    engine_faction_techniques techniques = api->faction_techniques(e, force);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && result.command_id != 0 && techniques.researching == FEATURE_TECH_PIKE_DRILL &&
                       techniques.turns_left == days / 10 && techniques.technique_points == 9000 && techniques.research_city == city,
               "the research runs: the faction researches the technique for the period and paid 1000 technique points");
    TEST_CHECK(counts, api->city_at(e, city).gold == gold - 1000 && api->actions_left(e, force) == points - FEATURE_RESEARCH_AP,
               "research pays 1000 gold and 50 action points");
    sim_event spent = {};
    TEST_CHECK(counts, feature_find_event(api, SIM_EVENT_TECHNIQUE_SPENT, mark, &spent) && spent.delta == -1000 && spent.faction == force &&
                       feature_count_events(api, SIM_EVENT_RESEARCH_STARTED, mark) == 1 && feature_count_events(api, SIM_EVENT_TECHNIQUE_GAINED, mark) == 0 &&
                       feature_count_events(api, SIM_EVENT_MERIT_GAINED, mark) == 3,
               "the ledger has the technique points, the start and the merit; research gives no technique points");
    engine_officer officer = {};
    TEST_CHECK(counts, feature_officer_by_id(api, ids[0], &officer) && officer.job_city == city, "the officers are busy with the research");
    u16 more[3];
    feature_take_officers(api, city, more, 1);
    command = feature_research_command(city, FEATURE_TECH_VETERANS, more);
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_LIMIT_REACHED, "refused: one research at a time for each faction (assumed)");

    mark = api->ledger_count(e);
    for (u32 turn = 0; turn < 12 && api->faction_techniques(e, force).researching != ENGINE_NO_TECHNIQUE; ++turn) feature_next_own_turn(api, force);
    techniques = api->faction_techniques(e, force);
    TEST_CHECK(counts, techniques.researching == ENGINE_NO_TECHNIQUE && ((techniques.owned >> FEATURE_TECH_PIKE_DRILL) & 1) &&
                       feature_count_events(api, SIM_EVENT_TECHNIQUE_RESEARCHED, mark) == 1 && feature_count_events(api, SIM_EVENT_OFFICER_RELEASED, mark) == 3,
               "after the period the faction has the technique and the three officers are free");
    feature_take_officers(api, city, ids, 3);
    command = feature_research_command(city, FEATURE_TECH_PIKE_DRILL, ids);
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_VALUE_AT_MAXIMUM, "refused: the faction has the technique");
    command.technique = FEATURE_TECH_GRAIN_RAID;
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_NONE, "level 2 of the line can start now");

    api->place_unit(e, (u8)(force + 1), api->city_slots(e, city).q, api->city_slots(e, city).r);
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_ENEMY_NEAR, "refused: an enemy unit within 2 hexes blocks research");
    engine_fixture_destroy(&setup.fixture);
}

/* ---- the effects of the techniques -------------------------------------- */

internal void
feature_technique_effect_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_city_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 city = setup.city, force = setup.force;

    /* 熟練兵: morale up to 120. */
    TEST_CHECK(counts, api->city_economy(e, city).morale_max == 100, "morale has a maximum of 100");
    api->grant_technique(e, force, FEATURE_TECH_VETERANS);
    TEST_CHECK(counts, api->city_economy(e, city).morale_max == 120 && ((api->faction_techniques(e, force).owned >> FEATURE_TECH_VETERANS) & 1),
               "with the veterans technique the maximum is 120");
    api->set_city_resource(e, city, SIM_RESOURCE_MORALE, 0, 100);
    u16 ids[3];
    feature_take_officers(api, city, ids, 3);
    engine_command train = feature_city_command(ENGINE_COMMAND_TRAIN, city, ids[0], ids[1], ids[2]);
    engine_check check = api->can_issue(e, &train);
    TEST_CHECK(counts, check.refusal == SIM_REFUSAL_NONE && check.value_after > 100 && check.value_after <= 120,
               "train raises morale above 100 with the veterans technique");

    /* 開發木獸 and 開發投石: the strong weapons replace the weak ones. */
    u8 workshop = feature_build(api, city, FEATURE_FACILITY_WORKSHOP, force);
    TEST_CHECK(counts, workshop != ENGINE_ANY_SLOT, "the city builds a workshop");
    feature_take_officers(api, city, ids, 1);
    engine_command produce = feature_city_command(ENGINE_COMMAND_PRODUCE, city, ids[0]);
    produce.weapon = FEATURE_WEAPON_WOOD_BEAST;
    TEST_CHECK(counts, api->can_issue(e, &produce).refusal == SIM_REFUSAL_NEEDS_TECHNIQUE, "the wood beast needs its technique");
    produce.weapon = FEATURE_WEAPON_RAM;
    TEST_CHECK(counts, api->can_issue(e, &produce).refusal == SIM_REFUSAL_NONE, "the ram can be produced before the technique");
    api->grant_technique(e, force, FEATURE_TECH_WOOD_BEASTS);
    TEST_CHECK(counts, api->can_issue(e, &produce).refusal == SIM_REFUSAL_WEAPON_REPLACED, "refused: the wood beast replaces the ram");
    produce.weapon = FEATURE_WEAPON_WOOD_BEAST;
    TEST_CHECK(counts, api->can_issue(e, &produce).refusal == SIM_REFUSAL_NONE, "the wood beast can be produced with its technique");
    api->grant_technique(e, force, FEATURE_TECH_CATAPULTS);
    produce.weapon = FEATURE_WEAPON_TOWER;
    TEST_CHECK(counts, api->can_issue(e, &produce).refusal == SIM_REFUSAL_WEAPON_REPLACED, "refused: the catapult replaces the tower");
    produce.weapon = FEATURE_WEAPON_CATAPULT;
    TEST_CHECK(counts, api->can_issue(e, &produce).refusal == SIM_REFUSAL_NONE, "the catapult can be produced with its technique");

    /* 培訓工兵: facilities recover more durability. */
    u32 mark = api->ledger_count(e);
    api->damage_facility(e, city, workshop, 500);
    api->grant_technique(e, force, FEATURE_TECH_ENGINEERS);
    feature_next_own_turn(api, force);
    sim_event repaired = {};
    TEST_CHECK(counts, feature_find_city_event(api, city, SIM_EVENT_FACILITY_REPAIRED, mark, &repaired) && repaired.delta == 200 && repaired.assumed,
               "with the engineers technique a facility recovers 200 durability each turn (assumed: two times)");

    /* 整備政令: public order decreases less. */
    u8 barracks = feature_build(api, city, FEATURE_FACILITY_BARRACKS, force);
    api->grant_technique(e, force, FEATURE_TECH_DECREES);
    api->set_city_resource(e, city, SIM_RESOURCE_PUBLIC_ORDER, 0, 60);
    api->set_city_resource(e, city, SIM_RESOURCE_SOLDIERS, 0, 1000);
    feature_take_officers(api, city, ids, 1);
    mark = api->ledger_count(e);
    TEST_CHECK(counts, barracks != ENGINE_ANY_SLOT && api->command_conscript(e, city, ids).refusal == SIM_REFUSAL_NONE, "the city conscripts");
    sim_event order = {};
    TEST_CHECK(counts, feature_find_city_event(api, city, SIM_EVENT_ORDER_LOST, mark, &order) && order.delta == -5,
               "with the decrees technique conscription takes 5 public order, not 10 (assumed: half)");

    /* The flags of M5 and M6 are in the set of the faction. */
    api->grant_technique(e, force, FEATURE_TECH_ROUGH_MARCH);
    TEST_CHECK(counts, (api->faction_techniques(e, force).owned >> FEATURE_TECH_ROUGH_MARCH) & 1, "a granted technique is in the set of the faction");
    engine_fixture_destroy(&setup.fixture);
}

/* ---- the merge ----------------------------------------------------------- */

/* Two free slots next to each other, then a third next to the first and two
   hexes from the second, if the city has them. */
internal b32
feature_merge_slots(engine_api *api, u8 city, u8 *first, u8 *second, u8 *third)
{
    engine_city_slots slots = api->city_slots(api->engine, city);
    for (u8 slot = 0; slot < slots.slot_count; ++slot)
    {
        if (api->facility_at(api->engine, city, slot).state != SIM_SLOT_EMPTY) continue;
        u8 near_slot = feature_slot_at_distance(api, city, slot, 1);
        if (near_slot == ENGINE_ANY_SLOT) continue;
        engine_facility a = api->facility_at(api->engine, city, slot), b = api->facility_at(api->engine, city, near_slot);
        for (u8 other = 0; other < slots.slot_count; ++other)
        {
            engine_facility c = api->facility_at(api->engine, city, other);
            if (other == slot || other == near_slot || c.state != SIM_SLOT_EMPTY) continue;
            if (hex_distance(hex_make(c.q, c.r), hex_make(a.q, a.r)) == 1 && hex_distance(hex_make(c.q, c.r), hex_make(b.q, b.r)) == 2)
            {
                *first = slot;
                *second = near_slot;
                *third = other;
                return true;
            }
        }
    }
    return false;
}

internal void
feature_merge_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_city_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 city = setup.city, force = setup.force;

    u8 first = 0, second = 0, third = 0;
    TEST_CHECK(counts, feature_merge_slots(api, city, &first, &second, &third), "the city has three slots for the merge test");
    b32 built = feature_build(api, city, FEATURE_FACILITY_MARKET, force, first) == first &&
                feature_build(api, city, FEATURE_FACILITY_MARKET, force, second) == second &&
                feature_build(api, city, FEATURE_FACILITY_MARKET, force, third) == third;
    TEST_CHECK(counts, built, "three markets stand: two pairs of neighbours");
    if (!built) { engine_fixture_destroy(&setup.fixture); return; }
    u32 value = api->facility_kind_at(e, FEATURE_FACILITY_MARKET).value;

    api->set_city_gold(e, city, 50000);
    api->set_actions(e, force, 255);
    u16 ids[3];
    feature_take_officers(api, city, ids, 2);
    engine_command command = feature_merge_command(city, first, second, ids[0]);
    engine_check check = api->can_issue(e, &command);
    engine_officer officer = {};
    feature_officer_by_id(api, ids[0], &officer);
    u32 merge_days = Maximum(1u, calendar_turns_for_days(60 - 40 * Minimum((u32)officer.politics, 100u) / 100)) * 10;
    TEST_CHECK(counts, check.refusal == SIM_REFUSAL_NONE && check.gold == FEATURE_MERGE_GOLD && check.action_points == FEATURE_MERGE_AP &&
                       check.value_before == 1 && check.value_after == 2 && check.slot == first && check.days == merge_days && check.assumed,
               "merge: can_issue gives 100 gold, 10 points, level 1 -> 2 and the period by politics (assumed line)");
    command.absorbed = third;
    command.slot     = second;
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_NOT_MERGEABLE, "refused: the two markets are not neighbours");
    command = feature_merge_command(city, first, first, ids[0]);
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_NOT_MERGEABLE, "refused: a facility cannot absorb itself");
    command = feature_merge_command(city, first, second, ENGINE_NO_OFFICER);
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_NO_OFFICER, "refused: no officer");
    api->set_actions(e, force, FEATURE_MERGE_AP - 1);
    TEST_CHECK(counts, api->command_merge(e, city, first, second, ids[0]).refusal == SIM_REFUSAL_NO_ACTION_POINTS, "refused: 9 action points");
    api->set_actions(e, force, 255);
    api->set_city_gold(e, city, FEATURE_MERGE_GOLD - 1);
    TEST_CHECK(counts, api->command_merge(e, city, first, second, ids[0]).refusal == SIM_REFUSAL_NO_GOLD, "refused: 99 gold");
    api->set_city_gold(e, city, 50000);

    u32 mark = api->ledger_count(e);
    u32 used = api->city_slots(e, city).used;
    engine_command_result result = api->command_merge(e, city, first, second, ids[0]);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && api->facility_at(e, city, second).state == SIM_SLOT_MERGING &&
                       api->facility_at(e, city, second).yield == 0 && api->facility_at(e, city, first).yield == value,
               "during the merge the absorbed market gives nothing and the absorber gives its level-1 output");
    TEST_CHECK(counts, feature_count_events(api, SIM_EVENT_MERGE_STARTED, mark) == 1 && feature_count_events(api, SIM_EVENT_GOLD_SPENT, mark) == 1,
               "the ledger has the start of the merge and the gold");
    TEST_CHECK(counts, api->command_merge(e, city, first, third, ids[1]).refusal == SIM_REFUSAL_NOT_MERGEABLE,
               "refused: the absorber merges one facility at a time");

    mark = api->ledger_count(e);
    for (u32 turn = 0; turn < 8 && api->facility_at(e, city, first).level < 2; ++turn) feature_next_own_turn(api, force);
    sim_event merged = {};
    TEST_CHECK(counts, api->facility_at(e, city, first).level == 2 && api->facility_at(e, city, second).state == SIM_SLOT_EMPTY &&
                       api->city_slots(e, city).used == used - 1 && feature_find_city_event(api, city, SIM_EVENT_FACILITY_MERGED, mark, &merged) &&
                       merged.delta == 2 && merged.detail == first,
               "after the period the absorber is level 2 and the merge frees one slot");
    TEST_CHECK(counts, api->facility_at(e, city, first).yield == value * 120 / 100, "a level-2 market gives 1.2 times");

    api->set_city_gold(e, city, 50000);
    api->set_actions(e, force, 255);
    feature_take_officers(api, city, ids, 1);
    command = feature_merge_command(city, third, first, ids[0]);
    TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_NOT_MERGEABLE, "refused: the level-2 market must be the absorber");
    command = feature_merge_command(city, first, third, ids[0]);
    TEST_CHECK(counts, api->can_issue(e, &command).value_after == 3 && api->command_merge(e, city, first, third, ids[0]).refusal == SIM_REFUSAL_NONE,
               "a level-2 absorber and a level-1 market merge to level 3");
    for (u32 turn = 0; turn < 8 && api->facility_at(e, city, first).level < 3; ++turn) feature_next_own_turn(api, force);
    TEST_CHECK(counts, api->facility_at(e, city, first).level == 3 && api->facility_at(e, city, first).yield == value * 150 / 100,
               "a level-3 market gives 1.5 times");
    u8 fourth = feature_slot_at_distance(api, city, first, 1);
    if (fourth != ENGINE_ANY_SLOT && feature_build(api, city, FEATURE_FACILITY_MARKET, force, fourth) == fourth)
    {
        feature_take_officers(api, city, ids, 1);
        command = feature_merge_command(city, first, fourth, ids[0]);
        TEST_CHECK(counts, api->can_issue(e, &command).refusal == SIM_REFUSAL_NOT_MERGEABLE, "refused: the maximum level is 3");
    }
    engine_fixture_destroy(&setup.fixture);
}

/* ---- the disasters ------------------------------------------------------- */

/* The first event of a kind of a city at or after "from", with a detail. */
internal u32
feature_count_city_events(engine_api *api, u8 city, sim_event_kind kind, u32 from)
{
    engine_ledger_view view = api->ledger_filter(api->engine, city, ENGINE_ANY_RESOURCE);
    u32 found = 0;
    for (u32 index = 0; index < view.count; ++index)
    {
        if (view.indices[index] >= from && api->ledger_at(api->engine, view.indices[index]).kind == kind) found++;
    }
    return found;
}

internal void
feature_disaster_effect_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_city_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 city = setup.city, force = setup.force;

    TEST_CHECK(counts, api->disaster_kind_count(e) == 4 && api->content_status(e).disaster_count == 4, "the content pack has four disasters");
    engine_disaster_kind locusts = api->disaster_kind_at(e, FEATURE_DISASTER_LOCUSTS);
    TEST_CHECK(counts, locusts.month_mask == ((1u << 1) | (1u << 4) | (1u << 7)) &&
                       api->disaster_kind_at(e, FEATURE_DISASTER_PLAGUE).month_mask == ((1u << 1) | (1u << 4) | (1u << 7) | (1u << 10)) &&
                       api->disaster_kind_at(e, FEATURE_DISASTER_HARVEST).month_mask == (1u << 7),
               "locusts in months 1, 4 and 7; plague also in month 10; the good harvest only in month 7 (documented)");

    /* Locusts: order at the start, the farms each turn, half of the grain payment. */
    u8 farm = feature_build(api, city, FEATURE_FACILITY_FARM, force);
    TEST_CHECK(counts, farm != ENGINE_ANY_SLOT, "the city builds a farm");
    api->set_city_resource(e, city, SIM_RESOURCE_PUBLIC_ORDER, 0, 60);
    u32 mark = api->ledger_count(e);
    engine_command_result started = api->start_disaster(e, city, FEATURE_DISASTER_LOCUSTS);
    sim_event order = {};
    TEST_CHECK(counts, started.refusal == SIM_REFUSAL_NONE && api->city_disasters(e, city).turns[FEATURE_DISASTER_LOCUSTS] == 9 &&
                       api->city_disasters(e, city).running == 1 && feature_count_city_events(api, city, SIM_EVENT_DISASTER_STARTED, mark) == 1 &&
                       feature_find_city_event(api, city, SIM_EVENT_ORDER_LOST, mark, &order) && order.delta == -10 && order.detail == 1 + FEATURE_DISASTER_LOCUSTS &&
                       order.assumed && api->city_economy(e, city).public_order == 50,
               "locusts start: public order -10 and 9 turns (assumed)");
    TEST_CHECK(counts, api->start_disaster(e, city, FEATURE_DISASTER_LOCUSTS).refusal == SIM_REFUSAL_NO_DISASTER &&
                       api->start_disaster(e, city, 9).refusal == SIM_REFUSAL_NO_DISASTER && api->start_disaster(e, 200, 0).refusal == SIM_REFUSAL_NO_CITY,
               "refused: the disaster runs, no such disaster, no such city");
    mark = api->ledger_count(e);
    feature_next_own_turn(api, force);
    sim_event damaged = {};
    TEST_CHECK(counts, feature_find_city_event(api, city, SIM_EVENT_FACILITY_DAMAGED, mark, &damaged) && damaged.detail == farm && damaged.delta == -200 &&
                       api->city_disasters(e, city).turns[FEATURE_DISASTER_LOCUSTS] == 8,
               "each turn the locusts take 200 durability from a farm (assumed) and count down");
    b32 grain_lost = false, ended = false;
    for (u32 turn = 0; turn < 10 && !ended; ++turn)
    {
        mark = api->ledger_count(e);
        feature_next_own_turn(api, force);
        sim_event income = {}, lost = {};
        if (feature_find_city_event(api, city, SIM_EVENT_GRAIN_INCOME, mark, &income) && feature_find_city_event(api, city, SIM_EVENT_GRAIN_LOST, mark, &lost))
        {
            grain_lost = lost.delta == -(s32)(income.delta / 2) && lost.assumed;
        }
        ended = feature_count_city_events(api, city, SIM_EVENT_DISASTER_ENDED, mark) == 1;
    }
    TEST_CHECK(counts, grain_lost, "at the grain payment the locusts take half of it (assumed)");
    TEST_CHECK(counts, ended && api->city_disasters(e, city).running == 0, "the locusts stop by themselves");

    /* Plague: soldiers each turn. */
    api->set_city_resource(e, city, SIM_RESOURCE_SOLDIERS, 0, 10000);
    api->start_disaster(e, city, FEATURE_DISASTER_PLAGUE);
    mark = api->ledger_count(e);
    feature_next_own_turn(api, force);
    sim_event soldiers = {};
    TEST_CHECK(counts, feature_find_city_event(api, city, SIM_EVENT_SOLDIERS_LOST, mark, &soldiers) && soldiers.delta == -500 && soldiers.assumed,
               "the plague takes 5 percent of the soldiers each turn (assumed)");

    /* Bandits: until public order reaches 80. */
    api->set_city_resource(e, city, SIM_RESOURCE_PUBLIC_ORDER, 0, 50);
    api->start_disaster(e, city, FEATURE_DISASTER_BANDITS);
    TEST_CHECK(counts, api->city_disasters(e, city).turns[FEATURE_DISASTER_BANDITS] == ENGINE_DISASTER_OPEN, "bandits have no length");
    mark = api->ledger_count(e);
    feature_next_own_turn(api, force);
    TEST_CHECK(counts, feature_count_city_events(api, city, SIM_EVENT_ORDER_LOST, mark) >= 1 &&
                       api->city_disasters(e, city).turns[FEATURE_DISASTER_BANDITS] == ENGINE_DISASTER_OPEN,
               "the bandits take public order each turn and stay");
    api->set_city_resource(e, city, SIM_RESOURCE_PUBLIC_ORDER, 0, 80);
    mark = api->ledger_count(e);
    feature_next_own_turn(api, force);
    TEST_CHECK(counts, api->city_disasters(e, city).turns[FEATURE_DISASTER_BANDITS] == 0 &&
                       feature_count_city_events(api, city, SIM_EVENT_DISASTER_ENDED, mark) >= 1,
               "at public order 80 the bandits stop (assumed until the bandit base of M6)");

    /* The good harvest acts at once. */
    mark = api->ledger_count(e);
    u32 season = api->city_income_forecast(e, city).amounts.grain_season;
    api->start_disaster(e, city, FEATURE_DISASTER_HARVEST);
    sim_event harvest = {};
    TEST_CHECK(counts, feature_find_city_event(api, city, SIM_EVENT_GRAIN_HARVEST, mark, &harvest) && harvest.delta == (s32)(season / 2) &&
                       api->city_disasters(e, city).turns[FEATURE_DISASTER_HARVEST] == 0,
               "the good harvest gives half of the grain of a season at once (assumed)");
    engine_fixture_destroy(&setup.fixture);
}

/* Two years with the disasters on: each disaster starts on day 1 of one of
   its months. With the setting off, nothing starts. */
internal void
feature_disaster_calendar_tests(test_counts *counts)
{
    engine_fixture f;
    engine_fixture_init(&f, 7, 65536);
    engine_api *api = &f.api;
    void *e = api->engine;
    api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
    TEST_CHECK(counts, !api->settings(e).disasters, "the test fixture turns the disasters off");
    u32 mark = api->ledger_count(e);
    for (u32 turn = 0; turn < 36; ++turn) feature_next_turn(api);
    TEST_CHECK(counts, feature_count_events(api, SIM_EVENT_DISASTER_STARTED, mark) == 0, "with the setting off no disaster starts in a year");

    engine_settings settings = api->settings(e);
    settings.disasters = true;
    api->set_settings(e, settings);
    b32 months_ok = true;
    u32 started = 0, kinds_seen = 0;
    for (u32 turn = 0; turn < 72; ++turn)
    {
        mark = api->ledger_count(e);
        feature_next_turn(api);
        engine_date date = api->date(e);
        for (u32 index = mark; index < api->ledger_count(e); ++index)
        {
            sim_event event = api->ledger_at(e, index);
            if (event.kind != SIM_EVENT_DISASTER_STARTED) continue;
            started++;
            kinds_seen |= 1u << event.detail;
            engine_disaster_kind kind = api->disaster_kind_at(e, event.detail);
            if (date.day != 1 || !((kind.month_mask >> date.month) & 1) || !event.assumed) months_ok = false;
        }
    }
    TEST_CHECK(counts, started > 0 && months_ok, "in two years disasters start, each on day 1 of one of its months");
    TEST_CHECK(counts, (kinds_seen & (1u << FEATURE_DISASTER_LOCUSTS)) && (kinds_seen & (1u << FEATURE_DISASTER_PLAGUE)),
               "locusts and plague both start in two years of 42 cities");
    engine_fixture_destroy(&f);
}

/* ---- the PK facilities --------------------------------------------------- */

internal void
feature_pk_facility_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_city_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 city = setup.city, force = setup.force;

    /* 符節台: more action points at the start of the turn. */
    api->set_actions(e, force, 0);
    u32 mark = api->ledger_count(e);
    feature_next_own_turn(api, force);
    sim_event before = {};
    feature_find_event(api, SIM_EVENT_ACTION_POINTS_RECOVERED, mark, &before);
    while (before.faction != force && feature_find_event(api, SIM_EVENT_ACTION_POINTS_RECOVERED, ++mark, &before)) {}
    u8 tally = feature_build(api, city, FEATURE_FACILITY_TALLY, force);
    api->set_actions(e, force, 0);
    mark = api->ledger_count(e);
    feature_next_own_turn(api, force);
    sim_event after = {};
    b32 found = false;
    for (u32 index = mark; index < api->ledger_count(e); ++index)
    {
        sim_event event = api->ledger_at(e, index);
        if (event.kind == SIM_EVENT_ACTION_POINTS_RECOVERED && event.faction == force) { after = event; found = true; }
    }
    TEST_CHECK(counts, tally != ENGINE_ANY_SLOT && found && before.faction == force && after.delta == before.delta + 10 && after.assumed,
               "a tally office adds 10 action points at the start of each turn (assumed)");

    /* 軍事府: train costs half the action points. */
    u16 ids[3];
    feature_take_officers(api, city, ids, 1);
    api->set_city_resource(e, city, SIM_RESOURCE_MORALE, 0, 50);
    engine_command train = feature_city_command(ENGINE_COMMAND_TRAIN, city, ids[0]);
    TEST_CHECK(counts, api->can_issue(e, &train).action_points == 20, "train costs 20 action points");
    TEST_CHECK(counts, feature_build(api, city, FEATURE_FACILITY_MILITARY, force) != ENGINE_ANY_SLOT, "the city builds a military office");
    feature_take_officers(api, city, ids, 1);
    train = feature_city_command(ENGINE_COMMAND_TRAIN, city, ids[0]);
    TEST_CHECK(counts, api->can_issue(e, &train).action_points == 10, "with a military office train costs 10 action points");

    /* 鍊兵所: ships take less time. */
    TEST_CHECK(counts, feature_build(api, city, FEATURE_FACILITY_SHIPYARD, force) != ENGINE_ANY_SLOT, "the city builds a shipyard");
    feature_take_officers(api, city, ids, 1);
    engine_command produce = feature_city_command(ENGINE_COMMAND_PRODUCE, city, ids[0]);
    produce.weapon = FEATURE_WEAPON_TOWER_SHIP;
    u32 days = api->can_issue(e, &produce).days;
    TEST_CHECK(counts, feature_build(api, city, FEATURE_FACILITY_DRILL_GROUND, force) != ENGINE_ANY_SLOT, "the city builds a drill ground");
    feature_take_officers(api, city, ids, 1);
    produce = feature_city_command(ENGINE_COMMAND_PRODUCE, city, ids[0]);
    produce.weapon = FEATURE_WEAPON_TOWER_SHIP;
    TEST_CHECK(counts, days > 0 && api->can_issue(e, &produce).days == Maximum(1u, calendar_turns_for_days(days * 75 / 100)) * 10,
               "with a drill ground a ship takes 75 percent of the period (assumed)");

    /* 人材府: research takes less time in the city. */
    api->set_technique_points(e, force, 10000);
    feature_take_officers(api, city, ids, 1);
    engine_command research = feature_research_command(city, FEATURE_TECH_PIKE_DRILL, ids);
    u32 research_days = api->can_issue(e, &research).days;
    TEST_CHECK(counts, feature_build(api, city, FEATURE_FACILITY_PERSONNEL, force) != ENGINE_ANY_SLOT, "the city builds a personnel office");
    feature_take_officers(api, city, ids, 1);
    research = feature_research_command(city, FEATURE_TECH_PIKE_DRILL, ids);
    TEST_CHECK(counts, research_days > 0 && api->can_issue(e, &research).days == Maximum(1u, calendar_turns_for_days(research_days * 75 / 100)) * 10,
               "with a personnel office a research takes 75 percent of the period (assumed)");
    engine_fixture_destroy(&setup.fixture);
}

/* ---- victory and defeat -------------------------------------------------- */

internal void
feature_outcome_tests(test_counts *counts)
{
    /* Victory: the player owns every city that has an owner. */
    {
        engine_fixture f;
        engine_fixture_init(&f, 1, 65536);
        engine_api *api = &f.api;
        void *e = api->engine;
        api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
        u8 player = api->whose_turn(e);
        TEST_CHECK(counts, api->outcome(e).kind == SIM_OUTCOME_NONE && api->outcome(e).end_year == 351, "the game runs; it ends in the year 351");
        for (u32 index = 0; index < api->city_count(e); ++index)
        {
            engine_city city = api->city_at(e, index);
            if (city.owner_force_id != ENGINE_NO_FACTION) api->set_city_owner(e, city.city_id, player);
        }
        u32 mark = api->ledger_count(e);
        api->end_turn(e);
        sim_event won = {};
        TEST_CHECK(counts, feature_find_event(api, SIM_EVENT_GAME_WON, mark, &won) && won.faction == player &&
                           api->outcome(e).kind == SIM_OUTCOME_VICTORY && api->outcome(e).force_id == player,
               "victory: the faction of the player owns every city that has an owner (empty cities do not count)");
        s64 day = api->date(e).days_since_start;
        api->end_turn(e);
        TEST_CHECK(counts, api->date(e).days_since_start == day, "after the end of the game, proceed does nothing");
        u16 ids[3];
        u8 city = feature_busiest_city(api);
        feature_take_officers(api, city, ids, 1);
        TEST_CHECK(counts, api->command_patrol(e, city, ids).refusal == SIM_REFUSAL_GAME_OVER, "after the end of the game each command refuses");
        engine_fixture_destroy(&f);
    }
    /* Defeat: the faction has no city. */
    {
        engine_fixture f;
        engine_fixture_init(&f, 1, 65536);
        engine_api *api = &f.api;
        void *e = api->engine;
        api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
        u8 player = api->whose_turn(e);
        u8 other = ENGINE_NO_FACTION;
        for (u32 index = 0; index < api->city_count(e) && other == ENGINE_NO_FACTION; ++index)
        {
            u8 owner = api->city_at(e, index).owner_force_id;
            if (owner != ENGINE_NO_FACTION && owner != player) other = owner;
        }
        for (u32 index = 0; index < api->city_count(e); ++index)
        {
            engine_city city = api->city_at(e, index);
            if (city.owner_force_id == player) api->set_city_owner(e, city.city_id, other);
        }
        u32 mark = api->ledger_count(e);
        feature_next_turn(api);
        sim_event lost = {};
        TEST_CHECK(counts, feature_find_event(api, SIM_EVENT_GAME_LOST, mark, &lost) && lost.faction == player && lost.detail == SIM_OUTCOME_DEFEAT_NO_CITY &&
                           api->outcome(e).kind == SIM_OUTCOME_DEFEAT_NO_CITY,
               "defeat: the faction of the player has no city");
        engine_fixture_destroy(&f);
    }
    /* Defeat: the year becomes 351. */
    {
        engine_fixture f;
        engine_fixture_init(&f, 1, 65536);
        engine_api *api = &f.api;
        void *e = api->engine;
        api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
        u8 player = api->whose_turn(e);
        api->set_date(e, 350, 12, 21);
        u32 mark = api->ledger_count(e);
        feature_next_turn(api);
        sim_event lost = {};
        TEST_CHECK(counts, api->date(e).year == 351 && feature_find_event(api, SIM_EVENT_GAME_LOST, mark, &lost) && lost.faction == player &&
                           lost.detail == SIM_OUTCOME_DEFEAT_TIME && api->outcome(e).kind == SIM_OUTCOME_DEFEAT_TIME,
               "defeat: the year becomes 351");
        engine_fixture_destroy(&f);
    }
}

internal void
feature_growth_tests(test_counts *counts)
{
    feature_research_tests(counts);
    feature_technique_effect_tests(counts);
    feature_merge_tests(counts);
    feature_disaster_effect_tests(counts);
    feature_disaster_calendar_tests(counts);
    feature_pk_facility_tests(counts);
    feature_outcome_tests(counts);
}
