/* A shared game (docs/MULTIPLAYER.md, MP3). The web page owns the lobby and
   the socket of the room. Each frame, the UI takes the events of the page
   (ui_net_begin_frame) and sends the new steps of the local seat
   (ui_net_end_frame), through the net functions of the platform:
   - join: the start image of the host loads. The factions of the seats
     with a player are human, the other factions go to the AI, and
     multiplayer_begin starts the shared game. Each player then reports
     the checksum of the start state (step -1).
   - step: a step of another player applies, and the UI reports the
     checksum after it.
   - seats, desync and error: the banner shows them.
   The UI calls engine_api only. Included by ui.cpp. */

#define UI_NET_MAX_EVENTS_PER_FRAME 256
#define UI_NET_SEATS_TEXT_SIZE      4096

internal void
ui_net_send(ui_frame_info *frame, const platform_net_event *event)
{
    if (frame->net_send) frame->net_send(event);
}

internal void
ui_net_send_checksum(ui_frame_info *frame, s32 step, u64 checksum, u8 acting)
{
    platform_net_event event = {};
    event.kind     = PLATFORM_NET_CHECKSUM;
    event.step     = step;
    event.checksum = checksum;
    event.acting   = acting;
    ui_net_send(frame, &event);
}

/* The lines "force<TAB>player[<TAB>online]" of the page. */
internal void
ui_net_read_seats(ui_state *ui, const char *text)
{
    ui->net.seat_count = 0;
    for (const char *line = text; line && *line && ui->net.seat_count < UI_NET_SEATS;)
    {
        const char *end = strchr(line, '\n');
        u32 length = end ? (u32)(end - line) : (u32)strlen(line);
        char row[UI_NET_NAME_SIZE + 32];
        snprintf(row, sizeof(row), "%.*s", (int)Minimum(length, (u32)sizeof(row) - 1), line);
        char *player = strchr(row, '\t');
        char *online = player ? strchr(player + 1, '\t') : 0;
        if (player) *player++ = 0;
        if (online) *online++ = 0;
        u32 index = ui->net.seat_count++;
        ui->net.seat_force[index]  = (u8)atoi(row);
        snprintf(ui->net.seat_player[index], UI_NET_NAME_SIZE, "%s", player ? player : "");
        ui->net.seat_online[index] = online ? atoi(online) != 0 : false;
        line = end ? end + 1 : 0;
    }
}

/* The name of the player of the faction, or 0 for a faction of the AI. */
internal const char *
ui_net_player_of(ui_state *ui, u8 force)
{
    for (u32 index = 0; index < ui->net.seat_count; ++index)
    {
        if (ui->net.seat_force[index] == force && ui->net.seat_player[index][0]) return ui->net.seat_player[index];
    }
    return 0;
}

internal void
ui_net_join(ui_state *ui, engine_api *api, ui_frame_info *frame, const platform_net_event *event)
{
    void *e = api->engine;
    /* MP6: after a desync, the page loads the good state of the room. The shared
       game of this engine ends first, because a shared game refuses a load. */
    if (api->multiplayer_info(e).shared) api->multiplayer_end(e);
    /* The names first: the load of a name index adds events to the ledger, and
       the ledger is a part of the checksum. The start image then replaces the ledger and the names. */
    ui_ensure_city_names(ui, api);
    if (!api->load_start_image(e, event->data, event->size))
    {
        snprintf(ui->net.error, sizeof(ui->net.error), "%s", text_get(ui->text, "net.bad_image"));
        return;
    }
    ui_net_read_seats(ui, event->text);
    b32 snapshot = event->step > 0;   /* MP5: a snapshot holds the human factions of the game */
    for (u32 index = 0; index < api->faction_count(e) && !snapshot; ++index)
    {
        /* The same setters on each engine: the seats without a player go to the AI. */
        u8 force = api->faction_at(e, index).force_id;
        api->set_faction_human(e, force, ui_net_player_of(ui, force) != 0);
    }
    api->multiplayer_begin(e, event->seat == PLATFORM_NET_NO_SEAT ? (u8)ENGINE_NO_FACTION : event->seat, event->host);
    if (snapshot) api->multiplayer_first_step(e, (u32)event->step);
    ui->net.joined        = true;
    ui->net.waiting       = false;
    ui->net.desync_step   = -1;
    ui->net.error[0]      = 0;
    ui->net.snapshot_turn = api->turn(e);
    ui_enter_game(ui);
    /* Each player reports the start state, or the state of the snapshot: the server compares the reports. */
    ui_net_send_checksum(frame, snapshot ? event->step - 1 : -1, api->state_checksum(e), api->whose_turn(e));
}

internal void
ui_net_apply_step(ui_state *ui, engine_api *api, ui_frame_info *frame, const platform_net_event *event)
{
    void *e = api->engine;
    if (!ui->net.joined) return;
    if (event->step < 0 || (u32)event->step != api->multiplayer_info(e).step_count)
    {
        snprintf(ui->net.error, sizeof(ui->net.error), "%s", text_get(ui->text, "net.missing_step"));
        return;
    }
    engine_step_apply_result applied = api->step_apply(e, event->data, event->size);
    if (applied.status != ENGINE_STEP_APPLIED)
    {
        snprintf(ui->net.error, sizeof(ui->net.error), "%s", text_get(ui->text, "net.bad_step"));
        return;
    }
    /* MP5: the host takes the snapshot after a step of the server. */
    if (api->multiplayer_info(e).host && (applied.kind == ENGINE_STEP_END_TURN || applied.kind == ENGINE_STEP_AI_TURN))
        ui->net.snapshot_author = true;
    ui_net_send_checksum(frame, event->step, applied.checksum, api->whose_turn(e));
}

/* A pack is the name of a folder under data/: lower-case letters, digits and "_". */
internal b32
ui_net_is_pack_name(const char *pack)
{
    if (!pack || !pack[0] || strlen(pack) >= CONTENT_KEY_SIZE) return false;
    for (const char *at = pack; *at; ++at)
    {
        if (!((*at >= 'a' && *at <= 'z') || (*at >= '0' && *at <= '9') || *at == '_')) return false;
    }
    return true;
}

/* MP4: the pack of the room comes before the join. If the engine has
   another pack, the game starts the engine of that pack (game.pack_request).
   Gives true when the UI must stop the events of this frame: the join then
   waits in the queue of the platform for the new engine and the new UI. */
internal b32
ui_net_use_pack(ui_state *ui, engine_api *api, const platform_net_event *event)
{
    if (!ui_net_is_pack_name(event->text))
    {
        snprintf(ui->net.error, sizeof(ui->net.error), "%s", text_get(ui->text, "net.bad_pack"));
        return false;
    }
    if (strcmp(api->content_pack(api->engine).pack, event->text) == 0) return false;
    snprintf(ui->game.pack_request, sizeof(ui->game.pack_request), "%s", event->text);
    return true;
}

/* MP7: a line of the chat from the page, "name<TAB>line". The page and the
   room server clean the text; the panel shows it as text. */
internal void
ui_net_chat_line(ui_state *ui, const char *text)
{
    if (!text || !text[0]) return;
    snprintf(ui->net.chat_lines[ui->net.chat_count % UI_NET_CHAT_LINES], UI_NET_CHAT_SIZE, "%s", text);
    ui->net.chat_count++;
    if (!ui->net.show_chat) ui->net.chat_unread++;
}

internal void
ui_net_timer(ui_state *ui, const platform_net_event *event)
{
    ui->net.timer_on      = true;
    ui->net.timer_paused  = event->step < 0;
    ui->net.timer_seconds = event->step < 0 ? 0.0f : (f32)event->step;
}

/* A short message in the banner: the result of a save, of a load or of a copy. */
internal void
ui_net_show_notice(ui_state *ui, u32 notice)
{
    if (notice >= PLATFORM_NET_NOTICE_COUNT) return;
    ui->net.notice         = notice;
    ui->net.notice_seconds = UI_NET_NOTICE_SECONDS;
}

/* MP6: a desync. The banner names the step, and the image of the state after
   it goes to the page. The server keeps the images for the developer. Step -1
   ends the desync: the room decided, and the game goes on. */
internal void
ui_net_desync(ui_state *ui, engine_api *api, ui_frame_info *frame, s32 step)
{
    ui->net.desync_step = step;
    memory_arena *arena = frame->frame_arena;
    void *e = api->engine;
    if (step < 0 || !arena) return;
    u32 capacity = api->start_image_size(e);
    if (capacity > arena->size - arena->used) return;
    u8 *image = (u8 *)arena_push_size(arena, capacity);
    platform_net_event state = {};
    state.kind = PLATFORM_NET_STATE_IMAGE;
    state.step = step;
    state.data = image;
    state.size = api->start_image(e, image, capacity);
    if (state.size) ui_net_send(frame, &state);
}

internal void
ui_net_begin_frame(ui_state *ui, engine_api *api, ui_frame_info *frame)
{
    ui->net.available = frame->net_send && frame->net_poll;
    if (!ui->net.available) return;
    if (ui->net.timer_on && !ui->net.timer_paused) ui->net.timer_seconds = Maximum(0.0f, ui->net.timer_seconds - frame->dt_seconds);
    if (ui->net.notice)
    {
        ui->net.notice_seconds -= frame->dt_seconds;
        if (ui->net.notice_seconds <= 0.0f) ui->net.notice = PLATFORM_NET_NOTICE_NONE;
    }
    platform_net_event event;
    for (u32 count = 0; count < UI_NET_MAX_EVENTS_PER_FRAME && frame->net_poll(&event); ++count)
    {
        switch (event.kind)
        {
            case PLATFORM_NET_JOIN:   ui_net_join(ui, api, frame, &event);       break;
            case PLATFORM_NET_STEP:   ui_net_apply_step(ui, api, frame, &event); break;
            case PLATFORM_NET_SEATS:  ui_net_read_seats(ui, event.text);         break;
            case PLATFORM_NET_DESYNC: ui_net_desync(ui, api, frame, event.step);  break;
            case PLATFORM_NET_ERROR:  snprintf(ui->net.error, sizeof(ui->net.error), "%s", event.text ? event.text : ""); break;
            case PLATFORM_NET_TIMER:  ui_net_timer(ui, &event);                  break;
            case PLATFORM_NET_CHAT:   ui_net_chat_line(ui, event.text);          break;
            case PLATFORM_NET_ROOM_INFO:
                snprintf(ui->net.invite_link, sizeof(ui->net.invite_link), "%s", event.text ? event.text : "");
                ui->net.late_join = event.step != 0;
                break;
            case PLATFORM_NET_NOTICE: ui_net_show_notice(ui, (u32)event.step);   break;
            case PLATFORM_NET_AUTO:   ui->net.auto_play = event.step != 0;       break;
            case PLATFORM_NET_PACK:
                if (ui_net_use_pack(ui, api, &event)) return;
                break;
            default: break;
        }
    }
}

/* "Play online" (ui_draw_start_settings): the start image and one line for
   each chosen ruler go to the page. The page makes the room. */
internal void
ui_net_host(ui_state *ui, engine_api *api, ui_frame_info *frame)
{
    void *e = api->engine;
    memory_arena *arena = frame->frame_arena;
    u32 capacity = api->start_image_size(e);
    if (!arena || (u64)capacity + UI_NET_SEATS_TEXT_SIZE > arena->size - arena->used) return;
    u8 *image = (u8 *)arena_push_size(arena, capacity);
    u32 size = api->start_image(e, image, capacity);
    char *seats = (char *)arena_push_size(arena, UI_NET_SEATS_TEXT_SIZE);
    u32 used = 0;
    seats[0] = 0;
    for (u32 index = 0; index < api->faction_count(e); ++index)
    {
        u8 force = api->faction_at(e, index).force_id;
        if (force >= 64 || !((ui->game.rulers >> force) & 1)) continue;
        char name[UI_NET_NAME_SIZE];
        ui_force_text(ui, api, force, name, sizeof(name));
        used += (u32)snprintf(seats + used, UI_NET_SEATS_TEXT_SIZE - used, "%u\t%s\n", force, name);
        if (used >= UI_NET_SEATS_TEXT_SIZE - UI_NET_NAME_SIZE - 8) break;
    }
    engine_slot_pack pack = api->content_pack(e);   /* MP4: the room records the pack, and each player starts its engine */
    platform_net_event event = {};
    event.kind = PLATFORM_NET_HOST;
    event.data = image;
    event.size = size;
    event.text = seats;
    event.pack = pack.pack;
    if (size) ui_net_send(frame, &event);
    ui->net.waiting = size != 0;
}

/* MP5: after the step that ends a game turn, the host sends the image of the
   state to the page. The page puts it in the bucket as the snapshot of that
   step. A page that joins later loads it and applies only the steps after it.
   Only the host saves. */
internal void
ui_net_send_snapshot(ui_state *ui, engine_api *api, ui_frame_info *frame)
{
    void *e = api->engine;
    engine_multiplayer_info info = api->multiplayer_info(e);
    b32 author  = ui->net.snapshot_author && info.host;
    b32 request = ui->net.snapshot_request;   /* 儲存 of the host: a snapshot in any turn */
    ui->net.snapshot_author  = false;
    ui->net.snapshot_request = request && info.outgoing;   /* the request waits for the steps of this frame */
    if (info.outgoing) return;
    if (!info.step_count)
    {
        /* No step yet: the start image on the server is the state of the game. */
        if (request) ui_net_show_notice(ui, PLATFORM_NET_NOTICE_SAVED);
        return;
    }
    if (!request && (!author || api->turn(e) == ui->net.snapshot_turn)) return;
    memory_arena *arena = frame->frame_arena;
    u32 capacity = api->start_image_size(e);
    u32 size = 0;
    u8 *image = 0;
    if (arena && capacity <= arena->size - arena->used)
    {
        image = (u8 *)arena_push_size(arena, capacity);
        size  = api->start_image(e, image, capacity);
    }
    if (!size)
    {
        if (request) ui_net_show_notice(ui, PLATFORM_NET_NOTICE_SAVE_FAILED);
        return;
    }
    platform_net_event snapshot = {};
    snapshot.kind     = PLATFORM_NET_SNAPSHOT;
    snapshot.step     = (s32)info.step_count - 1;
    snapshot.checksum = api->state_checksum(e);
    snapshot.acting   = request ? 1 : 0;
    snapshot.data     = image;
    snapshot.size     = size;
    ui_net_send(frame, &snapshot);
    ui->net.snapshot_turn = api->turn(e);
    if (request) ui_net_show_notice(ui, PLATFORM_NET_NOTICE_SAVING);
}

/* No faction acts when each faction is a faction of the AI. The sim runs all of them in one
   step and stops at the end of the game turn. The host then ends the game turn after a pause,
   so that the players can watch the game. */
internal void
ui_net_end_game_turn_of_the_ai(ui_state *ui, engine_api *api, ui_frame_info *frame)
{
    void *e = api->engine;
    engine_multiplayer_info info = api->multiplayer_info(e);
    if (!info.shared || !info.host || info.acting_force != ENGINE_NO_FACTION || ui->net.desync_step >= 0 || api->outcome(e).kind != SIM_OUTCOME_NONE)
    {
        ui->net.no_faction_seconds = 0.0f;
        return;
    }
    ui->net.no_faction_seconds += frame->dt_seconds;
    if (ui->net.no_faction_seconds < UI_NET_NO_FACTION_SECONDS) return;
    ui->net.no_faction_seconds = 0.0f;
    api->end_turn(e);
}

internal void
ui_net_end_frame(ui_state *ui, engine_api *api, ui_frame_info *frame)
{
    if (!ui->net.available) return;
    if (ui->net.host_request)
    {
        ui->net.host_request = false;
        ui_net_host(ui, api, frame);
    }
    if (ui->net.open_room)
    {
        ui->net.open_room = false;
        platform_net_event open = {};
        open.kind = PLATFORM_NET_OPEN_ROOM;
        open.text = ui->net.room_code;
        ui_net_send(frame, &open);
    }
    if (ui->net.chat_send)
    {
        /* MP7: the line goes to the room through the page. It is no step of the game. */
        ui->net.chat_send = false;
        platform_net_event chat = {};
        chat.kind = PLATFORM_NET_CHAT;
        chat.text = ui->net.chat_text;
        if (ui->net.chat_text[0]) ui_net_send(frame, &chat);
        ui->net.chat_text[0] = 0;
    }
    if (ui->net.take_seat)
    {
        ui->net.take_seat = false;
        platform_net_event seat = {};
        seat.kind = PLATFORM_NET_TAKE_SEAT;
        seat.step = ui->net.take_seat_force;
        ui_net_send(frame, &seat);
    }
    if (ui->net.copy_link)
    {
        /* The canvas has no access to the clipboard of the system. The page copies the link. */
        ui->net.copy_link = false;
        platform_net_event copy = {};
        copy.kind = PLATFORM_NET_COPY_LINK;
        copy.text = ui->net.invite_link;
        ui_net_send(frame, &copy);
    }
    if (ui->net.auto_send)
    {
        ui->net.auto_send = false;
        platform_net_event choice = {};
        choice.kind = PLATFORM_NET_AUTO;
        choice.step = ui->net.auto_want ? 1 : 0;
        ui_net_send(frame, &choice);
    }
    if (!ui->net.joined) return;
    ui_net_end_game_turn_of_the_ai(ui, api, frame);
    u8 bytes[ENGINE_STEP_WIRE_CAPACITY];
    for (;;)
    {
        engine_outgoing_step out = api->steps_outgoing(api->engine, bytes, sizeof(bytes));
        if (!out.size) break;
        platform_net_event step = {};
        step.kind = PLATFORM_NET_STEP;
        step.step = (s32)out.step;
        step.data = bytes;
        step.size = out.size;
        ui_net_send(frame, &step);
        ui_net_send_checksum(frame, (s32)out.step, out.checksum, out.acting);
        ui->net.snapshot_author = true;
    }
    ui_net_send_snapshot(ui, api, frame);
}

/* MP5: the room code of the title menu. The page opens the invite route of the code. */
internal void
ui_draw_title_room_code(ui_state *ui)
{
    if (!ui->net.available) return;
    char label[96];
    ImGui::Separator();
    ImGui::SetNextItemWidth(160);
    ImGui::InputText(ui_label(label, sizeof(label), ui, "net.room_code", "title_code"), ui->net.room_code, sizeof(ui->net.room_code),
                     ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank);
    ImGui::SameLine();
    ImGui::BeginDisabled(strlen(ui->net.room_code) != sizeof(ui->net.room_code) - 2);
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "net.join_room", "title_join"))) ui->net.open_room = true;
    ImGui::EndDisabled();
}

/* MP7: the chat panel of a shared game. Each line shows as text. The field
   sends a line to the page at the end of the frame (ui_net_end_frame). */
internal void
ui_draw_net_chat(ui_state *ui)
{
    if (!ui->net.show_chat || !ui->net.joined) return;
    char title[128], label[96], row[UI_NET_CHAT_SIZE + 32];
    ImGui::SetNextWindowSize(ImVec2(360, 260), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "net.chat", "NetChat"), &ui->net.show_chat)) { ImGui::End(); return; }
    ui->net.chat_unread = 0;
    u32 first = ui->net.chat_count > UI_NET_CHAT_LINES ? ui->net.chat_count - UI_NET_CHAT_LINES : 0;
    for (u32 index = first; index < ui->net.chat_count; ++index)
    {
        const char *text = ui->net.chat_lines[index % UI_NET_CHAT_LINES];
        const char *tab  = strchr(text, '\t');
        if (tab) snprintf(row, sizeof(row), "%.*s: %s###chat_line%u", (int)(tab - text), text, tab + 1, index - first);
        else     snprintf(row, sizeof(row), "%s###chat_line%u", text, index - first);
        ImGui::BeginDisabled();
        ImGui::Selectable(row, false);
        ImGui::EndDisabled();
    }
    ImGui::Separator();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80);
    b32 entered = ImGui::InputText("###chat_text", ui->net.chat_text, sizeof(ui->net.chat_text), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "net.chat_send", "chat_send")) || entered) ui->net.chat_send = true;
    ImGui::End();
}

internal b32
ui_net_faction_is_human(engine_api *api, u8 force)
{
    for (u32 index = 0; index < api->faction_count(api->engine); ++index)
    {
        engine_faction faction = api->faction_at(api->engine, index);
        if (faction.force_id == force) return faction.human;
    }
    return false;
}

/* MP9: the invite panel of a shared game. It shows the link and the free seats. A
   person without a seat can take a free seat: the page asks the server and loads again. */
internal void
ui_draw_net_invite(ui_state *ui, engine_api *api)
{
    if (!ui->net.show_invite || !ui->net.joined) return;
    char title[128], label[96], name[UI_NET_NAME_SIZE], row[UI_NET_NAME_SIZE * 2 + 96];
    ImGui::SetNextWindowSize(ImVec2(420, 220), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "net.invite", "NetInvite"), &ui->net.show_invite)) { ImGui::End(); return; }
    ImGui::TextUnformatted(text_get(ui->text, "net.invite_link"));
    ImGuiStyle &style = ImGui::GetStyle();
    ui_label(label, sizeof(label), ui, "net.copy", "copy_link");
    ImGui::SetNextItemWidth(-(ImGui::CalcTextSize(label, 0, true).x + style.FramePadding.x * 2.0f + style.ItemSpacing.x));
    ImGui::InputText("###invite_link", ui->net.invite_link, sizeof(ui->net.invite_link), ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button(label) && ui->net.invite_link[0]) ui->net.copy_link = true;
    b32 can_take = ui->net.late_join && api->multiplayer_info(api->engine).local_force == ENGINE_NO_FACTION;
    for (u32 index = 0; index < ui->net.seat_count; ++index)
    {
        u8 force = ui->net.seat_force[index];
        ui_force_text(ui, api, force, name, sizeof(name));
        if (ui->net.seat_player[index][0] || !can_take)
        {
            /* The faction and its player, or the computer for a free seat. */
            if (!ui->net.seat_player[index][0])          ImGui::Text("%s: %s", name, text_get(ui->text, "net.computer"));
            else if (ui_net_faction_is_human(api, force)) ImGui::Text("%s: %s", name, ui->net.seat_player[index]);
            else ImGui::Text("%s: %s (%s)", name, ui->net.seat_player[index], text_get(ui->text, "net.ai_plays"));
            continue;
        }
        snprintf(row, sizeof(row), "%s: %s###take_seat%u", name, text_get(ui->text, "net.take_seat"), (u32)force);
        if (ImGui::Button(row))
        {
            ui->net.take_seat       = true;
            ui->net.take_seat_force = force;
        }
    }
    ImGui::End();
}

/* The banner of a shared game: whose turn it is, the wait of the host, a
   desync and the last error. */
internal void
ui_draw_net_banner(ui_state *ui, engine_api *api)
{
    engine_multiplayer_info info = api->multiplayer_info(api->engine);
    if (!info.shared && !ui->net.waiting) return;
    char title[128], faction[UI_NET_NAME_SIZE] = "", who[UI_NET_NAME_SIZE * 2 + 96], line[sizeof(who) + 32];
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, 8.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "net.title", "NetBanner"), 0, flags)) { ImGui::End(); return; }
    /* The faction that acts and its player: "Faction (player)". */
    const char *player = ui_net_player_of(ui, info.acting_force);
    if (!player) player = text_get(ui->text, "net.computer");
    if (info.acting_force != ENGINE_NO_FACTION) ui_force_text(ui, api, info.acting_force, faction, sizeof(faction));
    if (!info.shared)                 snprintf(who, sizeof(who), "%s", text_get(ui->text, "net.waiting"));
    else if (ui->net.desync_step >= 0) snprintf(who, sizeof(who), "%s %d", text_get(ui->text, "net.desync"), ui->net.desync_step);
    else if (!faction[0])             snprintf(who, sizeof(who), "%s", text_get(ui->text, "net.computer_acts"));
    else if (info.local_force != ENGINE_NO_FACTION && info.acting_force == info.local_force)
        snprintf(who, sizeof(who), "%s: %s (%s)", text_get(ui->text, "net.your_turn"), faction, player);
    else                              snprintf(who, sizeof(who), "%s (%s) %s", faction, player, text_get(ui->text, "net.acts"));
    snprintf(line, sizeof(line), "%s###net_turn", who);
    ImGui::BeginDisabled();
    ImGui::Selectable(line, false);
    if (ui->net.notice)
    {
        local_persist const char *const notice_keys[PLATFORM_NET_NOTICE_COUNT] =
            { "", "net.saving", "net.saved", "net.save_failed", "net.load_off", "net.copied", "net.copy_failed", "net.host_saves" };
        snprintf(line, sizeof(line), "%s###net_notice", text_get(ui->text, notice_keys[ui->net.notice]));
        ImGui::Selectable(line, false);
    }
    if (info.shared && ui->net.timer_on && info.acting_force != ENGINE_NO_FACTION)
    {
        /* MP4: the time left in the faction turn, or the stop of the timer. */
        u32 seconds = (u32)ui->net.timer_seconds;
        if (ui->net.timer_paused) snprintf(line, sizeof(line), "%s###net_timer", text_get(ui->text, "net.paused"));
        else                      snprintf(line, sizeof(line), "%u:%02u###net_timer", seconds / 60, seconds % 60);
        ImGui::Selectable(line, false);
    }
    ImGui::EndDisabled();
    if (info.shared)
    {
        /* MP7: the chat, with the number of the lines that the player did not read. */
        if (ui->net.chat_unread) snprintf(line, sizeof(line), "%s (%u)###net_chat", text_get(ui->text, "net.chat"), ui->net.chat_unread);
        else                     snprintf(line, sizeof(line), "%s###net_chat", text_get(ui->text, "net.chat"));
        if (ImGui::SmallButton(line))
        {
            ui->net.show_chat   = !ui->net.show_chat;
            ui->net.chat_unread = 0;
        }
        /* MP9: the invite link and the free seats. */
        ImGui::SameLine();
        snprintf(line, sizeof(line), "%s###net_invite", text_get(ui->text, "net.invite"));
        if (ImGui::SmallButton(line)) ui->net.show_invite = !ui->net.show_invite;
        /* The AI plays the faction of the seat. The next click gives the faction back. */
        if (info.local_force != ENGINE_NO_FACTION)
        {
            ImGui::SameLine();
            snprintf(line, sizeof(line), "%s###net_auto", text_get(ui->text, ui->net.auto_play ? "net.auto_off" : "net.auto_on"));
            if (ImGui::SmallButton(line))
            {
                ui->net.auto_send = true;
                ui->net.auto_want = !ui->net.auto_play;
            }
        }
        ImGui::SameLine();
        snprintf(line, sizeof(line), "%s###net_follow", text_get(ui->text, "net.follow"));
        ImGui::Checkbox(line, &ui->net.follow);
    }
    if (ui->net.error[0]) ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s", ui->net.error);
    ImGui::End();
}
