/* M20 the turn of the others (ROADMAP M20; ui/clone-gap-report.md section
   7; ui/main-map.md "Scenes on the map"). The decision of the player
   (DECISIONS.md, 2026-10-20): the show of the AI round is off by default,
   and no part of it needs a click. The engine runs the whole round inside
   end_turn, so the sim stays instant. Only the front end plays it.
   - With the show off, 進行 ends the turn, and the next turn of the player
     starts with no message window. The log keeps every event.
   - With the show on, the UI plays the events of the AI factions after
     進行. The HUD names each AI faction in turn, with its 技巧 and no
     行動力. An event of a city with a sign rises as a value over the city.
     A unit that moved walks hex by hex from its hex before 進行. The
     camera stays where the player left it. The turn messages of the
     player follow the show.
   Included by ui.cpp after ui_report.cpp. */

#define UI_ROUND_FACTION_SECONDS 0.8f    /* the HUD names a new faction this long before its first event */
#define UI_ROUND_EVENT_SECONDS   0.35f   /* the pause after a value */
#define UI_ROUND_HEX_SECONDS     0.18f   /* one hex of a move */

internal bool ui_feel_bookkeeping(sim_event_kind kind);   /* ui_feel.cpp */
internal b32  ui_walk_hex(ui_state *ui, u32 unit_id, hex_coord *out);                                   /* ui_war.cpp */
internal b32  ui_war_take_event(ui_state *ui, engine_api *api, const sim_event *event, u8 player);     /* ui_war.cpp */

/* 進行: the turn of the player ends. With the show on, the UI keeps the
   hex of each unit and the start of the round in the ledger first. */
internal void
ui_round_proceed(ui_state *ui, engine_api *api)
{
    void *e = api->engine;
    u32 mark = api->ledger_count(e);
    ui->feel.message_shown = ui->feel.message_count;   /* the turn ends: the messages that the player did not read close */
    if (ui->round.show)
    {
        for (u32 id = 0; id < SIM_UNIT_CAPACITY; ++id) ui->round.known[id] = false;
        u32 count = api->unit_count(e);
        for (u32 id = 0; id < count && id < SIM_UNIT_CAPACITY; ++id)
        {
            engine_unit unit = api->unit_at(e, id);
            if (unit.id == ENGINE_NO_UNIT) continue;
            ui->round.before[id] = hex_make(unit.q, unit.r);
            ui->round.known[id]  = true;
        }
    }
    api->end_turn(e);
    if (!ui->round.show) return;
    ui->round.playing     = true;
    ui->round.next        = mark;
    ui->round.end         = api->ledger_count(e);
    ui->round.force       = ENGINE_NO_FACTION;
    ui->round.wait        = 0.0f;
    ui->round.named_count = 0;
    ui->round.shown_count = 0;
    ui->round.move_unit   = ENGINE_NO_UNIT;
}

/* The faction that the HUD names: the AI faction of the show, else the
   faction that acts. */
internal u8
ui_round_hud_force(ui_state *ui, engine_api *api)
{
    if (ui->round.playing && ui->round.force != ENGINE_NO_FACTION) return ui->round.force;
    return api->whose_turn(api->engine);
}

/* The hex where the scene draws a unit. During the show a unit stays on
   its hex before 進行 until its move plays, and then it walks. */
internal hex_coord
ui_round_unit_hex(ui_state *ui, engine_unit *unit)
{
    hex_coord now = hex_make(unit->q, unit->r);
    if (ui_walk_hex(ui, unit->id, &now)) return now;   /* a unit of the player walks its move */
    if (!ui->round.playing || unit->id >= SIM_UNIT_CAPACITY) return now;
    if (ui->round.move_unit == unit->id && ui->round.move_steps > 0)
    {
        f32 part = (f32)ui->round.move_step / (f32)ui->round.move_steps;
        f32 fq = (f32)ui->round.move_from.q + (f32)(ui->round.move_to.q - ui->round.move_from.q) * part;
        f32 fr = (f32)ui->round.move_from.r + (f32)(ui->round.move_to.r - ui->round.move_from.r) * part;
        return hex_round(fq, fr);
    }
    return ui->round.known[unit->id] ? ui->round.before[unit->id] : now;
}

/* True if the city is on the screen. The camera stays during the show, so
   a value of a city off the screen does not show. Without a scene (the
   headless tests), each city counts as on the screen. */
internal b32
ui_round_city_visible(ui_state *ui, engine_api *api, u8 city_id)
{
    ImVec2 display = ImGui::GetIO().DisplaySize;
    if (!ui->scene.enabled || !ui->scene.initialized || display.x <= 0.0f || display.y <= 0.0f) return true;
    engine_city_slots site = api->city_slots(api->engine, city_id);
    if (site.city_id == ENGINE_NO_CITY) return false;
    m4 view_projection = camera_view_projection(&ui->scene.cam, display.x, display.y);
    f32 x = 0.0f, y = 0.0f;
    if (!ui_scene_screen(api, view_projection, hex_make(site.q, site.r), UI_LABEL_LIFT, &x, &y)) return false;
    return x >= 0.0f && y >= 0.0f && x <= display.x && y <= display.y;
}

/* One event of an AI faction. Returns true when the event takes time. */
internal b32
ui_round_play_event(ui_state *ui, engine_api *api, u32 index, sim_event *event)
{
    if (event->kind == SIM_EVENT_UNIT_MOVED && event->detail < SIM_UNIT_CAPACITY)
    {
        engine_unit unit = api->unit_at(api->engine, event->detail);
        if (unit.id == ENGINE_NO_UNIT || !ui->round.known[unit.id]) return false;
        hex_coord from = ui->round.before[unit.id], to = hex_make(unit.q, unit.r);
        ui->round.before[unit.id] = to;   /* a second move of the unit in the round starts here */
        u32 steps = (u32)hex_distance(from, to);
        if (steps == 0) return false;
        ui->round.move_unit  = unit.id;
        ui->round.move_from  = from;
        ui->round.move_to    = to;
        ui->round.move_step  = 0;
        ui->round.move_steps = steps;
        ui->round.wait       = UI_ROUND_HEX_SECONDS;
        return true;
    }
    if (ui_war_take_event(ui, api, event, ui_report_player(api)))   /* a hit of a unit rises in the time of the show */
    {
        ui->round.wait = UI_ROUND_EVENT_SECONDS;
        return true;
    }
    engine_event_report report = api->event_report(api->engine, index);
    if (report.subject.kind != ENGINE_SUBJECT_CITY || report.sign == ENGINE_SIGN_NEUTRAL || !report.delta) return false;
    if (!ui_round_city_visible(ui, api, (u8)report.subject.id)) return false;
    if (ui->report.float_count < UI_FLOAT_CAPACITY)
    {
        u32 place = ui->report.float_count++;
        ui->report.floats[place].city         = (u8)report.subject.id;
        ui->report.floats[place].delta        = report.delta;
        ui->report.floats[place].sign         = report.sign;
        ui->report.floats[place].seconds      = UI_FLOAT_SECONDS;
        ui->report.floats[place].resource     = report.resource;
        ui->report.floats[place].resource_key = report.resource_key;
    }
    if (ui->round.shown_count < UI_ROUND_SHOWN_CAPACITY) ui->round.shown[ui->round.shown_count++] = index;
    ui->round.wait = UI_ROUND_EVENT_SECONDS;
    return true;
}

/* A walk: one hex in each step. */
internal void
ui_round_step_move(ui_state *ui)
{
    ui->round.move_step++;
    if (ui->round.move_step >= ui->round.move_steps) ui->round.move_unit = ENGINE_NO_UNIT;
    else                                             ui->round.wait = UI_ROUND_HEX_SECONDS;
}

/* Each frame, before the panels: the next step of the show. A new AI
   faction takes the HUD first. The events of the player and the books of
   the turn play no step. */
internal void
ui_round_update(ui_state *ui, engine_api *api, f32 dt_seconds)
{
    if (!ui->round.playing) return;
    ui->round.end = api->ledger_count(api->engine);   /* the UI can end the turns of more AI factions (ui_feel.cpp) */
    ui->round.wait -= dt_seconds;
    if (ui->round.wait > 0.0f) return;
    if (ui->round.move_unit != ENGINE_NO_UNIT)
    {
        ui_round_step_move(ui);
        return;
    }
    u8 player = ui_report_player(api);
    while (ui->round.next < ui->round.end)
    {
        u32 index = ui->round.next;
        sim_event event = api->ledger_at(api->engine, index);
        u8 faction = event.faction == SIM_NO_FORCE ? ENGINE_NO_FACTION : event.faction;
        if (ui_feel_bookkeeping(event.kind) || faction == ENGINE_NO_FACTION || faction == player)
        {
            ui->round.next++;
            continue;
        }
        if (faction != ui->round.force)
        {
            ui->round.force = faction;
            if (ui->round.named_count < UI_ROUND_FACTION_CAPACITY) ui->round.named[ui->round.named_count++] = faction;
            ui->round.wait = UI_ROUND_FACTION_SECONDS;
            return;
        }
        ui->round.next++;
        if (ui_round_play_event(ui, api, index, &event)) return;
    }
    ui->round.playing = false;
    ui->round.force   = ENGINE_NO_FACTION;
}
