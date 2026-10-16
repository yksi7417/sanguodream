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

/* A table column with the text of the locale and a stable id. */
internal void
ui_table_column(ui_state *ui, const char *text_id, const char *stable_id, ImGuiTableColumnFlags flags = 0, f32 width = 0.0f)
{
    char label[96];
    ImGui::TableSetupColumn(ui_label(label, sizeof(label), ui, text_id, stable_id), flags, width);
}

internal void
ui_draw_officer_row(ui_state *ui, engine_officer *officer)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::Text("%u", officer->officer_id);
    ImGui::TableNextColumn(); ui_name_cell(ui, officer->family_name, sizeof(officer->family_name));
    ImGui::TableNextColumn(); ui_name_cell(ui, officer->given_name, sizeof(officer->given_name));
    ImGui::TableNextColumn(); ui_text_id(officer->force_id, UI_NO_ID_8);
    ImGui::TableNextColumn(); ImGui::Text("%u", officer->status);
    ImGui::TableNextColumn(); ui_text_id(officer->base_location, UI_NO_ID_16);
    ImGui::TableNextColumn(); ImGui::Text("%u/%u/%u/%u/%u", officer->leadership, officer->war, officer->intelligence,
                                          officer->politics, officer->charm);
    ImGui::TableNextColumn(); ImGui::Text("%u-%u", officer->birth_year, officer->death_year);
    ImGui::TableNextColumn(); ImGui::Text("%u", officer->merit);   /* 0xFFFF for templates */
}

internal void
ui_draw_officers(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(640, 360), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "officers.title", "Officers"), &ui->show_officers))
    {
        u32 count = api->officer_count(api->engine);
        ImGui::Text("%u %s", count, text_get(ui->text, "officers.count"));
        ImGui::TextDisabled("%s", text_get(ui->text, "officers.help"));
        ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
        if (ImGui::BeginTable("officers", 9, flags))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ui_table_column(ui, "column.id", "id", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ui_table_column(ui, "column.family_name", "family");
            ui_table_column(ui, "column.given_name", "given");
            ui_table_column(ui, "column.force", "force", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ui_table_column(ui, "column.status", "status", ImGuiTableColumnFlags_WidthFixed, 45.0f);
            ui_table_column(ui, "column.location", "location", ImGuiTableColumnFlags_WidthFixed, 55.0f);
            ui_table_column(ui, "column.stats", "stats");
            ui_table_column(ui, "column.years", "years");
            ui_table_column(ui, "column.merit", "merit", ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableHeadersRow();
            ImGuiListClipper clipper;
            clipper.Begin((int)count);
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    engine_officer officer = api->officer_at(api->engine, (u32)row);
                    ui_draw_officer_row(ui, &officer);
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

internal void
ui_draw_city_row(ui_state *ui, engine_city *city, engine_city_name *name)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::Text("%u", city->city_id);
    ImGui::TableNextColumn();
    if (name->city_id == ENGINE_NO_CITY) ImGui::TextUnformatted("-");
    else                                 ui_name_cell(ui, name->name, sizeof(name->name));
    ImGui::TableNextColumn(); ui_text_id(city->owner_force_id, UI_NO_ID_8);
    ImGui::TableNextColumn(); ui_text_id(city->commanding_officer_id, UI_NO_ID_16);
    ImGui::TableNextColumn(); ImGui::Text("%u", city->gold);
    ImGui::TableNextColumn(); ImGui::Text("%u", city->troops);
    ImGui::TableNextColumn(); ImGui::Text("%u", city->food);
    ImGui::TableNextColumn(); ImGui::Text("%u", city->public_order);
    ImGui::TableNextColumn(); ImGui::Text("%u", city->durability_maximum);
}

internal void
ui_draw_cities(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(560, 360), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "cities.title", "Cities"), &ui->show_cities))
    {
        u32 count = api->city_count(api->engine);
        ImGui::Text("%u %s", count, text_get(ui->text, "cities.count"));
        ImGui::TextDisabled("%s", text_get(ui->text, "cities.help"));
        ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
        if (ImGui::BeginTable("cities", 9, flags))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ui_table_column(ui, "column.id", "id", ImGuiTableColumnFlags_WidthFixed, 30.0f);
            ui_table_column(ui, "column.name", "name", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ui_table_column(ui, "column.owner", "owner", ImGuiTableColumnFlags_WidthFixed, 45.0f);
            ui_table_column(ui, "column.commander", "commander", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ui_table_column(ui, "column.gold", "gold");
            ui_table_column(ui, "column.troops", "troops");
            ui_table_column(ui, "column.food", "food");
            ui_table_column(ui, "column.order", "order", ImGuiTableColumnFlags_WidthFixed, 45.0f);
            ui_table_column(ui, "column.durability", "durability");
            ImGui::TableHeadersRow();
            ImGuiListClipper clipper;
            clipper.Begin((int)count);
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    engine_city city = api->city_at(api->engine, (u32)row);
                    engine_city_name name = api->city_name_at(api->engine, city.city_id);
                    ui_draw_city_row(ui, &city, &name);
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
