/* Where things are (M11): the camera frames the city or the unit that a
   list picks, the scene draws a marker and a name over each city, the
   Hex info box tells what a picked hex is, the Units panel lists the
   units, and the City panel counts its facilities by kind and level as
   the 都市情報 screen does. Included by ui.cpp after ui_scene.cpp; it calls
   engine_api only. */

#define UI_FRAME_DISTANCE    24.0f   /* the camera distance after a frame */
#define UI_LABEL_LIFT        1.2f    /* world units above the top of the hex */
#define UI_FACILITY_COUNT_CAPACITY 32

/* ---- frame a city or a unit --------------------------------------------------------- */

/* A list picks a city: select it and frame it, also if it was selected. */
internal void
ui_frame_city(ui_state *ui, u8 city_id)
{
    ui->city.selected    = city_id;
    ui->scene.frame_city = true;
}

internal void
ui_frame_unit(ui_state *ui, u32 unit_id)
{
    ui_select_unit(ui, unit_id);
    ui->scene.frame_unit = unit_id != ENGINE_NO_UNIT;
}

/* The point of the scene above a hex, at the height of its terrain. */
internal v3
ui_hex_world(engine_api *api, hex_coord hex)
{
    engine_terrain_view view = api->terrain_view(api->engine);
    hex_offset o = hex_to_offset(hex);
    f32 y = 0.0f;
    if (o.col >= 0 && o.row >= 0 && (u32)o.col < view.width && (u32)o.row < view.height && view.level)
    {
        y = terrain_level_y(view.level[(u32)o.row * view.width + (u32)o.col]);
    }
    return hex_world_center(hex, y);
}

/* The camera and the Hex map panel go to the hex. The heading and the tilt stay. */
internal void
ui_frame_hex(ui_state *ui, engine_api *api, hex_coord hex)
{
    if (!ui->scene.initialized) ui_scene_reset_camera(ui, api);
    ui->scene.cam.target   = ui_hex_world(api, hex);
    ui->scene.cam.distance = UI_FRAME_DISTANCE;
    camera_clamp(&ui->scene.cam);
    ui->map.scroll_request = true;
    ui->map.scroll_to      = hex;
}

/* Each frame: a list request, or a new selection from any panel, frames
   the city or the unit. */
internal void
ui_follow_selection(ui_state *ui, engine_api *api)
{
    if (ui->scene.frame_city || ui->city.selected != ui->scene.framed_city)
    {
        engine_city_slots site = api->city_slots(api->engine, ui->city.selected);
        if (site.city_id != ENGINE_NO_CITY) ui_frame_hex(ui, api, hex_make(site.q, site.r));
        ui->scene.framed_city = ui->city.selected;
        ui->scene.frame_city  = false;
    }
    if (ui->scene.frame_unit || (ui->units.selected != ui->scene.framed_unit && ui->units.selected != ENGINE_NO_UNIT))
    {
        engine_unit unit = api->unit_at(api->engine, ui->units.selected);
        if (unit.id != ENGINE_NO_UNIT) ui_frame_hex(ui, api, hex_make(unit.q, unit.r));
        ui->scene.frame_unit = false;
    }
    ui->scene.framed_unit = ui->units.selected;
}

/* ---- what a hex is --------------------------------------------------------------------- */

/* The name of a region: its city, gate or port, or an outer area. */
internal void
ui_region_name(ui_state *ui, engine_api *api, u8 region, char *out, u32 out_size)
{
    if (region < SAVE_CITY_COUNT) { ui_city_label(ui, api, region, out, out_size); return; }
    if (region >= CONTENT_FIRST_BASE && region < CONTENT_FIRST_BASE + CONTENT_BASE_COUNT)
    {
        engine_base base = api->base_at(api->engine, (u32)region - CONTENT_FIRST_BASE);
        if (base.id != ENGINE_NO_ROW) { snprintf(out, out_size, "%s", ui_text_with_key(ui, "base", base.key)); return; }
    }
    snprintf(out, out_size, "%s", region == ENGINE_NO_REGION ? "-" : text_get(ui->text, "hexinfo.outer"));
}

/* The lines of the Hex info box for a hex, in the current window. */
internal void
ui_draw_hex_lines(ui_state *ui, engine_api *api, hex_coord hex)
{
    engine_hex data = api->hex_at(api->engine, hex.q, hex.r);
    hex_offset o = hex_to_offset(hex);
    if (!data.on_map) { ImGui::TextDisabled("%s", text_get(ui->text, "hexinfo.off_map")); return; }
    char name[UI_NAME_SIZE * 4];
    engine_terrain_kind terrain = api->terrain_kind_at(api->engine, data.terrain);
    ImGui::Text("x %d  y %d", o.col, o.row);
    ImGui::Text("%s: %s   %s: %u", text_get(ui->text, "hexinfo.terrain"), ui_text_with_key(ui, "terrain", terrain.key),
                text_get(ui->text, "hexinfo.height"), data.height);
    ui_region_name(ui, api, data.region, name, sizeof(name));
    ImGui::Text("%s: %s", text_get(ui->text, "hexinfo.region"), name);
    if (data.city_id != ENGINE_NO_CITY)
    {
        ui_city_label(ui, api, data.city_id, name, sizeof(name));
        ImGui::Text("%s: %s", text_get(ui->text, "hexinfo.city"), name);
    }
    if (data.base_id != ENGINE_NO_BASE)
    {
        ui_region_name(ui, api, (u8)data.base_id, name, sizeof(name));
        ImGui::Text("%s: %s", text_get(ui->text, "hexinfo.base"), name);
    }
    if (data.slot_city != ENGINE_NO_CITY)
    {
        ui_city_label(ui, api, data.slot_city, name, sizeof(name));
        ImGui::Text("%s: %s %u", text_get(ui->text, "hexinfo.slot"), name, data.slot_index + 1u);
        if (data.facility != ENGINE_NO_FACILITY)
        {
            engine_facility_kind kind = api->facility_kind_at(api->engine, data.facility);
            ImGui::Text("%s: %s  Lv%u", text_get(ui->text, "hexinfo.facility"), ui_text_with_key(ui, "facility", kind.key), data.facility_level);
        }
    }
    if (data.building_id != ENGINE_NO_BUILDING)
    {
        engine_building_kind kind = api->building_kind_at(api->engine, data.building_kind);
        engine_building building = api->building_at(api->engine, data.building_id);
        ImGui::Text("%s: %s  %u / %u", text_get(ui->text, "hexinfo.building"), ui_text_with_key(ui, "building", kind.key),
                    building.durability, building.durability_max);
    }
    if (data.unit_id != ENGINE_NO_UNIT)
    {
        engine_unit unit = api->unit_at(api->engine, data.unit_id);
        ui_unit_label(ui, api, &unit, name, sizeof(name));
        ImGui::Text("%s: %s  %u", text_get(ui->text, "hexinfo.unit"), name, unit.soldiers);
    }
    const char *sites[5] = {};
    u32 site_count = 0;
    if (data.trap == 1)        sites[site_count++] = text_get(ui->text, "hexinfo.dike");
    if (data.trap == 2)        sites[site_count++] = text_get(ui->text, "hexinfo.rockfall");
    if (data.bandit_site)      sites[site_count++] = text_get(ui->text, "hexinfo.bandit");
    if (data.event_site)       sites[site_count++] = text_get(ui->text, "hexinfo.event");
    if (data.flood)            sites[site_count++] = text_get(ui->text, "hexinfo.flood");
    for (u32 index = 0; index < site_count; ++index) ImGui::BulletText("%s", sites[index]);
    if (data.fire_turns) ImGui::Text("%s: %u", text_get(ui->text, "hexinfo.fire"), data.fire_turns);
}

/* The small Hex info box of a hex that a click in the scene picked. */
internal void
ui_draw_hex_info_box(ui_state *ui, engine_api *api)
{
    if (!ui->scene.has_pick) return;
    char title[128];
    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
    ui_context_place(ui, 0);   /* the game mode: the dialog opens next to its entity */
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "hexinfo.title", "HexInfo"), &open, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ui_context_track(ui);
        ui_draw_hex_lines(ui, api, ui->scene.pick);
    }
    ImGui::End();
    if (!open) ui->scene.has_pick = false;
}

/* ---- the plates over the bases and the units (ui/main-map.md) -------------------------- */

#define UI_PLATE_SLOT_DISTANCE 60.0f   /* the slots draw when the camera is nearer than this */

/* A plate: the name, a number (the soldiers) and a thin bar (a fraction,
   the durability of a base). It sits on the background, under the panels. */
internal void
ui_draw_plate(ImDrawList *draw, f32 x, f32 y, const char *name, u32 number, f32 fraction, ImU32 colour, bool selected)
{
    char line[UI_NAME_SIZE * 2 + 16];
    snprintf(line, sizeof(line), "%s %u", name, number);
    ImVec2 size = ImGui::CalcTextSize(line);
    ImVec2 top(x - size.x * 0.5f - 4.0f, y - size.y - 10.0f);
    ImVec2 bottom(x + size.x * 0.5f + 4.0f, y - 4.0f);
    draw->AddRectFilled(top, bottom, IM_COL32(0, 0, 0, 170), 3.0f);
    draw->AddRect(top, bottom, selected ? IM_COL32(255, 230, 60, 255) : colour, 3.0f, 0, selected ? 2.0f : 1.0f);
    draw->AddText(ImVec2(top.x + 4.0f, top.y + 1.0f), IM_COL32(255, 255, 255, 255), line);
    f32 clamped = fraction < 0.0f ? 0.0f : (fraction > 1.0f ? 1.0f : fraction);
    draw->AddRectFilled(ImVec2(top.x + 2.0f, bottom.y - 4.0f), ImVec2(top.x + 2.0f + (bottom.x - top.x - 4.0f) * clamped, bottom.y - 2.0f), colour);
}

/* The screen point over a hex, or false when it is off the screen. */
internal bool
ui_scene_screen(engine_api *api, m4 view_projection, hex_coord hex, f32 lift, f32 *x, f32 *y)
{
    ImGuiIO &io = ImGui::GetIO();
    v3 above = ui_hex_world(api, hex);
    above.y += lift;
    return camera_project(view_projection, above, io.DisplaySize.x, io.DisplaySize.y, x, y) &&
           *x >= 0.0f && *y >= 0.0f && *x <= io.DisplaySize.x && *y <= io.DisplaySize.y;
}

/* The first UTF-8 character of a text. */
internal void
ui_first_character(const char *text, char *out, u32 out_size)
{
    u32 length = text[0] ? 1 : 0;
    while (text[length] && (text[length] & 0xC0) == 0x80) length++;
    snprintf(out, out_size, "%.*s", (int)length, text);
}

/* ---- M12: the unit plates (ui/main-map.md, "Map labels") ----------------------------- */

#define UI_PLATE_GRAIN_LOW_TURNS   3    /* the supply icon is red below this (assumed, Q-215) */
#define UI_PLATE_GRAIN_SAFE_TURNS  10   /* the supply icon is green from this (assumed, Q-215) */
#define UI_PLATE_PAD               2.0f

enum ui_unit_mark { UI_UNIT_MARK_READY, UI_UNIT_MARK_DONE, UI_UNIT_MARK_DELEGATED, UI_UNIT_MARK_COUNT };

/* The status mark of a unit: 完 after it acted in this turn, 委 when its
   home city is in a delegated corps (not corps 0), else 未. The meaning of
   委 and the order of the marks are assumed (Q-215). */
internal u32
ui_unit_mark_of(engine_unit *unit, u8 corps)
{
    if (unit->acted) return UI_UNIT_MARK_DONE;
    if (corps != 0)  return UI_UNIT_MARK_DELEGATED;
    return UI_UNIT_MARK_READY;
}

/* The colour of the supply icon from the turns that the grain lasts:
   green, yellow, red, or grey when the grain is not known (assumed, Q-215). */
internal ImU32
ui_grain_colour(u32 grain_turns)
{
    if (grain_turns == ENGINE_GRAIN_TURNS_UNKNOWN) return IM_COL32(130, 130, 130, 255);
    if (grain_turns < UI_PLATE_GRAIN_LOW_TURNS)    return IM_COL32(220, 50, 40, 255);
    if (grain_turns < UI_PLATE_GRAIN_SAFE_TURNS)   return IM_COL32(230, 190, 40, 255);
    return IM_COL32(60, 180, 70, 255);
}

/* A unit plate: the portrait of the commander, the unit name and the
   status mark on the first row, the supply icon and the soldiers on the
   second. The portrait is the default portrait of the commander
   (ui_officer_portrait). Without one, a square in the faction colour with
   the first character of the commander's name stands in for it. */
internal void
ui_draw_unit_plate(ui_state *ui, engine_api *api, ImDrawList *draw, f32 x, f32 y, engine_unit *unit, bool selected)
{
    engine_officer commander = api->officer_by_id(api->engine, unit->officers[0]);
    char name[UI_NAME_SIZE * 2] = "?";
    if (commander.officer_id != ENGINE_NO_OFFICER) ui_officer_name(ui, &commander, name, sizeof(name));
    char label[UI_NAME_SIZE * 4], initial[16], soldiers[16];
    ui_unit_label(ui, api, unit, label, sizeof(label));
    ui_first_character(name, initial, sizeof(initial));
    snprintf(soldiers, sizeof(soldiers), "%u", unit->soldiers);
    u32 mark = ui_unit_mark_of(unit, api->city_corps(api->engine, unit->home_city));
    const char *mark_ids[UI_UNIT_MARK_COUNT] = { "plate.ready", "plate.done", "plate.delegated" };
    ImU32 mark_colours[UI_UNIT_MARK_COUNT] = { IM_COL32(40, 160, 60, 255), IM_COL32(120, 120, 120, 255), IM_COL32(50, 100, 220, 255) };
    const char *mark_text   = text_get(ui->text, mark_ids[mark]);
    const char *supply_text = text_get(ui->text, "plate.supply");

    f32 pad = UI_PLATE_PAD, line = ImGui::GetTextLineHeight();
    f32 portrait = line * 2.0f + pad;
    ImVec2 label_size = ImGui::CalcTextSize(label), mark_size = ImGui::CalcTextSize(mark_text);
    ImVec2 supply_size = ImGui::CalcTextSize(supply_text), soldier_size = ImGui::CalcTextSize(soldiers);
    f32 first_row  = label_size.x + pad * 3.0f + mark_size.x + pad * 2.0f;
    f32 second_row = supply_size.x + pad * 4.0f + soldier_size.x;
    f32 width  = portrait + pad * 4.0f + (first_row > second_row ? first_row : second_row);
    f32 height = portrait + pad * 2.0f;
    ImVec2 top(x - width * 0.5f, y - height - 4.0f), bottom(x + width * 0.5f, y - 4.0f);
    ImU32 faction = ui_faction_color(unit->force_id), white = IM_COL32(255, 255, 255, 255);
    draw->AddRectFilled(top, bottom, IM_COL32(0, 0, 0, 170), 3.0f);
    draw->AddRect(top, bottom, selected ? IM_COL32(255, 230, 60, 255) : faction, 3.0f, 0, selected ? 2.0f : 1.0f);

    ImVec2 face_top(top.x + pad, top.y + pad), face_bottom(top.x + pad + portrait, top.y + pad + portrait);
    draw->AddRectFilled(face_top, face_bottom, faction, 2.0f);
    ImTextureID face = commander.officer_id != ENGINE_NO_OFFICER ? ui_officer_portrait(ui, &commander) : 0;
    if (face)
    {
        /* A square from the 4:5 picture: the full width, from near the top. */
        draw->AddImageRounded(face, ImVec2(face_top.x + 1.0f, face_top.y + 1.0f), ImVec2(face_bottom.x - 1.0f, face_bottom.y - 1.0f),
                              ImVec2(0.0f, UI_PORTRAIT_SQUARE_TOP), ImVec2(1.0f, UI_PORTRAIT_SQUARE_TOP + UI_PORTRAIT_SQUARE_HEIGHT),
                              IM_COL32_WHITE, 2.0f);
    }
    else
    {
        ImVec2 initial_size = ImGui::CalcTextSize(initial);
        draw->AddText(ImVec2((face_top.x + face_bottom.x - initial_size.x) * 0.5f, (face_top.y + face_bottom.y - initial_size.y) * 0.5f), white, initial);
    }

    f32 text_x = face_bottom.x + pad * 2.0f;
    draw->AddText(ImVec2(text_x, top.y + pad), white, label);
    ImVec2 mark_top(bottom.x - pad * 3.0f - mark_size.x, top.y + pad);
    draw->AddRectFilled(mark_top, ImVec2(bottom.x - pad, mark_top.y + line), mark_colours[mark], 2.0f);
    draw->AddText(ImVec2(mark_top.x + pad, mark_top.y), white, mark_text);

    f32 second = top.y + pad * 2.0f + line;
    draw->AddRectFilled(ImVec2(text_x, second), ImVec2(text_x + supply_size.x + pad * 2.0f, second + line), ui_grain_colour(unit->grain_turns), 2.0f);
    draw->AddText(ImVec2(text_x + pad, second), white, supply_text);
    draw->AddText(ImVec2(text_x + supply_size.x + pad * 4.0f, second), white, soldiers);
}

/* A glyph on each slot of a city in view: the first character of the
   facility and its level, or a ring for a free slot. */
internal void
ui_draw_slot_glyphs(ui_state *ui, engine_api *api, m4 view_projection, ImDrawList *draw, u8 city_id)
{
    engine_city_slots site = api->city_slots(api->engine, city_id);
    for (u32 slot = 0; slot < site.slot_count; ++slot)
    {
        engine_facility facility = api->facility_at(api->engine, city_id, (u8)slot);
        f32 x = 0.0f, y = 0.0f;
        if (facility.city_id == ENGINE_NO_CITY || !ui_scene_screen(api, view_projection, hex_make(facility.q, facility.r), 0.3f, &x, &y)) continue;
        if (facility.facility == ENGINE_NO_FACILITY)
        {
            draw->AddCircle(ImVec2(x, y), 4.0f, IM_COL32(230, 230, 230, 200), 0, 1.5f);
            continue;
        }
        engine_facility_kind kind = api->facility_kind_at(api->engine, facility.facility);
        char initial[16], glyph[24];
        ui_first_character(ui_text_with_key(ui, "facility", kind.key), initial, sizeof(initial));
        snprintf(glyph, sizeof(glyph), "%s%u", initial, facility.level);
        ImVec2 size = ImGui::CalcTextSize(glyph);
        draw->AddRectFilled(ImVec2(x - size.x * 0.5f - 2.0f, y - size.y * 0.5f), ImVec2(x + size.x * 0.5f + 2.0f, y + size.y * 0.5f),
                            facility.state == SIM_SLOT_BUILT ? IM_COL32(40, 60, 120, 220) : IM_COL32(90, 90, 90, 220), 2.0f);
        draw->AddText(ImVec2(x - size.x * 0.5f, y - size.y * 0.5f), IM_COL32(255, 255, 255, 255), glyph);
    }
}

/* The plates of the cities, the gates, the ports and the units, the slots
   of the cities near the camera, and the commander of the unit under the
   mouse (ui/main-map.md: the name plates and the slot clusters). */
internal void
ui_draw_scene_labels(ui_state *ui, engine_api *api)
{
    if (!ui->scene.enabled || !ui->scene.initialized) return;
    ImGuiIO &io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) return;
    m4 view_projection = camera_view_projection(&ui->scene.cam, io.DisplaySize.x, io.DisplaySize.y);
    ImDrawList *draw = ImGui::GetBackgroundDrawList();
    bool close_up = ui->scene.cam.distance < UI_PLATE_SLOT_DISTANCE;
    for (u32 index = 0; index < api->city_count(api->engine); ++index)
    {
        engine_city city = api->city_at(api->engine, index);
        engine_city_slots site = api->city_slots(api->engine, city.city_id);
        if (site.city_id == ENGINE_NO_CITY) continue;
        if (close_up) ui_draw_slot_glyphs(ui, api, view_projection, draw, city.city_id);
        f32 x = 0.0f, y = 0.0f;
        if (!ui_scene_screen(api, view_projection, hex_make(site.q, site.r), UI_LABEL_LIFT, &x, &y)) continue;
        char name[UI_NAME_SIZE * 2];
        ui_city_label(ui, api, city.city_id, name, sizeof(name));
        engine_city_siege siege = api->city_siege(api->engine, city.city_id);
        f32 fraction = siege.durability_max ? (f32)siege.durability / (f32)siege.durability_max : 1.0f;
        ui_draw_plate(draw, x, y, name, city.troops, fraction, ui_faction_color(city.owner_force_id), city.city_id == ui->city.selected);
    }
    for (u32 index = 0; index < api->base_count(api->engine); ++index)
    {
        engine_base base = api->base_at(api->engine, index);
        f32 x = 0.0f, y = 0.0f;
        if (base.id == ENGINE_NO_ROW || !ui_scene_screen(api, view_projection, hex_make(base.q, base.r), UI_LABEL_LIFT, &x, &y)) continue;
        f32 fraction = base.durability_max ? (f32)base.durability / (f32)base.durability_max : 1.0f;
        ui_draw_plate(draw, x, y, ui_text_with_key(ui, "base", base.key), base.troops, fraction, ui_faction_color(base.force), false);
    }
    for (u32 id = 0; id < api->unit_count(api->engine); ++id)
    {
        engine_unit unit = api->unit_at(api->engine, id);
        f32 x = 0.0f, y = 0.0f;
        if (unit.id == ENGINE_NO_UNIT || !ui_scene_screen(api, view_projection, ui_round_unit_hex(ui, &unit), UI_LABEL_LIFT, &x, &y)) continue;   /* M20 */
        ui_draw_unit_plate(ui, api, draw, x, y, &unit, unit.id == ui->units.selected);   /* M12 */
    }
    if (ui->scene.has_hover && !io.WantCaptureMouse && !ui->hud_mode)   /* M21: the game mode shows the hover panel */
    {
        u32 unit_id = api->unit_at_hex(api->engine, ui->scene.hover.q, ui->scene.hover.r);
        if (unit_id != ENGINE_NO_UNIT)
        {
            engine_unit unit = api->unit_at(api->engine, unit_id);
            char name[UI_NAME_SIZE * 4], force[UI_NAME_SIZE * 2];
            ui_unit_label(ui, api, &unit, name, sizeof(name));
            ui_force_text(ui, api, unit.force_id, force, sizeof(force));
            ImGui::SetTooltip("%s\n%s  %u", name, force, unit.soldiers);
        }
    }
}

/* The 設置 field of the status bar: what the terrain lets a player place.
   M22: a city hex shows 不可 in red (clone-gap-report.md, section 1).
   Returns true when the text is red. */
internal bool
ui_status_place(ui_state *ui, engine_api *api, engine_hex *data, char *out, u32 out_size)
{
    if (data->city_id != ENGINE_NO_CITY)
    {
        snprintf(out, out_size, "%s", text_get(ui->text, "place.none"));
        return true;
    }
    engine_terrain_kind terrain = api->terrain_kind_at(api->engine, data->terrain);
    b32 water = terrain.cost[MOVEMENT_NAVY] && !terrain.cost[MOVEMENT_FOOT];
    if (terrain.buildable && data->base_id == ENGINE_NO_BASE)
    {
        snprintf(out, out_size, "%s / %s / %s", text_get(ui->text, "place.military"), text_get(ui->text, "place.camp"), text_get(ui->text, "place.trap"));
    }
    else if (water) snprintf(out, out_size, "%s", text_get(ui->text, "place.fire_ship"));
    else            snprintf(out, out_size, "-");
    return false;
}

/* The status bar at the bottom centre of the map view (ui/main-map.md):
   the terrain with the owning base, the coordinates x,y, and what the
   terrain lets a player place. It shows the hex under the mouse. */
internal void
ui_draw_status_bar(ui_state *ui, engine_api *api)
{
    if (!ui->scene.enabled || !ui->scene.has_hover) return;
    engine_hex data = api->hex_at(api->engine, ui->scene.hover.q, ui->scene.hover.r);
    if (!data.on_map) return;
    engine_terrain_kind terrain = api->terrain_kind_at(api->engine, data.terrain);
    char owner[UI_NAME_SIZE * 2];
    ui_region_name(ui, api, data.region, owner, sizeof(owner));
    char place[128];
    bool red = ui_status_place(ui, api, &data, place, sizeof(place));
    hex_offset o = hex_to_offset(ui->scene.hover);
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y - 8.0f), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.75f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("###StatusBar", 0, flags))
    {
        ImGui::Text("%s: %s (%s)    %s: %d,%d    %s:", text_get(ui->text, "statusbar.terrain"), ui_text_with_key(ui, "terrain", terrain.key), owner,
                    text_get(ui->text, "statusbar.coords"), o.col, o.row, text_get(ui->text, "statusbar.place"));
        ImGui::SameLine();
        if (red) ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.25f, 1.0f), "%s", place);
        else     ImGui::TextUnformatted(place);
    }
    ImGui::End();
}

/* ---- the Units panel --------------------------------------------------------------------- */

internal void
ui_draw_units_panel(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(360, 300), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "units.title", "Units"), &ui->show_units)) { ImGui::End(); return; }
    void *e = api->engine;
    u32 shown = 0;
    if (ImGui::BeginTable("units", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn(text_get(ui->text, "units.unit"));
        ImGui::TableSetupColumn(text_get(ui->text, "units.force"));
        ImGui::TableSetupColumn(text_get(ui->text, "units.soldiers"));
        ImGui::TableHeadersRow();
        for (u32 id = 0; id < api->unit_count(e); ++id)
        {
            engine_unit unit = api->unit_at(e, id);
            if (unit.id == ENGINE_NO_UNIT) continue;
            char name[UI_NAME_SIZE * 4], force[UI_NAME_SIZE * 2], label[UI_NAME_SIZE * 4 + 16];
            ui_unit_label(ui, api, &unit, name, sizeof(name));
            ui_force_text(ui, api, unit.force_id, force, sizeof(force));
            snprintf(label, sizeof(label), "%s###unit%u", name, unit.id);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(label, unit.id == ui->units.selected, ImGuiSelectableFlags_SpanAllColumns)) ui_frame_unit(ui, unit.id);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(force);
            ImGui::TableNextColumn();
            ImGui::Text("%u", unit.soldiers);
            shown++;
        }
        ImGui::EndTable();
    }
    if (!shown) ImGui::TextDisabled("-");
    ImGui::End();
}

/* ---- the facilities of a city by kind and level (都市情報) ------------------------------ */

struct ui_facility_count
{
    u8  facility;
    u8  level;
    u32 count;
};

/* The built facilities of the city, one row for each kind and level, in
   the order of the facility table and the level. Returns the rows. */
internal u32
ui_count_facilities(engine_api *api, u8 city_id, ui_facility_count *out, u32 capacity)
{
    u32 rows = 0;
    engine_city_slots site = api->city_slots(api->engine, city_id);
    for (u32 slot = 0; slot < site.slot_count; ++slot)
    {
        engine_facility facility = api->facility_at(api->engine, city_id, (u8)slot);
        if (facility.city_id == ENGINE_NO_CITY || facility.state != SIM_SLOT_BUILT || facility.facility == ENGINE_NO_FACILITY) continue;
        u32 row = 0;
        while (row < rows && (out[row].facility != facility.facility || out[row].level != facility.level)) row++;
        if (row == rows)
        {
            if (rows >= capacity) continue;
            u32 at = rows++;
            while (at > 0 && (out[at - 1].facility > facility.facility || (out[at - 1].facility == facility.facility && out[at - 1].level > facility.level)))
            {
                out[at] = out[at - 1];
                at--;
            }
            out[at].facility = facility.facility;
            out[at].level    = facility.level;
            out[at].count    = 0;
            row = at;
        }
        out[row].count++;
    }
    return rows;
}

/* The table of the City panel: the slots in use and the counts. */
internal void
ui_draw_city_facility_counts(ui_state *ui, engine_api *api, u8 city_id)
{
    ui_facility_count counts[UI_FACILITY_COUNT_CAPACITY];
    u32 rows = ui_count_facilities(api, city_id, counts, ArrayCount(counts));
    engine_city_slots site = api->city_slots(api->engine, city_id);
    ImGui::SeparatorText(text_get(ui->text, "city.facility_counts"));
    ImGui::Text("%s %u / %u", text_get(ui->text, "city.slots_used"), site.used, site.slot_count);
    if (ImGui::BeginTable("facility_counts", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
    {
        for (u32 row = 0; row < rows; ++row)
        {
            engine_facility_kind kind = api->facility_kind_at(api->engine, counts[row].facility);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(ui_text_with_key(ui, "facility", kind.key));
            ImGui::TableNextColumn();
            ImGui::Text("Lv%u", counts[row].level);
            ImGui::TableNextColumn();
            ImGui::Text("%u", counts[row].count);
        }
        ImGui::EndTable();
    }
}
