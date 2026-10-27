/* The Replay panel (docs/REPLAY.md): the steps of the game, one player
   action at a time. The buttons step back and forward, a turn number
   jumps to the start of that turn, and a click on a row shows the state
   after that step. The map and every panel show the state of the view,
   and the camera goes to the place of the step. Live closes the view.
   Included by ui.cpp. */

/* Shows the state after `position` steps and frames the place of the
   last of them. */
internal void
ui_replay_show(ui_state *ui, engine_api *api, u32 position)
{
    engine_game_replay_view view = api->game_replay_view(api->engine, position);
    if (!view.ok) return;
    ui->replay.viewing  = true;
    ui->replay.position = position;
    ui->replay.matches  = view.matches;
    if (position == 0) return;
    engine_game_replay_step step = api->game_replay_step_at(api->engine, position - 1);
    if (step.place.on_map) ui_frame_hex(ui, api, hex_make(step.place.q, step.place.r));
}

internal void
ui_replay_close(ui_state *ui, engine_api *api)
{
    if (ui->replay.viewing) api->game_replay_close(api->engine);
    ui->replay.viewing = false;
}

/* The words of a step: the command, or the change of the settings. */
internal const char *
ui_replay_step_text(ui_state *ui, const engine_game_replay_step *step)
{
    return ui_text_with_key(ui, "replay.step", step->key ? step->key : "none");
}

internal void
ui_draw_replay_controls(ui_state *ui, engine_api *api, u32 count)
{
    char label[96];
    u32 position = ui->replay.position;
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "replay.first", "replay_first"))) ui_replay_show(ui, api, 0);
    ImGui::SameLine();
    ImGui::BeginDisabled(position == 0);
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "replay.back", "replay_back"))) ui_replay_show(ui, api, position - 1);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(position >= count);
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "replay.forward", "replay_forward"))) ui_replay_show(ui, api, position + 1);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "replay.last", "replay_last"))) ui_replay_show(ui, api, count);
    ImGui::SameLine();
    ImGui::BeginDisabled(!ui->replay.viewing);
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "replay.live", "replay_live"))) ui_replay_close(ui, api);
    ImGui::EndDisabled();

    ImGui::SetNextItemWidth(90.0f);
    ImGui::InputInt(ui_label(label, sizeof(label), ui, "replay.turn", "replay_turn"), &ui->replay.turn);
    if (ui->replay.turn < 0) ui->replay.turn = 0;
    ImGui::SameLine();
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "replay.go", "replay_go")))
    {
        ui_replay_show(ui, api, api->game_replay_turn_step(api->engine, (u32)ui->replay.turn));
    }
}

/* One row for each step: the step, the turn, the player and the action.
   A click shows the state after the step. */
internal void
ui_draw_replay_steps(ui_state *ui, engine_api *api, u32 count)
{
    if (!ImGui::BeginTable("replay_steps", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                           ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 14))) return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn(text_get(ui->text, "replay.column.step"));
    ImGui::TableSetupColumn(text_get(ui->text, "replay.column.turn"));
    ImGui::TableSetupColumn(text_get(ui->text, "replay.column.player"));
    ImGui::TableSetupColumn(text_get(ui->text, "replay.column.action"), ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(text_get(ui->text, "replay.column.place"), ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
    ImGuiListClipper clipper;
    clipper.Begin((int)count);
    while (clipper.Step())
    {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
        {
            engine_game_replay_step step = api->game_replay_step_at(api->engine, (u32)row);
            if (step.index == ENGINE_NO_STEP) continue;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            char label[48];
            snprintf(label, sizeof(label), "%u%s###replay_step%u", step.index + 1, step.edited ? " !" : "", step.index);
            if (ImGui::Selectable(label, ui->replay.position == step.index + 1, ImGuiSelectableFlags_SpanAllColumns))
            {
                ui_replay_show(ui, api, step.index + 1);
            }
            ImGui::TableNextColumn(); ImGui::Text("%u", step.turn);
            char player[UI_NAME_SIZE * 2];
            ui_force_text(ui, api, step.force, player, sizeof(player));
            ImGui::TableNextColumn(); ImGui::TextUnformatted(player);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(ui_replay_step_text(ui, &step));
            char place[UI_NAME_SIZE * 4];
            ui_report_place(ui, api, &step.place, place, sizeof(place));
            ImGui::TableNextColumn(); ImGui::TextUnformatted(place);
        }
    }
    ImGui::EndTable();
}

/* The log lines of the last step of the view: what the player and the AI
   factions did in it. */
internal void
ui_draw_replay_events(ui_state *ui, engine_api *api)
{
    if (ui->replay.position == 0) return;
    engine_game_replay_step step = api->game_replay_step_at(api->engine, ui->replay.position - 1);
    if (step.index == ENGINE_NO_STEP) return;
    ImGui::Text(text_get(ui->text, "replay.events"), step.event_count);
    if (!ImGui::BeginChild("replay_events", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 8), ImGuiChildFlags_Borders)) { ImGui::EndChild(); return; }
    u32 shown = Minimum(step.event_count, (u32)UI_REPLAY_EVENT_LINES);
    for (u32 offset = 0; offset < shown; ++offset)
    {
        char line[UI_NAME_SIZE * 8];
        ui_report_line(ui, api, step.first_event + offset, TEXT_NO_VALUE, TEXT_LINE_LOG, line, sizeof(line));
        ImGui::TextUnformatted(line);
    }
    ImGui::EndChild();
}

internal void
ui_draw_game_replay(ui_state *ui, engine_api *api)
{
    void *e = api->engine;
    engine_game_replay_info info = api->game_replay_info(e);
    ui->replay.viewing  = info.viewing != 0;   /* a command returns the engine to the live game */
    ui->replay.position = info.viewing ? info.view_step : info.step_count;
    char title[96];
    ImGui::SetNextWindowSize(ImVec2(560, 560), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "replay.title", "GameReplay"), &ui->show_replay))
    {
        engine_date date = api->date(e);
        ImGui::Text(text_get(ui->text, "replay.position"), ui->replay.position, info.step_count, api->turn(e), date.year, date.month, date.day);
        if (!ui->replay.viewing) ImGui::TextUnformatted(text_get(ui->text, "replay.is_live"));
        if (info.status == ENGINE_GAME_REPLAY_EMPTY)     ImGui::TextUnformatted(text_get(ui->text, "replay.empty"));
        if (info.status == ENGINE_GAME_REPLAY_FULL)      ImGui::TextUnformatted(text_get(ui->text, "replay.full"));
        if (info.status == ENGINE_GAME_REPLAY_NO_MEMORY) ImGui::TextUnformatted(text_get(ui->text, "replay.no_memory"));
        if (info.first_edit != ENGINE_NO_STEP) ImGui::Text(text_get(ui->text, "replay.edited"), info.first_edit + 1);
        if (ui->replay.viewing && !ui->replay.matches) ImGui::TextUnformatted(text_get(ui->text, "replay.differs"));
        ui_draw_replay_controls(ui, api, info.step_count);
        ui_draw_replay_steps(ui, api, info.step_count);
        ui_draw_replay_events(ui, api);
    }
    ImGui::End();
}
