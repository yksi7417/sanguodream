/* Feature tests of the replay of a game (docs/REPLAY.md): the record of
   the steps of the players, the view of the state after step N, and the
   record in the save file. The main check: the view of step N shows the
   state that the live game had after step N. The tests compare what a
   front end observes through engine_api. */

#define FEATURE_REPLAY_SLOT        27    /* slot_28.hxs; no other test uses it */
#define FEATURE_REPLAY_STATES      256   /* the observed states of one test game */
#define FEATURE_REPLAY_TURN_LIMIT  300   /* the end_turn steps until the faction acts */

/* The fixture of these tests: the placeholder board with the sample save.
   The record needs more permanent memory than engine_fixture_init gives. */
internal void
feature_replay_fixture_init(engine_fixture *f)
{
    f->permanent = test_arena_create(Megabytes(48));
    f->transient = test_arena_create(Megabytes(16));
    engine_config config = engine_default_config();
    config.seed              = 7;
    config.ledger_capacity   = 16384;
    config.use_pack_map      = false;
    config.read_entire_file  = test_platform_read_entire_file;
    config.write_entire_file = test_platform_write_entire_file;
    config.save_folder       = feature_save_folder;
    f->backend = engine_create(&f->permanent, &f->transient, config);
    f->api     = engine_make_api(f->backend);
    engine_settings settings = f->api.settings(f->api.engine);
    settings.disasters      = false;
    settings.ability_change = false;
    settings.lifespan       = false;
    settings.ai             = false;
    settings.events         = false;
    f->api.set_settings(f->api.engine, settings);
}

internal void
feature_replay_mix(u64 *hash, u64 value)
{
    *hash = (*hash ^ value) * 1099511628211ull;
}

/* A hash of what a front end sees: the turn, the date, whose turn, the
   action points, the ledger count, the cities, the officers and the units. */
internal u64
feature_replay_observe(engine_api *api)
{
    void *e = api->engine;
    u64 hash = 14695981039346656037ull;
    engine_date date = api->date(e);
    feature_replay_mix(&hash, api->turn(e));
    feature_replay_mix(&hash, (u64)date.year << 16 | (u64)date.month << 8 | date.day);
    feature_replay_mix(&hash, api->whose_turn(e));
    feature_replay_mix(&hash, api->ledger_count(e));
    for (u32 force = 0; force < SIM_FORCE_CAPACITY; ++force) feature_replay_mix(&hash, api->actions_left(e, (u8)force));
    for (u32 index = 0; index < api->city_count(e); ++index)
    {
        engine_city city = api->city_at(e, index);
        feature_replay_mix(&hash, city.city_id | (u64)city.owner_force_id << 8 | (u64)city.morale << 16 | (u64)city.public_order << 24);
        feature_replay_mix(&hash, (u64)city.gold << 32 | city.troops);
        feature_replay_mix(&hash, city.food);
    }
    for (u32 index = 0; index < api->officer_count(e); ++index)
    {
        engine_officer officer = api->officer_at(e, index);
        feature_replay_mix(&hash, officer.officer_id | (u64)officer.force_id << 16 | (u64)officer.status << 24 | (u64)officer.job_city << 32);
        feature_replay_mix(&hash, officer.current_position | (u64)officer.merit << 16 | (u64)(officer.acted ? 1 : 0) << 32);
    }
    for (u32 id = 0; id < api->unit_count(e); ++id)
    {
        engine_unit unit = api->unit_at(e, id);
        if (unit.id == ENGINE_NO_UNIT) continue;
        feature_replay_mix(&hash, unit.id | (u64)unit.force_id << 16);
        feature_replay_mix(&hash, (u64)(u32)unit.q << 32 | (u32)unit.r);
        feature_replay_mix(&hash, (u64)unit.soldiers << 32 | unit.grain);
    }
    return hash;
}

/* The live game of a test: the observed state after each step. states[0]
   is the state before the first step. */
struct feature_replay_game
{
    u64 states[FEATURE_REPLAY_STATES];
    u32 state_count;
    u8  city;
    u8  force;
    u16 patrol_officer;
    u32 patrol_step;
    u32 patrol_turn;
    u32 patrol_first_event;
};

internal void
feature_replay_note(engine_api *api, feature_replay_game *game)
{
    if (game->state_count < FEATURE_REPLAY_STATES) game->states[game->state_count++] = feature_replay_observe(api);
}

/* Ends faction turns until the faction acts. Each end_turn is a step. */
internal b32
feature_replay_until_turn_of(engine_api *api, feature_replay_game *game, u8 force)
{
    for (u32 count = 0; count < FEATURE_REPLAY_TURN_LIMIT; ++count)
    {
        if (api->whose_turn(api->engine) == force) return true;
        api->end_turn(api->engine);
        feature_replay_note(api, game);
    }
    return false;
}

/* Plays a short game: the setters come before the first step, so the
   start state holds them. Then the faction patrols, conscripts, marches
   and ends turns. Then the AI turns on, and the turns go on. */
internal void
feature_replay_play(engine_api *api, feature_replay_game *game)
{
    void *e = api->engine;
    *game = {};
    api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
    game->city  = feature_busiest_city(api);
    game->force = api->city_at(e, game->city).owner_force_id;
    api->set_faction_human(e, game->force, true);
    api->set_city_gold(e, game->city, 50000);
    feature_replay_note(api, game);

    feature_replay_until_turn_of(api, game, game->force);
    u16 ids[SIM_DEVELOP_OFFICERS];
    feature_take_officers(api, game->city, ids, 1);
    game->patrol_officer     = ids[0];
    game->patrol_step        = game->state_count - 1;
    game->patrol_turn        = api->turn(e);
    game->patrol_first_event = api->ledger_count(e);
    if (api->command_patrol(e, game->city, ids).refusal == SIM_REFUSAL_NONE) feature_replay_note(api, game);
    feature_take_officers(api, game->city, ids, 1);
    if (api->command_conscript(e, game->city, ids).refusal == SIM_REFUSAL_NONE) feature_replay_note(api, game);
    engine_sortie_order order = api->sortie_proposal(e, game->city, 0);
    if (api->command_sortie(e, &order).refusal == SIM_REFUSAL_NONE)
    {
        feature_replay_note(api, game);
        for (u32 id = 0; id < api->unit_count(e); ++id)
        {
            engine_unit unit = api->unit_at(e, id);
            if (unit.id == ENGINE_NO_UNIT || unit.officers[0] != order.officers[0]) continue;
            engine_reach_view reach = api->reachable(e, id);
            if (reach.count > 1 && api->command_move(e, id, reach.cells[reach.count - 1].q, reach.cells[reach.count - 1].r).refusal == SIM_REFUSAL_NONE)
                feature_replay_note(api, game);
        }
    }
    api->end_turn(e);
    feature_replay_note(api, game);
    feature_replay_until_turn_of(api, game, game->force);

    engine_settings settings = api->settings(e);
    settings.ai = true;
    api->set_settings(e, settings);
    feature_replay_note(api, game);
    for (u32 turn = 0; turn < 3; ++turn)
    {
        api->end_turn(e);
        feature_replay_note(api, game);
        feature_replay_until_turn_of(api, game, game->force);
    }
}

/* The view of each step shows the state of the live game after that
   step. The test goes back from the end, then forward one step at a
   time, then in jumps. */
internal void
feature_replay_state_tests(test_counts *counts)
{
    engine_fixture f;
    feature_replay_fixture_init(&f);
    engine_api *api = &f.api;
    void *e = api->engine;
    TEST_CHECK(counts, api->game_replay_info(e).status == ENGINE_GAME_REPLAY_EMPTY && api->game_replay_info(e).step_count == 0,
               "before the first step, the record is empty");
    feature_replay_game game;
    feature_replay_play(api, &game);
    u32 steps = game.state_count - 1;
    u64 live = feature_replay_observe(api);
    engine_game_replay_info info = api->game_replay_info(e);
    TEST_CHECK(counts, info.status == ENGINE_GAME_REPLAY_RECORDING && info.step_count == steps && steps > 10 && !info.viewing,
               "the record has one step for each command, end_turn and change of the settings");
    TEST_CHECK(counts, info.first_edit == ENGINE_NO_STEP, "no state changed outside the steps");

    b32 backward = true, backward_matches = true;
    for (u32 step = steps + 1; step-- > 0;)
    {
        engine_game_replay_view view = api->game_replay_view(e, step);
        if (!view.ok || view.step != step || feature_replay_observe(api) != game.states[step]) backward = false;
        if (!view.matches) backward_matches = false;
    }
    TEST_CHECK(counts, backward, "the view of each step N, from the last step back to 0, shows the live state after step N");
    TEST_CHECK(counts, backward_matches, "each view has the checksum that the live game had after its step");
    TEST_CHECK(counts, api->game_replay_info(e).viewing && api->game_replay_info(e).view_step == 0, "the view stays open on step 0");

    b32 forward = true;
    for (u32 step = 1; step <= steps; ++step)
    {
        engine_game_replay_view view = api->game_replay_view(e, step);
        if (!view.ok || !view.matches || feature_replay_observe(api) != game.states[step]) forward = false;
    }
    TEST_CHECK(counts, forward, "a step forward from the view shows the live state after the next step");

    u32 jumps[] = { 3, steps, 1, steps / 2, steps / 2, 0, steps - 1 };
    b32 jumped = true;
    for (u32 index = 0; index < ArrayCount(jumps); ++index)
    {
        engine_game_replay_view view = api->game_replay_view(e, jumps[index]);
        if (!view.ok || !view.matches || feature_replay_observe(api) != game.states[jumps[index]]) jumped = false;
    }
    TEST_CHECK(counts, jumped, "a jump to any step, also to the same step again, shows the live state after that step");
    TEST_CHECK(counts, !api->game_replay_view(e, steps + 1).ok, "a step past the end has no view");

    api->game_replay_close(e);
    TEST_CHECK(counts, !api->game_replay_info(e).viewing && feature_replay_observe(api) == live,
               "close returns to the live game");
    TEST_CHECK(counts, api->game_replay_info(e).step_count == steps, "the view adds no step to the record");
    engine_fixture_destroy(&f);
}

/* Each step names its player, its turn and what it did. */
internal void
feature_replay_step_tests(test_counts *counts)
{
    engine_fixture f;
    feature_replay_fixture_init(&f);
    engine_api *api = &f.api;
    void *e = api->engine;
    feature_replay_game game;
    feature_replay_play(api, &game);
    u32 steps = api->game_replay_info(e).step_count;

    engine_game_replay_step patrol = api->game_replay_step_at(e, game.patrol_step);
    TEST_CHECK(counts, patrol.index == game.patrol_step && patrol.kind == ENGINE_STEP_COMMAND && patrol.command == SIM_COMMAND_PATROL &&
                       strcmp(patrol.key, "patrol") == 0,
               "the patrol is a command step with its key");
    TEST_CHECK(counts, patrol.force == game.force && patrol.turn == game.patrol_turn && patrol.officer == game.patrol_officer,
               "the patrol step names the faction of the player, the turn and the officer");
    TEST_CHECK(counts, patrol.first_event == game.patrol_first_event && patrol.event_count > 0,
               "the patrol step holds the range of its events in the ledger");
    TEST_CHECK(counts, patrol.place.kind == ENGINE_PLACE_CITY && patrol.place.id == game.city,
               "the patrol step has its city as its place on the map");

    engine_game_replay_step first = api->game_replay_step_at(e, 0);
    TEST_CHECK(counts, first.kind == ENGINE_STEP_COMMAND && first.command == SIM_COMMAND_END_TURN && strcmp(first.key, "end_turn") == 0,
               "the first step is an end_turn");
    b32 has_settings = false;
    b32 ordered = true;
    for (u32 index = 0; index < steps; ++index)
    {
        engine_game_replay_step step = api->game_replay_step_at(e, index);
        if (step.kind == ENGINE_STEP_SETTINGS && strcmp(step.key, "settings") == 0) has_settings = true;
        if (index > 0 && step.turn < api->game_replay_step_at(e, index - 1).turn) ordered = false;
    }
    TEST_CHECK(counts, has_settings, "the change of the settings is a step");
    TEST_CHECK(counts, ordered, "the steps are in the order of the turns");
    TEST_CHECK(counts, api->game_replay_step_at(e, steps).index == ENGINE_NO_STEP, "a step past the end is no step");

    u32 turn = game.patrol_turn;
    u32 found = api->game_replay_turn_step(e, turn);
    TEST_CHECK(counts, found <= game.patrol_step && api->game_replay_step_at(e, found).turn == turn &&
                       (found == 0 || api->game_replay_step_at(e, found - 1).turn < turn),
               "turn_step gives the first step of the turn");
    api->game_replay_view(e, found);
    TEST_CHECK(counts, api->turn(e) == turn, "the view of the first step of a turn shows that turn");
    TEST_CHECK(counts, api->game_replay_turn_step(e, api->game_replay_step_at(e, steps - 1).turn + 1) == steps,
               "a turn after the last step gives the end of the record");
    api->game_replay_close(e);

    /* A refused command is no step. */
    u16 none[SIM_DEVELOP_OFFICERS] = { ENGINE_NO_OFFICER, ENGINE_NO_OFFICER, ENGINE_NO_OFFICER };
    TEST_CHECK(counts, api->command_patrol(e, game.city, none).refusal != SIM_REFUSAL_NONE &&
                       api->game_replay_info(e).step_count == steps,
               "a refused command adds no step");
    engine_fixture_destroy(&f);
}

/* A command while the view is open returns to the live game first. A
   load starts a new record. A change outside the steps marks the step. */
internal void
feature_replay_live_tests(test_counts *counts)
{
    engine_fixture f;
    feature_replay_fixture_init(&f);
    engine_api *api = &f.api;
    void *e = api->engine;
    feature_replay_game game;
    feature_replay_play(api, &game);
    u32 steps = api->game_replay_info(e).step_count;
    u32 live_turn = api->turn(e);

    api->game_replay_view(e, 1);
    api->end_turn(e);
    engine_game_replay_info info = api->game_replay_info(e);
    TEST_CHECK(counts, !info.viewing && info.step_count == steps + 1 && api->turn(e) >= live_turn,
               "a command while the view is open runs on the live game and adds one step");
    engine_game_replay_view last = api->game_replay_view(e, steps + 1);
    TEST_CHECK(counts, last.ok && last.matches, "the step after the view replays the same");
    api->game_replay_close(e);

    /* A clear of the ledger is a step: the steps after it have other ledger indices. */
    u32 before_clear = api->game_replay_info(e).step_count;
    api->ledger_clear(e);
    api->end_turn(e);
    u64 cleared_live = feature_replay_observe(api);
    engine_game_replay_step clear = api->game_replay_step_at(e, before_clear);
    TEST_CHECK(counts, clear.kind == ENGINE_STEP_LEDGER_CLEAR && strcmp(clear.key, "ledger_clear") == 0, "a clear of the ledger is a step");
    b32 all_match = true;
    for (u32 step = api->game_replay_info(e).step_count + 1; step-- > 0;)
    {
        if (!api->game_replay_view(e, step).matches) all_match = false;
    }
    api->game_replay_view(e, before_clear + 2);
    TEST_CHECK(counts, all_match && feature_replay_observe(api) == cleared_live,
               "each view before and after the clear of the ledger has the state of the live game");
    api->game_replay_close(e);
    TEST_CHECK(counts, feature_replay_observe(api) == cleared_live, "close after the views returns to the live game");

    feature_replay_until_turn_of(api, &game, game.force);
    u32 before_edit = api->game_replay_info(e).step_count;
    api->set_city_gold(e, game.city, 12345);
    api->end_turn(e);
    info = api->game_replay_info(e);
    TEST_CHECK(counts, info.first_edit == before_edit && api->game_replay_step_at(e, before_edit).edited,
               "a setter between two steps marks the next step as edited");
    TEST_CHECK(counts, !api->game_replay_step_at(e, before_edit - 1).edited, "the steps before the edit are not marked");

    api->load_save(e, SAMPLE_SAVE_RELATIVE_PATH);
    info = api->game_replay_info(e);
    TEST_CHECK(counts, info.status == ENGINE_GAME_REPLAY_EMPTY && info.step_count == 0 && !info.viewing,
               "a load starts a new record");
    engine_fixture_destroy(&f);
}

/* The save file carries the record: a second engine loads the save and
   shows the same steps and the same states. */
internal void
feature_replay_save_tests(test_counts *counts)
{
    engine_fixture f;
    feature_replay_fixture_init(&f);
    engine_api *api = &f.api;
    void *e = api->engine;
    feature_replay_game game;
    feature_replay_play(api, &game);
    u32 steps = api->game_replay_info(e).step_count;
    u64 live = feature_replay_observe(api);
    api->game_replay_view(e, 2);
    TEST_CHECK(counts, api->save_game(e, FEATURE_REPLAY_SLOT, "replay") == ENGINE_SAVE_OK, "the game saves while the view is open");
    TEST_CHECK(counts, !api->game_replay_info(e).viewing && feature_replay_observe(api) == live,
               "the save returns to the live game first: the slot holds the live game");

    engine_fixture other;
    feature_replay_fixture_init(&other);
    engine_api *second = &other.api;
    void *e2 = second->engine;
    TEST_CHECK(counts, second->load_game(e2, FEATURE_REPLAY_SLOT) == ENGINE_SAVE_OK, "a second engine loads the save");
    engine_game_replay_info info = second->game_replay_info(e2);
    TEST_CHECK(counts, info.status == ENGINE_GAME_REPLAY_RECORDING && info.step_count == steps && info.version == api->game_replay_info(e).version,
               "the loaded game has the record of the save");
    TEST_CHECK(counts, feature_replay_observe(second) == live, "the loaded game is the live game of the save");
    b32 same_steps = true;
    for (u32 index = 0; index < steps; ++index)
    {
        engine_game_replay_step a = api->game_replay_step_at(e, index), b = second->game_replay_step_at(e2, index);
        if (a.kind != b.kind || a.command != b.command || a.force != b.force || a.turn != b.turn || a.first_event != b.first_event) same_steps = false;
    }
    TEST_CHECK(counts, same_steps, "each step of the loaded record is the step of the save");
    b32 same_states = true;
    for (u32 step = 0; step <= steps; ++step)
    {
        engine_game_replay_view view = second->game_replay_view(e2, step);
        if (!view.ok || !view.matches || feature_replay_observe(second) != game.states[step]) same_states = false;
    }
    TEST_CHECK(counts, same_states, "the second engine replays each step of the saved game to the same state");
    second->game_replay_close(e2);
    second->end_turn(e2);
    TEST_CHECK(counts, second->game_replay_info(e2).step_count == steps + 1, "the loaded game records its next steps after the old ones");
    engine_fixture_destroy(&other);
    engine_fixture_destroy(&f);
}

internal void
feature_game_replay_tests(test_counts *counts)
{
    feature_replay_state_tests(counts);
    feature_replay_step_tests(counts);
    feature_replay_live_tests(counts);
    feature_replay_save_tests(counts);
}
