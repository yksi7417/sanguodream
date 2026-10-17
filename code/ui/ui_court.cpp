/* The panels of M7 (court; ui/city-commands.md, ui/duel.md, debate.md): the
   Court panel (an officer, its state, 褒賞, 官職, 流放, 探索, 登庸, 授與,
   仲介, the captives), the Diplomacy panel (the factions, the diplomacy
   commands and the plots of factions), and the duel and debate screens.
   The UI calls engine_api only. */

/* The city of an officer, or ENGINE_NO_CITY if it is not in a city. */
internal u8
ui_officer_city(engine_api *api, u16 officer_id)
{
    engine_officer officer = api->officer_by_id(api->engine, officer_id);
    if (officer.officer_id == ENGINE_NO_OFFICER || officer.current_position >= SAVE_CITY_COUNT) return ENGINE_NO_CITY;
    return (u8)officer.current_position;
}

enum ui_officer_filter
{
    UI_OFFICERS_OWN,         /* active officers of the acting faction */
    UI_OFFICERS_RECRUITABLE, /* officers that the acting faction can recruit now */
    UI_OFFICERS_CAPTIVES,    /* captives that the acting faction holds */
    UI_OFFICERS_HELD,        /* own officers that the diplomacy target holds */
};

internal b32
ui_officer_matches(engine_api *api, engine_officer *officer, u8 force, u8 filter, u8 other)
{
    switch (filter)
    {
        case UI_OFFICERS_OWN:         return officer->force_id == force && officer->status <= 3;
        case UI_OFFICERS_RECRUITABLE: return api->officer_detail(api->engine, officer->officer_id).recruit_chance > 0;
        case UI_OFFICERS_CAPTIVES:    return officer->force_id == force && api->officer_detail(api->engine, officer->officer_id).captive;
        case UI_OFFICERS_HELD:
        {
            engine_officer_detail detail = api->officer_detail(api->engine, officer->officer_id);
            return officer->force_id == other && detail.captive && detail.former_force == force;
        }
        default: return false;
    }
}

/* A combo of officers; each row has the stable id ###o<officer id>. */
internal void
ui_officer_pick(ui_state *ui, engine_api *api, const char *text_id, const char *stable_id, u16 *value, u8 filter, u8 other)
{
    char label[96], preview[UI_NAME_SIZE * 2] = "-";
    u8 force = api->whose_turn(api->engine);
    if (*value != ENGINE_NO_OFFICER) ui_officer_text(ui, api, *value, preview, sizeof(preview));
    if (!ImGui::BeginCombo(ui_label(label, sizeof(label), ui, text_id, stable_id), preview)) return;
    for (u32 index = 0; index < api->officer_count(api->engine); ++index)
    {
        engine_officer officer = api->officer_at(api->engine, index);
        if (!ui_officer_matches(api, &officer, force, filter, other)) continue;
        char name[UI_NAME_SIZE * 2], row[128];
        ui_officer_name(ui, &officer, name, sizeof(name));
        snprintf(row, sizeof(row), "%s###o%u", name, officer.officer_id);
        if (ImGui::Selectable(row, *value == officer.officer_id)) *value = officer.officer_id;
    }
    ImGui::EndCombo();
}

/* A button that sends one court command and shows its refusal before. */
internal void
ui_court_button(ui_state *ui, engine_api *api, const char *text_id, const char *stable_id, engine_command_kind kind, engine_court_order *order)
{
    char label[96];
    engine_command command = {};
    command.kind  = kind;
    command.court = *order;
    engine_check check = api->can_issue(api->engine, &command);
    ImGui::BeginDisabled(check.refusal != SIM_REFUSAL_NONE);
    if (ImGui::Button(ui_label(label, sizeof(label), ui, text_id, stable_id)))
    {
        engine_command_result result = api->command_court(api->engine, kind, order);
        ui_growth_result(ui, text_id, result);
    }
    ImGui::EndDisabled();
    if (check.refusal != SIM_REFUSAL_NONE && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::SetTooltip("%s", ui_refusal_text(ui, check.refusal));
    }
}

internal engine_court_order
ui_court_order(engine_api *api, u16 officer, u16 target)
{
    engine_court_order order = {};
    order.city_id     = ui_officer_city(api, officer);
    order.officer     = officer;
    order.target      = target;
    order.officers[0] = order.officers[1] = order.officers[2] = ENGINE_NO_OFFICER;
    order.item        = ENGINE_NO_ROW;
    order.post        = ENGINE_NO_ROW;
    order.force       = order.force_2 = ENGINE_NO_FACTION;
    order.target_city = ENGINE_NO_CITY;
    return order;
}

/* The state of the selected officer. */
internal void
ui_draw_officer_state(ui_state *ui, engine_api *api, u16 officer_id)
{
    engine_officer_detail detail = api->officer_detail(api->engine, officer_id);
    engine_officer officer = api->officer_by_id(api->engine, officer_id);
    if (detail.officer_id == ENGINE_NO_OFFICER) return;
    char text[64], post[96] = "-";
    ImGui::Text("%s %u   %s %u   %s %u", text_get(ui->text, "court.loyalty"), detail.loyalty, text_get(ui->text, "court.merit"), officer.merit,
                text_get(ui->text, "court.age"), detail.age);
    text_label(ui->text, "personality", detail.personality, text, sizeof(text));
    ImGui::Text("%s: %s   %s %u   %s %u", text_get(ui->text, "court.personality"), text, text_get(ui->text, "court.ambition"), detail.ambition,
                text_get(ui->text, "court.justice"), detail.justice);
    text_label(ui->text, "health", detail.health, text, sizeof(text));
    ImGui::Text("%s: %s   %s %u", text_get(ui->text, "court.health"), text, text_get(ui->text, "court.stamina"), detail.stamina);
    if (detail.post != ENGINE_NO_ROW) snprintf(post, sizeof(post), "%s", ui_text_with_key(ui, "post", api->post_kind_at(api->engine, detail.post).key));
    ImGui::Text("%s: %s   %s %u", text_get(ui->text, "court.post"), post, text_get(ui->text, "court.soldiers"), detail.soldier_cap);
    char names[UI_NAME_SIZE * 2];
    ui_officer_text(ui, api, officer.spouse, names, sizeof(names));
    ImGui::Text("%s: %s", text_get(ui->text, "court.spouse"), names);
    ImGui::Text("%s:", text_get(ui->text, "court.liked"));
    for (u32 slot = 0; slot < OFFICER_RELATION_SLOTS; ++slot)
    {
        if (detail.liked[slot] == ENGINE_NO_OFFICER) continue;
        ui_officer_text(ui, api, detail.liked[slot], names, sizeof(names));
        ImGui::SameLine();
        ImGui::TextUnformatted(names);
    }
    ImGui::Text("%s:", text_get(ui->text, "court.disliked"));
    for (u32 slot = 0; slot < OFFICER_RELATION_SLOTS; ++slot)
    {
        if (detail.disliked[slot] == ENGINE_NO_OFFICER) continue;
        ui_officer_text(ui, api, detail.disliked[slot], names, sizeof(names));
        ImGui::SameLine();
        ImGui::TextUnformatted(names);
    }
}

internal void
ui_draw_court(ui_state *ui, engine_api *api)
{
    char title[128], label[96];
    ImGui::SetNextWindowSize(ImVec2(420, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "court.title", "Court"), &ui->show_court)) { ImGui::End(); return; }
    void *e = api->engine;
    u8 force = api->whose_turn(e);
    if (force == ENGINE_NO_FACTION)
    {
        ImGui::TextUnformatted(text_get(ui->text, "court.no_faction"));
        ImGui::End();
        return;
    }
    engine_rank_kind rank = api->rank_kind_at(e, api->faction_rank(e, force));
    ImGui::Text("%s: %s (%u)", text_get(ui->text, "court.rank"), ui_text_with_key(ui, "rank", rank.key), rank.soldiers);
    ui_officer_pick(ui, api, "court.officer", "court_officer", &ui->court.officer, UI_OFFICERS_OWN, ENGINE_NO_FACTION);
    u16 officer = ui->court.officer;
    if (officer != ENGINE_NO_OFFICER) ui_draw_officer_state(ui, api, officer);
    ImGui::Separator();

    /* 褒賞, 官職, 流放 of the selected officer */
    engine_court_order reward = ui_court_order(api, officer, ENGINE_NO_OFFICER);
    reward.officers[0] = officer;
    ui_court_button(ui, api, "court.reward", "reward", ENGINE_COMMAND_REWARD, &reward);
    ImGui::SameLine();
    engine_court_order exile = ui_court_order(api, officer, officer);
    exile.city_id = ui->city.selected;
    ui_court_button(ui, api, "court.exile", "exile", ENGINE_COMMAND_EXILE, &exile);
    char post_preview[96] = "-";
    if (ui->court.post != ENGINE_NO_ROW) snprintf(post_preview, sizeof(post_preview), "%s", ui_text_with_key(ui, "post", api->post_kind_at(e, ui->court.post).key));
    if (ImGui::BeginCombo(ui_label(label, sizeof(label), ui, "court.post", "post"), post_preview))
    {
        for (u32 row = 0; row < api->post_kind_count(e); ++row)
        {
            engine_post_kind kind = api->post_kind_at(e, row);
            char item[160];
            snprintf(item, sizeof(item), "%s  %u / %u###p%u", ui_text_with_key(ui, "post", kind.key), kind.merit, kind.soldiers, row);
            if (ImGui::Selectable(item, ui->court.post == row)) ui->court.post = (u8)row;
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    engine_court_order appoint = ui_court_order(api, officer, officer);
    appoint.city_id = ui->city.selected;
    appoint.post    = ui->court.post;
    ui_court_button(ui, api, "court.appoint", "appoint", ENGINE_COMMAND_APPOINT, &appoint);
    ImGui::Separator();

    /* 探索, 登庸 by the selected officer */
    engine_court_order search = ui_court_order(api, officer, ENGINE_NO_OFFICER);
    ui_court_button(ui, api, "court.search", "search", ENGINE_COMMAND_SEARCH, &search);
    ui_officer_pick(ui, api, "court.recruit_target", "recruit_target", &ui->court.target, UI_OFFICERS_RECRUITABLE, ENGINE_NO_FACTION);
    if (ui->court.target != ENGINE_NO_OFFICER)
    {
        ImGui::SameLine();
        ImGui::Text("%s %u%%", text_get(ui->text, "court.chance"), api->officer_detail(e, ui->court.target).recruit_chance);
    }
    engine_court_order recruit = ui_court_order(api, officer, ui->court.target);
    ui_court_button(ui, api, "court.recruit", "recruit", ENGINE_COMMAND_RECRUIT, &recruit);
    ImGui::Separator();

    /* 授與 */
    char item_preview[96] = "-";
    if (ui->court.item != ENGINE_NO_ROW)
    {
        engine_item held = api->item_at(e, ui->court.item);
        text_name(ui->text, held.name, sizeof(held.name), item_preview, sizeof(item_preview));
    }
    if (ImGui::BeginCombo(ui_label(label, sizeof(label), ui, "court.item", "item"), item_preview))
    {
        for (u32 index = 0; index < api->item_count(e); ++index)
        {
            engine_item held = api->item_at(e, index);
            if (held.id == ENGINE_NO_ROW) continue;
            engine_officer owner = api->officer_by_id(e, held.owner);
            b32 ours = owner.officer_id != ENGINE_NO_OFFICER ? owner.force_id == force : false;
            if (!ours && held.city != ENGINE_NO_CITY) ours = api->city_at(e, 0).city_id == held.city ? api->city_at(e, 0).owner_force_id == force : false;
            if (!ours) continue;
            char name[96], row[128];
            text_name(ui->text, held.name, sizeof(held.name), name, sizeof(name));
            snprintf(row, sizeof(row), "%s (%u)###i%u", name, held.strength, held.id);
            if (ImGui::Selectable(row, ui->court.item == held.id)) ui->court.item = held.id;
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    engine_court_order give = ui_court_order(api, officer, officer);
    give.city_id = ui->city.selected;
    give.item    = ui->court.item;
    ui_court_button(ui, api, "court.give", "give", ENGINE_COMMAND_GIVE_ITEM, &give);
    ImGui::Separator();

    /* 仲介 */
    ui_officer_pick(ui, api, "court.partner", "partner", &ui->court.partner, UI_OFFICERS_OWN, ENGINE_NO_FACTION);
    ImGui::RadioButton(ui_label(label, sizeof(label), ui, "court.sworn_kind", "sworn"), &ui->court.match_kind, SIM_MATCH_SWORN);
    ImGui::SameLine();
    ImGui::RadioButton(ui_label(label, sizeof(label), ui, "court.marriage_kind", "marriage"), &ui->court.match_kind, SIM_MATCH_MARRIAGE);
    engine_court_order match = ui_court_order(api, officer, ui->court.partner);
    match.city_id = ui->city.selected;
    match.kind    = (u8)ui->court.match_kind;
    ui_court_button(ui, api, "court.matchmake", "matchmake", ENGINE_COMMAND_MATCHMAKE, &match);
    ImGui::Separator();

    /* 流放 of a captive */
    ui_officer_pick(ui, api, "court.captive", "captive", &ui->court.captive, UI_OFFICERS_CAPTIVES, ENGINE_NO_FACTION);
    ImGui::SameLine();
    engine_court_order release = ui_court_order(api, ENGINE_NO_OFFICER, ui->court.captive);
    release.city_id = ui->city.selected;
    ui_court_button(ui, api, "court.release", "release", ENGINE_COMMAND_RELEASE, &release);
    if (ui->command_message[0]) ImGui::TextWrapped("%s", ui->command_message);
    ImGui::End();
}

/* ---- the Diplomacy panel ---------------------------------------------------------- */

internal void
ui_draw_diplomacy(ui_state *ui, engine_api *api)
{
    char title[128], label[96];
    ImGui::SetNextWindowSize(ImVec2(460, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "diplomacy.title", "Diplomacy"), &ui->show_diplomacy)) { ImGui::End(); return; }
    void *e = api->engine;
    u8 force = api->whose_turn(e);
    if (force == ENGINE_NO_FACTION)
    {
        ImGui::TextUnformatted(text_get(ui->text, "court.no_faction"));
        ImGui::End();
        return;
    }
    if (ImGui::BeginTable("factions", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 160)))
    {
        ui_table_column(ui, "diplomacy.faction", "faction");
        ui_table_column(ui, "diplomacy.friendliness", "friendliness");
        ui_table_column(ui, "diplomacy.relation", "relation");
        ui_table_column(ui, "diplomacy.months", "months");
        ImGui::TableHeadersRow();
        for (u32 index = 0; index < api->faction_count(e); ++index)
        {
            engine_faction faction = api->faction_at(e, index);
            if (faction.force_id == force || faction.force_id == ENGINE_NO_FACTION) continue;
            engine_diplomacy state = api->diplomacy(e, force, faction.force_id);
            char name[UI_NAME_SIZE * 2], row[128], level[64], relation[64];
            ui_force_text(ui, api, faction.force_id, name, sizeof(name));
            text_label(ui->text, "level", state.level, level, sizeof(level));
            text_label(ui->text, "relation", state.relation, relation, sizeof(relation));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            snprintf(row, sizeof(row), "%s###f%u", name, faction.force_id);
            if (ImGui::Selectable(row, ui->diplomacy.force == faction.force_id, ImGuiSelectableFlags_SpanAllColumns)) ui->diplomacy.force = faction.force_id;
            ImGui::TableNextColumn(); ImGui::Text("%s (%u)", level, state.friendliness);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(relation);
            ImGui::TableNextColumn(); ImGui::Text("%u", state.months);
        }
        ImGui::EndTable();
    }
    ui_officer_pick(ui, api, "diplomacy.envoy", "envoy", &ui->court.officer, UI_OFFICERS_OWN, ENGINE_NO_FACTION);
    ImGui::InputInt(ui_label(label, sizeof(label), ui, "diplomacy.gold", "dip_gold"), &ui->diplomacy.gold, 100, 1000);
    ImGui::InputInt(ui_label(label, sizeof(label), ui, "diplomacy.months", "dip_months"), &ui->diplomacy.months, 1, 6);
    if (ui->diplomacy.gold < 0) ui->diplomacy.gold = 0;
    if (ui->diplomacy.months < 0) ui->diplomacy.months = 0;
    engine_court_order order = ui_court_order(api, ui->court.officer, ENGINE_NO_OFFICER);
    order.force  = ui->diplomacy.force;
    order.gold   = (u32)ui->diplomacy.gold;
    order.months = (u32)ui->diplomacy.months;
    order.target_city = order.city_id;
    static const char *kinds[SIM_DIPLOMACY_KIND_COUNT] = { "goodwill", "alliance", "break", "ceasefire", "surrender", "exchange", "aid" };
    for (u32 kind = 0; kind < SIM_DIPLOMACY_KIND_COUNT; ++kind)
    {
        if (kind == SIM_DIPLOMACY_EXCHANGE) continue;
        char text_id[48];
        snprintf(text_id, sizeof(text_id), "diplomacy.%s", kinds[kind]);
        order.kind = (u8)kind;
        ui_court_button(ui, api, text_id, kinds[kind], ENGINE_COMMAND_DIPLOMACY, &order);
        if (kind % 3 != 2) ImGui::SameLine();
    }
    ImGui::NewLine();
    ui_officer_pick(ui, api, "diplomacy.captive", "held", &ui->diplomacy.captive, UI_OFFICERS_HELD, ui->diplomacy.force);
    ImGui::SameLine();
    engine_court_order exchange = order;
    exchange.kind   = SIM_DIPLOMACY_EXCHANGE;
    exchange.target = ui->diplomacy.captive;
    ui_court_button(ui, api, "diplomacy.exchange", "exchange", ENGINE_COMMAND_DIPLOMACY, &exchange);
    ImGui::Separator();

    /* The plots of factions */
    char second[UI_NAME_SIZE * 2] = "-";
    if (ui->diplomacy.second != ENGINE_NO_FACTION) ui_force_text(ui, api, ui->diplomacy.second, second, sizeof(second));
    if (ImGui::BeginCombo(ui_label(label, sizeof(label), ui, "diplomacy.second", "second"), second))
    {
        for (u32 index = 0; index < api->faction_count(e); ++index)
        {
            engine_faction faction = api->faction_at(e, index);
            if (faction.force_id == force || faction.force_id == ui->diplomacy.force) continue;
            char name[UI_NAME_SIZE * 2], row[128];
            ui_force_text(ui, api, faction.force_id, name, sizeof(name));
            snprintf(row, sizeof(row), "%s###s%u", name, faction.force_id);
            if (ImGui::Selectable(row, ui->diplomacy.second == faction.force_id)) ui->diplomacy.second = faction.force_id;
        }
        ImGui::EndCombo();
    }
    char city_text[UI_NAME_SIZE * 2] = "-";
    if (ui->diplomacy.city != ENGINE_NO_CITY) ui_location_text(ui, api, ui->diplomacy.city, city_text, sizeof(city_text));
    if (ImGui::BeginCombo(ui_label(label, sizeof(label), ui, "diplomacy.city", "plot_city"), city_text))
    {
        for (u32 index = 0; index < api->city_count(e); ++index)
        {
            engine_city city = api->city_at(e, index);
            if (city.owner_force_id != ui->diplomacy.force) continue;
            char name[UI_NAME_SIZE * 2], row[128];
            ui_location_text(ui, api, city.city_id, name, sizeof(name));
            snprintf(row, sizeof(row), "%s###c%u", name, city.city_id);
            if (ImGui::Selectable(row, ui->diplomacy.city == city.city_id)) ui->diplomacy.city = city.city_id;
        }
        ImGui::EndCombo();
    }
    engine_court_order plot = order;
    plot.force_2     = ui->diplomacy.second;
    plot.target_city = ui->diplomacy.city;
    static const char *plots[SIM_FACTION_PLOT_COUNT] = { "two_tigers", "drive_tiger", "rumour" };
    for (u32 kind = 0; kind < SIM_FACTION_PLOT_COUNT; ++kind)
    {
        char text_id[48];
        snprintf(text_id, sizeof(text_id), "plot_cmd.%s", plots[kind]);
        plot.kind = (u8)kind;
        ui_court_button(ui, api, text_id, plots[kind], ENGINE_COMMAND_FACTION_PLOT, &plot);
        if (kind + 1 < SIM_FACTION_PLOT_COUNT) ImGui::SameLine();
    }
    if (ui->command_message[0]) ImGui::TextWrapped("%s", ui->command_message);
    ImGui::End();
}

/* ---- the duel screen ------------------------------------------------------------------ */

internal void
ui_draw_duel_side(ui_state *ui, engine_api *api, engine_duel_side *side, const char *stable)
{
    char name[UI_NAME_SIZE * 2], overlay[64];
    ImGui::BeginGroup();
    ImGui::PushID(stable);
    ui_officer_text(ui, api, side->officers[side->fighter], name, sizeof(name));
    engine_officer fighter = api->officer_by_id(api->engine, side->officers[side->fighter]);
    ImGui::Text("%s  %s %u", name, text_get(ui->text, "duel.force"), fighter.war);
    snprintf(overlay, sizeof(overlay), "%s %u", text_get(ui->text, "duel.stamina"), side->stamina[side->fighter]);
    ImGui::ProgressBar((f32)side->stamina[side->fighter] / 100.0f, ImVec2(200, 0), overlay);
    snprintf(overlay, sizeof(overlay), "%s %u", text_get(ui->text, "duel.spirit"), side->spirit);
    ImGui::ProgressBar((f32)side->spirit / 300.0f, ImVec2(200, 0), overlay);
    char policy[64];
    text_label(ui->text, "duel_policy", side->policy, policy, sizeof(policy));
    ImGui::Text("%s", policy);
    ImGui::PopID();
    ImGui::EndGroup();
}

internal void
ui_draw_duel(ui_state *ui, engine_api *api)
{
    char title[128], label[96];
    ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "duel.title", "Duel"), &ui->show_duel)) { ImGui::End(); return; }
    void *e = api->engine;
    engine_duel_view view = api->duel_view(e);
    ImGui::Text("%s %02u", text_get(ui->text, "duel.round"), view.round);
    ui_draw_duel_side(ui, api, view.side + 0, "left");
    ImGui::SameLine(260);
    ui_draw_duel_side(ui, api, view.side + 1, "right");
    ImGui::Separator();
    ImGui::BeginDisabled(!view.active);
    for (u32 policy = 0; policy < DUEL_POLICY_COUNT; ++policy)
    {
        char text_id[32], stable[16];
        snprintf(text_id, sizeof(text_id), "duel.policy_%u", policy);
        snprintf(stable, sizeof(stable), "policy%u", policy);
        if (ImGui::RadioButton(ui_label(label, sizeof(label), ui, text_id, stable), view.side[0].policy == policy))
        {
            api->command_duel_action(e, SIM_DUEL_POLICY, (u8)policy);
        }
        if (policy + 1 < DUEL_POLICY_COUNT) ImGui::SameLine();
    }
    for (u32 special = 0; special < DUEL_SPECIAL_COUNT; ++special)
    {
        char text_id[32], stable[16];
        snprintf(text_id, sizeof(text_id), "duel.special_%u", special);
        snprintf(stable, sizeof(stable), "special%u", special);
        ImGui::BeginDisabled(!view.ready[special]);
        if (ImGui::Button(ui_label(label, sizeof(label), ui, text_id, stable))) api->command_duel_action(e, SIM_DUEL_SPECIAL, (u8)special);
        ImGui::EndDisabled();
        if (special % 4 != 3) ImGui::SameLine();
    }
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
    {
        u16 partner = view.side[0].officers[place];
        if (partner == ENGINE_NO_OFFICER || place == view.side[0].fighter) continue;
        char name[UI_NAME_SIZE * 2], button[128];
        ui_officer_text(ui, api, partner, name, sizeof(name));
        snprintf(button, sizeof(button), "%s: %s###sub%u", text_get(ui->text, "duel.substitute"), name, place);
        ImGui::BeginDisabled(view.side[0].stamina[place] == 0);
        if (ImGui::Button(button)) api->command_duel_action(e, SIM_DUEL_SUBSTITUTE, (u8)place);
        ImGui::EndDisabled();
        ImGui::SameLine();
    }
    ImGui::NewLine();
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "duel.next_round", "duel_round"))) api->command_duel_action(e, SIM_DUEL_ROUND, 0);
    ImGui::SameLine();
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "duel.auto", "duel_auto"))) api->command_duel_action(e, SIM_DUEL_AUTO, 0);
    ImGui::EndDisabled();
    if (!view.active && view.result != DUEL_RESULT_NONE)
    {
        char result[64], name[UI_NAME_SIZE * 2] = "-";
        text_label(ui->text, "duel_result", view.result, result, sizeof(result));
        if (view.winner < 2) ui_officer_text(ui, api, view.side[view.winner].officers[view.side[view.winner].fighter], name, sizeof(name));
        ImGui::Text("%s: %s", result, name);
        if (ImGui::Button(ui_label(label, sizeof(label), ui, "duel.close", "duel_close"))) ui->show_duel = false;
    }
    ImGui::End();
}

/* ---- the debate screen ------------------------------------------------------------------ */

internal void
ui_card_text(ui_state *ui, sim_card card, char *out, u32 out_size)
{
    char type[48];
    text_label(ui->text, "card", card.type, type, sizeof(type));
    if (card.type != CARD_TOPIC) { snprintf(out, out_size, "%s", type); return; }
    char topic[48], strength[16];
    text_label(ui->text, "topic", card.topic, topic, sizeof(topic));
    text_label(ui->text, "strength", card.strength, strength, sizeof(strength));
    snprintf(out, out_size, "%s %s", topic, strength);
}

internal void
ui_draw_debate_side(ui_state *ui, engine_api *api, engine_debate_side *side, const char *stable)
{
    char name[UI_NAME_SIZE * 2], overlay[64];
    ImGui::BeginGroup();
    ImGui::PushID(stable);
    ui_officer_text(ui, api, side->officer, name, sizeof(name));
    engine_officer officer = api->officer_by_id(api->engine, side->officer);
    ImGui::Text("%s  %s %u", name, text_get(ui->text, "debate.intellect"), officer.intelligence);
    snprintf(overlay, sizeof(overlay), "%s %d (%u)", text_get(ui->text, "debate.mind"), side->mind, side->rocks);
    ImGui::ProgressBar(side->mind > 0 ? (f32)side->mind / 100.0f : 0.0f, ImVec2(200, 0), overlay);
    snprintf(overlay, sizeof(overlay), "%s %u", text_get(ui->text, "debate.anger"), side->anger);
    ImGui::ProgressBar((f32)Minimum(side->anger, 100u) / 100.0f, ImVec2(200, 0), overlay);
    if (side->rage_rounds) ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.2f, 1.0f), "%s %u", text_get(ui->text, "debate.rage"), side->rage_rounds);
    ImGui::PopID();
    ImGui::EndGroup();
}

internal void
ui_draw_debate(ui_state *ui, engine_api *api)
{
    char title[128], label[96];
    ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "debate.title", "Debate"), &ui->show_debate)) { ImGui::End(); return; }
    void *e = api->engine;
    engine_debate_view view = api->debate_view(e);
    char topic[48];
    text_label(ui->text, "topic", view.topic, topic, sizeof(topic));
    ImGui::Text("%s: %s   %s %u", text_get(ui->text, "debate.topic"), topic, text_get(ui->text, "duel.round"), view.round);
    ui_draw_debate_side(ui, api, view.side + 0, "left");
    ImGui::SameLine(260);
    ui_draw_debate_side(ui, api, view.side + 1, "right");
    for (u32 index = 0; index < 2; ++index)
    {
        if (view.last[index].type >= CARD_TYPE_COUNT) continue;
        char card[96];
        ui_card_text(ui, view.last[index], card, sizeof(card));
        ImGui::Text("%s: %s", text_get(ui->text, index == 0 ? "debate.last_left" : "debate.last_right"), card);
    }
    ImGui::Separator();
    b32 playing = view.active && !view.finished;
    ImGui::BeginDisabled(!playing || view.turn != 0);
    for (u32 place = 0; place < view.side[0].hand_count; ++place)
    {
        char card[96], button[128];
        ui_card_text(ui, view.side[0].hand[place], card, sizeof(card));
        snprintf(button, sizeof(button), "%s###card%u", card, place);
        if (ImGui::Button(button)) api->command_debate_action(e, SIM_DEBATE_PLAY, (u8)place);
        ImGui::SameLine();
    }
    ImGui::NewLine();
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!playing);
    if (ImGui::Button(ui_label(label, sizeof(label), ui, view.turn == 0 ? "debate.speak" : "debate.listen", "debate_round")))
    {
        api->command_debate_action(e, SIM_DEBATE_ROUND, 0);
    }
    ImGui::SameLine();
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "duel.auto", "debate_auto"))) api->command_debate_action(e, SIM_DEBATE_AUTO, 0);
    ImGui::EndDisabled();
    if (view.active && view.finished)
    {
        ImGui::TextUnformatted(text_get(ui->text, view.winner == 0 ? "debate.won" : "debate.lost"));
        if (view.winner == 0)
        {
            if (ImGui::Button(ui_label(label, sizeof(label), ui, "debate.sympathy", "sympathy"))) api->command_debate_action(e, SIM_DEBATE_FINISH, 0);
            ImGui::SameLine();
            if (ImGui::Button(ui_label(label, sizeof(label), ui, "debate.press", "press"))) api->command_debate_action(e, SIM_DEBATE_FINISH, 1);
        }
        else if (ImGui::Button(ui_label(label, sizeof(label), ui, "debate.close", "debate_close")))
        {
            api->command_debate_action(e, SIM_DEBATE_FINISH, 0);
        }
    }
    if (!view.active && ImGui::Button(ui_label(label, sizeof(label), ui, "duel.close", "debate_done"))) ui->show_debate = false;
    ImGui::End();
}

/* The duel and debate screens open when one runs, and stay open to show
   the result until the player closes them. */
internal void
ui_draw_mini_games(ui_state *ui, engine_api *api)
{
    if (api->duel_view(api->engine).active)     ui->show_duel = true;
    if (api->debate_view(api->engine).active)   ui->show_debate = true;
    if (ui->show_duel)   ui_draw_duel(ui, api);
    if (ui->show_debate) ui_draw_debate(ui, api);
}
