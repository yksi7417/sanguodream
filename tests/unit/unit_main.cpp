/* unit_tests.exe: the base of the pyramid. Exercises functions and structs of
   each module in isolation: base.h, ecs/, sim/. No engine, no UI. The one file read is
   the sample save in inflate_tests.cpp. */

#define _CRT_SECURE_NO_WARNINGS
#include <string.h>

#include "base.h"
#include "ecs/ecs.h"
#include "ecs/ecs.cpp"
#include "sim/sim.h"
#include "sim/sim.cpp"

#include "test_framework.h"
#include "test_files.h"
#include "base_tests.cpp"
#include "ecs_tests.cpp"
#include "hex_tests.cpp"
#include "sim_tests.cpp"
#include "inflate_tests.cpp"
#include "save_format_tests.cpp"

int
main(void)
{
    setvbuf(stdout, 0, _IONBF, 0);
    int failed = 0;

    test_counts base = {};
    unit_base_tests(&base);
    failed |= test_report("unit/base", &base);

    test_counts ecs = {};
    unit_ecs_tests(&ecs);
    failed |= test_report("unit/ecs", &ecs);

    test_counts hex = {};
    unit_hex_tests(&hex);
    failed |= test_report("unit/hex", &hex);

    test_counts sim = {};
    unit_sim_tests(&sim);
    failed |= test_report("unit/sim", &sim);

    test_counts inflate = {};
    unit_inflate_tests(&inflate);
    failed |= test_report("unit/inflate", &inflate);

    test_counts save_format = {};
    unit_save_format_tests(&save_format);
    failed |= test_report("unit/save_format", &save_format);

    printf(failed ? "RESULT FAIL\n" : "RESULT PASS\n");
    return failed;
}
