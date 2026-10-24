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
/* ui_report.cpp (M18): the words of an event and the value after it. */
internal void ui_report_line(ui_state *ui, engine_api *api, u32 ledger_index, s64 value, u32 form, char *out, u32 out_size);
internal s64  ui_report_value(engine_api *api, const engine_event_report *report);
/* ui_round.cpp (M20): 進行 of the map menu and of the top bar (ui_game.cpp). */
internal void ui_round_proceed(ui_state *ui, engine_api *api);
internal void ui_frame_city(ui_state *ui, u8 city_id);   /* ui_locate.cpp; G2-006: the camera to the capital */
internal void ui_frame_hex(ui_state *ui, engine_api *api, hex_coord hex);   /* ui_locate.cpp; G3-009: the camera to the enemy unit */

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

/* A report for the player (M18): a kind that event_tones.tsv marks as a
   report, of the faction, or an attack of another faction on a city of
   the player. The other events go only to the log. The spec gives no list
   of the reports (an open point of M17), so the pack holds the list. */
internal bool
ui_feel_is_report(engine_api *api, u32 ledger_index, sim_event *event, u8 force)
{
    if (ui_feel_bookkeeping(event->kind)) return false;
    engine_event_report report = api->event_report(api->engine, ledger_index);
    if (!report.report) return false;
    if (event->kind == SIM_EVENT_FACTION_FELL) return true;   /* G2-059: the end of a force is news for every player */
    if (event->faction == force) return true;
    if (!ui_feel_hostile(event->kind) || report.subject.kind != ENGINE_SUBJECT_CITY) return false;
    return api->city_at(api->engine, (u8)report.subject.id).owner_force_id == force;
}

/* G2-007: the enemy city that threatens an own city: a city of another
   force next to an own city (10 days away) with more soldiers than the
   own city. The one with the most soldiers wins. ENGINE_NO_CITY if none.
   The rule is assumed: the original named 廬江 next to 柴桑. */
internal u8
ui_feel_threat_city(engine_api *api, u8 force)
{
    void *e = api->engine;
    u8 threat = ENGINE_NO_CITY;
    u32 most = 0;
    for (u32 own = 0; own < api->city_count(e); ++own)
    {
        engine_city mine = api->city_at(e, own);
        if (mine.owner_force_id != force) continue;
        for (u32 other = 0; other < api->city_count(e); ++other)
        {
            engine_city theirs = api->city_at(e, other);
            if (theirs.owner_force_id == force || theirs.owner_force_id == ENGINE_NO_FACTION) continue;
            if (theirs.troops <= mine.troops || theirs.troops <= most) continue;
            if (api->travel_days(e, mine.city_id, theirs.city_id) != CALENDAR_DAYS_PER_TURN) continue;
            most   = theirs.troops;
            threat = theirs.city_id;
        }
    }
    return threat;
}

/* The messages of a turn start: the heading, then the reports since the
   last turn start in ledger order, then the threat report of the adviser
   when an enemy city threatens an own city, and the closing line of the
   adviser (G2-006, G2-007). UI_MESSAGE_CAPACITY keeps the first ones. The
   log keeps all events. */
internal void
ui_feel_collect_messages(ui_state *ui, engine_api *api, u8 force)
{
    u32 count = api->ledger_count(api->engine);
    if (ui->feel.ledger_mark > count) ui->feel.ledger_mark = 0;   /* a load replaced the ledger */
    ui->feel.message_count  = 0;
    ui->feel.message_shown  = 0;
    ui->feel.framed_message = U32_MAX;
    ui->feel.messages[ui->feel.message_count++] = UI_MESSAGE_HEADING;
    for (u32 index = ui->feel.ledger_mark; index < count && ui->feel.message_count < UI_MESSAGE_CAPACITY - 2; ++index)
    {
        sim_event event = api->ledger_at(api->engine, index);
        if (ui_feel_is_report(api, index, &event, force)) ui->feel.messages[ui->feel.message_count++] = index;
    }
    ui->feel.threat_city = ui_feel_threat_city(api, force);
    if (ui->feel.threat_city != ENGINE_NO_CITY) ui->feel.messages[ui->feel.message_count++] = UI_MESSAGE_THREAT;
    /* G3-009 (observed 2026-10-23): an enemy unit that marches on an own city gives a line, with the
       camera on the unit. The goal comes from the last AI turn of its force (unit_goal). */
    for (u32 unit_id = 0; unit_id < api->unit_count(api->engine) && ui->feel.message_count < UI_MESSAGE_CAPACITY - 1; ++unit_id)
    {
        engine_unit unit = api->unit_at(api->engine, unit_id);
        if (unit.id == ENGINE_NO_UNIT || unit.force_id == force) continue;
        engine_unit_goal goal = api->unit_goal(api->engine, unit_id);
        if (!goal.known || goal.city_id == ENGINE_NO_CITY || api->city_at(api->engine, goal.city_id).owner_force_id != force) continue;
        ui->feel.messages[ui->feel.message_count++] = UI_MESSAGE_UNIT_THREAT | (unit_id & 0xFFFFu);
    }
    ui->feel.messages[ui->feel.message_count++] = UI_MESSAGE_NEXT_ORDER;
    ui->feel.ledger_mark = count;
}

/* The speaker of a message: the adviser for the lines of the adviser,
   the speaker of a report, else the ruler. */
internal u16
ui_feel_message_speaker(ui_state *ui, engine_api *api, u32 message)
{
    u16 adviser = api->adviser(api->engine, ui->feel.force);
    u16 ruler = api->force_ruler(api->engine, ui->feel.force);
    if (message == UI_MESSAGE_HEADING) return ruler;
    if ((message & UI_MESSAGE_UNIT_THREAT_MASK) == UI_MESSAGE_UNIT_THREAT && message != UI_MESSAGE_THREAT && message != UI_MESSAGE_NEXT_ORDER && message != UI_MESSAGE_HEADING)
    {
        /* G3-009: the governor of the target city speaks, else the adviser. */
        engine_unit_goal goal = api->unit_goal(api->engine, message & 0xFFFFu);
        u16 governor = goal.city_id != ENGINE_NO_CITY ? api->city_at(api->engine, goal.city_id).commanding_officer_id : (u16)ENGINE_NO_OFFICER;
        if (governor != ENGINE_NO_OFFICER && governor != 0xFFFF) return governor;
        return adviser != ENGINE_NO_OFFICER ? adviser : ruler;
    }
    if (message == UI_MESSAGE_NEXT_ORDER && ui->feel.first_turn && ui->feel.loaded_game) return ENGINE_NO_OFFICER;   /* G3-023: the emperor speaks, no portrait in the clone */
    if (message == UI_MESSAGE_THREAT || message == UI_MESSAGE_NEXT_ORDER) return adviser != ENGINE_NO_OFFICER ? adviser : ruler;
    u16 reporter = api->event_report(api->engine, message).speaker;
    return reporter != ENGINE_NO_OFFICER ? reporter : ruler;
}

/* G2-006, G2-007, G2-032: the camera goes to the place of the message that
   shows: the enemy city of the threat, or the city of the officer of a
   report (the prisoner that escaped). One move for each message. */
internal void
ui_feel_frame_message(ui_state *ui, engine_api *api)
{
    if (!ui_feel_messages_open(ui) || ui->feel.framed_message == ui->feel.message_shown) return;
    ui->feel.framed_message = ui->feel.message_shown;
    u32 message = ui->feel.messages[ui->feel.message_shown];
    if (message == UI_MESSAGE_THREAT) { ui_frame_city(ui, ui->feel.threat_city); return; }
    if (message == UI_MESSAGE_HEADING || message == UI_MESSAGE_NEXT_ORDER) return;
    if ((message & UI_MESSAGE_UNIT_THREAT_MASK) == UI_MESSAGE_UNIT_THREAT)
    {
        engine_unit unit = api->unit_at(api->engine, message & 0xFFFFu);   /* G3-009: the camera goes to the enemy unit */
        if (unit.id != ENGINE_NO_UNIT) ui_frame_hex(ui, api, hex_make(unit.q, unit.r));
        return;
    }
    engine_event_report report = api->event_report(api->engine, message);
    if (report.subject.kind != ENGINE_SUBJECT_OFFICER) return;
    engine_officer officer = api->officer_by_id(api->engine, report.subject.id);
    if (officer.base_location < api->city_count(api->engine)) ui_frame_city(ui, (u8)officer.base_location);
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
    if (!human || ui->round.playing) return;   /* M20: the messages follow the show */
    u32 turn = api->turn(e);
    if (ui->feel.seen_turn && ui->feel.turn == turn && ui->feel.force == force) return;
    ui->feel.seen_turn = true;
    ui->feel.turn      = turn;
    ui->feel.force     = force;
    if (ui->feel.first_turn)
    {
        /* G2-006: after the load, the camera stands at the capital of the player. */
        engine_officer ruler = api->officer_by_id(e, api->force_ruler(e, force));
        if (ruler.base_location < api->city_count(e)) ui_frame_city(ui, (u8)ruler.base_location);
    }
    if (ui->round.show)
    {
        ui_feel_collect_messages(ui, api, force);
        return;
    }
    ui->feel.first_turn = false;
    /* M20, the decision of the player: with the show off, the turn starts
       with no message to click. The reports stay in the log. */
    ui->feel.message_count = 0;
    ui->feel.message_shown = 0;
    ui->feel.ledger_mark   = api->ledger_count(e);
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
    ui_feel_frame_message(ui, api);
    if (!ui_feel_messages_open(ui) && ui->feel.seen_turn) ui->feel.first_turn = false;   /* the lines of the first turn are read */
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
    if ((message & UI_MESSAGE_UNIT_THREAT_MASK) == UI_MESSAGE_UNIT_THREAT && message != UI_MESSAGE_THREAT && message != UI_MESSAGE_NEXT_ORDER)
    {
        /* G3-009: 劉表大人，根據急報，發現了以長安為進攻目標的敵軍蹤影。 */
        char lord[UI_NAME_SIZE * 2], prefix[UI_NAME_SIZE * 3], city[UI_NAME_SIZE * 2], line[UI_NAME_SIZE * 6];
        ui_officer_text(ui, api, api->force_ruler(api->engine, ui->feel.force), lord, sizeof(lord));
        snprintf(prefix, sizeof(prefix), text_get(ui->text, "turn.lord"), lord);
        engine_unit_goal goal = api->unit_goal(api->engine, message & 0xFFFFu);
        city[0] = 0;
        if (goal.city_id != ENGINE_NO_CITY) ui_city_label(ui, api, goal.city_id, city, sizeof(city));
        snprintf(line, sizeof(line), text_get(ui->text, "turn.threat_unit"), city);
        snprintf(out, out_size, "%s%s", prefix, line);
        return;
    }
    if (message == UI_MESSAGE_THREAT || message == UI_MESSAGE_NEXT_ORDER)
    {
        /* G2-006, G2-007: the lines of the adviser, with the name of the ruler first (劉表大人，...). */
        char lord[UI_NAME_SIZE * 2], prefix[UI_NAME_SIZE * 3], city[UI_NAME_SIZE * 2];
        ui_officer_text(ui, api, api->force_ruler(api->engine, ui->feel.force), lord, sizeof(lord));
        snprintf(prefix, sizeof(prefix), text_get(ui->text, "turn.lord"), lord);
        if (message == UI_MESSAGE_THREAT)
        {
            ui_city_label(ui, api, ui->feel.threat_city, city, sizeof(city));
            char line[UI_NAME_SIZE * 6];
            snprintf(line, sizeof(line), text_get(ui->text, "turn.threat"), city);
            snprintf(out, out_size, "%s%s", prefix, line);
        }
        else if (ui->feel.first_turn && ui->feel.loaded_game)
        {
            snprintf(out, out_size, text_get(ui->text, "turn.loaded"), lord);   /* G3-023: 劉表，可終於輪到你了啊。 */
        }
        else
        {
            snprintf(out, out_size, "%s%s", prefix, text_get(ui->text, ui->feel.first_turn ? "turn.our_turn" : "turn.next_order"));
        }
        return;
    }
    engine_event_report report = api->event_report(api->engine, message);
    /* G2-032: a report with a line of its own speaks it (say.<key>); the others read as the log. */
    char id[64];
    snprintf(id, sizeof(id), "say.%s", report.key ? report.key : "none");
    u32 form = text_find_string(ui->text, id) ? TEXT_LINE_SPEECH : TEXT_LINE_LOG;
    ui_report_line(ui, api, message, ui_report_value(api, &report), form, out, out_size);   /* M18 */
}

/* main-map.md, "Message window": 689 x 197 at the bottom centre, the
   portrait of the speaker at the left, the text at the right. M18: the
   speaker of each report comes from event_report, and the heading has the
   ruler. The window shows no counter. The whole window is the button that
   advances. */
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
        u32 message = ui->feel.messages[ui->feel.message_shown];
        u16 speaker_id = ui_feel_message_speaker(ui, api, message);
        if (speaker_id != ENGINE_NO_OFFICER) ui_draw_officer_portrait(ui, api, speaker_id);
        char speaker[UI_NAME_SIZE * 2], text[UI_NAME_SIZE * 6];
        speaker[0] = 0;
        if (speaker_id != ENGINE_NO_OFFICER) ui_officer_text(ui, api, speaker_id, speaker, sizeof(speaker));
        else ui_force_text(ui, api, ui->feel.force, speaker, sizeof(speaker));
        ui_feel_message_text(ui, api, message, text, sizeof(text));
        ImGui::Separator();
        ImGui::TextWrapped("%s", text);
        ImGui::TextDisabled("%s", speaker);
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
