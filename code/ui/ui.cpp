/* Debug UI panels. Reads game and ECS state directly; the only place ImGui is
   called from inside the DLL. Included by hexdom.cpp after game_state and
   ledger_append exist. */

internal void
ui_draw_arena(const char *name, memory_arena *arena)
{
    f32 fraction = arena->size ? (f32)((f64)arena->used / (f64)arena->size) : 0.0f;
    char label[64];
    snprintf(label, sizeof(label), "%.2f / %.2f MB", (f64)arena->used / 1048576.0, (f64)arena->size / 1048576.0);
    ImGui::Text("%s", name);
    ImGui::SameLine(140.0f);
    ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f), label);
}

internal void
ui_draw(game_state *game, game_input *input, memory_arena *frame)
{
    ui_state *ui = &game->ui;

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Windows"))
        {
            ImGui::MenuItem("Simulation", 0, &ui->show_sim);
            ImGui::MenuItem("Event ledger", 0, &ui->show_ledger);
            ImGui::MenuItem("Memory", 0, &ui->show_memory);
            ImGui::Separator();
            ImGui::MenuItem("ImGui demo", 0, &ui->show_demo);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        ImGui::Text("turn %u", game->sim->turn);
        ImGui::Separator();
        ImGui::Text("dll reloads %u", game->reload_count);
        ImGui::Separator();
        ImGui::Text("%.1f ms  %.0f fps", (f64)input->dt_seconds * 1000.0, (f64)ImGui::GetIO().Framerate);
        ImGui::EndMainMenuBar();
    }

    if (ui->show_demo)
    {
        ImGui::ShowDemoWindow(&ui->show_demo);
    }

    if (ui->show_sim)
    {
        ImGui::SetNextWindowSize(ImVec2(360, 220), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Simulation", &ui->show_sim))
        {
            ImGui::Text("Turn: %u", game->sim->turn);
            ImGui::Text("Entities alive: %u / %u", game->sim->entities.alive_count, game->sim->entities.capacity);
            ImGui::Text("Next command id: %u", game->sim->next_command_id);
            ImGui::Separator();
            if (ImGui::Button("Advance turn"))
            {
                sim_event_sink sink = sim_sink_from_arena(frame, 256);
                sim_command cmd = sim_make_command(game->sim, SIM_COMMAND_END_TURN);
                sim_apply(game->sim, cmd, &sink);
                ledger_append(game, &sink);
            }
            ImGui::SameLine();
            if (ImGui::Button("Advance 12 turns"))
            {
                sim_event_sink sink = sim_sink_from_arena(frame, 4096);
                for (u32 i = 0; i < 12; ++i)
                {
                    sim_command cmd = sim_make_command(game->sim, SIM_COMMAND_END_TURN);
                    sim_apply(game->sim, cmd, &sink);
                }
                ledger_append(game, &sink);
            }
            ImGui::Spacing();
            ImGui::TextWrapped("Mechanics implemented: none yet. Each lands from "
                               "spec-src/spec/mechanics/ with its fixture rows passing in sim_tests.exe.");
        }
        ImGui::End();
    }

    if (ui->show_ledger)
    {
        ImGui::SetNextWindowSize(ImVec2(560, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Event ledger", &ui->show_ledger))
        {
            ImGui::Text("%u events (capacity %u)", game->ledger_count, game->ledger_capacity);
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear")) game->ledger_count = 0;
            ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                    ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
            if (ImGui::BeginTable("ledger", 6, flags))
            {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                ImGui::TableSetupColumn("turn", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                ImGui::TableSetupColumn("event");
                ImGui::TableSetupColumn("subject", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("delta", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("cause", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableHeadersRow();
                ImGuiListClipper clipper;
                clipper.Begin((int)game->ledger_count);
                while (clipper.Step())
                {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                    {
                        sim_event *ev = game->ledger + row;
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%d", row);
                        ImGui::TableNextColumn(); ImGui::Text("%u", ev->turn);
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(sim_event_name(ev->kind));
                        ImGui::TableNextColumn(); ImGui::Text("%u", ev->subject);
                        ImGui::TableNextColumn(); ImGui::Text("%d", ev->delta);
                        ImGui::TableNextColumn(); ImGui::Text("%u", ev->cause);
                    }
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    if (ui->show_memory)
    {
        ImGui::SetNextWindowSize(ImVec2(420, 160), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Memory", &ui->show_memory))
        {
            ui_draw_arena("permanent", &game->permanent);
            ui_draw_arena("transient", &game->transient);
            ui_draw_arena("frame", frame);
        }
        ImGui::End();
    }
}
