/* The option "Follow the action" of the banner of a shared game (docs/MULTIPLAYER.md, "The tools
   of the banner"). The camera goes to each action of another faction. The officer of the action
   then says what it does, the location and the order. The camera stays for the pause that the
   viewer sets. In the turn of the local seat, the camera stays with the player.
   The sim runs an AI turn in one step, so the page reads the actions from the ledger after the
   sim made them. Each page has its own pause, and no pause changes the game.
   Included by ui.cpp after ui_report.cpp. */

/* The kinds of the ledger that are an attack on a unit or a base. The ledger does not name the
   attacker of a plain attack, so the officer who takes the attack speaks. */
internal b32
ui_net_follow_is_attack(u32 kind)
{
    return kind == SIM_EVENT_UNIT_DAMAGED || kind == SIM_EVENT_BASE_DAMAGED || kind == SIM_EVENT_BASE_SOLDIERS_LOST ||
           kind == SIM_EVENT_FACILITY_DAMAGED || kind == SIM_EVENT_FACILITY_DESTROYED;
}

/* The kinds of the ledger that are an action of an officer. The events of the calendar, for
   example the income, also carry the command that ended the turn, so the list names each kind. */
internal b32
ui_net_follow_is_action(u32 kind)
{
    if (ui_net_follow_is_attack(kind)) return true;
    switch (kind)
    {
        case SIM_EVENT_UNIT_SORTIED:
        case SIM_EVENT_UNIT_ENTERED_CITY:
        case SIM_EVENT_UNIT_RETREATED:
        case SIM_EVENT_UNIT_SUPPLIED:
        case SIM_EVENT_UNIT_DESTROYED:
        case SIM_EVENT_TACTIC_USED:
        case SIM_EVENT_PLOT_USED:
        case SIM_EVENT_BASE_CAPTURED:
        case SIM_EVENT_BASE_TAKEN:
        case SIM_EVENT_BUILDING_STARTED:
        case SIM_EVENT_DEVELOP_STARTED:
        case SIM_EVENT_PATROLLED:
        case SIM_EVENT_CONSCRIPTED:
        case SIM_EVENT_TRAINED:
        case SIM_EVENT_GRAIN_BOUGHT:
        case SIM_EVENT_GRAIN_SOLD:
        case SIM_EVENT_PRODUCTION_STARTED:
        case SIM_EVENT_RESEARCH_STARTED:
        case SIM_EVENT_MERGE_STARTED:
        case SIM_EVENT_OFFICER_FOUND:
        case SIM_EVENT_OFFICER_RECRUITED:
        case SIM_EVENT_RECRUIT_FAILED:
        case SIM_EVENT_DUEL_STARTED:
        case SIM_EVENT_DEBATE_STARTED:
            return true;
        default:
            return false;
    }
}

/* True if the event is an action that the viewer can follow, and gives its hex. An AI move has
   no command, and each move gets a caption. Other actions get one caption for each command. */
internal b32
ui_net_follow_hex(ui_state *ui, engine_api *api, u32 index, hex_coord *out)
{
    sim_event event = api->ledger_at(api->engine, index);
    if (event.kind == SIM_EVENT_UNIT_MOVED)
    {
        engine_unit unit = api->unit_at(api->engine, event.detail);
        if (unit.id == ENGINE_NO_UNIT) return false;
        *out = hex_make(unit.q, unit.r);
        return true;
    }
    if (!ui_net_follow_is_action(event.kind)) return false;
    if (event.cause && event.cause == ui->net.follow_cause) return false;
    engine_event_report report = api->event_report(api->engine, index);
    if (!report.place.on_map) return false;
    *out = hex_make(report.place.q, report.place.r);
    ui->net.follow_cause = event.cause;
    return true;
}

/* The line of the location: the city on the hex, else the nearest city. */
internal void
ui_net_follow_where(ui_state *ui, engine_api *api, hex_coord hex)
{
    u8  nearest  = ENGINE_NO_CITY;
    s32 distance = 0;
    for (u32 id = 0; id < api->city_count(api->engine); ++id)
    {
        engine_city_slots site = api->city_slots(api->engine, (u8)id);
        if (site.city_id == ENGINE_NO_CITY) continue;
        s32 away = hex_distance(hex, hex_make(site.q, site.r));
        if (nearest != ENGINE_NO_CITY && away >= distance) continue;
        nearest  = site.city_id;
        distance = away;
    }
    char city[UI_NAME_SIZE * 2] = "";
    if (nearest != ENGINE_NO_CITY) ui_city_label(ui, api, nearest, city, sizeof(city));
    snprintf(ui->net.follow_where, sizeof(ui->net.follow_where), text_get(ui->text, distance == 0 ? "net.follow_at" : "net.follow_near"), city, hex.q, hex.r);
}

/* The long order of a unit: its target is a hex or the unit of an officer. Gives false if the unit has none. */
internal b32
ui_net_follow_long_order(ui_state *ui, engine_api *api, const engine_unit *unit, char *out, u32 out_size)
{
    if (unit->order == UNIT_ORDER_MOVE)
    {
        snprintf(out, out_size, text_get(ui->text, "net.order_march"), unit->order_q, unit->order_r);
        return true;
    }
    if (unit->order == UNIT_ORDER_ATTACK_FACILITY)
    {
        snprintf(out, out_size, text_get(ui->text, "net.order_facility"), unit->order_q, unit->order_r);
        return true;
    }
    if (unit->order != UNIT_ORDER_DESTROY && unit->order != UNIT_ORDER_DRIVE && unit->order != UNIT_ORDER_FOLLOW) return false;
    engine_unit target = api->unit_at(api->engine, unit->order_target);
    char name[UI_NAME_SIZE * 2] = "";
    if (target.id != ENGINE_NO_UNIT && target.officers[0] != ENGINE_NO_OFFICER) ui_officer_text(ui, api, target.officers[0], name, sizeof(name));
    const char *key = unit->order == UNIT_ORDER_DESTROY ? "net.order_destroy" : unit->order == UNIT_ORDER_DRIVE ? "net.order_drive" : "net.order_follow";
    snprintf(out, out_size, text_get(ui->text, key), name);
    return true;
}

/* The goal that the AI chose for the unit in its last turn. Gives false if the unit has none. */
internal b32
ui_net_follow_goal(ui_state *ui, engine_api *api, u32 unit_id, char *out, u32 out_size)
{
    engine_unit_goal goal = api->unit_goal(api->engine, unit_id);
    if (!goal.known) return false;
    if (goal.city_id == ENGINE_NO_CITY)
    {
        snprintf(out, out_size, text_get(ui->text, "net.order_march"), goal.q, goal.r);
        return true;
    }
    char city[UI_NAME_SIZE * 2] = "";
    ui_city_label(ui, api, goal.city_id, city, sizeof(city));
    const char *key = goal.kind == AI_GOAL_DEFEND         ? "net.order_defend"
                    : goal.kind == AI_GOAL_TAKE_FREE_CITY ? "net.order_free_city"
                    : goal.kind == AI_GOAL_RESUPPLY       ? "net.order_resupply"
                                                          : "net.order_attack";
    snprintf(out, out_size, text_get(ui->text, key), city);
    return true;
}

/* The line of the order: the long order of the unit, its transport, the goal of the AI, else the ruler. */
internal void
ui_net_follow_order(ui_state *ui, engine_api *api, const engine_unit *unit)
{
    char order[UI_NAME_SIZE * 4] = "";
    b32 found = false;
    if (unit)
    {
        found = ui_net_follow_long_order(ui, api, unit, order, sizeof(order));
        if (!found && unit->destination != ENGINE_NO_CITY)
        {
            char city[UI_NAME_SIZE * 2] = "";
            ui_city_label(ui, api, unit->destination, city, sizeof(city));
            snprintf(order, sizeof(order), text_get(ui->text, "net.order_carry"), city);
            found = true;
        }
        if (!found) found = ui_net_follow_goal(ui, api, unit->id, order, sizeof(order));
    }
    if (!found) snprintf(order, sizeof(order), "%s", text_get(ui->text, "net.order_ruler"));
    snprintf(ui->net.follow_order, sizeof(ui->net.follow_order), text_get(ui->text, "net.follow_order"), order);
}

/* The line of the action: the words of the officer for a march, else the line of the log. */
internal void
ui_net_follow_what(ui_state *ui, engine_api *api, u32 index, const engine_event_report *report)
{
    const char *key = 0;
    switch (report->kind)
    {
        case SIM_EVENT_UNIT_MOVED:        key = "net.say_moved";     break;
        case SIM_EVENT_UNIT_SORTIED:      key = "net.say_sortied";   break;
        case SIM_EVENT_UNIT_ENTERED_CITY: key = "net.say_entered";   break;
        case SIM_EVENT_UNIT_RETREATED:    key = "net.say_retreated"; break;
        default: break;
    }
    if (key)
    {
        snprintf(ui->net.follow_what, sizeof(ui->net.follow_what), "%s", text_get(ui->text, key));
        return;
    }
    char line[sizeof(ui->net.follow_what) / 2];
    ui_report_line(ui, api, index, ui_report_value(api, report), TEXT_LINE_LOG, line, sizeof(line));
    if (ui_net_follow_is_attack(report->kind)) snprintf(ui->net.follow_what, sizeof(ui->net.follow_what), text_get(ui->text, "net.say_attacked"), line);
    else                                       snprintf(ui->net.follow_what, sizeof(ui->net.follow_what), "%s", line);
}

/* The officer who speaks, and all the lines of the caption of one action. */
internal void
ui_net_follow_caption(ui_state *ui, engine_api *api, u32 index, hex_coord hex)
{
    sim_event event = api->ledger_at(api->engine, index);
    engine_event_report report = api->event_report(api->engine, index);
    engine_unit unit = {};
    unit.id = ENGINE_NO_UNIT;
    if (event.kind == SIM_EVENT_UNIT_MOVED)            unit = api->unit_at(api->engine, event.detail);
    else if (report.place.kind == ENGINE_PLACE_UNIT)   unit = api->unit_at(api->engine, report.place.id);
    b32 has_unit = unit.id != ENGINE_NO_UNIT;
    u16 speaker = report.speaker;
    if (report.place.officer != ENGINE_NO_OFFICER) speaker = report.place.officer;
    if (has_unit && unit.officers[0] != ENGINE_NO_OFFICER) speaker = unit.officers[0];
    u8 force = has_unit ? unit.force_id : report.faction;
    char name[UI_NAME_SIZE * 2] = "", faction[UI_NAME_SIZE * 2] = "";
    if (speaker != ENGINE_NO_OFFICER) ui_officer_text(ui, api, speaker, name, sizeof(name));
    if (force != ENGINE_NO_FACTION) ui_force_text(ui, api, force, faction, sizeof(faction));
    if (name[0] && faction[0]) snprintf(ui->net.follow_who, sizeof(ui->net.follow_who), "%s (%s)", name, faction);
    else                       snprintf(ui->net.follow_who, sizeof(ui->net.follow_who), "%s%s", name, faction);
    ui->net.follow_speaker = speaker;
    ui->net.follow_hex     = hex;
    ui->net.follow_shown   = true;
    ui_net_follow_what(ui, api, index, &report);
    ui_net_follow_where(ui, api, hex);
    ui_net_follow_order(ui, api, has_unit ? &unit : 0);
}

internal void
ui_net_follow_update(ui_state *ui, engine_api *api, f32 dt_seconds)
{
    u32 count = api->ledger_count(api->engine);
    engine_multiplayer_info info = api->multiplayer_info(api->engine);
    if (!ui->net.follow || !info.shared || ui->net.follow_next > count)
    {
        ui->net.follow_next  = count;   /* the option starts with the next action, and a ledger can get shorter */
        ui->net.follow_wait  = 0.0f;
        ui->net.follow_shown = false;
        return;
    }
    ui->net.follow_wait -= dt_seconds;
    if (ui->net.follow_wait > 0.0f) return;
    ui->net.follow_shown = false;
    if (info.local_force != ENGINE_NO_FACTION && info.acting_force == info.local_force)
    {
        ui->net.follow_next = count;
        return;
    }
    while (ui->net.follow_next < count)
    {
        u32 index = ui->net.follow_next++;
        hex_coord hex;
        if (!ui_net_follow_hex(ui, api, index, &hex)) continue;
        ui_frame_hex(ui, api, hex);
        ui_net_follow_caption(ui, api, index, hex);
        ui->net.follow_wait = ui->net.follow_seconds;
        return;
    }
}

/* The caption over the hex of the action. It takes no input, so the player can still use the map. */
internal void
ui_draw_net_follow_caption(ui_state *ui, engine_api *api)
{
    if (!ui->net.follow || !ui->net.follow_shown) return;
    ImVec2 display = ImGui::GetIO().DisplaySize;
    f32 x = display.x * 0.5f, y = display.y * 0.5f;
    if (ui->scene.enabled && ui->scene.initialized && display.x > 0.0f && display.y > 0.0f)
    {
        m4 view_projection = camera_view_projection(&ui->scene.cam, display.x, display.y);
        f32 screen_x = 0.0f, screen_y = 0.0f;
        if (ui_scene_screen(api, view_projection, ui->net.follow_hex, UI_LABEL_LIFT, &screen_x, &screen_y)) { x = screen_x; y = screen_y; }
    }
    f32 half = UI_NET_CAPTION_WIDTH * 0.5f;
    x = Maximum(half, Minimum(x, display.x - half));
    y = Maximum(UI_NET_CAPTION_TOP, Minimum(y, display.y));
    ImGui::SetNextWindowPos(ImVec2(x, y - 8.0f), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGui::SetNextWindowSize(ImVec2(UI_NET_CAPTION_WIDTH, 0.0f), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin("###NetFollowCaption", 0, flags))
    {
        if (ui->net.follow_speaker != ENGINE_NO_OFFICER) ui_draw_officer_portrait(ui, api, ui->net.follow_speaker);
        ImGui::TextColored(ImVec4(0.62f, 0.82f, 0.96f, 1.0f), "%s", ui->net.follow_who);
        ImGui::TextWrapped("%s", ui->net.follow_what);
        ImGui::TextWrapped("%s", ui->net.follow_where);
        ImGui::TextWrapped("%s", ui->net.follow_order);
    }
    ImGui::End();
}
