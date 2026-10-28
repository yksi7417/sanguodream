/* Where things are (M11): the camera frames the city or the unit that a
   list picks, the scene draws a marker and a name over each city, the
   Hex info box tells what a picked hex is, the Units panel lists the
   units, and the City panel counts its facilities by kind and level as
   the 都市情報 screen does. Included by ui.cpp after ui_scene.cpp; it calls
   engine_api only. */

#define UI_FRAME_DISTANCE    24.0f   /* the camera distance after a frame */
#define UI_LABEL_LIFT        1.2f    /* world units above the top of the hex */
#define UI_UNIT_PLATE_LIFT   1.45f   /* the plate of a unit sits on the head of its sprite (UI_SCENE_UNIT_SPRITE_SIZE is 1.4; the player asked for it on 2026-10-24) */
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
        if (base.id != ENGINE_NO_ROW) { ui_base_name(ui, api, &base, out, out_size); return; }
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

internal s32   ui_hit_mark(ui_state *ui, u32 unit_id, u8 kind);   /* ui_war.cpp */
internal ImU32 ui_hit_colour(s32 mark, ImU32 plain);

#define UI_PLATE_GRAIN_LOW_TURNS   3    /* the supply icon is red below this (assumed, Q-215) */
#define UI_PLATE_GRAIN_SAFE_TURNS  10   /* the supply icon is green from this (assumed, Q-215) */
#define UI_PLATE_PAD               2.0f

enum ui_unit_mark { UI_UNIT_MARK_READY, UI_UNIT_MARK_DONE, UI_UNIT_MARK_DELEGATED, UI_UNIT_MARK_COUNT };

/* The status mark of a unit: 委 when it has a long order (the original
   shows 委 after 殲滅 and after a far 移動, G2-052 and G2-058), 完 after it
   acted in this turn, 委 when its home city is in a delegated corps (not
   corps 0; assumed, Q-215), else 未. */
internal u32
ui_unit_mark_of(engine_unit *unit, u8 corps)
{
    /* A unit with a long order shows 完 after its step of this turn (the
       player asked on 2026-10-24: 委 alone did not tell whether it was done). */
    if (unit->acted || (unit->order && unit->moved)) return UI_UNIT_MARK_DONE;
    if (unit->order) return UI_UNIT_MARK_DELEGATED;
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

/* ---- the flag of a unit (docs/GRAPHICS.md, "Faction colours") ------------------------ */

#define UI_FLAG_CHARACTER_SCALE 1.6f    /* the character of the flag, in font sizes */
#define UI_FLAG_PAD_SCALE       0.22f   /* the margin round the character, in character sizes */
#define UI_FLAG_TAIL_SCALE      0.30f   /* the depth of the swallowtail, in character sizes */
#define UI_FLAG_LIGHT_LUMA      150     /* a colour this light or lighter gets a dark character */
#define UI_FLAG_NAME_SIZE       16      /* two Chinese characters in UTF-8, with the NUL */

/* The family name of the ruler of a faction, in UTF-8. A corps of a save
   has no ruler, so then the leader of corps 1 names it, as in
   ui_force_text. The flag shows the Chinese characters in each locale,
   from the Big5 bytes, as the portraits do. "" if the faction has no leader. */
internal void
ui_force_family_name(engine_api *api, u8 force_id, char *out, u32 out_size)
{
    out[0] = 0;
    if (force_id == ENGINE_NO_FACTION) return;
    u16 ruler = api->force_ruler(api->engine, force_id);
    for (u32 index = 0; ruler == ENGINE_NO_OFFICER && index < api->faction_count(api->engine); ++index)
    {
        engine_faction faction = api->faction_at(api->engine, index);
        if (faction.force_id == force_id) ruler = faction.leader_officer_id;
    }
    if (ruler == ENGINE_NO_OFFICER) return;
    engine_officer officer = api->officer_by_id(api->engine, ruler);
    if (officer.officer_id == ENGINE_NO_OFFICER) return;
    text_decode_big5_into(officer.family_name, sizeof(officer.family_name), out, out_size);
}

/* The colour of the character on a flag: dark on a light flag, white on a
   dark flag. The second colour is the outline of the character: a dark
   outline round a white character, none round a dark one (a light outline
   makes the dark strokes grey). */
internal void
ui_flag_ink(ImU32 flag, ImU32 *ink, ImU32 *outline)
{
    u32 red = (flag >> IM_COL32_R_SHIFT) & 0xFF, green = (flag >> IM_COL32_G_SHIFT) & 0xFF, blue = (flag >> IM_COL32_B_SHIFT) & 0xFF;
    u32 luma = (299 * red + 587 * green + 114 * blue) / 1000;
    b32 light = luma >= UI_FLAG_LIGHT_LUMA;
    *ink     = light ? IM_COL32(25, 20, 20, 255) : IM_COL32(255, 255, 255, 255);
    *outline = light ? 0 : IM_COL32(0, 0, 0, 200);
}

/* The flag of a unit: a banner with a swallowtail in the colour of the
   faction. It shows the family name of the ruler in large characters,
   one under the other: 曹, 孫, 劉 or 公孫. A pole holds it at its left edge.
   right and bottom are the corner of the flag next to the plate. The flag
   asks the engine for the ruler and the colour, and commands nothing. */
internal void
ui_draw_unit_flag(engine_api *api, ImDrawList *draw, f32 right, f32 bottom, u8 force_id)
{
    char family[UI_FLAG_NAME_SIZE];
    ui_force_family_name(api, force_id, family, sizeof(family));
    char characters[2][UI_FLAG_NAME_SIZE] = {};
    ui_first_character(family, characters[0], sizeof(characters[0]));
    ui_first_character(family + strlen(characters[0]), characters[1], sizeof(characters[1]));
    u32 lines = characters[1][0] ? 2 : 1;

    ImFont *font = ImGui::GetFont();
    f32 size = ImGui::GetFontSize() * UI_FLAG_CHARACTER_SCALE;
    f32 pad = size * UI_FLAG_PAD_SCALE, tail = size * UI_FLAG_TAIL_SCALE;
    f32 width = size + pad * 2.0f, height = size * (f32)lines + pad * 2.0f + tail;
    f32 left = right - width, top = bottom - height, body_bottom = bottom - tail, middle = (left + right) * 0.5f;
    ImU32 colour = ui_faction_color(api, force_id), ink, outline;
    ui_flag_ink(colour, &ink, &outline);
    ImU32 edge = IM_COL32(20, 16, 12, 230);

    /* The pole: from above the flag to below it, with a gold knob on top. */
    f32 pole = left - 1.5f;
    draw->AddLine(ImVec2(pole, top - pad), ImVec2(pole, bottom + height * 0.5f), IM_COL32(60, 40, 20, 255), 3.0f);
    draw->AddCircleFilled(ImVec2(pole, top - pad), 3.5f, IM_COL32(230, 190, 60, 255));

    /* The cloth: a body and two tails, then the outline of the whole shape. */
    draw->AddRectFilled(ImVec2(left, top), ImVec2(right, body_bottom), colour);
    draw->AddTriangleFilled(ImVec2(left, body_bottom), ImVec2(middle, body_bottom), ImVec2(left, bottom), colour);
    draw->AddTriangleFilled(ImVec2(middle, body_bottom), ImVec2(right, body_bottom), ImVec2(right, bottom), colour);
    ImVec2 shape[5] = { ImVec2(left, top), ImVec2(right, top), ImVec2(right, bottom), ImVec2(middle, body_bottom), ImVec2(left, bottom) };
    draw->AddPolyline(shape, 5, edge, ImDrawFlags_Closed, 1.5f);

    for (u32 line = 0; line < lines; ++line)
    {
        ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, characters[line]);
        ImVec2 at(middle - extent.x * 0.5f, top + pad + size * (f32)line + (size - extent.y) * 0.5f);
        for (s32 dx = -1; outline && dx <= 2; ++dx)
        {
            for (s32 dy = -1; dy <= 1; ++dy)
            {
                if ((dx != 0 && dx != 1) || dy) draw->AddText(font, size, ImVec2(at.x + (f32)dx, at.y + (f32)dy), outline, characters[line]);
            }
        }
        /* Two strokes one pixel apart: a bold character that reads at a distance. */
        draw->AddText(font, size, at, ink, characters[line]);
        draw->AddText(font, size, ImVec2(at.x + 1.0f, at.y), ink, characters[line]);
    }
}

/* A unit plate: the portrait of the commander, the unit name and the
   status mark on the first row, the supply icon and the soldiers on the
   second. The portrait is the default portrait of the commander
   (ui_officer_portrait). Without one, a square in the faction colour with
   the first character of the commander's name stands in for it. The flag
   of the faction stands at the left of the plate (ui_draw_unit_flag). */
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
    ImU32 faction = ui_faction_color(api, unit->force_id), white = IM_COL32(255, 255, 255, 255);
    if (ui->units.plate_count < UI_PLATE_CAPACITY)   /* G2-042: a click on the plate reaches the unit */
    {
        u32 place = ui->units.plate_count++;
        ui->units.plates[place].unit = unit->id;
        ui->units.plates[place].x0 = top.x;    ui->units.plates[place].y0 = top.y;
        ui->units.plates[place].x1 = bottom.x; ui->units.plates[place].y1 = bottom.y;
    }
    draw->AddRectFilled(top, bottom, IM_COL32(0, 0, 0, 170), 3.0f);
    draw->AddRect(top, bottom, selected ? IM_COL32(255, 230, 60, 255) : faction, 3.0f, 0, selected ? 2.0f : 1.0f);
    ui_draw_unit_flag(api, draw, top.x - 2.0f, bottom.y, unit->force_id);   /* the faction at a glance (the player, 2026-10-25) */

    ImVec2 face_top(top.x + pad, top.y + pad), face_bottom(top.x + pad + portrait, top.y + pad + portrait);
    draw->AddRectFilled(face_top, face_bottom, faction, 2.0f);
    ImTextureID face = commander.officer_id != ENGINE_NO_OFFICER ? ui_officer_portrait(ui, api, &commander) : 0;
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
    /* After a hit, the soldiers show in red (a loss) or green (a gain) for a few seconds (ui_war.cpp). */
    s32 hit = ui_hit_mark(ui, unit->id, (u8)UI_HIT_SOLDIERS);
    ImVec2 soldiers_at(text_x + supply_size.x + pad * 4.0f, second);
    draw->AddText(soldiers_at, ui_hit_colour(hit, white), soldiers);
    if (hit) draw->AddLine(ImVec2(soldiers_at.x, second + line), ImVec2(soldiers_at.x + soldier_size.x, second + line), ui_hit_colour(hit, white), 2.0f);
}

/* A glyph on each slot of a city in view: the first character of the
   facility and its level, or a ring for a free slot. A build in progress
   shows the turns left after the character, on an orange plate. */
internal void
ui_draw_slot_glyphs(ui_state *ui, engine_api *api, m4 view_projection, ImDrawList *draw, u8 city_id)
{
    engine_city_slots site = api->city_slots(api->engine, city_id);
    for (u32 slot = 0; slot < site.slot_count; ++slot)
    {
        engine_facility facility = api->facility_at(api->engine, city_id, (u8)slot);
        f32 x = 0.0f, y = 0.0f;
        f32 lift = facility.facility == ENGINE_NO_FACILITY ? 0.3f : UI_FACILITY_GLYPH_LIFT;
        if (facility.city_id == ENGINE_NO_CITY || !ui_scene_screen(api, view_projection, hex_make(facility.q, facility.r), lift, &x, &y)) continue;
        if (facility.facility == ENGINE_NO_FACILITY)
        {
            draw->AddCircle(ImVec2(x, y), 4.0f, IM_COL32(230, 230, 230, 200), 0, 1.5f);
            continue;
        }
        engine_facility_kind kind = api->facility_kind_at(api->engine, facility.facility);
        char initial[16], glyph[24];
        ui_first_character(ui_text_with_key(ui, "facility", kind.key), initial, sizeof(initial));
        b32 in_progress = facility.state == SIM_SLOT_BUILDING;
        if (in_progress) snprintf(glyph, sizeof(glyph), "%s-%u", initial, facility.turns_left);
        else             snprintf(glyph, sizeof(glyph), "%s%u", initial, facility.level);
        ImVec2 size = ImGui::CalcTextSize(glyph);
        ImU32 plate = facility.state == SIM_SLOT_BUILT ? IM_COL32(40, 60, 120, 220) : (in_progress ? IM_COL32(170, 100, 20, 220) : IM_COL32(90, 90, 90, 220));
        draw->AddRectFilled(ImVec2(x - size.x * 0.5f - 2.0f, y - size.y * 0.5f), ImVec2(x + size.x * 0.5f + 2.0f, y + size.y * 0.5f), plate, 2.0f);
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
    ui->units.plate_count = 0;
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
        ui_draw_plate(draw, x, y, name, city.troops, fraction, ui_faction_color(api, city.owner_force_id), city.city_id == ui->city.selected);
    }
    for (u32 index = 0; index < api->base_count(api->engine); ++index)
    {
        engine_base base = api->base_at(api->engine, index);
        f32 x = 0.0f, y = 0.0f;
        if (base.id == ENGINE_NO_ROW || !ui_scene_screen(api, view_projection, hex_make(base.q, base.r), UI_LABEL_LIFT, &x, &y)) continue;
        f32 fraction = base.durability_max ? (f32)base.durability / (f32)base.durability_max : 1.0f;
        char name[UI_NAME_SIZE * 2];
        ui_draw_plate(draw, x, y, ui_base_name(ui, api, &base, name, sizeof(name)), base.troops, fraction, ui_faction_color(api, base.force), false);
    }
    for (u32 id = 0; id < api->unit_count(api->engine); ++id)
    {
        engine_unit unit = api->unit_at(api->engine, id);
        f32 x = 0.0f, y = 0.0f;
        if (unit.id == ENGINE_NO_UNIT || !ui_scene_screen(api, view_projection, ui_round_unit_hex(ui, &unit), UI_UNIT_PLATE_LIFT, &x, &y)) continue;   /* M20 */
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
        /* G2-054: during a target pick, the bar names the pick (以戰法進行攻擊). */
        if (ui->combat.pick != UI_PICK_NONE)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.96f, 0.86f, 0.42f, 1.0f), "    %s",
                               text_get(ui->text, ui->combat.pick == UI_PICK_TACTIC ? "pick.tactic" : "pick.target"));
        }
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
    u32 count;      /* built */
    u32 building;   /* builds in progress of the kind and the level */
};

/* The facilities of the city, built or in progress, one row for each kind
   and level, in the order of the facility table and the level. Returns
   the rows. */
internal u32
ui_count_facilities(engine_api *api, u8 city_id, ui_facility_count *out, u32 capacity)
{
    u32 rows = 0;
    engine_city_slots site = api->city_slots(api->engine, city_id);
    for (u32 slot = 0; slot < site.slot_count; ++slot)
    {
        engine_facility facility = api->facility_at(api->engine, city_id, (u8)slot);
        if (facility.city_id == ENGINE_NO_CITY || facility.facility == ENGINE_NO_FACILITY) continue;
        if (facility.state != SIM_SLOT_BUILT && facility.state != SIM_SLOT_BUILDING) continue;
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
            out[at].building = 0;
            row = at;
        }
        if (facility.state == SIM_SLOT_BUILT) out[row].count++;
        else                                  out[row].building++;
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
    char number[32];
    ui_format_facility_count(number, sizeof(number), site.built, site.building);
    ImGui::SeparatorText(text_get(ui->text, "city.facility_counts"));
    ImGui::Text("%s %s / %u", text_get(ui->text, "city.slots_used"), number, site.slot_count);
    if (ImGui::BeginTable("facility_counts", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
    {
        for (u32 row = 0; row < rows; ++row)
        {
            engine_facility_kind kind = api->facility_kind_at(api->engine, counts[row].facility);
            ui_format_facility_count(number, sizeof(number), counts[row].count, counts[row].building);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(ui_text_with_key(ui, "facility", kind.key));
            ImGui::TableNextColumn();
            ImGui::Text("Lv%u", counts[row].level);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(number);
        }
        ImGui::EndTable();
    }
}
