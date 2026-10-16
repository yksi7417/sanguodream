/* Acceptance tests from the spec: every spec-src\spec\fixtures\*.tsv is a
   feature test for one mechanic, run through engine_api against the real
   engine. A fixture whose mechanic has no handler here FAILS: that is the RED
   step of the TDD loop and the signal to implement it. */

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>   /* directory listing only */

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

    u32 line_count = 1;
    for (char *p = text; *p; ++p) if (*p == '\n') line_count++;

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
global_variable fixture_handler g_fixture_handlers[] = {
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
