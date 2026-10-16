/* The hex map debug panel: draws every cell of the map through engine_api,
   lets the user pick two cells, and shows the path cost between them.
   Included by ui.cpp. */

#include <math.h>   /* cosf, sinf for the hexagon outline */

internal void
ui_map_init(ui_state *ui)
{
    ui->map.has_from = false;
    ui->map.has_to   = false;
    ui->map.zoom     = 12.0f;
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

internal void
ui_draw_hexagon(ImDrawList *draw, hex_point center, f32 size, ImU32 fill, ImU32 outline)
{
    ImVec2 points[6];
    for (u32 i = 0; i < 6; ++i)
    {
        f32 angle = (60.0f * (f32)i - 30.0f) * 3.14159265f / 180.0f;
        points[i] = ImVec2(center.x + size * cosf(angle), center.y + size * sinf(angle));
    }
    draw->AddConvexPolyFilled(points, 6, fill);
    draw->AddPolyline(points, 6, outline, ImDrawFlags_Closed, 1.0f);
}

internal void
ui_draw_map(ui_state *ui, engine_api *api)
{
    ImGui::SetNextWindowSize(ImVec2(640, 480), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Hex map", &ui->show_map))
    {
        engine_map_size size = api->map_size(api->engine);
        ImGui::Text("%u x %u cells", size.width, size.height);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::SliderFloat("zoom", &ui->map.zoom, 4.0f, 30.0f, "%.0f px");
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear selection")) { ui->map.has_from = ui->map.has_to = false; }

        if (ui->map.has_from) ImGui::Text("from (%d, %d)", ui->map.from.q, ui->map.from.r);
        else                  ImGui::Text("click a cell to pick 'from', another to pick 'to'");
        if (ui->map.has_to)
        {
            ImGui::SameLine();
            u32 cost = api->path_cost(api->engine, ui->map.from.q, ui->map.from.r, ui->map.to.q, ui->map.to.r);
            if (cost == HEX_NO_PATH) ImGui::Text("to (%d, %d): no path", ui->map.to.q, ui->map.to.r);
            else                     ImGui::Text("to (%d, %d): path cost %u", ui->map.to.q, ui->map.to.r, cost);
        }

        ImGui::BeginChild("map_canvas", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
        ImDrawList *draw = ImGui::GetWindowDrawList();
        f32 cell = ui->map.zoom;
        hex_point origin;
        origin.x = ImGui::GetCursorScreenPos().x + cell;
        origin.y = ImGui::GetCursorScreenPos().y + cell;

        for (u32 row = 0; row < size.height; ++row)
        {
            for (u32 col = 0; col < size.width; ++col)
            {
                hex_offset o;
                o.col = (s32)col;
                o.row = (s32)row;
                hex_coord h = hex_from_offset(o);
                u8 cost = api->cell_cost(api->engine, h.q, h.r);
                ImU32 outline = IM_COL32(20, 20, 20, 255);
                if (ui->map.has_from && hex_equal(h, ui->map.from)) outline = IM_COL32(255, 220, 60, 255);
                if (ui->map.has_to   && hex_equal(h, ui->map.to))   outline = IM_COL32(255, 90, 60, 255);
                ui_draw_hexagon(draw, hex_to_pixel(h, cell, origin), cell - 0.5f, ui_map_cell_color(cost), outline);
            }
        }

        /* Reserve the canvas so the child scrolls, and take clicks on it. */
        f32 canvas_w = cell * (1.7320508f * (f32)size.width + 2.0f);
        f32 canvas_h = cell * (1.5f * (f32)size.height + 2.0f);
        ImGui::InvisibleButton("cells", ImVec2(canvas_w, canvas_h));
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            hex_point mouse;
            mouse.x = ImGui::GetIO().MousePos.x;
            mouse.y = ImGui::GetIO().MousePos.y;
            hex_coord picked = hex_from_pixel(mouse, cell, origin);
            if (!ui->map.has_from || ui->map.has_to)
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
