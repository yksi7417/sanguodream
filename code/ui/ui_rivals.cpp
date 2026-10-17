/* The panels of M8 (rivals; ui/screen-flow.md 君主 submenu, ui/information-
   screens.md, ui/main-map.md): the Ruler panel (the emperor, 軍團, 軍師 and
   the advice, 評定), the Bases panel (港關), the event dialog, and the
   bases on the mini map. The UI calls engine_api only. */

internal const char *
ui_proposal_text(ui_state *ui, engine_api *api, engine_proposal *proposal, char *out, u32 out_size)
{
    char city[UI_NAME_SIZE * 2] = "-", officer[UI_NAME_SIZE * 2] = "-";
    if (proposal->city_id != ENGINE_NO_CITY) ui_city_label(ui, api, proposal->city_id, city, sizeof(city));
    if (proposal->officer != ENGINE_NO_OFFICER) ui_officer_text(ui, api, proposal->officer, officer, sizeof(officer));
    snprintf(out, out_size, "%s  %s  %s  (%u)", ui_text_with_key(ui, "ai", proposal->key), city, officer, proposal->score);
    return out;
}

/* The emperor, his city and his protector. */
internal void
ui_draw_emperor_line(ui_state *ui, engine_api *api)
{
    engine_emperor emperor = api->emperor(api->engine);
    char who[64], city[UI_NAME_SIZE * 2] = "-", protector[UI_NAME_SIZE * 2] = "-";
    text_label(ui->text, "emperor", emperor.who, who, sizeof(who));
    if (emperor.city != ENGINE_NO_CITY) ui_city_label(ui, api, emperor.city, city, sizeof(city));
    if (emperor.protector != ENGINE_NO_FACTION) ui_force_text(ui, api, emperor.protector, protector, sizeof(protector));
    ImGui::Text("%s: %s   %s   %s: %s", text_get(ui->text, "ruler.emperor"), who, city, text_get(ui->text, "ruler.protector"), protector);
}

internal void
ui_corps_result(ui_state *ui, engine_command_result result)
{
    ui_growth_result(ui, "ruler.corps", result);
}

/* 軍團: the corps of the faction, and the form that creates or changes one. */
internal void
ui_draw_corps_section(ui_state *ui, engine_api *api, u8 force)
{
    char label[96];
    void *e = api->engine;
    if (ImGui::BeginTable("corps_table", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn(text_get(ui->text, "ruler.corps_number"));
        ImGui::TableSetupColumn(text_get(ui->text, "ruler.viceroy"));
        ImGui::TableSetupColumn(text_get(ui->text, "ruler.cities"));
        ImGui::TableSetupColumn(text_get(ui->text, "ruler.policy"));
        ImGui::TableSetupColumn(text_get(ui->text, "ruler.points"));
        ImGui::TableHeadersRow();
        for (u8 index = 1; index < SIM_CORPS_CAPACITY; ++index)
        {
            engine_corps corps = api->corps_at(e, force, index);
            if (!corps.alive) continue;
            char name[UI_NAME_SIZE * 2], policy[64], row[32];
            ui_officer_text(ui, api, corps.viceroy, name, sizeof(name));
            text_label(ui->text, "corps_policy", corps.policy, policy, sizeof(policy));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            snprintf(row, sizeof(row), "%u###corps%u", index, index);
            if (ImGui::Selectable(row, ui->ruler.corps == index, ImGuiSelectableFlags_SpanAllColumns)) ui->ruler.corps = index;
            ImGui::TableNextColumn(); ImGui::TextUnformatted(name);
            ImGui::TableNextColumn(); ImGui::Text("%u", corps.city_count);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(policy);
            ImGui::TableNextColumn(); ImGui::Text("%u", corps.action_points);
        }
        ImGui::EndTable();
    }
    ui_officer_pick(ui, api, "ruler.viceroy", "viceroy", &ui->ruler.viceroy, UI_OFFICERS_OWN, ENGINE_NO_FACTION);
    char cities[160] = "";
    u32 used = 0, chosen = 0;
    for (u32 city = 0; city < SAVE_CITY_COUNT; ++city)
    {
        if (!((ui->ruler.cities >> city) & 1)) continue;
        chosen++;
        if (used < sizeof(cities) - 8) used += (u32)snprintf(cities + used, sizeof(cities) - used, "%s%u", used ? " " : "", city);
    }
    if (!chosen) snprintf(cities, sizeof(cities), "-");
    if (ImGui::BeginCombo(ui_label(label, sizeof(label), ui, "ruler.cities", "corps_cities"), cities))
    {
        for (u32 index = 0; index < api->city_count(e); ++index)
        {
            engine_city city = api->city_at(e, index);
            if (city.owner_force_id != force) continue;
            char name[UI_NAME_SIZE * 2], row[128];
            ui_city_label(ui, api, city.city_id, name, sizeof(name));
            snprintf(row, sizeof(row), "%s###rc%u", name, city.city_id);
            b32 on = (ui->ruler.cities >> city.city_id) & 1;
            if (ImGui::Selectable(row, on != 0, ImGuiSelectableFlags_NoAutoClosePopups)) ui->ruler.cities ^= 1ull << city.city_id;
        }
        ImGui::EndCombo();
    }
    for (int policy = 0; policy < CORPS_POLICY_COUNT; ++policy)
    {
        char text[64], stable[32];
        text_label(ui->text, "corps_policy", (u32)policy, text, sizeof(text));
        snprintf(stable, sizeof(stable), "%s###policy%d", text, policy);
        if (policy) ImGui::SameLine();
        ImGui::RadioButton(stable, &ui->ruler.policy, policy);
    }
    for (u32 flag = 0; flag < 7; ++flag)
    {
        char text[64], stable[32];
        text_label(ui->text, "corps_flag", flag, text, sizeof(text));
        snprintf(stable, sizeof(stable), "%s###flag%u", text, flag);
        if (flag % 4) ImGui::SameLine();
        ImGui::CheckboxFlags(stable, &ui->ruler.flags, 1u << flag);
    }
    sim_corps_order order = {};
    order.viceroy        = ui->ruler.viceroy;
    order.cities         = ui->ruler.cities;
    order.policy         = (u8)ui->ruler.policy;
    order.target         = ENGINE_NO_FACTION;
    order.flags          = ui->ruler.flags;
    order.transport_city = ENGINE_NO_CITY;
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "ruler.create", "corps_create")))
    {
        order.action = CORPS_CREATE;
        ui_corps_result(ui, api->command_corps(e, &order));
    }
    ImGui::SameLine();
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "ruler.change", "corps_change")))
    {
        order.action = CORPS_CHANGE;
        order.corps  = ui->ruler.corps;
        ui_corps_result(ui, api->command_corps(e, &order));
    }
    ImGui::SameLine();
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "ruler.dissolve", "corps_dissolve")))
    {
        order.action = CORPS_DISSOLVE;
        order.corps  = ui->ruler.corps;
        ui_corps_result(ui, api->command_corps(e, &order));
    }
}

/* 軍師: the adviser and the advice. */
internal void
ui_draw_adviser_section(ui_state *ui, engine_api *api, u8 force)
{
    char label[96], name[UI_NAME_SIZE * 2] = "-";
    void *e = api->engine;
    u16 adviser = api->adviser(e, force);
    if (adviser != ENGINE_NO_OFFICER) ui_officer_text(ui, api, adviser, name, sizeof(name));
    ImGui::Text("%s: %s", text_get(ui->text, "ruler.adviser"), name);
    ui_officer_pick(ui, api, "ruler.adviser_pick", "adviser_pick", &ui->ruler.adviser, UI_OFFICERS_OWN, ENGINE_NO_FACTION);
    ImGui::SameLine();
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "ruler.appoint_adviser", "appoint_adviser")))
    {
        ui_growth_result(ui, "ruler.adviser", api->command_adviser(e, force, ui->ruler.adviser));
    }
    engine_plan_view advice = api->advice(e, force);
    if (!advice.count) { ImGui::TextDisabled("%s", text_get(ui->text, "ruler.no_advice")); return; }
    for (u32 index = 0; index < advice.count; ++index)
    {
        char text[256];
        ImGui::BulletText("%s", ui_proposal_text(ui, api, advice.items + index, text, sizeof(text)));
    }
}

/* 評定: the proposals of the council in the city of the ruler; a click acts on one. */
internal void
ui_draw_council_section(ui_state *ui, engine_api *api, u8 force)
{
    void *e = api->engine;
    engine_officer ruler = api->officer_by_id(e, api->force_ruler(e, force));
    u8 city = ruler.officer_id != ENGINE_NO_OFFICER && ruler.current_position < SAVE_CITY_COUNT ? (u8)ruler.current_position : (u8)ENGINE_NO_CITY;
    engine_plan_view council = api->council(e, force, city);
    if (council.refusal != SIM_REFUSAL_NONE) { ImGui::TextDisabled("%s", ui_refusal_text(ui, council.refusal)); return; }
    char name[UI_NAME_SIZE * 2];
    ui_city_label(ui, api, city, name, sizeof(name));
    ImGui::Text("%s: %s", text_get(ui->text, "ruler.council_city"), name);
    for (u32 index = 0; index < council.count; ++index)
    {
        char text[256], button[300];
        snprintf(button, sizeof(button), "%s###council%u", ui_proposal_text(ui, api, council.items + index, text, sizeof(text)), index);
        if (ImGui::Button(button)) ui_growth_result(ui, "ruler.council", api->command_council(e, force, city, (u8)index));
    }
}

internal void
ui_draw_ruler(ui_state *ui, engine_api *api)
{
    char title[128], label[96];
    ImGui::SetNextWindowSize(ImVec2(480, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "ruler.title", "Ruler"), &ui->show_ruler)) { ImGui::End(); return; }
    u8 force = api->whose_turn(api->engine);
    if (force == ENGINE_NO_FACTION)
    {
        ImGui::TextUnformatted(text_get(ui->text, "court.no_faction"));
        ImGui::End();
        return;
    }
    ui_draw_emperor_line(ui, api);
    if (ImGui::CollapsingHeader(ui_label(label, sizeof(label), ui, "ruler.corps", "corps_header"), ImGuiTreeNodeFlags_DefaultOpen)) ui_draw_corps_section(ui, api, force);
    if (ImGui::CollapsingHeader(ui_label(label, sizeof(label), ui, "ruler.adviser", "adviser_header"), ImGuiTreeNodeFlags_DefaultOpen)) ui_draw_adviser_section(ui, api, force);
    if (ImGui::CollapsingHeader(ui_label(label, sizeof(label), ui, "ruler.council", "council_header"), ImGuiTreeNodeFlags_DefaultOpen)) ui_draw_council_section(ui, api, force);
    if (ui->command_message[0]) ImGui::TextWrapped("%s", ui->command_message);
    ImGui::End();
}

/* ---- 港關 ---------------------------------------------------------------------------- */

internal void
ui_draw_bases(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(560, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "bases.title", "Bases"), &ui->show_bases)) { ImGui::End(); return; }
    void *e = api->engine;
    if (ImGui::BeginTable("bases_table", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(text_get(ui->text, "bases.name"));
        ImGui::TableSetupColumn(text_get(ui->text, "bases.kind"));
        ImGui::TableSetupColumn(text_get(ui->text, "bases.city"));
        ImGui::TableSetupColumn(text_get(ui->text, "bases.faction"));
        ImGui::TableSetupColumn(text_get(ui->text, "bases.durability"));
        ImGui::TableSetupColumn(text_get(ui->text, "bases.troops"));
        ImGui::TableHeadersRow();
        for (u32 index = 0; index < api->base_count(e); ++index)
        {
            engine_base base = api->base_at(e, index);
            if (base.id == ENGINE_NO_ROW) continue;
            char kind[48], city[UI_NAME_SIZE * 2], force[UI_NAME_SIZE * 2] = "-";
            text_label(ui->text, "base_type", base.type, kind, sizeof(kind));
            ui_city_label(ui, api, base.owner_city, city, sizeof(city));
            if (base.force != ENGINE_NO_FACTION) ui_force_text(ui, api, base.force, force, sizeof(force));
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%u %s", base.id, ui_text_with_key(ui, "base", base.key));
            ImGui::TableNextColumn(); ImGui::TextUnformatted(kind);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(city);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(force);
            ImGui::TableNextColumn(); ImGui::Text("%u / %u", base.durability, base.durability_max);
            ImGui::TableNextColumn(); ImGui::Text("%u", base.troops);
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

/* The bases on the mini map: a hollow square in the colour of the owner. */
internal void
ui_draw_minimap_bases(engine_api *api, ImDrawList *draw, ImVec2 base_pos, f32 cell)
{
    for (u32 index = 0; index < api->base_count(api->engine); ++index)
    {
        engine_base base = api->base_at(api->engine, index);
        if (base.id == ENGINE_NO_ROW) continue;
        hex_offset o = hex_to_offset(hex_make(base.q, base.r));
        ImVec2 corner = ui_minimap_point(base_pos, o.col, o.row, cell);
        f32 x = corner.x, y = corner.y;
        draw->AddRect(ImVec2(x - 0.5f * cell, y - 0.5f * cell), ImVec2(x + 1.5f * cell, y + 1.5f * cell), ui_faction_color(base.force), 0.0f, 0, 1.5f);
    }
}

/* ---- the event dialog ---------------------------------------------------------------------- */

/* An event that waits for a choice opens this dialog; the choice closes it. */
internal void
ui_draw_event_dialog(ui_state *ui, engine_api *api)
{
    void *e = api->engine;
    engine_pending_event pending = api->pending_event(e);
    if (!pending.active) return;
    engine_event_kind kind = api->event_kind_at(e, pending.event);
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(420, 220), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "event.title", "Event"))) { ImGui::End(); return; }
    ImGui::TextUnformatted(ui_text_with_key(ui, "event", kind.key));
    char name[UI_NAME_SIZE * 2], role[64];
    if (pending.officer_a != ENGINE_NO_OFFICER)
    {
        ui_officer_text(ui, api, pending.officer_a, name, sizeof(name));
        snprintf(role, sizeof(role), "event_role.%s.a", kind.key);
        ImGui::Text("%s: %s", text_get(ui->text, role), name);
    }
    if (pending.officer_b != ENGINE_NO_OFFICER)
    {
        ui_officer_text(ui, api, pending.officer_b, name, sizeof(name));
        snprintf(role, sizeof(role), "event_role.%s.b", kind.key);
        ImGui::Text("%s: %s", text_get(ui->text, role), name);
    }
    ImGui::TextWrapped("%s", ui_text_with_key(ui, "event_question", kind.key));
    for (u32 choice = 0; choice < pending.choices; ++choice)
    {
        char id[64], button[160];
        snprintf(id, sizeof(id), "event_choice.%s.%u", kind.key, choice);
        snprintf(button, sizeof(button), "%s###choice%u", text_get(ui->text, id), choice);
        if (ImGui::Button(button)) ui_growth_result(ui, "event.title", api->command_event_choice(e, (u8)choice));
    }
    ImGui::End();
}
