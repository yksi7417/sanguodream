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
        case UI_PICK_DUEL:
        {
            /* G3-008: the list of the officers with 成功率 opens; a row sends the command. */
            engine_command probe = {};
            probe.kind = ENGINE_COMMAND_DUEL;
            probe.unit = unit_id;
            probe.q = hex.q; probe.r = hex.r;
            result = {};
            result.refusal = api->can_issue(e, &probe).refusal;
            if (result.refusal == SIM_REFUSAL_NONE)
            {
                ui->combat.duel_pick = true;
                ui->combat.duel_hex  = hex;
                ui->combat.pick      = UI_PICK_NONE;
                return true;
            }
        } break;
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

/* G3-008 (ui/unit-commands.md; the run of 2026-10-23): after the target
   of 單挑, the window 武將 lists the officers of the unit with 成功率, 武力
   and 體力. The status line says 請選擇要挑起單挑的武將. A row sends one
   command_duel_officer. */
internal void
ui_draw_duel_pick(ui_state *ui, engine_api *api)
{
    void *e = api->engine;
    engine_unit unit = api->unit_at(e, ui->units.selected);
    if (unit.id == ENGINE_NO_UNIT) { ui->combat.duel_pick = false; return; }
    char title[128], label[160], name[UI_NAME_SIZE * 2];
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);
    ui_context_place(ui, 1);
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "duel.pick_title", "DuelPick"), &ui->combat.duel_pick, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ui_context_track(ui);
        ImGui::TextUnformatted(text_get(ui->text, "duel.pick_hint"));
        ImGui::Separator();
        ImGui::Text("%s   %s   %s   %s", text_get(ui->text, "duel.pick_officer"), text_get(ui->text, "duel.pick_chance"),
                    text_get(ui->text, "duel.force"), text_get(ui->text, "duel.stamina"));
        for (u32 slot = 0; slot < SIM_COMMAND_OFFICERS; ++slot)
        {
            if (unit.officers[slot] == ENGINE_NO_OFFICER) continue;
            engine_officer officer = api->officer_by_id(e, unit.officers[slot]);
            engine_officer_detail detail = api->officer_detail(e, unit.officers[slot]);
            engine_command probe = {};
            probe.kind = ENGINE_COMMAND_DUEL;
            probe.unit = unit.id;
            probe.row  = (u8)slot;
            probe.q = ui->combat.duel_hex.q; probe.r = ui->combat.duel_hex.r;
            engine_check check = api->can_issue(e, &probe);
            ui_officer_text(ui, api, unit.officers[slot], name, sizeof(name));
            snprintf(label, sizeof(label), "%s   %u%%   %u   %u###duel_officer_%u", name, check.quantity, officer.war, detail.stamina, slot);
            ImGui::BeginDisabled(check.refusal != SIM_REFUSAL_NONE);
            if (ImGui::Selectable(label, false))
            {
                engine_command_result result = api->command_duel_officer(e, unit.id, ui->combat.duel_hex.q, ui->combat.duel_hex.r, (u8)slot);
                ui_growth_result(ui, "unit_menu.duel", result);
                ui_units_note_result(ui, result);
                ui->combat.duel_pick = false;
                if (result.refusal == SIM_REFUSAL_NONE && ui_strike_of_pick(UI_PICK_DUEL) != UI_STRIKE_KIND_COUNT) ui_strike_push(ui, ui_strike_of_pick(UI_PICK_DUEL), ui->combat.duel_hex);
            }
            ImGui::EndDisabled();
        }
        if (ImGui::Button(ui_label(label, sizeof(label), ui, "unit_target.no", "duel_pick_back"), ImVec2(80.0f, 0.0f))) ui->combat.duel_pick = false;
    }
    ImGui::End();
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

/* U-002 (play report 2026-10-24): true when the engine accepts the
   building on a hex next to the unit. A 設置 needs the technique, the gold
   and a buildable hex, and can_issue checks all three. */
internal b32
ui_build_has_place(ui_state *ui, engine_api *api, engine_unit *unit, u32 row)
{
    hex_coord center = ui_unit_menu_hex(ui, unit);
    for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d)
    {
        if (ui_pick_allows(ui, api, UI_PICK_BUILD, (u8)row, hex_neighbor(center, d))) return true;
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

/* The tactics of the class that the unit fights with, the plots, or the
   buildings that the unit can place. A choice starts the pick of the
   target hex. */
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
            /* G2-054: the cost in 氣力 in brackets, as the column of the original.
               U-003: on water a land unit fights as its ship, so the list follows effective_type. */
            for (u32 row = 0; row < api->tactic_kind_count(e); ++row)
            {
                engine_tactic_kind tactic = api->tactic_kind_at(e, row);
                if (unit.effective_type == ENGINE_NO_UNIT_TYPE || !((tactic.unit_types >> unit.effective_type) & 1)) continue;
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
                ui_action_row(ui, row, "building", building.key, detail, ui_build_has_place(ui, api, &unit, row));
            }
        }
    }
    ImGui::End();
}
