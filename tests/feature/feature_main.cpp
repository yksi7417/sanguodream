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
    u32 files = feature_fixture_tests(fixtures_dir, &arena, &fixture_counts);
    printf("%s %u/%u feature/fixtures (%u files)\n", fixture_counts.failed ? "FAIL" : "PASS",
           fixture_counts.passed, fixture_counts.passed + fixture_counts.failed, files);
    failed |= fixture_counts.failed ? 1 : 0;
    test_arena_destroy(&arena);

    printf(failed ? "RESULT FAIL\n" : "RESULT PASS\n");
    return failed;
}
