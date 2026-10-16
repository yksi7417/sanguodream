/* sim_tests.exe: headless acceptance tests for the simulation core.

   1. Self-tests of the arena, ECS and sim primitives.
   2. Every spec-src\spec\fixtures\*.tsv is an acceptance test for one
      mechanic. A fixture whose mechanic has no handler here FAILS: that is
      the RED step of the TDD loop and the signal to implement it.

   Exit code: 0 when everything passes, 1 otherwise.
   Usage: sim_tests.exe [fixtures-dir]   (default: <exe dir>\..\spec-src\spec\fixtures) */

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>   /* directory listing only; the sim itself has no Windows dependency */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base.h"
#include "ecs/ecs.h"
#include "ecs/ecs.cpp"
#include "sim/sim.h"
#include "sim/sim.cpp"

struct test_counts
{
    u32 passed;
    u32 failed;
};

global_variable memory_arena g_arena;

#define TEST_CHECK(counts, condition, name)                                      \
    do {                                                                         \
        if (condition) { (counts)->passed++; }                                   \
        else { (counts)->failed++; printf("  FAIL %s (line %d)\n", name, __LINE__); } \
    } while (0)

/* ---- self-tests --------------------------------------------------------- */

internal void
selftest_arena(test_counts *counts)
{
    u8 buffer[512];
    memory_arena arena;
    arena_init(&arena, buffer, sizeof(buffer));
    void *first  = arena_push_size(&arena, 10);
    void *second = arena_push_size(&arena, 10);
    TEST_CHECK(counts, first != 0 && second != 0, "arena pushes");
    TEST_CHECK(counts, ((u64)second & 15) == 0, "arena aligns to 16");
    TEST_CHECK(counts, (u8 *)second >= (u8 *)first + 10, "arena does not overlap");
    u32 *zeroed = PushArray(&arena, 4, u32);
    TEST_CHECK(counts, zeroed[0] == 0 && zeroed[3] == 0, "arena zeroes memory");
    arena_reset(&arena);
    TEST_CHECK(counts, arena.used == 0, "arena reset");
}

struct test_position
{
    s32 q;
    s32 r;
};

internal void
selftest_ecs(test_counts *counts)
{
    entity_manager em;
    em_init(&em, &g_arena, 64);
    component_table<test_position> positions;
    table_init(&positions, &g_arena, 64, 8);

    entity_id a = em_create(&em);
    entity_id b = em_create(&em);
    entity_id c = em_create(&em);
    TEST_CHECK(counts, a != ENTITY_NULL && a != b && b != c, "entities are distinct");
    TEST_CHECK(counts, em.alive_count == 3, "alive count");

    test_position pa = { 1, 2 }, pb = { 3, 4 }, pc = { 5, 6 };
    table_add(&positions, a, pa);
    table_add(&positions, b, pb);
    table_add(&positions, c, pc);
    TEST_CHECK(counts, positions.count == 3, "table count after add");
    TEST_CHECK(counts, table_get(&positions, b) && table_get(&positions, b)->r == 4, "table_get");

    test_position pa2 = { 7, 8 };
    table_add(&positions, a, pa2);
    TEST_CHECK(counts, positions.count == 3 && table_get(&positions, a)->q == 7, "add overwrites");

    TEST_CHECK(counts, table_remove(&positions, a), "remove returns true");
    TEST_CHECK(counts, positions.count == 2, "count after remove");
    TEST_CHECK(counts, table_get(&positions, a) == 0, "removed entity has no component");
    TEST_CHECK(counts, table_get(&positions, c)->q == 5 && table_get(&positions, b)->q == 3, "swap-with-last keeps others");
    TEST_CHECK(counts, !table_remove(&positions, a), "second remove returns false");

    em_destroy(&em, b);
    TEST_CHECK(counts, !em_alive(&em, b), "destroyed entity is dead");
    TEST_CHECK(counts, table_get(&positions, b) != 0, "orphaned component stays until its owner removes it");
    entity_id d = em_create(&em);
    TEST_CHECK(counts, entity_index(d) == entity_index(b) && d != b, "slot reused with new generation");
    TEST_CHECK(counts, table_get(&positions, d) == 0, "new entity in the slot does not see the orphan");
    TEST_CHECK(counts, table_remove(&positions, b), "orphan removable by the old id");
    TEST_CHECK(counts, positions.count == 1 && table_get(&positions, c)->q == 5, "only c remains");

    u32 iterated = 0;
    TableEach(&positions, i) { iterated++; }
    TEST_CHECK(counts, iterated == positions.count, "TableEach visits dense range");

    u32 added = 0;
    for (u32 i = 0; i < 16; ++i)
    {
        entity_id e = em_create(&em);
        test_position p = { (s32)i, 0 };
        if (table_add(&positions, e, p)) added++;
    }
    TEST_CHECK(counts, positions.count == positions.capacity, "table fills to capacity");
    TEST_CHECK(counts, added == positions.capacity - 1, "table_add returns null when full");
}

internal void
selftest_sim(test_counts *counts)
{
    sim_state *first  = PushStruct(&g_arena, sim_state);
    sim_state *second = PushStruct(&g_arena, sim_state);
    sim_state *third  = PushStruct(&g_arena, sim_state);
    sim_init(first,  &g_arena, 42);
    sim_init(second, &g_arena, 42);
    sim_init(third,  &g_arena, 43);

    b32 same = true;
    b32 differs = false;
    for (u32 i = 0; i < 1000; ++i)
    {
        u32 x = sim_random_u32(first);
        u32 y = sim_random_u32(second);
        u32 z = sim_random_u32(third);
        if (x != y) same = false;
        if (x != z) differs = true;
    }
    TEST_CHECK(counts, same, "prng deterministic for equal seeds");
    TEST_CHECK(counts, differs, "prng differs for different seeds");

    b32 in_range = true;
    u32 histogram[7] = {};
    for (u32 i = 0; i < 7000; ++i)
    {
        u32 v = sim_random_below(first, 7);
        if (v >= 7) in_range = false; else histogram[v]++;
    }
    b32 spread = true;
    for (u32 i = 0; i < 7; ++i) if (histogram[i] < 700 || histogram[i] > 1300) spread = false;
    TEST_CHECK(counts, in_range, "random_below stays in range");
    TEST_CHECK(counts, spread, "random_below is roughly uniform");

    sim_event_sink sink = sim_sink_from_arena(&g_arena, 16);
    sim_command cmd = sim_make_command(first, SIM_COMMAND_END_TURN);
    TEST_CHECK(counts, cmd.id == 1, "first command id is 1");
    sim_apply(first, cmd, &sink);
    TEST_CHECK(counts, first->turn == 1, "end_turn advances turn");
    TEST_CHECK(counts, sink.count == 1 && sink.events[0].kind == SIM_EVENT_TURN_ADVANCED && sink.events[0].turn == 1,
               "end_turn emits turn_advanced");
}

/* ---- fixtures ----------------------------------------------------------- */

#define TSV_MAX_COLUMNS 64

struct tsv_table
{
    u32    column_count;
    char  *columns[TSV_MAX_COLUMNS];
    u32    row_count;
    char **cells;   /* row * column_count + column */
};

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

internal u32
split_tabs(char *line, char **out, u32 max)
{
    u32 count = 0;
    char *start = line;
    for (char *p = line;; ++p)
    {
        if (*p == '\t' || *p == 0)
        {
            b32 end = (*p == 0);
            *p = 0;
            if (count < max) out[count++] = start;
            if (end) break;
            start = p + 1;
        }
    }
    return count;
}

internal b32
tsv_load(const char *path, memory_arena *arena, tsv_table *table)
{
    u64 size = 0;
    char *text = read_text_file(path, arena, &size);
    if (!text) return false;

    /* Count lines first so the cell array can be sized. */
    u32 line_count = 0;
    for (char *p = text; *p; ++p) if (*p == '\n') line_count++;
    line_count++;

    char **lines = PushArray(arena, line_count, char *);
    u32 used = 0;
    char *cursor = text;
    while (*cursor)
    {
        char *line = cursor;
        char *newline = strchr(cursor, '\n');
        if (newline) { *newline = 0; cursor = newline + 1; }
        else cursor += strlen(cursor);
        size_t len = strlen(line);
        if (len && line[len - 1] == '\r') line[len - 1] = 0;
        if (line[0]) lines[used++] = line;
    }
    if (used == 0) return false;

    table->column_count = split_tabs(lines[0], table->columns, TSV_MAX_COLUMNS);
    table->row_count    = used - 1;
    table->cells        = PushArray(arena, (u64)table->row_count * table->column_count, char *);
    for (u32 row = 0; row < table->row_count; ++row)
    {
        char *fields[TSV_MAX_COLUMNS];
        u32 n = split_tabs(lines[row + 1], fields, TSV_MAX_COLUMNS);
        for (u32 col = 0; col < table->column_count; ++col)
        {
            table->cells[row * table->column_count + col] = col < n ? fields[col] : (char *)"";
        }
    }
    return true;
}

internal s32
tsv_column(tsv_table *table, const char *name)
{
    for (u32 i = 0; i < table->column_count; ++i)
    {
        if (strcmp(table->columns[i], name) == 0) return (s32)i;
    }
    return -1;
}

internal const char *
tsv_cell(tsv_table *table, u32 row, const char *column)
{
    s32 col = tsv_column(table, column);
    return col < 0 ? "" : table->cells[row * table->column_count + (u32)col];
}

/* A handler runs every row of one mechanic's fixture file through the sim
   and compares. It records one pass or fail per row. */
typedef void fixture_handler_fn(tsv_table *table, test_counts *counts);

struct fixture_handler
{
    const char         *mechanic;   /* fixture file name without .tsv */
    fixture_handler_fn *run;
};

/* One entry per mechanic in spec-src/spec/mechanics/. Add the handler in the
   RED step, before implementing the mechanic in code/sim/. */
global_variable fixture_handler g_handlers[] = {
    { 0, 0 }   /* sentinel */
};

internal fixture_handler *
find_handler(const char *mechanic)
{
    for (fixture_handler *h = g_handlers; h->mechanic; ++h)
    {
        if (strcmp(h->mechanic, mechanic) == 0) return h;
    }
    return 0;
}

internal u32
run_fixtures(const char *dir, test_counts *totals)
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
        if (!tsv_load(path, &g_arena, &table))
        {
            printf("FAIL 0/0 %s (could not parse)\n", mechanic);
            totals->failed++;
            continue;
        }

        fixture_handler *handler = find_handler(mechanic);
        if (!handler)
        {
            printf("FAIL 0/%u %s (no handler in tests/sim_tests.cpp)\n", table.row_count, mechanic);
            totals->failed += table.row_count ? table.row_count : 1;
            continue;
        }

        test_counts counts = {};
        handler->run(&table, &counts);
        printf("%s %u/%u %s\n", counts.failed ? "FAIL" : "PASS",
               counts.passed, counts.passed + counts.failed, mechanic);
        totals->passed += counts.passed;
        totals->failed += counts.failed;
    } while (FindNextFileA(search, &found));
    FindClose(search);
    return file_count;
}

/* ---- main --------------------------------------------------------------- */

int
main(int argc, char **argv)
{
    setvbuf(stdout, 0, _IONBF, 0);   /* keep output if an Assert breaks the process */
    u64 arena_size = Megabytes(64);
    void *block = malloc((size_t)arena_size);
    if (!block) { printf("out of memory\n"); return 1; }
    arena_init(&g_arena, block, arena_size);

    test_counts self = {};
    selftest_arena(&self);
    selftest_ecs(&self);
    selftest_sim(&self);
    printf("%s %u/%u selftest\n", self.failed ? "FAIL" : "PASS", self.passed, self.passed + self.failed);

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

    test_counts fixtures = {};
    u32 files = run_fixtures(fixtures_dir, &fixtures);
    printf("%s %u/%u fixtures (%u files)\n", fixtures.failed ? "FAIL" : "PASS",
           fixtures.passed, fixtures.passed + fixtures.failed, files);

    u32 failed = self.failed + fixtures.failed;
    printf(failed ? "RESULT FAIL\n" : "RESULT PASS\n");
    free(block);
    return failed ? 1 : 0;
}
