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
#include "save_map_tests.cpp"
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
    test_counts save_map_counts = {};
    feature_save_map_tests(&save_map_counts);
    failed |= test_report("feature/save_map", &save_map_counts);

    char fixtures_dir[MAX_PATH];
    if (argc > 1)
    {
        snprintf(fixtures_dir, sizeof(fixtures_dir), "%s", argv[1]);
    }
    else
    {
        char exe_dir[MAX_PATH];
        DWORD length = GetModuleFileNameA(0, exe_dir, MAX_PATH);
        for (DWORD i = length; i > 0; --i)
        {
            if (exe_dir[i - 1] == '\\') { exe_dir[i - 1] = 0; break; }
        }
        snprintf(fixtures_dir, sizeof(fixtures_dir), "%s\\..\\spec-src\\spec\\fixtures", exe_dir);
    }

    memory_arena arena = test_arena_create(Megabytes(64));
    test_counts fixture_counts = {};
    u32 files = feature_fixture_tests(fixtures_dir, "", false, &arena, &fixture_counts);
    char map_dir[MAX_PATH];
    snprintf(map_dir, sizeof(map_dir), "%s\\map", fixtures_dir);
    arena_reset(&arena);
    files += feature_fixture_tests(map_dir, "map/", true, &arena, &fixture_counts);   /* M10: the map fixtures */
    printf("%s %u/%u feature/fixtures (%u files)\n", fixture_counts.failed ? "FAIL" : "PASS",
           fixture_counts.passed, fixture_counts.passed + fixture_counts.failed, files);
    failed |= fixture_counts.failed ? 1 : 0;
    test_arena_destroy(&arena);

    printf(failed ? "RESULT FAIL\n" : "RESULT PASS\n");
    return failed;
}
