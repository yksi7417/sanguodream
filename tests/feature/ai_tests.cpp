/* Feature tests of M15, an opponent worth playing (knowledge/ai.md,
   ROADMAP.md M15), through engine_api with the real engine and the sample
   save on the placeholder map: the record of the last AI turn
   (ai_explain), a free city that the AI takes, a city that the AI defends,
   and a unit that goes home before its grain ends. The goals, the scores
   and the rules ai.* are assumed (Q-185). The spec has no year log of the
   original, so the statistics test of the roadmap waits for it. */

#define FEATURE_AI_FREE_CITY_TURNS 6
#define FEATURE_AI_RESUPPLY_TURNS  3

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
        engine_ai_option *take = feature_ai_key(&explain, "take_free");
        took_out |= take && take->kind == SIM_COMMAND_SORTIE && take->city_id == home;
        turns++;
    }
    TEST_CHECK(counts, took_out, "given a free city within 12 hexes, the AI sends a unit out of its city (row take_free, priority 65)");
    u8 owner = api->city_at(e, target).owner_force_id;
    sim_event captured = {};
    TEST_CHECK(counts, owner != ENGINE_NO_FACTION && owner != human && feature_find_city_event(api, target, SIM_EVENT_BASE_CAPTURED, mark, &captured),
               "the AI takes the free city within 6 turns (assumed goal: a free city scores 50 above a city of an enemy)");
    TEST_CHECK(counts, owner != force || aimed, "the record of the taker shows the chosen goal 'take a free city' with that city");
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
    u32 one_turn = unit.soldiers / FEATURE_GRAIN_SOLDIERS;
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

internal void
feature_ai_tests(test_counts *counts)
{
    feature_ai_explain_tests(counts);
    feature_ai_free_city_tests(counts);
    feature_ai_defend_tests(counts);
    feature_ai_grain_tests(counts);
}
