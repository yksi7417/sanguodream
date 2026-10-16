/* Debug UI panels. The only place ImGui is called from inside the DLL, and
   the only thing it does to the game is call engine_api. */

#include <stdio.h>   /* snprintf for labels */
#include "imgui.h"
#include "ui/ui.h"

#include "ui/ui_map.cpp"

internal void
ui_init(ui_state *ui)
{
    ui->show_demo   = true;
    ui->show_sim    = true;
    ui->show_ledger = true;
    ui->show_memory = false;
    ui->show_map    = true;
    ui_map_init(ui);
}

internal void
ui_draw_usage(const char *name, u64 used, u64 size)
{
    f32 fraction = size ? (f32)((f64)used / (f64)size) : 0.0f;
    char label[64];
    snprintf(label, sizeof(label), "%.2f / %.2f MB", (f64)used / 1048576.0, (f64)size / 1048576.0);
    ImGui::Text("%s", name);
    ImGui::SameLine(140.0f);
    ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f), label);
}

internal void
ui_draw_menu_bar(ui_state *ui, engine_api *api, ui_frame_info *frame)
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Windows"))
        {
            ImGui::MenuItem("Simulation", 0, &ui->show_sim);
            ImGui::MenuItem("Hex map", 0, &ui->show_map);
            ImGui::MenuItem("Event ledger", 0, &ui->show_ledger);
            ImGui::MenuItem("Memory", 0, &ui->show_memory);
            ImGui::Separator();
            ImGui::MenuItem("ImGui demo", 0, &ui->show_demo);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        ImGui::Text("turn %u", api->turn(api->engine));
        ImGui::Separator();
        ImGui::Text("dll reloads %u", frame->dll_reload_count);
        ImGui::Separator();
        ImGui::Text("%.1f ms  %.0f fps", (f64)frame->dt_seconds * 1000.0, (f64)ImGui::GetIO().Framerate);
        ImGui::EndMainMenuBar();
    }
}

internal void
ui_draw_simulation(ui_state *ui, engine_api *api)
{
    ImGui::SetNextWindowSize(ImVec2(360, 220), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Simulation", &ui->show_sim))
    {
        ImGui::Text("Turn: %u", api->turn(api->engine));
        ImGui::Text("Entities alive: %u / %u", api->entities_alive(api->engine), api->entity_capacity(api->engine));
        ImGui::Separator();
        if (ImGui::Button("Advance turn"))
        {
            api->end_turn(api->engine);
        }
        ImGui::SameLine();
        if (ImGui::Button("Advance 12 turns"))
        {
            api->advance_turns(api->engine, 12);
        }
        ImGui::Spacing();
        ImGui::TextWrapped("Mechanics implemented: none yet. Each lands from "
                           "spec-src/spec/mechanics/ with its fixture rows passing in feature_tests.exe.");
    }
    ImGui::End();
}

internal void
ui_draw_ledger(ui_state *ui, engine_api *api)
{
    ImGui::SetNextWindowSize(ImVec2(560, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Event ledger", &ui->show_ledger))
    {
        u32 count = api->ledger_count(api->engine);
        ImGui::Text("%u events", count);
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear"))
        {
            api->ledger_clear(api->engine);
            count = 0;
        }
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
            clipper.Begin((int)count);
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    sim_event ev = api->ledger_at(api->engine, (u32)row);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("%d", row);
                    ImGui::TableNextColumn(); ImGui::Text("%u", ev.turn);
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(sim_event_name(ev.kind));
                    ImGui::TableNextColumn(); ImGui::Text("%u", ev.subject);
                    ImGui::TableNextColumn(); ImGui::Text("%d", ev.delta);
                    ImGui::TableNextColumn(); ImGui::Text("%u", ev.cause);
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

internal void
ui_draw_memory(ui_state *ui, engine_api *api, ui_frame_info *frame)
{
    ImGui::SetNextWindowSize(ImVec2(420, 160), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Memory", &ui->show_memory))
    {
        engine_memory_stats stats = api->memory(api->engine);
        ui_draw_usage("permanent", stats.permanent_used, stats.permanent_size);
        ui_draw_usage("transient", stats.transient_used, stats.transient_size);
        ui_draw_usage("frame", frame->frame_arena_used, frame->frame_arena_size);
    }
    ImGui::End();
}

internal void
ui_draw(ui_state *ui, engine_api *api, ui_frame_info *frame)
{
    ui_draw_menu_bar(ui, api, frame);
    if (ui->show_demo)   ImGui::ShowDemoWindow(&ui->show_demo);
    if (ui->show_sim)    ui_draw_simulation(ui, api);
    if (ui->show_map)    ui_draw_map(ui, api);
    if (ui->show_ledger) ui_draw_ledger(ui, api);
    if (ui->show_memory) ui_draw_memory(ui, api, frame);
}
