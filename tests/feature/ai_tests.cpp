/* Feature tests of M15, an opponent worth playing (knowledge/ai.md,
   ROADMAP.md M15), through engine_api with the real engine and the sample
   save on the placeholder map: the record of the last AI turn
   (ai_explain), a free city that the AI takes, a city that the AI defends,
   and a unit that goes home before its grain ends. The goals, the scores
   and the rules ai.* are assumed (Q-185). The spec has no year log of the
   original, so the statistics test of the roadmap waits for it. */

#define FEATURE_AI_FREE_CITY_TURNS 6
#define FEATURE_AI_RESUPPLY_TURNS  3
#define FEATURE_AI_MONTHS_LATER    6   /* past ai.attack_after_months */
#define FEATURE_AI_DEAD_END_NEAR   6
#define FEATURE_AI_DEAD_END_FAR    20
#define FEATURE_AI_MARCH_TURNS     8
#define FEATURE_AI_WAR_SOLDIERS    40000    /* above the rows sortie and reinforce (15000) and ai.sortie_reserve */
#define FEATURE_AI_WAR_GRAIN       400000   /* feeds 40000 soldiers for 400 turns */
#define FEATURE_AI_HUNGRY_SOLDIERS 20000
#define FEATURE_AI_HUNGRY_GRAIN    1500     /* 3 turns for 20000 soldiers at grain.soldiers 40 */
#define FEATURE_AI_TARGET_HEXES    30       /* ai.target_hexes of the placeholder pack */
#define FEATURE_AI_WAR_DAY         11
#define FEATURE_AI_UNIT_LIMIT 1024

/* The hex of a city: the centre of its flower. */
internal hex_coord
feature_city_hex(engine_api *api, u8 city)
{
    engine_city_slots site = api->city_slots(api->engine, city);
    return hex_make(site.q, site.r);
}

/* An AI faction other than the human one that owns a city with a free
   officer and soldiers; *city_out is that city. */
internal u8
feature_ai_faction(engine_api *api, u8 human, u8 *city_out)
{
    u16 ids[8];
    for (u32 city = 0; city < api->city_count(api->engine); ++city)
    {
        engine_city info = api->city_at(api->engine, city);
        if (info.owner_force_id == ENGINE_NO_FACTION || info.owner_force_id == human) continue;
        if (api->force_ruler(api->engine, info.owner_force_id) == ENGINE_NO_OFFICER) continue;
        if (!feature_free_officers(api, (u8)city, ids, ArrayCount(ids))) continue;
        *city_out = (u8)city;
        return info.owner_force_id;
    }
    return ENGINE_NO_FACTION;
}

/* A city with soldiers, gold, grain and pikes for a sortie, and the AI
   faction at war with the human faction. */
internal void
feature_ai_arm_city(engine_api *api, u8 city, u8 force, u8 human)
{
    void *e = api->engine;
    api->set_city_resource(e, city, SIM_RESOURCE_SOLDIERS, 0, 30000);
    api->set_city_resource(e, city, SIM_RESOURCE_GRAIN, 0, 100000);
    api->set_city_resource(e, city, SIM_RESOURCE_WEAPONS, 0, 30000);   /* pikes */
    api->set_city_gold(e, city, 20000);
    api->set_actions(e, force, 255);
    api->set_diplomacy(e, force, human, 0, RELATION_NONE, 0);
}

/* A free hex at a distance from the hex, or false. */
internal b32
feature_free_hex_at(engine_api *api, hex_coord centre, s32 distance, u32 unit_id, u8 force, hex_coord *found)
{
    for (s32 dq = -distance; dq <= distance; ++dq)
    {
        for (s32 dr = -distance; dr <= distance; ++dr)
        {
            hex_coord hex = hex_make(centre.q + dq, centre.r + dr);
            if (hex_distance(hex, centre) != distance) continue;
            if (api->set_unit_place(api->engine, unit_id, force, hex.q, hex.r)) { *found = hex; return true; }
        }
    }
    return false;
}

/* The first option of the record with the goal (and the unit, if one is
   given), or 0. */
internal engine_ai_option *
feature_ai_option(engine_ai_explain *explain, u8 goal, u32 unit_id, b32 chosen_only)
{
    for (u32 index = 0; index < explain->count; ++index)
    {
        engine_ai_option *option = explain->options + index;
        if (option->goal != goal || (chosen_only && !option->chosen)) continue;
        if (unit_id != ENGINE_NO_UNIT && option->unit != unit_id) continue;
        return option;
    }
    return 0;
}

/* The chosen city option of a row of ai_priorities.tsv, or 0. */
internal engine_ai_option *
feature_ai_key(engine_ai_explain *explain, const char *key)
{
    for (u32 index = 0; index < explain->count; ++index)
    {
        engine_ai_option *option = explain->options + index;
        if (option->goal == AI_GOAL_CITY_COMMAND && option->chosen && strcmp(option->key, key) == 0) return option;
    }
    return 0;
}

/* The deserted soldiers of one unit from "from" on. */
internal u32
feature_unit_desertions(engine_api *api, u32 unit_id, u32 from)
{
    u32 found = 0;
    for (u32 index = from; index < api->ledger_count(api->engine); ++index)
    {
        sim_event event = api->ledger_at(api->engine, index);
        if (event.kind == SIM_EVENT_UNIT_DESERTED && event.detail == unit_id) found++;
    }
    return found;
}

/* ---- ai_explain: the scored options of the last AI turn --------------------------- */

internal void
feature_ai_explain_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_rivals_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 human = setup.force, city = ENGINE_NO_CITY;
    u8 force = feature_ai_faction(api, human, &city);
    TEST_CHECK(counts, force != ENGINE_NO_FACTION, "the sample has an AI faction with a free officer in a city");
    if (force == ENGINE_NO_FACTION) { engine_fixture_destroy(&setup.fixture); return; }

    engine_ai_explain before = api->ai_explain(e, force);
    TEST_CHECK(counts, before.faction == force && before.turn == 0 && before.count == 0, "before its first AI turn a faction has no record");
    TEST_CHECK(counts, api->ai_explain(e, ENGINE_NO_FACTION).count == 0, "no faction, no record");

    feature_turn_on(api, true, false);
    api->set_city_resource(e, city, SIM_RESOURCE_PUBLIC_ORDER, 0, 0);
    api->set_actions(e, force, 255);
    u32 mark = api->ledger_count(e);
    feature_next_own_turn(api, human);
    engine_ai_explain explain = api->ai_explain(e, force);
    u32 chosen = 0, city_commands = 0;
    b32 sorted = true, keyed = true;
    for (u32 index = 0; index < explain.count; ++index)
    {
        engine_ai_option *option = explain.options + index;
        chosen += option->chosen != 0;
        if (option->goal != AI_GOAL_CITY_COMMAND) continue;
        city_commands += option->chosen != 0;
        keyed &= option->key[0] != 0 && option->unit == ENGINE_NO_UNIT;
        if (index && explain.options[index - 1].goal == AI_GOAL_CITY_COMMAND && !explain.options[index - 1].chosen && !option->chosen &&
            option->score > explain.options[index - 1].score) sorted = false;
    }
    sim_event planned = {};
    b32 found = false;
    for (u32 index = mark; index < api->ledger_count(e); ++index)
    {
        sim_event event = api->ledger_at(e, index);
        if (event.kind == SIM_EVENT_AI_PLANNED && event.faction == force) { planned = event; found = true; }
    }
    TEST_CHECK(counts, explain.turn > 0 && explain.count > 0 && chosen > 0 && keyed,
               "ai_explain: after its turn the faction has a record of scored options; each city option names its row of ai_priorities.tsv");
    TEST_CHECK(counts, found && city_commands > 0 && city_commands <= (u32)planned.delta,
               "the chosen city options are commands that the AI gave in that turn (the ledger counts them in ai_planned)");
    TEST_CHECK(counts, sorted, "the options that the AI did not choose follow their score, the highest first");
    engine_ai_option *patrol = 0;
    for (u32 index = 0; index < explain.count && !patrol; ++index)
    {
        if (explain.options[index].chosen && explain.options[index].city_id == city && explain.options[index].kind == SIM_COMMAND_PATROL) patrol = explain.options + index;
    }
    TEST_CHECK(counts, patrol && strcmp(patrol->key, "patrol") == 0 && patrol->officer != ENGINE_NO_OFFICER,
               "given public order 0 in a city, the record shows the chosen 巡察 of that city with its officer");
    TEST_CHECK(counts, api->ai_explain(e, human).count == 0, "the human faction has no AI turn and no record");
    engine_fixture_destroy(&setup.fixture);
}

/* ---- the AI takes a free city ------------------------------------------------------- */

internal void
feature_ai_free_city_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_rivals_setup_init(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 human = setup.force, home = ENGINE_NO_CITY;
    /* The test puts the date back to the first months of the scenario (G2-001: the sample loads at
       197/10/1) and takes a fresh turn, so that the acted flags of the save (G2-004) do not pick the city. */
    api->set_date(e, 194, 6, 1);
    feature_next_own_turn(api, human);
    u8 force = feature_ai_faction(api, human, &home);
    if (force == ENGINE_NO_FACTION) { TEST_CHECK(counts, false, "the sample has an AI faction"); engine_fixture_destroy(&setup.fixture); return; }

    /* The nearest city of another owner becomes free and empty. */
    hex_coord from = feature_city_hex(api, home);
    u8 target = ENGINE_NO_CITY;
    s32 best = 0x7FFFFFFF;
    for (u32 city = 0; city < api->city_count(e); ++city)
    {
        engine_city info = api->city_at(e, city);
        if (info.owner_force_id == force || info.owner_force_id == human) continue;
        s32 distance = hex_distance(feature_city_hex(api, (u8)city), from);
        if (distance < best) { best = distance; target = (u8)city; }
    }
    TEST_CHECK(counts, target != ENGINE_NO_CITY, "a city of a third owner stands near the city of the AI faction");
    if (target == ENGINE_NO_CITY) { engine_fixture_destroy(&setup.fixture); return; }
    api->set_city_owner(e, target, ENGINE_NO_FACTION);
    api->set_city_resource(e, target, SIM_RESOURCE_SOLDIERS, 0, 0);
    feature_ai_arm_city(api, home, force, human);
    feature_turn_on(api, true, false);

    /* In the first months of the scenario the AI does not march on a city yet. */
    feature_next_own_turn(api, human);
    engine_ai_explain early = api->ai_explain(e, force);
    TEST_CHECK(counts, early.turn > 0 && !feature_ai_key(&early, "take_free") && !feature_ai_key(&early, "sortie") &&
                       api->city_at(e, target).owner_force_id == ENGINE_NO_FACTION,
               "in the first 5 months after the start of the scenario the AI sends no unit against a city (assumed rule ai.attack_after_months)");
    engine_date today = api->date(e);
    u32 later = (u32)today.month - 1 + FEATURE_AI_MONTHS_LATER;
    api->set_date(e, (u16)(today.year + later / 12), (u8)(later % 12 + 1), 1);

    u32 mark = api->ledger_count(e);
    u32 turns = 0;
    b32 aimed = false, took_out = false;
    while (turns < FEATURE_AI_FREE_CITY_TURNS && api->city_at(e, target).owner_force_id == ENGINE_NO_FACTION)
    {
        api->set_actions(e, force, 255);
        feature_next_own_turn(api, human);
        engine_ai_explain explain = api->ai_explain(e, force);
        engine_ai_option *goal = feature_ai_option(&explain, AI_GOAL_TAKE_FREE_CITY, ENGINE_NO_UNIT, true);
        aimed |= goal && goal->city_id == target;
        /* Any city of the faction can send the unit. A city with an own unit next to it sends none
           (the play test of 2026-11-06: one unit for each free city). */
        engine_ai_option *take = feature_ai_key(&explain, "take_free");
        took_out |= take && take->kind == SIM_COMMAND_SORTIE && api->city_at(e, take->city_id).owner_force_id == force;
        turns++;
    }
    TEST_CHECK(counts, took_out, "given a free city within 30 hexes, the AI sends a unit out of its city (row take_free, priority 65)");
    u8 owner = api->city_at(e, target).owner_force_id;
    sim_event captured = {};
    TEST_CHECK(counts, owner != ENGINE_NO_FACTION && owner != human && feature_find_city_event(api, target, SIM_EVENT_BASE_CAPTURED, mark, &captured),
               "the AI takes the free city within 6 turns (assumed goal: a free city scores 50 above a city of an enemy)");
    TEST_CHECK(counts, owner != force || aimed, "the record of the taker shows the chosen goal 'take a free city' with that city");
    engine_fixture_destroy(&setup.fixture);
}

/* ---- the AI marches around an obstacle ---------------------------------------------- */

/* True if no hex in the reach of the unit is nearer to the goal than the
   hex of the unit: a march to the nearest hex stops there. */
internal b32
feature_ai_dead_end(engine_api *api, u32 unit_id, hex_coord goal)
{
    engine_reach_view reach = api->reachable(api->engine, unit_id);
    if (reach.count < 2) return false;
    s32 own = hex_distance(reach.cells[0], goal);
    for (u32 index = 1; index < reach.count; ++index)
    {
        if (hex_distance(reach.cells[index], goal) < own) return false;
    }
    return true;
}

/* True if the unit has a path to a hex next to the flower of the city at
   the goal. No unit enters the 7 hexes of a city of another owner. */
internal b32
feature_ai_path_beside(engine_api *api, u32 unit_id, hex_coord goal)
{
    for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d)
    {
        hex_coord beside = hex_neighbor(hex_neighbor(goal, d), d);
        if (api->path_to(api->engine, unit_id, beside.q, beside.r).found) return true;
    }
    return false;
}

/* A free hex 6 to 20 hexes from the goal where the unit stands at a dead
   end and has a path to the city. The unit stays on that hex. */
internal b32
feature_ai_dead_end_hex(engine_api *api, u32 unit_id, u8 force, hex_coord goal, hex_coord *found)
{
    for (s32 distance = FEATURE_AI_DEAD_END_NEAR; distance <= FEATURE_AI_DEAD_END_FAR; ++distance)
    {
        for (s32 dq = -distance; dq <= distance; ++dq)
        {
            for (s32 dr = -distance; dr <= distance; ++dr)
            {
                hex_coord hex = hex_make(goal.q + dq, goal.r + dr);
                if (hex_distance(hex, goal) != distance) continue;
                if (!api->set_unit_place(api->engine, unit_id, force, hex.q, hex.r)) continue;
                if (!feature_ai_dead_end(api, unit_id, goal)) continue;
                if (!feature_ai_path_beside(api, unit_id, goal)) continue;
                *found = hex;
                return true;
            }
        }
    }
    return false;
}

internal void
feature_ai_march_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_map_setup(&setup);   /* the real map: the placeholder map has no obstacle */
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 human = setup.force, home = ENGINE_NO_CITY;
    u8 force = feature_ai_faction(api, human, &home);
    u32 unit_id = feature_sortie_unit(api, setup.city, 1);
    if (force == ENGINE_NO_FACTION || unit_id == ENGINE_NO_UNIT) { TEST_CHECK(counts, false, "an AI faction and a unit"); engine_fixture_destroy(&setup.fixture); return; }

    /* The nearest city of a third owner becomes free and empty. */
    hex_coord from = feature_city_hex(api, home);
    u8 target = ENGINE_NO_CITY;
    s32 best = 0x7FFFFFFF;
    for (u32 city = 0; city < api->city_count(e); ++city)
    {
        engine_city info = api->city_at(e, city);
        if (info.owner_force_id == force || info.owner_force_id == human) continue;
        s32 distance = hex_distance(feature_city_hex(api, (u8)city), from);
        if (distance < best) { best = distance; target = (u8)city; }
    }
    if (target == ENGINE_NO_CITY) { TEST_CHECK(counts, false, "a city of a third owner"); engine_fixture_destroy(&setup.fixture); return; }
    api->set_city_owner(e, target, ENGINE_NO_FACTION);
    api->set_city_resource(e, target, SIM_RESOURCE_SOLDIERS, 0, 0);
    api->set_diplomacy(e, force, human, 0, RELATION_NONE, 0);

    /* The unit goes to the AI faction, on a hex where each reachable hex is as far from the city or farther. */
    hex_coord goal = feature_city_hex(api, target), start = {};
    b32 placed = feature_ai_dead_end_hex(api, unit_id, force, goal, &start);
    TEST_CHECK(counts, placed, "a unit of the AI stands 6 to 20 hexes from a free city; no hex in its reach is nearer; a path goes around");
    if (!placed) { engine_fixture_destroy(&setup.fixture); return; }

    feature_turn_on(api, true, false);
    api->set_actions(e, force, 255);
    feature_next_own_turn(api, human);
    engine_ai_explain explain = api->ai_explain(e, force);
    engine_ai_option *aim = feature_ai_option(&explain, AI_GOAL_TAKE_FREE_CITY, unit_id, true);
    engine_unit unit = api->unit_at(e, unit_id);
    TEST_CHECK(counts, aim && aim->city_id == target, "the chosen goal of the unit is the free city");
    TEST_CHECK(counts, unit.id != ENGINE_NO_UNIT && (unit.q != start.q || unit.r != start.r),
               "the unit leaves the dead end: it follows the lowest-cost path to the city (assumed path rule Q-227)");

    u32 turns = 1;
    while (turns < FEATURE_AI_MARCH_TURNS && api->city_at(e, target).owner_force_id == ENGINE_NO_FACTION)
    {
        api->set_actions(e, force, 255);
        feature_next_own_turn(api, human);
        turns++;
    }
    u8 owner = api->city_at(e, target).owner_force_id;
    TEST_CHECK(counts, owner != ENGINE_NO_FACTION && owner != human, "an AI faction takes the free city within 8 turns");
    engine_fixture_destroy(&setup.fixture);
}

/* ---- the AI defends a city under attack --------------------------------------------- */

internal void
feature_ai_defend_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_march_setup(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 human = setup.force, city = ENGINE_NO_CITY;
    u8 force = feature_ai_faction(api, human, &city);
    u32 enemy = feature_sortie_unit(api, setup.city, 1);
    if (force == ENGINE_NO_FACTION || enemy == ENGINE_NO_UNIT) { TEST_CHECK(counts, false, "an AI city and a unit of the player"); engine_fixture_destroy(&setup.fixture); return; }

    feature_ai_arm_city(api, city, force, human);
    hex_coord placed = {};
    b32 beside = feature_free_hex_at(api, feature_city_hex(api, city), 2, enemy, human, &placed);
    TEST_CHECK(counts, beside, "a unit of the player stands 2 hexes from a city of the AI");

    feature_turn_on(api, true, false);
    u32 mark = api->ledger_count(e);
    feature_next_own_turn(api, human);
    sim_event sortied = {};
    b32 out = feature_find_city_event(api, city, SIM_EVENT_UNIT_SORTIED, mark, &sortied);
    engine_ai_explain explain = api->ai_explain(e, force);
    engine_ai_option *defend = feature_ai_option(&explain, AI_GOAL_DEFEND, ENGINE_NO_UNIT, true);
    u32 hurt = 0;
    for (u32 index = mark; index < api->ledger_count(e); ++index)
    {
        sim_event event = api->ledger_at(e, index);
        hurt += event.detail == enemy && (event.kind == SIM_EVENT_UNIT_DAMAGED || event.kind == SIM_EVENT_UNIT_DESTROYED);
    }
    engine_ai_option *first = explain.count ? explain.options : 0;
    TEST_CHECK(counts, out && first && first->chosen && strcmp(first->key, "defend") == 0 && first->kind == SIM_COMMAND_SORTIE && first->city_id == city,
               "the first command of the AI is a sortie from the city under attack (row defend, priority 70 + urgency 20)");
    TEST_CHECK(counts, defend && defend->city_id == city && defend->q == placed.q && defend->r == placed.r,
               "the record shows the chosen goal 'defend' of a unit, with the city and the hex of the enemy");
    TEST_CHECK(counts, hurt, "the defender attacks the enemy unit in the same turn: the ledger has the damage of the enemy");
    engine_fixture_destroy(&setup.fixture);
}

/* ---- the AI does not starve its units ----------------------------------------------- */

internal void
feature_ai_grain_tests(test_counts *counts)
{
    feature_city_setup setup;
    feature_march_setup(&setup);
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 human = setup.force, city = ENGINE_NO_CITY;
    u8 force = feature_ai_faction(api, human, &city);
    u32 unit_id = feature_sortie_unit(api, setup.city, 1);
    if (force == ENGINE_NO_FACTION || unit_id == ENGINE_NO_UNIT) { TEST_CHECK(counts, false, "an AI city and a unit"); engine_fixture_destroy(&setup.fixture); return; }

    /* The unit goes to the AI faction, 2 hexes from its city, with grain for one turn. */
    hex_coord placed = {};
    b32 moved = feature_free_hex_at(api, feature_city_hex(api, city), 2, unit_id, force, &placed);
    engine_unit unit = api->unit_at(e, unit_id);
    u32 one_turn = unit.soldiers / 10;   /* unit.grain_soldiers: one grain for 10 soldiers (G2-049) */
    api->set_unit_grain(e, unit_id, one_turn);
    TEST_CHECK(counts, moved && api->unit_at(e, unit_id).grain == one_turn && api->unit_at(e, unit_id).grain_turns == 1,
               "set_unit_grain: the unit of the AI has grain for 1 turn");
    api->set_diplomacy(e, force, human, 0, RELATION_NONE, 0);

    feature_turn_on(api, true, false);
    u32 mark = api->ledger_count(e);
    feature_next_own_turn(api, human);
    engine_ai_explain explain = api->ai_explain(e, force);
    engine_ai_option *resupply = feature_ai_option(&explain, AI_GOAL_RESUPPLY, unit_id, true);
    TEST_CHECK(counts, resupply && resupply->city_id == city,
               "given grain for fewer turns than the march home plus 1, the goal of the unit is its nearest own city");
    for (u32 turn = 1; turn < FEATURE_AI_RESUPPLY_TURNS; ++turn) feature_next_own_turn(api, human);
    sim_event entered = {};
    TEST_CHECK(counts, feature_find_city_event(api, city, SIM_EVENT_UNIT_ENTERED_CITY, mark, &entered) && entered.detail == unit_id,
               "the unit enters its city before the grain ends");
    TEST_CHECK(counts, feature_unit_desertions(api, unit_id, mark) == 0, "no soldier of the unit deserts for want of grain");
    engine_fixture_destroy(&setup.fixture);
}

/* ---- the AI war of the play test of 2026-11-06 ---------------------------------------- */

/* The sample past ai.attack_after_months, with the AI on and a fresh turn
   of the human faction; *force and *home are an AI faction and its city.
   The day 11 keeps the next two turns away from the grain income of day 1. */
internal b32
feature_ai_war_setup(feature_city_setup *setup, u8 *force, u8 *home, b32 real_map)
{
    feature_rivals_setup_on(setup, real_map);
    engine_api *api = setup->api;
    void *e = api->engine;
    engine_date today = api->date(e);
    u32 later = (u32)today.month - 1 + FEATURE_AI_MONTHS_LATER;
    api->set_date(e, (u16)(today.year + later / 12), (u8)(later % 12 + 1), FEATURE_AI_WAR_DAY);
    feature_turn_on(api, true, false);
    feature_next_own_turn(api, setup->force);
    *force = feature_ai_faction(api, setup->force, home);
    return *force != ENGINE_NO_FACTION;
}

/* A city of the faction with soldiers above the rows sortie and reinforce,
   grain, gold and the weapons in the four stocks. */
internal void
feature_ai_war_city(engine_api *api, u8 city, u8 force, const u32 *weapons)
{
    void *e = api->engine;
    api->set_city_resource(e, city, SIM_RESOURCE_SOLDIERS, 0, FEATURE_AI_WAR_SOLDIERS);
    api->set_city_resource(e, city, SIM_RESOURCE_GRAIN, 0, FEATURE_AI_WAR_GRAIN);
    api->set_city_resource(e, city, SIM_RESOURCE_PUBLIC_ORDER, 0, 100);
    api->set_city_resource(e, city, SIM_RESOURCE_MORALE, 0, 100);
    for (u32 stock = 0; stock < 4; ++stock) api->set_city_resource(e, city, SIM_RESOURCE_WEAPONS, stock, weapons[stock]);
    api->set_city_gold(e, city, 20000);
    api->set_actions(e, force, 255);
}

/* The key of the type of the first unit of the faction from the city
   that is not in the list of the units before the turn, or 0. */
internal const char *
feature_ai_new_unit_type(engine_api *api, u8 force, u8 city, const b32 *alive_before, engine_unit_type *type)
{
    for (u32 unit = 0; unit < api->unit_count(api->engine); ++unit)
    {
        engine_unit info = api->unit_at(api->engine, unit);
        if (info.id == ENGINE_NO_UNIT || alive_before[unit] || info.force_id != force || info.home_city != city) continue;
        *type = api->unit_type_at(api->engine, info.unit_type);
        return type->key;
    }
    return 0;
}

internal void
feature_ai_sortie_type_tests(test_counts *counts)
{
    static const u32 halberds_only[4] = { 0, FEATURE_AI_WAR_SOLDIERS, 0, 0 };
    static const u32 no_weapons[4]    = { 0, 0, 0, 0 };
    const u32 *cases[2] = { halberds_only, no_weapons };
    const char *expected[2] = { "halberd", "sword" };
    const char *labels[2] = {
        "given halberds and no pikes, the AI sends a unit of halberds (before the play test of 2026-11-06 it sent no unit)",
        "given no weapons at all, the AI sends a unit of swords (assumed rule ai.sortie_min_soldiers)" };
    for (u32 index = 0; index < 2; ++index)
    {
        feature_city_setup setup;
        u8 force = ENGINE_NO_FACTION, home = ENGINE_NO_CITY;
        if (!feature_ai_war_setup(&setup, &force, &home, false)) { TEST_CHECK(counts, false, "the sample has an AI faction"); engine_fixture_destroy(&setup.fixture); return; }
        engine_api *api = setup.api;
        feature_ai_war_city(api, home, force, cases[index]);
        b32 alive_before[FEATURE_AI_UNIT_LIMIT] = {};
        for (u32 unit = 0; unit < api->unit_count(api->engine) && unit < FEATURE_AI_UNIT_LIMIT; ++unit)
        {
            alive_before[unit] = api->unit_at(api->engine, unit).id != ENGINE_NO_UNIT;
        }
        feature_next_own_turn(api, setup.force);
        engine_unit_type type = {};
        const char *key = feature_ai_new_unit_type(api, force, home, alive_before, &type);
        TEST_CHECK(counts, key && strcmp(key, expected[index]) == 0, labels[index]);
        engine_fixture_destroy(&setup.fixture);
    }
}

internal void
feature_ai_buy_grain_tests(test_counts *counts)
{
    feature_city_setup setup;
    u8 force = ENGINE_NO_FACTION, home = ENGINE_NO_CITY;
    if (!feature_ai_war_setup(&setup, &force, &home, false)) { TEST_CHECK(counts, false, "the sample has an AI faction"); engine_fixture_destroy(&setup.fixture); return; }
    engine_api *api = setup.api;
    void *e = api->engine;
    feature_calm_faction(api, force);
    api->set_city_resource(e, home, SIM_RESOURCE_SOLDIERS, 0, FEATURE_AI_HUNGRY_SOLDIERS);   /* below the half of the cap: 徵兵 is wanted */
    /* Grain for 3 turns: the soldiers eat at the start of the turn, and with no grain they leave before the AI acts. */
    api->set_city_resource(e, home, SIM_RESOURCE_GRAIN, 0, FEATURE_AI_HUNGRY_GRAIN);
    api->set_city_gold(e, home, 20000);
    api->set_actions(e, force, 255);
    feature_next_own_turn(api, setup.force);
    /* The record of the AI turn has the chosen options and the options of the first plan. */
    engine_ai_explain explain = api->ai_explain(e, force);
    b32 buys = false, conscripts = false;
    for (u32 index = 0; index < explain.count; ++index)
    {
        engine_ai_option *option = explain.options + index;
        if (option->goal != AI_GOAL_CITY_COMMAND || option->city_id != home) continue;
        buys       |= option->chosen && strcmp(option->key, "buy_grain") == 0 && option->kind == SIM_COMMAND_MERCHANT;
        conscripts |= option->kind == SIM_COMMAND_CONSCRIPT;
    }
    TEST_CHECK(counts, buys, "given a city with grain for 3 turns and spare gold, the AI buys grain from the merchant (row buy_grain)");
    TEST_CHECK(counts, !conscripts, "given a city with grain for 3 turns, the AI proposes no 徵兵 there: the new soldiers leave when the grain ends (rule ai.grain_turns)");
    TEST_CHECK(counts, api->city_at(e, home).food > FEATURE_AI_HUNGRY_GRAIN && api->city_at(e, home).gold < 20000,
               "after its turn the city has grain, and it paid gold for it");
    engine_fixture_destroy(&setup.fixture);
}

/* An inner city: a city A of the faction with no city of a faction at
   war and no city without an owner within ai.target_hexes, and another
   city B of the faction with one.
   The test makes the faction an ally of each faction but one, so that the
   enemy cities are those of one faction only. */
/* The hexes from the city to the nearest city of the enemy faction or
   without an owner. A city without an owner is a target too. */
internal s32
feature_ai_target_distance(engine_api *api, u8 city, u8 enemy)
{
    s32 nearest = 0x7FFFFFFF;
    for (u32 target = 0; target < api->city_count(api->engine); ++target)
    {
        u8 owner = api->city_at(api->engine, target).owner_force_id;
        if (owner != enemy && owner != ENGINE_NO_FACTION) continue;
        nearest = Minimum(nearest, hex_distance(feature_city_hex(api, city), feature_city_hex(api, (u8)target)));
    }
    return nearest;
}

internal b32
feature_ai_inner_city(engine_api *api, u8 force, u8 *inner, u8 *enemy_force)
{
    void *e = api->engine;
    for (u32 other = 0; other < SIM_FORCE_CAPACITY; ++other)
    {
        if (other == force || api->force_ruler(e, (u8)other) == ENGINE_NO_OFFICER) continue;
        b32 has_inner = false, has_front = false;
        u8 candidate = ENGINE_NO_CITY;
        for (u32 city = 0; city < api->city_count(e); ++city)
        {
            if (api->city_at(e, city).owner_force_id != force) continue;
            s32 nearest = feature_ai_target_distance(api, (u8)city, (u8)other);
            if (nearest > FEATURE_AI_TARGET_HEXES && candidate == ENGINE_NO_CITY) { candidate = (u8)city; has_inner = true; }
            if (nearest <= FEATURE_AI_TARGET_HEXES) has_front = true;
        }
        if (!has_inner || !has_front) continue;
        *inner = candidate;
        *enemy_force = (u8)other;
        return true;
    }
    return false;
}

internal void
feature_ai_reinforce_tests(test_counts *counts)
{
    feature_city_setup setup;
    u8 force = ENGINE_NO_FACTION, home = ENGINE_NO_CITY;
    if (!feature_ai_war_setup(&setup, &force, &home, true)) { TEST_CHECK(counts, false, "the sample has an AI faction"); engine_fixture_destroy(&setup.fixture); return; }
    engine_api *api = setup.api;
    void *e = api->engine;
    u8 inner = ENGINE_NO_CITY, enemy = ENGINE_NO_FACTION;
    /* The first AI faction with a city far from one enemy and a city near it. */
    b32 found = false;
    for (u32 other = 0; other < SIM_FORCE_CAPACITY && !found; ++other)
    {
        if (other == setup.force || api->force_ruler(e, (u8)other) == ENGINE_NO_OFFICER) continue;
        force = (u8)other;
        found = feature_ai_inner_city(api, force, &inner, &enemy);
    }
    TEST_CHECK(counts, found, "the sample has an AI faction with a city far from the cities of one other faction, and a city near them");
    if (!found) { engine_fixture_destroy(&setup.fixture); return; }
    for (u32 other = 0; other < SIM_FORCE_CAPACITY; ++other)
    {
        if (other != force) api->set_diplomacy(e, force, (u8)other, 100, (u8)(other == enemy ? RELATION_NONE : RELATION_ALLIANCE), 0);
    }
    for (u32 city = 0; city < api->city_count(e); ++city)
    {
        if (api->city_at(e, city).owner_force_id != force) continue;
        api->set_city_resource(e, (u8)city, SIM_RESOURCE_PUBLIC_ORDER, 0, 100);
        api->set_city_resource(e, (u8)city, SIM_RESOURCE_MORALE, 0, 100);
        api->set_city_resource(e, (u8)city, SIM_RESOURCE_GRAIN, 0, FEATURE_AI_WAR_GRAIN);
    }
    static const u32 pikes[4] = { FEATURE_AI_WAR_SOLDIERS, 0, 0, 0 };
    feature_ai_war_city(api, inner, force, pikes);
    feature_next_own_turn(api, setup.force);
    engine_ai_explain explain = api->ai_explain(e, force);
    /* The armed city, or another inner city of the faction with an army of the save, can send it. */
    engine_ai_option *reinforce = feature_ai_key(&explain, "reinforce");
    u8 from = reinforce ? reinforce->city_id : ENGINE_NO_CITY;
    TEST_CHECK(counts, reinforce && reinforce->kind == SIM_COMMAND_TRANSPORT && api->city_at(e, from).owner_force_id == force &&
                       feature_ai_target_distance(api, from, enemy) > FEATURE_AI_TARGET_HEXES,
               "given a city with a large army and no enemy city within 30 hexes, the AI sends its soldiers out of it (row reinforce)");
    b32 carried = false;
    for (u32 unit = 0; unit < api->unit_count(e) && from != ENGINE_NO_CITY; ++unit)
    {
        engine_unit info = api->unit_at(e, unit);
        if (info.id == ENGINE_NO_UNIT || info.force_id != force || info.home_city != from || info.destination == ENGINE_NO_CITY) continue;
        carried = info.soldiers > 0 && api->city_at(e, info.destination).owner_force_id == force &&
                  feature_ai_target_distance(api, info.destination, enemy) <= FEATURE_AI_TARGET_HEXES;
    }
    TEST_CHECK(counts, carried, "the transport carries soldiers to a city of the faction on a front: an enemy city is within 30 hexes of it");
    engine_fixture_destroy(&setup.fixture);
}

internal void
feature_ai_tests(test_counts *counts)
{
    feature_ai_explain_tests(counts);
    feature_ai_free_city_tests(counts);
    feature_ai_march_tests(counts);
    feature_ai_defend_tests(counts);
    feature_ai_grain_tests(counts);
    feature_ai_sortie_type_tests(counts);
    feature_ai_buy_grain_tests(counts);
    feature_ai_reinforce_tests(counts);
}
