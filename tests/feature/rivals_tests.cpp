/* Feature tests of M8 rivals (knowledge/ai.md, factions-corps-and-posts.md,
   city-commands.md, events.md, bases-ports-and-passes.md, siege-and-
   bases.md; F-032, F-120 to F-123, F-130), through engine_api with the real
   engine and the sample save: what the AI issues for a given state, the
   turn of the computer factions, 軍團 and the delegated corps, 軍師, 評定,
   the Han emperor, the events of events.tsv, and the ports and passes. The
   tables of the AI, the corps points and the event amounts are assumed
   (Q-183 to Q-186). */

#define FEATURE_PATROL_PRIORITY    60
#define FEATURE_TRAIN_PRIORITY     40
#define FEATURE_AI_URGENCY         20
#define FEATURE_ADVISER_INTELLECT  70
#define FEATURE_ADVICE_PER_INTELLECT 20
#define FEATURE_EMPEROR_TECHNIQUE  50
#define FEATURE_EMPEROR_CITY       15
#define FEATURE_BASE_GOLD          100
#define FEATURE_EVENT_TREASON      0
#define FEATURE_EVENT_BRONZE_BIRD  1
#define FEATURE_EVENT_XIAN_DEATH   10
#define FEATURE_TREASON_INFORMER_LOSS 20

/* The city where the ruler of the faction stands, or ENGINE_NO_CITY. */
internal u8
feature_ruler_city(engine_api *api, u8 force)
{
    engine_officer ruler = {};
    if (!feature_officer_by_id(api, api->force_ruler(api->engine, force), &ruler) || ruler.current_position >= SAVE_CITY_COUNT) return ENGINE_NO_CITY;
    return (u8)ruler.current_position;
}

/* Another city of the faction with a free officer, not the city of the ruler. */
internal u8
feature_corps_city(engine_api *api, u8 force)
{
    u8 ruler_city = feature_ruler_city(api, force);
    u16 ids[8];
    for (u32 city = 0; city < api->city_count(api->engine); ++city)
    {
        if (city == ruler_city || api->city_at(api->engine, city).owner_force_id != force) continue;
        if (feature_free_officers(api, (u8)city, ids, ArrayCount(ids))) return (u8)city;
    }
    return ENGINE_NO_CITY;
}

/* Every city of the faction calm and full of soldiers: the AI then has no
   urgent need, and a test changes one city to make one. */
internal void
feature_calm_faction(engine_api *api, u8 force)
{
    for (u32 city = 0; city < api->city_count(api->engine); ++city)
    {
        if (api->city_at(api->engine, city).owner_force_id != force) continue;
        api->set_city_resource(api->engine, (u8)city, SIM_RESOURCE_PUBLIC_ORDER, 0, 100);
        api->set_city_resource(api->engine, (u8)city, SIM_RESOURCE_SOLDIERS, 0, 1000000);
        api->set_city_resource(api->engine, (u8)city, SIM_RESOURCE_MORALE, 0, 100);
    }
}

/* The first proposal of the plan for the city, or 0. M15: the sorties of
   the rows defend and take_free depend on the units and free cities near
   the city, so the city tests of M8 skip them. */
internal engine_proposal *
feature_first_for_city(engine_plan_view *view, u8 city)
{
    for (u32 index = 0; index < view->count; ++index)
    {
        if (view->items[index].city_id == city && view->items[index].kind != SIM_COMMAND_SORTIE) return view->items + index;
    }
    return 0;
}

/* The free officer of the city with the best max(leadership, charm), and
   one with intellect at least (or below) a bound. */
internal u16
feature_pick_officer(engine_api *api, u8 city, u32 intellect_bound, b32 at_least)
{
    u16 ids[32];
    u32 count = feature_free_officers(api, city, ids, ArrayCount(ids));
    u16 best = ENGINE_NO_OFFICER;
    u32 best_value = 0;
    for (u32 index = 0; index < count; ++index)
    {
        engine_officer officer = {};
        feature_officer_by_id(api, ids[index], &officer);
        if (officer.status == OFFICER_STATUS_RULER) continue;
        if (intellect_bound && (at_least ? officer.intelligence < intellect_bound : officer.intelligence >= intellect_bound)) continue;
        u32 value = Maximum((u32)officer.leadership, (u32)officer.charm);
        if (best == ENGINE_NO_OFFICER || value > best_value) { best = officer.officer_id; best_value = value; }
    }
    return best;
}

/* An officer of the faction anywhere, with intellect at least (or below) a bound. */
internal u16
feature_faction_officer(engine_api *api, u8 force, u32 intellect_bound, b32 at_least)
{
    for (u32 index = 0; index < api->officer_count(api->engine); ++index)
    {
        engine_officer officer = api->officer_at(api->engine, index);
        if (officer.force_id != force || officer.status == OFFICER_STATUS_RULER || officer.status > 3 || officer.current_position >= SAVE_CITY_COUNT) continue;
        if (at_least ? officer.intelligence >= intellect_bound : officer.intelligence < intellect_bound) return officer.officer_id;
    }
    return ENGINE_NO_OFFICER;
}

internal sim_corps_order
feature_corps_order(u8 action, u8 corps, u16 viceroy, u8 city)
{
    sim_corps_order order = {};
    order.action         = action;
    order.corps          = corps;
    order.viceroy        = viceroy;
    order.cities         = city < SAVE_CITY_COUNT ? 1ull << city : 0;
    order.policy         = CORPS_POLICY_DELEGATE;
    order.target         = SIM_NO_FORCE;
    order.flags          = CORPS_FLAG_ALL;
    order.transport_city = SIM_NO_CITY;
    return order;
}

internal void
feature_turn_on(engine_api *api, b32 ai, b32 events)
{
    engine_settings settings = api->settings(api->engine);
    settings.ai     = ai;
    settings.events = events;
    api->set_settings(api->engine, settings);
}

/* Loads the sample and plays the faction with the most cities whose ruler
   stands in a city (the council and the treason event need him there);
   setup->city is the city of the ruler. Each city of the faction gets gold. */
internal void
feature_rivals_setup_init(feature_city_setup *setup)
{
    engine_fixture_init(&setup->fixture, 1, 65536);
    setup->api = &setup->fixture.api;
    engine_api *api = setup->api;
    void *e = api->engine;
    api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
    u32 best_cities = 0;
    setup->force = ENGINE_NO_FACTION;
    setup->city  = ENGINE_NO_CITY;
    for (u32 force = 0; force < SIM_FORCE_CAPACITY; ++force)
    {
        u8 ruler_city = feature_ruler_city(api, (u8)force);
        if (ruler_city == ENGINE_NO_CITY || api->city_at(e, ruler_city).owner_force_id != force) continue;
        u32 cities = 0;
        for (u32 city = 0; city < api->city_count(e); ++city) cities += api->city_at(e, city).owner_force_id == force;
        if (cities > best_cities) { best_cities = cities; setup->force = (u8)force; setup->city = ruler_city; }
    }
    feature_play_as(api, setup->force);
    for (u32 city = 0; city < api->city_count(e); ++city) if (api->city_at(e, city).owner_force_id == setup->force) api->set_city_gold(e, (u8)city, 50000);
    api->set_actions(e, setup->force, 255);
}

/* ---- F-123: the AI planner ----------------------------------------------------------- */

internal void
feature_ai_plan_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_rivals_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 city = setup.city, force = setup.force;
    TEST_CHECK(counts, api->content_status(e).priority_count == 12, "ai_priorities.tsv has the 12 rows of the planner (M15 adds defend and take_free)");

    feature_calm_faction(api, force);
    api->set_city_resource(e, city, SIM_RESOURCE_PUBLIC_ORDER, 0, 0);
    engine_plan_view plan = api->ai_plan(e, force);
    TEST_CHECK(counts, plan.count > 0 && plan.items[0].kind == SIM_COMMAND_PATROL && plan.items[0].city_id == city &&
                       plan.items[0].score == FEATURE_PATROL_PRIORITY + FEATURE_AI_URGENCY && strcmp(plan.items[0].key, "patrol") == 0 &&
                       plan.items[0].officer != ENGINE_NO_OFFICER,
               "given public order 0, the AI issues 巡察 in that city first (priority 60 + urgency 20)");
    for (u32 index = 1; index < plan.count; ++index)
    {
        if (plan.items[index].score > plan.items[index - 1].score) { TEST_CHECK(counts, false, "the plan is sorted by score"); break; }
    }

    api->set_city_resource(e, city, SIM_RESOURCE_PUBLIC_ORDER, 0, 100);
    api->set_city_resource(e, city, SIM_RESOURCE_MORALE, 0, 0);
    plan = api->ai_plan(e, force);
    engine_proposal *first = feature_first_for_city(&plan, city);
    TEST_CHECK(counts, first && first->kind == SIM_COMMAND_TRAIN && first->score == FEATURE_TRAIN_PRIORITY + FEATURE_AI_URGENCY,
               "given morale 0, the AI issues 訓練 in that city first (priority 40 + urgency 20, above 開發 at 50)");
    api->set_city_resource(e, city, SIM_RESOURCE_MORALE, 0, 100);
    api->set_city_resource(e, city, SIM_RESOURCE_SOLDIERS, 0, 1000000);
    first = feature_first_for_city(&(plan = api->ai_plan(e, force)), city);
    TEST_CHECK(counts, !first || (first->kind != SIM_COMMAND_PATROL && first->kind != SIM_COMMAND_CONSCRIPT),
               "given a calm city full of soldiers, the AI does not patrol or conscript there");

    api->set_actions(e, force, 0);
    TEST_CHECK(counts, api->ai_plan(e, force).count == 0, "given no action points, the AI proposes nothing: each proposal passes the check of its command");
    api->set_actions(e, force, 255);
    engine_fixture_destroy(&setup.fixture);
}

/* The computer factions act on their turns with the same commands. */
internal void
feature_ai_turn_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_rivals_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 force = setup.force;

    u32 mark = api->ledger_count(e);
    feature_next_own_turn(api, force);
    TEST_CHECK(counts, feature_count_events(api, SIM_EVENT_AI_PLANNED, mark) == 0, "with the AI off, the computer factions only pass");

    feature_turn_on(api, true, false);
    mark = api->ledger_count(e);
    feature_next_own_turn(api, force);
    u32 planned = 0, acted = 0, own = 0, commands = 0;
    u32 count = api->ledger_count(e);
    for (u32 index = mark; index < count; ++index)
    {
        sim_event event = api->ledger_at(e, index);
        if (event.kind == SIM_EVENT_AI_PLANNED)
        {
            planned++;
            acted += event.delta > 0;
            own   += event.faction == force;
        }
        commands += event.kind == SIM_EVENT_ACTION_POINTS_SPENT;
    }
    TEST_CHECK(counts, planned > 0 && acted > 0 && own == 0 && commands > 0,
               "with the AI on, each computer faction plans its turn and pays action points for its commands; the human faction does not plan");
    engine_fixture_destroy(&setup.fixture);
}

/* ---- F-120: corps and delegation --------------------------------------------------- */

internal void
feature_corps_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_rivals_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 force = setup.force;
    u8 ruler_city = feature_ruler_city(api, force);
    u8 city = feature_corps_city(api, force);
    TEST_CHECK(counts, ruler_city != ENGINE_NO_CITY && city != ENGINE_NO_CITY, "the faction has a city of the ruler and another city with officers");
    if (city == ENGINE_NO_CITY) { engine_fixture_destroy(&setup.fixture); return; }
    u16 viceroy = feature_pick_officer(api, city, 0, true);

    TEST_CHECK(counts, !api->corps_at(e, force, 1).alive && api->city_corps(e, city) == 0, "at the start all cities belong to the corps of the ruler");
    sim_corps_order order = feature_corps_order(CORPS_CREATE, 0, viceroy, ruler_city);
    TEST_CHECK(counts, api->command_corps(e, &order).refusal == SIM_REFUSAL_NO_CITY, "refused: the city of the ruler stays in his corps");
    order = feature_corps_order(CORPS_CREATE, 0, api->force_ruler(e, force), city);
    TEST_CHECK(counts, api->command_corps(e, &order).refusal == SIM_REFUSAL_NO_OFFICER, "refused: the ruler cannot be a 都督");
    order = feature_corps_order(CORPS_DISSOLVE, 3, ENGINE_NO_OFFICER, ENGINE_NO_CITY);
    TEST_CHECK(counts, api->command_corps(e, &order).refusal == SIM_REFUSAL_NO_CORPS, "refused: no corps to dissolve");

    u32 mark = api->ledger_count(e);
    order = feature_corps_order(CORPS_CREATE, 0, viceroy, city);
    engine_command_result result = api->command_corps(e, &order);
    engine_corps corps = api->corps_at(e, force, 1);
    engine_officer officer = {};
    feature_officer_by_id(api, viceroy, &officer);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && corps.alive && corps.viceroy == viceroy && corps.city_count == 1 &&
                       api->city_corps(e, city) == 1 && officer.status == OFFICER_STATUS_VICEROY,
               "軍團: a new corps 1 takes the city; its 都督 has the viceroy status");
    TEST_CHECK(counts, feature_count_events(api, SIM_EVENT_CORPS_CREATED, mark) == 1, "the ledger has the new corps");
    sim_corps_order second = feature_corps_order(CORPS_CREATE, 0, viceroy, city);
    TEST_CHECK(counts, api->command_corps(e, &second).refusal == SIM_REFUSAL_OFFICER_BUSY, "refused: one officer leads one corps");

    /* The delegated corps acts at the start of each turn of its faction. */
    feature_turn_on(api, true, false);
    b32 patrolled = false;
    u32 points = 0;
    for (u32 attempt = 0; attempt < 4 && !patrolled; ++attempt)
    {
        api->set_city_gold(e, city, 50000);
        api->set_city_resource(e, city, SIM_RESOURCE_PUBLIC_ORDER, 0, 0);
        mark = api->ledger_count(e);
        feature_next_own_turn(api, force);
        sim_event found = {};
        patrolled = feature_find_city_event(api, city, SIM_EVENT_PATROLLED, mark, &found);
        points = Maximum(points, api->corps_at(e, force, 1).action_points);
    }
    TEST_CHECK(counts, patrolled, "the delegated corps patrols its city with its own action points (assumed: max(統率, 魅力) / 4 each turn)");
    feature_turn_on(api, false, false);

    mark = api->ledger_count(e);
    order = feature_corps_order(CORPS_DISSOLVE, 1, ENGINE_NO_OFFICER, ENGINE_NO_CITY);
    result = api->command_corps(e, &order);
    feature_officer_by_id(api, viceroy, &officer);
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && !api->corps_at(e, force, 1).alive && api->city_corps(e, city) == 0 &&
                       officer.status == OFFICER_STATUS_REGULAR_OFFICER && feature_count_events(api, SIM_EVENT_CORPS_DISSOLVED, mark) == 1,
               "dissolve: the cities return to the corps of the ruler; the 都督 is a regular officer again");
    (void)points;
    engine_fixture_destroy(&setup.fixture);
}

/* ---- F-121: council and adviser ---------------------------------------------------- */

internal void
feature_council_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_rivals_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 force = setup.force;
    u8 ruler_city = feature_ruler_city(api, force);
    u8 other_city = feature_corps_city(api, force);

    u16 weak = feature_faction_officer(api, force, FEATURE_ADVISER_INTELLECT, false);
    u16 wise = feature_faction_officer(api, force, FEATURE_ADVISER_INTELLECT, true);
    TEST_CHECK(counts, weak != ENGINE_NO_OFFICER && wise != ENGINE_NO_OFFICER, "the faction has officers below and above intellect 70");
    engine_plan_view advice = api->advice(e, force);
    TEST_CHECK(counts, api->adviser(e, force) == ENGINE_NO_OFFICER && advice.count == 0 && advice.refusal == SIM_REFUSAL_NO_OFFICER,
               "without a 軍師 there is no advice");
    TEST_CHECK(counts, api->command_adviser(e, force, weak).refusal == SIM_REFUSAL_NO_APTITUDE, "refused: a 軍師 needs intellect 70 (assumed, Q-186)");
    u32 mark = api->ledger_count(e);
    TEST_CHECK(counts, api->command_adviser(e, force, wise).refusal == SIM_REFUSAL_NONE && api->adviser(e, force) == wise &&
                       feature_count_events(api, SIM_EVENT_ADVISER_APPOINTED, mark) == 1,
               "軍師: the officer becomes the adviser of the faction");
    feature_calm_faction(api, force);
    api->set_city_resource(e, ruler_city, SIM_RESOURCE_PUBLIC_ORDER, 0, 0);
    engine_officer adviser = {};
    feature_officer_by_id(api, wise, &adviser);
    advice = api->advice(e, force);
    engine_plan_view plan = api->ai_plan(e, force);
    u32 shown = Minimum(Maximum(1u, (u32)adviser.intelligence / FEATURE_ADVICE_PER_INTELLECT), plan.count);
    TEST_CHECK(counts, advice.count == shown && advice.count > 0 && advice.items[0].kind == plan.items[0].kind && advice.items[0].city_id == plan.items[0].city_id,
               "the advice is the first proposals of the planner, one for each 20 points of intellect (assumed)");

    engine_plan_view council = api->council(e, force, ruler_city);
    b32 one_city = council.count > 0;
    for (u32 index = 0; index < council.count; ++index) one_city &= council.items[index].city_id == ruler_city;
    TEST_CHECK(counts, council.refusal == SIM_REFUSAL_NONE && one_city && council.items[0].kind == SIM_COMMAND_PATROL,
               "評定: the council proposes commands for the city of the ruler, the most urgent first");
    if (other_city != ENGINE_NO_CITY)
    {
        TEST_CHECK(counts, api->council(e, force, other_city).refusal == SIM_REFUSAL_RULER_ABSENT &&
                           api->command_council(e, force, other_city, 0).refusal == SIM_REFUSAL_RULER_ABSENT,
                   "refused: the council meets only where the ruler stands");
    }
    TEST_CHECK(counts, api->command_council(e, force, ruler_city, ENGINE_PLAN_CAPACITY).refusal == SIM_REFUSAL_NO_TARGET, "refused: no such proposal");
    mark = api->ledger_count(e);
    engine_command_result result = api->command_council(e, force, ruler_city, 0);
    sim_event patrolled = {};
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && feature_find_city_event(api, ruler_city, SIM_EVENT_PATROLLED, mark, &patrolled) &&
                       patrolled.cause == result.command_id,
               "the officers act on the chosen proposal: the ledger has the patrol with the council command as its cause");
    engine_fixture_destroy(&setup.fixture);
}

/* ---- F-122: the Han emperor ---------------------------------------------------------- */

internal void
feature_emperor_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_rivals_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 force = setup.force;

    engine_date date = api->date(e);
    u32 stamp = (u32)date.year * 12 + date.month;
    u8 expected = (u8)(stamp < 189u * 12 + 4 ? EMPEROR_LING : stamp < 189u * 12 + 9 ? EMPEROR_SHAO : stamp < 234u * 12 + 4 ? EMPEROR_XIAN : EMPEROR_NONE);
    engine_emperor emperor = api->emperor(e);
    u8 owner = api->city_at(e, FEATURE_EMPEROR_CITY).owner_force_id;
    TEST_CHECK(counts, emperor.who == expected && emperor.city == FEATURE_EMPEROR_CITY && emperor.protector == (expected == EMPEROR_NONE ? ENGINE_NO_FACTION : owner),
               "the emperor of the start date lives in 洛陽 (assumed, Q-186); the owner of the city protects him");

    api->set_emperor(e, EMPEROR_XIAN, setup.city);
    TEST_CHECK(counts, api->emperor(e).protector == force, "the owner of the city of the emperor is his protector");
    api->set_technique_points(e, force, 0);
    u8 month = api->date(e).month;
    u32 mark = api->ledger_count(e);
    for (u32 step = 0; step < 4 && api->date(e).month == month; ++step) feature_next_own_turn(api, force);
    b32 gained = false;
    u32 count = api->ledger_count(e);
    for (u32 index = mark; index < count; ++index)
    {
        sim_event event = api->ledger_at(e, index);
        gained |= event.kind == SIM_EVENT_TECHNIQUE_GAINED && event.faction == force && event.delta == FEATURE_EMPEROR_TECHNIQUE;
    }
    TEST_CHECK(counts, gained && api->technique_points(e, force) >= FEATURE_EMPEROR_TECHNIQUE,
               "each month the protector gains 50 technique points (assumed)");

    mark = api->ledger_count(e);
    engine_command_result result = api->trigger_event(e, FEATURE_EVENT_XIAN_DEATH, ENGINE_NO_FACTION, ENGINE_NO_CITY, ENGINE_NO_OFFICER);
    sim_event changed = {};
    TEST_CHECK(counts, result.refusal == SIM_REFUSAL_NONE && api->emperor(e).who == EMPEROR_NONE && api->emperor(e).protector == ENGINE_NO_FACTION &&
                       feature_find_event(api, SIM_EVENT_EMPEROR_CHANGED, mark, &changed) && changed.detail == EMPEROR_NONE &&
                       feature_count_events(api, SIM_EVENT_EVENT_FIRED, mark) == 1,
               "獻帝駕崩: after the event there is no emperor and no protector");
    engine_fixture_destroy(&setup.fixture);
}

/* ---- F-130: the events table -------------------------------------------------------- */

internal void
feature_event_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_rivals_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 force = setup.force;

    engine_event_kind treason = api->event_kind_at(e, FEATURE_EVENT_TREASON), bronze = api->event_kind_at(e, FEATURE_EVENT_BRONZE_BIRD);
    engine_event_kind transport = api->event_kind_at(e, 21);
    TEST_CHECK(counts, api->event_kind_count(e) == 22 && api->content_status(e).event_count == 22 && strcmp(treason.key, "treason") == 0 &&
                       treason.check == EVENT_CHECK_QUARTER && treason.min_days == 360 && bronze.chance == 100 &&
                       strcmp(transport.key, "transport_arrived") == 0 && transport.check == EVENT_CHECK_NONE,
               "events.tsv has the 21 generic events of events.md with their checks and chances, and the question of a transport (G2-027)");
    TEST_CHECK(counts, api->trigger_event(e, 200, force, ENGINE_NO_CITY, ENGINE_NO_OFFICER).refusal == SIM_REFUSAL_NO_EVENT, "refused: no such event");
    TEST_CHECK(counts, api->command_event_choice(e, 0).refusal == SIM_REFUSAL_NO_EVENT, "refused: no event waits for a choice");
    TEST_CHECK(counts, api->relic_count(e) == 7 && api->relic_at(e, 0).alive && api->relic_at(e, 0).kind == RELIC_RUIN && api->relic_at(e, 1).kind == RELIC_TEMPLE,
               "the map has one ruin and six temples (placeholder places, Q-034)");

    /* 謀反的嫌疑 needs a corps whose 都督 is not loyal. */
    u8 city = feature_corps_city(api, force);
    u16 viceroy = city != ENGINE_NO_CITY ? feature_pick_officer(api, city, 0, true) : (u16)ENGINE_NO_OFFICER;
    TEST_CHECK(counts, api->trigger_event(e, FEATURE_EVENT_TREASON, force, ENGINE_NO_CITY, ENGINE_NO_OFFICER).refusal == SIM_REFUSAL_NONE &&
                       !api->pending_event(e).active,
               "an event whose conditions fail does nothing, even when forced (no corps, no suspect)");
    if (viceroy == ENGINE_NO_OFFICER) { engine_fixture_destroy(&setup.fixture); return; }
    sim_corps_order order = feature_corps_order(CORPS_CREATE, 0, viceroy, city);
    api->command_corps(e, &order);
    engine_officer_edit edit = feature_edit_of(api, viceroy);
    edit.loyalty = 50;
    api->set_officer(e, viceroy, &edit);

    u32 mark = api->ledger_count(e);
    engine_command_result fired = api->trigger_event(e, FEATURE_EVENT_TREASON, force, ENGINE_NO_CITY, ENGINE_NO_OFFICER);
    engine_pending_event pending = api->pending_event(e);
    TEST_CHECK(counts, fired.refusal == SIM_REFUSAL_NONE && pending.active && pending.event == FEATURE_EVENT_TREASON && pending.force == force &&
                       pending.officer_b == viceroy && pending.choices == 2 && feature_count_events(api, SIM_EVENT_EVENT_FIRED, mark) == 1,
               "謀反的嫌疑 fires: the human ruler must choose about the 都督");
    sim_corps_order other = feature_corps_order(CORPS_DISSOLVE, 1, ENGINE_NO_OFFICER, ENGINE_NO_CITY);
    TEST_CHECK(counts, api->command_corps(e, &other).refusal == SIM_REFUSAL_BUSY && api->command_event_choice(e, 2).refusal == SIM_REFUSAL_NO_EVENT,
               "refused: other commands wait for the choice, and the choice must exist");
    u32 factions = api->faction_count(e);
    mark = api->ledger_count(e);
    engine_command_result chosen = api->command_event_choice(e, 1);
    engine_officer rebel = {};
    feature_officer_by_id(api, viceroy, &rebel);
    TEST_CHECK(counts, chosen.refusal == SIM_REFUSAL_NONE && !api->pending_event(e).active && rebel.force_id != force && rebel.status == OFFICER_STATUS_RULER &&
                       api->city_at(e, city).owner_force_id == rebel.force_id && feature_count_events(api, SIM_EVENT_FACTION_FOUNDED, mark) == 1 &&
                       feature_count_events(api, SIM_EVENT_EVENT_CHOSEN, mark) == 1 && api->faction_count(e) >= factions,
               "the choice 'it is treason' makes the 都督 found a faction with the cities of his corps");
    engine_fixture_destroy(&setup.fixture);

    /* The other choice: the informer loses loyalty. */
    feature_rivals_setup_init(&setup);
    api = setup.api;
    e = api->engine;
    force = setup.force;
    city = feature_corps_city(api, force);
    viceroy = feature_pick_officer(api, city, 0, true);
    order = feature_corps_order(CORPS_CREATE, 0, viceroy, city);
    api->command_corps(e, &order);
    edit = feature_edit_of(api, viceroy);
    edit.loyalty = 50;
    api->set_officer(e, viceroy, &edit);
    api->trigger_event(e, FEATURE_EVENT_TREASON, force, ENGINE_NO_CITY, ENGINE_NO_OFFICER);
    pending = api->pending_event(e);
    if (pending.active && pending.officer_a != ENGINE_NO_OFFICER)
    {
        engine_officer_edit informer = feature_edit_of(api, pending.officer_a);
        informer.loyalty = 90;
        api->set_officer(e, pending.officer_a, &informer);
        api->command_event_choice(e, 0);
        TEST_CHECK(counts, api->officer_detail(e, pending.officer_a).loyalty == 90 - FEATURE_TREASON_INFORMER_LOSS &&
                           api->corps_at(e, force, 1).alive && api->city_at(e, city).owner_force_id == force,
                   "the choice 'it is a slander' keeps the corps; the informer loses 20 loyalty (assumed)");
    }
    else TEST_CHECK(counts, pending.active, "the second treason event waits for a choice");
    engine_fixture_destroy(&setup.fixture);
}

/* ---- F-032: the ports and passes ---------------------------------------------------- */

internal void
feature_base_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_march_setup(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 force = setup.force;

    engine_base first = api->base_at(e, 0), port = api->base_at(e, 10);
    TEST_CHECK(counts, api->base_count(e) == 45 && api->content_status(e).base_count == 45 && first.id == 42 && first.type == BASE_PASS &&
                       port.id == 52 && port.type == BASE_PORT && first.durability == first.durability_max && first.durability > 0,
               "bases.tsv has the 10 passes (42 .. 51) and the 35 ports (52 .. 86) with full durability");
    b32 placed = true, empty_free = true;
    for (u32 index = 0; index < api->base_count(e); ++index)
    {
        engine_base base = api->base_at(e, index);
        placed &= api->base_at_hex(e, base.q, base.r) == index;
        for (u32 city = 0; city < api->city_count(e); ++city)
        {
            engine_city_slots site = api->city_slots(e, (u8)city);
            placed &= !(site.q == base.q && site.r == base.r);
        }
        if (base.force == ENGINE_NO_FACTION) empty_free &= base.troops == 0;
    }
    TEST_CHECK(counts, placed && empty_free, "each base has its own hex, not on a city; a base with no owner has no soldiers");
    engine_base hukou = api->base_at(e, 72 - CONTENT_FIRST_BASE);
    TEST_CHECK(counts, hukou.troops == 6266 && hukou.force != ENGINE_NO_FACTION,
               "the owner and the soldiers of a base come from its record in the save (皖口港 6266; Q-183)");

    /* The gold of the bases goes into the monthly income of their city. */
    u32 index = 0;
    while (index < api->base_count(e) &&
           (api->base_at(e, index).force == ENGINE_NO_FACTION ||
            api->base_at(e, index).force != api->city_at(e, api->base_at(e, index).owner_city).owner_force_id)) ++index;
    engine_base base = api->base_at(e, index);
    u8 city_owner = api->city_at(e, base.owner_city).owner_force_id;
    u32 same = 0;
    for (u32 other = 0; other < api->base_count(e); ++other)
    {
        engine_base row = api->base_at(e, other);
        same += row.owner_city == base.owner_city && row.force == city_owner;
    }
    TEST_CHECK(counts, index < api->base_count(e) && api->city_income_forecast(e, base.owner_city).amounts.gold_bases == same * FEATURE_BASE_GOLD,
               "a city gets 100 gold each month for each of its bases with the same owner (assumed)");
    api->set_base(e, index, (u8)(city_owner == 1 ? 2 : 1), 1000, base.durability_max);
    TEST_CHECK(counts, api->city_income_forecast(e, base.owner_city).amounts.gold_bases == (same - 1) * FEATURE_BASE_GOLD,
               "a base of another faction gives its city no gold");

    /* A base blocks the march, and falls to an attack. */
    u8 enemy = feature_other_faction(api, force, 0);
    u32 unit_id = feature_sortie_unit(api, setup.city, 1);
    b32 beside = false;
    for (u32 target = 0; target < api->base_count(e) && !beside; ++target)
    {
        engine_base row = api->base_at(e, target);
        for (u32 d = 0; d < HEX_DIRECTION_COUNT && !beside; ++d)
        {
            hex_coord hex = hex_neighbor(hex_make(row.q, row.r), d);
            if (api->unit_at_hex(e, hex.q, hex.r) == ENGINE_NO_UNIT && api->set_unit_place(e, unit_id, force, hex.q, hex.r)) { beside = true; index = target; }
        }
    }
    base = api->base_at(e, index);
    TEST_CHECK(counts, unit_id != ENGINE_NO_UNIT && beside && api->command_move(e, unit_id, base.q, base.r).refusal != SIM_REFUSAL_NONE,
               "refused: a unit cannot enter the hex of a base");
    api->set_base(e, index, enemy, 100, 1);
    u32 mark = api->ledger_count(e);
    engine_command_result attack = api->command_attack(e, unit_id, base.q, base.r);
    sim_event taken = {};
    TEST_CHECK(counts, attack.refusal == SIM_REFUSAL_NONE && api->base_at(e, index).force == force &&
                       feature_find_event(api, SIM_EVENT_BASE_TAKEN, mark, &taken) && taken.faction == force && taken.detail == base.id,
               "an attack that takes the last durability of an enemy base captures it");
    engine_fixture_destroy(&setup.fixture);
}

internal void
feature_rivals_tests(test_counts *counts)
{
    feature_ai_plan_tests(counts);
    feature_ai_turn_tests(counts);
    feature_corps_tests(counts);
    feature_council_tests(counts);
    feature_emperor_tests(counts);
    feature_event_tests(counts);
    feature_base_tests(counts);
}
