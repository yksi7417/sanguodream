/* The Officers and Cities panels: tables of the loaded game, read row by row
   through engine_api. They command nothing. Included by ui.cpp.

   Names are Big5 bytes. The spec gives no character mapping
   (QUESTIONS.md, Q-013), so the panels show the bytes as hex. City names
   live in Scenario.s11, which is not a fixture yet (Q-014), so the Cities
   panel shows ids. */

#define UI_SAMPLE_SAVE_PATH "..\\spec-src\\spec\\fixtures\\saves\\sample.s11"
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

internal void
ui_draw_officer_row(engine_officer *officer)
{
    char name[32];
    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::Text("%u", officer->officer_id);
    ImGui::TableNextColumn(); ui_format_name_hex(name, sizeof(name), officer->family_name, sizeof(officer->family_name));
                              ImGui::TextUnformatted(name);
    ImGui::TableNextColumn(); ui_format_name_hex(name, sizeof(name), officer->given_name, sizeof(officer->given_name));
                              ImGui::TextUnformatted(name);
    ImGui::TableNextColumn(); ui_text_id(officer->force_id, UI_NO_ID_8);
    ImGui::TableNextColumn(); ImGui::Text("%u", officer->status);
    ImGui::TableNextColumn(); ui_text_id(officer->base_location, UI_NO_ID_16);
    ImGui::TableNextColumn(); ImGui::Text("%u/%u/%u/%u/%u", officer->leadership, officer->war, officer->intelligence,
                                          officer->politics, officer->charm);
    ImGui::TableNextColumn(); ImGui::Text("%u-%u", officer->birth_year, officer->death_year);
    ImGui::TableNextColumn(); ui_text_id(officer->merit, UI_NO_ID_16);
}

internal void
ui_draw_officers(ui_state *ui, engine_api *api)
{
    ImGui::SetNextWindowSize(ImVec2(640, 360), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Officers", &ui->show_officers))
    {
        u32 count = api->officer_count(api->engine);
        ImGui::Text("%u officers", count);
        ImGui::TextDisabled("Names: Big5 bytes as hex (the spec gives no character mapping). "
                            "Status: 0 ruler, 1 viceroy, 2 governor, 3 regular, 6 free, 8 dead.");
        ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
        if (ImGui::BeginTable("officers", 9, flags))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableSetupColumn("family name");
            ImGui::TableSetupColumn("given name");
            ImGui::TableSetupColumn("force", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableSetupColumn("status", ImGuiTableColumnFlags_WidthFixed, 45.0f);
            ImGui::TableSetupColumn("location", ImGuiTableColumnFlags_WidthFixed, 55.0f);
            ImGui::TableSetupColumn("L/W/I/P/C");
            ImGui::TableSetupColumn("years");
            ImGui::TableSetupColumn("merit", ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableHeadersRow();
            ImGuiListClipper clipper;
            clipper.Begin((int)count);
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    engine_officer officer = api->officer_at(api->engine, (u32)row);
                    ui_draw_officer_row(&officer);
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

internal void
ui_draw_city_row(engine_city *city)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::Text("%u", city->city_id);
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
    ImGui::SetNextWindowSize(ImVec2(560, 360), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Cities", &ui->show_cities))
    {
        u32 count = api->city_count(api->engine);
        ImGui::Text("%u cities", count);
        ImGui::TextDisabled("City names are in Scenario.s11, which is not a fixture yet. "
                            "Public order and durability maximum are hypotheses in the spec.");
        ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
        if (ImGui::BeginTable("cities", 8, flags))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthFixed, 30.0f);
            ImGui::TableSetupColumn("owner", ImGuiTableColumnFlags_WidthFixed, 45.0f);
            ImGui::TableSetupColumn("commander", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn("gold");
            ImGui::TableSetupColumn("troops");
            ImGui::TableSetupColumn("food");
            ImGui::TableSetupColumn("order", ImGuiTableColumnFlags_WidthFixed, 45.0f);
            ImGui::TableSetupColumn("durability");
            ImGui::TableHeadersRow();
            ImGuiListClipper clipper;
            clipper.Begin((int)count);
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    engine_city city = api->city_at(api->engine, (u32)row);
                    ui_draw_city_row(&city);
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
