/* feature_tests.exe: the middle of the pyramid. Drives the real backend only
   through engine_api: behavioral tests of the engine, then every spec fixture
   as an acceptance test. No UI.

   Usage: feature_tests.exe [fixtures-dir]
   Default fixtures dir: <exe dir>\..\spec-src\spec\fixtures */

#define _CRT_SECURE_NO_WARNINGS
#include <string.h>

#include "base.h"
#include "ecs/ecs.h"
#include "ecs/ecs.cpp"
#include "sim/sim.h"
#include "sim/sim.cpp"
#include "engine/engine.h"
#include "engine/engine.cpp"

#include "test_framework.h"
#include "test_files.h"
#include "engine_tests.cpp"
#include "load_save_tests.cpp"
#include "scenario_fixture_tests.cpp"
#include "name_index_tests.cpp"
#include "display_name_tests.cpp"
#include "turn_tests.cpp"
#include "develop_tests.cpp"
#include "economy_tests.cpp"
#include "city_command_tests.cpp"
#include "growth_tests.cpp"
#include "march_tests.cpp"
#include "combat_tests.cpp"
#include "court_tests.cpp"
#include "rivals_tests.cpp"
#include "playable_tests.cpp"
#include "map_tests.cpp"
#include "world_map_tests.cpp"   /* a map from real geography (docs/MAPS.md) */
#include "save_map_tests.cpp"
#include "replay_tests.cpp"
#include "ai_tests.cpp"
#include "order_tests.cpp"     /* the long orders of a unit (clone gap report 2) */
#include "people_tests.cpp"    /* the journeys of officers and the plots on the road (clone gap report 2) */
#include "content_source_tests.cpp"
#include "report_tests.cpp"     /* M18 */
#include "save_compat_tests.cpp" /* the saves of older builds (the play report of 2026-10-23) */
#include "colour_tests.cpp"      /* the flag colours of the factions */
#include "game_replay_tests.cpp" /* the replay of a game, step by step */
#include "multiplayer_tests.cpp"  /* a shared game: two engines, one stream of steps */
#include "fixture_tests.cpp"

int
main(int argc, char **argv)
{
    setvbuf(stdout, 0, _IONBF, 0);
    int failed = 0;

    test_counts engine_counts = {};
    feature_engine_tests(&engine_counts);
    failed |= test_report("feature/engine", &engine_counts);

    test_counts load_counts = {};
    feature_load_save_tests(&load_counts);
    failed |= test_report("feature/load_save", &load_counts);

    test_counts scenario_counts = {};
    feature_scenario_fixture_tests(&scenario_counts);
    failed |= test_report("feature/scenario_fixtures", &scenario_counts);

    test_counts name_counts = {};
    feature_name_index_tests(&name_counts);
    failed |= test_report("feature/name_index", &name_counts);

    test_counts display_counts = {};
    feature_display_name_tests(&display_counts);
    failed |= test_report("feature/display_names", &display_counts);

    test_counts turn_counts = {};
    feature_turn_tests(&turn_counts);
    failed |= test_report("feature/turns", &turn_counts);

    test_counts develop_counts = {};
    feature_develop_tests(&develop_counts);
    failed |= test_report("feature/develop", &develop_counts);

    test_counts economy_counts = {};
    feature_economy_tests(&economy_counts);
    failed |= test_report("feature/economy", &economy_counts);

    test_counts command_counts = {};
    feature_city_command_tests(&command_counts);
    failed |= test_report("feature/city_commands", &command_counts);

    test_counts growth_counts = {};
    feature_growth_tests(&growth_counts);
    failed |= test_report("feature/growth", &growth_counts);

    test_counts march_counts = {};
    feature_march_tests(&march_counts);
    failed |= test_report("feature/march", &march_counts);

    test_counts combat_counts = {};
    feature_combat_tests(&combat_counts);
    failed |= test_report("feature/combat", &combat_counts);

    test_counts court_counts = {};
    feature_court_tests(&court_counts);
    failed |= test_report("feature/court", &court_counts);
    test_counts rivals_counts = {};
    feature_rivals_tests(&rivals_counts);
    failed |= test_report("feature/rivals", &rivals_counts);
    test_counts playable_counts = {};
    feature_playable_tests(&playable_counts);
    failed |= test_report("feature/playable", &playable_counts);
    test_counts map_counts = {};
    feature_map_tests(&map_counts);
    failed |= test_report("feature/map", &map_counts);
    test_counts world_map_counts = {};
    feature_world_map_tests(&world_map_counts);
    feature_pack_scenario_tests(&world_map_counts);
    feature_pack_save_tests(&world_map_counts);
    feature_pack_map_tests_all(&world_map_counts);
    feature_pack_story_tests(&world_map_counts);
    failed |= test_report("feature/world_map", &world_map_counts);
    test_counts save_map_counts = {};
    feature_save_map_tests(&save_map_counts);
    failed |= test_report("feature/save_map", &save_map_counts);
    test_counts replay_counts = {};
    feature_replay_tests(&replay_counts);
    failed |= test_report("feature/replay", &replay_counts);
    test_counts ai_counts = {};
    feature_ai_tests(&ai_counts);
    failed |= test_report("feature/ai", &ai_counts);
    test_counts order_counts = {};
    feature_order_tests(&order_counts);
    failed |= test_report("feature/orders", &order_counts);
    test_counts people_counts = {};
    feature_people_tests(&people_counts);
    failed |= test_report("feature/people", &people_counts);
    test_counts content_counts = {};
    feature_content_source_tests(&content_counts);
    failed |= test_report("feature/content_source", &content_counts);

    test_counts report_counts = {};
    feature_report_tests(&report_counts);
    failed |= test_report("feature/report", &report_counts);

    test_counts compat_counts = {};
    feature_save_compat_tests(&compat_counts);
    failed |= test_report("feature/save_compat", &compat_counts);

    test_counts colour_counts = {};
    feature_colour_tests(&colour_counts);
    failed |= test_report("feature/colours", &colour_counts);

    test_counts game_replay_counts = {};
    feature_game_replay_tests(&game_replay_counts);
    failed |= test_report("feature/game_replay", &game_replay_counts);

    test_counts multiplayer_counts = {};
    feature_multiplayer_tests(&multiplayer_counts);
    failed |= test_report("feature/multiplayer", &multiplayer_counts);

    char fixtures_dir[MAX_PATH];
    if (argc > 1)
    {
        snprintf(fixtures_dir, sizeof(fixtures_dir), "%s", argv[1]);
    }
    else
    {
        char exe_dir[MAX_PATH];
        test_exe_dir(exe_dir, MAX_PATH);
        snprintf(fixtures_dir, sizeof(fixtures_dir), "%s/../spec-src/spec/fixtures", exe_dir);
    }

    memory_arena arena = test_arena_create(Megabytes(64));
    test_counts fixture_counts = {};
    u32 files = feature_fixture_tests(fixtures_dir, "", false, &arena, &fixture_counts);
    char map_dir[MAX_PATH];
    snprintf(map_dir, sizeof(map_dir), "%s/map", fixtures_dir);
    arena_reset(&arena);
    files += feature_fixture_tests(map_dir, "map/", true, &arena, &fixture_counts);   /* M10: the map fixtures */
    printf("%s %u/%u feature/fixtures (%u files)\n", fixture_counts.failed ? "FAIL" : "PASS",
           fixture_counts.passed, fixture_counts.passed + fixture_counts.failed, files);
    failed |= fixture_counts.failed ? 1 : 0;
    test_arena_destroy(&arena);

    printf(failed ? "RESULT FAIL\n" : "RESULT PASS\n");
    return failed;
}
