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
#define FEATURE_MP_RECORD_CHECKSUM 0x11f2b7d592d6916eull   /* the play run of 2026-11-07: the AI builds, and its debates end without a pause */

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
    b32  started;         /* B loaded the start image of A */
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
    feature_mp_start(&pair->a.api, &pair->city, &pair->force);
    /* B takes the start image of A (MP3), as a guest takes the image of the host. */
    engine_api *a = &pair->a.api, *b = &pair->b.api;
    u32 size = a->start_image_size(a->engine);
    u8 *image = (u8 *)malloc(size);
    u32 written = a->start_image(a->engine, image, size);
    pair->started = written > 0 && b->load_start_image(b->engine, image, written);
    free(image);
    pair->numbered = pair->applied = pair->agreed = true;
}

/* The start image: B gets the state of A, also without a load of its own. */
internal void
feature_mp_start_image_tests(test_counts *counts)
{
    feature_mp_pair pair;
    feature_mp_pair_init(&pair);
    engine_api *a = &pair.a.api, *b = &pair.b.api;
    TEST_CHECK(counts, pair.started, "B loads the start image of A");
    TEST_CHECK(counts, a->state_checksum(a->engine) == b->state_checksum(b->engine) &&
                       feature_replay_observe(a) == feature_replay_observe(b),
               "after the start image, B has the checksum and the cities, officers and units of A");
    TEST_CHECK(counts, b->ledger_count(b->engine) == a->ledger_count(a->engine), "the start image holds the ledger");
    TEST_CHECK(counts, b->game_replay_info(b->engine).status == ENGINE_GAME_REPLAY_EMPTY, "the start image starts a new replay record");
    u8 small[16];
    TEST_CHECK(counts, a->start_image(a->engine, small, sizeof(small)) == 0, "a short buffer takes no start image");
    TEST_CHECK(counts, !b->load_start_image(b->engine, small, sizeof(small)) &&
                       b->state_checksum(b->engine) == a->state_checksum(a->engine),
               "bytes that are no image change nothing");
    b->multiplayer_begin(b->engine, ENGINE_NO_FACTION, false);
    u32 size = a->start_image_size(a->engine);
    u8 *image = (u8 *)malloc(size);
    u32 written = a->start_image(a->engine, image, size);
    TEST_CHECK(counts, !b->load_start_image(b->engine, image, written), "a shared game refuses a start image");
    free(image);
    engine_fixture_destroy(&pair.a);
    engine_fixture_destroy(&pair.b);
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
        if (out.acting != to->whose_turn(to->engine)) pair->agreed = false;   /* the faction of the checksum message */
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
    u8 acting = a->whose_turn(ea);
    a->end_turn(ea);
    engine_outgoing_step out = a->steps_outgoing(ea, bytes, 4);
    TEST_CHECK(counts, out.size == 0 && a->multiplayer_info(ea).outgoing == 1, "a short buffer takes no step, and the step stays in the queue");
    out = a->steps_outgoing(ea, bytes, sizeof(bytes));
    TEST_CHECK(counts, bytes[0] == ENGINE_STEP_WIRE_VERSION && bytes[1] == ENGINE_STEP_COMMAND && bytes[3] == acting,
               "the bytes of a step name the version, the kind and the faction that acted, for the room server");
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

/* The bytes of a step of the room server (MP4): the header only, as
   web/rooms.py writes it. Byte 3 is the faction that acts in the room. */
internal u32
feature_mp_server_step(u8 *bytes, u8 kind, u8 force, u8 acting, u32 value)
{
    bytes[0] = ENGINE_STEP_WIRE_VERSION;
    bytes[1] = kind;
    bytes[2] = force;
    bytes[3] = acting;
    for (u32 index = 0; index < 4; ++index) bytes[4 + index] = (u8)(value >> (8 * index));
    return ENGINE_STEP_WIRE_HEADER;
}

/* The server step goes to both engines, as the server sends it to each
   player. True if both apply it and the checksums agree after it. */
internal b32
feature_mp_server_apply(feature_mp_pair *pair, u8 kind, u8 force, u32 value)
{
    engine_api *a = &pair->a.api, *b = &pair->b.api;
    u8 bytes[ENGINE_STEP_WIRE_HEADER];
    u32 size = feature_mp_server_step(bytes, kind, force, a->whose_turn(a->engine), value);
    engine_step_apply_result on_a = a->step_apply(a->engine, bytes, size);
    engine_step_apply_result on_b = b->step_apply(b->engine, bytes, size);
    pair->steps++;
    return on_a.status == ENGINE_STEP_APPLIED && on_b.status == ENGINE_STEP_APPLIED && on_a.checksum == on_b.checksum;
}

internal b32
feature_mp_is_human(engine_api *api, u8 force)
{
    for (u32 index = 0; index < api->faction_count(api->engine); ++index)
    {
        engine_faction faction = api->faction_at(api->engine, index);
        if (faction.force_id == force) return faction.human;
    }
    return false;
}

/* MP4: the steps of the room server. A timer ends a faction turn, the AI
   takes the faction of an absent player and finishes its turn, and the
   player gets the faction back. */
internal void
feature_mp_server_steps_tests(test_counts *counts)
{
    feature_mp_pair pair;
    feature_mp_pair_init(&pair);
    engine_api *a = &pair.a.api, *b = &pair.b.api;
    void *ea = a->engine, *eb = b->engine;
    u8 other_city  = feature_mp_other_city(a, pair.force);
    u8 other_force = a->city_at(ea, other_city).owner_force_id;
    engine_settings settings = a->settings(ea);
    settings.ai = true;
    a->set_settings(ea, settings);
    b->set_settings(eb, settings);
    a->set_faction_human(ea, other_force, true);
    b->set_faction_human(eb, other_force, true);
    a->multiplayer_begin(ea, pair.force, true);
    b->multiplayer_begin(eb, other_force, false);
    feature_mp_until_turn_of(&pair, other_force);
    TEST_CHECK(counts, b->whose_turn(eb) == other_force && pair.agreed, "the faction of B acts");

    u32 steps = b->multiplayer_info(eb).step_count;
    TEST_CHECK(counts, feature_mp_server_apply(&pair, ENGINE_STEP_END_TURN, other_force, 0),
               "the end_turn step of the server applies on both engines, and the checksums agree");
    TEST_CHECK(counts, a->whose_turn(ea) != other_force && b->whose_turn(eb) == a->whose_turn(ea),
               "after the end_turn step of the server, the faction of B no longer acts");
    TEST_CHECK(counts, b->multiplayer_info(eb).step_count == steps + 1 && a->multiplayer_info(ea).outgoing == 0,
               "a step of the server counts, and it goes into no queue");
    u64 before = b->state_checksum(eb);
    TEST_CHECK(counts, feature_mp_server_apply(&pair, ENGINE_STEP_END_TURN, other_force, 0) && b->state_checksum(eb) == before &&
                       b->multiplayer_info(eb).step_count == steps + 2,
               "an end_turn step for a faction that does not act changes nothing, but it counts");

    feature_mp_until_turn_of(&pair, other_force);
    TEST_CHECK(counts, b->whose_turn(eb) == other_force, "the faction of B acts again");
    u32 turn = b->turn(eb);
    /* One step makes the faction a faction of the AI and finishes its turn. With two
       steps, the host is the author between them, because a faction without a player
       acts. Then the host and the server can both send the next step. */
    TEST_CHECK(counts, feature_mp_server_apply(&pair, ENGINE_STEP_AI_TURN, other_force, 0),
               "the AI turn step applies on both engines, and the checksums agree");
    TEST_CHECK(counts, !feature_mp_is_human(a, other_force) && !feature_mp_is_human(b, other_force),
               "the AI turn step makes the faction a faction of the AI");
    TEST_CHECK(counts, b->whose_turn(eb) != other_force && (b->whose_turn(eb) == pair.force || b->turn(eb) > turn),
               "the AI finishes the faction turn, and the next human faction acts");
    a->end_turn(ea);
    feature_mp_relay(&pair);
    TEST_CHECK(counts, pair.agreed && pair.applied, "after the AI turn, the steps of A apply on B and the checksums agree");
    engine_game_replay_info replay = b->game_replay_info(eb);
    engine_game_replay_view view = b->game_replay_view(eb, replay.step_count - 1);
    TEST_CHECK(counts, view.ok && view.matches, "the replay of B rebuilds the AI turn with the checksum of the live game");
    b->game_replay_close(eb);
    b32 ai_step = false;
    for (u32 index = 0; index < replay.step_count; ++index)
    {
        if (strcmp(b->game_replay_step_at(eb, index).key, "ai_turn") == 0) ai_step = true;
    }
    TEST_CHECK(counts, ai_step, "the replay record has the step ai_turn");

    TEST_CHECK(counts, feature_mp_server_apply(&pair, ENGINE_STEP_HUMAN, other_force, 1) && feature_mp_is_human(b, other_force),
               "set_faction_human true from the server gives the faction back to the player");
    feature_mp_until_turn_of(&pair, other_force);
    TEST_CHECK(counts, b->whose_turn(eb) == other_force && b->multiplayer_info(eb).local_acts,
               "the faction of B acts again, and B is the author");
    engine_fixture_destroy(&pair.a);
    engine_fixture_destroy(&pair.b);
}

/* MP5: a snapshot of the live game, then the log after it. Engine C joins
   late: it loads the snapshot of A and applies only the steps after it. */
/* MP9: a faction of the AI in the start image goes to a late player with a step
   of the server. The faction then acts in the turn order, and the player is the author. */
internal void
feature_mp_late_seat_tests(test_counts *counts)
{
    feature_mp_pair pair;
    feature_mp_pair_init(&pair);
    engine_api *a = &pair.a.api, *b = &pair.b.api;
    void *ea = a->engine, *eb = b->engine;
    u8 other_city  = feature_mp_other_city(a, pair.force);
    u8 other_force = a->city_at(ea, other_city).owner_force_id;
    engine_settings settings = a->settings(ea);
    settings.ai = true;
    a->set_settings(ea, settings);
    b->set_settings(eb, settings);
    a->multiplayer_begin(ea, pair.force, true);
    b->multiplayer_begin(eb, other_force, false);
    TEST_CHECK(counts, !feature_mp_is_human(a, other_force) && !feature_mp_is_human(b, other_force),
               "the faction of the late player is a faction of the AI at the start");
    TEST_CHECK(counts, a->end_turn(ea) != 0, "the player of the start ends a faction turn");
    feature_mp_relay(&pair);
    TEST_CHECK(counts, pair.agreed && pair.applied, "the checksums agree after that step");
    TEST_CHECK(counts, feature_mp_server_apply(&pair, ENGINE_STEP_HUMAN, other_force, 1) &&
                       feature_mp_is_human(a, other_force) && feature_mp_is_human(b, other_force),
               "the step of the server gives the faction to the late player on both engines");
    feature_mp_until_turn_of(&pair, other_force);
    TEST_CHECK(counts, a->whose_turn(ea) == other_force && b->whose_turn(eb) == other_force && pair.agreed,
               "the faction of the late player acts in the turn order, and the checksums agree");
    TEST_CHECK(counts, b->multiplayer_info(eb).local_acts && !a->multiplayer_info(ea).local_acts,
               "the late player is the author, and the host cannot act for the faction");
    TEST_CHECK(counts, b->end_turn(eb) != 0, "the late player ends the faction turn");
    feature_mp_relay(&pair);
    TEST_CHECK(counts, pair.agreed && pair.applied && a->state_checksum(ea) == b->state_checksum(eb),
               "the step of the late player applies on the other engine with the same checksum");
    engine_fixture_destroy(&pair.a);
    engine_fixture_destroy(&pair.b);
}

internal void
feature_mp_snapshot_tests(test_counts *counts)
{
    feature_mp_pair pair;
    feature_mp_pair_init(&pair);
    engine_api *a = &pair.a.api;
    void *ea = a->engine;
    a->multiplayer_begin(ea, pair.force, true);
    pair.b.api.multiplayer_begin(pair.b.api.engine, ENGINE_NO_FACTION, false);
    u32 start_turn = a->turn(ea);
    feature_mp_until_turn_of(&pair, pair.force);
    a->end_turn(ea);
    feature_mp_relay(&pair);
    feature_mp_until_turn_of(&pair, pair.force);
    TEST_CHECK(counts, a->turn(ea) > start_turn && pair.steps > 0, "a game turn passes in the shared game");

    /* The snapshot: the image of the live game after the last step. */
    u32 snapshot_step = a->multiplayer_info(ea).step_count - 1;
    u64 snapshot_checksum = a->state_checksum(ea);
    u32 size = a->start_image_size(ea);
    u8 *image = (u8 *)malloc(size);
    u32 written = a->start_image(ea, image, size);
    TEST_CHECK(counts, written > 0, "the host takes a snapshot of the live game");

    engine_fixture c;
    feature_replay_fixture_init(&c);
    engine_api *capi = &c.api;
    void *ec = capi->engine;
    TEST_CHECK(counts, capi->load_start_image(ec, image, written) && capi->state_checksum(ec) == snapshot_checksum,
               "the snapshot loads and gives the checksum of the step of the snapshot");
    free(image);
    capi->multiplayer_begin(ec, ENGINE_NO_FACTION, false);
    capi->multiplayer_first_step(ec, snapshot_step + 1);
    TEST_CHECK(counts, capi->multiplayer_info(ec).step_count == snapshot_step + 1,
               "after the snapshot, the engine counts the steps from the step after the snapshot");
    capi->multiplayer_first_step(ec, 0);
    TEST_CHECK(counts, capi->multiplayer_info(ec).step_count == snapshot_step + 1, "the first step is set one time, before a step");

    /* The game goes on. Each step of A goes to B and to C. */
    b32 c_agreed = true, c_numbered = true;
    u8 bytes[ENGINE_STEP_WIRE_CAPACITY];
    for (u32 round = 0; round < 2; ++round)
    {
        if (!a->end_turn(ea)) break;
        for (;;)
        {
            engine_outgoing_step out = a->steps_outgoing(ea, bytes, sizeof(bytes));
            if (!out.size) break;
            if (out.step != capi->multiplayer_info(ec).step_count) c_numbered = false;
            pair.b.api.step_apply(pair.b.api.engine, bytes, out.size);
            engine_step_apply_result applied = capi->step_apply(ec, bytes, out.size);
            if (applied.status != ENGINE_STEP_APPLIED || applied.checksum != out.checksum) c_agreed = false;
        }
        feature_mp_until_turn_of(&pair, pair.force);
    }
    TEST_CHECK(counts, c_numbered, "the steps after the snapshot have the numbers that C expects");
    TEST_CHECK(counts, c_agreed && capi->state_checksum(ec) == a->state_checksum(ea),
               "the snapshot plus the log after it gives the checksum of the live game");
    engine_fixture_destroy(&c);
    engine_fixture_destroy(&pair.a);
    engine_fixture_destroy(&pair.b);
}

/* MP6: an engine with one changed city gives another checksum at the next
   step. The engine then loads the good state of the room and follows again. */
internal void
feature_mp_desync_tests(test_counts *counts)
{
    feature_mp_pair pair;
    feature_mp_pair_init(&pair);
    engine_api *a = &pair.a.api, *b = &pair.b.api;
    void *ea = a->engine, *eb = b->engine;
    b->set_city_gold(eb, pair.city, a->city_at(ea, pair.city).gold + 1);   /* a cheat, or a bug of the determinism */
    a->multiplayer_begin(ea, pair.force, true);
    b->multiplayer_begin(eb, ENGINE_NO_FACTION, false);
    feature_mp_until_turn_of(&pair, pair.force);
    a->end_turn(ea);
    u8 bytes[ENGINE_STEP_WIRE_CAPACITY];
    engine_outgoing_step out = a->steps_outgoing(ea, bytes, sizeof(bytes));
    engine_step_apply_result applied = b->step_apply(eb, bytes, out.size);
    TEST_CHECK(counts, out.size > 0 && applied.status == ENGINE_STEP_APPLIED && applied.checksum != out.checksum,
               "an engine with one changed city gives another checksum at the next step");

    /* The recovery: B loads the good state of the step, and the steps after it agree. */
    u32 size = a->start_image_size(ea);
    u8 *image = (u8 *)malloc(size);
    u32 written = a->start_image(ea, image, size);
    b->multiplayer_end(eb);
    b32 loaded = written && b->load_start_image(eb, image, written);
    free(image);
    b->multiplayer_begin(eb, ENGINE_NO_FACTION, false);
    b->multiplayer_first_step(eb, out.step + 1);
    TEST_CHECK(counts, loaded && b->state_checksum(eb) == out.checksum, "the good state of the room gives the checksum of the step");
    a->end_turn(ea);
    out = a->steps_outgoing(ea, bytes, sizeof(bytes));
    applied = b->step_apply(eb, bytes, out.size);
    TEST_CHECK(counts, applied.status == ENGINE_STEP_APPLIED && applied.checksum == out.checksum &&
                       b->multiplayer_info(eb).step_count == a->multiplayer_info(ea).step_count,
               "after the load of the good state, the next step gives the checksum of the host");
    engine_fixture_destroy(&pair.a);
    engine_fixture_destroy(&pair.b);
}

/* MP6: the input of the referee (code/wasm_referee.cpp) for a log of the
   host, and the checksums of the host after each step. The test of the
   referee in tests/server/referee_tests.py runs the referee under node on
   these files. Thus the referee and this engine must agree.
   The images of the native build and of the web build have different
   layouts. The referee gets the images of the browsers, so its test takes
   the files of the web build ("run_tests.bat --web"). */
#ifdef __EMSCRIPTEN__
#define FEATURE_MP_REFEREE_INPUT  "referee_fixture.bin"
#define FEATURE_MP_REFEREE_OUTPUT "referee_fixture.txt"
#else
#define FEATURE_MP_REFEREE_INPUT  "referee_fixture_native.bin"
#define FEATURE_MP_REFEREE_OUTPUT "referee_fixture_native.txt"
#endif
#define FEATURE_MP_REFEREE_TURNS  4

struct feature_mp_bytes
{
    u8 *data;
    u32 used, capacity;
};

internal void
feature_mp_put(feature_mp_bytes *out, const void *data, u32 size)
{
    if (out->used + size > out->capacity) return;
    memcpy(out->data + out->used, data, size);
    out->used += size;
}

internal void
feature_mp_put_u32(feature_mp_bytes *out, u32 value)
{
    u8 bytes[4] = { (u8)value, (u8)(value >> 8), (u8)(value >> 16), (u8)(value >> 24) };
    feature_mp_put(out, bytes, 4);
}

internal void
feature_mp_referee_fixture_tests(test_counts *counts)
{
    feature_mp_pair pair;
    feature_mp_pair_init(&pair);
    engine_api *a = &pair.a.api;
    void *ea = a->engine;
    u8 humans[64];
    u32 human_count = 0;
    for (u32 index = 0; index < a->faction_count(ea) && human_count < ArrayCount(humans); ++index)
    {
        engine_faction faction = a->faction_at(ea, index);
        if (faction.human) humans[human_count++] = faction.force_id;
    }
    u32 image_size = a->start_image_size(ea);
    u32 input_capacity = image_size + (u32)Megabytes(1);
    feature_mp_bytes input = { (u8 *)malloc(input_capacity), 0, input_capacity };
    u8 *image = (u8 *)malloc(image_size);
    image_size = a->start_image(ea, image, image_size);
    const char *pack = "placeholder";
    feature_mp_put(&input, "HXRF", 4);
    feature_mp_put_u32(&input, 1);                  /* REFEREE_VERSION */
    feature_mp_put_u32(&input, 1);                  /* REFEREE_PLACEHOLDER_MAP: the map of feature_replay_fixture_init */
    feature_mp_put_u32(&input, 16384);              /* its ledger */
    feature_mp_put_u32(&input, 0);                  /* the start image */
    feature_mp_put_u32(&input, (u32)strlen(pack));
    feature_mp_put(&input, pack, (u32)strlen(pack));
    feature_mp_put_u32(&input, human_count);
    feature_mp_put(&input, humans, human_count);
    feature_mp_put_u32(&input, image_size);
    feature_mp_put(&input, image, image_size);
    free(image);

    a->multiplayer_begin(ea, pair.force, true);
    char expected[4096];
    u32 written = 0, step_count = 0, count_at = input.used;
    feature_mp_put_u32(&input, 0);                  /* the number of the steps: set below */
    u8 bytes[ENGINE_STEP_WIRE_CAPACITY];
    for (u32 turn = 0; turn < FEATURE_MP_REFEREE_TURNS && a->end_turn(ea); ++turn)
    {
        for (engine_outgoing_step out = a->steps_outgoing(ea, bytes, sizeof(bytes)); out.size;
             out = a->steps_outgoing(ea, bytes, sizeof(bytes)))
        {
            feature_mp_put_u32(&input, out.size);
            feature_mp_put(&input, bytes, out.size);
            written += (u32)snprintf(expected + written, sizeof(expected) - written, "%u %016llx\n", out.step,
                                     (unsigned long long)out.checksum);
            step_count++;
        }
    }
    written += (u32)snprintf(expected + written, sizeof(expected) - written, "done %u\n", step_count);
    for (u32 index = 0; index < 4; ++index) input.data[count_at + index] = (u8)(step_count >> (8 * index));
    TEST_CHECK(counts, step_count == FEATURE_MP_REFEREE_TURNS && input.used < input.capacity, "the host makes the log of the referee fixture");
    TEST_CHECK(counts, test_platform_write_entire_file(FEATURE_MP_REFEREE_INPUT, input.data, input.used) &&
                       test_platform_write_entire_file(FEATURE_MP_REFEREE_OUTPUT, expected, written),
               "the test writes the input and the checksums of the referee fixture");
    free(input.data);
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

/* The play report of 2026-11-04: a guest of a game of han_196 saw 漢中 at the place
   of 建業. The city ids of the placeholder pack are not the ids of a pack. The guest
   must show the names of the start image, also after it loaded another name index. */
internal void
feature_mp_pack_names_tests(test_counts *counts)
{
    engine_fixture host, guest;
    feature_world_map_init(&host, HEXDOM_DATA_ROOT "/han_196");
    feature_world_map_init(&guest, HEXDOM_DATA_ROOT "/han_196");
    engine_api *h = &host.api, *g = &guest.api;
    TEST_CHECK(counts, h->load_scenario(h->engine, h->scenario_at(h->engine, 0).file).status == ENGINE_LOAD_OK, "the host loads the scenario of 196");
    TEST_CHECK(counts, g->load_name_index(g->engine, NAME_INDEX_RELATIVE_PATH).status == ENGINE_LOAD_OK &&
                       memcmp(g->city_name_at(g->engine, 36).name, "\xBA\x7E\xA4\xA4", 4) == 0,
               "before the join, the guest has the name index of the placeholder pack: city 36 is 漢中");

    u32 capacity = h->start_image_size(h->engine);
    u8 *image = (u8 *)malloc(capacity);
    u32 written = h->start_image(h->engine, image, capacity);
    TEST_CHECK(counts, written && g->load_start_image(g->engine, image, written), "the guest loads the start image of the host");
    free(image);
    TEST_CHECK(counts, memcmp(g->city_name_at(g->engine, 36).name, "\xAB\xD8\xB7\x7E", 4) == 0, "after the join, city 36 of the guest is 建業");
    TEST_CHECK(counts, memcmp(g->city_name_at(g->engine, 19).name, "\xBA\x7E\xA4\xA4", 4) == 0, "after the join, city 19 of the guest is 漢中");
    TEST_CHECK(counts, g->state_checksum(g->engine) == h->state_checksum(h->engine), "the names do not change the checksum");
    engine_fixture_destroy(&host);
    engine_fixture_destroy(&guest);
}

internal void
feature_multiplayer_tests(test_counts *counts)
{
    feature_mp_pack_names_tests(counts);
    feature_mp_start_image_tests(counts);
    feature_mp_lockstep_tests(counts);
    feature_mp_lock_tests(counts);
    feature_mp_bytes_tests(counts);
    feature_mp_server_steps_tests(counts);
    feature_mp_late_seat_tests(counts);
    feature_mp_snapshot_tests(counts);
    feature_mp_desync_tests(counts);
    feature_mp_referee_fixture_tests(counts);
    feature_mp_record_checksum_tests(counts);
}
