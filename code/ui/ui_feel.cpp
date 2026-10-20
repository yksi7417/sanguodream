/* M17: the feel of the first hour (ui/title-and-load.md, ui/screen-flow.md,
   ui/main-map.md "Message window"). Four parts:
   - the rider of the title menu: after a click the other buttons hide, and
     the screen opens after UI_TITLE_SELECT_SECONDS;
   - the load transition: a dark screen with no controls for
     UI_LOAD_TRANSITION_SECONDS after a load;
   - the turn of the other factions: after 進行 the UI ends their turns
     until a player acts again;
   - the message window: at the start of a turn of a player, the period
     heading and the reports of the turn show one at a time. A left click
     advances.
   The sim stays instant. Only the front end waits. With original_timing
   off, the title and the load act at once, as in M9 to M16. */

internal void ui_event_subject_text(ui_state *ui, engine_api *api, u32 entity, char *out, u32 out_size);

internal bool
ui_feel_messages_open(ui_state *ui)
{
    return ui->feel.message_shown < ui->feel.message_count;
}

internal bool
ui_feel_in_transition(ui_state *ui)
{
    return ui->feel.transition_seconds > 0.0f;
}

internal void
ui_feel_begin_transition(ui_state *ui)
{
    if (ui->feel.original_timing) ui->feel.transition_seconds = UI_LOAD_TRANSITION_SECONDS;
}

/* A new game or a load: the next turn start of a player shows its messages. */
internal void
ui_feel_reset_turn(ui_state *ui)
{
    ui->feel.seen_turn     = false;
    ui->feel.ledger_mark   = 0;
    ui->feel.message_count = 0;
    ui->feel.message_shown = 0;
}

/* The title button acts at once, or after the rider when the original
   timing is on. A click during the rider does nothing. */
internal void
ui_feel_choose_title(ui_state *ui, u32 choice)
{
    if (ui->feel.title_choice != UI_TITLE_NONE) return;
    ui->feel.title_choice  = choice;
    ui->feel.title_seconds = 0.0f;
}

/* The kinds of the ledger that keep the books of a turn. They give no message. */
internal bool
ui_feel_bookkeeping(sim_event_kind kind)
{
    switch (kind)
    {
        case SIM_EVENT_NONE:
        case SIM_EVENT_TURN_ADVANCED:
        case SIM_EVENT_SAVE_LOADED:
        case SIM_EVENT_NAME_INDEX_LOADED:
        case SIM_EVENT_FACTION_TURN_STARTED:
        case SIM_EVENT_FACTION_TURN_ENDED:
        case SIM_EVENT_AI_PASSED:
        case SIM_EVENT_ACTION_POINTS_RECOVERED:
        case SIM_EVENT_ACTION_POINTS_SPENT:
            return true;
        default:
            return false;
    }
}

/* The kinds of the ledger that report an attack on a base or a facility. */
internal bool
ui_feel_hostile(sim_event_kind kind)
{
    switch (kind)
    {
        case SIM_EVENT_BASE_DAMAGED:
        case SIM_EVENT_BASE_SOLDIERS_LOST:
        case SIM_EVENT_BASE_CAPTURED:
        case SIM_EVENT_FACILITY_DAMAGED:
        case SIM_EVENT_FACILITY_DESTROYED:
            return true;
        default:
            return false;
    }
}

/* A report for the player: an event of the faction, or an attack of
   another faction on a city of the player. The spec gives no list of the
   reports (an open point of M17), so the clean room uses this rule. */
internal bool
ui_feel_is_report(engine_api *api, sim_event *event, u8 force)
{
    if (ui_feel_bookkeeping(event->kind)) return false;
    if (event->faction == force) return true;
    if (!ui_feel_hostile(event->kind) || !event->subject) return false;
    engine_subject subject = api->subject_of(api->engine, event->subject);
    return subject.kind == ENGINE_SUBJECT_CITY && api->city_at(api->engine, (u8)subject.id).owner_force_id == force;
}

/* The messages of a turn start: the heading, then the reports since the
   last turn start in ledger order. UI_MESSAGE_CAPACITY keeps the first
   ones. The log keeps all events. */
internal void
ui_feel_collect_messages(ui_state *ui, engine_api *api, u8 force)
{
    u32 count = api->ledger_count(api->engine);
    if (ui->feel.ledger_mark > count) ui->feel.ledger_mark = 0;   /* a load replaced the ledger */
    ui->feel.message_count = 0;
    ui->feel.message_shown = 0;
    ui->feel.messages[ui->feel.message_count++] = UI_MESSAGE_HEADING;
    for (u32 index = ui->feel.ledger_mark; index < count && ui->feel.message_count < UI_MESSAGE_CAPACITY; ++index)
    {
        sim_event event = api->ledger_at(api->engine, index);
        if (ui_feel_is_report(api, &event, force)) ui->feel.messages[ui->feel.message_count++] = index;
    }
    ui->feel.ledger_mark = count;
}

/* Is the faction a player, and is any faction a player. */
internal bool
ui_feel_human(engine_api *api, u8 force, bool *any_human)
{
    bool human = false;
    *any_human = false;
    for (u32 index = 0; index < api->faction_count(api->engine); ++index)
    {
        engine_faction faction = api->faction_at(api->engine, index);
        *any_human = *any_human || faction.human;
        if (faction.force_id == force) human = faction.human != 0;
    }
    return human;
}

/* screen-flow.md: after 進行 the other factions act, then a new turn of a
   player starts with its messages. The UI ends the turns of the AI
   factions. A game without a player stays as it is. */
internal void
ui_feel_turn_start(ui_state *ui, engine_api *api)
{
    void *e = api->engine;
    bool any_human = false;
    u8 force = api->whose_turn(e);
    bool human = ui_feel_human(api, force, &any_human);
    if (!any_human) return;
    for (u32 step = 0; step < UI_FEEL_PASS_LIMIT && !human; ++step)
    {
        api->end_turn(e);
        force = api->whose_turn(e);
        human = ui_feel_human(api, force, &any_human);
    }
    if (!human) return;
    u32 turn = api->turn(e);
    if (ui->feel.seen_turn && ui->feel.turn == turn && ui->feel.force == force) return;
    ui->feel.seen_turn = true;
    ui->feel.turn      = turn;
    ui->feel.force     = force;
    ui_feel_collect_messages(ui, api, force);
}

internal void ui_title_open(ui_state *ui, engine_api *api, u32 choice);

/* Each frame, before the panels. */
internal void
ui_feel_update(ui_state *ui, engine_api *api, f32 dt_seconds, bool in_game)
{
    if (ui->feel.title_choice != UI_TITLE_NONE)
    {
        ui->feel.title_seconds += dt_seconds;
        if (ui->feel.title_seconds >= UI_TITLE_SELECT_SECONDS)
        {
            u32 choice = ui->feel.title_choice;
            ui->feel.title_choice = UI_TITLE_NONE;
            ui_title_open(ui, api, choice);
        }
    }
    if (ui_feel_in_transition(ui))
    {
        ui->feel.transition_seconds -= dt_seconds;
        if (ui->context.kind != UI_CONTEXT_NONE) ui_context_close(ui);   /* no controls */
        return;
    }
    if (!ui->hud_mode || !in_game || !ui->feel.turn_messages) return;
    ui_feel_turn_start(ui, api);
    /* screen-flow.md, rule 2: a left click anywhere advances a message. */
    if (ui_feel_messages_open(ui) && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().WantCaptureMouse)
        ui->feel.message_shown++;
}

/* The text of one message: the heading 《year month 上旬》 or one report. */
internal void
ui_feel_message_text(ui_state *ui, engine_api *api, u32 message, char *out, u32 out_size)
{
    if (message == UI_MESSAGE_HEADING)
    {
        engine_date date = api->date(api->engine);
        const char *part = date.day >= 21 ? "msg.late" : date.day >= 11 ? "msg.middle" : "msg.early";
        snprintf(out, out_size, text_get(ui->text, "msg.heading"), date.year, date.month, text_get(ui->text, part));
        return;
    }
    sim_event event = api->ledger_at(api->engine, message);
    char subject[UI_NAME_SIZE * 2];
    ui_event_subject_text(ui, api, event.subject, subject, sizeof(subject));
    snprintf(out, out_size, "%s  %s  %+d", sim_event_name(event.kind), subject, event.delta);
}

/* main-map.md, "Message window": 689 x 197 at the bottom centre, the
   portrait of the speaker at the left, the text at the right. The ruler
   speaks: the spec does not name the speaker of each report. The whole
   window is the button that advances. */
internal void
ui_draw_messages(ui_state *ui, engine_api *api)
{
    if (!ui_feel_messages_open(ui)) return;
    ImVec2 display = ImGui::GetIO().DisplaySize;
    ImVec2 size(UI_MESSAGE_WIDTH < display.x ? UI_MESSAGE_WIDTH : display.x, UI_MESSAGE_HEIGHT);
    ImGui::SetNextWindowPos(ImVec2((display.x - size.x) * 0.5f, display.y - size.y - UI_MESSAGE_MARGIN), ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoScrollWithMouse;
    if (ImGui::Begin("###Messages", 0, flags))
    {
        ImVec2 origin = ImGui::GetCursorScreenPos();
        ImVec2 area   = ImGui::GetContentRegionAvail();
        u16 ruler = api->force_ruler(api->engine, ui->feel.force);
        if (ruler != ENGINE_NO_OFFICER) ui_draw_officer_portrait(ui, api, ruler);
        char speaker[UI_NAME_SIZE * 2], text[UI_NAME_SIZE * 4];
        ui_force_text(ui, api, ui->feel.force, speaker, sizeof(speaker));
        ui_feel_message_text(ui, api, ui->feel.messages[ui->feel.message_shown], text, sizeof(text));
        ImGui::Separator();
        ImGui::TextWrapped("%s", text);
        ImGui::TextDisabled("%s   %u / %u", speaker, ui->feel.message_shown + 1, ui->feel.message_count);
        ImGui::SetCursorScreenPos(origin);
        ImGui::SetNextItemAllowOverlap();
        if (ImGui::InvisibleButton("###next", ImVec2(area.x > 1.0f ? area.x : 1.0f, area.y > 1.0f ? area.y : 1.0f)))
            ui->feel.message_shown++;
    }
    ImGui::End();
}

/* title-and-load.md: a dark landscape with no controls while a save loads. */
internal void
ui_draw_load_transition(ui_state *ui)
{
    if (!ui_feel_in_transition(ui)) return;
    ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::SetNextWindowFocus();
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.05f, 0.07f, 1.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("###LoadTransition", 0, flags);
    ImGui::End();
    ImGui::PopStyleColor();
}
