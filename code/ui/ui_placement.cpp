/* M19 the 開發 dialog of the game mode and the placement mode
   (ui/city-commands.md, "開發 (develop)" and "Placement mode"). The dialog
   shows the grid of the facilities with the built count of each, the cost
   fields, the 執行武將 button and the three officer slots. 決定 sends no
   command. It closes the dialog and starts the placement mode. A panel at
   the top right shows the order. The 場所 arrows step through the empty
   slots of the city and frame each one. A left click on an empty slot of
   the city sends one command_develop. 返回 or a right click stops the
   placement and sends nothing. The debug layout keeps the combo
   dialog of ui_city.cpp. Included by ui.cpp after ui_officer_pick.cpp. */

#define UI_DEVELOP_GRID_COLUMNS 7       /* the cells of one row of the grid of the original */
#define UI_DEVELOP_CELL_WIDTH   100.0f

internal void ui_frame_hex(ui_state *ui, engine_api *api, hex_coord hex);                         /* ui_locate.cpp */
internal void ui_growth_result(ui_state *ui, const char *title_id, engine_command_result result);   /* ui_growth.cpp */

/* The built facilities of a city, counted by the facility id. */
internal void
ui_count_built_facilities(engine_api *api, u8 city_id, u32 *counts, u32 count_size)
{
    for (u32 index = 0; index < count_size; ++index) counts[index] = 0;
    engine_city_slots slots = api->city_slots(api->engine, city_id);
    for (u32 index = 0; index < slots.slot_count; ++index)
    {
        engine_facility slot = api->facility_at(api->engine, city_id, (u8)index);
        if (slot.state == SIM_SLOT_BUILT && slot.facility < count_size) counts[slot.facility]++;
    }
}

/* The grid of the facilities: the name with the built count, and the cost.
   A click selects the facility, and a frame of colour marks it. */
internal void
ui_draw_develop_grid(ui_state *ui, engine_api *api)
{
    u32 counts[256];
    ui_count_built_facilities(api, ui->develop.city, counts, ArrayCount(counts));
    ImGui::TextUnformatted(text_get(ui->text, "develop.facility"));
    u32 kind_count = api->facility_kind_count(api->engine);
    for (u32 index = 0; index < kind_count; ++index)
    {
        engine_facility_kind kind = api->facility_kind_at(api->engine, index);
        if (kind.id == ENGINE_NO_FACILITY) continue;
        char label[128];
        snprintf(label, sizeof(label), "%s (%u)\n%u###fac%u", ui_text_with_key(ui, "facility", kind.key), counts[kind.id], kind.cost, kind.id);
        if (index % UI_DEVELOP_GRID_COLUMNS != 0) ImGui::SameLine();
        bool selected = ui->develop.facility == kind.id;
        if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
        if (ImGui::Button(label, ImVec2(UI_DEVELOP_CELL_WIDTH, ImGui::GetTextLineHeightWithSpacing() * 2.2f))) ui->develop.facility = kind.id;
        if (selected) ImGui::PopStyleColor();
    }
}

/* The empty slot after (step 1) or before (step -1) the slot "from" in
   the order of the slots. After the last slot, the first slot follows.
   ENGINE_ANY_SLOT for no empty slot. */
internal u8
ui_placement_step_slot(engine_api *api, u8 city_id, u8 from, s32 step)
{
    engine_city_slots slots = api->city_slots(api->engine, city_id);
    s32 count = (s32)slots.slot_count;
    if (count == 0) return ENGINE_ANY_SLOT;
    s32 start = from == ENGINE_ANY_SLOT ? (step > 0 ? -1 : 0) : (s32)from;
    for (s32 tries = 1; tries <= count; ++tries)
    {
        s32 slot = ((start + step * tries) % count + count) % count;
        if (api->facility_at(api->engine, city_id, (u8)slot).state == SIM_SLOT_EMPTY) return (u8)slot;
    }
    return ENGINE_ANY_SLOT;
}

/* 決定 of the dialog: the order waits for the slot. */
internal void
ui_placement_start(ui_state *ui, engine_api *api)
{
    ui->placement.active   = true;
    ui->placement.city     = ui->develop.city;
    ui->placement.facility = ui->develop.facility;
    ui->placement.slot     = ui_placement_step_slot(api, ui->develop.city, ENGINE_ANY_SLOT, 1);
    for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place) ui->placement.officers[place] = ui->develop.officers[place];
    ui->placement.hover_valid = false;
    ui->develop.prepared_city = ENGINE_NO_CITY;   /* the next open proposes again */
    ui_context_close(ui);
    ui->show_develop = false;
}

/* The fields of the dialog: 資金 cost/stock, 期間, 空地 used/total, 行動力
   cost/available, or the refusal. */
internal void
ui_draw_develop_fields(ui_state *ui, engine_api *api, engine_check *check)
{
    b32 runs = check->refusal == SIM_REFUSAL_NONE;
    engine_city city = api->city_at(api->engine, ui->develop.city);
    engine_city_slots slots = api->city_slots(api->engine, ui->develop.city);
    ImGui::Text("%s: %u / %u   %s: %u%s   %s: %u / %u", text_get(ui->text, "command.gold"), runs ? check->gold : 0, city.gold,
                text_get(ui->text, "command.period"), runs ? check->days : 0, text_get(ui->text, "command.days"),
                text_get(ui->text, "develop.slot"), slots.used, slots.slot_count);
    u32 available = city.owner_force_id != ENGINE_NO_FACTION ? api->actions_left(api->engine, city.owner_force_id) : 0;
    ImGui::Text("%s: %u / %u", text_get(ui->text, "command.action_points"), runs ? check->action_points : 0, available);
    if (!runs) ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "%s: %s", text_get(ui->text, "develop.refused"), ui_refusal_text(ui, check->refusal));
}

/* The three officer slots: a portrait and the name under the 執行武將 button. */
internal void
ui_draw_officer_slots(ui_state *ui, engine_api *api, const u16 *officers, u32 places)
{
    f32 height = ImGui::GetTextLineHeightWithSpacing() * UI_SLOT_PORTRAIT_LINES;
    for (u32 place = 0; place < places; ++place)
    {
        ImGui::BeginGroup();
        ui_draw_officer_face(ui, api, officers[place], height);
        ImGui::NewLine();
        char name[UI_NAME_SIZE * 2] = "-";
        if (officers[place] != ENGINE_NO_OFFICER)
        {
            engine_officer officer = api->officer_by_id(api->engine, officers[place]);
            ui_officer_name(ui, &officer, name, sizeof(name));
        }
        ImGui::TextUnformatted(name);
        ImGui::EndGroup();
        if (place + 1 < places) ImGui::SameLine();
    }
}

internal void
ui_draw_develop_game(ui_state *ui, engine_api *api)
{
    ImGuiIO &io = ImGui::GetIO();
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), io.DisplaySize, IM_COL32(0, 0, 0, UI_DIALOG_DIM_ALPHA));
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.45f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    char title[128];
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "develop.title", "Develop"), &ui->show_develop, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ui_context_track(ui);
        if (ui->develop.prepared_city != ui->develop.city) ui_develop_reset_for_city(ui, api);
        ui_draw_develop_grid(ui, api);
        engine_command command = {};
        command.kind     = ENGINE_COMMAND_DEVELOP;
        command.city_id  = ui->develop.city;
        command.facility = ui->develop.facility;
        command.slot     = ENGINE_ANY_SLOT;   /* the placement chooses the slot */
        for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place) command.officers[place] = ui->develop.officers[place];
        engine_check check = api->can_issue(api->engine, &command);
        ImGui::Separator();
        ui_draw_develop_fields(ui, api, &check);
        char label[96];
        if (ImGui::Button(ui_label(label, sizeof(label), ui, "command.executor", "executor")))
        {
            ui_picker_open(ui, api, UI_PICKER_FOR_DEVELOP, ui->develop.city, SIM_DEVELOP_OFFICERS);
        }
        ui_draw_officer_slots(ui, api, ui->develop.officers, SIM_DEVELOP_OFFICERS);
        ImGui::BeginDisabled(check.refusal != SIM_REFUSAL_NONE);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.25f, 1.0f));
        b32 decide = ImGui::Button(ui_label(label, sizeof(label), ui, "command.decide", "decide"));
        ImGui::PopStyleColor();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.45f, 0.1f, 1.0f));
        if (ImGui::Button(ui_label(label, sizeof(label), ui, "command.back", "back"))) ui->show_develop = false;
        ImGui::PopStyleColor();
        if (decide) ui_placement_start(ui, api);
    }
    ImGui::End();
}

/* The hex of a slot, for the frame of the 場所 arrows. */
internal void
ui_placement_frame_slot(ui_state *ui, engine_api *api)
{
    if (ui->placement.slot == ENGINE_ANY_SLOT) return;
    engine_facility slot = api->facility_at(api->engine, ui->placement.city, ui->placement.slot);
    if (slot.city_id != ENGINE_NO_CITY) ui_frame_hex(ui, api, hex_make(slot.q, slot.r));
}

/* True if the hex is an empty slot of the city of the placement. */
internal b32
ui_placement_valid_hex(ui_state *ui, engine_api *api, hex_coord hex, u8 *slot_out)
{
    engine_hex data = api->hex_at(api->engine, hex.q, hex.r);
    if (data.slot_city != ui->placement.city) return false;
    if (api->facility_at(api->engine, data.slot_city, data.slot_index).state != SIM_SLOT_EMPTY) return false;
    *slot_out = data.slot_index;
    return true;
}

/* A click on the map while the placement waits. Returns false when no
   placement waits. A hex that is not an empty slot of the city does nothing. */
internal b32
ui_placement_click(ui_state *ui, engine_api *api, hex_coord hex)
{
    if (!ui->placement.active) return false;
    u8 slot = ENGINE_ANY_SLOT;
    if (!ui_placement_valid_hex(ui, api, hex, &slot)) return true;
    engine_command_result result = api->command_develop(api->engine, ui->placement.city, ui->placement.facility, ui->placement.officers, slot);
    ui_growth_result(ui, "develop.title", result);
    if (result.refusal == SIM_REFUSAL_NONE) ui->placement.active = false;
    return true;
}

/* The panel of the order at the top right. Its colour is green while the
   mouse is over an empty slot of the city, brown otherwise (assumed in the
   spec). */
internal void
ui_draw_placement(ui_state *ui, engine_api *api)
{
    if (!ui->placement.active) return;
    u8 slot_under = ENGINE_ANY_SLOT;
    ui->placement.hover_valid = ui->scene.has_hover && ui_placement_valid_hex(ui, api, ui->scene.hover, &slot_under);
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 8.0f, 8.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImVec4 colour = ui->placement.hover_valid ? ImVec4(0.10f, 0.35f, 0.15f, 0.95f) : ImVec4(0.35f, 0.24f, 0.12f, 0.95f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, colour);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;
    if (ImGui::Begin("###Placement", 0, flags))
    {
        engine_command command = {};
        command.kind     = ENGINE_COMMAND_DEVELOP;
        command.city_id  = ui->placement.city;
        command.facility = ui->placement.facility;
        command.slot     = ui->placement.slot;
        for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place) command.officers[place] = ui->placement.officers[place];
        engine_check check = api->can_issue(api->engine, &command);
        ImGui::Text("%s: %s", text_get(ui->text, "develop.facility"), ui_facility_name(ui, api, ui->placement.facility));
        ImGui::Text("%s: %u%s", text_get(ui->text, "command.period"), check.days, text_get(ui->text, "command.days"));
        ImGui::Text("%s: %u", text_get(ui->text, "placement.cost"), check.gold);
        ImGui::TextUnformatted(text_get(ui->text, "placement.officers"));
        for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place)
        {
            if (ui->placement.officers[place] == ENGINE_NO_OFFICER) continue;
            engine_officer officer = api->officer_by_id(api->engine, ui->placement.officers[place]);
            char name[UI_NAME_SIZE * 2];
            ui_officer_name(ui, &officer, name, sizeof(name));
            ImGui::Text("  %s", name);
        }
        ImGui::TextUnformatted(text_get(ui->text, "placement.site"));
        ImGui::SameLine();
        if (ImGui::ArrowButton("###slot_prev", ImGuiDir_Left))
        {
            ui->placement.slot = ui_placement_step_slot(api, ui->placement.city, ui->placement.slot, -1);
            ui_placement_frame_slot(ui, api);
        }
        ImGui::SameLine();
        if (ui->placement.slot == ENGINE_ANY_SLOT) ImGui::TextUnformatted("-");
        else                                        ImGui::Text("%u", ui->placement.slot);
        ImGui::SameLine();
        if (ImGui::ArrowButton("###slot_next", ImGuiDir_Right))
        {
            ui->placement.slot = ui_placement_step_slot(api, ui->placement.city, ui->placement.slot, 1);
            ui_placement_frame_slot(ui, api);
        }
        ImGui::TextUnformatted(text_get(ui->text, "placement.hint"));
        char label[96];
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.45f, 0.1f, 1.0f));
        if (ImGui::Button(ui_label(label, sizeof(label), ui, "command.back", "place_back"))) ui->placement.active = false;
        ImGui::PopStyleColor();
    }
    ImGui::End();
    ImGui::PopStyleColor();
}
