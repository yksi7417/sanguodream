/* M15 the AI view (tutorial 28): the scored options of the last AI turn of
   one faction (engine_api ai_explain), and a line in the scene from each
   unit of that faction to the hex of its goal. A row frames its unit or
   its city. The view is a tool for "why did the AI do that"; it commands
   nothing. Included by ui.cpp. */

#define UI_AI_LINE_THICKNESS 2.0f
#define UI_AI_GOAL_RADIUS    5.0f

/* The faction of the view: the chosen one, else the first AI faction. */
internal u8
ui_ai_faction(ui_state *ui, engine_api *api)
{
    if (ui->ai.faction != ENGINE_NO_FACTION) return ui->ai.faction;
    for (u32 index = 0; index < api->faction_count(api->engine); ++index)
    {
        engine_faction faction = api->faction_at(api->engine, index);
        if (faction.force_id != ENGINE_NO_FACTION && !faction.human) return faction.force_id;
    }
    return ENGINE_NO_FACTION;
}

internal void
ui_ai_faction_combo(ui_state *ui, engine_api *api, u8 force)
{
    char label[96], preview[UI_NAME_SIZE * 2] = "-";
    if (force != ENGINE_NO_FACTION) ui_force_text(ui, api, force, preview, sizeof(preview));
    if (!ImGui::BeginCombo(ui_label(label, sizeof(label), ui, "aiview.faction", "ai_faction"), preview)) return;
    for (u32 index = 0; index < api->faction_count(api->engine); ++index)
    {
        engine_faction faction = api->faction_at(api->engine, index);
        if (faction.force_id == ENGINE_NO_FACTION || faction.human) continue;
        char name[UI_NAME_SIZE * 2], row[128];
        ui_force_text(ui, api, faction.force_id, name, sizeof(name));
        snprintf(row, sizeof(row), "%s###ai_f%u", name, faction.force_id);
        if (ImGui::Selectable(row, force == faction.force_id)) ui->ai.faction = faction.force_id;
    }
    ImGui::EndCombo();
}

/* One row: the mark of a chosen option, the goal, the command, the target,
   the officer and the score. A click frames the unit, else the city. */
internal void
ui_draw_ai_option(ui_state *ui, engine_api *api, engine_ai_option *option, u32 index)
{
    char goal_key[32], target[UI_NAME_SIZE * 2] = "-", officer[UI_NAME_SIZE * 2] = "-", row[160];
    snprintf(goal_key, sizeof(goal_key), "aiview.goal.%u", option->goal);
    if (option->city_id != ENGINE_NO_CITY) ui_city_label(ui, api, option->city_id, target, sizeof(target));
    if (option->officer != ENGINE_NO_OFFICER) ui_officer_text(ui, api, option->officer, officer, sizeof(officer));
    const char *command = option->goal == AI_GOAL_CITY_COMMAND ? ui_text_with_key(ui, "ai", option->key) : text_get(ui->text, "aiview.march");
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    snprintf(row, sizeof(row), "%s###ai_option%u", option->chosen ? "*" : " ", index);
    if (ImGui::Selectable(row, false, ImGuiSelectableFlags_SpanAllColumns))
    {
        if (option->unit != ENGINE_NO_UNIT) ui_frame_unit(ui, option->unit);
        else if (option->city_id != ENGINE_NO_CITY) ui_frame_city(ui, option->city_id);
    }
    ImGui::TableNextColumn(); ImGui::TextUnformatted(text_get(ui->text, goal_key));
    ImGui::TableNextColumn(); ImGui::TextUnformatted(command);
    ImGui::TableNextColumn(); ImGui::TextUnformatted(target);
    ImGui::TableNextColumn(); ImGui::TextUnformatted(officer);
    ImGui::TableNextColumn(); ImGui::Text("%u", option->score);
}

internal void
ui_draw_ai(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(560, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "aiview.title", "AI"), &ui->show_ai)) { ImGui::End(); return; }
    ImGui::TextWrapped("%s", text_get(ui->text, "aiview.help"));
    u8 force = ui_ai_faction(ui, api);
    ui_ai_faction_combo(ui, api, force);
    engine_ai_explain explain = api->ai_explain(api->engine, force);
    if (!explain.turn)
    {
        ImGui::TextUnformatted(text_get(ui->text, "aiview.no_record"));
        ImGui::End();
        return;
    }
    ImGui::Text(text_get(ui->text, "aiview.summary"), explain.count);
    if (ImGui::BeginTable("ai_options", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                          ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 14)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 1.5f);
        ui_table_column(ui, "aiview.goal", "goal");
        ui_table_column(ui, "aiview.command", "command");
        ui_table_column(ui, "aiview.target", "target");
        ui_table_column(ui, "aiview.officer", "officer");
        ui_table_column(ui, "aiview.score", "score");
        ImGui::TableHeadersRow();
        for (u32 index = 0; index < explain.count; ++index) ui_draw_ai_option(ui, api, explain.options + index, index);
        ImGui::EndTable();
    }
    ImGui::End();
}

/* The scene: a line from each unit of the view to the hex of its chosen
   goal, in the colour of the faction, with a ring at the goal. */
internal void
ui_draw_ai_lines(ui_state *ui, engine_api *api)
{
    if (!ui->scene.enabled || !ui->scene.initialized) return;
    ImGuiIO &io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) return;
    u8 force = ui_ai_faction(ui, api);
    if (force == ENGINE_NO_FACTION) return;
    engine_ai_explain explain = api->ai_explain(api->engine, force);
    m4 view_projection = camera_view_projection(&ui->scene.cam, io.DisplaySize.x, io.DisplaySize.y);
    ImDrawList *draw = ImGui::GetBackgroundDrawList();
    ImU32 colour = ui_faction_color(api, force);
    for (u32 index = 0; index < explain.count; ++index)
    {
        engine_ai_option *option = explain.options + index;
        if (!option->chosen || option->unit == ENGINE_NO_UNIT) continue;
        engine_unit unit = api->unit_at(api->engine, option->unit);
        f32 from_x = 0.0f, from_y = 0.0f, to_x = 0.0f, to_y = 0.0f;
        if (!ui_scene_screen(api, view_projection, hex_make(option->q, option->r), UI_LABEL_LIFT, &to_x, &to_y)) continue;
        draw->AddCircle(ImVec2(to_x, to_y), UI_AI_GOAL_RADIUS, colour, 0, UI_AI_LINE_THICKNESS);
        if (unit.id == ENGINE_NO_UNIT || !ui_scene_screen(api, view_projection, hex_make(unit.q, unit.r), UI_LABEL_LIFT, &from_x, &from_y)) continue;
        draw->AddLine(ImVec2(from_x, from_y), ImVec2(to_x, to_y), colour, UI_AI_LINE_THICKNESS);
    }
}
