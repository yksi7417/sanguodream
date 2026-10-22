/* The HUD of the game mode (main-map.md, "Status area" and "List panel"):
   at the top left the season, the date, the ruler, 行動力 and 技巧, three
   buttons (中地圖, 情報, settings) and the list of the bases, officers and
   units of the player. Items that can act come first; the others are grey.
   A click on a row moves the camera there and highlights the row; a right
   click opens its dialog. This file also draws the map menu and the
   system menu, and turns the clicks on the scene into dialogs. Included by
   ui.cpp after ui_minimap.cpp; it calls engine_api only. */

#define UI_HUD_MARGIN        8.0f
#define UI_HUD_WIDTH         250.0f
#define UI_HUD_LIST_ROWS     13      /* the visible rows of the list (main-map.md) */
#define UI_HUD_LOW_POINTS    10      /* 行動力 below this shows in red (assumed; the capture shows red at 2) */
#define UI_HUD_ROW_CAPACITY  1024
#define UI_HUD_FIRST_BASE    42      /* engine_officer.current_position: cities 0..41, gates and ports 42..86 */
#define UI_HUD_FIRST_UNIT    87      /* engine_officer.current_position: 87 + the unit index */
#define UI_HUD_NO_POSITION   0xFFFF

struct ui_hud_row
{
    u8   kind;       /* 0 city, 1 gate or port, 2 officer, 3 unit */
    u32  id;         /* the city, the base row, the officer or the unit */
    bool ready;      /* it can still act in this turn */
};

/* True if an active officer of the force stands in the base and has not acted. */
internal bool
ui_hud_base_ready(engine_api *api, u8 force, u32 location)
{
    for (u32 index = 0; index < api->officer_count(api->engine); ++index)
    {
        engine_officer officer = api->officer_at(api->engine, index);
        if (officer.force_id == force && officer.status <= 3 && officer.current_position == location && !officer.acted) return true;
    }
    return false;
}

/* The rows of one tab: the ready items first, then the others. */
internal u32
ui_hud_rows(engine_api *api, u8 force, int tab, ui_hud_row *rows, u32 capacity)
{
    void *e = api->engine;
    u32 count = 0;
    for (u32 pass = 0; pass < 2; ++pass)
    {
        bool want_ready = pass == 0;
        if (tab == UI_HUD_TAB_BASES)
        {
            for (u32 index = 0; index < api->city_count(e) && count < capacity; ++index)
            {
                engine_city city = api->city_at(e, index);
                if (city.owner_force_id != force) continue;
                bool ready = ui_hud_base_ready(api, force, city.city_id);
                if (ready == want_ready) rows[count++] = { 0, city.city_id, ready };
            }
            for (u32 index = 0; index < api->base_count(e) && count < capacity; ++index)
            {
                engine_base base = api->base_at(e, index);
                if (base.id == ENGINE_NO_ROW || base.force != force) continue;
                bool ready = ui_hud_base_ready(api, force, base.id);
                if (ready == want_ready) rows[count++] = { 1, index, ready };
            }
        }
        else if (tab == UI_HUD_TAB_OFFICERS)
        {
            for (u32 index = 0; index < api->officer_count(e) && count < capacity; ++index)
            {
                engine_officer officer = api->officer_at(e, index);
                if (officer.force_id != force || officer.status > 3) continue;
                bool ready = !officer.acted && officer.current_position < UI_HUD_FIRST_UNIT;
                if (ready == want_ready) rows[count++] = { 2, index, ready };
            }
        }
        else
        {
            for (u32 id = 0; id < api->unit_count(e) && count < capacity; ++id)
            {
                engine_unit unit = api->unit_at(e, id);
                if (unit.id == ENGINE_NO_UNIT || unit.force_id != force) continue;
                bool ready = !unit.acted;
                if (ready == want_ready) rows[count++] = { 3, id, ready };
            }
        }
    }
    return count;
}

/* The label of a row, and true if it is the selected item. */
internal bool
ui_hud_row_label(ui_state *ui, engine_api *api, ui_hud_row *row, char *out, u32 out_size)
{
    char name[UI_NAME_SIZE * 4];
    bool selected = false;
    switch (row->kind)
    {
        case 0:
            ui_city_label(ui, api, (u8)row->id, name, sizeof(name));
            selected = ui->city.selected == row->id;
            break;
        case 1:
        {
            engine_base base = api->base_at(api->engine, row->id);
            snprintf(name, sizeof(name), "%s", ui_text_with_key(ui, "base", base.key));
        } break;
        case 2:
        {
            engine_officer officer = api->officer_at(api->engine, row->id);
            ui_officer_name(ui, &officer, name, sizeof(name));
        } break;
        default:
        {
            engine_unit unit = api->unit_at(api->engine, row->id);
            ui_unit_label(ui, api, &unit, name, sizeof(name));
            selected = ui->units.selected == row->id;
        } break;
    }
    snprintf(out, out_size, "%s%s###hud_row%u_%u", name, row->kind == 3 && row->ready ? "  \xE2\x97\x8F" : "", row->kind, row->id);
    return selected;
}

/* M22, screen-flow.md, rule 3: a selection or an open menu hides the
   list panel, the log and the mini map. The status area stays. */
internal bool
ui_hud_panels_hidden(ui_state *ui)
{
    return ui->context.kind != UI_CONTEXT_NONE || ui->units.selected != ENGINE_NO_UNIT || ui->round.confirm ||
           ui->info.screen != 0 || ui->show_political || ui->placement.active;
}

/* M22: a left click on a unit row moves the camera to the unit. It
   selects nothing and opens no window (clone-gap-report.md, section 8). */
internal void
ui_hud_look_at_unit(ui_state *ui, engine_api *api, u32 unit_id)
{
    engine_unit unit = api->unit_at(api->engine, unit_id);
    if (unit.id != ENGINE_NO_UNIT) ui_frame_hex(ui, api, hex_make(unit.q, unit.r));
}

/* A click on a row: the camera goes to the item. A right click also opens its dialog. */
internal void
ui_hud_row_click(ui_state *ui, engine_api *api, ui_hud_row *row, bool open)
{
    ImGuiIO &io = ImGui::GetIO();
    switch (row->kind)
    {
        case 0:
            ui_frame_city(ui, (u8)row->id);
            if (open) ui_context_open(ui, api, UI_CONTEXT_CITY, io.MousePos.x, io.MousePos.y);
            break;
        case 1:
        {
            engine_base base = api->base_at(api->engine, row->id);
            ui_frame_hex(ui, api, hex_make(base.q, base.r));
        } break;
        case 2:
        {
            engine_officer officer = api->officer_at(api->engine, row->id);
            if (officer.current_position < UI_HUD_FIRST_BASE)
            {
                ui_frame_city(ui, (u8)officer.current_position);
                if (open) ui_context_open(ui, api, UI_CONTEXT_CITY, io.MousePos.x, io.MousePos.y);
            }
            else if (officer.current_position >= UI_HUD_FIRST_UNIT && officer.current_position != UI_HUD_NO_POSITION)
            {
                u32 unit_id = officer.current_position - UI_HUD_FIRST_UNIT;
                if (!open) { ui_hud_look_at_unit(ui, api, unit_id); break; }
                ui_frame_unit(ui, unit_id);
                ui_context_open(ui, api, UI_CONTEXT_UNIT, io.MousePos.x, io.MousePos.y);
            }
        } break;
        default:
            if (!open) { ui_hud_look_at_unit(ui, api, row->id); break; }
            ui_frame_unit(ui, row->id);
            ui_context_open(ui, api, UI_CONTEXT_UNIT, io.MousePos.x, io.MousePos.y);
            break;
    }
}

/* The list panel: three tabs, a header row and the rows. */
internal void
ui_draw_hud_list(ui_state *ui, engine_api *api, u8 force)
{
    const char *tab_ids[UI_HUD_TAB_COUNT] = { "hud.tab_bases", "hud.tab_officers", "hud.tab_units" };
    const char *tab_keys[UI_HUD_TAB_COUNT] = { "hud_bases", "hud_officers", "hud_units" };
    char label[128];
    if (!ImGui::BeginTabBar("###hud_tabs")) return;
    for (int tab = 0; tab < UI_HUD_TAB_COUNT; ++tab)
    {
        ImGui::SetNextItemWidth(UI_TAB_PITCH - ImGui::GetStyle().ItemInnerSpacing.x);   /* M22: the tab pitch of the original */
        if (!ImGui::BeginTabItem(ui_label(label, sizeof(label), ui, tab_ids[tab], tab_keys[tab]))) continue;
        ui->hud_tab = tab;
        char header[UI_NAME_SIZE * 2];
        /* G2-008: the header is an own base: the selected city when the player owns it, else the first own city. */
        u8 shown = ui->city.selected;
        if (shown == ENGINE_NO_CITY || api->city_at(api->engine, shown).owner_force_id != force)
        {
            for (u32 index = 0; index < api->city_count(api->engine); ++index)
            {
                engine_city city = api->city_at(api->engine, index);
                if (city.owner_force_id == force) { shown = city.city_id; break; }
            }
        }
        if (tab == UI_HUD_TAB_OFFICERS) ui_force_text(ui, api, force, header, sizeof(header));
        else                            ui_city_label(ui, api, shown, header, sizeof(header));
        ImGui::TextUnformatted(header);
        ImGui::Separator();
        f32 height = UI_LIST_PITCH * (f32)UI_HUD_LIST_ROWS;
        if (ImGui::BeginChild("###hud_rows", ImVec2(0.0f, height)))
        {
            local_persist ui_hud_row rows[UI_HUD_ROW_CAPACITY];
            u32 count = ui_hud_rows(api, force, tab, rows, UI_HUD_ROW_CAPACITY);
            ImVec2 row_size(0.0f, UI_LIST_PITCH - ImGui::GetStyle().ItemSpacing.y);   /* M22: the row pitch of the original */
            for (u32 index = 0; index < count; ++index)
            {
                char row_label[UI_NAME_SIZE * 4 + 32];
                bool selected = ui_hud_row_label(ui, api, rows + index, row_label, sizeof(row_label));
                if (!rows[index].ready) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                if (ImGui::Selectable(row_label, selected, 0, row_size)) ui_hud_row_click(ui, api, rows + index, false);
                if (ImGui::IsItemHovered() && ui_right_clicked()) ui_hud_row_click(ui, api, rows + index, true);   /* on the release, as the scene */
                if (!rows[index].ready) ImGui::PopStyleColor();
            }
            if (!count) ImGui::TextDisabled("-");
        }
        ImGui::EndChild();
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}

/* A button of the HUD that opens a dialog under it. */
internal void
ui_hud_button(ui_state *ui, engine_api *api, const char *text_id, const char *key, u8 kind)
{
    char label[96];
    if (ImGui::Button(ui_label(label, sizeof(label), ui, text_id, key)))
    {
        if (ui->context.kind == kind) ui_context_close(ui);
        else                          ui_context_open(ui, api, kind, ImGui::GetItemRectMin().x - UI_CONTEXT_OFFSET, ImGui::GetItemRectMax().y);
    }
}

/* The status area and the list (main-map.md: 466 x 131 and 164 x 358 at the top left). */
internal void
ui_draw_hud(ui_state *ui, engine_api *api)
{
    void *e = api->engine;
    ImGui::SetNextWindowPos(ImVec2(UI_HUD_MARGIN, UI_HUD_MARGIN), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(UI_HUD_WIDTH, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing;
    if (ImGui::Begin("###HUD", 0, flags))
    {
        engine_date date = api->date(e);
        u8 force = ui_round_hud_force(ui, api);   /* M20: the AI faction of the show, else the faction that acts */
        bool ai_turn = ui->round.playing && force != api->whose_turn(e);
        const char *seasons[4] = { "season.spring", "season.summer", "season.autumn", "season.winter" };
        char when[64], ruler[UI_NAME_SIZE * 2] = "-";
        snprintf(when, sizeof(when), text_get(ui->text, "hud.date"), date.year, date.month, date.day);
        if (force != ENGINE_NO_FACTION) ui_force_text(ui, api, force, ruler, sizeof(ruler));
        ImGui::Text("%s  %s  %s", text_get(ui->text, seasons[date.season < 4 ? date.season : 0]), when, ruler);
        u32 points = force != ENGINE_NO_FACTION ? api->actions_left(e, force) : 0;
        u32 technique = force != ENGINE_NO_FACTION ? api->technique_points(e, force) : 0;
        if (!ai_turn)   /* M20: an AI faction shows its 技巧 and no 行動力 */
        {
            ImGui::TextUnformatted(text_get(ui->text, "topbar.points"));
            ImGui::SameLine();
            if (points < UI_HUD_LOW_POINTS) ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.3f, 1.0f), "%u", points);
            else                            ImGui::Text("%u", points);
            ImGui::SameLine(0.0f, 16.0f);
        }
        ImGui::Text("%s %u", text_get(ui->text, "hud.technique"), technique);
        ui_hud_button(ui, api, "hud.full_map", "hud_map", UI_CONTEXT_FULL_MAP);
        ImGui::SameLine();
        ui_hud_button(ui, api, "hud.info", "hud_info", UI_CONTEXT_INFO);
        ImGui::SameLine();
        ui_hud_button(ui, api, "hud.system", "hud_system", UI_CONTEXT_SYSTEM_MENU);
        ImGui::SameLine();
        ui_hud_button(ui, api, "hud.help", "hud_help", UI_CONTEXT_HELP);   /* M22: the fourth icon button "?" */
        /* M17, M20, M22: the list hides under the messages, the show, a selection and a menu */
        if (force != ENGINE_NO_FACTION && !ui_feel_messages_open(ui) && !ui->round.playing && !ui_hud_panels_hidden(ui)) ui_draw_hud_list(ui, api, force);
    }
    ImGui::End();
}

/* One item of a menu: true when the player picks it. */
internal bool
ui_menu_item(ui_state *ui, const char *text_id, const char *key, bool enabled)
{
    char label[96];
    if (!enabled) ImGui::BeginDisabled();
    ImVec2 size(150.0f, UI_MENU_PITCH - ImGui::GetStyle().ItemSpacing.y);   /* M22: the menu pitch of the original */
    bool picked = ImGui::Selectable(ui_label(label, sizeof(label), ui, text_id, key), false, 0, size);
    if (!enabled) ImGui::EndDisabled();
    return picked;
}

internal bool
ui_begin_menu_window(ui_state *ui, const char *name)
{
    ui_context_place(ui, 0);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoMove;
    bool open = ImGui::Begin(name, 0, flags);
    if (open) ui_context_track(ui);
    return open;
}

/* True if a unit of the faction that acts has not acted in this turn. A
   unit with a long order acts at 進行 by itself. */
internal bool
ui_hud_units_waiting(engine_api *api)
{
    u8 force = api->whose_turn(api->engine);
    for (u32 id = 0; id < api->unit_count(api->engine); ++id)
    {
        engine_unit unit = api->unit_at(api->engine, id);
        if (unit.id != ENGINE_NO_UNIT && unit.force_id == force && !unit.acted && !unit.order) return true;
    }
    return false;
}

/* True if a unit of the faction that acts has a long order: 委任部隊行動
   is white (main-map.md, "Map context menu"). */
internal bool
ui_hud_units_ordered(engine_api *api)
{
    u8 force = api->whose_turn(api->engine);
    for (u32 id = 0; id < api->unit_count(api->engine); ++id)
    {
        engine_unit unit = api->unit_at(api->engine, id);
        /* G2-033: white when a unit of the player has not acted, or has a long order to run. */
        if (unit.id != ENGINE_NO_UNIT && unit.force_id == force && (unit.order || !unit.acted)) return true;
    }
    return false;
}

/* The map menu (main-map.md, "Map context menu"): a right click on the
   empty map. M22: the five items of the original. 進行 asks first when
   units of the player have not acted (clone-gap-report.md, section 2). */
internal void
ui_draw_map_menu(ui_state *ui, engine_api *api)
{
    if (ui_begin_menu_window(ui, "###MapMenu"))
    {
        f32 x = ui->context.x, y = ui->context.y;
        if (ui_menu_item(ui, "topbar.proceed", "proceed", true))
        {
            ui_context_close(ui);
            if (ui_hud_units_waiting(api)) ui->round.confirm = true;
            else                           ui_round_proceed(ui, api);   /* M20 */
        }
        if (ui_menu_item(ui, "hud.delegate", "delegate", ui_hud_units_ordered(api)))
        {
            ui_context_close(ui);
            engine_command_result result = api->command_delegate(api->engine);   /* the long orders run now (manual p.24) */
            ui_growth_result(ui, "hud.delegate", result);
        }
        if (ui_menu_item(ui, "topbar.info", "open_info", true))          ui_context_open(ui, api, UI_CONTEXT_INFO, x, y);
        if (ui_menu_item(ui, "topbar.save", "open_save", true))         { ui_open_saveload(ui, api, true);  ui_context_open(ui, api, UI_CONTEXT_SAVELOAD, x, y); }
        if (ui_menu_item(ui, "topbar.load", "open_load", true))          { ui_open_saveload(ui, api, false); ui_context_open(ui, api, UI_CONTEXT_SAVELOAD, x, y); }
    }
    ImGui::End();
}

/* The system menu (main-map.md, "System menu"): the settings button of the
   HUD. M22: the six items of the original. The debug panels open with F12
   only. */
internal void
ui_draw_system_menu(ui_state *ui, engine_api *api)
{
    if (ui_begin_menu_window(ui, "###SystemMenu"))
    {
        f32 x = ui->context.x, y = ui->context.y;
        if (ui_menu_item(ui, "topbar.save", "open_save", true))          { ui_open_saveload(ui, api, true);  ui_context_open(ui, api, UI_CONTEXT_SAVELOAD, x, y); }
        if (ui_menu_item(ui, "topbar.load", "open_load", true))          { ui_open_saveload(ui, api, false); ui_context_open(ui, api, UI_CONTEXT_SAVELOAD, x, y); }
        if (ui_menu_item(ui, "hud.settings", "settings", true))          ui_context_open(ui, api, UI_CONTEXT_SETTINGS, x, y);
        if (ui_menu_item(ui, "hud.edit", "edit", true))                  ui_context_open(ui, api, UI_CONTEXT_EDITOR, x, y);
        if (ui_menu_item(ui, "hud.to_title", "to_title", true))         { ui_context_close(ui); ui->show_title = true; }
        if (ui_menu_item(ui, "hud.quit", "quit", true))                  { ui_context_close(ui); ui->quit_requested = true; }
    }
    ImGui::End();
}

/* The clicks on the scene in the game mode (screen-flow.md and the choice
   of the user): a left click works as in the original. It selects or
   moves an own unit and opens its command menu (ui_unit_click), or it
   opens the City dialog of a city. A right click opens the dialog of the
   city or the unit under the cursor, or the map menu on the empty map. */
internal void
ui_hud_scene_clicks(ui_state *ui, engine_api *api)
{
    ImGuiIO &io = ImGui::GetIO();
    if (io.WantCaptureMouse || !ui->scene.has_hover) return;
    bool left  = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    bool right = ui_right_clicked() && !ui->context.consumed;
    if (!left && !right) return;
    hex_coord hex = ui->scene.hover;
    /* G2-042: the plate of a unit sits above its hex and covers other hexes.
       A click on a plate is a click on the hex of its unit. */
    u32 plated = ui_plate_unit_at(ui, io.MousePos.x, io.MousePos.y);
    if (plated != ENGINE_NO_UNIT)
    {
        engine_unit on_plate = api->unit_at(api->engine, plated);
        if (on_plate.id != ENGINE_NO_UNIT) hex = hex_make(on_plate.q, on_plate.r);
    }
    engine_hex data = api->hex_at(api->engine, hex.q, hex.r);
    f32 x = io.MousePos.x, y = io.MousePos.y;
    if (ui->placement.active)   /* M19: a 開發 waits for its slot (ui_placement.cpp) */
    {
        if (left) ui_placement_click(ui, api, hex);
        else      ui->placement.active = false;
        return;
    }
    if (left && ui_unit_click(ui, api, hex))
    {
        ui->scene.framed_unit = ui->units.selected;   /* the camera stays: the unit is under the cursor */
        if (ui->show_unit) ui_context_open(ui, api, UI_CONTEXT_UNIT, x, y);
        return;
    }
    if (data.city_id != ENGINE_NO_CITY)
    {
        ui->city.selected       = data.city_id;
        ui->scene.framed_city   = data.city_id;   /* the camera stays */
        ui_context_open(ui, api, UI_CONTEXT_CITY, x, y);
        return;
    }
    if (!right) return;
    if (data.unit_id != ENGINE_NO_UNIT)
    {
        /* M22, screen-flow.md: a right click on an own unit opens the popup
           詳細情報 and 所屬武將. The unit of another faction opens its panel. */
        bool own = api->unit_at(api->engine, data.unit_id).force_id == api->whose_turn(api->engine);
        ui_select_unit(ui, data.unit_id);
        ui->scene.framed_unit = data.unit_id;
        if (own) ui->show_unit = false;
        ui_context_open(ui, api, (u8)(own ? UI_CONTEXT_UNIT_POPUP : UI_CONTEXT_UNIT), x, y);
        return;
    }
    ui->context.hex = hex;
    ui_context_open(ui, api, UI_CONTEXT_MAP_MENU, x, y);
}

/* M22: the question before 進行 (clone-gap-report.md, section 2): the
   buttons 是 and 否 above one line, at the bottom centre, without a
   portrait. 否 and a right click go back to the map. */
internal void
ui_draw_proceed_confirm(ui_state *ui, engine_api *api)
{
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y - UI_MESSAGE_MARGIN), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoMove;
    if (ImGui::Begin("###ProceedConfirm", 0, flags))
    {
        char label[96], ruler[UI_NAME_SIZE * 2] = "-";
        u8 force = api->whose_turn(api->engine);
        if (force != ENGINE_NO_FACTION) ui_force_text(ui, api, force, ruler, sizeof(ruler));
        bool yes = ImGui::Button(ui_label(label, sizeof(label), ui, "confirm.yes", "confirm_yes"), ImVec2(80.0f, 0.0f));
        ImGui::SameLine();
        bool no = ImGui::Button(ui_label(label, sizeof(label), ui, "confirm.no", "confirm_no"), ImVec2(80.0f, 0.0f));
        ImGui::Text(text_get(ui->text, "confirm.proceed"), ruler);
        if (yes || no || ui_right_clicked()) ui->round.confirm = false;
        if (yes) ui_round_proceed(ui, api);
    }
    ImGui::End();
}

/* M22: the help window of the "?" button. The spec has no capture of the
   help screen of the original. This window lists the controls of hexdom. */
internal void
ui_draw_help(ui_state *ui)
{
    char title[96];
    ui_center_next_window();
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "help.title", "Help"), &ui->show_help, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ui_context_track(ui);
        ImGui::TextUnformatted(text_get(ui->text, "help.text"));
    }
    ImGui::End();
}

/* The dialogs that the HUD draws itself. */
internal void
ui_draw_hud_menus(ui_state *ui, engine_api *api)
{
    if (ui->context.kind == UI_CONTEXT_MAP_MENU)    ui_draw_map_menu(ui, api);
    if (ui->context.kind == UI_CONTEXT_SYSTEM_MENU) ui_draw_system_menu(ui, api);
    if (ui->show_help)                              ui_draw_help(ui);
    if (ui->round.confirm)                          ui_draw_proceed_confirm(ui, api);
}
