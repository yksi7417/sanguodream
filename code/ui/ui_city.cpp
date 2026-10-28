/* The City panel and the Develop dialog (city-commands.md, 開發;
   ui/city-commands.md). The City panel shows the slots of one city. The
   Develop dialog picks a facility, a slot and up to three officers, shows
   the answer of can_issue, and sends one command_develop for one click of
   Execute. Both read the game only through engine_api. Included by ui.cpp. */

#define UI_DEVELOP_OFFICER_LIST 64   /* the free officers that the dialog lists */

internal void ui_draw_city_command_buttons(ui_state *ui, engine_api *api, u16 first_officer);   /* ui_city_commands.cpp */
internal void ui_draw_growth_buttons(ui_state *ui, engine_api *api, u16 first_officer);         /* ui_growth.cpp */
internal void ui_draw_march_buttons(ui_state *ui, engine_api *api, u16 first_officer);          /* ui_march.cpp */
internal void ui_draw_city_disasters(ui_state *ui, engine_api *api, u8 city_id);
internal sim_refusal ui_probe_city_command(engine_api *api, u8 city, engine_command_kind kind, u16 first_officer);   /* ui_city_commands.cpp */
internal u32  ui_free_officers(engine_api *api, u8 city_id, u16 *ids, u32 capacity);

internal const char *
ui_text_with_key(ui_state *ui, const char *prefix, const char *key)
{
    char id[64];
    snprintf(id, sizeof(id), "%s.%s", prefix, key);
    return text_get(ui->text, id);
}

/* The name of a pass or a port: the UI string "base.<key>", or else the
   Big5 name of the name index. A pack with its own scenario names its bases
   in its bases.tsv (docs/MAPS.md, W2). Returns out. */
internal const char *
ui_base_name(ui_state *ui, engine_api *api, const engine_base *base, char *out, u32 out_size)
{
    char id[64];
    snprintf(id, sizeof(id), "base.%s", base->key);
    const char *text = text_get(ui->text, id);
    if (text != id) snprintf(out, out_size, "%s", text);
    else            ui_location_text(ui, api, base->id, out, out_size);
    return out;
}

internal const char *
ui_facility_name(ui_state *ui, engine_api *api, u8 facility)
{
    engine_facility_kind kind = api->facility_kind_at(api->engine, facility);
    if (kind.id == ENGINE_NO_FACILITY) return "-";
    return ui_text_with_key(ui, "facility", kind.key);
}

internal const char *
ui_refusal_text(ui_state *ui, sim_refusal refusal)
{
    return ui_text_with_key(ui, "refusal", sim_refusal_name(refusal));
}

internal const char *
ui_slot_state_text(ui_state *ui, u8 state)
{
    switch (state)
    {
        case SIM_SLOT_EMPTY:    return text_get(ui->text, "slot.empty");
        case SIM_SLOT_BUILDING: return text_get(ui->text, "slot.building");
        case SIM_SLOT_BUILT:    return text_get(ui->text, "slot.built");
        case SIM_SLOT_MERGING:  return text_get(ui->text, "slot.merging");
        default:                return "?";
    }
}

/* The name of a city: the decoded name; its id until a name index loads;
   the id and the name when the panels show raw numbers. */
internal void
ui_city_label(ui_state *ui, engine_api *api, u8 city_id, char *out, u32 out_size)
{
    if (city_id >= CONTENT_FIRST_BASE && city_id < CONTENT_FIRST_BASE + CONTENT_BASE_COUNT)
    {
        /* A port or a pass is a base of the spec too (the play report of 2026-10-23): its name is "base.<key>". */
        engine_base base = api->base_at(api->engine, (u32)city_id - CONTENT_FIRST_BASE);
        if (base.id != ENGINE_NO_ROW) { ui_base_name(ui, api, &base, out, out_size); return; }
    }
    engine_city_name name = api->city_name_at(api->engine, city_id);
    char decoded[UI_NAME_SIZE] = "";
    if (name.city_id != ENGINE_NO_CITY) text_name(ui->text, name.name, sizeof(name.name), decoded, sizeof(decoded));
    if (!decoded[0])       snprintf(out, out_size, "%u", city_id);
    else if (ui->show_raw) snprintf(out, out_size, "%u %s", city_id, decoded);
    else                   snprintf(out, out_size, "%s", decoded);
}

/* True if the force owns a loaded city. */
internal b32
ui_force_owns_a_city(engine_api *api, u8 force)
{
    u32 count = api->city_count(api->engine);
    for (u32 index = 0; index < count; ++index)
    {
        if (api->city_at(api->engine, index).owner_force_id == force) return true;
    }
    return false;
}

/* While the panels list only the cities of the player, the City panel
   shows one of them: a city of another force goes to the first own city. */
internal void ui_frame_city(ui_state *ui, u8 city_id);                                     /* ui_locate.cpp (M11) */
internal void ui_draw_city_facility_counts(ui_state *ui, engine_api *api, u8 city_id);     /* ui_locate.cpp (M11) */

internal void
ui_city_keep_selection(ui_state *ui, engine_api *api, engine_city *city)
{
    u8 force = ui_city_filter_force(ui, api);
    if (force == ENGINE_NO_FACTION || city->owner_force_id == force) return;
    u32 count = api->city_count(api->engine);
    for (u32 index = 0; index < count; ++index)
    {
        engine_city other = api->city_at(api->engine, index);
        if (other.owner_force_id != force) continue;
        ui->city.selected = other.city_id;
        *city = other;
        return;
    }
}

/* The combo lists the cities of the player, or every city when "All
   cities" is on or the player owns none. */
internal void
ui_draw_city_combo(ui_state *ui, engine_api *api)
{
    char label[128], preview[96];
    ui_city_label(ui, api, ui->city.selected, preview, sizeof(preview));
    if (ImGui::BeginCombo(ui_label(label, sizeof(label), ui, "city.select", "city_select"), preview))
    {
        u32 count = api->city_count(api->engine);
        u8 force = ui_city_filter_force(ui, api);
        if (force != ENGINE_NO_FACTION && !ui_force_owns_a_city(api, force)) force = ENGINE_NO_FACTION;
        for (u32 index = 0; index < count; ++index)
        {
            engine_city city = api->city_at(api->engine, index);
            if (force != ENGINE_NO_FACTION && city.owner_force_id != force) continue;
            char name[96];
            ui_city_label(ui, api, city.city_id, name, sizeof(name));
            snprintf(label, sizeof(label), "%s###c%u", name, city.city_id);
            bool selected = city.city_id == ui->city.selected;
            if (ImGui::Selectable(label, selected)) ui_frame_city(ui, city.city_id);
        }
        ImGui::EndCombo();
    }
}

internal void
ui_draw_slot_row(ui_state *ui, engine_api *api, engine_facility *slot)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::Text("%u", slot->slot);
    ImGui::TableNextColumn();
    if (slot->facility == ENGINE_NO_FACILITY) ImGui::TextUnformatted("-");
    else if (slot->level > 1)                 ImGui::Text("%s Lv%u", ui_facility_name(ui, api, slot->facility), slot->level);
    else                                      ImGui::TextUnformatted(ui_facility_name(ui, api, slot->facility));
    ImGui::TableNextColumn(); ImGui::TextUnformatted(ui_slot_state_text(ui, slot->state));
    ImGui::TableNextColumn();
    if (slot->state == SIM_SLOT_BUILDING) ImGui::Text("%u", slot->turns_left);
    else                                  ImGui::TextUnformatted("-");
    ImGui::TableNextColumn();
    if (slot->yield) ImGui::Text("%u", slot->yield);
    else             ImGui::TextUnformatted("-");
}

/* A refusal that the action points, the gold or the technique points
   cause. The City panel greys a button for these; the dialog explains the
   other refusals (no officer, no slot, an enemy near). */
internal b32
ui_refusal_is_cost(sim_refusal refusal)
{
    return refusal == SIM_REFUSAL_NO_ACTION_POINTS || refusal == SIM_REFUSAL_NO_GOLD ||
           refusal == SIM_REFUSAL_NO_TECHNIQUE_POINTS;
}

/* A button of the City panel. A cost refusal greys it and names the
   reason in a tooltip. Returns true on a click. */
internal b32
ui_city_button(ui_state *ui, const char *text_id, const char *stable_id, sim_refusal refusal)
{
    char label[96];
    b32 grey = ui_refusal_is_cost(refusal);
    ImGui::BeginDisabled(grey);
    b32 clicked = ImGui::Button(ui_label(label, sizeof(label), ui, text_id, stable_id));
    ImGui::EndDisabled();
    if (grey && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::SetTooltip("%s: %s", text_get(ui->text, "develop.refused"), ui_refusal_text(ui, refusal));
    }
    return clicked;
}

/* The command buttons of the City panel: only for a city of the faction
   that acts. Each button asks can_issue with a trial order, so that a
   command the action points or the gold do not allow is grey. */
internal void ui_draw_base_menu(ui_state *ui, engine_api *api, u16 first_officer);   /* ui_base_menu.cpp */

internal void
ui_draw_city_buttons(ui_state *ui, engine_api *api, engine_city *city)
{
    u8 acting = api->whose_turn(api->engine);
    if (city->owner_force_id == ENGINE_NO_FACTION || city->owner_force_id != acting)
    {
        ImGui::TextDisabled("%s", text_get(ui->text, "city.not_yours"));
        return;
    }
    u16 first_officer = ENGINE_NO_OFFICER;
    ui_free_officers(api, ui->city.selected, &first_officer, 1);
    if (ui->hud_mode)
    {
        ui_draw_base_menu(ui, api, first_officer);   /* M14: the menu of the original (ui_base_menu.cpp) */
        if (ui->command_message[0]) ImGui::TextUnformatted(ui->command_message);
        return;
    }
    if (ui_city_button(ui, "city.develop", "open_develop",
                       ui_probe_city_command(api, ui->city.selected, ENGINE_COMMAND_DEVELOP, first_officer)))
    {
        ui->show_develop = true;
        ui->develop.city = ui->city.selected;
        ui->develop.message[0] = 0;
    }
    ui_draw_growth_buttons(ui, api, first_officer);
    ImGui::SameLine();
    ui_draw_city_command_buttons(ui, api, first_officer);
    ui_draw_march_buttons(ui, api, first_officer);
}

/* M19, ui/city-commands.md "Base panel": the fields of the city panel of
   the original, in its order. 治安 and 氣力 are of 100. The weapons are
   the stock of each row of weapons.tsv. 現役 is the free officers of the
   city of its active officers. */
internal void
ui_draw_city_game_fields(ui_state *ui, engine_api *api, u8 city_id)
{
    engine_city city = api->city_at(api->engine, city_id);
    engine_city_slots slots = api->city_slots(api->engine, city_id);
    engine_city_economy economy = api->city_economy(api->engine, city_id);
    engine_city_siege siege = api->city_siege(api->engine, city_id);
    char name[UI_NAME_SIZE * 2];
    ui_city_label(ui, api, city_id, name, sizeof(name));
    ImGui::TextColored(ImVec4(0.62f, 0.82f, 0.96f, 1.0f), "%s", name);
    ImGui::Separator();
    ImGui::Text("%s %u   %s %u", text_get(ui->text, "column.gold"), city.gold, text_get(ui->text, "column.food"), city.food);
    char number[32];
    ui_format_facility_count(number, sizeof(number), slots.built, slots.building);   /* 設施數 built (+in progress) / slots */
    ImGui::Text("%s %u / 100   %s %s / %u", text_get(ui->text, "command.order"), economy.public_order,
                text_get(ui->text, "city.slots"), number, slots.slot_count);
    ImGui::Text("%s %u / %u", text_get(ui->text, "city.durability"), siege.durability, siege.durability_max);
    ImGui::Text("%s %u   %s %u / 100", text_get(ui->text, "command.soldiers"), economy.soldiers, text_get(ui->text, "command.morale"),
                economy.morale);
    u32 count = api->weapon_kind_count(api->engine);
    for (u32 index = 0; index < count; ++index)
    {
        engine_weapon_kind kind = api->weapon_kind_at(api->engine, index);
        u32 stock = kind.stock < CITY_EQUIPMENT_STOCK_COUNT ? economy.stocks[kind.stock] : 0;
        if (index % 4 != 0) ImGui::SameLine();
        ImGui::Text("%s %u", ui_text_with_key(ui, "weapon", kind.key), stock);
    }
    ImGui::Text("%s %u   %s %u", text_get(ui->text, "city.gold_income"), slots.gold_income, text_get(ui->text, "city.grain_income"),
                slots.grain_income);
    char price[64];
    snprintf(price, sizeof(price), text_get(ui->text, "city.price_text"), economy.grain_per_gold);
    ImGui::Text("%s %s", text_get(ui->text, "city.market_price"), price);
    /* G2-016: 現役 counts all officers of the city, also those in its units (base_location). 在野
       counts the free officers that appeared (assumed, Q-223). */
    u32 active = 0, captives = 0, ronin = 0;
    u16 year = api->date(api->engine).year;
    for (u32 index = 0; index < api->officer_count(api->engine); ++index)
    {
        engine_officer officer = api->officer_at(api->engine, index);
        if (officer.status <= 3 && officer.force_id == city.owner_force_id && officer.base_location == city_id) { active++; continue; }
        if (officer.current_position != city_id) continue;
        if (officer.status == 6 && officer.appearance_year <= year) ronin++;
        else if (officer.status != 6 && api->officer_detail(api->engine, officer.officer_id).captive) captives++;
    }
    u16 free_ids[UI_DEVELOP_OFFICER_LIST];
    u32 free_count = ui_free_officers(api, city_id, free_ids, ArrayCount(free_ids));
    ImGui::Text("%s %u / %u   %s %u   %s %u", text_get(ui->text, "city.active"), free_count, active, text_get(ui->text, "city.captives"),
                captives, text_get(ui->text, "city.ronin"), ronin);
    ImGui::Separator();
}

/* The panel of a port or a pass (ui/city-commands.md, "Port or pass
   panel": the name, 資金, 兵糧, 耐久, 士兵, 氣力, the weapon stock, 現役 and
   俘虜; no 治安, 設施數, income, 市價 or 在野). The buttons 出征 and 輸送
   follow for the base of the acting force (the play report of 2026-10-23).
   The other buttons of the port menu of the original wait. */
internal void
ui_draw_base_panel(ui_state *ui, engine_api *api, u8 location)
{
    engine_depot depot = api->depot_at(api->engine, location);
    engine_base base = api->base_at(api->engine, (u32)location - CONTENT_FIRST_BASE);
    char name[UI_NAME_SIZE * 2];
    ui_city_label(ui, api, location, name, sizeof(name));
    ImGui::TextColored(ImVec4(0.62f, 0.82f, 0.96f, 1.0f), "%s", name);
    ImGui::Separator();
    ImGui::Text("%s %u   %s %u", text_get(ui->text, "column.gold"), depot.gold, text_get(ui->text, "column.food"), depot.grain);
    ImGui::Text("%s %u / %u", text_get(ui->text, "city.durability"), base.durability, base.durability_max);
    ImGui::Text("%s %u / %u   %s %u / 100", text_get(ui->text, "command.soldiers"), depot.troops, depot.soldier_cap,
                text_get(ui->text, "command.morale"), depot.morale);
    u32 count = api->weapon_kind_count(api->engine);
    for (u32 index = 0; index < count; ++index)
    {
        engine_weapon_kind kind = api->weapon_kind_at(api->engine, index);
        u32 stock = kind.stock < CITY_EQUIPMENT_STOCK_COUNT ? depot.stocks[kind.stock] : 0;
        if (index % 4 != 0) ImGui::SameLine();
        ImGui::Text("%s %u", ui_text_with_key(ui, "weapon", kind.key), stock);
    }
    u32 active = 0, captives = 0;
    for (u32 index = 0; index < api->officer_count(api->engine); ++index)
    {
        engine_officer officer = api->officer_at(api->engine, index);
        if (officer.status <= 3 && officer.force_id == depot.force && officer.base_location == location) { active++; continue; }
        if (officer.current_position == location && officer.status != 6 && api->officer_detail(api->engine, officer.officer_id).captive) captives++;
    }
    u16 free_ids[UI_DEVELOP_OFFICER_LIST];
    u32 free_count = ui_free_officers(api, location, free_ids, ArrayCount(free_ids));
    ImGui::Text("%s %u / %u   %s %u", text_get(ui->text, "city.active"), free_count, active, text_get(ui->text, "city.captives"), captives);
    ImGui::Separator();
    if (!depot.exists || depot.force != api->whose_turn(api->engine))
    {
        ImGui::TextDisabled("%s", text_get(ui->text, "city.not_yours"));
        return;
    }
    ui_draw_march_buttons(ui, api, free_count ? free_ids[0] : (u16)ENGINE_NO_OFFICER);
    if (ui->command_message[0]) ImGui::TextUnformatted(ui->command_message);
}

internal void
ui_draw_city(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(440, 420), ImGuiCond_FirstUseEver);
    ui_context_place(ui, 0);   /* the game mode: the dialog opens next to its entity */
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "city.title", "City"), &ui->show_city))
    {
        ui_context_track(ui);
        if (ImGui::IsWindowAppearing()) ImGui::SetScrollY(0.0f);   /* G2-024: the menu opens at its top each time */
        if (api->city_count(api->engine) == 0)
        {
            ImGui::TextWrapped("%s", text_get(ui->text, "city.none"));
            ImGui::End();
            return;
        }
        if (ui->city.selected >= CONTENT_FIRST_BASE)
        {
            ui_draw_base_panel(ui, api, ui->city.selected);   /* a port or a pass (the play report of 2026-10-23) */
            ImGui::End();
            return;
        }
        if (ui->hud_mode)
        {
            /* M19: the base panel of the original and its command menu,
               without the combo, the check boxes and the slot table of the
               debug layout. */
            ui_draw_city_game_fields(ui, api, ui->city.selected);
            engine_city chosen = api->city_at(api->engine, ui->city.selected);
            ui_draw_city_buttons(ui, api, &chosen);
            ImGui::End();
            return;
        }
        ui_draw_city_combo(ui, api);
        ImGui::SameLine();
        ui_all_cities_toggle(ui);
        ImGui::SameLine();
        ui_raw_toggle(ui);
        engine_city city = api->city_at(api->engine, ui->city.selected);
        ui_city_keep_selection(ui, api, &city);
        engine_city_slots slots = api->city_slots(api->engine, ui->city.selected);
        ImGui::Text("%s:", text_get(ui->text, "column.owner"));
        ImGui::SameLine();
        ui_code_cell(ui, api, UI_CODE_FORCE, city.owner_force_id, UI_NO_ID_8);
        ImGui::SameLine();
        ImGui::Text("  %s: %u   %s: %u", text_get(ui->text, "column.gold"), city.gold, text_get(ui->text, "column.food"), city.food);
        ImGui::Text("%s:", text_get(ui->text, "city.specialty"));
        ImGui::SameLine();
        ui_code_cell(ui, api, UI_CODE_SPECIALTY, slots.specialty, U32_MAX);
        char number[32];
        ui_format_facility_count(number, sizeof(number), slots.built, slots.building);
        ImGui::SameLine();
        ImGui::Text("  %s: %s / %u", text_get(ui->text, "city.slots"), number, slots.slot_count);
        ImGui::Text("%s: %u   %s: %u", text_get(ui->text, "city.gold_income"), slots.gold_income,
                    text_get(ui->text, "city.grain_income"), slots.grain_income);
        ImGui::Text("%s:", text_get(ui->text, "city.governor"));
        ImGui::SameLine();
        ui_code_cell(ui, api, UI_CODE_OFFICER, slots.governor_id, UI_NO_ID_16);
        engine_city_economy economy = api->city_economy(api->engine, ui->city.selected);
        ImGui::Text("%s: %u   %s: %u   %s: %u / %u", text_get(ui->text, "command.order"), economy.public_order,
                    text_get(ui->text, "command.morale"), economy.morale, text_get(ui->text, "command.soldiers"),
                    economy.soldiers, economy.soldier_cap);
        ui_draw_city_buttons(ui, api, &city);
        ui_draw_city_disasters(ui, api, ui->city.selected);

        ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
        if (ImGui::BeginTable("slots", 5, flags))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ui_table_column(ui, "column.slot", "slot", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ui_table_column(ui, "column.facility", "facility");
            ui_table_column(ui, "column.state", "state");
            ui_table_column(ui, "column.turns_left", "turns_left", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ui_table_column(ui, "column.yield", "yield", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableHeadersRow();
            for (u32 index = 0; index < slots.slot_count; ++index)
            {
                engine_facility slot = api->facility_at(api->engine, ui->city.selected, (u8)index);
                ui_draw_slot_row(ui, api, &slot);
            }
            ImGui::EndTable();
        }
        ui_draw_city_facility_counts(ui, api, ui->city.selected);   /* M11: the counts of the city information screen */
    }
    ImGui::End();
}

/* The officers that can develop in the city now: active officers of the
   owner in the city without a job that did not act in this turn. */
internal u32
ui_free_officers(engine_api *api, u8 city_id, u16 *ids, u32 capacity)
{
    /* The force of the base: a city, or a port or a pass (the play report of 2026-10-23). */
    u8 force = city_id < CONTENT_FIRST_BASE ? api->city_at(api->engine, city_id).owner_force_id : api->depot_at(api->engine, city_id).force;
    u32 found = 0;
    u32 count = api->officer_count(api->engine);
    for (u32 index = 0; index < count && found < capacity; ++index)
    {
        engine_officer officer = api->officer_at(api->engine, index);
        if (officer.status <= 3 && officer.force_id == force && officer.current_position == city_id &&
            officer.job_city == ENGINE_NO_CITY && !officer.acted && !officer.travel_days)
        {
            ids[found++] = officer.officer_id;
        }
    }
    return found;
}

internal void
ui_officer_label(ui_state *ui, engine_api *api, u16 officer_id, char *out, u32 out_size)
{
    if (officer_id == ENGINE_NO_OFFICER)
    {
        snprintf(out, out_size, "%s", text_get(ui->text, "develop.no_officer"));
        return;
    }
    u32 count = api->officer_count(api->engine);
    for (u32 index = 0; index < count; ++index)
    {
        engine_officer officer = api->officer_at(api->engine, index);
        if (officer.officer_id != officer_id) continue;
        char name[UI_NAME_SIZE * 2];
        ui_officer_name(ui, &officer, name, sizeof(name));
        snprintf(out, out_size, "%s (%u)", name, officer.politics);
        return;
    }
    snprintf(out, out_size, "%u", officer_id);
}

internal void
ui_draw_facility_combo(ui_state *ui, engine_api *api)
{
    char label[128];
    if (ImGui::BeginCombo(ui_label(label, sizeof(label), ui, "develop.facility", "facility"),
                          ui_facility_name(ui, api, ui->develop.facility)))
    {
        u32 count = api->facility_kind_count(api->engine);
        for (u32 index = 0; index < count; ++index)
        {
            engine_facility_kind kind = api->facility_kind_at(api->engine, index);
            snprintf(label, sizeof(label), "%s  %u###f%u", ui_text_with_key(ui, "facility", kind.key), kind.cost, kind.id);
            if (ImGui::Selectable(label, kind.id == ui->develop.facility)) ui->develop.facility = kind.id;
        }
        ImGui::EndCombo();
    }
}

internal void
ui_draw_slot_combo(ui_state *ui, engine_api *api)
{
    char label[128], preview[64];
    if (ui->develop.slot == ENGINE_ANY_SLOT) snprintf(preview, sizeof(preview), "%s", text_get(ui->text, "develop.any_slot"));
    else                                     snprintf(preview, sizeof(preview), "%u", ui->develop.slot);
    if (ImGui::BeginCombo(ui_label(label, sizeof(label), ui, "develop.slot", "slot"), preview))
    {
        snprintf(label, sizeof(label), "%s###s_any", text_get(ui->text, "develop.any_slot"));
        if (ImGui::Selectable(label, ui->develop.slot == ENGINE_ANY_SLOT)) ui->develop.slot = ENGINE_ANY_SLOT;
        engine_city_slots slots = api->city_slots(api->engine, ui->develop.city);
        for (u32 index = 0; index < slots.slot_count; ++index)
        {
            engine_facility slot = api->facility_at(api->engine, ui->develop.city, (u8)index);
            if (slot.state != SIM_SLOT_EMPTY) continue;
            snprintf(label, sizeof(label), "%u###s%u", index, index);
            if (ImGui::Selectable(label, ui->develop.slot == index)) ui->develop.slot = (u8)index;
        }
        ImGui::EndCombo();
    }
}

internal void
ui_draw_officer_combo(ui_state *ui, engine_api *api, u32 place, const u16 *free_ids, u32 free_count)
{
    char label[128], preview[96], combo[128];
    ui_officer_label(ui, api, ui->develop.officers[place], preview, sizeof(preview));
    snprintf(combo, sizeof(combo), "%s %u###officer%u", text_get(ui->text, "develop.officer"), place + 1, place + 1);
    ui_draw_officer_face(ui, api, ui->develop.officers[place], ImGui::GetFrameHeight());   /* M19 */
    if (ImGui::BeginCombo(combo, preview))
    {
        snprintf(label, sizeof(label), "%s###o_none", text_get(ui->text, "develop.no_officer"));
        if (ImGui::Selectable(label, ui->develop.officers[place] == ENGINE_NO_OFFICER)) ui->develop.officers[place] = ENGINE_NO_OFFICER;
        for (u32 index = 0; index < free_count; ++index)
        {
            char name[96];
            ui_officer_label(ui, api, free_ids[index], name, sizeof(name));
            snprintf(label, sizeof(label), "%s###o%u", name, free_ids[index]);
            ui_draw_officer_face(ui, api, free_ids[index], ImGui::GetTextLineHeight() * 1.5f);
            if (ImGui::Selectable(label, ui->develop.officers[place] == free_ids[index])) ui->develop.officers[place] = free_ids[index];
        }
        ImGui::EndCombo();
    }
}

/* M19: the abilities that rank the officers of a proposal. */
enum ui_ability
{
    UI_ABILITY_LEADERSHIP,
    UI_ABILITY_WAR,
    UI_ABILITY_INTELLIGENCE,
    UI_ABILITY_POLITICS,
    UI_ABILITY_CHARM,
};

internal u32
ui_officer_ability(engine_officer *officer, u32 ability)
{
    switch (ability)
    {
        case UI_ABILITY_LEADERSHIP:   return officer->leadership;
        case UI_ABILITY_WAR:          return officer->war;
        case UI_ABILITY_INTELLIGENCE: return officer->intelligence;
        case UI_ABILITY_POLITICS:     return officer->politics;
        case UI_ABILITY_CHARM:        return officer->charm;
        default:                      return 0;
    }
}

/* M19, the request of the player (2026-10-20): a dialog proposes the most
   suitable officers, as many as the command takes. The ability of the
   command ranks the free officers of the city. The list of the original
   shows that ability in its first column
   (spec/experiments/session-1-results-2026-10-20.md), and the original
   fills three slots (ui/clone-gap-report.md, section 4). A tie keeps the
   order of the officer table. Returns the count of the officers. */
internal u32
ui_propose_officers(engine_api *api, u8 city_id, u32 ability, u16 *out, u32 places)
{
    u16 free_ids[UI_DEVELOP_OFFICER_LIST];
    u32 free_count = ui_free_officers(api, city_id, free_ids, ArrayCount(free_ids));
    u32 chosen = 0;
    for (u32 place = 0; place < places; ++place) out[place] = ENGINE_NO_OFFICER;   /* a short list leaves the other slots empty */
    for (u32 place = 0; place < places; ++place)
    {
        u32 best = free_count, best_value = 0;
        for (u32 index = 0; index < free_count; ++index)
        {
            if (free_ids[index] == ENGINE_NO_OFFICER) continue;
            engine_officer officer = api->officer_by_id(api->engine, free_ids[index]);
            u32 value = ui_officer_ability(&officer, ability);
            if (best == free_count || value > best_value) { best = index; best_value = value; }
        }
        if (best == free_count) break;
        out[place] = free_ids[best];
        free_ids[best] = ENGINE_NO_OFFICER;
        chosen++;
    }
    return chosen;
}

/* The dialog keeps its choices while it shows one city. For a new city,
   or after a command, it proposes the three officers with the most
   politics (the list of 開發 shows 政治) and any free slot. */
internal void
ui_develop_reset_for_city(ui_state *ui, engine_api *api)
{
    ui->develop.slot = ENGINE_ANY_SLOT;
    ui_propose_officers(api, ui->develop.city, UI_ABILITY_POLITICS, ui->develop.officers, SIM_DEVELOP_OFFICERS);
    ui->develop.prepared_city = ui->develop.city;
}

internal void ui_draw_develop_game(ui_state *ui, engine_api *api);   /* ui_placement.cpp (M19) */

internal void
ui_draw_develop(ui_state *ui, engine_api *api)
{
    if (ui->hud_mode)
    {
        ui_draw_develop_game(ui, api);   /* M19: the grid of the original and the placement mode */
        return;
    }
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(420, 300), ImGuiCond_FirstUseEver);
    ui_context_place(ui, 1);   /* the game mode: the dialog opens next to its entity */
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "develop.title", "Develop"), &ui->show_develop))
    {
        ui_context_track(ui);
        if (ui->develop.prepared_city != ui->develop.city) ui_develop_reset_for_city(ui, api);
        char city_name[96];
        ui_city_label(ui, api, ui->develop.city, city_name, sizeof(city_name));
        ImGui::Text("%s: %s", text_get(ui->text, "city.select"), city_name);
        ui_draw_facility_combo(ui, api);
        ui_draw_slot_combo(ui, api);
        u16 free_ids[UI_DEVELOP_OFFICER_LIST];
        u32 free_count = ui_free_officers(api, ui->develop.city, free_ids, ArrayCount(free_ids));
        for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place) ui_draw_officer_combo(ui, api, place, free_ids, free_count);

        engine_command command = {};
        command.kind     = ENGINE_COMMAND_DEVELOP;
        command.city_id  = ui->develop.city;
        command.facility = ui->develop.facility;
        command.slot     = ui->develop.slot;
        for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place) command.officers[place] = ui->develop.officers[place];
        engine_check check = api->can_issue(api->engine, &command);
        ImGui::Separator();
        if (check.refusal == SIM_REFUSAL_NONE)
        {
            ImGui::Text("%s: %u   %s: %u %s   %s: %u", text_get(ui->text, "develop.cost"), check.gold,
                        text_get(ui->text, "develop.period"), check.days, text_get(ui->text, "develop.days"),
                        text_get(ui->text, "develop.action_points"), check.action_points);
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "%s: %s", text_get(ui->text, "develop.refused"),
                               ui_refusal_text(ui, check.refusal));
        }

        /* A refused command is a disabled button: the click sends nothing. */
        char label[96];
        ImGui::BeginDisabled(check.refusal != SIM_REFUSAL_NONE);
        if (ImGui::Button(ui_label(label, sizeof(label), ui, "develop.execute", "execute")))
        {
            engine_command_result result = api->command_develop(api->engine, command.city_id, command.facility,
                                                                command.officers, command.slot);
            if (result.refusal == SIM_REFUSAL_NONE)
            {
                snprintf(ui->develop.message, sizeof(ui->develop.message), "%s %u", text_get(ui->text, "develop.started"), result.command_id);
                ui->develop.prepared_city = ENGINE_NO_CITY;   /* take the next free officer */
            }
            else
            {
                snprintf(ui->develop.message, sizeof(ui->develop.message), "%s: %s", text_get(ui->text, "develop.refused"),
                         ui_refusal_text(ui, result.refusal));
            }
        }
        ImGui::EndDisabled();
        if (ui->develop.message[0])
        {
            ImGui::SameLine();
            ImGui::TextUnformatted(ui->develop.message);
        }
    }
    ImGui::End();
}
