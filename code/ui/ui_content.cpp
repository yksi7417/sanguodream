/* M16 the Content panel (tutorial 29): the install folder of the player and
   the source of each asset. Set names the folder, and Clear forgets it.
   A scenario of the install has a Start button. The panel reads
   content_source and commands nothing else. Included by ui.cpp. */

/* The officer view frames the portrait of its officer. No source gives a
   picture yet, so the frame shows the first character of the name and the
   source that content_source reports. Each source has its own item id. */
internal void
ui_draw_officer_portrait(ui_state *ui, engine_api *api, u16 officer_id)
{
    /* The default portraits (docs/ASSETS.md): the picture when the archive
       has one for the officer. The frame with the first character stays
       for a game without the archive and for the tests. */
    engine_officer officer = api->officer_by_id(api->engine, officer_id);
    ImTextureID face = ui_officer_portrait(ui, &officer);
    if (face)
    {
        f32 width = ImGui::GetTextLineHeightWithSpacing() * UI_PORTRAIT_LINES * 4.0f / 5.0f;
        ImGui::Image(face, ImVec2(width, width * 5.0f / 4.0f));
        return;
    }
    engine_content_source source = api->content_source(api->engine, ENGINE_ASSET_PORTRAITS);
    char name[UI_NAME_SIZE * 2];
    ui_officer_text(ui, api, officer_id, name, sizeof(name));
    char first[TEXT_UTF8_MAX_BYTES + 1] = {};
    u32 length = 1;
    while (name[length] && (name[length] & 0xC0) == 0x80 && length < TEXT_UTF8_MAX_BYTES) length++;
    if (name[0]) memcpy(first, name, length);

    char source_label[64], label[128];
    text_label(ui->text, "content.source", source.source, source_label, sizeof(source_label));
    snprintf(label, sizeof(label), "%s###portrait_%u", first, source.source);
    f32 side = ImGui::GetTextLineHeight() * 3.0f;
    ImGui::Selectable(label, false, ImGuiSelectableFlags_Disabled, ImVec2(side, side));
    ImGui::SameLine();
    ImGui::Text("%s: %s", text_get(ui->text, "content.portrait"), source_label);
}

/* One row for each asset kind: its source, its counts and why. */
internal void
ui_draw_content_sources(ui_state *ui, engine_api *api)
{
    if (!ImGui::BeginTable("content_sources", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) return;
    ImGui::TableSetupColumn(text_get(ui->text, "content.asset"));
    ImGui::TableSetupColumn(text_get(ui->text, "content.source"));
    ImGui::TableSetupColumn(text_get(ui->text, "content.count"));
    ImGui::TableSetupColumn(text_get(ui->text, "content.install_files"));
    ImGui::TableSetupColumn(text_get(ui->text, "content.detail"), ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
    for (u8 asset = 0; asset < ENGINE_ASSET_KIND_COUNT; ++asset)
    {
        engine_content_source source = api->content_source(api->engine, asset);
        char text[64];
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextUnformatted(text_label(ui->text, "content.asset", asset, text, sizeof(text)));
        ImGui::TableNextColumn(); ImGui::TextUnformatted(text_label(ui->text, "content.source", source.source, text, sizeof(text)));
        ImGui::TableNextColumn(); ImGui::Text("%u", source.count);
        ImGui::TableNextColumn(); ImGui::Text("%u", source.install_files);
        ImGui::TableNextColumn(); ImGui::TextWrapped("%s", source.detail ? source.detail : "");
    }
    ImGui::EndTable();
}

/* The scenarios of the install, with the title in the font of the game. */
internal void
ui_draw_install_scenarios(ui_state *ui, engine_api *api)
{
    u32 count = api->install_scenario_count(api->engine);
    if (count == 0) return;
    if (!ImGui::BeginTable("install_scenarios", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) return;
    ImGui::TableSetupColumn(text_get(ui->text, "content.number"));
    ImGui::TableSetupColumn(text_get(ui->text, "content.scenario"), ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(text_get(ui->text, "content.start_date"));
    ImGui::TableSetupColumn("");
    ImGui::TableHeadersRow();
    for (u32 index = 0; index < count; ++index)
    {
        engine_install_scenario scenario = api->install_scenario_at(api->engine, index);
        if (scenario.number == ENGINE_NO_ROW) continue;
        char title[SCENARIO_TITLE_MAX_SIZE * 2];
        text_decode_big5_into(scenario.title, sizeof(scenario.title), title, sizeof(title));
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("%u", scenario.number);
        ImGui::TableNextColumn(); ImGui::TextUnformatted(title);
        ImGui::TableNextColumn(); ImGui::Text("%u/%u", scenario.year, scenario.month);
        ImGui::TableNextColumn();
        char label[96];
        snprintf(label, sizeof(label), "%s###start%u", text_get(ui->text, "content.start"), index);
        if (ImGui::SmallButton(label)) api->load_scenario(api->engine, scenario.file);
    }
    ImGui::EndTable();
}

internal void
ui_draw_content(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(640, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "content.title", "Content"), &ui->show_content)) { ImGui::End(); return; }
    ImGui::TextWrapped("%s", text_get(ui->text, "content.help"));
    char label[96];
    snprintf(label, sizeof(label), "%s###install_folder", text_get(ui->text, "content.folder"));
    ImGui::InputText(label, ui->content.folder, sizeof(ui->content.folder));
    snprintf(label, sizeof(label), "%s###set_install", text_get(ui->text, "content.set"));
    if (ImGui::Button(label))
    {
        ui->content.result     = api->set_install_folder(api->engine, ui->content.folder);
        ui->content.has_result = true;
    }
    ImGui::SameLine();
    snprintf(label, sizeof(label), "%s###clear_install", text_get(ui->text, "content.clear"));
    if (ImGui::Button(label))
    {
        ui->content.result     = api->set_install_folder(api->engine, "");
        ui->content.has_result = true;
    }
    if (ui->content.has_result) ImGui::TextWrapped("%s", ui->content.result.detail ? ui->content.result.detail : "");
    ui_draw_content_sources(ui, api);
    ui_draw_install_scenarios(ui, api);
    ImGui::End();
}
