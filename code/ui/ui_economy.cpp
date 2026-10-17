/* The Economy panel (M3): one row for each city of the faction that acts,
   with the stocks, the income of the next month and season, the salaries,
   the grain that the soldiers eat each turn, and the values that the city
   commands change. It reads only the queries city_at, city_income_forecast,
   city_economy and technique_points. Included by ui.cpp. */

internal void
ui_draw_economy_row(ui_state *ui, engine_api *api, u8 city_id)
{
    engine_income_forecast forecast = api->city_income_forecast(api->engine, city_id);
    engine_city_economy economy = api->city_economy(api->engine, city_id);
    sim_income_forecast *amounts = &forecast.amounts;
    char name[96];
    ui_city_label(ui, api, city_id, name, sizeof(name));
    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::TextUnformatted(name);
    ImGui::TableNextColumn(); ImGui::Text("%u / %u", economy.gold, economy.gold_cap);
    ImGui::TableNextColumn(); ImGui::Text("%u%s", amounts->gold_month, amounts->gold_every_turn ? " *" : "");
    ImGui::TableNextColumn(); ImGui::Text("%u", amounts->salaries);
    ImGui::TableNextColumn(); ImGui::Text("%u / %u", economy.grain, economy.grain_cap);
    ImGui::TableNextColumn(); ImGui::Text("%u%s", amounts->grain_season, amounts->grain_every_month ? " *" : "");
    ImGui::TableNextColumn(); ImGui::Text("%u", amounts->grain_per_turn);
    ImGui::TableNextColumn(); ImGui::Text("%u (%u%%)%s", economy.public_order, amounts->order_percent,
                                          economy.bandits_blocked ? "" : " !");
    ImGui::TableNextColumn(); ImGui::Text("%u", economy.morale);
    ImGui::TableNextColumn(); ImGui::Text("%u / %u", economy.soldiers, economy.soldier_cap);
}

internal void
ui_draw_economy(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(820, 320), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "economy.title", "Economy"), &ui->show_economy))
    {
        u8 force = api->whose_turn(api->engine);
        if (force != ENGINE_NO_FACTION)
        {
            ImGui::Text("%s:", text_get(ui->text, "column.owner"));
            ImGui::SameLine();
            ui_code_cell(ui, api, UI_CODE_FORCE, force, ENGINE_NO_FACTION);
            ImGui::SameLine();
            ImGui::Text("  %s: %u", text_get(ui->text, "economy.technique"), api->technique_points(api->engine, force));
        }
        ImGui::TextWrapped("%s", text_get(ui->text, "economy.help"));
        ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
        if (ImGui::BeginTable("economy", 10, flags))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ui_table_column(ui, "city.select", "city");
            ui_table_column(ui, "column.gold", "gold");
            ui_table_column(ui, "city.gold_income", "gold_income");
            ui_table_column(ui, "economy.salaries", "salaries");
            ui_table_column(ui, "column.food", "grain");
            ui_table_column(ui, "city.grain_income", "grain_income");
            ui_table_column(ui, "economy.grain_per_turn", "grain_per_turn");
            ui_table_column(ui, "command.order", "order");
            ui_table_column(ui, "command.morale", "morale");
            ui_table_column(ui, "command.soldiers", "soldiers");
            ImGui::TableHeadersRow();
            u32 count = api->city_count(api->engine);
            for (u32 index = 0; index < count; ++index)
            {
                engine_city city = api->city_at(api->engine, index);
                if (city.owner_force_id == ENGINE_NO_FACTION) continue;
                if (force != ENGINE_NO_FACTION && city.owner_force_id != force) continue;
                ui_draw_economy_row(ui, api, city.city_id);
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}
