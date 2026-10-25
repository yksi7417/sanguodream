/* The mini map (M12, ui/main-map.md "Mini map"): a round map of the whole
   continent inside a compass ring. The map turns with the camera, so its
   top is the forward direction of the main view, and the ring shows 北, 東,
   南 and 西 at their places. Each city is a square in the colour of its
   owner, each gate and port a hollow square, and each unit a diamond. The
   roads (主徑) are lines. A white frame shows the ground that the main
   view shows. A click or a drag on the disc moves the camera to that point.
   Included by ui.cpp after ui_locate.cpp; it calls engine_api only. */

#define UI_MINIMAP_FILL        0.98f   /* the half diagonal of the map over the radius of the disc */
#define UI_MINIMAP_RING_WIDTH  16.0f   /* px: the band of the compass ring round the disc */
#define UI_MINIMAP_RADIUS_MIN  40.0f   /* px */
#define UI_MINIMAP_CITY_HALF   3.0f    /* px: half the side of a city square */
#define UI_MINIMAP_BASE_HALF   2.0f    /* px: half the side of a gate or port square */
#define UI_MINIMAP_UNIT_HALF   2.5f    /* px: half the diagonal of a unit diamond */
#define UI_MINIMAP_ROAD_KIND   2       /* the road table: a road hex */
#define UI_MINIMAP_ROAD_END    1       /* the road table: a base hex that a road goes into */
#define UI_MINIMAP_MARGIN      8.0f    /* px from the bottom right corner of the screen in the game mode */

/* Where the disc is on the screen and how the map fits in it. */
struct ui_minimap_frame
{
    camera *cam;
    ImVec2  centre;     /* px: the centre of the disc */
    f32     radius;     /* px: the radius of the disc */
    f32     scale;      /* px for each world unit */
    v3      middle;     /* the world point at the centre of the disc: the centre of the map */
};

internal ImVec2
ui_minimap_screen(ui_minimap_frame *frame, f32 world_x, f32 world_z)
{
    f32 mx = 0.0f, my = 0.0f;
    camera_ground_to_minimap(frame->cam, world_x - frame->middle.x, world_z - frame->middle.z, &mx, &my);
    return ImVec2(frame->centre.x + mx * frame->scale, frame->centre.y + my * frame->scale);
}

internal ImVec2
ui_minimap_hex(ui_minimap_frame *frame, hex_coord hex)
{
    v3 world = hex_world_center(hex, 0.0f);
    return ui_minimap_screen(frame, world.x, world.z);
}

internal bool
ui_minimap_inside(ui_minimap_frame *frame, ImVec2 point)
{
    f32 dx = point.x - frame->centre.x, dy = point.y - frame->centre.y;
    return dx * dx + dy * dy <= frame->radius * frame->radius;
}

internal bool
ui_minimap_on_map(engine_map_size size, hex_coord hex)
{
    hex_offset o = hex_to_offset(hex);
    return o.col >= 0 && o.row >= 0 && (u32)o.col < size.width && (u32)o.row < size.height;
}

/* The disc fits the whole map at every heading: its centre is the centre
   of the map, and the half diagonal of the map is UI_MINIMAP_FILL of the
   radius. */
internal ui_minimap_frame
ui_minimap_fit(camera *cam, engine_map_size size, ImVec2 centre, f32 radius)
{
    hex_offset first = { 0, 0 };
    hex_offset last  = { size.width ? (s32)size.width - 1 : 0, size.height ? (s32)size.height - 1 : 0 };
    v3 low  = hex_world_center(hex_from_offset(first), 0.0f);
    v3 high = hex_world_center(hex_from_offset(last), 0.0f);
    f32 half_x = fabsf(high.x - low.x) * 0.5f + 1.0f;
    f32 half_z = fabsf(high.z - low.z) * 0.5f + 1.0f;
    ui_minimap_frame frame;
    frame.cam    = cam;
    frame.centre = centre;
    frame.radius = radius;
    frame.scale  = radius * UI_MINIMAP_FILL / sqrtf(half_x * half_x + half_z * half_z);
    frame.middle = v3_make((low.x + high.x) * 0.5f, 0.0f, (low.z + high.z) * 0.5f);
    return frame;
}

/* The terrain colour of each hex inside the disc. */
internal void
ui_draw_minimap_terrain(ui_minimap_frame *frame, engine_api *api, ImDrawList *draw, engine_map_size size, engine_terrain_view view)
{
    f32 half = frame->scale * 0.95f;   /* the squares overlap at every heading */
    if (half < 0.6f) half = 0.6f;
    for (u32 row = 0; row < size.height; ++row)
    {
        for (u32 col = 0; col < size.width; ++col)
        {
            hex_offset o; o.col = (s32)col; o.row = (s32)row;
            hex_coord hex = hex_from_offset(o);
            ImVec2 point = ui_minimap_hex(frame, hex);
            if (!ui_minimap_inside(frame, point)) continue;
            ImU32 fill;
            if (view.tile && col < view.width && row < view.height) fill = ui_map_tile_color(view.tile[row * view.width + col]);
            else                                                    fill = ui_map_cell_color(api->cell_cost(api->engine, hex.q, hex.r));
            draw->AddRectFilled(ImVec2(point.x - half, point.y - half), ImVec2(point.x + half, point.y + half), fill);
        }
    }
}

/* The roads: a line between each two neighbour hexes of 主徑, and from a
   road into a city, a gate or a port. Each pair draws once: the loop takes
   only the first three directions of each hex. */
internal void
ui_draw_minimap_roads(ui_minimap_frame *frame, engine_api *api, ImDrawList *draw, engine_terrain_view view)
{
    if (!view.kind) return;
    u8 road[256] = {};
    for (u32 kind = 0; kind < api->terrain_kind_count(api->engine) && kind < 256; ++kind)
    {
        engine_terrain_kind terrain = api->terrain_kind_at(api->engine, kind);
        if (strcmp(terrain.key, "main_road") == 0) road[kind] = UI_MINIMAP_ROAD_KIND;
        else if (strcmp(terrain.key, "city") == 0 || strcmp(terrain.key, "port") == 0 || strcmp(terrain.key, "gate") == 0) road[kind] = UI_MINIMAP_ROAD_END;
    }
    engine_map_size size; size.width = view.width; size.height = view.height;
    for (u32 row = 0; row < view.height; ++row)
    {
        for (u32 col = 0; col < view.width; ++col)
        {
            u8 here = road[view.kind[row * view.width + col]];
            if (!here) continue;
            hex_offset o; o.col = (s32)col; o.row = (s32)row;
            hex_coord hex = hex_from_offset(o);
            for (u32 direction = 0; direction < HEX_DIRECTION_COUNT / 2; ++direction)
            {
                hex_coord next = hex_neighbor(hex, direction);
                if (!ui_minimap_on_map(size, next)) continue;
                hex_offset next_offset = hex_to_offset(next);
                u8 there = road[view.kind[(u32)next_offset.row * view.width + (u32)next_offset.col]];
                if (!there || (here != UI_MINIMAP_ROAD_KIND && there != UI_MINIMAP_ROAD_KIND)) continue;
                ImVec2 from = ui_minimap_hex(frame, hex), to = ui_minimap_hex(frame, next);
                if (!ui_minimap_inside(frame, from) || !ui_minimap_inside(frame, to)) continue;
                draw->AddLine(from, to, IM_COL32(235, 205, 130, 255), 1.0f);
            }
        }
    }
}

/* The cities as squares in the colour of the owner (the owner of the
   loaded save), with the border of the diplomacy of M7; the gates and the
   ports as hollow squares; the units as diamonds. */
internal void
ui_draw_minimap_markers(ui_minimap_frame *frame, ui_state *ui, engine_api *api, ImDrawList *draw)
{
    u8 acting = api->whose_turn(api->engine);
    for (u32 index = 0; index < api->city_count(api->engine); ++index)
    {
        engine_city city = api->city_at(api->engine, index);
        engine_city_slots slots = api->city_slots(api->engine, city.city_id);
        ImVec2 point = ui_minimap_hex(frame, hex_make(slots.q, slots.r));
        if (!ui_minimap_inside(frame, point)) continue;
        f32 half = UI_MINIMAP_CITY_HALF;
        draw->AddRectFilled(ImVec2(point.x - half, point.y - half), ImVec2(point.x + half, point.y + half), ui_faction_color(city.owner_force_id));
        /* M7 (diplomacy.md, manual p.8): an ally in blue, a ceasefire in yellow, a hostile faction in red. */
        ImU32 border = IM_COL32(20, 20, 20, 255);
        if (acting != ENGINE_NO_FACTION && city.owner_force_id != ENGINE_NO_FACTION && city.owner_force_id != acting)
        {
            u8 relation = api->diplomacy(api->engine, acting, city.owner_force_id).relation;
            border = relation == RELATION_ALLIANCE ? IM_COL32(60, 120, 255, 255) :
                     relation == RELATION_CEASEFIRE ? IM_COL32(250, 220, 40, 255) : IM_COL32(230, 40, 40, 255);
        }
        draw->AddRect(ImVec2(point.x - half - 1.0f, point.y - half - 1.0f), ImVec2(point.x + half + 1.0f, point.y + half + 1.0f), border, 0.0f, 0, 1.0f);
        if (city.city_id == ui->city.selected)
        {
            char name[UI_NAME_SIZE * 2];
            ui_city_label(ui, api, city.city_id, name, sizeof(name));
            draw->AddText(ImVec2(point.x + half + 3.0f, point.y - half - 3.0f), IM_COL32(255, 230, 60, 255), name);
        }
    }
    for (u32 index = 0; index < api->base_count(api->engine); ++index)
    {
        engine_base base = api->base_at(api->engine, index);
        if (base.id == ENGINE_NO_ROW) continue;
        ImVec2 point = ui_minimap_hex(frame, hex_make(base.q, base.r));
        if (!ui_minimap_inside(frame, point)) continue;
        f32 half = UI_MINIMAP_BASE_HALF;
        draw->AddRect(ImVec2(point.x - half, point.y - half), ImVec2(point.x + half, point.y + half), ui_faction_color(base.force), 0.0f, 0, 1.5f);
    }
    for (u32 id = 0; id < api->unit_count(api->engine); ++id)
    {
        engine_unit unit = api->unit_at(api->engine, id);
        if (unit.id == ENGINE_NO_UNIT) continue;
        ImVec2 point = ui_minimap_hex(frame, hex_make(unit.q, unit.r));
        if (!ui_minimap_inside(frame, point)) continue;
        f32 half = UI_MINIMAP_UNIT_HALF;
        ImVec2 diamond[4] = { ImVec2(point.x, point.y - half), ImVec2(point.x + half, point.y), ImVec2(point.x, point.y + half), ImVec2(point.x - half, point.y) };
        draw->AddConvexPolyFilled(diamond, 4, ui_faction_color(unit.force_id));
        if (unit.id == ui->units.selected) draw->AddPolyline(diamond, 4, IM_COL32(255, 255, 255, 255), ImDrawFlags_Closed, 1.0f);
    }
}

/* The white frame: the ground under the four corners of the screen. */
internal void
ui_draw_minimap_view_frame(ui_minimap_frame *frame, ImDrawList *draw)
{
    ImGuiIO &io = ImGui::GetIO();
    f32 width = io.DisplaySize.x, height = io.DisplaySize.y;
    if (width <= 0.0f || height <= 0.0f) return;
    m4 view_projection = camera_view_projection(frame->cam, width, height);
    f32 corners[4][2] = { { 0.0f, 0.0f }, { width, 0.0f }, { width, height }, { 0.0f, height } };
    ImVec2 points[4];
    for (u32 index = 0; index < 4; ++index)
    {
        ray sight = camera_ray(view_projection, corners[index][0], corners[index][1], width, height);
        v3 hit;
        if (!ray_hit_plane_y(sight, frame->cam->target.y, &hit)) return;
        points[index] = ui_minimap_screen(frame, hit.x, hit.z);
    }
    draw->PushClipRect(ImVec2(frame->centre.x - frame->radius, frame->centre.y - frame->radius),
                       ImVec2(frame->centre.x + frame->radius, frame->centre.y + frame->radius), true);
    draw->AddPolyline(points, 4, IM_COL32(255, 255, 255, 235), ImDrawFlags_Closed, 1.5f);
    draw->PopClipRect();
}

/* The compass ring round the disc: 北, 東, 南 and 西 at the ground
   directions, so the ring turns with the camera. */
internal void
ui_draw_minimap_ring(ui_minimap_frame *frame, ui_state *ui, ImDrawList *draw)
{
    f32 middle = frame->radius + UI_MINIMAP_RING_WIDTH * 0.5f;
    draw->AddCircle(frame->centre, middle, IM_COL32(45, 35, 25, 235), 64, UI_MINIMAP_RING_WIDTH);
    draw->AddCircle(frame->centre, frame->radius, IM_COL32(230, 230, 230, 220), 64, 1.0f);
    draw->AddCircle(frame->centre, frame->radius + UI_MINIMAP_RING_WIDTH, IM_COL32(230, 230, 230, 220), 64, 1.0f);
    const char *labels[4] = { text_get(ui->text, "compass.north"), text_get(ui->text, "compass.east"), text_get(ui->text, "compass.south"), text_get(ui->text, "compass.west") };
    f32 ground[4][2] = { { 0.0f, -1.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f }, { -1.0f, 0.0f } };   /* x east, z south */
    for (u32 index = 0; index < 4; ++index)
    {
        f32 mx = 0.0f, my = 0.0f;
        camera_ground_to_minimap(frame->cam, ground[index][0], ground[index][1], &mx, &my);
        ImVec2 label_size = ImGui::CalcTextSize(labels[index]);
        draw->AddText(ImVec2(frame->centre.x + mx * middle - label_size.x * 0.5f, frame->centre.y + my * middle - label_size.y * 0.5f),
                      IM_COL32(255, 255, 255, 255), labels[index]);
    }
}

/* A click or a drag on the disc: the camera goes to the hex under the
   mouse. The distance, the heading and the tilt stay. */
internal void
ui_minimap_move_camera(ui_minimap_frame *frame, ui_state *ui, engine_api *api, engine_map_size size, ImVec2 point)
{
    f32 dx = 0.0f, dz = 0.0f;
    camera_minimap_to_ground(frame->cam, (point.x - frame->centre.x) / frame->scale, (point.y - frame->centre.y) / frame->scale, &dx, &dz);
    hex_offset o = hex_to_offset(hex_from_world(v3_make(frame->middle.x + dx, 0.0f, frame->middle.z + dz)));
    if (!size.width || !size.height) return;
    o.col = o.col < 0 ? 0 : (o.col >= (s32)size.width  ? (s32)size.width  - 1 : o.col);
    o.row = o.row < 0 ? 0 : (o.row >= (s32)size.height ? (s32)size.height - 1 : o.row);
    hex_coord hex = hex_from_offset(o);
    ui->scene.cam.target   = ui_hex_world(api, hex);
    camera_clamp(&ui->scene.cam);
    ui->map.scroll_request = true;
    ui->map.scroll_to      = hex;
}

internal void
ui_draw_minimap(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(300, 320), ImGuiCond_FirstUseEver);
    if (ui->hud_mode)   /* the game mode: at the bottom right when it appears and after a change of the screen size (main-map.md: 1376, 740, 302, 272) */
    {
        ImVec2 display = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(display.x - UI_MINIMAP_MARGIN, display.y - UI_MINIMAP_MARGIN), ui_corner_cond(&ui->minimap_corner), ImVec2(1.0f, 1.0f));
    }
    bool *close = ui->hud_mode ? 0 : &ui->show_minimap;   /* M22: the original has no close button */
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "minimap.title", "MiniMap"), close,
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        if (!ui->scene.initialized) ui_scene_reset_camera(ui, api);
        engine_map_size size = api->map_size(api->engine);
        ImVec2 avail = ImGui::GetContentRegionAvail();
        f32 diameter = avail.x < avail.y ? avail.x : avail.y;
        f32 radius = diameter * 0.5f - UI_MINIMAP_RING_WIDTH;
        if (radius < UI_MINIMAP_RADIUS_MIN) radius = UI_MINIMAP_RADIUS_MIN;
        f32 outer = radius + UI_MINIMAP_RING_WIDTH;
        ImVec2 corner = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("minimap", ImVec2(outer * 2.0f, outer * 2.0f));
        ui_minimap_frame frame = ui_minimap_fit(&ui->scene.cam, size, ImVec2(corner.x + outer, corner.y + outer), radius);
        if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left) && ui_minimap_inside(&frame, ImGui::GetIO().MousePos))
        {
            ui_minimap_move_camera(&frame, ui, api, size, ImGui::GetIO().MousePos);
        }
        ImDrawList *draw = ImGui::GetWindowDrawList();
        engine_terrain_view view = api->terrain_view(api->engine);   /* M10: one view for the tiles of the 40,000 hexes */
        draw->AddCircleFilled(frame.centre, radius, IM_COL32(20, 40, 70, 255), 64);
        ui_draw_minimap_terrain(&frame, api, draw, size, view);
        ui_draw_minimap_roads(&frame, api, draw, view);
        ui_draw_minimap_markers(&frame, ui, api, draw);
        ui_draw_minimap_view_frame(&frame, draw);
        ui_draw_minimap_ring(&frame, ui, draw);
    }
    ImGui::End();
}
