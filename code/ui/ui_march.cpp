/* The panels of M5 (march): the 出征 (sortie) and 輸送 (transport) dialogs,
   the unit panel, the unit command menu of ui/unit-commands.md (待命 and
   退卻 now; the other items are grey until M6), the mini map with the
   faction colours, and the units and cities on the Hex map panel. Every
   change of the game is one engine_api command. Included by ui.cpp. */

internal void ui_draw_supply(ui_state *ui, engine_api *api, engine_unit *transport);
internal b32  ui_combat_click(ui_state *ui, engine_api *api, hex_coord hex);                                       /* ui_combat.cpp */
internal void ui_start_pick(ui_state *ui, u8 pick, u8 row);
internal void ui_draw_map_combat(ui_state *ui, engine_api *api, ImDrawList *draw, f32 cell, hex_point origin);

/* ---- faction colours (placeholder palette; terrain-and-map.md, "Map display") */

global_variable const ImU32 ui_faction_palette[] = {
    IM_COL32(220, 60, 60, 255),  IM_COL32(60, 120, 230, 255), IM_COL32(60, 190, 90, 255),  IM_COL32(230, 200, 50, 255),
    IM_COL32(170, 80, 210, 255), IM_COL32(240, 140, 40, 255), IM_COL32(60, 200, 200, 255), IM_COL32(230, 110, 170, 255),
    IM_COL32(150, 110, 70, 255), IM_COL32(120, 160, 60, 255), IM_COL32(90, 90, 200, 255),  IM_COL32(200, 200, 200, 255),
};

internal ImU32
ui_faction_color(u8 force)
{
    if (force == ENGINE_NO_FACTION) return IM_COL32(120, 120, 120, 255);
    return ui_faction_palette[force % ArrayCount(ui_faction_palette)];
}

/* The same colour as an RGBA word of the render commands (ABGR in memory). */
internal u32
ui_faction_rgba(u8 force)
{
    return (u32)ui_faction_color(force);
}

/* ---- the 出征 and 輸送 dialogs --------------------------------------------- */

internal void
ui_march_reset(engine_api *api, ui_march_dialog *dialog, b32 transport)
{
    u8 type = 1;   /* the pike row; the proposal fills the officers */
    if (transport)
    {
        for (u32 index = 0; index < api->unit_type_count(api->engine); ++index)
        {
            if (api->unit_type_at(api->engine, index).transport) type = (u8)index;
        }
    }
    dialog->order = api->sortie_proposal(api->engine, dialog->city, type);
    dialog->order.unit_type = type;
    if (transport)
    {
        dialog->order.officers[1] = ENGINE_NO_OFFICER;
        dialog->order.officers[2] = ENGINE_NO_OFFICER;
        dialog->order.grain = 0;
    }
    dialog->soldiers = (int)dialog->order.soldiers;
    dialog->gold     = (int)dialog->order.gold;
    dialog->grain    = (int)dialog->order.grain;
    for (u32 index = 0; index < 4; ++index) dialog->cargo[index] = 0;
    dialog->prepared_city = dialog->city;
}

internal void
ui_draw_unit_type_combo(ui_state *ui, engine_api *api, ui_march_dialog *dialog)
{
    char label[128];
    engine_unit_type chosen = api->unit_type_at(api->engine, dialog->order.unit_type);
    const char *preview = chosen.id == ENGINE_NO_UNIT_TYPE ? "-" : ui_text_with_key(ui, "unit_type", chosen.key);
    if (ImGui::BeginCombo(ui_label(label, sizeof(label), ui, "sortie.unit_type", "unit_type"), preview))
    {
        for (u32 index = 0; index < api->unit_type_count(api->engine); ++index)
        {
            engine_unit_type type = api->unit_type_at(api->engine, index);
            if (type.transport) continue;
            snprintf(label, sizeof(label), "%s###u%u", ui_text_with_key(ui, "unit_type", type.key), type.id);
            if (ImGui::Selectable(label, type.id == dialog->order.unit_type)) dialog->order.unit_type = type.id;
        }
        ImGui::EndCombo();
    }
}

internal void
ui_draw_destination_combo(ui_state *ui, engine_api *api, ui_march_dialog *dialog)
{
    char label[128], preview[96];
    if (dialog->order.destination == ENGINE_NO_CITY) snprintf(preview, sizeof(preview), "-");
    else ui_city_label(ui, api, dialog->order.destination, preview, sizeof(preview));
    engine_city here = api->city_at(api->engine, dialog->city);
    if (ImGui::BeginCombo(ui_label(label, sizeof(label), ui, "transport.destination", "destination"), preview))
    {
        for (u32 index = 0; index < api->city_count(api->engine); ++index)
        {
            engine_city city = api->city_at(api->engine, index);
            if (city.owner_force_id != here.owner_force_id || city.city_id == dialog->city) continue;
            char name[96];
            ui_city_label(ui, api, city.city_id, name, sizeof(name));
            snprintf(label, sizeof(label), "%s###d%u", name, city.city_id);
            if (ImGui::Selectable(label, dialog->order.destination == city.city_id)) dialog->order.destination = city.city_id;
        }
        ImGui::EndCombo();
    }
}

internal void
ui_draw_amount(ui_state *ui, const char *text_id, const char *stable_id, int *value)
{
    char label[96];
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputInt(ui_label(label, sizeof(label), ui, text_id, stable_id), value, 100, 1000);
    if (*value < 0) *value = 0;
}

internal void
ui_draw_unit_values(ui_state *ui, engine_unit_values *values)
{
    ImGui::Text("%s %u  %s %u  %s %u  %s %u  %s %u", text_get(ui->text, "unit.attack"), values->attack, text_get(ui->text, "unit.defence"),
                values->defence, text_get(ui->text, "unit.intellect"), values->intellect, text_get(ui->text, "unit.build"), values->build,
                text_get(ui->text, "unit.move"), values->move);
}

/* 出征 (transport = false) or 輸送 (transport = true). */
internal void
ui_draw_march_dialog(ui_state *ui, engine_api *api, b32 transport)
{
    ui_march_dialog *dialog = transport ? &ui->transport : &ui->sortie;
    bool *show = transport ? &ui->show_transport : &ui->show_sortie;
    const char *title_id = transport ? "transport.title" : "sortie.title";
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(480, 440), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, title_id, transport ? "Transport" : "Sortie"), show))
    {
        if (dialog->prepared_city != dialog->city) ui_march_reset(api, dialog, transport);
        dialog->order.city_id = dialog->city;
        char city_name[96], label[96];
        ui_city_label(ui, api, dialog->city, city_name, sizeof(city_name));
        ImGui::Text("%s: %s", text_get(ui->text, "city.select"), city_name);
        if (transport) ui_draw_destination_combo(ui, api, dialog);
        else           ui_draw_unit_type_combo(ui, api, dialog);
        if (ImGui::Button(ui_label(label, sizeof(label), ui, "sortie.auto", "auto")))
        {
            u8 destination = dialog->order.destination;
            u8 type = dialog->order.unit_type;
            ui_march_reset(api, dialog, transport);
            dialog->order.destination = destination;
            if (!transport) dialog->order.unit_type = type;
            engine_sortie_order proposal = api->sortie_proposal(api->engine, dialog->city, dialog->order.unit_type);
            for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place) dialog->order.officers[place] = proposal.officers[place];
            if (!transport) { dialog->soldiers = (int)proposal.soldiers; dialog->grain = (int)proposal.grain; }
        }
        ImGui::SameLine();
        ImGui::Button(ui_label(label, sizeof(label), ui, "command.executor", "executor"));
        u16 free_ids[UI_DEVELOP_OFFICER_LIST];
        u32 free_count = ui_free_officers(api, dialog->city, free_ids, ArrayCount(free_ids));
        u32 places = transport ? 1 : SIM_COMMAND_OFFICERS;
        for (u32 place = 0; place < places; ++place)
        {
            ui_draw_growth_officer(ui, api, dialog->order.officers + place, place, CONTENT_ABILITY_LEADERSHIP, free_ids, free_count);
        }
        ui_draw_amount(ui, "sortie.soldiers", "soldiers", &dialog->soldiers);
        ui_draw_amount(ui, "sortie.gold", "gold", &dialog->gold);
        ui_draw_amount(ui, "sortie.grain", "grain", &dialog->grain);
        if (transport)
        {
            for (u32 index = 0; index < 4; ++index)
            {
                engine_weapon_kind weapon = api->weapon_kind_at(api->engine, index);
                char stable[16];
                snprintf(stable, sizeof(stable), "cargo%u", index);
                ImGui::SetNextItemWidth(160.0f);
                snprintf(label, sizeof(label), "%s###%s", ui_text_with_key(ui, "weapon", weapon.key), stable);
                ImGui::InputInt(label, dialog->cargo + index, 100, 1000);
                if (dialog->cargo[index] < 0) dialog->cargo[index] = 0;
            }
        }
        dialog->order.soldiers = (u32)dialog->soldiers;
        dialog->order.gold     = (u32)dialog->gold;
        dialog->order.grain    = (u32)dialog->grain;
        for (u32 index = 0; index < CITY_EQUIPMENT_STOCK_COUNT; ++index) dialog->order.cargo[index] = 0;
        for (u32 index = 0; transport && index < 4; ++index)
        {
            engine_weapon_kind weapon = api->weapon_kind_at(api->engine, index);
            if (weapon.stock < CITY_EQUIPMENT_STOCK_COUNT) dialog->order.cargo[weapon.stock] = (u32)dialog->cargo[index];
        }

        engine_city city = api->city_at(api->engine, dialog->city);
        if (!transport)
        {
            engine_unit_values values = api->unit_values(api->engine, city.owner_force_id, dialog->order.unit_type, dialog->order.officers);
            ui_draw_unit_values(ui, &values);
        }
        engine_command command = {};
        command.kind   = transport ? ENGINE_COMMAND_TRANSPORT : ENGINE_COMMAND_SORTIE;
        command.sortie = dialog->order;
        engine_check check = api->can_issue(api->engine, &command);
        b32 runs = check.refusal == SIM_REFUSAL_NONE;
        ImGui::Separator();
        ImGui::Text("%s: %u / %u", text_get(ui->text, "command.action_points"), runs ? check.action_points : 0,
                    city.owner_force_id != ENGINE_NO_FACTION ? api->actions_left(api->engine, city.owner_force_id) : 0);
        if (!runs) ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "%s: %s", text_get(ui->text, "develop.refused"), ui_refusal_text(ui, check.refusal));
        if (ui_draw_decide_back(ui, runs, show))
        {
            engine_command_result result = transport ? api->command_transport(api->engine, &dialog->order)
                                                     : api->command_sortie(api->engine, &dialog->order);
            ui_growth_result(ui, title_id, result);
            if (result.refusal == SIM_REFUSAL_NONE)
            {
                dialog->prepared_city = ENGINE_NO_CITY;
                *show = false;
            }
        }
    }
    ImGui::End();
}

/* The buttons of the City panel that open 出征 and 輸送 for its city. */
internal void
ui_draw_march_buttons(ui_state *ui, engine_api *api, u16 first_officer)
{
    if (ui_city_button(ui, "city.sortie", "open_sortie",
                       ui_probe_city_command(api, ui->city.selected, ENGINE_COMMAND_SORTIE, first_officer)))
    {
        ui->show_sortie = true;
        ui->sortie.city = ui->city.selected;
    }
    ImGui::SameLine();
    if (ui_city_button(ui, "city.transport", "open_transport",
                       ui_probe_city_command(api, ui->city.selected, ENGINE_COMMAND_TRANSPORT, first_officer)))
    {
        ui->show_transport = true;
        ui->transport.city = ui->city.selected;
    }
}

/* ---- the selected unit ------------------------------------------------------ */

internal const char *
ui_grade_text(u32 aptitude)
{
    static const char *letters[4] = { "C", "B", "A", "S" };
    return aptitude < 4 ? letters[aptitude] : "-";
}

internal void
ui_unit_label(ui_state *ui, engine_api *api, engine_unit *unit, char *out, u32 out_size)
{
    engine_officer commander = api->officer_by_id(api->engine, unit->officers[0]);
    if (commander.officer_id == ENGINE_NO_OFFICER)
    {
        snprintf(out, out_size, "%s %u", text_get(ui->text, "unit.title"), unit->id);
        return;
    }
    char name[UI_NAME_SIZE * 2];
    ui_officer_name(ui, &commander, name, sizeof(name));
    snprintf(out, out_size, "%s%s", name, text_get(ui->text, "unit.suffix"));
}

/* Selects a unit: the Unit panel shows it, the map shows its range. */
internal void
ui_select_unit(ui_state *ui, u32 unit_id)
{
    ui->units.selected      = unit_id;
    ui->units.supply_target = ENGINE_NO_UNIT;
    ui->show_unit           = unit_id != ENGINE_NO_UNIT;
    ui->show_unit_menu      = false;
}

/* A click on the map with a unit of the faction that acts selected: a hex
   of the range moves it there (one command_move), then the command menu
   opens; its own hex opens the menu. Returns true if the click was for the
   unit. */
internal b32
ui_unit_click(ui_state *ui, engine_api *api, hex_coord hex)
{
    if (ui_combat_click(ui, api, hex)) return true;   /* a target of 攻擊 .. 撤除 (ui_combat.cpp) */
    u32 on_hex = api->unit_at_hex(api->engine, hex.q, hex.r);
    u8 acting = api->whose_turn(api->engine);
    if (on_hex != ENGINE_NO_UNIT && on_hex != ui->units.selected && api->unit_at(api->engine, on_hex).force_id == acting)
    {
        ui_select_unit(ui, on_hex);
        return true;
    }
    if (ui->units.selected == ENGINE_NO_UNIT) return false;
    engine_unit unit = api->unit_at(api->engine, ui->units.selected);
    if (unit.id == ENGINE_NO_UNIT) { ui_select_unit(ui, ENGINE_NO_UNIT); return false; }
    if (unit.q == hex.q && unit.r == hex.r)
    {
        ui->show_unit_menu = true;
        return true;
    }
    engine_reach_view reach = api->reachable(api->engine, unit.id);
    for (u32 index = 1; index < reach.count; ++index)
    {
        if (reach.cells[index].q != hex.q || reach.cells[index].r != hex.r) continue;
        engine_command_result result = api->command_move(api->engine, unit.id, hex.q, hex.r);
        ui_growth_result(ui, "unit.move_done", result);
        snprintf(ui->units.message, sizeof(ui->units.message), "%s", ui->command_message);
        if (result.refusal == SIM_REFUSAL_NONE) ui->show_unit_menu = api->unit_at(api->engine, unit.id).id != ENGINE_NO_UNIT;
        return true;
    }
    ui_select_unit(ui, ENGINE_NO_UNIT);
    return false;
}

internal void
ui_draw_unit_panel(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(300, 380), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "unit.title", "Unit"), &ui->show_unit))
    {
        engine_unit unit = api->unit_at(api->engine, ui->units.selected);
        if (unit.id == ENGINE_NO_UNIT)
        {
            ImGui::TextUnformatted(text_get(ui->text, "unit.none"));
            ImGui::End();
            return;
        }
        char name[128];
        ui_unit_label(ui, api, &unit, name, sizeof(name));
        ImGui::TextUnformatted(name);
        ImGui::Text("%s:", text_get(ui->text, "column.owner"));
        ImGui::SameLine();
        ui_code_cell(ui, api, UI_CODE_FORCE, unit.force_id, ENGINE_NO_FACTION);
        for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
        {
            ImGui::Text("%s:", text_get(ui->text, place == 0 ? "unit.commander" : "unit.deputy"));
            ImGui::SameLine();
            ui_code_cell(ui, api, UI_CODE_OFFICER, unit.officers[place], UI_NO_ID_16);
        }
        ImGui::Text("%s %u  %s %u", text_get(ui->text, "command.gold"), unit.gold, text_get(ui->text, "command.grain"), unit.grain);
        ImGui::Text("%s %u  %s %u", text_get(ui->text, "command.soldiers"), unit.soldiers, text_get(ui->text, "command.morale"), unit.morale);
        engine_unit_type type = api->unit_type_at(api->engine, unit.unit_type);
        ImGui::Text("%s: %s (%s)", text_get(ui->text, "unit.class"), type.id == ENGINE_NO_UNIT_TYPE ? "-" : ui_text_with_key(ui, "unit_type", type.key),
                    ui_grade_text(unit.values.aptitude));
        ui_draw_unit_values(ui, &unit.values);
        if (unit.state)
        {
            char state_id[32];
            snprintf(state_id, sizeof(state_id), "unit.state_%u", unit.state);
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "%s (%u)", text_get(ui->text, state_id), unit.state_turns);
        }
        if (ui->combat.pick != UI_PICK_NONE)
        {
            char cancel[64];
            if (ImGui::Button(ui_label(cancel, sizeof(cancel), ui, "unit.pick_cancel", "pick_cancel"))) ui->combat.pick = UI_PICK_NONE;
        }
        if (ui->units.message[0]) ImGui::TextUnformatted(ui->units.message);
        if (type.transport) ui_draw_supply(ui, api, &unit);
    }
    ImGui::End();
}

/* 補給 in the Unit panel of a transport unit: an adjacent unit of the
   faction, and the amounts. */
internal void
ui_draw_supply(ui_state *ui, engine_api *api, engine_unit *transport)
{
    char label[128], preview[96] = "-";
    ImGui::Separator();
    if (ui->units.supply_target != ENGINE_NO_UNIT)
    {
        engine_unit target = api->unit_at(api->engine, ui->units.supply_target);
        if (target.id != ENGINE_NO_UNIT) ui_unit_label(ui, api, &target, preview, sizeof(preview));
    }
    if (ImGui::BeginCombo(ui_label(label, sizeof(label), ui, "unit.supply_target", "supply_target"), preview))
    {
        for (u32 id = 0; id < api->unit_count(api->engine); ++id)
        {
            engine_unit other = api->unit_at(api->engine, id);
            if (other.id == ENGINE_NO_UNIT || other.id == transport->id || other.force_id != transport->force_id) continue;
            if (hex_distance(hex_make(other.q, other.r), hex_make(transport->q, transport->r)) != 1) continue;
            char name[96];
            ui_unit_label(ui, api, &other, name, sizeof(name));
            snprintf(label, sizeof(label), "%s###s%u", name, other.id);
            if (ImGui::Selectable(label, ui->units.supply_target == other.id)) ui->units.supply_target = other.id;
        }
        ImGui::EndCombo();
    }
    ui_draw_amount(ui, "sortie.soldiers", "supply_soldiers", &ui->units.supply_soldiers);
    ui_draw_amount(ui, "sortie.gold", "supply_gold", &ui->units.supply_gold);
    ui_draw_amount(ui, "sortie.grain", "supply_grain", &ui->units.supply_grain);
    engine_command command = {};
    command.kind        = ENGINE_COMMAND_SUPPLY;
    command.unit        = transport->id;
    command.target_unit = ui->units.supply_target;
    command.soldiers    = (u32)ui->units.supply_soldiers;
    command.gold        = (u32)ui->units.supply_gold;
    command.grain       = (u32)ui->units.supply_grain;
    engine_check check = api->can_issue(api->engine, &command);
    ImGui::BeginDisabled(check.refusal != SIM_REFUSAL_NONE);
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "unit.supply", "supply")))
    {
        engine_command_result result = api->command_supply(api->engine, command.unit, command.target_unit, command.soldiers, command.gold, command.grain);
        ui_growth_result(ui, "unit.supply", result);
        snprintf(ui->units.message, sizeof(ui->units.message), "%s", ui->command_message);
    }
    ImGui::EndDisabled();
    if (check.refusal != SIM_REFUSAL_NONE) ImGui::TextDisabled("%s", ui_refusal_text(ui, check.refusal));
}

/* The pick of a menu item of M6, or UI_PICK_NONE. */
internal u8
ui_menu_pick(const char *item)
{
    static const char *names[UI_PICK_COUNT] = { "", "attack", "joint", "tactic", "plot", "build", "repair", "dismantle", "duel" };
    for (u8 pick = 1; pick < UI_PICK_COUNT; ++pick)
    {
        if (strcmp(item, names[pick]) == 0) return pick;
    }
    return UI_PICK_NONE;
}

/* The unit command menu (ui/unit-commands.md): the items of a combat unit
   or of a transport unit, in the order of the manual. 待命 and 退卻 run now;
   補給 opens the Unit panel of the transport unit; 攻擊, 齊攻, 修補 and 撤除
   wait for a target hex; 戰法, 計略 and 設置 open the Actions list first.
   單挑 (M7) waits for the hex of the enemy unit. */
internal void
ui_draw_unit_menu(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(150, 370), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "unit.menu", "UnitMenu"), &ui->show_unit_menu))
    {
        engine_unit unit = api->unit_at(api->engine, ui->units.selected);
        if (unit.id == ENGINE_NO_UNIT)
        {
            ui->show_unit_menu = false;
            ImGui::End();
            return;
        }
        engine_unit_type type = api->unit_type_at(api->engine, unit.unit_type);
        static const char *combat_items[]    = { "wait", "attack", "joint", "tactic", "duel", "plot", "build", "repair", "dismantle", "retreat" };
        static const char *transport_items[] = { "wait", "attack", "plot", "build", "repair", "dismantle", "supply", "retreat" };
        const char **items = type.transport ? transport_items : combat_items;
        u32 count = type.transport ? ArrayCount(transport_items) : ArrayCount(combat_items);
        char label[96], text_id[32];
        for (u32 index = 0; index < count; ++index)
        {
            const char *item = items[index];
            b32 wait = strcmp(item, "wait") == 0, retreat = strcmp(item, "retreat") == 0, supply = strcmp(item, "supply") == 0;
            u8 pick = ui_menu_pick(item);
            engine_command command = {};
            command.kind = wait ? ENGINE_COMMAND_WAIT : ENGINE_COMMAND_RETREAT;
            command.unit = unit.id;
            b32 enabled = supply || pick != UI_PICK_NONE ||
                          ((wait || retreat) && api->can_issue(api->engine, &command).refusal == SIM_REFUSAL_NONE);
            if (pick != UI_PICK_NONE && (unit.acted || unit.state != 0)) enabled = false;
            snprintf(text_id, sizeof(text_id), "unit_menu.%s", item);
            ImGui::BeginDisabled(!enabled);
            if (ImGui::Button(ui_label(label, sizeof(label), ui, text_id, item), ImVec2(120.0f, 0.0f)))
            {
                if (pick == UI_PICK_TACTIC || pick == UI_PICK_PLOT || pick == UI_PICK_BUILD)
                {
                    ui->combat.list    = pick;
                    ui->show_actions   = true;
                    ui->show_unit_menu = false;
                }
                else if (pick != UI_PICK_NONE)
                {
                    ui_start_pick(ui, pick, 0);
                }
                else if (supply)
                {
                    ui->show_unit = true;
                }
                else
                {
                    engine_command_result result = wait ? api->command_wait(api->engine, unit.id) : api->command_retreat(api->engine, unit.id);
                    ui_growth_result(ui, text_id, result);
                    snprintf(ui->units.message, sizeof(ui->units.message), "%s", ui->command_message);
                    ui->show_unit_menu = false;
                    if (retreat && result.refusal == SIM_REFUSAL_NONE) ui_select_unit(ui, ENGINE_NO_UNIT);
                }
            }
            ImGui::EndDisabled();
        }
    }
    ImGui::End();
}

/* ---- the map overlays -------------------------------------------------------- */

/* Draws the cities and the units on the Hex map panel, and the range of the
   selected unit (green outlines; its hex cyan; ui/unit-commands.md). */
internal void
ui_draw_map_units(ui_state *ui, engine_api *api, ImDrawList *draw, f32 cell, hex_point origin)
{
    if (ui->units.selected != ENGINE_NO_UNIT)
    {
        engine_reach_view reach = api->reachable(api->engine, ui->units.selected);
        for (u32 index = 0; index < reach.count; ++index)
        {
            hex_point center = hex_to_pixel(reach.cells[index], cell, origin);
            ImU32 color = index == 0 ? IM_COL32(60, 220, 230, 255) : IM_COL32(80, 240, 80, 255);
            ui_draw_hexagon(draw, center, cell - 1.5f, IM_COL32(0, 0, 0, 0), color);
        }
    }
    ui_draw_map_combat(ui, api, draw, cell, origin);
    for (u32 index = 0; index < api->city_count(api->engine); ++index)
    {
        engine_city city = api->city_at(api->engine, index);
        engine_city_slots slots = api->city_slots(api->engine, city.city_id);
        hex_point center = hex_to_pixel(hex_make(slots.q, slots.r), cell, origin);
        f32 half = cell * 0.6f;
        draw->AddRectFilled(ImVec2(center.x - half, center.y - half), ImVec2(center.x + half, center.y + half), ui_faction_color(city.owner_force_id));
        draw->AddRect(ImVec2(center.x - half, center.y - half), ImVec2(center.x + half, center.y + half), IM_COL32(20, 20, 20, 255));
    }
    for (u32 id = 0; id < api->unit_count(api->engine); ++id)
    {
        engine_unit unit = api->unit_at(api->engine, id);
        if (unit.id == ENGINE_NO_UNIT) continue;
        hex_point center = hex_to_pixel(hex_make(unit.q, unit.r), cell, origin);
        f32 half = cell * 0.55f;
        ImVec2 diamond[4] = { ImVec2(center.x, center.y - half), ImVec2(center.x + half, center.y), ImVec2(center.x, center.y + half),
                              ImVec2(center.x - half, center.y) };
        draw->AddConvexPolyFilled(diamond, 4, ui_faction_color(unit.force_id));
        draw->AddPolyline(diamond, 4, unit.id == ui->units.selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(20, 20, 20, 255),
                          ImDrawFlags_Closed, 1.5f);
    }
}

