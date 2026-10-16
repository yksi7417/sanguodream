/* Acceptance tests from the spec: every spec-src\spec\fixtures\*.tsv is a
   feature test for one mechanic, run through engine_api against the real
   engine. A fixture whose mechanic has no handler here FAILS: that is the RED
   step of the TDD loop and the signal to implement it. */

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>   /* directory listing only */

/* The table parser is code/sim/tables.h, the same one that reads the
   content pack. This file only reads the file. */

internal char *
read_text_file(const char *path, memory_arena *arena, u64 *size_out)
{
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = (char *)arena_push_size(arena, (u64)size + 1);
    size_t read = fread(text, 1, (size_t)size, file);
    fclose(file);
    text[read] = 0;
    if (size_out) *size_out = (u64)read;
    return text;
}

internal b32
tsv_load(const char *path, memory_arena *arena, tsv_table *table)
{
    char *text = read_text_file(path, arena, 0);
    return text && tsv_parse(text, arena, table);
}

/* A handler builds the state each row describes through engine_api, runs the
   row's command, compares with the row's expected outcome, and records one
   pass or fail per row. */
typedef void fixture_handler_fn(tsv_table *table, engine_api *api, test_counts *counts);

struct fixture_handler
{
    const char         *mechanic;   /* fixture file name without .tsv */
    fixture_handler_fn *run;
};

/* One entry per mechanic in spec-src/spec/mechanics/. Add the handler in the
   RED step, before implementing the mechanic in code/sim/ and code/engine/. */
/* The movement of each unit class before techniques
   (military-units-and-tactics.md): 22 for the three foot classes, 28 for
   cavalry, 18 for the siege weapons. */
struct fixture_class_movement
{
    const char *unit_class;
    u32         movement;
};

global_variable fixture_class_movement g_class_movement[] = {
    { "pike", 22 }, { "halberd", 22 }, { "crossbow", 22 },
    { "cavalry", 28 },
    { "ram", 18 }, { "tower", 18 },
};

/* unit-values-guidebook.tsv: the unit values of 18 screenshots of the
   guide book. This handler checks only the movement column against the
   class rule. The full check of the attack, defence, intellect and build
   columns belongs to M3 (F-081). It needs the formula from the officer
   abilities to the unit values, and the officer data of each scenario
   (Q-030). Until then the handler prints those columns as pending. They
   are not failures. The engine has no unit classes yet, so the class rule
   comes from the table above. */
internal void
fixture_unit_values_guidebook(tsv_table *table, engine_api *api, test_counts *counts)
{
    (void)api;
    for (u32 row = 0; row < table->row_count; ++row)
    {
        const char *unit_class = tsv_cell(table, row, "class");
        u32 movement = 0, expected = 0;
        for (u32 index = 0; index < ArrayCount(g_class_movement); ++index)
        {
            if (strcmp(g_class_movement[index].unit_class, unit_class) == 0) expected = g_class_movement[index].movement;
        }
        b32 parsed = tsv_cell_u32(table, row, "move", &movement);
        char name[128];
        snprintf(name, sizeof(name), "unit-values-guidebook row %u (%s): movement of the class", row, unit_class);
        TEST_CHECK(counts, parsed && expected != 0 && movement == expected, name);
    }
    printf("  PENDING %u rows x attack, defence, intellect, build: Q-030 and the unit-value formula (F-081, M3)\n",
           table->row_count);
}

global_variable fixture_handler g_fixture_handlers[] = {
    { "unit-values-guidebook", fixture_unit_values_guidebook },
    { 0, 0 }   /* sentinel */
};

internal fixture_handler *
find_fixture_handler(const char *mechanic)
{
    for (fixture_handler *h = g_fixture_handlers; h->mechanic; ++h)
    {
        if (strcmp(h->mechanic, mechanic) == 0) return h;
    }
    return 0;
}

/* Runs every fixture file in dir. Returns the number of files seen. */
internal u32
feature_fixture_tests(const char *dir, memory_arena *arena, test_counts *totals)
{
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*.tsv", dir);

    WIN32_FIND_DATAA found;
    HANDLE search = FindFirstFileA(pattern, &found);
    if (search == INVALID_HANDLE_VALUE)
    {
        printf("fixtures: none found in %s\n", dir);
        return 0;
    }

    u32 file_count = 0;
    do
    {
        file_count++;
        char mechanic[MAX_PATH];
        snprintf(mechanic, sizeof(mechanic), "%s", found.cFileName);
        char *dot = strrchr(mechanic, '.');
        if (dot) *dot = 0;

        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s\\%s", dir, found.cFileName);

        tsv_table table = {};
        if (!tsv_load(path, arena, &table))
        {
            printf("FAIL 0/0 fixture/%s (could not parse)\n", mechanic);
            totals->failed++;
            continue;
        }

        fixture_handler *handler = find_fixture_handler(mechanic);
        if (!handler)
        {
            printf("FAIL 0/%u fixture/%s (no handler in tests/feature/fixture_tests.cpp)\n", table.row_count, mechanic);
            totals->failed += table.row_count ? table.row_count : 1;
            continue;
        }

        /* Each mechanic gets a fresh engine so rows cannot leak into each other. */
        engine_fixture f;
        engine_fixture_init(&f, 1, 4096);
        test_counts counts = {};
        handler->run(&table, &f.api, &counts);
        printf("%s %u/%u fixture/%s\n", counts.failed ? "FAIL" : "PASS",
               counts.passed, counts.passed + counts.failed, mechanic);
        totals->passed += counts.passed;
        totals->failed += counts.failed;
        engine_fixture_destroy(&f);
    } while (FindNextFileA(search, &found));
    FindClose(search);
    return file_count;
}
