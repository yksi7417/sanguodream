/* The hex map debug panel: draws every cell of the map through engine_api,
   lets the user pick two cells, and shows the path cost between them.
   Included by ui.cpp. */

#include <math.h>   /* cosf, sinf for the hexagon outline */

/* ui_march.cpp: the units and cities on the map, and the clicks for a unit. */
internal void ui_draw_map_units(ui_state *ui, engine_api *api, ImDrawList *draw, f32 cell, hex_point origin);
internal b32  ui_unit_click(ui_state *ui, engine_api *api, hex_coord hex);
internal void ui_draw_hex_lines(ui_state *ui, engine_api *api, hex_coord hex);   /* ui_locate.cpp (M11) */

#define UI_MAP_ZOOM_MIN         4.0f
#define UI_MAP_ZOOM_MAX         30.0f
#define UI_MAP_KEY_ZOOM_STEP    2.0f     /* pixels of cell size for one PageUp or PageDown */
#define UI_MAP_KEY_SCROLL_SPEED 600.0f   /* pixels each second while W, A, S or D is down */

internal void
ui_map_init(ui_state *ui)
{
    ui->map.has_from = false;
    ui->map.has_to   = false;
    ui->map.zoom     = 12.0f;
    ui->map.focused  = false;
    ui->map.has_city = false;
    ui->map.city     = 0;
}

/* The keys of the Hex map panel while it has the focus: PageUp and
   PageDown change the zoom; W, A, S and D scroll the canvas (in the child). */
internal void
ui_map_handle_zoom_keys(ui_state *ui)
{
    ui->map.focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (!ui->map.focused || ImGui::GetIO().WantTextInput) return;
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp))   ui->map.zoom = Minimum(ui->map.zoom + UI_MAP_KEY_ZOOM_STEP, UI_MAP_ZOOM_MAX);
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) ui->map.zoom = Maximum(ui->map.zoom - UI_MAP_KEY_ZOOM_STEP, UI_MAP_ZOOM_MIN);
}

internal void
ui_map_handle_scroll_keys(ui_state *ui)
{
    ImGuiIO &io = ImGui::GetIO();
    if (!ui->map.focused || io.WantTextInput) return;
    f32 step = UI_MAP_KEY_SCROLL_SPEED * io.DeltaTime;
    if (ImGui::IsKeyDown(ImGuiKey_W)) ImGui::SetScrollY(ImGui::GetScrollY() - step);
    if (ImGui::IsKeyDown(ImGuiKey_S)) ImGui::SetScrollY(ImGui::GetScrollY() + step);
    if (ImGui::IsKeyDown(ImGuiKey_A)) ImGui::SetScrollX(ImGui::GetScrollX() - step);
    if (ImGui::IsKeyDown(ImGuiKey_D)) ImGui::SetScrollX(ImGui::GetScrollX() + step);
}

internal ImU32
ui_map_cell_color(u8 cost)
{
    switch (cost)
    {
        case HEX_IMPASSABLE: return IM_COL32(40, 60, 110, 255);
        case 1:              return IM_COL32(90, 140, 70, 255);
        case 2:              return IM_COL32(60, 110, 60, 255);
        case 3:              return IM_COL32(120, 100, 60, 255);
        default:             return IM_COL32(110, 110, 110, 255);
    }
}

/* The colour of an atlas tile of terrain.tsv (the same tiles as the scene). */
internal ImU32
ui_map_tile_color(u8 tile)
{
    switch (tile)
    {
        case 0:  return IM_COL32(40, 70, 130, 255);    /* water */
        case 1:  return IM_COL32(95, 145, 70, 255);    /* grass, earth */
        case 2:  return IM_COL32(50, 100, 55, 255);    /* forest, swamp */
        case 3:  return IM_COL32(140, 120, 80, 255);   /* waste */
        case 4:  return IM_COL32(110, 100, 95, 255);   /* cliff */
        case 5:  return IM_COL32(200, 180, 120, 255);  /* roads, bases */
        case 6:  return IM_COL32(205, 195, 140, 255);  /* sand, shore */
        case 7:  return IM_COL32(150, 90, 150, 255);   /* poison spring */
        default: return IM_COL32(110, 110, 110, 255);
    }
}

internal void
ui_draw_hexagon(ImDrawList *draw, hex_point center, f32 size, ImU32 fill, ImU32 outline)
{
    ImVec2 points[6];
    for (u32 i = 0; i < 6; ++i)
    {
        f32 angle = (60.0f * (f32)i) * 3.14159265f / 180.0f;   /* flat-top: a corner points east */
        points[i] = ImVec2(center.x + size * cosf(angle), center.y + size * sinf(angle));
    }
    draw->AddConvexPolyFilled(points, 6, fill);
    draw->AddPolyline(points, 6, outline, ImDrawFlags_Closed, 1.0f);
}

internal void
ui_draw_map(ui_state *ui, engine_api *api)
{
    ImGui::SetNextWindowSize(ImVec2(640, 480), ImGuiCond_FirstUseEver);
    ui_context_place(ui, 0);   /* the game mode: the dialog opens next to its entity */
    if (ImGui::Begin("Hex map", &ui->show_map))
    {
        ui_context_track(ui);
        ui_map_handle_zoom_keys(ui);
        engine_map_size size = api->map_size(api->engine);
        ImGui::Text("%u x %u cells", size.width, size.height);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::SliderFloat("zoom", &ui->map.zoom, UI_MAP_ZOOM_MIN, UI_MAP_ZOOM_MAX, "%.0f px");
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear selection")) { ui->map.has_from = ui->map.has_to = false; }

        if (ui->map.has_from) ImGui::Text("from (%d, %d)", ui->map.from.q, ui->map.from.r);
        else                  ImGui::Text("click a cell to pick 'from', another to pick 'to'; W A S D scroll, PageUp PageDown zoom");
        if (ui->map.has_to)
        {
            ImGui::SameLine();
            u32 cost = api->path_cost(api->engine, ui->map.from.q, ui->map.from.r, ui->map.to.q, ui->map.to.r);
            if (cost == HEX_NO_PATH) ImGui::Text("to (%d, %d): no path", ui->map.to.q, ui->map.to.r);
            else                     ImGui::Text("to (%d, %d): path cost %u", ui->map.to.q, ui->map.to.r, cost);
        }

        /* M10, M11: what the picked hex is (the lines of the Hex info box). */
        if (ui->map.has_from && ImGui::TreeNodeEx("hex_lines", ImGuiTreeNodeFlags_DefaultOpen, "%s", text_get(ui->text, "hexinfo.title")))
        {
            ui_draw_hex_lines(ui, api, ui->map.from);
            ImGui::TreePop();
        }
        if (ui->map.has_city)
        {
            ImGui::Text("city %u: its slots have a yellow frame", ui->map.city);
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear city")) ui->map.has_city = false;
        }

        ImGui::BeginChild("map_canvas", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
        ui_map_handle_scroll_keys(ui);
        ImDrawList *draw = ImGui::GetWindowDrawList();
        f32 cell = ui->map.zoom;
        /* M11: a framed city or unit: put its hex in the middle of the canvas. */
        if (ui->map.scroll_request)
        {
            hex_point zero = { cell, cell };
            hex_point at = hex_to_pixel(ui->map.scroll_to, cell, zero);
            ImVec2 view = ImGui::GetContentRegionAvail();
            ImGui::SetScrollX(Maximum(0.0f, at.x - view.x * 0.5f));
            ImGui::SetScrollY(Maximum(0.0f, at.y - view.y * 0.5f));
            ui->map.scroll_request = false;
        }
        hex_point origin;
        origin.x = ImGui::GetCursorScreenPos().x + cell;
        origin.y = ImGui::GetCursorScreenPos().y + cell;

        /* The real map has 40,000 hexes: draw only the hexes in the clip
           rectangle of the canvas. */
        engine_terrain_view view = api->terrain_view(api->engine);
        ImVec2 clip_min = draw->GetClipRectMin(), clip_max = draw->GetClipRectMax();
        for (u32 row = 0; row < size.height; ++row)
        {
            for (u32 col = 0; col < size.width; ++col)
            {
                hex_offset o;
                o.col = (s32)col;
                o.row = (s32)row;
                hex_coord h = hex_from_offset(o);
                hex_point center = hex_to_pixel(h, cell, origin);
                if (center.x + cell < clip_min.x || center.x - cell > clip_max.x || center.y + cell < clip_min.y || center.y - cell > clip_max.y) continue;
                ImU32 fill;
                if (view.tile && col < view.width && row < view.height) fill = ui_map_tile_color(view.tile[row * view.width + col]);
                else                                                    fill = ui_map_cell_color(api->cell_cost(api->engine, h.q, h.r));
                ImU32 outline = IM_COL32(20, 20, 20, 255);
                if (ui->map.has_from && hex_equal(h, ui->map.from)) outline = IM_COL32(255, 220, 60, 255);
                if (ui->map.has_to   && hex_equal(h, ui->map.to))   outline = IM_COL32(255, 90, 60, 255);
                ui_draw_hexagon(draw, center, cell - 0.5f, fill, outline);
            }
        }

        /* M10: the slots of the selected city (開發地 of the map). */
        if (ui->map.has_city)
        {
            engine_city_slots slots = api->city_slots(api->engine, ui->map.city);
            for (u32 slot = 0; slot < slots.slot_count; ++slot)
            {
                engine_facility facility = api->facility_at(api->engine, ui->map.city, (u8)slot);
                if (facility.city_id == ENGINE_NO_CITY) continue;
                ui_draw_hexagon(draw, hex_to_pixel(hex_make(facility.q, facility.r), cell, origin), cell - 1.5f, IM_COL32(0, 0, 0, 0), IM_COL32(255, 230, 40, 255));
            }
        }

        ui_draw_map_units(ui, api, draw, cell, origin);

        /* Reserve the canvas so the child scrolls, and take clicks on it.
           Flat-top hexes: 1.5 cells for each column, sqrt(3) cells for each row. */
        f32 canvas_w = cell * (1.5f * (f32)size.width + 2.0f);
        f32 canvas_h = cell * (1.7320508f * ((f32)size.height + 0.5f) + 2.0f);
        ImGui::InvisibleButton("cells", ImVec2(canvas_w, canvas_h));
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            hex_point mouse;
            mouse.x = ImGui::GetIO().MousePos.x;
            mouse.y = ImGui::GetIO().MousePos.y;
            hex_coord picked = hex_from_pixel(mouse, cell, origin);
            engine_hex picked_hex = api->hex_at(api->engine, picked.q, picked.r);
            if (picked_hex.city_id != ENGINE_NO_CITY)
            {
                ui->map.has_city = true;
                ui->map.city     = picked_hex.city_id;
            }
            if (ui_unit_click(ui, api, picked))
            {
                /* the click selected, moved or opened the menu of a unit */
            }
            else if (!ui->map.has_from || ui->map.has_to)
            {
                ui->map.from     = picked;
                ui->map.has_from = true;
                ui->map.has_to   = false;
            }
            else
            {
                ui->map.to     = picked;
                ui->map.has_to = true;
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();
}
