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
        engine_ai_option *take = feature_ai_key(&explain, "take_free");
        /* The play run of 2026-11-07: the AI now builds, and the officer of home can be busy. Another city of the faction can send the unit. */
        took_out |= take && take->kind == SIM_COMMAND_SORTIE && take->city_id < SAVE_CITY_COUNT && api->city_at(e, take->city_id).owner_force_id == force;
        turns++;
    }
    TEST_CHECK(counts, took_out, "given a free city within 30 hexes, the AI sends a unit out of one of its cities (row take_free, priority 65)");
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

/* ---- the play of the AI alone on a pack (the play run of 2026-11-07) ------------- */

#define FEATURE_AI_PLAY_YEARS        3
#define FEATURE_AI_PLAY_TROOP_FLOOR  90   /* the percent of the start troops that the AI keeps after the years, also with its wars. Farm 250: approximately 67 */
#define FEATURE_AI_PLAY_SORTIES      20
#define FEATURE_AI_PLAY_CAPTURES     6    /* the cities that change owner in the years, at the least (U-012) */
#define FEATURE_AI_PLAY_ALLIANCE_TURNS 6  /* an alliance of the AI lasts at least these turns (U-015) */
#define FEATURE_AI_PLAY_UNPAID_LIMIT 4    /* the city months without salaries in the years, at the most. Without the reserve: 7 */
#define FEATURE_AI_PLAY_RUIN_LIMIT   130  /* the facilities that the disasters destroy in the years, at the most. Patrol below 70: approximately 190 */   /* the units that the AI factions send out in the years, at the least */

/* The soldiers of all factions in their cities and in their units. */
internal u64
feature_all_troops(engine_api *api)
{
    u64 troops = 0;
    for (u32 index = 0; index < api->faction_count(api->engine); ++index) troops += api->faction_at(api->engine, index).troops;
    for (u32 index = 0; index < api->unit_count(api->engine); ++index) troops += api->unit_at(api->engine, index).soldiers;
    return troops;
}

/* The play run of 2026-11-07 let the AI play han_196 alone for 15 years. The
   AI built nothing and conscripted nothing, and the troops fell to almost 0.
   The faction planner had the flag 儲存金錢 (CORPS_FLAG_ALL), so it never
   proposed a development. A debate of an AI recruiter stayed open, and it
   refused the commands of each later faction (SIM_REFUSAL_BUSY). */
internal void
feature_ai_pack_play_tests(test_counts *counts)
{
    engine_fixture f;
    feature_world_map_init(&f, HEXDOM_DATA_ROOT "/han_196");
    engine_api *api = &f.api;
    void *e = api->engine;
    TEST_CHECK(counts, api->load_scenario(e, api->scenario_at(e, 0).file).status == ENGINE_LOAD_OK, "the scenario of 196 loads");
    engine_settings settings = api->settings(e);
    settings.ai        = true;
    settings.disasters = true;   /* the bandits destroyed the facilities of the AI in the play run */
    api->set_settings(e, settings);
    u8 holders[SIM_FORCE_CAPACITY] = {};
    for (u32 index = 0; index < api->faction_count(e); ++index)
    {
        engine_faction faction = api->faction_at(e, index);
        api->set_faction_human(e, faction.force_id, false);
        if (faction.city_count && faction.force_id < SIM_FORCE_CAPACITY) holders[faction.force_id] = 1;
    }
    u64 start_troops = feature_all_troops(api);
    u32 mark = api->ledger_count(e);
    u32 start_year = api->date(e).year;
    for (u32 guard = 0; guard < FEATURE_AI_PLAY_YEARS * 36 * 4 && api->date(e).year < start_year + FEATURE_AI_PLAY_YEARS; ++guard) api->end_turn(e);

    u32 developed[SIM_FORCE_CAPACITY] = {};
    u32 conscripted = 0, open_debates = 0, busy_turns = 0, sorties = 0, ruined = 0, unpaid = 0, captures = 0, alliances = 0, short_alliances = 0;
    u32 allied_turn[SIM_FORCE_CAPACITY][SIM_FORCE_CAPACITY] = {};
    for (u32 index = mark; index < api->ledger_count(e); ++index)
    {
        sim_event event = api->ledger_at(e, index);
        if (event.kind == SIM_EVENT_DEBATE_STARTED) open_debates++;
        if (event.kind == SIM_EVENT_DEBATE_ENDED && open_debates) open_debates--;
        if (event.kind == SIM_EVENT_AI_PLANNED && open_debates) busy_turns++;
        if (event.kind == SIM_EVENT_DEVELOP_STARTED && event.faction < SIM_FORCE_CAPACITY) developed[event.faction]++;
        if (event.kind == SIM_EVENT_CONSCRIPTED) conscripted++;
        if (event.kind == SIM_EVENT_UNIT_SORTIED) sorties++;
        if (event.kind == SIM_EVENT_FACILITY_DESTROYED) ruined++;
        if (event.kind == SIM_EVENT_SALARIES_UNPAID) unpaid++;
        if (event.kind == SIM_EVENT_BASE_CAPTURED) captures++;
        if (event.kind == SIM_EVENT_RELATION_CHANGED && event.faction < SIM_FORCE_CAPACITY && event.detail < SIM_FORCE_CAPACITY)
        {
            u32 *since = &allied_turn[Minimum((u32)event.faction, event.detail)][Maximum((u32)event.faction, event.detail)];
            if (event.delta == RELATION_ALLIANCE) { alliances++; *since = event.turn + 1; }
            else if (event.delta == RELATION_NONE && *since && event.turn + 1 < *since + FEATURE_AI_PLAY_ALLIANCE_TURNS) short_alliances++;
        }
    }
    u32 holders_without_build = 0;
    for (u32 force = 0; force < SIM_FORCE_CAPACITY; ++force) if (holders[force] && !developed[force]) holders_without_build++;
    TEST_CHECK(counts, busy_turns == 0, "no AI faction starts its turn while a debate of another faction is open");
    TEST_CHECK(counts, holders_without_build == 0, "each AI faction that holds a city starts a development in the first years");
    TEST_CHECK(counts, conscripted > 0, "the AI factions conscript after they build barracks");
    TEST_CHECK(counts, sorties >= FEATURE_AI_PLAY_SORTIES, "the AI factions send units out after ai.attack_after_months");
    TEST_CHECK(counts, ruined <= FEATURE_AI_PLAY_RUIN_LIMIT,
               "the AI patrols up to order 90, above order.bandit_threshold 80, so few bandits destroy its facilities");
    TEST_CHECK(counts, unpaid <= FEATURE_AI_PLAY_UNPAID_LIMIT, "the AI keeps the salaries of ai.salary_months months before it spends gold (U-011)");
    TEST_CHECK(counts, captures >= FEATURE_AI_PLAY_CAPTURES, "the AI factions take cities: several cities march on one target (U-012)");
    TEST_CHECK(counts, alliances > 0 && short_alliances == 0,
               "the AI factions ally with a neighbour when another enemy stands near, and keep the alliance (U-015)");
    printf("  the AI play: %u alliances, %u captures, %u unpaid months, %u sorties, %u conscriptions, %u facilities destroyed, troops %llu of %llu\n", alliances, captures, unpaid, sorties, conscripted, ruined,
           (unsigned long long)feature_all_troops(api), (unsigned long long)start_troops);
    TEST_CHECK(counts, feature_all_troops(api) * 100 >= start_troops * FEATURE_AI_PLAY_TROOP_FLOOR,
               "the AI factions keep their start troops after the years: the grain feeds them (farm 1125, knowledge/city-economy.md)");
    if (feature_all_troops(api) * 100 < start_troops * FEATURE_AI_PLAY_TROOP_FLOOR)
        printf("  the troops fell from %llu to %llu\n", (unsigned long long)start_troops, (unsigned long long)feature_all_troops(api));
    engine_fixture_destroy(&f);
}

internal void
feature_ai_tests(test_counts *counts)
{
    feature_ai_explain_tests(counts);
    feature_ai_free_city_tests(counts);
    feature_ai_march_tests(counts);
    feature_ai_defend_tests(counts);
    feature_ai_grain_tests(counts);
    feature_ai_pack_play_tests(counts);
}
