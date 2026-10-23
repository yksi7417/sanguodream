/* The dialogs on demand of the game mode (main-map.md, screen-flow.md, and
   the choice of the user): a click on a city, a unit or the empty map, or
   a button of the HUD, opens one dialog next to the cursor. The close
   button of the dialog, or a right click outside it, closes it. A dialog
   also closes when its work is done: a command of a City dialog, or the
   action of the unit. Only one dialog is open at a time. Included by
   ui.cpp before the panels; it calls engine_api only. */

#define UI_CONTEXT_OFFSET       14.0f    /* px from the cursor to the first window */
#define UI_CONTEXT_SECOND_X     330.0f   /* px from the first window to the second (a command dialog) */
#define UI_CONTEXT_CLICK_SLACK  16.0f    /* px of mouse travel: more is a camera drag, not a click */

internal void ui_select_unit(ui_state *ui, u32 unit_id);   /* ui_march.cpp */

/* A right click: the button went up, and the mouse did not drag. */
internal bool
ui_right_clicked(void)
{
    ImGuiIO &io = ImGui::GetIO();
    return ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
           io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Right] < UI_CONTEXT_CLICK_SLACK * UI_CONTEXT_CLICK_SLACK;
}

/* The show flag of the main window of a dialog; 0 for a menu that the HUD draws. */
internal bool *
ui_context_flag(ui_state *ui, u8 kind)
{
    switch (kind)
    {
        case UI_CONTEXT_CITY:      return &ui->show_city;
        case UI_CONTEXT_UNIT:      return &ui->show_unit;
        case UI_CONTEXT_HEX:       return &ui->scene.has_pick;
        case UI_CONTEXT_INFO:      return &ui->show_info;
        case UI_CONTEXT_FULL_MAP:  return ui->hud_mode ? &ui->show_political : &ui->show_map;   /* M21: 中地圖 in the game mode */
        case UI_CONTEXT_SAVELOAD:  return &ui->show_saveload;
        case UI_CONTEXT_SETTINGS:  return &ui->show_settings;
        case UI_CONTEXT_OPTIONS:   return &ui->show_options;
        case UI_CONTEXT_HELP:      return &ui->show_help;
        case UI_CONTEXT_EDITOR:    return &ui->show_editor;
        default:                   return 0;
    }
}

/* Closes the dialog and the windows that it opened. */
internal void
ui_context_close(ui_state *ui)
{
    switch (ui->context.kind)
    {
        case UI_CONTEXT_CITY:
            ui->show_city = ui->show_develop = ui->show_research = ui->show_merge = false;
            ui->show_sortie = ui->show_transport = ui->show_picker = false;
            ui->show_move_officers = ui->show_summon = ui->show_posts = ui->show_exile = ui->show_plot = ui->show_reward = false;
            for (u32 kind = 0; kind < UI_COMMAND_COUNT; ++kind) ui->show_command[kind] = false;
            break;
        case UI_CONTEXT_UNIT:
            ui->show_unit = ui->show_unit_menu = ui->show_actions = false;
            ui->combat.pick    = UI_PICK_NONE;
            ui->units.selected = ENGINE_NO_UNIT;   /* M22: the selection ends with its dialog */
            break;
        default:
        {
            bool *flag = ui_context_flag(ui, ui->context.kind);
            if (flag) *flag = false;
        } break;
    }
    ui->context.kind = UI_CONTEXT_NONE;
}

/* Opens a dialog at the point (the cursor or a button). A second open of
   the same kind moves it to the new point and keeps its windows. */
internal void
ui_context_open(ui_state *ui, engine_api *api, u8 kind, f32 x, f32 y)
{
    if (ui->context.kind != kind) ui_context_close(ui);
    ui->context.kind        = kind;
    ui->context.x           = x;
    ui->context.y           = y;
    ui->context.place_frame = ImGui::GetFrameCount();
    ui->context.ledger_mark = api->ledger_count(api->engine);
    ui->context.unit_acted  = api->unit_at(api->engine, ui->units.selected).acted;
    bool *flag = ui_context_flag(ui, kind);
    if (flag) *flag = true;
}

/* Before the Begin of a window of the dialog: the window goes next to the
   anchor when it appears, and in the frame after an open. slot 0 is the
   first window, slot 1 a second one beside it. The pivot keeps the window
   on the screen near the right and the bottom edges. */
internal void
ui_context_place(ui_state *ui, u32 slot)
{
    if (!ui->hud_mode || ui->context.kind == UI_CONTEXT_NONE) return;
    ImGuiIO &io = ImGui::GetIO();
    bool right  = ui->context.x > io.DisplaySize.x * 0.6f;
    bool bottom = ui->context.y > io.DisplaySize.y * 0.6f;
    f32 shift = UI_CONTEXT_OFFSET + (f32)slot * UI_CONTEXT_SECOND_X;
    ImVec2 at(ui->context.x + (right ? -shift : shift), ui->context.y + (bottom ? -UI_CONTEXT_OFFSET : UI_CONTEXT_OFFSET));
    ImGuiCond cond = ImGui::GetFrameCount() == ui->context.place_frame + 1 ? ImGuiCond_Always : ImGuiCond_Appearing;
    ImGui::SetNextWindowPos(at, cond, ImVec2(right ? 1.0f : 0.0f, bottom ? 1.0f : 0.0f));
}

/* As ui_context_place, for a window of a known size: the window stays on
   the screen (G2-064: the research window of a city at the left edge
   opened partly outside the screen). */
internal void
ui_context_place_sized(ui_state *ui, u32 slot, ImVec2 size)
{
    if (!ui->hud_mode || ui->context.kind == UI_CONTEXT_NONE) return;
    ImGuiIO &io = ImGui::GetIO();
    bool right  = ui->context.x > io.DisplaySize.x * 0.6f;
    bool bottom = ui->context.y > io.DisplaySize.y * 0.6f;
    f32 shift = UI_CONTEXT_OFFSET + (f32)slot * UI_CONTEXT_SECOND_X;
    ImVec2 top_left(ui->context.x + (right ? -shift - size.x : shift), ui->context.y + (bottom ? -UI_CONTEXT_OFFSET - size.y : UI_CONTEXT_OFFSET));
    if (top_left.x + size.x > io.DisplaySize.x) top_left.x = io.DisplaySize.x - size.x;
    if (top_left.y + size.y > io.DisplaySize.y) top_left.y = io.DisplaySize.y - size.y;
    if (top_left.x < 0.0f) top_left.x = 0.0f;
    if (top_left.y < 0.0f) top_left.y = 0.0f;
    ImGuiCond cond = ImGui::GetFrameCount() == ui->context.place_frame + 1 ? ImGuiCond_Always : ImGuiCond_Appearing;
    ImGui::SetNextWindowPos(top_left, cond);
}

/* Inside a window of the dialog: the mouse over it keeps the dialog open
   on a right click. */
internal void
ui_context_track(ui_state *ui)
{
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) ui->context.hovered = true;
}

/* M22, screen-flow.md, rule 1: a right click goes back one level. A
   command dialog goes back to the city menu. A target pick goes back to
   the unit menu, the menu to the selection, and the selection to the
   idle map. Other dialogs close. */
internal void
ui_context_back(ui_state *ui)
{
    if (ui->context.kind == UI_CONTEXT_CITY)
    {
        if (ui->show_picker) { ui->show_picker = false; return; }
        bool dialog = ui->show_develop || ui->show_research || ui->show_merge || ui->show_sortie || ui->show_transport ||
                      ui->show_move_officers || ui->show_summon || ui->show_posts || ui->show_exile || ui->show_plot || ui->show_reward;
        for (u32 kind = 0; kind < UI_COMMAND_COUNT; ++kind) dialog = dialog || ui->show_command[kind];
        if (dialog && ui->show_city)
        {
            ui->show_develop = ui->show_research = ui->show_merge = ui->show_sortie = ui->show_transport = false;
            ui->show_move_officers = ui->show_summon = ui->show_posts = ui->show_exile = ui->show_plot = ui->show_reward = false;
            for (u32 kind = 0; kind < UI_COMMAND_COUNT; ++kind) ui->show_command[kind] = false;
            return;
        }
    }
    if (ui->context.kind == UI_CONTEXT_UNIT)
    {
        if (ui->units.target_kind != UI_TARGET_NONE) { ui->units.target_kind = UI_TARGET_NONE; return; }   /* the target menu goes back to the selection */
        if (ui->combat.pick != UI_PICK_NONE || ui->show_actions)
        {
            ui->combat.pick    = UI_PICK_NONE;
            ui->show_actions   = false;
            ui->show_unit_menu = true;
            return;
        }
        if (ui->show_unit_menu) { ui->show_unit_menu = false; ui->units.has_pending_move = false; return; }   /* G2-045: the unit stays */
    }
    if (ui->context.kind == UI_CONTEXT_UNIT || ui->context.kind == UI_CONTEXT_UNIT_POPUP) ui_select_unit(ui, ENGINE_NO_UNIT);
    ui_context_close(ui);
}

/* After the panels of a frame: a right click goes back one level, and the
   end of the work of a dialog closes it. The scene then does not use the
   click. */
internal void
ui_context_update(ui_state *ui, engine_api *api)
{
    ui->context.consumed = false;
    if (ui->hud_mode && ui->context.kind != UI_CONTEXT_NONE)
    {
        bool *flag = ui_context_flag(ui, ui->context.kind);
        bool just_opened = ImGui::GetFrameCount() <= ui->context.place_frame;
        if (flag && !*flag)   /* the close button */
        {
            if (ui->context.kind == UI_CONTEXT_UNIT) ui->units.selected = ENGINE_NO_UNIT;
            ui->context.kind = UI_CONTEXT_NONE;
        }
        else if (!just_opened && ui_right_clicked())
        {
            ui_context_back(ui);
            ui->context.consumed = true;
        }
        else if (ui->context.kind == UI_CONTEXT_CITY && api->ledger_count(api->engine) != ui->context.ledger_mark)
        {
            ui_context_close(ui);   /* a command of the city ran */
        }
        else if (ui->context.kind == UI_CONTEXT_UNIT)
        {
            engine_unit unit = api->unit_at(api->engine, ui->units.selected);
            if (unit.id == ENGINE_NO_UNIT || (unit.acted && !ui->context.unit_acted)) ui_context_close(ui);   /* the unit acted */
        }
    }
    ui->context.hovered = false;
}
