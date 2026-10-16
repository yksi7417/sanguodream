#ifndef TABLES_H
#define TABLES_H
/* The TSV table parser that the content packs and the spec fixtures share
   (ROADMAP 2.1: every table is a flat TSV with a header row). It parses a
   text buffer that the caller read. It does no I/O, so the sim core can use
   it.

   Format: the first line that is not empty and not a comment is the
   header. Each other line is one row. Columns are separated by one tab.
   A line that starts with '#' is a comment. An empty line is ignored. A
   "\r" at the end of a line is removed. */

#include "base.h"

#define TSV_MAX_COLUMNS 64

struct tsv_table
{
    u32    column_count;
    char  *columns[TSV_MAX_COLUMNS];
    u32    row_count;
    char **cells;   /* row * column_count + column; a missing cell is "" */
};

/* Cuts a line at each tab, in place. Returns the number of fields. */
internal u32
tsv_split_tabs(char *line, char **fields, u32 max_fields)
{
    u32 count = 0;
    char *start = line;
    for (char *cursor = line;; ++cursor)
    {
        if (*cursor == '\t' || *cursor == 0)
        {
            b32 end = (*cursor == 0);
            *cursor = 0;
            if (count < max_fields) fields[count++] = start;
            if (end) break;
            start = cursor + 1;
        }
    }
    return count;
}

internal b32
tsv_line_is_data(const char *line)
{
    return line[0] != 0 && line[0] != '#';
}

/* Parses text (NUL terminated) in place. The cells point into text, so
   text must live as long as the table. Returns false if the text has no
   header line. */
internal b32
tsv_parse(char *text, memory_arena *arena, tsv_table *table)
{
    *table = {};
    u32 line_capacity = 1;
    for (char *cursor = text; *cursor; ++cursor)
    {
        if (*cursor == '\n') line_capacity++;
    }

    char **lines = PushArray(arena, line_capacity, char *);
    u32 used = 0;
    char *cursor = text;
    while (*cursor)
    {
        char *line = cursor;
        while (*cursor && *cursor != '\n') cursor++;
        if (*cursor == '\n') *cursor++ = 0;
        u64 length = strlen(line);
        if (length && line[length - 1] == '\r') line[length - 1] = 0;
        if (tsv_line_is_data(line)) lines[used++] = line;
    }
    if (used == 0) return false;

    table->column_count = tsv_split_tabs(lines[0], table->columns, TSV_MAX_COLUMNS);
    table->row_count    = used - 1;
    table->cells        = PushArray(arena, (u64)table->row_count * table->column_count, char *);
    for (u32 row = 0; row < table->row_count; ++row)
    {
        char *fields[TSV_MAX_COLUMNS];
        u32 field_count = tsv_split_tabs(lines[row + 1], fields, TSV_MAX_COLUMNS);
        for (u32 column = 0; column < table->column_count; ++column)
        {
            table->cells[row * table->column_count + column] = column < field_count ? fields[column] : (char *)"";
        }
    }
    return true;
}

/* The index of a column by its header name, or -1. */
internal s32
tsv_column(tsv_table *table, const char *name)
{
    for (u32 column = 0; column < table->column_count; ++column)
    {
        if (strcmp(table->columns[column], name) == 0) return (s32)column;
    }
    return -1;
}

/* The cell of a row in a named column, or "" if the column is missing. */
internal const char *
tsv_cell(tsv_table *table, u32 row, const char *column_name)
{
    s32 column = tsv_column(table, column_name);
    return column < 0 || row >= table->row_count ? "" : table->cells[row * table->column_count + (u32)column];
}

/* A decimal number without a sign. Returns false for an empty cell, a
   character that is not a digit, or a value above U32_MAX. */
internal b32
tsv_parse_u32(const char *cell, u32 *value)
{
    if (!cell[0]) return false;
    u64 result = 0;
    for (const char *cursor = cell; *cursor; ++cursor)
    {
        if (*cursor < '0' || *cursor > '9') return false;
        result = result * 10 + (u64)(*cursor - '0');
        if (result > U32_MAX) return false;
    }
    *value = (u32)result;
    return true;
}

internal b32
tsv_cell_u32(tsv_table *table, u32 row, const char *column_name, u32 *value)
{
    return tsv_parse_u32(tsv_cell(table, row, column_name), value);
}

/* True if the cell is "yes". */
internal b32
tsv_cell_yes(tsv_table *table, u32 row, const char *column_name)
{
    return strcmp(tsv_cell(table, row, column_name), "yes") == 0;
}

#endif /* TABLES_H */
