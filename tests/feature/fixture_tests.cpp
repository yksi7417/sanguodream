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
    const char         *mechanic;    /* fixture file name without .tsv */
    fixture_handler_fn *run;
    const char         *blocked_by;  /* not zero: the spec cannot give the rule yet; the rows are not run */
};

/* One entry per mechanic in spec-src/spec/mechanics/. Add the handler in the
   RED step, before implementing the mechanic in code/sim/ and code/engine/.

   A blocked entry names the open question that stops the handler. Its rows
   count as neither a pass nor a failure, and the runner prints the reason
   on each run. Remove the entry when the question closes. */
global_variable fixture_handler g_fixture_handlers[] = {
    /* military-units-and-tactics.md: "The formula from abilities to unit
       values is unknown", and Q-030 must map the scenario numbers of the
       guide book to the scenario files. F-081, milestone M3. */
    { "unit-values-guidebook", 0, "Q-030 and the unknown unit-value formula (F-081, M3)" },
    { 0, 0, 0 }   /* sentinel */
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

        if (handler->blocked_by)
        {
            printf("BLOCKED %u rows fixture/%s (%s)\n", table.row_count, mechanic, handler->blocked_by);
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
