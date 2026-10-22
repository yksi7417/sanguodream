/* The dialogs of group 3 of the second clone gap report: 移動 and 召喚
   (G2-036, G2-037), 官職 and 流放 of the 君主 menu (G2-060), and the plots
   驅虎吞狼 and 流言 with a target base list (G2-065). Each 決定 sends one
   command through engine_api, except 官職, which sends one command_court
   for each staged assignment. Included by ui.cpp after ui_court.cpp. */

/* A click on a row of officers with check marks: toggles the officer. */
internal void
ui_people_toggle(u16 *chosen, u32 *count, u32 capacity, u16 officer_id)
{
    for (u32 index = 0; index < *count; ++index)
    {
        if (chosen[index] != officer_id) continue;
        for (u32 next = index + 1; next < *count; ++next) chosen[next - 1] = chosen[next];
        (*count)--;
        return;
    }
    if (*count < capacity) chosen[(*count)++] = officer_id;
}

internal b32
ui_people_chosen(const u16 *chosen, u32 count, u16 officer_id)
{
    for (u32 index = 0; index < count; ++index) if (chosen[index] == officer_id) return true;
    return false;
}

/* One row of an officer with a check mark and the five abilities. */
internal void
ui_people_officer_row(ui_state *ui, engine_api *api, u16 officer_id, u16 *chosen, u32 *count, u32 capacity, const char *extra)
{
    engine_officer officer = api->officer_by_id(api->engine, officer_id);
    if (officer.officer_id == ENGINE_NO_OFFICER) return;
    char name[UI_NAME_SIZE * 2], row[160];
    ui_officer_text(ui, api, officer_id, name, sizeof(name));
    b32 on = ui_people_chosen(chosen, *count, officer_id);
    snprintf(row, sizeof(row), "%s %s  %s  %u %u %u %u %u###o%u", on ? "[x]" : "[ ]", name, extra ? extra : "", officer.leadership, officer.war,
             officer.intelligence, officer.politics, officer.charm, officer_id);
    if (ImGui::Selectable(row, on != 0)) ui_people_toggle(chosen, count, capacity, officer_id);
}

internal void
ui_people_fill_order(engine_move_order *order, u8 city, u8 target, const u16 *chosen, u32 count)
{
    *order = {};
    order->city_id     = city;
    order->target_city = target;
    for (u32 place = 0; place < SIM_MOVE_OFFICERS; ++place) order->officers[place] = place < count ? chosen[place] : (u16)ENGINE_NO_OFFICER;
}

/* The lines 行動力 and 抵達日數 of a dialog, from can_issue. */
internal b32
ui_people_cost_lines(ui_state *ui, engine_api *api, engine_command *command, u8 force, b32 days)
{
    engine_check check = api->can_issue(api->engine, command);
    b32 runs = check.refusal == SIM_REFUSAL_NONE;
    ImGui::Separator();
    if (days) ImGui::Text("%s %u%s", text_get(ui->text, "move.days"), runs ? check.days : 0, text_get(ui->text, "unit_target.day_unit"));
    ImGui::Text("%s: %u / %u", text_get(ui->text, "command.action_points"), runs ? check.action_points : 0,
                force != ENGINE_NO_FACTION ? api->actions_left(api->engine, force) : 0);
    if (!runs) ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "%s: %s", text_get(ui->text, "develop.refused"), ui_refusal_text(ui, check.refusal));
    return runs;
}

/* 移動 (G2-036): the target city with its days, the free officers of the
   city with check marks, and 行動力 20. */
internal void
ui_draw_move_officers(ui_state *ui, engine_api *api)
{
    char title[128], label[96], city_text[UI_NAME_SIZE * 2];
    ImGui::SetNextWindowSize(ImVec2(520, 440), ImGuiCond_FirstUseEver);
    ui_context_place(ui, 1);
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "move.title", "MoveOfficers"), &ui->show_move_officers)) { ImGui::End(); return; }
    ui_context_track(ui);
    void *e = api->engine;
    u8 city = ui->move.city;
    engine_city here = api->city_at(e, city);
    ui_city_label(ui, api, city, city_text, sizeof(city_text));
    ImGui::Text("%s: %s", text_get(ui->text, "city.select"), city_text);
    char preview[96] = "-";
    if (ui->move.target != ENGINE_NO_CITY) ui_city_label(ui, api, ui->move.target, preview, sizeof(preview));
    if (ImGui::BeginCombo(ui_label(label, sizeof(label), ui, "move.target", "move_target"), preview))
    {
        for (u32 index = 0; index < api->city_count(e); ++index)
        {
            engine_city other = api->city_at(e, index);
            if (other.owner_force_id != here.owner_force_id || other.city_id == city) continue;
            char name[96], row[160];
            ui_city_label(ui, api, other.city_id, name, sizeof(name));
            snprintf(row, sizeof(row), "%s  %u%s###t%u", name, api->travel_days(e, city, other.city_id), text_get(ui->text, "unit_target.day_unit"), other.city_id);
            if (ImGui::Selectable(row, ui->move.target == other.city_id)) ui->move.target = other.city_id;
        }
        ImGui::EndCombo();
    }
    ImGui::Text("%s (%u)", text_get(ui->text, "move.executors"), ui->move.count);
    u16 ids[32];
    u32 free_count = ui_free_officers(api, city, ids, ArrayCount(ids));
    ImGui::BeginChild("move_rows", ImVec2(0, 220), ImGuiChildFlags_Borders);
    for (u32 index = 0; index < free_count; ++index) ui_people_officer_row(ui, api, ids[index], ui->move.officers, &ui->move.count, SIM_MOVE_OFFICERS, 0);
    ImGui::EndChild();
    engine_command command = {};
    command.kind = ENGINE_COMMAND_MOVE_OFFICERS;
    ui_people_fill_order(&command.move, city, ui->move.target, ui->move.officers, ui->move.count);
    b32 runs = ui_people_cost_lines(ui, api, &command, here.owner_force_id, true);
    if (ui_draw_decide_back(ui, runs, &ui->show_move_officers))
    {
        engine_command_result result = api->command_move_officers(e, &command.move);
        ui_growth_result(ui, "move.title", result);
        if (result.refusal == SIM_REFUSAL_NONE) ui->show_move_officers = false;
    }
    ImGui::End();
}

/* 召喚 (G2-037): the officers of the other cities of the faction with
   their base and days, with check marks, and 行動力 20. */
internal void
ui_draw_summon(ui_state *ui, engine_api *api)
{
    char title[128], city_text[UI_NAME_SIZE * 2];
    ImGui::SetNextWindowSize(ImVec2(560, 440), ImGuiCond_FirstUseEver);
    ui_context_place(ui, 1);
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "summon.title", "Summon"), &ui->show_summon)) { ImGui::End(); return; }
    ui_context_track(ui);
    void *e = api->engine;
    u8 city = ui->summon.city;
    engine_city here = api->city_at(e, city);
    ui_city_label(ui, api, city, city_text, sizeof(city_text));
    ImGui::Text("%s: %s", text_get(ui->text, "city.select"), city_text);
    ImGui::Text("%s (%u)", text_get(ui->text, "move.executors"), ui->summon.count);
    ImGui::BeginChild("summon_rows", ImVec2(0, 260), ImGuiChildFlags_Borders);
    for (u32 index = 0; index < api->officer_count(e); ++index)
    {
        engine_officer officer = api->officer_at(e, index);
        if (officer.status > 3 || officer.status == 0 || officer.force_id != here.owner_force_id || officer.current_position >= SAVE_CITY_COUNT ||
            officer.current_position != officer.base_location || officer.current_position == city || officer.travel_days ||
            officer.job_city != ENGINE_NO_CITY) continue;
        char base[UI_NAME_SIZE * 2], extra[128];
        ui_city_label(ui, api, (u8)officer.current_position, base, sizeof(base));
        snprintf(extra, sizeof(extra), "%s %u%s", base, api->travel_days(e, (u8)officer.current_position, city), text_get(ui->text, "unit_target.day_unit"));
        ui_people_officer_row(ui, api, officer.officer_id, ui->summon.officers, &ui->summon.count, SIM_MOVE_OFFICERS, extra);
    }
    ImGui::EndChild();
    engine_command command = {};
    command.kind = ENGINE_COMMAND_SUMMON;
    ui_people_fill_order(&command.move, city, city, ui->summon.officers, ui->summon.count);
    b32 runs = ui_people_cost_lines(ui, api, &command, here.owner_force_id, true);
    if (ui_draw_decide_back(ui, runs, &ui->show_summon))
    {
        engine_command_result result = api->command_summon(e, &command.move);
        ui_growth_result(ui, "summon.title", result);
        if (result.refusal == SIM_REFUSAL_NONE) ui->show_summon = false;
    }
    ImGui::End();
}

/* The holder of a post after the staged changes: the staged officer, or
   the holder of the game. */
internal u16
ui_posts_holder(ui_state *ui, engine_api *api, u8 force, u8 post)
{
    for (u32 index = ui->posts.staged_count; index > 0; --index)
    {
        if (ui->posts.staged_post[index - 1] == post) return ui->posts.staged_officer[index - 1];
    }
    for (u32 index = 0; index < ui->posts.staged_count; ++index)
    {
        if (ui->posts.staged_post[index] == ENGINE_NO_ROW && ui->posts.staged_officer[index] == api->post_holder(api->engine, force, post)) return ENGINE_NO_OFFICER;
    }
    return api->post_holder(api->engine, force, post);
}

internal void
ui_posts_stage(ui_state *ui, u16 officer, u8 post)
{
    if (ui->posts.staged_count >= ArrayCount(ui->posts.staged_officer)) return;
    ui->posts.staged_officer[ui->posts.staged_count] = officer;
    ui->posts.staged_post[ui->posts.staged_count]    = post;
    ui->posts.staged_count++;
}

/* 官職 (G2-060): the table of the posts with the holder, the officers
   without a post, 任命, 免職, 全解除, 回復原狀 and 決定. The original drags
   an officer onto a row; the clone selects a row and an officer, then
   clicks 任命. 決定 sends one command_court for each staged change. */
internal void
ui_draw_posts(ui_state *ui, engine_api *api)
{
    char title[128], label[96];
    ImGui::SetNextWindowSize(ImVec2(760, 520), ImGuiCond_FirstUseEver);
    ui_context_place(ui, 1);
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "posts.title", "Posts"), &ui->show_posts)) { ImGui::End(); return; }
    ui_context_track(ui);
    void *e = api->engine;
    u8 force = api->whose_turn(e);
    if (force == ENGINE_NO_FACTION) { ImGui::TextUnformatted(text_get(ui->text, "court.no_faction")); ImGui::End(); return; }
    u8 rank = api->faction_rank(e, force);
    ImGui::BeginChild("post_rows", ImVec2(430, 360), ImGuiChildFlags_Borders);
    if (ImGui::BeginTable("posts", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
    {
        ImGui::TableSetupColumn(text_get(ui->text, "posts.post"));
        ImGui::TableSetupColumn(text_get(ui->text, "posts.merit"));
        ImGui::TableSetupColumn(text_get(ui->text, "posts.soldiers"));
        ImGui::TableSetupColumn(text_get(ui->text, "posts.effect"));
        ImGui::TableSetupColumn(text_get(ui->text, "posts.holder"));
        ImGui::TableHeadersRow();
        for (u32 index = 0; index < api->post_kind_count(e); ++index)
        {
            engine_post_kind post = api->post_kind_at(e, index);
            if (post.id == ENGINE_NO_ROW || post.rank > rank) continue;
            char row[128], holder[UI_NAME_SIZE * 2] = "-", effect[48] = "-";
            u16 holder_id = ui_posts_holder(ui, api, force, post.id);
            if (holder_id != ENGINE_NO_OFFICER) ui_officer_text(ui, api, holder_id, holder, sizeof(holder));
            if (post.ability < CONTENT_ABILITY_COUNT)
            {
                char ability[32];
                text_label(ui->text, "ability", post.ability, ability, sizeof(ability));
                snprintf(effect, sizeof(effect), "%s+%u", ability, post.bonus);
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            snprintf(row, sizeof(row), "%s###post%u", ui_text_with_key(ui, "post", post.key), post.id);
            if (ImGui::Selectable(row, ui->posts.post == post.id, ImGuiSelectableFlags_SpanAllColumns)) ui->posts.post = post.id;
            ImGui::TableNextColumn(); ImGui::Text("%u", post.merit);
            ImGui::TableNextColumn(); ImGui::Text("%u", post.soldiers);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(effect);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(holder);
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("free_rows", ImVec2(0, 360), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted(text_get(ui->text, "posts.free_officers"));
    for (u32 index = 0; index < api->officer_count(e); ++index)
    {
        engine_officer officer = api->officer_at(e, index);
        if (officer.force_id != force || officer.status == 0 || officer.status > 3) continue;
        b32 holds = false;
        for (u32 post = 0; post < api->post_kind_count(e) && !holds; ++post) holds = ui_posts_holder(ui, api, force, (u8)post) == officer.officer_id;
        if (holds) continue;
        char name[UI_NAME_SIZE * 2], row[128];
        ui_officer_text(ui, api, officer.officer_id, name, sizeof(name));
        snprintf(row, sizeof(row), "%s  %u###free%u", name, officer.merit, officer.officer_id);
        if (ImGui::Selectable(row, ui->posts.officer == officer.officer_id)) ui->posts.officer = officer.officer_id;
    }
    ImGui::EndChild();
    b32 can_appoint = ui->posts.post != ENGINE_NO_ROW && ui->posts.officer != ENGINE_NO_OFFICER;
    ImGui::BeginDisabled(!can_appoint);
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "posts.appoint", "post_appoint")))
    {
        ui_posts_stage(ui, ui->posts.officer, ui->posts.post);
        ui->posts.officer = ENGINE_NO_OFFICER;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    u16 current = ui->posts.post != ENGINE_NO_ROW ? ui_posts_holder(ui, api, force, ui->posts.post) : (u16)ENGINE_NO_OFFICER;
    ImGui::BeginDisabled(current == ENGINE_NO_OFFICER);
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "posts.dismiss", "post_dismiss"))) ui_posts_stage(ui, current, ENGINE_NO_ROW);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "posts.clear_all", "post_clear_all")))
    {
        for (u32 post = 0; post < api->post_kind_count(e); ++post)
        {
            u16 holder = ui_posts_holder(ui, api, force, (u8)post);
            if (holder != ENGINE_NO_OFFICER) ui_posts_stage(ui, holder, ENGINE_NO_ROW);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "posts.reset", "post_reset"))) ui->posts.staged_count = 0;
    if (ui_draw_decide_back(ui, ui->posts.staged_count > 0, &ui->show_posts))
    {
        u8 city = ui->city.selected;
        for (u32 index = 0; index < ui->posts.staged_count; ++index)
        {
            engine_court_order order = {};
            order.city_id = city;
            order.officer = ENGINE_NO_OFFICER;
            order.officers[0] = order.officers[1] = order.officers[2] = ENGINE_NO_OFFICER;
            order.target  = ui->posts.staged_officer[index];
            order.post    = ui->posts.staged_post[index];
            order.item    = 0xFF;
            engine_command_result result = api->command_court(e, ENGINE_COMMAND_APPOINT, &order);
            ui_growth_result(ui, "posts.title", result);
        }
        ui->posts.staged_count = 0;
        ui->show_posts = false;
    }
    if (ui->command_message[0]) ImGui::TextWrapped("%s", ui->command_message);
    ImGui::End();
}

/* 流放 (G2-060): an own officer leaves the faction. */
internal void
ui_draw_exile(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(420, 420), ImGuiCond_FirstUseEver);
    ui_context_place(ui, 1);
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "exile.title", "Exile"), &ui->show_exile)) { ImGui::End(); return; }
    ui_context_track(ui);
    void *e = api->engine;
    u8 force = api->whose_turn(e);
    ImGui::TextUnformatted(text_get(ui->text, "exile.officer"));
    ImGui::BeginChild("exile_rows", ImVec2(0, 280), ImGuiChildFlags_Borders);
    for (u32 index = 0; index < api->officer_count(e); ++index)
    {
        engine_officer officer = api->officer_at(e, index);
        if (officer.force_id != force || officer.status == 0 || officer.status > 3) continue;
        char name[UI_NAME_SIZE * 2], row[128];
        ui_officer_text(ui, api, officer.officer_id, name, sizeof(name));
        snprintf(row, sizeof(row), "%s  %u###ex%u", name, api->officer_detail(e, officer.officer_id).loyalty, officer.officer_id);
        if (ImGui::Selectable(row, ui->exile.officer == officer.officer_id)) ui->exile.officer = officer.officer_id;
    }
    ImGui::EndChild();
    engine_command command = {};
    command.kind = ENGINE_COMMAND_EXILE;
    command.court.city_id = ui->city.selected;
    command.court.officer = ENGINE_NO_OFFICER;
    command.court.officers[0] = command.court.officers[1] = command.court.officers[2] = ENGINE_NO_OFFICER;
    command.court.target  = ui->exile.officer;
    command.court.item    = 0xFF;
    command.court.post    = 0xFF;
    b32 runs = ui->exile.officer != ENGINE_NO_OFFICER && api->can_issue(e, &command).refusal == SIM_REFUSAL_NONE;
    if (ui_draw_decide_back(ui, runs, &ui->show_exile))
    {
        engine_command_result result = api->command_court(e, ENGINE_COMMAND_EXILE, &command.court);
        ui_growth_result(ui, "exile.title", result);
        if (result.refusal == SIM_REFUSAL_NONE) ui->show_exile = false;
    }
    ImGui::End();
}

/* 驅虎吞狼 and 流言 (G2-065): the bases of the other factions with 太守,
   現役, 治安 and 日數, one executor from the free officers of the city in
   the order of 智力, the gold, the days and 行動力. */
internal void
ui_draw_plot_dialog(ui_state *ui, engine_api *api)
{
    static const char *plots[SIM_FACTION_PLOT_COUNT] = { "two_tigers", "drive_tiger", "rumour" };
    char title[128], text_id[48];
    snprintf(text_id, sizeof(text_id), "plot_cmd.%s", plots[ui->plot.kind < SIM_FACTION_PLOT_COUNT ? ui->plot.kind : 2]);
    ImGui::SetNextWindowSize(ImVec2(620, 520), ImGuiCond_FirstUseEver);
    ui_context_place(ui, 1);
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, text_id, "PlotDialog"), &ui->show_plot)) { ImGui::End(); return; }
    ui_context_track(ui);
    void *e = api->engine;
    u8 city = ui->plot.city;
    engine_city here = api->city_at(e, city);
    ImGui::TextUnformatted(text_get(ui->text, "plot.target"));
    ImGui::BeginChild("plot_targets", ImVec2(0, 200), ImGuiChildFlags_Borders);
    if (ImGui::BeginTable("targets", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
    {
        ImGui::TableSetupColumn(text_get(ui->text, "hexinfo.city"));
        ImGui::TableSetupColumn(text_get(ui->text, "plot.governor"));
        ImGui::TableSetupColumn(text_get(ui->text, "plot.active"));
        ImGui::TableSetupColumn(text_get(ui->text, "plot.order"));
        ImGui::TableSetupColumn(text_get(ui->text, "plot.days"));
        ImGui::TableHeadersRow();
        for (u32 index = 0; index < api->city_count(e); ++index)
        {
            engine_city other = api->city_at(e, index);
            if (other.owner_force_id == here.owner_force_id || other.owner_force_id == ENGINE_NO_FACTION) continue;
            char name[96], governor[UI_NAME_SIZE * 2] = "-", row[160];
            ui_city_label(ui, api, other.city_id, name, sizeof(name));
            if (other.commanding_officer_id != 0xFFFF) ui_officer_text(ui, api, other.commanding_officer_id, governor, sizeof(governor));
            u32 active = 0;
            for (u32 o = 0; o < api->officer_count(e); ++o)
            {
                engine_officer officer = api->officer_at(e, o);
                if (officer.status <= 3 && officer.current_position == other.city_id) active++;
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            snprintf(row, sizeof(row), "%s###pc%u", name, other.city_id);
            if (ImGui::Selectable(row, ui->plot.target_city == other.city_id, ImGuiSelectableFlags_SpanAllColumns)) ui->plot.target_city = other.city_id;
            ImGui::TableNextColumn(); ImGui::TextUnformatted(governor);
            ImGui::TableNextColumn(); ImGui::Text("%u", active);
            ImGui::TableNextColumn(); ImGui::Text("%u", other.public_order);
            ImGui::TableNextColumn(); ImGui::Text("%u", api->travel_days(e, city, other.city_id));
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
    ImGui::TextUnformatted(text_get(ui->text, "plot.executor"));
    u16 ids[32];
    u32 free_count = ui_free_officers(api, city, ids, ArrayCount(ids));
    /* The list of the original sorts by 智力 (G2-065). */
    for (u32 a = 0; a < free_count; ++a)
    {
        for (u32 b = a + 1; b < free_count; ++b)
        {
            if (api->officer_by_id(e, ids[b]).intelligence > api->officer_by_id(e, ids[a]).intelligence) { u16 swap = ids[a]; ids[a] = ids[b]; ids[b] = swap; }
        }
    }
    ImGui::BeginChild("plot_officers", ImVec2(0, 120), ImGuiChildFlags_Borders);
    for (u32 index = 0; index < free_count; ++index)
    {
        engine_officer officer = api->officer_by_id(e, ids[index]);
        char name[UI_NAME_SIZE * 2], row[128];
        ui_officer_text(ui, api, ids[index], name, sizeof(name));
        snprintf(row, sizeof(row), "%s  %u###po%u", name, officer.intelligence, ids[index]);
        if (ImGui::Selectable(row, ui->plot.officer == ids[index])) ui->plot.officer = ids[index];
    }
    ImGui::EndChild();
    engine_command command = {};
    command.kind = ENGINE_COMMAND_FACTION_PLOT;
    command.court.city_id     = city;
    command.court.officer     = ui->plot.officer;
    command.court.officers[0] = command.court.officers[1] = command.court.officers[2] = ENGINE_NO_OFFICER;
    command.court.target      = ENGINE_NO_OFFICER;
    command.court.kind        = ui->plot.kind;
    command.court.target_city = ui->plot.target_city;
    command.court.force       = ui->plot.target_city != ENGINE_NO_CITY ? api->city_at(e, ui->plot.target_city).owner_force_id : (u8)ENGINE_NO_FACTION;
    command.court.item        = 0xFF;
    command.court.post        = 0xFF;
    engine_check check = api->can_issue(e, &command);
    b32 runs = ui->plot.target_city != ENGINE_NO_CITY && ui->plot.officer != ENGINE_NO_OFFICER && check.refusal == SIM_REFUSAL_NONE;
    ImGui::Separator();
    ImGui::Text("%s: %u / %u", text_get(ui->text, "command.gold"), runs ? check.gold : 0, here.gold);
    if (ui->plot.target_city != ENGINE_NO_CITY)
    {
        ImGui::Text("%s %u%s", text_get(ui->text, "move.days"), api->travel_days(e, city, ui->plot.target_city), text_get(ui->text, "unit_target.day_unit"));
    }
    ImGui::Text("%s: %u / %u", text_get(ui->text, "command.action_points"), runs ? check.action_points : 0,
                here.owner_force_id != ENGINE_NO_FACTION ? api->actions_left(e, here.owner_force_id) : 0);
    if (!runs && check.refusal != SIM_REFUSAL_NONE) ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "%s: %s", text_get(ui->text, "develop.refused"), ui_refusal_text(ui, check.refusal));
    if (ui_draw_decide_back(ui, runs, &ui->show_plot))
    {
        engine_command_result result = api->command_court(e, ENGINE_COMMAND_FACTION_PLOT, &command.court);
        ui_growth_result(ui, text_id, result);
        if (result.refusal == SIM_REFUSAL_NONE) ui->show_plot = false;
    }
    ImGui::End();
}

internal void
ui_draw_people_dialogs(ui_state *ui, engine_api *api)
{
    if (ui->show_move_officers) ui_draw_move_officers(ui, api);
    if (ui->show_summon)        ui_draw_summon(ui, api);
    if (ui->show_posts)         ui_draw_posts(ui, api);
    if (ui->show_exile)         ui_draw_exile(ui, api);
    if (ui->show_plot)          ui_draw_plot_dialog(ui, api);
}
