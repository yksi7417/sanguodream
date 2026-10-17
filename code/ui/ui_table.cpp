/* Table views: the filter box and the sort of the big tables (Officers,
   Cities, Event ledger). A view keeps the display order of one table: the
   source indices that pass the filter, in the order of the sorted column.
   The panel draws each row from a fresh query, so the cells stay live. The
   view builds its order again when the filter, the sort, the raw toggle,
   the locale, the force filter, the row count, the turn or the ledger
   changes. Included by ui.cpp before the panels. */

#include <stdlib.h>   /* qsort */

/* One cell as the view sees it: the text for the filter, and a number for
   the sort when the column is numeric. */
internal void
ui_key_number(ui_sort_key *key, s64 number)
{
    key->numeric = true;
    key->number  = number;
    snprintf(key->text, sizeof(key->text), "%lld", (long long)number);
}

internal void
ui_key_text(ui_sort_key *key, const char *text)
{
    key->numeric = false;
    key->number  = 0;
    snprintf(key->text, sizeof(key->text), "%s", text);
}

/* Sorts by the number, filters by the text: "3 / 5", "180-234". */
internal void
ui_key_mixed(ui_sort_key *key, s64 number, const char *text)
{
    key->numeric = true;
    key->number  = number;
    snprintf(key->text, sizeof(key->text), "%s", text);
}

/* A name or a label as ui_code_cell shows it: the raw number when the
   panels show raw numbers. */
internal void
ui_key_code(ui_state *ui, engine_api *api, ui_sort_key *key, ui_code_kind kind, u32 value, u32 none)
{
    if (ui->show_raw)
    {
        if (value == none) ui_key_mixed(key, -1, "-");
        else               ui_key_number(key, value);
        return;
    }
    char text[UI_NAME_SIZE * 2];
    ui_code_text(ui, api, kind, value, text, sizeof(text));
    ui_key_text(key, text);
}

/* True if needle is in hay. ASCII letters match in both cases; the other
   bytes (the UTF-8 of a name) must match exactly. */
internal b32
ui_text_contains(const char *hay, const char *needle)
{
    if (!needle[0]) return true;
    for (const char *start = hay; *start; ++start)
    {
        const char *h = start, *n = needle;
        while (*h && *n)
        {
            char a = *h, b = *n;
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) break;
            ++h; ++n;
        }
        if (!*n) return true;
    }
    return false;
}

/* qsort has no context argument, so the keys of the sort go through these. */
global_variable ui_sort_key *g_ui_sort_keys;
global_variable b32          g_ui_sort_descending;

internal int
ui_sort_compare(const void *left, const void *right)
{
    u32 a = *(const u32 *)left, b = *(const u32 *)right;
    ui_sort_key *ka = g_ui_sort_keys + a, *kb = g_ui_sort_keys + b;
    int order;
    if (ka->numeric && kb->numeric) order = ka->number < kb->number ? -1 : (ka->number > kb->number ? 1 : 0);
    else                            order = strcmp(ka->text, kb->text);
    if (g_ui_sort_descending) order = -order;
    if (order == 0) order = a < b ? -1 : (a > b ? 1 : 0);   /* a stable order */
    return order;
}

internal b32
ui_table_view_stale(ui_state *ui, engine_api *api, ui_table_view *view, ui_table_source *source)
{
    if (!view->built) return true;
    if (view->built_source_count != source->source_count) return true;
    if (view->built_force != source->only_force) return true;
    if (view->built_raw != ui->show_raw) return true;
    if (view->built_sort_column != view->sort_column || view->built_sort_descending != view->sort_descending) return true;
    if (strcmp(view->built_filter, view->filter) != 0) return true;
    if (ui->text && strcmp(view->built_locale, ui->text->locale) != 0) return true;
    if (view->built_turn != api->turn(api->engine)) return true;
    if (view->built_ledger_count != api->ledger_count(api->engine)) return true;
    return false;
}

/* Builds the display order: the rows that pass the force filter and the
   text filter, sorted by the column of the view. */
internal void
ui_table_view_build(ui_state *ui, engine_api *api, ui_table_view *view, ui_table_source *source)
{
    u32 columns = source->column_count < UI_TABLE_MAX_COLUMNS ? source->column_count : UI_TABLE_MAX_COLUMNS;
    b32 sorted = view->sort_column >= 0 && (u32)view->sort_column < columns;
    b32 filtered = view->filter[0] != 0;
    u32 count = source->source_count < source->capacity ? source->source_count : source->capacity;
    view->count = 0;
    for (u32 index = 0; index < count; ++index)
    {
        ui_sort_key cells[UI_TABLE_MAX_COLUMNS];
        b32 needs_cells = filtered || sorted || source->only_force != ENGINE_NO_FACTION;
        u8 force = ENGINE_NO_FACTION;
        if (needs_cells) force = source->row(ui, api, source->user, index, cells);
        if (source->only_force != ENGINE_NO_FACTION && force != source->only_force) continue;
        if (filtered)
        {
            b32 match = false;
            for (u32 column = 0; column < columns && !match; ++column) match = ui_text_contains(cells[column].text, view->filter);
            if (!match) continue;
        }
        if (sorted) source->keys[index] = cells[view->sort_column];
        source->rows[view->count++] = index;
    }
    if (sorted && view->count > 1)
    {
        g_ui_sort_keys       = source->keys;
        g_ui_sort_descending = view->sort_descending;
        qsort(source->rows, view->count, sizeof(u32), ui_sort_compare);
    }
    view->built                 = true;
    view->built_source_count    = source->source_count;
    view->built_force           = source->only_force;
    view->built_raw             = ui->show_raw;
    view->built_sort_column     = view->sort_column;
    view->built_sort_descending = view->sort_descending;
    view->built_turn            = api->turn(api->engine);
    view->built_ledger_count    = api->ledger_count(api->engine);
    snprintf(view->built_filter, sizeof(view->built_filter), "%s", view->filter);
    snprintf(view->built_locale, sizeof(view->built_locale), "%s", ui->text ? ui->text->locale : "");
}

/* Call before the rows: the view is up to date after it. */
internal void
ui_table_view_update(ui_state *ui, engine_api *api, ui_table_view *view, ui_table_source *source)
{
    if (ui_table_view_stale(ui, api, view, source)) ui_table_view_build(ui, api, view, source);
}

/* The filter box of a table, on the header line of its panel. */
internal void
ui_table_filter_box(ui_state *ui, ui_table_view *view, const char *stable_id)
{
    char label[96];
    snprintf(label, sizeof(label), "###%s", stable_id);
    ImGui::SetNextItemWidth(UI_TABLE_FILTER_WIDTH);
    ImGui::InputTextWithHint(label, text_get(ui->text, "table.filter"), view->filter, sizeof(view->filter));
}

/* Call after TableHeadersRow: takes a click on a header into the view. */
internal void
ui_table_read_sort(ui_table_view *view)
{
    ImGuiTableSortSpecs *specs = ImGui::TableGetSortSpecs();
    if (!specs || !specs->SpecsDirty) return;
    if (specs->SpecsCount > 0)
    {
        view->sort_column     = specs->Specs[0].ColumnIndex;
        view->sort_descending = specs->Specs[0].SortDirection == ImGuiSortDirection_Descending;
    }
    else
    {
        view->sort_column = UI_SORT_NO_COLUMN;
    }
    specs->SpecsDirty = false;
}

#define UI_TABLE_FLAGS (ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | \
                        ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortTristate)
