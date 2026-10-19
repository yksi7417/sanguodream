/* M13 the Fidelity panel (tutorial 26): the save pairs of the original,
   the replay of one pair and the fields that differ. A replay replaces the
   game: the panel is a tool, not a game screen. Included by ui.cpp. */

internal const char *
ui_replay_command_key(u8 kind)
{
    switch (kind)
    {
        case ENGINE_REPLAY_END_TURN:  return "fidelity.end_turn";
        case ENGINE_REPLAY_DEVELOP:   return "fidelity.develop";
        case ENGINE_REPLAY_CONSCRIPT: return "fidelity.conscript";
        case ENGINE_REPLAY_PATROL:    return "fidelity.patrol";
        case ENGINE_REPLAY_TRAIN:     return "fidelity.train";
        default:                      return "fidelity.no_handler";
    }
}

/* The pairs of pairs.tsv, with one Replay button for each row. */
internal void
ui_draw_fidelity_pairs(ui_state *ui, engine_api *api)
{
    void *e = api->engine;
    if (!ImGui::BeginTable("fidelity_pairs", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) return;
    ImGui::TableSetupColumn(text_get(ui->text, "fidelity.pair"));
    ImGui::TableSetupColumn(text_get(ui->text, "fidelity.command"));
    ImGui::TableSetupColumn(text_get(ui->text, "fidelity.questions"));
    ImGui::TableSetupColumn("");
    ImGui::TableHeadersRow();
    for (u32 index = 0; index < api->replay_pair_count(e); ++index)
    {
        engine_replay_pair pair = api->replay_pair_at(e, index);
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("%s/%03u", pair.rule, pair.number);
        ImGui::TableNextColumn(); ImGui::TextUnformatted(text_get(ui->text, ui_replay_command_key(pair.command.kind)));
        ImGui::TableNextColumn(); ImGui::TextUnformatted(pair.q_ids);
        ImGui::TableNextColumn();
        char label[96];
        snprintf(label, sizeof(label), "%s###replay%u", text_get(ui->text, "fidelity.replay"), index);
        if (ImGui::SmallButton(label))
        {
            char before[ENGINE_REPLAY_PATH_SIZE + 16], after[ENGINE_REPLAY_PATH_SIZE + 16];
            snprintf(before, sizeof(before), "%s\\before.s11", pair.folder);
            snprintf(after, sizeof(after), "%s\\after.s11", pair.folder);
            ui->fidelity.report     = api->replay_pair(e, before, &pair.command, after);
            ui->fidelity.pair       = index;
            ui->fidelity.has_report = true;
        }
    }
    ImGui::EndTable();
}

/* The counts of the last replay and one row for each field that differs.
   A confirmed field has a mark. */
internal void
ui_draw_fidelity_report(ui_state *ui, engine_api *api)
{
    engine_replay_report *report = &ui->fidelity.report;
    engine_replay_pair pair = api->replay_pair_at(api->engine, ui->fidelity.pair);
    ImGui::Text("%s/%03u", pair.rule, pair.number);
    if (report->status != ENGINE_REPLAY_OK)
    {
        char key[48];
        snprintf(key, sizeof(key), "fidelity.status.%u", report->status);
        ImGui::TextUnformatted(text_get(ui->text, key));
        return;
    }
    ImGui::Text(text_get(ui->text, "fidelity.summary"), report->fields, report->differ, report->differ_verified);
    if (!ImGui::BeginTable("fidelity_diffs", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                           ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 14))) return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn(text_get(ui->text, "fidelity.field"));
    ImGui::TableSetupColumn(text_get(ui->text, "fidelity.before"));
    ImGui::TableSetupColumn(text_get(ui->text, "fidelity.replayed"));
    ImGui::TableSetupColumn(text_get(ui->text, "fidelity.original"));
    ImGui::TableSetupColumn(text_get(ui->text, "fidelity.rule"));
    ImGui::TableSetupColumn("Q");
    ImGui::TableHeadersRow();
    ImGuiListClipper clipper;
    clipper.Begin((int)report->diff_count);
    while (clipper.Step())
    {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
        {
            engine_replay_diff diff = api->replay_diff_at(api->engine, (u32)row);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%s%s", diff.verified ? "* " : "  ", diff.path);
            ImGui::TableNextColumn(); ImGui::Text("%lld", diff.before);
            ImGui::TableNextColumn(); ImGui::Text("%lld", diff.replayed);
            ImGui::TableNextColumn(); ImGui::Text("%lld", diff.original);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(diff.rule);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(diff.q_id);
        }
    }
    ImGui::EndTable();
}

internal void
ui_draw_fidelity(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(640, 520), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "fidelity.title", "Fidelity"), &ui->show_fidelity)) { ImGui::End(); return; }
    ImGui::TextWrapped("%s", text_get(ui->text, "fidelity.help"));
    ui_draw_fidelity_pairs(ui, api);
    if (ui->fidelity.has_report)
    {
        ImGui::Separator();
        ui_draw_fidelity_report(ui, api);
    }
    ImGui::End();
}
