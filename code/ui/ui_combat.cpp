/* The panels of M6 (combat; ui/unit-commands.md): the target pick of 攻擊,
   齊攻, 戰法, 計略, 設置, 修補 and 撤除 on the Hex map, the Actions list
   that chooses a tactic, a plot or a building, and the buildings and the
   fire on the map. The UI calls engine_api only. */

internal engine_command_kind
ui_pick_command_kind(u8 pick)
{
    switch (pick)
    {
        case UI_PICK_ATTACK:    return ENGINE_COMMAND_ATTACK;
        case UI_PICK_JOINT:     return ENGINE_COMMAND_JOINT_ATTACK;
        case UI_PICK_TACTIC:    return ENGINE_COMMAND_TACTIC;
        case UI_PICK_PLOT:      return ENGINE_COMMAND_PLOT;
        case UI_PICK_BUILD:     return ENGINE_COMMAND_BUILD;
        case UI_PICK_REPAIR:    return ENGINE_COMMAND_REPAIR;
        case UI_PICK_DISMANTLE: return ENGINE_COMMAND_DISMANTLE;
        case UI_PICK_DUEL:      return ENGINE_COMMAND_DUEL;
        default:                return ENGINE_COMMAND_KIND_COUNT;
    }
}

/* The text id of the menu item of a pick, for the result message. */
internal const char *
ui_pick_text_id(u8 pick)
{
    static const char *ids[UI_PICK_COUNT] = { "unit.menu", "unit_menu.attack", "unit_menu.joint", "unit_menu.tactic", "unit_menu.plot",
                                              "unit_menu.build", "unit_menu.repair", "unit_menu.dismantle", "unit_menu.duel" };
    return pick < UI_PICK_COUNT ? ids[pick] : "unit.menu";
}

internal void ui_strike_push(ui_state *ui, u8 kind, hex_coord hex);   /* ui_war.cpp */
internal b32  ui_walk_active(ui_state *ui);                           /* ui_war.cpp */
internal b32  ui_pick_allows(ui_state *ui, engine_api *api, u8 pick, u8 row, hex_coord hex);   /* below: the probe of a target */
internal u8   ui_strike_of_pick(u8 pick);

/* Starts a pick: the next click on the Hex map is the target. */
internal void
ui_start_pick(ui_state *ui, u8 pick, u8 row)
{
    ui->combat.pick    = pick;
    ui->combat.row     = row;
    ui->show_unit_menu = false;
    ui->show_actions   = false;
    snprintf(ui->units.message, sizeof(ui->units.message), "%s", text_get(ui->text, "unit.pick_hint"));
}

/* A click on the Hex map while a pick waits: one command on that hex.
   Returns false if no pick waits. */
internal b32
ui_combat_click(ui_state *ui, engine_api *api, hex_coord hex)
{
    u8 pick = ui->combat.pick;
    if (pick == UI_PICK_NONE) return false;
    u32 unit_id = ui->units.selected;
    void *e = api->engine;
    engine_command_result result = {};
    /* G2-045: the move of the pending menu first. The strike then waits for the walk. */
    if (!ui_commit_pending_move(ui, api)) { ui->combat.pick = UI_PICK_NONE; return true; }
    b32 walking = ui_walk_active(ui);
    switch (pick)
    {
        case UI_PICK_ATTACK:    result = api->command_attack(e, unit_id, hex.q, hex.r); break;
        case UI_PICK_JOINT:     result = api->command_joint_attack(e, unit_id, hex.q, hex.r); break;
        case UI_PICK_TACTIC:    result = api->command_tactic(e, unit_id, ui->combat.row, hex.q, hex.r); break;
        case UI_PICK_PLOT:      result = api->command_plot(e, unit_id, ui->combat.row, hex.q, hex.r); break;
        case UI_PICK_BUILD:     result = api->command_build(e, unit_id, ui->combat.row, hex.q, hex.r); break;
        case UI_PICK_REPAIR:    result = api->command_repair(e, unit_id, hex.q, hex.r); break;
        case UI_PICK_DISMANTLE: result = api->command_dismantle(e, unit_id, hex.q, hex.r); break;
        case UI_PICK_DUEL:      result = api->command_duel(e, unit_id, hex.q, hex.r); break;
        default: break;
    }
    ui_growth_result(ui, ui_pick_text_id(pick), result);
    ui_units_note_result(ui, result);
    ui->combat.pick = UI_PICK_NONE;
    /* The strike effect on the target hex, so that the player sees the action. */
    if (result.refusal == SIM_REFUSAL_NONE && ui_strike_of_pick(pick) != UI_STRIKE_KIND_COUNT)
    {
        if (walking)
        {
            ui->war.walk.has_strike  = true;
            ui->war.walk.strike_kind = ui_strike_of_pick(pick);
            ui->war.walk.strike_hex  = hex;
        }
        else
        {
            ui_strike_push(ui, ui_strike_of_pick(pick), hex);
        }
    }
    return true;
}

/* The hexes where the pick can hit now (red outlines), the buildings (a
   triangle in the colour of the faction; grey for a trap) and the fire
   (an orange dot with the turns left). */
internal void
ui_draw_map_combat(ui_state *ui, engine_api *api, ImDrawList *draw, f32 cell, hex_point origin)
{
    void *e = api->engine;
    for (u32 id = 0; id < api->building_count(e); ++id)
    {
        engine_building building = api->building_at(e, id);
        if (building.id == ENGINE_NO_BUILDING) continue;
        hex_point center = hex_to_pixel(hex_make(building.q, building.r), cell, origin);
        f32 half = cell * 0.6f;
        ImU32 color = building.force_id == ENGINE_NO_FACTION ? IM_COL32(150, 150, 150, 255) : ui_faction_color(building.force_id);
        if (!building.complete) color = (color & 0x00FFFFFFu) | 0x80000000u;
        draw->AddTriangleFilled(ImVec2(center.x, center.y - half), ImVec2(center.x + half, center.y + half * 0.8f),
                                ImVec2(center.x - half, center.y + half * 0.8f), color);
        draw->AddTriangle(ImVec2(center.x, center.y - half), ImVec2(center.x + half, center.y + half * 0.8f),
                          ImVec2(center.x - half, center.y + half * 0.8f), IM_COL32(20, 20, 20, 255));
    }
    engine_fire_view fire = api->fire_view(e);
    if (fire.turns)
    {
        for (u32 row = 0; row < fire.height; ++row)
        {
            for (u32 col = 0; col < fire.width; ++col)
            {
                u8 turns = fire.turns[row * fire.width + col];
                if (!turns) continue;
                hex_offset o;
                o.col = (s32)col;
                o.row = (s32)row;
                hex_point center = hex_to_pixel(hex_from_offset(o), cell, origin);
                draw->AddCircleFilled(ImVec2(center.x, center.y), cell * (0.25f + 0.08f * (f32)turns), IM_COL32(255, 120, 20, 200));
            }
        }
    }
    if (ui->combat.pick == UI_PICK_NONE || ui->units.selected == ENGINE_NO_UNIT) return;
    engine_unit unit = api->unit_at(e, ui->units.selected);
    if (unit.id == ENGINE_NO_UNIT) return;
    hex_coord center = ui_unit_menu_hex(ui, &unit);   /* G2-045: around the destination of a pending move */
    for (s32 dq = -3; dq <= 3; ++dq)
    {
        for (s32 dr = -3; dr <= 3; ++dr)
        {
            hex_coord hex = hex_make(center.q + dq, center.r + dr);
            if (hex_distance(hex, center) > 3) continue;
            if (!ui_pick_allows(ui, api, ui->combat.pick, ui->combat.row, hex)) continue;
            ui_draw_hexagon(draw, hex_to_pixel(hex, cell, origin), cell - 2.5f, IM_COL32(0, 0, 0, 0), IM_COL32(240, 60, 50, 255));
        }
    }
}

/* True when the pick of the selected unit can hit the hex (the probe of
   the command). The 2D map, the scene and the tactic column use it. */
internal b32
ui_pick_allows(ui_state *ui, engine_api *api, u8 pick, u8 row, hex_coord hex)
{
    if (pick == UI_PICK_NONE || ui->units.selected == ENGINE_NO_UNIT) return false;
    engine_command command = {};
    command.kind = ui_pick_command_kind(pick);
    command.unit = ui->units.selected;
    command.row  = row;
    command.q    = hex.q;
    command.r    = hex.r;
    return ui_unit_check(ui, api, &command).refusal == SIM_REFUSAL_NONE;   /* G2-045: from the hex of the menu */
}

/* True when a hex within 3 of the unit holds a target of the tactic
   (G2-054: a tactic without a target in its pattern is grey). */
internal b32
ui_tactic_has_target(ui_state *ui, engine_api *api, engine_unit *unit, u32 row)
{
    hex_coord center = ui_unit_menu_hex(ui, unit);
    for (s32 dq = -3; dq <= 3; ++dq)
    {
        for (s32 dr = -3; dr <= 3; ++dr)
        {
            hex_coord hex = hex_make(center.q + dq, center.r + dr);
            if (hex_distance(hex, center) > 3) continue;
            if (ui_pick_allows(ui, api, UI_PICK_TACTIC, (u8)row, hex)) return true;
        }
    }
    return false;
}

/* One row of the Actions list. A row that no target allows is grey. A
   choice closes the list and the unit menu, and starts the pick. */
internal void
ui_action_row(ui_state *ui, u32 row, const char *prefix, const char *key, const char *detail, b32 enabled)
{
    char label[160];
    snprintf(label, sizeof(label), "%s  %s###a%u", ui_text_with_key(ui, prefix, key), detail, row);
    ImGui::BeginDisabled(!enabled);
    if (ImGui::Selectable(label, false))
    {
        ui_start_pick(ui, ui->combat.list, (u8)row);
        ui->show_unit_menu = false;
    }
    ImGui::EndDisabled();
}

/* The tactics of the class of the unit, the plots, or the buildings that
   the unit can place. A choice starts the pick of the target hex. */
internal void
ui_draw_actions(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(280, 320), ImGuiCond_FirstUseEver);
    ui_context_place(ui, 1);   /* the game mode: the dialog opens next to its entity */
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "actions.title", "Actions"), &ui->show_actions))
    {
        ui_context_track(ui);
        void *e = api->engine;
        engine_unit unit = api->unit_at(e, ui->units.selected);
        if (unit.id == ENGINE_NO_UNIT)
        {
            ImGui::TextUnformatted(text_get(ui->text, "unit.none"));
            ImGui::End();
            return;
        }
        char detail[64];
        if (ui->combat.list == UI_PICK_TACTIC)
        {
            /* G2-054: the cost in 氣力 in brackets, as the column of the original. */
            for (u32 row = 0; row < api->tactic_kind_count(e); ++row)
            {
                engine_tactic_kind tactic = api->tactic_kind_at(e, row);
                if (!((tactic.unit_types >> unit.unit_type) & 1)) continue;
                snprintf(detail, sizeof(detail), "(%u)  %s %s", tactic.morale, text_get(ui->text, "actions.grade"), ui_grade_text(tactic.aptitude));
                ui_action_row(ui, row, "tactic", tactic.key, detail, ui_tactic_has_target(ui, api, &unit, row));
            }
        }
        else if (ui->combat.list == UI_PICK_PLOT)
        {
            for (u32 row = 0; row < api->plot_kind_count(e); ++row)
            {
                engine_plot_kind plot = api->plot_kind_at(e, row);
                snprintf(detail, sizeof(detail), "(%u)", plot.morale);
                ui_action_row(ui, row, "plot", plot.key, detail, true);
            }
        }
        else
        {
            for (u32 row = 0; row < api->building_kind_count(e); ++row)
            {
                engine_building_kind building = api->building_kind_at(e, row);
                if (building.natural) continue;
                snprintf(detail, sizeof(detail), "%s %u", text_get(ui->text, "command.gold"), building.gold);
                ui_action_row(ui, row, "building", building.key, detail, true);
            }
        }
    }
    ImGui::End();
}
