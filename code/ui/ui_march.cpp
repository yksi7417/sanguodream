/* The panels of M5 (march): the 出征 (sortie) and 輸送 (transport) dialogs,
   the unit panel, the unit command menu of ui/unit-commands.md (待命 and
   退卻 now; the other items are grey until M6), the mini map with the
   faction colours, and the units and cities on the Hex map panel. Every
   change of the game is one engine_api command. Included by ui.cpp. */

internal void ui_draw_supply(ui_state *ui, engine_api *api, engine_unit *transport);
internal b32  ui_combat_click(ui_state *ui, engine_api *api, hex_coord hex);                                       /* ui_combat.cpp */
internal void ui_start_pick(ui_state *ui, u8 pick, u8 row);
internal b32  ui_pick_allows(ui_state *ui, engine_api *api, u8 pick, u8 row, hex_coord hex);                        /* ui_combat.cpp */
internal void ui_draw_map_combat(ui_state *ui, engine_api *api, ImDrawList *draw, f32 cell, hex_point origin);
internal void ui_walk_plan(ui_state *ui, engine_api *api, u32 unit_id, s32 q, s32 r);                  /* ui_war.cpp */
internal void ui_walk_begin(ui_state *ui, engine_api *api, u32 unit_id, bool open_menu);
internal void ui_walk_cancel(ui_state *ui);
internal s32  ui_hit_mark(ui_state *ui, u32 unit_id, u8 kind);
internal ImU32 ui_hit_colour(s32 mark, ImU32 plain);

/* ---- faction colours (terrain-and-map.md, "Map display") ------------------ */

/* The colour of a faction from the engine (faction_colours.tsv of the pack).
   The flags, the plates, the maps and the models all use it. */
internal ImU32
ui_faction_color(engine_api *api, u8 force)
{
    return (ImU32)api->faction_colour(api->engine, force);
}

/* The same colour as an RGBA word of the render commands (ABGR in memory). */
internal u32
ui_faction_rgba(engine_api *api, u8 force)
{
    return api->faction_colour(api->engine, force);
}

/* ---- the 出征 and 輸送 dialogs --------------------------------------------- */

internal const char *ui_info_skill_name(ui_state *ui, engine_api *api, u8 skill);   /* ui_information.cpp */
internal void        ui_draw_unit_values(ui_state *ui, engine_unit_values *values);  /* below */
internal const char *ui_grade_text(u32 aptitude);                                    /* below */

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
    if (transport)
    {
        dialog->order = api->sortie_proposal(api->engine, dialog->city, type);
        dialog->order.unit_type = type;
        dialog->order.officers[1] = ENGINE_NO_OFFICER;
        dialog->order.officers[2] = ENGINE_NO_OFFICER;
        dialog->order.grain = 0;
    }
    else
    {
        /* G2-044: 出征 opens with empty slots and no soldiers, so 決定 is
           grey. A preset button fills the slots. */
        engine_sortie_order empty = {};
        empty.city_id     = dialog->city;
        empty.unit_type   = type;
        empty.destination = ENGINE_NO_CITY;
        for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place) empty.officers[place] = ENGINE_NO_OFFICER;
        dialog->order = empty;
    }
    dialog->soldiers = (int)dialog->order.soldiers;
    dialog->gold     = (int)dialog->order.gold;
    dialog->grain    = (int)dialog->order.grain;
    for (u32 index = 0; index < 4; ++index) dialog->cargo[index] = 0;
    dialog->prepared_city = dialog->city;
}

/* The stock of the weapon of a class in the city, or U32_MAX for a class
   without a weapon (the sword, the boat). */
internal u32
ui_class_stock(engine_api *api, engine_city_economy *economy, engine_unit_type *type)
{
    if (type->weapon == ENGINE_NO_WEAPON) return U32_MAX;
    engine_weapon_kind weapon = api->weapon_kind_at(api->engine, type->weapon);
    return weapon.stock < CITY_EQUIPMENT_STOCK_COUNT ? economy->stocks[weapon.stock] : 0;
}

/* The unit type of the 走舸, the ship of a land unit without a ship of its own. */
internal u8
ui_boat_type(engine_api *api)
{
    for (u32 index = 0; index < api->unit_type_count(api->engine); ++index)
    {
        engine_unit_type type = api->unit_type_at(api->engine, index);
        if (type.movement == MOVEMENT_NAVY && type.weapon == ENGINE_NO_WEAPON) return type.id;
    }
    return ENGINE_NO_UNIT_TYPE;
}

/* The base of a 出征 or 輸送 dialog as a city and its economy: a city
   through city_at and city_economy, a port or a pass through depot_at
   (the play report of 2026-10-23). */
internal void
ui_depot_view(engine_api *api, u8 location, engine_city *city, engine_city_economy *economy)
{
    if (location < CONTENT_FIRST_BASE)
    {
        *city    = api->city_at(api->engine, location);
        *economy = api->city_economy(api->engine, location);
        return;
    }
    engine_depot depot = api->depot_at(api->engine, location);
    *city    = {};
    *economy = {};
    city->city_id        = location;
    city->owner_force_id = depot.force;
    city->gold           = depot.gold;
    city->food           = depot.grain;
    city->troops         = depot.troops;
    city->morale         = (u8)Minimum(depot.morale, 255u);
    for (u32 index = 0; index < CITY_EQUIPMENT_STOCK_COUNT; ++index)
    {
        city->equipment_stocks[index] = depot.stocks[index];
        economy->stocks[index]        = depot.stocks[index];
    }
    economy->city_id     = location;
    economy->gold        = depot.gold;
    economy->grain       = depot.grain;
    economy->soldiers    = depot.troops;
    economy->soldier_cap = depot.soldier_cap;
    economy->morale      = depot.morale;
    economy->morale_max  = 100;
}

internal engine_city
ui_depot_city(engine_api *api, u8 location)
{
    engine_city city;
    engine_city_economy economy;
    ui_depot_view(api, location, &city, &economy);
    return city;
}

/* G2-044: the preset of a class fills the slots with the proposal of the
   engine, two grain for each soldier and 1000 gold (the preset of the
   original; the gold is what the city can give). */
internal void
ui_sortie_preset(engine_api *api, ui_march_dialog *dialog, u8 type)
{
    engine_sortie_order proposal = api->sortie_proposal(api->engine, dialog->city, type);
    engine_city city = ui_depot_city(api, dialog->city);
    dialog->order.unit_type = type;
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place) dialog->order.officers[place] = proposal.officers[place];
    dialog->soldiers = (int)proposal.soldiers;
    dialog->grain    = (int)proposal.grain;
    dialog->gold     = (int)Minimum(1000u, city.gold);
}

/* G2-044: the preset buttons 編制槍兵, 編制戟兵, 編制弩兵 and 編制騎兵, grey
   when the city has no weapon of the class. Then the row 武器: one button
   for each land class, grey without its weapon, with the 效果 line of the
   chosen class. */
internal void
ui_draw_sortie_classes(ui_state *ui, engine_api *api, ui_march_dialog *dialog, engine_city_economy *economy)
{
    char label[128];
    static const char *presets[] = { "pike", "halberd", "crossbow", "cavalry" };
    for (u32 index = 0; index < api->unit_type_count(api->engine); ++index)
    {
        engine_unit_type type = api->unit_type_at(api->engine, index);
        b32 preset = false;
        for (u32 which = 0; which < ArrayCount(presets); ++which) preset = preset || strcmp(type.key, presets[which]) == 0;
        if (!preset) continue;
        if (strcmp(type.key, "pike") != 0) ImGui::SameLine();
        snprintf(label, sizeof(label), "%s%s###preset%u", text_get(ui->text, "sortie.preset"), ui_text_with_key(ui, "unit_type", type.key), type.id);
        ImGui::BeginDisabled(ui_class_stock(api, economy, &type) == 0);
        if (ImGui::Button(label)) ui_sortie_preset(api, dialog, type.id);
        ImGui::EndDisabled();
    }
    ImGui::Text("%s:", text_get(ui->text, "sortie.weapons"));
    for (u32 index = 0; index < api->unit_type_count(api->engine); ++index)
    {
        engine_unit_type type = api->unit_type_at(api->engine, index);
        if (type.transport || type.movement == MOVEMENT_NAVY) continue;
        ImGui::SameLine();
        b32 chosen = type.id == dialog->order.unit_type;
        snprintf(label, sizeof(label), "%s###u%u", ui_text_with_key(ui, "unit_type", type.key), type.id);
        if (chosen) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
        ImGui::BeginDisabled(ui_class_stock(api, economy, &type) == 0);
        if (ImGui::SmallButton(label)) dialog->order.unit_type = type.id;
        ImGui::EndDisabled();
        if (chosen) ImGui::PopStyleColor();
    }
    engine_unit_type chosen = api->unit_type_at(api->engine, dialog->order.unit_type);
    const char *effect = "-";
    if (chosen.id != ENGINE_NO_UNIT_TYPE && chosen.weapon != ENGINE_NO_WEAPON)
    {
        char id[64];
        snprintf(id, sizeof(id), "weapon_effect.%s", api->weapon_kind_at(api->engine, chosen.weapon).key);
        const char *found = text_find_string(ui->text, id);
        if (found) effect = found;
    }
    ImGui::Text("%s: %s", text_get(ui->text, "sortie.effect"), effect);
}

/* G2-057: the row 艦船: the 走舸 always, a ship with a weapon row when the
   city holds one. The choice goes into the order. */
internal void
ui_draw_sortie_ships(ui_state *ui, engine_api *api, ui_march_dialog *dialog, engine_city_economy *economy)
{
    char label[128];
    ImGui::Text("%s:", text_get(ui->text, "sortie.ships"));
    for (u32 index = 0; index < api->unit_type_count(api->engine); ++index)
    {
        engine_unit_type type = api->unit_type_at(api->engine, index);
        if (type.movement != MOVEMENT_NAVY) continue;
        ImGui::SameLine();
        b32 boat = type.weapon == ENGINE_NO_WEAPON;
        b32 chosen = boat ? dialog->order.ship == 0 : dialog->order.ship == type.id;
        snprintf(label, sizeof(label), "%s###ship%u", ui_text_with_key(ui, "unit_type", type.key), type.id);
        if (chosen) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
        ImGui::BeginDisabled(!boat && ui_class_stock(api, economy, &type) == 0);
        if (ImGui::SmallButton(label)) dialog->order.ship = boat ? 0 : type.id;
        ImGui::EndDisabled();
        if (chosen) ImGui::PopStyleColor();
    }
}

/* The class of one tab of 部隊能力, its grade, the morale and the tactics
   of the class (grey when the grade of the unit is below the grade that
   the tactic needs). */
internal void
ui_draw_sortie_class(ui_state *ui, engine_api *api, u8 type_id, engine_unit_values *values, engine_city_economy *economy)
{
    ui_draw_unit_values(ui, values);
    engine_unit_type type = api->unit_type_at(api->engine, type_id);
    ImGui::Text("%s %s (%s)   %s %u", text_get(ui->text, "unit.class"), type.id == ENGINE_NO_UNIT_TYPE ? "-" : ui_text_with_key(ui, "unit_type", type.key),
                ui_grade_text(values->aptitude), text_get(ui->text, "command.morale"), economy->morale);
    ImGui::Text("%s:", text_get(ui->text, "sortie.tactics"));
    for (u32 row = 0; type_id != ENGINE_NO_UNIT_TYPE && row < api->tactic_kind_count(api->engine); ++row)
    {
        engine_tactic_kind tactic = api->tactic_kind_at(api->engine, row);
        if (!((tactic.unit_types >> type_id) & 1)) continue;
        ImGui::SameLine();
        ImGui::BeginDisabled(values->aptitude == 255 || tactic.aptitude > values->aptitude);
        ImGui::TextUnformatted(ui_text_with_key(ui, "tactic", tactic.key));
        ImGui::EndDisabled();
    }
}

/* G2-044: 部隊能力 with the tabs 陸上 and 水上. U-003: on water the unit
   fights as its ship, so the tab 水上 lists the class and the tactics of
   the ship, and the tab 陸上 those of the land class. */
internal void
ui_draw_sortie_ability(ui_state *ui, engine_api *api, ui_march_dialog *dialog, engine_city *city, engine_city_economy *economy)
{
    ImGui::TextUnformatted(text_get(ui->text, "sortie.ability"));
    u8 ship = dialog->order.ship ? dialog->order.ship : ui_boat_type(api);
    engine_unit_values land  = api->unit_values(api->engine, city->owner_force_id, dialog->order.unit_type, dialog->order.officers);
    engine_unit_values water = api->unit_values(api->engine, city->owner_force_id, ship, dialog->order.officers);
    if (ImGui::BeginTabBar("###ability"))
    {
        char tab[64];
        snprintf(tab, sizeof(tab), "%s###land", text_get(ui->text, "sortie.land"));
        if (ImGui::BeginTabItem(tab)) { ui_draw_sortie_class(ui, api, dialog->order.unit_type, &land, economy); ImGui::EndTabItem(); }
        snprintf(tab, sizeof(tab), "%s###water", text_get(ui->text, "sortie.water"));
        if (ImGui::BeginTabItem(tab)) { ui_draw_sortie_class(ui, api, ship, &water, economy); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
}

/* G2-044: 據點庫存, the stock of the base before and after the sortie: the
   weapon of the class, the soldiers, the gold and the grain. A line that
   goes to zero is red. */
internal void
ui_draw_sortie_stock(ui_state *ui, engine_api *api, ui_march_dialog *dialog, engine_city *city, engine_city_economy *economy)
{
    ImGui::TextUnformatted(text_get(ui->text, "sortie.stock"));
    engine_unit_type type = api->unit_type_at(api->engine, dialog->order.unit_type);
    struct stock_line { const char *name; u32 before, taken; } lines[4];
    u32 count = 0;
    if (type.id != ENGINE_NO_UNIT_TYPE && type.weapon != ENGINE_NO_WEAPON)
    {
        engine_weapon_kind weapon = api->weapon_kind_at(api->engine, type.weapon);
        u32 stock = ui_class_stock(api, economy, &type);
        /* A siege weapon or a ship takes one item; a soldier class takes one weapon for each soldier. */
        u32 taken = weapon.max_days ? 1 : dialog->order.soldiers;
        lines[count++] = { ui_text_with_key(ui, "weapon", weapon.key), stock, taken };
    }
    lines[count++] = { text_get(ui->text, "command.soldiers"), economy->soldiers, dialog->order.soldiers };
    lines[count++] = { text_get(ui->text, "command.gold"), city->gold, dialog->order.gold };
    lines[count++] = { text_get(ui->text, "command.grain"), city->food, dialog->order.grain };
    for (u32 index = 0; index < count; ++index)
    {
        u32 after = lines[index].taken > lines[index].before ? 0 : lines[index].before - lines[index].taken;
        ImVec4 colour = after == 0 ? ImVec4(1.0f, 0.4f, 0.35f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_Text);
        ImGui::TextColored(colour, "%s %u -> %u", lines[index].name, lines[index].before, after);
    }
}

internal void
ui_draw_destination_combo(ui_state *ui, engine_api *api, ui_march_dialog *dialog)
{
    char label[128], preview[96];
    if (dialog->order.destination == ENGINE_NO_CITY) snprintf(preview, sizeof(preview), "-");
    else ui_city_label(ui, api, dialog->order.destination, preview, sizeof(preview));
    engine_city here = ui_depot_city(api, dialog->city);
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
    ImVec2 size = transport ? ImVec2(480, 440) : ImVec2(640, 620);
    ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
    ui_context_place_sized(ui, 1, size);   /* the game mode: next to its entity, and on the screen (G2-044) */
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, title_id, transport ? "Transport" : "Sortie"), show))
    {
        ui_context_track(ui);
        if (dialog->prepared_city != dialog->city) ui_march_reset(api, dialog, transport);
        dialog->order.city_id = dialog->city;
        char city_name[96], label[96];
        ui_city_label(ui, api, dialog->city, city_name, sizeof(city_name));
        ImGui::Text("%s: %s", text_get(ui->text, "city.select"), city_name);
        engine_city city;
        engine_city_economy economy;
        ui_depot_view(api, dialog->city, &city, &economy);   /* a city, or a port or a pass */
        if (transport)
        {
            ui_draw_destination_combo(ui, api, dialog);
            if (ImGui::Button(ui_label(label, sizeof(label), ui, "sortie.auto", "auto")))
            {
                u8 destination = dialog->order.destination;
                ui_march_reset(api, dialog, transport);
                dialog->order.destination = destination;
            }
            ImGui::SameLine();
        }
        else
        {
            ui_draw_sortie_classes(ui, api, dialog, &economy);
            ui_draw_sortie_ships(ui, api, dialog, &economy);
        }
        ImGui::Button(ui_label(label, sizeof(label), ui, "command.executor", "executor"));
        u16 free_ids[UI_DEVELOP_OFFICER_LIST];
        u32 free_count = ui_free_officers(api, dialog->city, free_ids, ArrayCount(free_ids));
        u32 places = transport ? 1 : SIM_COMMAND_OFFICERS;
        for (u32 place = 0; place < places; ++place)
        {
            ui_draw_growth_officer(ui, api, dialog->order.officers + place, place, CONTENT_ABILITY_LEADERSHIP, free_ids, free_count);
        }
        if (transport)
        {
            ui_draw_amount(ui, "sortie.soldiers", "soldiers", &dialog->soldiers);
            ui_draw_amount(ui, "sortie.gold", "gold", &dialog->gold);
            ui_draw_amount(ui, "sortie.grain", "grain", &dialog->grain);
        }
        else
        {
            /* G2-044: three sliders, with the days that the grain lasts at one grain for ten soldiers each turn. */
            ImGui::SetNextItemWidth(260.0f);
            ImGui::SliderInt(ui_label(label, sizeof(label), ui, "sortie.soldiers", "soldiers"), &dialog->soldiers, 0, (int)economy.soldiers);
            ImGui::SetNextItemWidth(260.0f);
            ImGui::SliderInt(ui_label(label, sizeof(label), ui, "sortie.gold", "gold"), &dialog->gold, 0, (int)city.gold);
            ImGui::SetNextItemWidth(260.0f);
            ImGui::SliderInt(ui_label(label, sizeof(label), ui, "sortie.grain", "grain"), &dialog->grain, 0, (int)city.food);
            if (dialog->soldiers >= 10)
            {
                ImGui::SameLine();
                ImGui::Text("%u %s", (u32)dialog->grain / ((u32)dialog->soldiers / 10) * CALENDAR_DAYS_PER_TURN, text_get(ui->text, "sortie.days"));
            }
            if (dialog->soldiers < 0) dialog->soldiers = 0;
            if (dialog->gold < 0) dialog->gold = 0;
            if (dialog->grain < 0) dialog->grain = 0;
        }
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

        if (!transport)
        {
            ImGui::Separator();
            ui_draw_sortie_ability(ui, api, dialog, &city, &economy);
            ImGui::Separator();
            ui_draw_sortie_stock(ui, api, dialog, &city, &economy);
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
                /* G2-042: the move step of the new unit starts at once. The
                   unit is the one that the commander of the order leads. */
                u32 new_unit = ENGINE_NO_UNIT;
                for (u32 id = 0; id < api->unit_count(api->engine); ++id)
                {
                    engine_unit unit = api->unit_at(api->engine, id);
                    if (unit.id != ENGINE_NO_UNIT && unit.officers[0] == dialog->order.officers[0]) new_unit = id;
                }
                if (!transport && new_unit != ENGINE_NO_UNIT)
                {
                    ui_select_unit(ui, new_unit);
                    ui->scene.framed_unit = new_unit;   /* the camera stays at the city */
                    if (ui->hud_mode) ui_context_open(ui, api, UI_CONTEXT_UNIT, ui->context.x, ui->context.y);
                }
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
    ui->units.target_kind   = UI_TARGET_NONE;
    ui->units.target_unit   = ENGINE_NO_UNIT;
    ui->show_unit           = unit_id != ENGINE_NO_UNIT;
    ui->show_unit_menu      = false;
    ui->units.has_pending_move = false;
}

/* G2-045: the hex of the command menu: the destination of the pending
   move, else the hex of the unit. The original opens the menu at the
   destination, and the unit moves only with the choice. */
internal hex_coord
ui_unit_menu_hex(ui_state *ui, engine_unit *unit)
{
    return ui->units.has_pending_move ? ui->units.pending_move : hex_make(unit->q, unit->r);
}

/* The check of a command of the selected unit from the hex of the menu. */
internal engine_check
ui_unit_check(ui_state *ui, engine_api *api, const engine_command *command)
{
    if (ui->units.has_pending_move && command->unit == ui->units.selected)
    {
        return api->can_issue_after_move(api->engine, command, ui->units.pending_move.q, ui->units.pending_move.r);
    }
    return api->can_issue(api->engine, command);
}

/* The move of the pending menu, when the player chose: one command_move,
   then the walk. Returns false when the engine refused the move. */
internal b32
ui_commit_pending_move(ui_state *ui, engine_api *api)
{
    if (!ui->units.has_pending_move) return true;
    ui->units.has_pending_move = false;
    engine_unit unit = api->unit_at(api->engine, ui->units.selected);
    if (unit.id == ENGINE_NO_UNIT) return false;
    hex_coord to = ui->units.pending_move;
    ui_walk_plan(ui, api, unit.id, to.q, to.r);   /* the path, before the move changes the hex */
    engine_command_result result = api->command_move(api->engine, unit.id, to.q, to.r);
    ui_units_note_result(ui, result);
    if (result.refusal == SIM_REFUSAL_NONE) ui_walk_begin(ui, api, unit.id, false);
    else { ui_walk_cancel(ui); ui_growth_result(ui, "unit.move_done", result); }
    return result.refusal == SIM_REFUSAL_NONE;
}

/* Opens the target menu of the selected unit at a hex out of its range. */
internal void
ui_open_target_menu(ui_state *ui, u8 kind, hex_coord hex, u32 unit_id)
{
    ui->units.target_kind = kind;
    ui->units.target_hex  = hex;
    ui->units.target_unit = unit_id;
    ui->show_unit_menu    = false;
}

/* A click on the map with a unit of the faction that acts selected: a hex
   of the range moves it there (one command_move), then the command menu
   opens; its own hex opens the menu. With no unit selected, a click on an
   own unit selects it (G2-042). With a unit selected, a click out of the
   range opens the target menu (ui_draw_unit_target_menu): on an enemy
   unit 殲滅 and 驅趕 (G2-052), on an own unit the follow order, on a far
   hex 移動 (G2-058). Returns true if the click was for the unit. */
internal b32
ui_unit_click(ui_state *ui, engine_api *api, hex_coord hex)
{
    if (ui_combat_click(ui, api, hex)) return true;   /* a target of 攻擊 .. 撤除 (ui_combat.cpp) */
    u32 on_hex = api->unit_at_hex(api->engine, hex.q, hex.r);
    u8 acting = api->whose_turn(api->engine);
    b32 has_selection = ui->units.selected != ENGINE_NO_UNIT && api->unit_at(api->engine, ui->units.selected).id != ENGINE_NO_UNIT;
    if (!has_selection && on_hex != ENGINE_NO_UNIT && api->unit_at(api->engine, on_hex).force_id == acting)
    {
        ui_select_unit(ui, on_hex);
        return true;
    }
    if (!has_selection) { ui_select_unit(ui, ENGINE_NO_UNIT); return false; }
    engine_unit unit = api->unit_at(api->engine, ui->units.selected);
    if (unit.acted && on_hex != ENGINE_NO_UNIT && on_hex != unit.id && api->unit_at(api->engine, on_hex).force_id == acting)
    {
        /* The selected unit is done for this turn: a click on another own unit
           selects that unit, and asks for no follow order (the player, 2026-10-24). */
        ui_select_unit(ui, on_hex);
        return true;
    }
    if (unit.q == hex.q && unit.r == hex.r)
    {
        ui->units.has_pending_move = false;
        ui->show_unit_menu = true;
        return true;
    }
    engine_reach_view reach = api->reachable(api->engine, unit.id);
    for (u32 index = 1; index < reach.count; ++index)
    {
        if (reach.cells[index].q != hex.q || reach.cells[index].r != hex.r) continue;
        /* base_at takes the index of base_at_hex, not the location id of hex_at. */
        u32 base = api->base_at_hex(api->engine, hex.q, hex.r);
        if (base != ENGINE_NO_BASE && api->base_at(api->engine, base).force == unit.force_id)
        {
            /* An own port or pass in the range: the question 是否進入, not the
               command menu (the player, 2026-10-24). 是 moves the unit, and the
               unit enters the base. */
            ui_open_target_menu(ui, UI_TARGET_BASE, hex, ENGINE_NO_UNIT);
            return true;
        }
        /* G2-045: the command menu opens at the destination, with its items
           checked from there. The unit moves with the choice
           (ui_commit_pending_move), so a right click leaves it in place. */
        ui->units.has_pending_move = true;
        ui->units.pending_move     = hex;
        ui->show_unit_menu         = true;
        ui->context.x = ImGui::GetIO().MousePos.x;
        ui->context.y = ImGui::GetIO().MousePos.y;
        return true;
    }
    if (on_hex != ENGINE_NO_UNIT)
    {
        engine_unit other = api->unit_at(api->engine, on_hex);
        ui_open_target_menu(ui, (u8)(other.force_id == unit.force_id ? UI_TARGET_FOLLOW : UI_TARGET_ENEMY), hex, on_hex);
        return true;
    }
    /* G3-020: a facility of another force at a far hex: the question before the attack order. */
    engine_hex data = api->hex_at(api->engine, hex.q, hex.r);
    if (data.slot_city != ENGINE_NO_CITY && data.facility != ENGINE_NO_FACILITY &&
        api->city_at(api->engine, data.slot_city).owner_force_id != unit.force_id)
    {
        ui_open_target_menu(ui, UI_TARGET_FACILITY, hex, ENGINE_NO_UNIT);
        return true;
    }
    if (api->path_to(api->engine, unit.id, hex.q, hex.r).found)
    {
        ui_open_target_menu(ui, UI_TARGET_FAR, hex, ENGINE_NO_UNIT);
        return true;
    }
    ui_select_unit(ui, ENGINE_NO_UNIT);
    return false;
}

/* The name of the weapon of a city stock: the last row of weapons.tsv with
   that stock (木獸 for the stock of 衝車 and 木獸). */
internal const char *
ui_stock_name(ui_state *ui, engine_api *api, u32 stock)
{
    const char *name = "-";
    for (u32 index = 0; index < api->weapon_kind_count(api->engine); ++index)
    {
        engine_weapon_kind kind = api->weapon_kind_at(api->engine, index);
        if (kind.id != ENGINE_NO_WEAPON && kind.stock == stock) name = ui_text_with_key(ui, "weapon", kind.key);
    }
    return name;
}

/* The cargo of a transport as the 部隊情報 screen shows it (Q-217): a row
   of 槍 戟 弩 軍馬 and a row of 木獸 井闌 樓船, and the other stocks when the
   unit carries them. */
internal void
ui_draw_unit_cargo(ui_state *ui, engine_api *api, engine_unit *unit)
{
    const u32 rows[2][3] = { { CITY_STOCK_SPEARS, CITY_STOCK_HALBERDS, CITY_STOCK_CROSSBOWS },
                             { CITY_STOCK_BATTERING_RAMS, CITY_STOCK_SIEGE_TOWERS, CITY_STOCK_SHIP_TYPE_3 } };
    if (!ImGui::BeginTable("unit_cargo", 4, ImGuiTableFlags_SizingFixedFit)) return;
    for (u32 row = 0; row < 2; ++row)
    {
        ImGui::TableNextRow();
        for (u32 cell = 0; cell < 3; ++cell)
        {
            ImGui::TableNextColumn();
            ImGui::Text("%s %u", ui_stock_name(ui, api, rows[row][cell]), unit->cargo[rows[row][cell]]);
        }
        ImGui::TableNextColumn();
        if (row == 0) ImGui::Text("%s %u", ui_stock_name(ui, api, CITY_STOCK_WAR_HORSES), unit->cargo[CITY_STOCK_WAR_HORSES]);
    }
    const u32 others[3] = { CITY_STOCK_CATAPULTS, CITY_STOCK_SHIP_TYPE_1, CITY_STOCK_SHIP_TYPE_2 };
    for (u32 index = 0; index < 3; ++index)
    {
        if (!unit->cargo[others[index]]) continue;
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%s %u", ui_stock_name(ui, api, others[index]), unit->cargo[others[index]]);
    }
    ImGui::EndTable();
}

internal void
ui_draw_unit_panel(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(300, 380), ImGuiCond_FirstUseEver);
    ui_context_place(ui, 1);   /* the game mode: the dialog opens next to its entity */
    if (ui->hud_mode) ImGui::SetNextWindowPos(ImVec2(18.0f, 160.0f), ImGuiCond_Appearing);   /* G2-053: at the left edge, under the status area */
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "unit.title", "Unit"), &ui->show_unit))
    {
        ui_context_track(ui);
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
        /* G2-046: 狀態 (普通 when nothing is on the unit) and the skill of each officer in a red box. */
        char state_id[32];
        snprintf(state_id, sizeof(state_id), "unit.state_%u", unit.state);
        ImGui::Text("%s: %s", text_get(ui->text, "unit.state"), text_get(ui->text, state_id));
        for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
        {
            ImGui::Text("%s:", text_get(ui->text, place == 0 ? "unit.commander" : "unit.deputy"));
            ImGui::SameLine();
            ui_code_cell(ui, api, UI_CODE_OFFICER, unit.officers[place], UI_NO_ID_16);
            if (unit.officers[place] == ENGINE_NO_OFFICER) continue;
            engine_officer officer = api->officer_by_id(api->engine, unit.officers[place]);
            if (officer.skill_number == 255) continue;
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
            ImGui::SmallButton(ui_info_skill_name(ui, api, officer.skill_number));
            ImGui::PopStyleColor();
        }
        ImGui::Text("%s %u  %s %u", text_get(ui->text, "command.gold"), unit.gold, text_get(ui->text, "command.grain"), unit.grain);
        /* A changed value keeps its colour for a few seconds after its hit (ui_war.cpp). */
        ImU32 plain = ImGui::GetColorU32(ImGuiCol_Text);
        ImGui::Text("%s", text_get(ui->text, "command.soldiers"));
        ImGui::SameLine();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ui_hit_colour(ui_hit_mark(ui, unit.id, (u8)UI_HIT_SOLDIERS), plain)), "%u", unit.soldiers);
        ImGui::SameLine();
        ImGui::Text(" %s", text_get(ui->text, "command.morale"));
        ImGui::SameLine();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ui_hit_colour(ui_hit_mark(ui, unit.id, (u8)UI_HIT_MORALE), plain)), "%u", unit.morale);
        engine_unit_type type = api->unit_type_at(api->engine, unit.unit_type);
        ImGui::Text("%s: %s (%s)", text_get(ui->text, "unit.class"), type.id == ENGINE_NO_UNIT_TYPE ? "-" : ui_text_with_key(ui, "unit_type", type.key),
                    ui_grade_text(unit.values.aptitude));
        /* G2-046: 艦船 with the navy grade of the officers, and 俘虜. A unit without
           a ship of its own rows as a 走舸. The clone keeps no captive on a unit:
           a captured officer goes to a city of the captor (assumed, Q-176). */
        if (type.id != ENGINE_NO_UNIT_TYPE && type.movement != MOVEMENT_NAVY)
        {
            u8 ship = unit.ship_type != ENGINE_NO_UNIT_TYPE ? unit.ship_type : ui_boat_type(api);
            engine_unit_type ship_kind = api->unit_type_at(api->engine, ship);
            engine_unit_values afloat = api->unit_values(api->engine, unit.force_id, ship, unit.officers);
            ImGui::Text("%s: %s (%s)", text_get(ui->text, "unit.ship"), ship_kind.id == ENGINE_NO_UNIT_TYPE ? "-" : ui_text_with_key(ui, "unit_type", ship_kind.key),
                        ui_grade_text(afloat.aptitude));
        }
        ImGui::Text("%s: 0", text_get(ui->text, "unit.captives"));
        ui_draw_unit_values(ui, &unit.values);
        if (type.transport) ui_draw_unit_cargo(ui, api, &unit);
        if (unit.state)
        {
            /* 混亂 ends by a chance each turn (Q-218): it has no count of turns to show. */
            if (unit.state == UNIT_STATE_CONFUSED) ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "%s", text_get(ui->text, state_id));
            else ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "%s (%u)", text_get(ui->text, state_id), unit.state_turns);
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
        ui_units_note_result(ui, result);
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
    ui_context_place(ui, 0);   /* the game mode: the dialog opens next to its entity */
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "unit.menu", "UnitMenu"), &ui->show_unit_menu))
    {
        ui_context_track(ui);
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
        /* G2-045, G2-050, G3-005, G3-007: 攻擊, 齊攻, 單挑, 修補 and 撤除 are
           grey when no hex within 3 of the menu hex takes the command (the
           probe of the engine: the range of the class, a target, an own
           building). */
        hex_coord mine = ui_unit_menu_hex(ui, &unit);
        for (u32 index = 0; index < count; ++index)
        {
            const char *item = items[index];
            b32 wait = strcmp(item, "wait") == 0, retreat = strcmp(item, "retreat") == 0, supply = strcmp(item, "supply") == 0;
            u8 pick = ui_menu_pick(item);
            engine_command command = {};
            command.kind = wait ? ENGINE_COMMAND_WAIT : ENGINE_COMMAND_RETREAT;
            command.unit = unit.id;
            b32 enabled = supply || pick != UI_PICK_NONE ||
                          ((wait || retreat) && ui_unit_check(ui, api, &command).refusal == SIM_REFUSAL_NONE);
            if (pick != UI_PICK_NONE && (unit.acted || unit.state != 0)) enabled = false;
            if (enabled && (pick == UI_PICK_ATTACK || pick == UI_PICK_JOINT || pick == UI_PICK_DUEL || pick == UI_PICK_REPAIR || pick == UI_PICK_DISMANTLE))
            {
                b32 any = false;
                for (s32 dq = -3; dq <= 3 && !any; ++dq)
                {
                    for (s32 dr = -3; dr <= 3 && !any; ++dr)
                    {
                        hex_coord hex = hex_make(mine.q + dq, mine.r + dr);
                        if (hex_distance(hex, mine) <= 3 && ui_pick_allows(ui, api, pick, 0, hex)) any = true;
                    }
                }
                enabled = any;
            }
            snprintf(text_id, sizeof(text_id), "unit_menu.%s", item);
            ImGui::BeginDisabled(!enabled);
            if (ImGui::Button(ui_label(label, sizeof(label), ui, text_id, item), ImVec2(120.0f, 0.0f)))
            {
                if (pick == UI_PICK_TACTIC || pick == UI_PICK_PLOT || pick == UI_PICK_BUILD)
                {
                    /* G2-054: the list opens as a column next to the menu, and the menu stays. */
                    ui->combat.list  = pick;
                    ui->show_actions = true;
                }
                else if (pick != UI_PICK_NONE)
                {
                    ui_start_pick(ui, pick, 0);
                }
                else if (supply)
                {
                    if (ui_commit_pending_move(ui, api)) ui->show_unit = true;   /* G2-045: the move first */
                }
                else
                {
                    /* G2-045: the move of the pending menu first, then 待命 or 退卻. */
                    b32 moved = ui_commit_pending_move(ui, api);
                    engine_command_result result = {};
                    if (moved)
                    {
                        result = wait ? api->command_wait(api->engine, unit.id) : api->command_retreat(api->engine, unit.id);
                        ui_growth_result(ui, text_id, result);
                        ui_units_note_result(ui, result);
                    }
                    ui->show_unit_menu = false;
                    if (moved && retreat && result.refusal == SIM_REFUSAL_NONE) ui_select_unit(ui, ENGINE_NO_UNIT);
                }
            }
            ImGui::EndDisabled();
        }
    }
    ImGui::End();
}

/* The lines of the panel under the target menu (G2-052): the days to
   arrive, the days of grain and the bases on the way. */
internal void
ui_draw_path_panel(ui_state *ui, engine_api *api, engine_path_view *path)
{
    const char *day = text_get(ui->text, "unit_target.day_unit");
    ImGui::Separator();
    ImGui::Text("%s %u%s", text_get(ui->text, "unit_target.days"), path->days, day);
    if (path->grain_days == ENGINE_GRAIN_TURNS_ENDLESS) ImGui::Text("%s -", text_get(ui->text, "unit_target.grain"));
    else ImGui::Text("%s %u%s", text_get(ui->text, "unit_target.grain"), path->grain_days, day);
    ImGui::Text("%s", text_get(ui->text, "unit_target.bases"));
    for (u32 index = 0; index < path->base_count; ++index)
    {
        engine_base base = api->base_at(api->engine, path->bases[index]);
        if (base.id == ENGINE_NO_ROW) continue;
        ImGui::SameLine();
        ImGui::TextUnformatted(ui_text_with_key(ui, "base", base.key));
    }
}

/* The target menu at a hex out of the range of the selected unit
   (ui_unit_click). An enemy unit: 殲滅 and 驅趕 (G2-052). An own unit: the
   question of the follow order with 是 and 否 (G2-052). A far hex: 移動
   (G2-058; 設置 at a far hex has no rule yet and stays grey). The panel
   shows the plan. One click sends one command. */
internal void
ui_draw_unit_target_menu(ui_state *ui, engine_api *api)
{
    if (ui->units.target_kind == UI_TARGET_NONE) return;
    engine_unit unit = api->unit_at(api->engine, ui->units.selected);
    if (unit.id == ENGINE_NO_UNIT) { ui->units.target_kind = UI_TARGET_NONE; return; }
    char title[128], label[160];
    bool open = true;
    hex_coord hex = ui->units.target_hex;
    ImGui::SetNextWindowSize(ImVec2(260, 0), ImGuiCond_Always);
    ui_context_place(ui, 0);
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "unit_target.title", "UnitTarget"), &open, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ui_context_track(ui);
        engine_path_view path = api->path_to(api->engine, unit.id, hex.q, hex.r);
        engine_command_result result = {};
        b32 sent = false;
        const char *text_id = "unit_target.title";
        u8 kind = ui->units.target_kind;
        if (kind == UI_TARGET_ENEMY)
        {
            if (ImGui::Button(ui_label(label, sizeof(label), ui, "unit_target.destroy", "destroy"), ImVec2(120.0f, 0.0f)))
            {
                result = api->command_destroy(api->engine, unit.id, ui->units.target_unit); sent = true; text_id = "unit_target.destroy";
            }
            if (ImGui::Button(ui_label(label, sizeof(label), ui, "unit_target.drive", "drive"), ImVec2(120.0f, 0.0f)))
            {
                result = api->command_drive(api->engine, unit.id, ui->units.target_unit); sent = true; text_id = "unit_target.drive";
            }
        }
        else if (kind == UI_TARGET_FACILITY)
        {
            /* G3-020: 即將對 市場 Lv3進行攻擊。確定嗎？ with 是 and 否. */
            engine_hex data = api->hex_at(api->engine, hex.q, hex.r);
            engine_facility facility = api->facility_at(api->engine, data.slot_city, data.slot_index);
            char name[128] = "?";
            if (facility.facility != ENGINE_NO_FACILITY)
            {
                snprintf(name, sizeof(name), "%s Lv%u", ui_text_with_key(ui, "facility", api->facility_kind_at(api->engine, facility.facility).key), facility.level);
            }
            ImGui::Text("%s%s%s", text_get(ui->text, "unit_target.facility_before"), name, text_get(ui->text, "unit_target.facility_after"));
            if (ImGui::Button(ui_label(label, sizeof(label), ui, "unit_target.yes", "facility_yes"), ImVec2(80.0f, 0.0f)))
            {
                result = api->command_attack_facility(api->engine, unit.id, hex.q, hex.r); sent = true; text_id = "unit_target.facility_before";
            }
            ImGui::SameLine();
            if (ImGui::Button(ui_label(label, sizeof(label), ui, "unit_target.no", "facility_no"), ImVec2(80.0f, 0.0f))) open = false;
        }
        else if (kind == UI_TARGET_BASE)
        {
            /* The entry of an own port or pass: 是 moves the unit onto the base, and the sim takes it in. */
            char name[128] = "?";
            u32 base = api->base_at_hex(api->engine, hex.q, hex.r);
            if (base != ENGINE_NO_BASE) ui_city_label(ui, api, api->base_at(api->engine, base).id, name, sizeof(name));
            ImGui::Text("%s%s%s", text_get(ui->text, "unit_target.base_before"), name, text_get(ui->text, "unit_target.base_after"));
            if (ImGui::Button(ui_label(label, sizeof(label), ui, "unit_target.yes", "base_yes"), ImVec2(80.0f, 0.0f)))
            {
                ui_walk_plan(ui, api, unit.id, hex.q, hex.r);
                result = api->command_move(api->engine, unit.id, hex.q, hex.r); sent = true; text_id = "unit_target.base_before";
                if (result.refusal == SIM_REFUSAL_NONE) ui_walk_begin(ui, api, unit.id, false);
                else                                    ui_walk_cancel(ui);
            }
            ImGui::SameLine();
            if (ImGui::Button(ui_label(label, sizeof(label), ui, "unit_target.no", "base_no"), ImVec2(80.0f, 0.0f))) open = false;
        }
        else if (kind == UI_TARGET_FOLLOW)
        {
            engine_unit target = api->unit_at(api->engine, ui->units.target_unit);
            char name[128] = "?";
            if (target.id != ENGINE_NO_UNIT) ui_unit_label(ui, api, &target, name, sizeof(name));
            ImGui::Text("%s%s%s", text_get(ui->text, "unit_target.follow_before"), name, text_get(ui->text, "unit_target.follow_after"));
            if (ImGui::Button(ui_label(label, sizeof(label), ui, "unit_target.yes", "follow_yes"), ImVec2(80.0f, 0.0f)))
            {
                result = api->command_follow(api->engine, unit.id, ui->units.target_unit); sent = true; text_id = "unit_target.follow_before";
            }
            ImGui::SameLine();
            if (ImGui::Button(ui_label(label, sizeof(label), ui, "unit_target.no", "follow_no"), ImVec2(80.0f, 0.0f))) open = false;
        }
        else
        {
            if (ImGui::Button(ui_label(label, sizeof(label), ui, "unit_target.move", "far_move"), ImVec2(120.0f, 0.0f)))
            {
                ui_walk_plan(ui, api, unit.id, hex.q, hex.r);
                result = api->command_move(api->engine, unit.id, hex.q, hex.r); sent = true; text_id = "unit_target.move";
                if (result.refusal == SIM_REFUSAL_NONE) ui_walk_begin(ui, api, unit.id, false);   /* the part of the way of this turn */
                else                                    ui_walk_cancel(ui);
            }
            ImGui::BeginDisabled(true);
            ImGui::Button(ui_label(label, sizeof(label), ui, "unit_target.build", "far_build"), ImVec2(120.0f, 0.0f));
            ImGui::EndDisabled();
        }
        if (kind != UI_TARGET_FOLLOW || path.found) ui_draw_path_panel(ui, api, &path);
        if (sent)
        {
            ui_growth_result(ui, text_id, result);
            ui_units_note_result(ui, result);
            open = false;
            if (result.refusal == SIM_REFUSAL_NONE)
            {
                /* The unit has its order: the selection ends, as after 待命 (the plate shows 委). */
                ui_select_unit(ui, ENGINE_NO_UNIT);
                if (ui->context.kind == UI_CONTEXT_UNIT) ui_context_close(ui);
            }
        }
    }
    ImGui::End();
    if (!open) ui->units.target_kind = UI_TARGET_NONE;
}

/* The unit that a point of the screen hits on a plate of the last frame,
   or ENGINE_NO_UNIT (G2-042: a click on a plate is a click on the hex of
   its unit). */
internal u32
ui_plate_unit_at(ui_state *ui, f32 x, f32 y)
{
    for (u32 index = 0; index < ui->units.plate_count; ++index)
    {
        if (x >= ui->units.plates[index].x0 && x <= ui->units.plates[index].x1 &&
            y >= ui->units.plates[index].y0 && y <= ui->units.plates[index].y1)
        {
            return ui->units.plates[index].unit;
        }
    }
    return ENGINE_NO_UNIT;
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
        draw->AddRectFilled(ImVec2(center.x - half, center.y - half), ImVec2(center.x + half, center.y + half), ui_faction_color(api, city.owner_force_id));
        draw->AddRect(ImVec2(center.x - half, center.y - half), ImVec2(center.x + half, center.y + half), IM_COL32(20, 20, 20, 255));
    }
    for (u32 id = 0; id < api->unit_count(api->engine); ++id)
    {
        engine_unit unit = api->unit_at(api->engine, id);
        if (unit.id == ENGINE_NO_UNIT) continue;
        hex_point center = hex_to_pixel(ui_round_unit_hex(ui, &unit), cell, origin);   /* a walk shows hex by hex */
        f32 half = cell * 0.55f;
        ImVec2 diamond[4] = { ImVec2(center.x, center.y - half), ImVec2(center.x + half, center.y), ImVec2(center.x, center.y + half),
                              ImVec2(center.x - half, center.y) };
        draw->AddConvexPolyFilled(diamond, 4, ui_faction_color(api, unit.force_id));
        draw->AddPolyline(diamond, 4, unit.id == ui->units.selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(20, 20, 20, 255),
                          ImDrawFlags_Closed, 1.5f);
    }
}

