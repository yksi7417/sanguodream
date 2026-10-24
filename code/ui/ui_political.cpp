/* M21 the political map 中地圖 and the hover panels (ui/information-
   screens.md "中地圖"; ui/main-map.md "Hover panels").
   - The 地圖 button of the HUD opens 中地圖 in the game mode. The map fills
     each region in the colour of the faction of its city, or in a shade of
     the soldiers, the gold or the grain of the city (the group 背景). The
     group 顯示 shows the cities as squares, the gates and the ports as
     diamonds, and the units as dots. 返回 closes the map.
   - A panel next to the mouse shows the facts of the city or the unit
     under it.
   Both command nothing. Included by ui.cpp after ui_information.cpp. */

enum ui_political_background
{
    UI_POLITICAL_FACTION,    /* 勢力 */
    UI_POLITICAL_SOLDIERS,   /* 士兵 */
    UI_POLITICAL_GOLD,       /* 資金 */
    UI_POLITICAL_GRAIN,      /* 兵糧 */
    UI_POLITICAL_BACKGROUNDS
};

#define UI_POLITICAL_MAP_SIZE   625.0f   /* information-screens.md: the map is 625 x 625 */
#define UI_POLITICAL_CITY_HALF  4.0f
#define UI_POLITICAL_BASE_HALF  3.5f
#define UI_POLITICAL_UNIT_RADIUS 2.5f
#define UI_HOVER_OFFSET         18.0f    /* px from the mouse to the hover panel */

/* The value of a city for a background of numbers. */
internal f32
ui_political_value(engine_city *city, int background)
{
    switch (background)
    {
        case UI_POLITICAL_SOLDIERS: return (f32)city->troops;
        case UI_POLITICAL_GOLD:     return (f32)city->gold;
        case UI_POLITICAL_GRAIN:    return (f32)city->food;
        default:                    return 0.0f;
    }
}

/* The fill of the region of each city: the colour of the owner, or a shade
   from dark to yellow for the share of the largest value. */
internal void
ui_political_city_fills(ui_state *ui, engine_api *api, ImU32 *fills, u32 capacity)
{
    f32 largest = 1.0f;
    u32 count = api->city_count(api->engine);
    for (u32 index = 0; index < count; ++index)
    {
        engine_city city = api->city_at(api->engine, index);
        f32 value = ui_political_value(&city, ui->political.background);
        if (value > largest) largest = value;
    }
    for (u32 index = 0; index < capacity; ++index) fills[index] = IM_COL32(70, 70, 70, 255);
    for (u32 index = 0; index < count; ++index)
    {
        engine_city city = api->city_at(api->engine, index);
        if (city.city_id >= capacity) continue;
        if (ui->political.background == UI_POLITICAL_FACTION)
        {
            fills[city.city_id] = city.owner_force_id == ENGINE_NO_FACTION ? IM_COL32(150, 150, 150, 255) : ui_faction_color(city.owner_force_id);
            continue;
        }
        f32 share = ui_political_value(&city, ui->political.background) / largest;
        fills[city.city_id] = IM_COL32((int)(40.0f + 210.0f * share), (int)(40.0f + 170.0f * share), 40, 255);
    }
}

/* The screen point of a hex on the map: offset rows, every second row half
   a cell to the right. */
internal ImVec2
ui_political_point(ImVec2 origin, f32 cell, hex_coord hex)
{
    hex_offset o = hex_to_offset(hex);
    return ImVec2(origin.x + ((f32)o.col + ((o.row & 1) ? 0.5f : 0.0f)) * cell, origin.y + (f32)o.row * cell);
}

internal void
ui_draw_political_regions(ui_state *ui, engine_api *api, ImDrawList *draw, ImVec2 origin, f32 cell)
{
    engine_region_view regions = api->region_view(api->engine);
    engine_terrain_view terrain = api->terrain_view(api->engine);
    ImU32 fills[256];
    ui_political_city_fills(ui, api, fills, ArrayCount(fills));
    for (u32 row = 0; row < regions.height; ++row)
    {
        for (u32 col = 0; col < regions.width; ++col)
        {
            u32 cell_index = row * regions.width + col;
            u8 city = regions.region ? regions.region_city[regions.region[cell_index]] : (u8)ENGINE_NO_CITY;
            ImU32 fill;
            if (city != ENGINE_NO_CITY)                                      fill = fills[city];
            else if (terrain.tile && col < terrain.width && row < terrain.height) fill = ui_map_tile_color(terrain.tile[cell_index]);
            else                                                             fill = IM_COL32(30, 50, 80, 255);
            f32 x = origin.x + ((f32)col + ((row & 1) ? 0.5f : 0.0f)) * cell, y = origin.y + (f32)row * cell;
            draw->AddRectFilled(ImVec2(x, y), ImVec2(x + cell + 0.5f, y + cell + 0.5f), fill);
        }
    }
}

/* The marks of 顯示: cities, gates and ports, units. */
internal void
ui_draw_political_marks(ui_state *ui, engine_api *api, ImDrawList *draw, ImVec2 origin, f32 cell)
{
    void *e = api->engine;
    if (ui->political.cities)
    {
        for (u32 index = 0; index < api->city_count(e); ++index)
        {
            engine_city city = api->city_at(e, index);
            engine_city_slots site = api->city_slots(e, city.city_id);
            ImVec2 point = ui_political_point(origin, cell, hex_make(site.q, site.r));
            f32 half = UI_POLITICAL_CITY_HALF;
            draw->AddRectFilled(ImVec2(point.x - half, point.y - half), ImVec2(point.x + half, point.y + half), ui_faction_color(city.owner_force_id));
            draw->AddRect(ImVec2(point.x - half, point.y - half), ImVec2(point.x + half, point.y + half), IM_COL32(255, 255, 255, 255));
        }
    }
    if (ui->political.bases)
    {
        for (u32 index = 0; index < api->base_count(e); ++index)
        {
            engine_base base = api->base_at(e, index);
            if (base.id == ENGINE_NO_ROW) continue;
            ImVec2 point = ui_political_point(origin, cell, hex_make(base.q, base.r));
            f32 half = UI_POLITICAL_BASE_HALF;
            ImVec2 diamond[4] = { ImVec2(point.x, point.y - half), ImVec2(point.x + half, point.y), ImVec2(point.x, point.y + half), ImVec2(point.x - half, point.y) };
            draw->AddConvexPolyFilled(diamond, 4, ui_faction_color(base.force));
            draw->AddPolyline(diamond, 4, IM_COL32(255, 255, 255, 255), ImDrawFlags_Closed, 1.0f);
        }
    }
    if (ui->political.units)
    {
        for (u32 id = 0; id < api->unit_count(e); ++id)
        {
            engine_unit unit = api->unit_at(e, id);
            if (unit.id == ENGINE_NO_UNIT) continue;
            draw->AddCircleFilled(ui_political_point(origin, cell, hex_make(unit.q, unit.r)), UI_POLITICAL_UNIT_RADIUS, ui_faction_color(unit.force_id));
        }
    }
}

/* The control column: the groups 背景 and 顯示, and 返回. */
internal void
ui_draw_political_controls(ui_state *ui)
{
    static const char *backgrounds[UI_POLITICAL_BACKGROUNDS][2] = { { "political.faction", "bg_faction" }, { "command.soldiers", "bg_soldiers" },
                                                                    { "command.gold", "bg_gold" }, { "command.grain", "bg_grain" } };
    char label[96];
    ImGui::BeginGroup();
    ImGui::SeparatorText(text_get(ui->text, "political.background"));
    for (int index = 0; index < UI_POLITICAL_BACKGROUNDS; ++index)
    {
        if (ImGui::RadioButton(ui_label(label, sizeof(label), ui, backgrounds[index][0], backgrounds[index][1]), ui->political.background == index))
            ui->political.background = index;
    }
    ImGui::SeparatorText(text_get(ui->text, "political.show"));
    ImGui::Checkbox(ui_label(label, sizeof(label), ui, "political.cities", "show_cities"), &ui->political.cities);
    ImGui::Checkbox(ui_label(label, sizeof(label), ui, "political.bases", "show_bases"), &ui->political.bases);
    ImGui::Checkbox(ui_label(label, sizeof(label), ui, "political.units", "show_units"), &ui->political.units);
    ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeight()));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.45f, 0.1f, 1.0f));
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "command.back", "political_back"))) ui->show_political = false;
    ImGui::PopStyleColor();
    ImGui::EndGroup();
}

internal void
ui_draw_political(ui_state *ui, engine_api *api)
{
    ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    char title[96];
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "political.title", "PoliticalMap"), &ui->show_political, flags))
    {
        ui_context_track(ui);
        ui_draw_political_controls(ui);
        ImGui::SameLine();
        engine_map_size size = api->map_size(api->engine);
        f32 side = (f32)(size.width > size.height ? size.width : size.height);
        f32 cell = side > 0.0f ? UI_POLITICAL_MAP_SIZE / (side + 0.5f) : 1.0f;
        ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("###political_map", ImVec2(UI_POLITICAL_MAP_SIZE, UI_POLITICAL_MAP_SIZE));
        ImDrawList *draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(origin, ImVec2(origin.x + UI_POLITICAL_MAP_SIZE, origin.y + UI_POLITICAL_MAP_SIZE), IM_COL32(20, 40, 70, 255));
        ui_draw_political_regions(ui, api, draw, origin, cell);
        ui_draw_political_marks(ui, api, draw, origin, cell);
    }
    ImGui::End();
}

/* ---- the hover panels ------------------------------------------------------ */

internal void
ui_hover_city(ui_state *ui, engine_api *api, u8 city_id)
{
    engine_city city = api->city_at(api->engine, city_id);
    engine_city_economy economy = api->city_economy(api->engine, city_id);
    engine_city_siege siege = api->city_siege(api->engine, city_id);
    char name[UI_NAME_SIZE * 2], force[UI_NAME_SIZE * 2], governor[UI_NAME_SIZE * 2];
    ui_city_label(ui, api, city_id, name, sizeof(name));
    ui_code_text(ui, api, UI_CODE_FORCE, city.owner_force_id, force, sizeof(force));
    ui_code_text(ui, api, UI_CODE_OFFICER, city.commanding_officer_id, governor, sizeof(governor));
    ImGui::TextColored(ImVec4(0.62f, 0.82f, 0.96f, 1.0f), "%s", name);
    ImGui::ColorButton("###hover_colour", ImGui::ColorConvertU32ToFloat4(ui_faction_color(city.owner_force_id)), ImGuiColorEditFlags_NoTooltip, ImVec2(10.0f, 10.0f));
    ImGui::SameLine();
    ImGui::TextUnformatted(force);
    ImGui::Text("%s %s", text_get(ui->text, "info.col.governor"), governor);
    ImGui::Text("%s %u   %s %u", text_get(ui->text, "command.gold"), city.gold, text_get(ui->text, "command.grain"), city.food);
    ImGui::Text("%s %u / 100   %s %u / %u", text_get(ui->text, "command.order"), economy.public_order, text_get(ui->text, "city.durability"),
                siege.durability, siege.durability_max);
    ImGui::Text("%s %u", text_get(ui->text, "command.soldiers"), economy.soldiers);
    u16 free_ids[UI_DEVELOP_OFFICER_LIST];
    u32 free_count = ui_free_officers(api, city_id, free_ids, ArrayCount(free_ids)), total = 0;
    for (u32 index = 0; index < api->officer_count(api->engine); ++index)
    {
        engine_officer officer = api->officer_at(api->engine, index);
        total += officer.current_position == city_id && officer.status <= 3 && officer.force_id == city.owner_force_id;
    }
    ImGui::Text("%s %u / %u", text_get(ui->text, "city.active"), free_count, total);
}

internal void
ui_hover_unit(ui_state *ui, engine_api *api, u32 unit_id)
{
    engine_unit unit = api->unit_at(api->engine, unit_id);
    char name[UI_NAME_SIZE * 4], force[UI_NAME_SIZE * 2];
    ui_unit_label(ui, api, &unit, name, sizeof(name));
    ui_force_text(ui, api, unit.force_id, force, sizeof(force));
    ImGui::TextColored(ImVec4(0.62f, 0.82f, 0.96f, 1.0f), "%s", name);
    ImGui::ColorButton("###hover_colour", ImGui::ColorConvertU32ToFloat4(ui_faction_color(unit.force_id)), ImGuiColorEditFlags_NoTooltip, ImVec2(10.0f, 10.0f));
    ImGui::SameLine();
    ImGui::TextUnformatted(force);
    for (u32 place = 1; place < 3; ++place)   /* 副將 */
    {
        char deputy[UI_NAME_SIZE * 2];
        ui_code_text(ui, api, UI_CODE_OFFICER, unit.officers[place], deputy, sizeof(deputy));
        ImGui::Text("%s %s", text_get(ui->text, "hover.deputy"), deputy);
    }
    ImGui::Text("%s %u   %s %u", text_get(ui->text, "command.gold"), unit.gold, text_get(ui->text, "command.grain"), unit.grain);
    ImGui::Text("%s %u   %s %u", text_get(ui->text, "command.soldiers"), unit.soldiers, text_get(ui->text, "command.morale"), unit.morale);
    engine_unit_type type = api->unit_type_at(api->engine, unit.unit_type);
    ImGui::Text("%s %s %s", text_get(ui->text, "unit.class"), type.id == ENGINE_NO_UNIT_TYPE ? "-" : ui_text_with_key(ui, "unit_type", type.key),
                ui_grade_text(unit.values.aptitude));
}

/* ui/main-map.md, "Hover panels": a small panel next to the city or the
   unit under the mouse. It takes no input. */
/* G3-020: the hover panel of a facility: its name and level, its owner and
   its durability as current / maximum (the original: 孫策軍, 耐久 1000/1000). */
internal void
ui_hover_facility(ui_state *ui, engine_api *api, u8 city_id, u8 slot)
{
    void *e = api->engine;
    engine_facility facility = api->facility_at(e, city_id, slot);
    if (facility.facility == ENGINE_NO_FACILITY) return;
    char owner[UI_NAME_SIZE * 2] = "-";
    engine_city city = api->city_at(e, city_id);
    if (city.owner_force_id != ENGINE_NO_FACTION) ui_force_text(ui, api, city.owner_force_id, owner, sizeof(owner));
    ImGui::Text("%s Lv%u", ui_text_with_key(ui, "facility", api->facility_kind_at(e, facility.facility).key), facility.level);
    ImGui::Text("%s%s", owner, text_get(ui->text, "hover.army"));
    ImGui::Text("%s %u/%u", text_get(ui->text, "city.durability"), facility.durability, facility.durability_max);
}

/* U-001 (play report 2026-10-24): the hover panel of a field building:
   its name, its owner and its durability as current / maximum, as G3-020
   does for a facility. A trap without an owner shows "-". */
internal void
ui_hover_building(ui_state *ui, engine_api *api, u32 building_id)
{
    void *e = api->engine;
    engine_building building = api->building_at(e, building_id);
    if (building.id == ENGINE_NO_BUILDING) return;
    char owner[UI_NAME_SIZE * 2] = "-";
    if (building.force_id != ENGINE_NO_FACTION) ui_force_text(ui, api, building.force_id, owner, sizeof(owner));
    ImGui::TextUnformatted(ui_text_with_key(ui, "building", api->building_kind_at(e, building.kind).key));
    if (building.force_id != ENGINE_NO_FACTION) ImGui::Text("%s%s", owner, text_get(ui->text, "hover.army"));
    else                                        ImGui::TextUnformatted(owner);
    ImGui::Text("%s %u/%u", text_get(ui->text, "city.durability"), building.durability, building.durability_max);
}

internal void
ui_draw_hover_panel(ui_state *ui, engine_api *api)
{
    ImGuiIO &io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;
    /* G3-006, G3-021: no hover panel during a target pick, a strike or a duel. */
    if (ui->combat.pick != UI_PICK_NONE || ui->war.strike_count > 0 || api->duel_view(api->engine).active) return;
    /* The plate of a unit (the portrait and the name) counts as its hex. */
    u32 plated = ui_plate_unit_at(ui, io.MousePos.x, io.MousePos.y);
    engine_hex data = {};
    data.unit_id = ENGINE_NO_UNIT;
    data.city_id = ENGINE_NO_CITY;
    if (plated != ENGINE_NO_UNIT)          data.unit_id = plated;
    else if (ui->scene.has_hover)          data = api->hex_at(api->engine, ui->scene.hover.q, ui->scene.hover.r);
    b32 facility_hex = data.unit_id == ENGINE_NO_UNIT && data.city_id == ENGINE_NO_CITY && data.slot_city != ENGINE_NO_CITY && data.facility != ENGINE_NO_FACILITY;
    b32 building_hex = data.unit_id == ENGINE_NO_UNIT && data.city_id == ENGINE_NO_CITY && !facility_hex && plated == ENGINE_NO_UNIT &&
                       data.building_id != ENGINE_NO_BUILDING;   /* U-001 */
    if (data.unit_id == ENGINE_NO_UNIT && data.city_id == ENGINE_NO_CITY && !facility_hex && !building_hex) return;
    /* G3-015: while the window of a city is open, the hover panel of a city stays closed. The
       ghost text of the report was this panel under the mouse, over the window that opened. */
    if (data.unit_id == ENGINE_NO_UNIT && data.city_id != ENGINE_NO_CITY && ui->context.kind == UI_CONTEXT_CITY) return;
    ImGui::SetNextWindowPos(ImVec2(io.MousePos.x + UI_HOVER_OFFSET, io.MousePos.y + UI_HOVER_OFFSET), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.9f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings;
    const char *window = data.unit_id != ENGINE_NO_UNIT ? "###HoverUnit" :
                         (facility_hex ? "###HoverFacility" : (building_hex ? "###HoverBuilding" : "###HoverCity"));
    if (ImGui::Begin(window, 0, flags))
    {
        if (data.unit_id != ENGINE_NO_UNIT) ui_hover_unit(ui, api, data.unit_id);
        else if (facility_hex)              ui_hover_facility(ui, api, data.slot_city, data.slot_index);
        else if (building_hex)              ui_hover_building(ui, api, data.building_id);
        else                                ui_hover_city(ui, api, data.city_id);
    }
    ImGui::End();
}
