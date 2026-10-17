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
#include "ui/ui_city_commands.cpp"
#include "ui/ui_economy.cpp"
#include "ui/ui_growth.cpp"
#include "ui/ui_march.cpp"
#include "ui/ui_combat.cpp"
#include "ui/ui_court.cpp"

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
    ui->show_economy          = false;
    for (u32 kind = 0; kind < UI_COMMAND_COUNT; ++kind)
    {
        ui->show_command[kind] = false;
        ui->commands[kind] = {};
        ui->commands[kind].prepared_city = ENGINE_NO_CITY;
        ui->commands[kind].amount        = UI_MERCHANT_DEFAULT_AMOUNT;
    }
    ui->command_message[0]    = 0;
    ui->show_research         = false;
    ui->research              = {};
    ui->research.prepared_city = ENGINE_NO_CITY;
    ui->research.technique    = ENGINE_NO_TECHNIQUE;
    ui->show_merge            = false;
    ui->merge                 = {};
    ui->merge.prepared_city   = ENGINE_NO_CITY;
    ui->merge.absorber        = ENGINE_ANY_SLOT;
    ui->merge.absorbed        = ENGINE_ANY_SLOT;
    ui->show_techniques       = false;
    ui->show_sortie           = false;
    ui->show_transport        = false;
    ui->sortie                = {};
    ui->transport             = {};
    ui->sortie.prepared_city    = ENGINE_NO_CITY;
    ui->transport.prepared_city = ENGINE_NO_CITY;
    ui->show_unit             = false;
    ui->show_unit_menu        = false;
    ui->show_minimap          = false;
    ui->units                 = {};
    ui->units.selected        = ENGINE_NO_UNIT;
    ui->units.supply_target   = ENGINE_NO_UNIT;
    ui->show_court            = false;
    ui->show_diplomacy        = false;
    ui->show_duel             = false;
    ui->show_debate           = false;
    ui->court                 = {};
    ui->court.officer = ui->court.target = ui->court.partner = ui->court.captive = ENGINE_NO_OFFICER;
    ui->court.post            = ENGINE_NO_ROW;
    ui->court.item            = ENGINE_NO_ROW;
    ui->diplomacy             = {};
    ui->diplomacy.force       = ENGINE_NO_FACTION;
    ui->diplomacy.second      = ENGINE_NO_FACTION;
    ui->diplomacy.city        = ENGINE_NO_CITY;
    ui->diplomacy.captive     = ENGINE_NO_OFFICER;
    ui->diplomacy.envoy       = ENGINE_NO_OFFICER;
    ui->diplomacy.gold        = 1000;
    ui->diplomacy.months      = 6;
    ui->ledger_city           = ENGINE_ANY_CITY;
    ui->ledger_resource       = ENGINE_ANY_RESOURCE;
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
            ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.economy", "Economy"), 0, &ui->show_economy);
            ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.techniques", "Techniques"), 0, &ui->show_techniques);
            ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.unit", "Unit"), 0, &ui->show_unit);
            ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.minimap", "MiniMap"), 0, &ui->show_minimap);
        ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.court", "Court"), 0, &ui->show_court);
        ImGui::MenuItem(ui_label(label, sizeof(label), ui, "menu.diplomacy", "Diplomacy"), 0, &ui->show_diplomacy);
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
        ui_draw_outcome(ui, api);
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

/* The filters of the ledger: a city and a resource. A choice changes only
   the front end; the panel then asks the query ledger_filter. */
internal void
ui_draw_ledger_filters(ui_state *ui, engine_api *api)
{
    char label[128], preview[96];
    if (ui->ledger_city == ENGINE_ANY_CITY) snprintf(preview, sizeof(preview), "%s", text_get(ui->text, "ledger.any"));
    else                                    ui_city_label(ui, api, ui->ledger_city, preview, sizeof(preview));
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::BeginCombo(ui_label(label, sizeof(label), ui, "ledger.city", "ledger_city"), preview))
    {
        snprintf(label, sizeof(label), "%s###lc_any", text_get(ui->text, "ledger.any"));
        if (ImGui::Selectable(label, ui->ledger_city == ENGINE_ANY_CITY)) ui->ledger_city = ENGINE_ANY_CITY;
        u32 count = api->city_count(api->engine);
        for (u32 index = 0; index < count; ++index)
        {
            engine_city city = api->city_at(api->engine, index);
            char name[96];
            ui_city_label(ui, api, city.city_id, name, sizeof(name));
            snprintf(label, sizeof(label), "%s###lc%u", name, city.city_id);
            if (ImGui::Selectable(label, ui->ledger_city == city.city_id)) ui->ledger_city = city.city_id;
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ui->ledger_resource == ENGINE_ANY_RESOURCE) snprintf(preview, sizeof(preview), "%s", text_get(ui->text, "ledger.any"));
    else snprintf(preview, sizeof(preview), "%s", ui_text_with_key(ui, "resource", sim_resource_name((sim_resource)ui->ledger_resource)));
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::BeginCombo(ui_label(label, sizeof(label), ui, "ledger.resource", "ledger_resource"), preview))
    {
        snprintf(label, sizeof(label), "%s###lr_any", text_get(ui->text, "ledger.any"));
        if (ImGui::Selectable(label, ui->ledger_resource == ENGINE_ANY_RESOURCE)) ui->ledger_resource = ENGINE_ANY_RESOURCE;
        for (u32 resource = SIM_RESOURCE_GOLD; resource < SIM_RESOURCE_COUNT; ++resource)
        {
            snprintf(label, sizeof(label), "%s###lr%u", ui_text_with_key(ui, "resource", sim_resource_name((sim_resource)resource)), resource);
            if (ImGui::Selectable(label, ui->ledger_resource == resource)) ui->ledger_resource = (u8)resource;
        }
        ImGui::EndCombo();
    }
}

internal void
ui_draw_ledger(ui_state *ui, engine_api *api)
{
    ImGui::SetNextWindowSize(ImVec2(640, 300), ImGuiCond_FirstUseEver);
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
        ImGui::SameLine();
        ui_draw_ledger_filters(ui, api);
        b32 filtered = ui->ledger_city != ENGINE_ANY_CITY || ui->ledger_resource != ENGINE_ANY_RESOURCE;
        engine_ledger_view view = {};
        if (filtered)
        {
            view  = api->ledger_filter(api->engine, ui->ledger_city, ui->ledger_resource);
            count = view.count;
        }
        ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
        if (ImGui::BeginTable("ledger", 9, flags))
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
            ImGui::TableSetupColumn("detail", ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableHeadersRow();
            ImGuiListClipper clipper;
            clipper.Begin((int)count);
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    u32 index = filtered ? view.indices[row] : (u32)row;
                    sim_event ev = api->ledger_at(api->engine, index);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("%u", index);
                    ImGui::TableNextColumn(); ImGui::Text("%u", ev.turn);
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(sim_event_name(ev.kind));
                    ImGui::TableNextColumn(); ImGui::Text("%u", ev.subject);
                    ImGui::TableNextColumn(); ImGui::Text("%d", ev.delta);
                    ImGui::TableNextColumn(); ImGui::Text("%u", ev.cause);
                    ImGui::TableNextColumn();
                    if (ev.faction == ENGINE_NO_FACTION) ImGui::TextUnformatted("-");
                    else                                 ImGui::Text("%u", ev.faction);
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(ev.assumed ? "assumed" : "");
                    ImGui::TableNextColumn(); ImGui::Text("%u", ev.detail);
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
    if (ui->show_settings) ui_draw_settings(ui, api);
    if (ui->show_city)    ui_draw_city(ui, api);
    if (ui->show_develop) ui_draw_develop(ui, api);
    if (ui->show_economy) ui_draw_economy(ui, api);
    if (ui->show_research)   ui_draw_research(ui, api);
    if (ui->show_merge)      ui_draw_merge(ui, api);
    if (ui->show_techniques) ui_draw_techniques(ui, api);
    if (ui->show_sortie)     ui_draw_march_dialog(ui, api, false);
    if (ui->show_transport)  ui_draw_march_dialog(ui, api, true);
    if (ui->show_unit)       ui_draw_unit_panel(ui, api);
    if (ui->show_unit_menu)  ui_draw_unit_menu(ui, api);
    if (ui->show_minimap)    ui_draw_minimap(ui, api);
    if (ui->show_actions)    ui_draw_actions(ui, api);
    if (ui->show_court)      ui_draw_court(ui, api);
    if (ui->show_diplomacy)  ui_draw_diplomacy(ui, api);
    ui_draw_mini_games(ui, api);   /* the duel and debate screens open when one runs */
    for (u32 kind = 0; kind < UI_COMMAND_COUNT; ++kind)
    {
        if (ui->show_command[kind]) ui_draw_city_command(ui, api, kind);
    }

    /* The scene goes last: by now ImGui knows which windows want the mouse,
       and the panels above may have changed the camera or the selection. */
    if (ui->scene.enabled && frame->render)
    {
        if (!ui->scene.initialized) ui_scene_reset_camera(ui, api);
        ui_scene_handle_mouse(ui, api, frame->render->viewport_width, frame->render->viewport_height);
    }
    ui_scene_push(ui, api, frame);
}
