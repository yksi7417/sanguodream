/* The Officers and Cities panels: tables of the loaded game, read row by row
   through engine_api. They command nothing. Included by ui.cpp.

   Names are Big5 bytes in the engine. The panels show them through the
   text module: text_name gives the row of names/<locale>.tsv, or else the
   decoded bytes (zh-TW). A tooltip on a name shows its bytes as hex, for
   spec question Q-013. City names come from the name index (Scenario.s11)
   through city_name_at. Until a name index loads, the Cities panel shows
   "-" in the name column. */

#define UI_SAMPLE_SAVE_PATH "..\\spec-src\\spec\\fixtures\\saves\\sample.s11"
#define UI_NAME_INDEX_PATH  "..\\spec-src\\spec\\fixtures\\scenarios\\Scenario.s11"
#define UI_NO_ID_16         0xFFFF
#define UI_NO_ID_8          0xFF

/* Writes the bytes of a NUL-padded name as hex pairs, "B1E4 B77C". */
internal void
ui_format_name_hex(char *out, u32 out_size, const u8 *bytes, u32 size)
{
    u32 written = 0;
    out[0] = 0;
    for (u32 index = 0; index < size && bytes[index]; ++index)
    {
        b32 pair_start = index > 0 && index % 2 == 0;
        int used = snprintf(out + written, out_size - written, pair_start ? " %02X" : "%02X", bytes[index]);
        if (used < 0 || written + (u32)used >= out_size) break;
        written += (u32)used;
    }
}

/* An id column: the number, or "-" for the spec's "none" value. */
internal void
ui_text_id(u32 value, u32 none)
{
    if (value == none) ImGui::TextUnformatted("-");
    else               ImGui::Text("%u", value);
}

#define UI_NAME_SIZE 64   /* UTF-8 of one name: a Big5 name field decodes to at most 3 bytes for each byte */

/* One name cell: the display name, and its bytes as hex in a tooltip. */
internal void
ui_name_cell(ui_state *ui, const u8 *bytes, u32 size)
{
    char name[UI_NAME_SIZE];
    text_name(ui->text, bytes, size, name, sizeof(name));
    ImGui::TextUnformatted(name);
    if (ImGui::IsItemHovered())
    {
        char hex[3 * UI_NAME_SIZE];
        ui_format_name_hex(hex, sizeof(hex), bytes, size);
        ImGui::SetTooltip("%s: %s", text_get(ui->text, "name.bytes"), hex);
    }
}

/* ---- id and code cells --------------------------------------------------

   An id column names its record and a code column shows its label. The
   raw number is in a tooltip on hover, and the "raw" check box of the
   panel (ui->show_raw) shows the raw numbers in place of the names. The
   names come from engine_api queries; the labels come from the locale TSV
   through text_label. */

/* The raw number of the last item, in a tooltip on hover. */
internal void
ui_raw_tooltip(ui_state *ui, u32 value)
{
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s: %u", text_get(ui->text, "label.raw"), value);
}

/* The family name and the given name of an officer, in out. */
internal void
ui_officer_name(ui_state *ui, engine_officer *officer, char *out, u32 out_size)
{
    char family[UI_NAME_SIZE], given[UI_NAME_SIZE];
    text_name(ui->text, officer->family_name, sizeof(officer->family_name), family, sizeof(family));
    text_name(ui->text, officer->given_name, sizeof(officer->given_name), given, sizeof(given));
    snprintf(out, out_size, "%s%s", family, given);
}

/* An officer id as the name of the officer; "-" for none; "?" and the id
   if no officer has the id. */
internal void
ui_officer_text(ui_state *ui, engine_api *api, u16 officer_id, char *out, u32 out_size)
{
    if (officer_id == ENGINE_NO_OFFICER) { snprintf(out, out_size, "-"); return; }
    engine_officer officer = api->officer_by_id(api->engine, officer_id);
    if (officer.officer_id == ENGINE_NO_OFFICER) snprintf(out, out_size, "?%u", officer_id);
    else                                        ui_officer_name(ui, &officer, out, out_size);
}

/* A force id as the name of its ruler (force_ruler); "-" for none; "force N"
   if the force has no ruler. */
internal void
ui_force_text(ui_state *ui, engine_api *api, u8 force_id, char *out, u32 out_size)
{
    if (force_id == ENGINE_NO_FACTION) { snprintf(out, out_size, "-"); return; }
    u16 ruler = api->force_ruler(api->engine, force_id);
    if (ruler == ENGINE_NO_OFFICER) snprintf(out, out_size, "%s %u", text_get(ui->text, "label.force"), force_id);
    else                            ui_officer_text(ui, api, ruler, out, out_size);
}

/* The faction of the player: the faction that acts if a human plays it,
   else the first human faction, else the faction that acts. */
internal u8
ui_player_force(engine_api *api)
{
    u8 acting = api->whose_turn(api->engine);
    u8 first_human = ENGINE_NO_FACTION;
    u32 count = api->faction_count(api->engine);
    for (u32 index = 0; index < count; ++index)
    {
        engine_faction faction = api->faction_at(api->engine, index);
        if (!faction.human) continue;
        if (faction.force_id == acting) return acting;
        if (first_human == ENGINE_NO_FACTION) first_human = faction.force_id;
    }
    return first_human != ENGINE_NO_FACTION ? first_human : acting;
}

/* The force whose cities the Cities panel and the City combo list, or
   ENGINE_NO_FACTION for every city ("All cities", or no player). */
internal u8
ui_city_filter_force(ui_state *ui, engine_api *api)
{
    return ui->show_all_cities ? ENGINE_NO_FACTION : ui_player_force(api);
}

/* The "All cities" check box. The Cities panel and the City panel share it. */
internal void
ui_all_cities_toggle(ui_state *ui)
{
    char label[96];
    ImGui::Checkbox(ui_label(label, sizeof(label), ui, "cities.all", "all_cities"), &ui->show_all_cities);
}

/* A location as its name from the name index; "-" for none; the number
   until a name index loads. */
internal void
ui_location_text(ui_state *ui, engine_api *api, u16 location, char *out, u32 out_size)
{
    if (location == ENGINE_NO_LOCATION) { snprintf(out, out_size, "-"); return; }
    engine_location_name name = api->location_name_at(api->engine, location);
    if (name.location == ENGINE_NO_LOCATION) snprintf(out, out_size, "%u", location);
    else                                     text_name(ui->text, name.name, sizeof(name.name), out, out_size);
}

enum ui_code_kind
{
    UI_CODE_FORCE,
    UI_CODE_OFFICER,
    UI_CODE_LOCATION,
    UI_CODE_STATUS,      /* text id "status.<code>" */
    UI_CODE_SPECIALTY,   /* text id "specialty.<code>" */
};

/* The text of one id or code, as a name or a label. */
internal void
ui_code_text(ui_state *ui, engine_api *api, ui_code_kind kind, u32 value, char *out, u32 out_size)
{
    switch (kind)
    {
        case UI_CODE_FORCE:     ui_force_text(ui, api, (u8)value, out, out_size);     break;
        case UI_CODE_OFFICER:   ui_officer_text(ui, api, (u16)value, out, out_size);  break;
        case UI_CODE_LOCATION:  ui_location_text(ui, api, (u16)value, out, out_size); break;
        case UI_CODE_STATUS:    text_label(ui->text, "status", value, out, out_size);    break;
        case UI_CODE_SPECIALTY: text_label(ui->text, "specialty", value, out, out_size); break;
    }
}

/* One id or code cell: the name or the label, or the raw number when the
   panel shows raw numbers. The raw number is also in a tooltip. */
internal void
ui_code_cell(ui_state *ui, engine_api *api, ui_code_kind kind, u32 value, u32 none)
{
    if (ui->show_raw)
    {
        ui_text_id(value, none);
        return;
    }
    char text[UI_NAME_SIZE * 2];
    ui_code_text(ui, api, kind, value, text, sizeof(text));
    ImGui::TextUnformatted(text);
    ui_raw_tooltip(ui, value);
}

/* The "raw" check box of a panel. All panels share the one setting. */
internal void
ui_raw_toggle(ui_state *ui)
{
    char label[96];
    ImGui::Checkbox(ui_label(label, sizeof(label), ui, "label.raw_toggle", "raw"), &ui->show_raw);
}

/* A table column with the text of the locale and a stable id. */
internal void
ui_table_column(ui_state *ui, const char *text_id, const char *stable_id, ImGuiTableColumnFlags flags = 0, f32 width = 0.0f)
{
    char label[96];
    ImGui::TableSetupColumn(ui_label(label, sizeof(label), ui, text_id, stable_id), flags, width);
}

internal void
ui_draw_officer_row(ui_state *ui, engine_api *api, engine_officer *officer)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::Text("%u", officer->officer_id);
    ImGui::TableNextColumn(); ui_name_cell(ui, officer->family_name, sizeof(officer->family_name));
    ImGui::TableNextColumn(); ui_name_cell(ui, officer->given_name, sizeof(officer->given_name));
    ImGui::TableNextColumn(); ui_code_cell(ui, api, UI_CODE_FORCE, officer->force_id, UI_NO_ID_8);
    ImGui::TableNextColumn(); ui_code_cell(ui, api, UI_CODE_STATUS, officer->status, U32_MAX);
    ImGui::TableNextColumn(); ui_code_cell(ui, api, UI_CODE_LOCATION, officer->base_location, UI_NO_ID_16);
    ImGui::TableNextColumn(); ImGui::Text("%u/%u/%u/%u/%u", officer->leadership, officer->war, officer->intelligence,
                                          officer->politics, officer->charm);
    ImGui::TableNextColumn(); ImGui::Text("%u-%u", officer->birth_year, officer->death_year);
    ImGui::TableNextColumn(); ImGui::Text("%u", officer->merit);   /* 0xFFFF for templates */
}

#include "ui/ui_table.cpp"   /* the filter and the sort of the tables; needs the code cells above */

/* The cells of one officer row for the filter and the sort of the view
   (ui_table.cpp): the same texts as ui_draw_officer_row shows. */
internal u8
ui_officer_row_cells(ui_state *ui, engine_api *api, void *user, u32 source, ui_sort_key *cells)
{
    (void)user;
    engine_officer officer = api->officer_at(api->engine, source);
    char name[UI_NAME_SIZE], text[48];
    ui_key_number(cells + 0, officer.officer_id);
    text_name(ui->text, officer.family_name, sizeof(officer.family_name), name, sizeof(name));
    ui_key_text(cells + 1, name);
    text_name(ui->text, officer.given_name, sizeof(officer.given_name), name, sizeof(name));
    ui_key_text(cells + 2, name);
    ui_key_code(ui, api, cells + 3, UI_CODE_FORCE, officer.force_id, UI_NO_ID_8);
    ui_key_code(ui, api, cells + 4, UI_CODE_STATUS, officer.status, U32_MAX);
    ui_key_code(ui, api, cells + 5, UI_CODE_LOCATION, officer.base_location, UI_NO_ID_16);
    snprintf(text, sizeof(text), "%u/%u/%u/%u/%u", officer.leadership, officer.war, officer.intelligence, officer.politics, officer.charm);
    ui_key_mixed(cells + 6, (s64)officer.leadership + officer.war + officer.intelligence + officer.politics + officer.charm, text);
    snprintf(text, sizeof(text), "%u-%u", officer.birth_year, officer.death_year);
    ui_key_mixed(cells + 7, officer.birth_year, text);
    ui_key_number(cells + 8, officer.merit);
    return officer.force_id;
}

internal void
ui_draw_officers(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(640, 360), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "officers.title", "Officers"), &ui->show_officers))
    {
        u32 count = api->officer_count(api->engine);
        ui_table_source source = {};
        source.source_count = count;
        source.column_count = 9;
        source.only_force   = ENGINE_NO_FACTION;
        source.row          = ui_officer_row_cells;
        source.rows         = ui->officers_rows;
        source.keys         = ui->officers_keys;
        source.capacity     = ArrayCount(ui->officers_rows);
        ui_table_view_update(ui, api, &ui->officers_view, &source);
        ImGui::Text("%u %s", count, text_get(ui->text, "officers.count"));
        ImGui::SameLine();
        ui_table_filter_box(ui, &ui->officers_view, "filter_officers");
        ImGui::SameLine();
        ui_raw_toggle(ui);
        ImGui::TextDisabled("%s", text_get(ui->text, "officers.help"));
        if (ImGui::BeginTable("officers", 9, UI_TABLE_FLAGS))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ui_table_column(ui, "column.id", "id", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ui_table_column(ui, "column.family_name", "family");
            ui_table_column(ui, "column.given_name", "given");
            ui_table_column(ui, "column.force", "force", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ui_table_column(ui, "column.status", "status", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ui_table_column(ui, "column.location", "location", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ui_table_column(ui, "column.stats", "stats");
            ui_table_column(ui, "column.years", "years");
            ui_table_column(ui, "column.merit", "merit", ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableHeadersRow();
            ui_table_read_sort(&ui->officers_view);
            ImGuiListClipper clipper;
            clipper.Begin((int)ui->officers_view.count);
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    engine_officer officer = api->officer_at(api->engine, ui->officers_rows[row]);
                    ui_draw_officer_row(ui, api, &officer);
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

internal void
ui_draw_city_row(ui_state *ui, engine_api *api, engine_city *city, engine_city_name *name)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::Text("%u", city->city_id);
    ImGui::TableNextColumn();
    if (name->city_id == ENGINE_NO_CITY) ImGui::TextUnformatted("-");
    else                                 ui_name_cell(ui, name->name, sizeof(name->name));
    ImGui::TableNextColumn(); ui_code_cell(ui, api, UI_CODE_FORCE, city->owner_force_id, UI_NO_ID_8);
    ImGui::TableNextColumn(); ui_code_cell(ui, api, UI_CODE_OFFICER, city->commanding_officer_id, UI_NO_ID_16);
    ImGui::TableNextColumn(); ImGui::Text("%u", city->gold);
    ImGui::TableNextColumn(); ImGui::Text("%u", city->troops);
    ImGui::TableNextColumn(); ImGui::Text("%u", city->food);
    ImGui::TableNextColumn(); ImGui::Text("%u", city->public_order);
    ImGui::TableNextColumn(); ImGui::Text("%u", city->durability_maximum);
}

/* The cells of one city row for the view: the texts of ui_draw_city_row. */
internal u8
ui_city_row_cells(ui_state *ui, engine_api *api, void *user, u32 source, ui_sort_key *cells)
{
    (void)user;
    engine_city city = api->city_at(api->engine, source);
    engine_city_name name = api->city_name_at(api->engine, city.city_id);
    char decoded[UI_NAME_SIZE];
    ui_key_number(cells + 0, city.city_id);
    if (name.city_id == ENGINE_NO_CITY) ui_key_text(cells + 1, "-");
    else
    {
        text_name(ui->text, name.name, sizeof(name.name), decoded, sizeof(decoded));
        ui_key_text(cells + 1, decoded);
    }
    ui_key_code(ui, api, cells + 2, UI_CODE_FORCE, city.owner_force_id, UI_NO_ID_8);
    ui_key_code(ui, api, cells + 3, UI_CODE_OFFICER, city.commanding_officer_id, UI_NO_ID_16);
    ui_key_number(cells + 4, city.gold);
    ui_key_number(cells + 5, city.troops);
    ui_key_number(cells + 6, city.food);
    ui_key_number(cells + 7, city.public_order);
    ui_key_number(cells + 8, city.durability_maximum);
    return city.owner_force_id;
}

internal void
ui_draw_cities(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(560, 360), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "cities.title", "Cities"), &ui->show_cities))
    {
        u32 count = api->city_count(api->engine);
        ui_table_source source = {};
        source.source_count = count;
        source.column_count = 9;
        source.only_force   = ui_city_filter_force(ui, api);
        source.row          = ui_city_row_cells;
        source.rows         = ui->cities_rows;
        source.keys         = ui->cities_keys;
        source.capacity     = ArrayCount(ui->cities_rows);
        ui_table_view_update(ui, api, &ui->cities_view, &source);
        if (ui->cities_view.count == 0 && source.only_force != ENGINE_NO_FACTION)
        {
            /* the player owns no city: the panel lists every city */
            source.only_force = ENGINE_NO_FACTION;
            ui_table_view_update(ui, api, &ui->cities_view, &source);
        }
        ImGui::Text("%u %s", count, text_get(ui->text, "cities.count"));
        ImGui::SameLine();
        ui_all_cities_toggle(ui);
        ImGui::SameLine();
        ui_table_filter_box(ui, &ui->cities_view, "filter_cities");
        ImGui::SameLine();
        ui_raw_toggle(ui);
        ImGui::TextDisabled("%s", text_get(ui->text, "cities.help"));
        if (ImGui::BeginTable("cities", 9, UI_TABLE_FLAGS))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ui_table_column(ui, "column.id", "id", ImGuiTableColumnFlags_WidthFixed, 30.0f);
            ui_table_column(ui, "column.name", "name", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ui_table_column(ui, "column.owner", "owner", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ui_table_column(ui, "column.commander", "commander", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ui_table_column(ui, "column.gold", "gold");
            ui_table_column(ui, "column.troops", "troops");
            ui_table_column(ui, "column.food", "food");
            ui_table_column(ui, "column.order", "order", ImGuiTableColumnFlags_WidthFixed, 45.0f);
            ui_table_column(ui, "column.durability", "durability");
            ImGui::TableHeadersRow();
            ui_table_read_sort(&ui->cities_view);
            ImGuiListClipper clipper;
            clipper.Begin((int)ui->cities_view.count);
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    engine_city city = api->city_at(api->engine, ui->cities_rows[row]);
                    engine_city_name name = api->city_name_at(api->engine, city.city_id);
                    ui_draw_city_row(ui, api, &city, &name);
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

/* The button sends one load_save command; the result text is copied into
   ui_state, because the engine's detail string lives in the DLL image. */
internal void
ui_draw_load_sample_save(ui_state *ui, engine_api *api)
{
    if (ImGui::Button("Load sample save"))
    {
        engine_load_result result = api->load_save(api->engine, UI_SAMPLE_SAVE_PATH);
        if (result.status == ENGINE_LOAD_OK)
        {
            snprintf(ui->load_message, sizeof(ui->load_message), "loaded (command %u)", result.command_id);
        }
        else
        {
            snprintf(ui->load_message, sizeof(ui->load_message), "not loaded: %s", result.detail ? result.detail : "?");
        }
    }
    if (ui->load_message[0])
    {
        ImGui::SameLine();
        ImGui::TextUnformatted(ui->load_message);
    }
}

/* The button sends one load_name_index command, as the sample save button. */
internal void
ui_draw_load_city_names(ui_state *ui, engine_api *api)
{
    if (ImGui::Button("Load city names"))
    {
        engine_load_result result = api->load_name_index(api->engine, UI_NAME_INDEX_PATH);
        if (result.status == ENGINE_LOAD_OK)
        {
            snprintf(ui->names_message, sizeof(ui->names_message), "names loaded (command %u)", result.command_id);
        }
        else
        {
            snprintf(ui->names_message, sizeof(ui->names_message), "names not loaded: %s", result.detail ? result.detail : "?");
        }
    }
    if (ui->names_message[0])
    {
        ImGui::SameLine();
        ImGui::TextUnformatted(ui->names_message);
    }
}
