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
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "hexinfo.title", "HexInfo"), &open, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ui_draw_hex_lines(ui, api, ui->scene.pick);
    }
    ImGui::End();
    if (!open) ui->scene.has_pick = false;
}

/* ---- the names over the cities, the units under the mouse ---------------------------- */

/* A marker and the name of each city over its flower, and the name of the
   unit under the mouse. They draw on the background, under the panels. */
internal void
ui_draw_scene_labels(ui_state *ui, engine_api *api)
{
    if (!ui->scene.enabled || !ui->scene.initialized) return;
    ImGuiIO &io = ImGui::GetIO();
    f32 width = io.DisplaySize.x, height = io.DisplaySize.y;
    if (width <= 0.0f || height <= 0.0f) return;
    m4 view_projection = camera_view_projection(&ui->scene.cam, width, height);
    ImDrawList *draw = ImGui::GetBackgroundDrawList();
    for (u32 index = 0; index < api->city_count(api->engine); ++index)
    {
        engine_city city = api->city_at(api->engine, index);
        engine_city_slots site = api->city_slots(api->engine, city.city_id);
        if (site.city_id == ENGINE_NO_CITY) continue;
        v3 above = ui_hex_world(api, hex_make(site.q, site.r));
        above.y += UI_LABEL_LIFT;
        f32 x = 0.0f, y = 0.0f;
        if (!camera_project(view_projection, above, width, height, &x, &y) || x < 0.0f || y < 0.0f || x > width || y > height) continue;
        char name[UI_NAME_SIZE * 2];
        ui_city_label(ui, api, city.city_id, name, sizeof(name));
        ImVec2 size = ImGui::CalcTextSize(name);
        bool selected = city.city_id == ui->city.selected;
        draw->AddCircleFilled(ImVec2(x, y), selected ? 7.0f : 5.0f, ui_faction_color(city.owner_force_id));
        draw->AddCircle(ImVec2(x, y), selected ? 8.0f : 6.0f, IM_COL32(255, 255, 255, 230), 0, selected ? 2.5f : 1.5f);
        ImVec2 text(x - size.x * 0.5f, y - 10.0f - size.y);
        draw->AddRectFilled(ImVec2(text.x - 3.0f, text.y - 1.0f), ImVec2(text.x + size.x + 3.0f, text.y + size.y + 1.0f), IM_COL32(0, 0, 0, 150), 3.0f);
        draw->AddText(text, selected ? IM_COL32(255, 230, 60, 255) : IM_COL32(255, 255, 255, 255), name);
    }
    if (ui->scene.has_hover && !io.WantCaptureMouse)
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
