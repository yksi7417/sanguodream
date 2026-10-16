/* Debug UI panels. The only place ImGui is called from inside the DLL, and
   the only thing it does to the game is call engine_api. */

#include <stdio.h>   /* snprintf for labels */
#include "imgui.h"
#include "ui/ui.h"

/* A label for a window, a menu item or a column: the text of the id in the
   active locale, then "###" and a stable id. ImGui and the tests find the
   item by the stable id, so the locale does not change its ImGui id. */
internal const char *
ui_label(char *out, u32 out_size, ui_state *ui, const char *text_id, const char *stable_id)
{
    snprintf(out, out_size, "%s###%s", text_get(ui->text, text_id), stable_id);
    return out;
}

#include "ui/ui_map.cpp"
#include "ui/ui_world.cpp"
#include "ui/ui_settings.cpp"
#include "ui/ui_city.cpp"

/* Writes an overlay colour for one cell of the terrain view, if it is on the map. */
internal void
ui_scene_mark(u32 *overlay, engine_terrain_view view, hex_coord h, u32 rgba)
{
    hex_offset o = hex_to_offset(h);
    if (o.col < 0 || o.row < 0 || (u32)o.col >= view.width || (u32)o.row >= view.height) return;
    overlay[(u32)o.row * view.width + (u32)o.col] = rgba;
}

#include "ui/ui_scene.cpp"

internal void
ui_init(ui_state *ui, text_catalog *text)
{
    ui->show_demo   = true;
    ui->show_sim    = true;
    ui->show_ledger = true;
    ui->show_memory = false;
    ui->show_map    = false;     /* the 2D panel stays available from the Windows menu */
    ui->show_officers = true;
    ui->show_cities   = true;
    ui->show_scene    = true;
    ui->show_settings = false;
    ui->show_city     = true;
    ui->show_develop  = false;
    ui->show_raw      = false;
    ui->city.selected = 0;
    ui->develop = {};
    ui->develop.prepared_city = ENGINE_NO_CITY;
    ui->develop.slot          = ENGINE_ANY_SLOT;
    ui->text          = text;
    ui->pending_locale[0] = 0;
    ui->load_message[0] = 0;
    ui->names_message[0] = 0;
    ui_map_init(ui);
    ui_scene_init(ui);
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
        /* The debug panels keep English names; the game panels use the locale. */
        char label[128];
        if (ImGui::BeginMenu(ui_label(label, sizeof(label), ui, "menu.windows", "Windows")))
        {
            ImGui::MenuItem("Simulation", 0, &ui->show_sim);
            ImGui::MenuItem("Scene", 0, &ui->show_scene);
            ImGui::MenuItem("Hex map", 0, &ui->show_map);
            ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.officers", "Officers"), 0, &ui->show_officers);
            ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.cities", "Cities"), 0, &ui->show_cities);
            ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.city", "City"), 0, &ui->show_city);
            ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.develop", "Develop"), 0, &ui->show_develop);
            ImGui::MenuItem("Event ledger", 0, &ui->show_ledger);
            ImGui::MenuItem("Memory", 0, &ui->show_memory);
            ImGui::Separator();
            ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.settings", "Settings"), 0, &ui->show_settings);
            ImGui::MenuItem("ImGui demo", 0, &ui->show_demo);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        engine_date date = api->date(api->engine);
        ImGui::Text("turn %u  %u-%02u-%02u", api->turn(api->engine), date.year, date.month, date.day);
        ImGui::Separator();
        ImGui::Text("dll reloads %u", frame->dll_reload_count);
        ImGui::Separator();
        ImGui::Text("%.1f ms  %.0f fps", (f64)frame->dt_seconds * 1000.0, (f64)ImGui::GetIO().Framerate);
        ImGui::EndMainMenuBar();
    }
}

/* The date, the faction that acts, and the order of this turn. */
internal void
ui_draw_turn_status(ui_state *ui, engine_api *api)
{
    engine_date date = api->date(api->engine);
    ImGui::Text("Date: %u-%02u-%02u (%s), day %lld", date.year, date.month, date.day,
                calendar_season_name((calendar_season)date.season), (long long)date.days_since_start);
    u8 acting = api->whose_turn(api->engine);
    if (acting == ENGINE_NO_FACTION) ImGui::TextUnformatted("Acting: none (Advance turn moves the date)");
    else
    {
        ImGui::TextUnformatted("Acting:");
        ImGui::SameLine();
        ui_code_cell(ui, api, UI_CODE_FORCE, acting, ENGINE_NO_FACTION);
        ImGui::SameLine();
        ImGui::Text("(force %u), %u action points", acting, api->actions_left(api->engine, acting));
    }
    u32 factions = api->faction_count(api->engine);
    if (factions && ImGui::CollapsingHeader("Turn order"))
    {
        if (ImGui::BeginTable("turn_order", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("#");
            ImGui::TableSetupColumn("force");
            ImGui::TableSetupColumn("cities");
            ImGui::TableSetupColumn("officers");
            ImGui::TableSetupColumn("AP");
            ImGui::TableSetupColumn("player");
            ImGui::TableHeadersRow();
            for (u32 index = 0; index < factions; ++index)
            {
                engine_faction faction = api->faction_at(api->engine, index);
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%u%s", index, faction.acted ? " done" : "");
                ImGui::TableNextColumn(); ui_code_cell(ui, api, UI_CODE_FORCE, faction.force_id, ENGINE_NO_FACTION);
                ImGui::TableNextColumn(); ImGui::Text("%u", faction.city_count);
                ImGui::TableNextColumn(); ImGui::Text("%u", faction.officer_count);
                ImGui::TableNextColumn(); ImGui::Text("%u", faction.action_points);
                ImGui::TableNextColumn(); ImGui::TextUnformatted(faction.human ? "human" : "AI");
            }
            ImGui::EndTable();
        }
    }
}

internal void
ui_draw_simulation(ui_state *ui, engine_api *api)
{
    ImGui::SetNextWindowSize(ImVec2(380, 260), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Simulation", &ui->show_sim))
    {
        ImGui::Text("Turn: %u", api->turn(api->engine));
        ImGui::Text("Entities alive: %u / %u", api->entities_alive(api->engine), api->entity_capacity(api->engine));
        ui_draw_turn_status(ui, api);
        ImGui::Separator();
        /* "Advance turn" is the proceed button: the faction that acts ends its turn. */
        if (ImGui::Button("Advance turn"))
        {
            api->end_turn(api->engine);
        }
        ImGui::SameLine();
        if (ImGui::Button("Advance 12 turns"))
        {
            api->advance_turns(api->engine, 12);
        }
        ui_draw_load_sample_save(ui, api);
        ui_draw_load_city_names(ui, api);
        ImGui::Spacing();
        engine_content_status content = api->content_status(api->engine);
        ImGui::Text("Content pack: %s (%u facilities, %u cities, %u rules)", content.loaded ? "loaded" : content.detail,
                    content.facility_count, content.city_count, content.rule_count);
        ImGui::TextWrapped("Mechanics: calendar, turn order, action points, develop (M1, M2). "
                           "Amounts marked 'assumed' in the ledger are not from the spec.");
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
        if (ImGui::BeginTable("ledger", 8, flags))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableSetupColumn("turn", ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableSetupColumn("event");
            ImGui::TableSetupColumn("subject", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("delta", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("cause", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("faction", ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableSetupColumn("assumed", ImGuiTableColumnFlags_WidthFixed, 60.0f);
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
                    ImGui::TableNextColumn();
                    if (ev.faction == ENGINE_NO_FACTION) ImGui::TextUnformatted("-");
                    else                                 ImGui::Text("%u", ev.faction);
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(ev.assumed ? "assumed" : "");
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
    ui_apply_pending_locale(ui);
    ui_draw_menu_bar(ui, api, frame);
    if (ui->show_demo)   ImGui::ShowDemoWindow(&ui->show_demo);
    if (ui->show_sim)    ui_draw_simulation(ui, api);
    if (ui->show_scene)  ui_draw_scene_panel(ui, api);
    if (ui->show_map)    ui_draw_map(ui, api);
    if (ui->show_officers) ui_draw_officers(ui, api);
    if (ui->show_cities) ui_draw_cities(ui, api);
    if (ui->show_ledger) ui_draw_ledger(ui, api);
    if (ui->show_memory) ui_draw_memory(ui, api, frame);
    if (ui->show_settings) ui_draw_settings(ui);
    if (ui->show_city)    ui_draw_city(ui, api);
    if (ui->show_develop) ui_draw_develop(ui, api);

    /* The scene goes last: by now ImGui knows which windows want the mouse,
       and the panels above may have changed the camera or the selection. */
    if (ui->scene.enabled && frame->render)
    {
        if (!ui->scene.initialized) ui_scene_reset_camera(ui, api);
        ui_scene_handle_mouse(ui, api, frame->render->viewport_width, frame->render->viewport_height);
    }
    ui_scene_push(ui, api, frame);
}
