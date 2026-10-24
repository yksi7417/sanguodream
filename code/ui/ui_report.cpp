/* M18: the feedback of a command (ROADMAP M18; main-map.md "Bottom-left
   panel", "Speech bubble" and "Floating value"; ui/clone-gap-report.md
   sections 1, 4 and 7). Three parts:
   - the message log at the bottom left: a heading for each turn and one
     line for each event, in the words of the text provider;
   - the speech bubble over the base after a command of a player: the
     first officer of the command says a line in the tone of the event;
   - the values that rise over the base after the command.
   The engine gives the facts (event_report). A text provider writes the
   words (text.h). The player decided on 2026-10-20 that the provider is a
   plug-in, so that a small language model can replace the templates.
   Included by ui.cpp after ui_locate.cpp, for the screen point of a base. */

internal bool ui_feel_bookkeeping(sim_event_kind kind);   /* ui_feel.cpp */

internal void
ui_set_text_provider(ui_state *ui, text_provider provider)
{
    ui->report.provider = provider.line ? provider : text_template_provider();
}

/* The first faction that a player plays, or ENGINE_NO_FACTION. */
internal u8
ui_report_player(engine_api *api)
{
    for (u32 index = 0; index < api->faction_count(api->engine); ++index)
    {
        engine_faction faction = api->faction_at(api->engine, index);
        if (faction.human) return faction.force_id;
    }
    return ENGINE_NO_FACTION;
}

/* The value of the city or the officer after the event, for the templates
   that show it. */
internal s64
ui_report_value(engine_api *api, const engine_event_report *report)
{
    if (!report->key) return TEXT_NO_VALUE;
    const char *key = report->key;
    if (report->subject.kind == ENGINE_SUBJECT_OFFICER)
    {
        engine_officer officer = api->officer_by_id(api->engine, report->subject.id);
        if (strcmp(key, "loyalty_changed") == 0) return api->officer_detail(api->engine, report->subject.id).loyalty;
        if (strcmp(key, "merit_gained") == 0)    return officer.merit;
        if (strcmp(key, "ability_changed") == 0)
        {
            u8 values[] = { officer.leadership, officer.war, officer.intelligence, officer.politics, officer.charm };
            return report->detail < ArrayCount(values) ? values[report->detail] : TEXT_NO_VALUE;
        }
        return TEXT_NO_VALUE;
    }
    if (report->subject.kind != ENGINE_SUBJECT_CITY) return TEXT_NO_VALUE;
    if (strcmp(key, "price_changed") == 0) return report->detail;   /* the new price */
    engine_city city = api->city_at(api->engine, report->subject.id);
    if (city.city_id == ENGINE_NO_CITY) return TEXT_NO_VALUE;
    if (strcmp(key, "patrolled") == 0 || strcmp(key, "order_lost") == 0 || strcmp(key, "order_dropped") == 0) return city.public_order;
    if (strcmp(key, "conscripted") == 0)                                  return city.troops;
    if (strcmp(key, "trained") == 0 || strcmp(key, "morale_lost") == 0)  return city.morale;
    if (strcmp(key, "gold_earned") == 0)                                  return city.gold;
    if (strcmp(key, "grain_bought") == 0)                                 return city.food;
    if (strcmp(key, "weapons_produced") == 0)
    {
        /* G2-029: the new stock of the weapon (the detail of the event). */
        engine_weapon_kind weapon = api->weapon_kind_at(api->engine, report->detail);
        engine_city_economy economy = api->city_economy(api->engine, (u8)report->subject.id);
        return weapon.id != ENGINE_NO_WEAPON && weapon.stock < CITY_EQUIPMENT_STOCK_COUNT ? (s64)economy.stocks[weapon.stock] : TEXT_NO_VALUE;
    }
    return TEXT_NO_VALUE;
}

/* The name of the subject of a report, or "". */
internal void
ui_report_subject(ui_state *ui, engine_api *api, const engine_event_report *report, char *out, u32 out_size)
{
    out[0] = 0;
    if (report->subject.kind == ENGINE_SUBJECT_CITY)         ui_city_label(ui, api, (u8)report->subject.id, out, out_size);
    else if (report->subject.kind == ENGINE_SUBJECT_OFFICER) ui_officer_text(ui, api, report->subject.id, out, out_size);
}

/* One line of words for a ledger event: a log line or a speech. */
internal void
ui_report_line(ui_state *ui, engine_api *api, u32 ledger_index, s64 value, u32 form, char *out, u32 out_size)
{
    engine_event_report report = api->event_report(api->engine, ledger_index);
    char subject[UI_NAME_SIZE * 2], speaker[UI_NAME_SIZE * 2], faction[UI_NAME_SIZE * 2];
    ui_report_subject(ui, api, &report, subject, sizeof(subject));
    if (report.kind == SIM_EVENT_TECHNIQUE_RESEARCHED)
    {
        /* G2-062: the line names the technique (開發投石的技巧研究完畢). */
        engine_technique_kind technique = api->technique_kind_at(api->engine, report.detail);
        snprintf(subject, sizeof(subject), "%s", ui_text_with_key(ui, "technique", technique.key));
    }
    speaker[0] = faction[0] = 0;
    if (report.speaker != ENGINE_NO_OFFICER) ui_officer_text(ui, api, report.speaker, speaker, sizeof(speaker));
    if ((report.kind == SIM_EVENT_WEAPONS_PRODUCED || report.kind == SIM_EVENT_WEAPONS_FREE) && form == TEXT_LINE_LOG)
    {
        /* G2-029: the log line names the weapon (新野之槍庫存數變為29644). */
        engine_weapon_kind weapon = api->weapon_kind_at(api->engine, report.detail);
        if (weapon.id != ENGINE_NO_WEAPON && weapon.key[0]) snprintf(speaker, sizeof(speaker), "%s", ui_text_with_key(ui, "weapon", weapon.key));
    }
    if (form == TEXT_LINE_LOG)
    {
        /* The lines that name a thing of the event through the speaker slot:
           the item, the post, the other officer or the ability. */
        if (report.kind == SIM_EVENT_ITEM_GIVEN)
        {
            engine_item item = api->item_at(api->engine, report.detail);
            if (item.key[0]) snprintf(speaker, sizeof(speaker), "%s", ui_text_with_key(ui, "item", item.key));
        }
        else if (report.kind == SIM_EVENT_POST_APPOINTED)
        {
            engine_post_kind post = api->post_kind_at(api->engine, report.detail);
            if (post.id != ENGINE_NO_ROW) snprintf(speaker, sizeof(speaker), "%s", ui_text_with_key(ui, "post", post.key));
        }
        else if (report.kind == SIM_EVENT_SWORN || report.kind == SIM_EVENT_MARRIED)
        {
            ui_officer_text(ui, api, (u16)report.detail, speaker, sizeof(speaker));
        }
        else if (report.kind == SIM_EVENT_ABILITY_CHANGED)
        {
            static const char *abilities[] = { "leadership", "war", "intelligence", "politics", "charm" };
            if (report.detail < ArrayCount(abilities)) snprintf(speaker, sizeof(speaker), "%s", ui_text_with_key(ui, "picker", abilities[report.detail]));
        }
    }
    if (report.faction != ENGINE_NO_FACTION) ui_force_text(ui, api, report.faction, faction, sizeof(faction));
    if (!subject[0]) memcpy(subject, faction, sizeof(subject));   /* an event of a faction names the faction */
    text_event_facts facts = {};
    facts.key         = report.key ? report.key : "none";
    facts.tone        = report.tone;
    facts.sign        = report.sign;
    facts.personality = report.personality;
    facts.subject     = subject;
    facts.speaker     = speaker;
    facts.faction     = faction;
    facts.delta       = report.delta;
    facts.value       = value;
    text_write_line(&ui->report.provider, ui->text, &facts, form, out, out_size);
    /* G3-010: an event without a name (the ending of a faction) starts with a bare full-width colon: drop it. */
    if (out_size > 3 && (u8)out[0] == 0xEF && (u8)out[1] == 0xBC && (u8)out[2] == 0x9A) memmove(out, out + 3, strlen(out + 3) + 1);
}

internal void
ui_report_push(ui_state *ui, u32 line, s64 value)
{
    u32 place = (ui->report.line_first + ui->report.line_count) % UI_LOG_CAPACITY;
    if (ui->report.line_count == UI_LOG_CAPACITY) ui->report.line_first = (ui->report.line_first + 1) % UI_LOG_CAPACITY;
    else ui->report.line_count++;
    ui->report.lines[place]  = line;
    ui->report.values[place] = value;
}

/* The feedback of one event of a command of the player: the first event
   of a new command opens the bubble, and each event with a sign rises as
   a value. */
internal void
ui_report_feedback(ui_state *ui, engine_api *api, const engine_event_report *report, s64 value)
{
    u8 city = ENGINE_NO_CITY;
    if (report->subject.kind == ENGINE_SUBJECT_CITY) city = (u8)report->subject.id;
    else if (report->subject.kind == ENGINE_SUBJECT_OFFICER)
    {
        /* G3-022: an officer answers over its city (the captive after 登庸 or 處決). */
        engine_officer officer = api->officer_by_id(api->engine, report->subject.id);
        if (officer.base_location < api->city_count(api->engine)) city = (u8)officer.base_location;
    }
    if (city == ENGINE_NO_CITY) return;
    u32 open_cause = ui->report.bubble_event == ENGINE_NO_EVENT ? 0 :
                     api->event_report(api->engine, ui->report.bubble_event).cause;
    if (ui->report.bubble_seconds <= 0.0f || open_cause != report->cause)
    {
        ui->report.bubble_event   = report->ledger_index;
        ui->report.bubble_city    = city;
        ui->report.bubble_seconds = UI_BUBBLE_SECONDS;
        ui->report.bubble_value   = value;
        ui->report.float_count    = 0;
        ui->report.feedback_count++;
    }
    if (report->sign == ENGINE_SIGN_NEUTRAL || !report->delta || ui->report.float_count >= UI_FLOAT_CAPACITY) return;
    u32 place = ui->report.float_count++;
    ui->report.floats[place].city         = (u8)report->subject.id;
    ui->report.floats[place].delta        = report->delta;
    ui->report.floats[place].sign         = report->sign;
    ui->report.floats[place].seconds      = UI_FLOAT_SECONDS;
    ui->report.floats[place].resource     = report->resource;
    ui->report.floats[place].resource_key = report->resource_key;
}

/* Each frame: reads the new ledger events into the log, opens the
   feedback of a command of the player, and runs the timers. */
internal void
ui_report_update(ui_state *ui, engine_api *api, f32 dt_seconds)
{
    u32 count = api->ledger_count(api->engine);
    if (ui->report.ledger_mark > count)   /* a load replaced the ledger */
    {
        ui->report.ledger_mark = count;   /* G3-023: the log is empty after a load, as in the original */
        ui->report.line_count  = 0;
        ui->report.line_first  = 0;
        ui->report.has_heading = false;
        ui->report.bubble_seconds = 0.0f;
        ui->report.float_count = 0;
    }
    u8 player = ui_report_player(api);
    for (u32 index = ui->report.ledger_mark; index < count; ++index)
    {
        sim_event event = api->ledger_at(api->engine, index);
        if (ui_feel_bookkeeping(event.kind)) continue;
        engine_event_report report = api->event_report(api->engine, index);
        if (!report.log) continue;   /* a side effect of a command: no line and no value */
        /* G3-010, G3-017: the log lists the numbers of the city commands and the bandits of the
           player only. The events of other forces that change a value stay out. */
        if (player != ENGINE_NO_FACTION && report.faction != player && report.faction != ENGINE_NO_FACTION &&
            (report.resource != SIM_RESOURCE_NONE || event.kind == SIM_EVENT_BANDIT_SITE)) continue;
        if (!ui->report.has_heading || report.turn != ui->report.heading_turn)
        {
            s64 packed = (s64)report.date.year * 10000 + report.date.month * 100 + report.date.day;
            ui_report_push(ui, UI_LOG_HEADING | (report.turn & ~UI_LOG_HEADING), packed);
            ui->report.has_heading  = true;
            ui->report.heading_turn = report.turn;
        }
        s64 value = ui_report_value(api, &report);
        ui_report_push(ui, index, value);
        /* G3-016: no bubble during the show of the AI round; the turn start reports the threats. */
        if (report.cause && player != ENGINE_NO_FACTION && report.faction == player && !ui->round.playing) ui_report_feedback(ui, api, &report, value);
    }
    ui->report.ledger_mark = count;
    if (ui->report.bubble_seconds > 0.0f) ui->report.bubble_seconds -= dt_seconds;
    u32 kept = 0;
    for (u32 index = 0; index < ui->report.float_count; ++index)
    {
        ui->report.floats[index].seconds -= dt_seconds;
        if (ui->report.floats[index].seconds > 0.0f) ui->report.floats[kept++] = ui->report.floats[index];
    }
    ui->report.float_count = kept;
}

internal ImVec4
ui_report_colour(u8 sign)
{
    if (sign == ENGINE_SIGN_GOOD) return ImVec4(0.96f, 0.86f, 0.42f, 1.0f);
    if (sign == ENGINE_SIGN_BAD)  return ImVec4(0.96f, 0.48f, 0.42f, 1.0f);
    return ImVec4(0.92f, 0.92f, 0.92f, 1.0f);
}

/* One line of the log: a heading or the words of an event. */
internal void
ui_draw_log_line(ui_state *ui, engine_api *api, u32 place)
{
    u32 line = ui->report.lines[place];
    if (line & UI_LOG_HEADING)
    {
        s64 packed = ui->report.values[place];
        u32 year = (u32)(packed / 10000), month = (u32)(packed / 100 % 100), day = (u32)(packed % 100);
        const char *part = day >= 21 ? "msg.late" : day >= 11 ? "msg.middle" : "msg.early";
        char heading[64];
        snprintf(heading, sizeof(heading), text_get(ui->text, "msg.heading"), year, month, text_get(ui->text, part));
        ImGui::TextColored(ImVec4(0.62f, 0.82f, 0.96f, 1.0f), "%s", heading);
        return;
    }
    char text[UI_NAME_SIZE * 6];
    ui_report_line(ui, api, line, ui->report.values[place], TEXT_LINE_LOG, text, sizeof(text));
    engine_event_report report = api->event_report(api->engine, line);
    ImGui::PushStyleColor(ImGuiCol_Text, ui_report_colour(report.sign));
    ImGui::Bullet();
    ImGui::SameLine();
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
    /* The facts behind the words: the key of the kind and its tone. A
       template author sees which kind needs a template. */
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s  %s  %+d", report.key ? report.key : "-", report.tone, report.delta);
}

/* main-map.md, "Bottom-left panel": the message log. The filter shows all
   events or only those of the player (the player keeps the events of the
   other factions here, spec DECISIONS.md, 2026-10-20). */
internal void
ui_draw_log(ui_state *ui, engine_api *api)
{
    if (!ui->report.show_log) return;
    ImVec2 display = ImGui::GetIO().DisplaySize;
    f32 height = display.y * UI_LOG_HEIGHT_SHARE;
    ImGui::SetNextWindowPos(ImVec2(UI_LOG_MARGIN, display.y - height - UI_LOG_MARGIN), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(UI_LOG_WIDTH, height), ImGuiCond_Appearing);
    char title[64];
    snprintf(title, sizeof(title), "%s###Log", text_get(ui->text, "log.title"));
    if (ImGui::Begin(title, &ui->report.show_log, ImGuiWindowFlags_NoCollapse))
    {
        if (ImGui::RadioButton(text_get(ui->text, "log.filter.all"), !ui->report.mine_only)) ui->report.mine_only = false;
        ImGui::SameLine();
        if (ImGui::RadioButton(text_get(ui->text, "log.filter.mine"), ui->report.mine_only)) ui->report.mine_only = true;
        ImGui::Separator();
        ImGui::BeginChild("###LogLines");
        u8 player = ui_report_player(api);
        for (u32 offset = 0; offset < ui->report.line_count; ++offset)
        {
            u32 place = (ui->report.line_first + offset) % UI_LOG_CAPACITY;
            u32 line = ui->report.lines[place];
            if (ui->report.mine_only && !(line & UI_LOG_HEADING) && api->event_report(api->engine, line).faction != player) continue;
            ImGui::PushID((int)offset);
            ui_draw_log_line(ui, api, place);
            ImGui::PopID();
        }
        /* G2-019: a new line scrolls the log to the end, as the original shows the newest line at the bottom. */
        if (ui->report.lines_seen != ui->report.line_count || ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) ImGui::SetScrollHereY(1.0f);
        ui->report.lines_seen = ui->report.line_count;
        ImGui::EndChild();
    }
    ImGui::End();
}

/* The screen point over a city, or the centre of the upper screen when the
   scene does not draw (the headless ui tests). */
internal ImVec2
ui_report_anchor(ui_state *ui, engine_api *api, u8 city_id)
{
    ImVec2 display = ImGui::GetIO().DisplaySize;
    ImVec2 point(display.x * 0.5f, display.y * 0.35f);
    if (!ui->scene.enabled || !ui->scene.initialized || display.x <= 0.0f || display.y <= 0.0f) return point;
    engine_city_slots site = api->city_slots(api->engine, city_id);
    if (site.city_id == ENGINE_NO_CITY) return point;
    m4 view_projection = camera_view_projection(&ui->scene.cam, display.x, display.y);
    f32 x = 0.0f, y = 0.0f;
    if (ui_scene_screen(api, view_projection, hex_make(site.q, site.r), UI_LABEL_LIFT, &x, &y)) point = ImVec2(x, y);
    return point;
}

/* main-map.md, "Speech bubble" and "Floating value": the bubble does not
   block input and closes by itself. The values rise and fade. */
internal void
ui_draw_feedback(ui_state *ui, engine_api *api)
{
    if (ui->report.bubble_seconds > 0.0f && ui->report.bubble_event != ENGINE_NO_EVENT)
    {
        ImVec2 anchor = ui_report_anchor(ui, api, ui->report.bubble_city);
        engine_event_report report = api->event_report(api->engine, ui->report.bubble_event);
        char speaker[UI_NAME_SIZE * 2], line[UI_NAME_SIZE * 6];
        speaker[0] = 0;
        if (report.speaker != ENGINE_NO_OFFICER) ui_officer_text(ui, api, report.speaker, speaker, sizeof(speaker));
        ui_report_line(ui, api, ui->report.bubble_event, ui->report.bubble_value, TEXT_LINE_SPEECH, line, sizeof(line));
        ImGui::SetNextWindowPos(ImVec2(anchor.x, anchor.y - 8.0f), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
        ImGui::SetNextWindowSize(ImVec2(UI_BUBBLE_WIDTH, 0.0f), ImGuiCond_Always);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize;
        if (ImGui::Begin("###Speech", 0, flags))
        {
            if (report.speaker != ENGINE_NO_OFFICER) ui_draw_officer_portrait(ui, api, report.speaker);
            ImGui::TextColored(ImVec4(0.62f, 0.82f, 0.96f, 1.0f), "%s", speaker);
            ImGui::TextWrapped("%s", line);
        }
        ImGui::End();
    }
    ImDrawList *draw = ImGui::GetForegroundDrawList();
    for (u32 index = 0; index < ui->report.float_count; ++index)
    {
        ImVec2 anchor = ui_report_anchor(ui, api, ui->report.floats[index].city);
        f32 progress = 1.0f - ui->report.floats[index].seconds / UI_FLOAT_SECONDS;
        /* The field of the value comes first (資金收入 +2047), so that the
           player knows what the number is. */
        const char *field = "";
        char id[48];
        if (ui->report.floats[index].resource != SIM_RESOURCE_NONE && ui->report.floats[index].resource_key)
        {
            snprintf(id, sizeof(id), "resource.%s", ui->report.floats[index].resource_key);
            field = text_get(ui->text, id);
        }
        char value[96];
        if (field[0]) snprintf(value, sizeof(value), "%s %+d", field, ui->report.floats[index].delta);
        else          snprintf(value, sizeof(value), "%+d", ui->report.floats[index].delta);
        ImVec4 colour = ui_report_colour(ui->report.floats[index].sign);
        colour.w = 1.0f - progress * 0.6f;
        f32 rise = progress * UI_FLOAT_RISE + 16.0f * (f32)index;
        ImVec2 at(anchor.x + 24.0f, anchor.y - 30.0f - rise);
        f32 size = ImGui::GetFontSize() * 1.4f;
        /* G2-018: an arrow before the value, down and red for a loss, up for a gain. */
        u8 sign = ui->report.floats[index].sign;
        if (sign == ENGINE_SIGN_GOOD || sign == ENGINE_SIGN_BAD)
        {
            f32 half = size * 0.22f, mid = at.y + size * 0.5f;
            ImVec2 tip(at.x - half - 4.0f, sign == ENGINE_SIGN_BAD ? mid + half : mid - half);
            ImVec2 left(at.x - 2.0f * half - 4.0f, sign == ENGINE_SIGN_BAD ? mid - half : mid + half);
            ImVec2 right(at.x - 4.0f, left.y);
            draw->AddTriangleFilled(tip, left, right, ImGui::ColorConvertFloat4ToU32(colour));
        }
        draw->AddText(ImGui::GetFont(), size, at, ImGui::ColorConvertFloat4ToU32(colour), value);
    }
}
