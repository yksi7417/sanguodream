/* Feature tests of a shared game (docs/MULTIPLAYER.md, MP1). Two engines
   run in one test. Engine A is the host and plays through engine_api.
   Engine B applies the steps of A with step_apply, as a browser applies
   the steps that come from the server. After each step, the checksums of
   the two engines agree. The tests also check the locks of a shared
   game: the seat that does not act, the setters and the bad bytes. */

#define FEATURE_MP_TURNS 30   /* the turns of the record with one fixed checksum */

/* The checksum of the state after the first FEATURE_MP_TURNS turns of
   feature_mp_record_checksum_tests. The same test runs native and under
   node (run_tests.bat --web), so the browser and Windows agree on it. If a
   rule changes on purpose, set this value to the checksum that the test
   prints. */
#define FEATURE_MP_RECORD_CHECKSUM 0xdcba2ee7b448432aull

/* The pair of a shared game, and the facts of its steps. */
struct feature_mp_pair
{
    engine_fixture a;
    engine_fixture b;
    u8   city;            /* the city of the faction of A */
    u8   force;
    u32  steps;           /* the steps that went from A to B */
    b32  numbered;        /* each step has the next number */
    b32  applied;         /* B applied each step */
    b32  agreed;          /* after each step, B has the checksum of A */
};

/* The same start on each engine: the sample save, one human faction, and
   the gold. The sample save has a human faction of its own, and no seat
   holds it, so it goes to the AI. These setters come before
   multiplayer_begin. In MP3 a start image takes their place. */
internal void
feature_mp_start(engine_api *api, u8 *city, u8 *force)
{
    void *e = api->engine;
    api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
    *city  = feature_busiest_city(api);
    *force = api->city_at(e, *city).owner_force_id;
    for (u32 other = 0; other < SIM_NO_FORCE; ++other) api->set_faction_human(e, (u8)other, other == *force);
    api->set_city_gold(e, *city, 50000);
}

internal void
feature_mp_pair_init(feature_mp_pair *pair)
{
    *pair = {};
    feature_replay_fixture_init(&pair->a);
    feature_replay_fixture_init(&pair->b);
    u8 city_b, force_b;
    feature_mp_start(&pair->a.api, &pair->city, &pair->force);
    feature_mp_start(&pair->b.api, &city_b, &force_b);
    pair->numbered = pair->applied = pair->agreed = true;
}

/* Moves each step of one engine to the other, as the server relays them. */
internal void
feature_mp_relay_from(feature_mp_pair *pair, engine_api *from, engine_api *to)
{
    u8 bytes[ENGINE_STEP_WIRE_CAPACITY];
    for (;;)
    {
        engine_outgoing_step out = from->steps_outgoing(from->engine, bytes, sizeof(bytes));
        if (!out.size) break;
        if (out.step != pair->steps) pair->numbered = false;
        engine_step_apply_result applied = to->step_apply(to->engine, bytes, out.size);
        if (applied.status != ENGINE_STEP_APPLIED) pair->applied = false;
        if (applied.checksum != out.checksum || out.checksum != to->state_checksum(to->engine)) pair->agreed = false;
        pair->steps++;
    }
}

internal void
feature_mp_relay(feature_mp_pair *pair)
{
    feature_mp_relay_from(pair, &pair->a.api, &pair->b.api);
    feature_mp_relay_from(pair, &pair->b.api, &pair->a.api);
}

/* The seat that acts ends its faction turn until the faction acts. The
   relay runs after each step. */
internal void
feature_mp_until_turn_of(feature_mp_pair *pair, u8 force)
{
    engine_api *a = &pair->a.api, *b = &pair->b.api;
    for (u32 count = 0; count < FEATURE_REPLAY_TURN_LIMIT; ++count)
    {
        if (a->whose_turn(a->engine) == force) return;
        engine_api *seat = a->multiplayer_info(a->engine).local_acts ? a : b;
        if (!seat->end_turn(seat->engine)) return;
        feature_mp_relay(pair);
    }
}

/* The game of feature_replay_play, with the relay after each step. */
internal void
feature_mp_play(feature_mp_pair *pair)
{
    engine_api *a = &pair->a.api;
    void *e = a->engine;
    feature_mp_until_turn_of(pair, pair->force);
    u16 ids[SIM_DEVELOP_OFFICERS];
    feature_take_officers(a, pair->city, ids, 1);
    a->command_patrol(e, pair->city, ids);
    feature_mp_relay(pair);
    feature_take_officers(a, pair->city, ids, 1);
    a->command_conscript(e, pair->city, ids);
    feature_mp_relay(pair);
    engine_sortie_order order = a->sortie_proposal(e, pair->city, 0);
    if (a->command_sortie(e, &order).refusal == SIM_REFUSAL_NONE)
    {
        feature_mp_relay(pair);
        for (u32 id = 0; id < a->unit_count(e); ++id)
        {
            engine_unit unit = a->unit_at(e, id);
            if (unit.id == ENGINE_NO_UNIT || unit.officers[0] != order.officers[0]) continue;
            engine_reach_view reach = a->reachable(e, id);
            if (reach.count > 1) a->command_move(e, id, reach.cells[reach.count - 1].q, reach.cells[reach.count - 1].r);
            feature_mp_relay(pair);
        }
    }
    a->end_turn(e);
    feature_mp_relay(pair);
    feature_mp_until_turn_of(pair, pair->force);

    engine_settings settings = a->settings(e);
    settings.ai = true;
    a->set_settings(e, settings);   /* the host changes the settings: a step */
    feature_mp_relay(pair);
    for (u32 turn = 0; turn < 3; ++turn)
    {
        a->end_turn(e);
        feature_mp_relay(pair);
        feature_mp_until_turn_of(pair, pair->force);
    }
}

/* Engine B applies the steps of A, and after each step it has the state of A. */
internal void
feature_mp_lockstep_tests(test_counts *counts)
{
    feature_mp_pair pair;
    feature_mp_pair_init(&pair);
    engine_api *a = &pair.a.api, *b = &pair.b.api;
    TEST_CHECK(counts, a->state_checksum(a->engine) == b->state_checksum(b->engine),
               "two engines with the same start have the same checksum");
    TEST_CHECK(counts, !a->multiplayer_info(a->engine).shared && a->multiplayer_info(a->engine).local_acts,
               "outside a shared game, the local player acts");

    a->multiplayer_begin(a->engine, pair.force, true);
    b->multiplayer_begin(b->engine, ENGINE_NO_FACTION, false);
    engine_multiplayer_info info = b->multiplayer_info(b->engine);
    TEST_CHECK(counts, info.shared && !info.host && info.local_force == ENGINE_NO_FACTION && !info.local_acts,
               "a spectator seat never acts");
    feature_mp_play(&pair);

    TEST_CHECK(counts, pair.steps >= 8 && pair.steps == a->game_replay_info(a->engine).step_count,
               "each step of the record of A goes to the network");
    TEST_CHECK(counts, pair.numbered, "the steps of A have the numbers 0, 1, 2 and up, in the order of the game");
    TEST_CHECK(counts, pair.applied, "B applies each step of A");
    TEST_CHECK(counts, pair.agreed, "after each step, B has the checksum that A had after it");
    TEST_CHECK(counts, feature_replay_observe(a) == feature_replay_observe(b), "B shows the cities, the officers and the units of A");
    TEST_CHECK(counts, a->settings(a->engine).ai && b->settings(b->engine).ai, "the change of the settings of the host reaches B");
    TEST_CHECK(counts, b->game_replay_info(b->engine).step_count == a->game_replay_info(a->engine).step_count,
               "the replay record of B has each step of A");
    info = a->multiplayer_info(a->engine);
    TEST_CHECK(counts, info.outgoing == 0 && !info.overflow && info.step_count == pair.steps &&
                       b->multiplayer_info(b->engine).step_count == pair.steps,
               "the queue of A is empty, and both engines count the same steps");
    engine_fixture_destroy(&pair.a);
    engine_fixture_destroy(&pair.b);
}

/* The city of another faction than the force, with the most officers. */
internal u8
feature_mp_other_city(engine_api *api, u8 force)
{
    void *e = api->engine;
    u8 best = ENGINE_NO_CITY;
    u32 best_count = 0;
    for (u32 index = 0; index < api->city_count(e); ++index)
    {
        engine_city city = api->city_at(e, index);
        if (city.owner_force_id == ENGINE_NO_FACTION || city.owner_force_id == force) continue;
        u16 ids[SIM_DEVELOP_OFFICERS];
        u32 count = feature_take_officers(api, city.city_id, ids, SIM_DEVELOP_OFFICERS);
        if (count > best_count) { best = city.city_id; best_count = count; }
    }
    return best;
}

/* The locks of a shared game. */
internal void
feature_mp_lock_tests(test_counts *counts)
{
    feature_mp_pair pair;
    feature_mp_pair_init(&pair);
    engine_api *a = &pair.a.api, *b = &pair.b.api;
    void *ea = a->engine, *eb = b->engine;
    u8 other_city  = feature_mp_other_city(a, pair.force);
    u8 other_force = a->city_at(ea, other_city).owner_force_id;
    a->set_faction_human(ea, other_force, true);
    b->set_faction_human(eb, other_force, true);
    a->multiplayer_begin(ea, pair.force, true);
    b->multiplayer_begin(eb, other_force, false);
    feature_mp_until_turn_of(&pair, pair.force);
    TEST_CHECK(counts, a->whose_turn(ea) == pair.force && pair.agreed, "the faction of A acts, and B follows");

    u16 ids[SIM_DEVELOP_OFFICERS];
    feature_take_officers(b, pair.city, ids, 1);
    engine_command_result seat = b->command_patrol(eb, pair.city, ids);
    TEST_CHECK(counts, seat.refusal == ENGINE_REFUSAL_NOT_YOUR_SEAT && seat.command_id == 0,
               "a command of the seat of B, while the faction of A acts, gets ENGINE_REFUSAL_NOT_YOUR_SEAT");
    TEST_CHECK(counts, b->end_turn(eb) == 0 && b->whose_turn(eb) == pair.force, "B cannot end the turn of A");
    TEST_CHECK(counts, b->multiplayer_info(eb).outgoing == 0, "a refused command of B puts no step in the queue");
    TEST_CHECK(counts, b->state_checksum(eb) == a->state_checksum(ea), "the refusals of B change nothing");

    u16 none[SIM_DEVELOP_OFFICERS] = { ENGINE_NO_OFFICER, ENGINE_NO_OFFICER, ENGINE_NO_OFFICER };
    TEST_CHECK(counts, a->command_patrol(ea, pair.city, none).refusal != SIM_REFUSAL_NONE && a->multiplayer_info(ea).outgoing == 0,
               "a command that the sim refuses puts no step in the queue");
    TEST_CHECK(counts, a->start_disaster(ea, pair.city, 0).refusal == ENGINE_REFUSAL_NOT_YOUR_SEAT && a->multiplayer_info(ea).outgoing == 0,
               "an editor command is no step of a shared game, also for the seat that acts");

    u64 before = a->state_checksum(ea);
    u32 gold   = a->city_at(ea, pair.city).gold;
    a->set_city_gold(ea, pair.city, 1);
    a->set_faction_human(ea, pair.force, false);
    a->ledger_clear(ea);
    a->set_seed(ea, 99);
    TEST_CHECK(counts, a->city_at(ea, pair.city).gold == gold && a->state_checksum(ea) == before && a->multiplayer_info(ea).outgoing == 0,
               "the setters, the change of the human factions, the clear of the ledger and the seed change nothing in a shared game");
    engine_settings settings = b->settings(eb);
    settings.disasters = !settings.disasters;
    b->set_settings(eb, settings);
    TEST_CHECK(counts, b->settings(eb).disasters != settings.disasters && b->multiplayer_info(eb).outgoing == 0,
               "only the host can change the settings");

    /* A step of B: the faction of B acts after A ends its turn. */
    a->end_turn(ea);
    feature_mp_relay(&pair);
    feature_mp_until_turn_of(&pair, other_force);
    if (b->whose_turn(eb) == other_force)
    {
        feature_take_officers(b, other_city, ids, 1);
        engine_command_result patrol = b->command_patrol(eb, other_city, ids);
        u8 bytes[ENGINE_STEP_WIRE_CAPACITY];
        engine_outgoing_step out = b->steps_outgoing(eb, bytes, sizeof(bytes));
        engine_step_apply_result applied = a->step_apply(ea, bytes, out.size);
        TEST_CHECK(counts, patrol.refusal == SIM_REFUSAL_NONE && out.size > 0 && applied.status == ENGINE_STEP_APPLIED &&
                           applied.checksum == out.checksum,
                   "when the faction of B acts, B is the author of the step, and A applies it");
    }
    else TEST_CHECK(counts, false, "the faction of B acts after the faction of A");
    engine_fixture_destroy(&pair.a);
    engine_fixture_destroy(&pair.b);
}

/* Bytes that are no step change nothing. */
internal void
feature_mp_bytes_tests(test_counts *counts)
{
    feature_mp_pair pair;
    feature_mp_pair_init(&pair);
    engine_api *a = &pair.a.api, *b = &pair.b.api;
    void *ea = a->engine, *eb = b->engine;
    u8 bytes[ENGINE_STEP_WIRE_CAPACITY] = {};
    TEST_CHECK(counts, b->step_apply(eb, bytes, 8).status == ENGINE_STEP_NOT_SHARED, "outside a shared game, step_apply does nothing");

    a->multiplayer_begin(ea, pair.force, true);
    b->multiplayer_begin(eb, ENGINE_NO_FACTION, false);
    a->end_turn(ea);
    engine_outgoing_step out = a->steps_outgoing(ea, bytes, 4);
    TEST_CHECK(counts, out.size == 0 && a->multiplayer_info(ea).outgoing == 1, "a short buffer takes no step, and the step stays in the queue");
    out = a->steps_outgoing(ea, bytes, sizeof(bytes));
    u64 before = b->state_checksum(eb);
    engine_step_apply_result result = b->step_apply(eb, bytes, out.size - 1);
    TEST_CHECK(counts, result.status == ENGINE_STEP_BAD_BYTES && result.checksum == before && b->state_checksum(eb) == before,
               "a short step changes nothing");
    bytes[0] = ENGINE_STEP_WIRE_VERSION + 1;
    TEST_CHECK(counts, b->step_apply(eb, bytes, out.size).status == ENGINE_STEP_BAD_BYTES, "a step of another version changes nothing");
    bytes[0] = ENGINE_STEP_WIRE_VERSION;
    bytes[1] = ENGINE_STEP_KIND_COUNT;
    TEST_CHECK(counts, b->step_apply(eb, bytes, out.size).status == ENGINE_STEP_BAD_BYTES, "a step of an unknown kind changes nothing");

    sim_command load = {};
    load.kind = SIM_COMMAND_LOAD_SAVE;
    u8 forged[ENGINE_STEP_WIRE_CAPACITY] = { ENGINE_STEP_WIRE_VERSION, ENGINE_STEP_COMMAND };
    u32 size = ENGINE_STEP_WIRE_HEADER + sim_wire_write_command(&load, forged + ENGINE_STEP_WIRE_HEADER, sizeof(forged) - ENGINE_STEP_WIRE_HEADER);
    TEST_CHECK(counts, b->step_apply(eb, forged, size).status == ENGINE_STEP_KIND_REFUSED && b->state_checksum(eb) == before,
               "a load as a step is refused and changes nothing");

    bytes[1] = ENGINE_STEP_COMMAND;
    result = b->step_apply(eb, bytes, out.size);
    TEST_CHECK(counts, result.status == ENGINE_STEP_APPLIED && result.checksum == out.checksum,
               "after the bad bytes, the good step applies and gives the checksum of A");
    engine_fixture_destroy(&pair.a);
    engine_fixture_destroy(&pair.b);
}

/* A record of FEATURE_MP_TURNS turns with the AI gives one fixed checksum. */
internal void
feature_mp_record_checksum_tests(test_counts *counts)
{
    engine_fixture f;
    feature_replay_fixture_init(&f);
    engine_api *api = &f.api;
    void *e = api->engine;
    u8 city, force;
    feature_mp_start(api, &city, &force);
    engine_settings settings = api->settings(e);
    settings.ai = true;
    api->set_settings(e, settings);
    u32 start = api->turn(e);
    for (u32 count = 0; count < FEATURE_MP_TURNS * 64 && api->turn(e) < start + FEATURE_MP_TURNS; ++count) api->end_turn(e);
    u64 checksum = api->state_checksum(e);
    TEST_CHECK(counts, api->turn(e) == start + FEATURE_MP_TURNS, "the record plays FEATURE_MP_TURNS turns");
    if (checksum != FEATURE_MP_RECORD_CHECKSUM)
        printf("  the checksum after %u turns is 0x%016llxull\n", FEATURE_MP_TURNS, (unsigned long long)checksum);
    TEST_CHECK(counts, checksum == FEATURE_MP_RECORD_CHECKSUM,
               "the record of 30 turns gives FEATURE_MP_RECORD_CHECKSUM, native and under node");
    engine_fixture_destroy(&f);
}

internal void
feature_multiplayer_tests(test_counts *counts)
{
    feature_mp_lockstep_tests(counts);
    feature_mp_lock_tests(counts);
    feature_mp_bytes_tests(counts);
    feature_mp_record_checksum_tests(counts);
}
