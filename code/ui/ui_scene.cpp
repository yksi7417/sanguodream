/* The 2.5D scene: the map as the window background with an orbit camera.
   This file reads the map through engine_api, writes the frame's
   render_commands, turns mouse input into camera moves and picks, and draws
   a small "Scene" panel with buttons that the walkthroughs and the ui tests
   use instead of mouse drags. Included by ui.cpp.

   The selection (from, to) is shared with the 2D Hex map panel, so both
   views show the same picks and the same path cost. */

#define UI_SCENE_ORBIT_STEP  (15.0f * RENDER_PI / 180.0f)
#define UI_SCENE_TILT_STEP   (5.0f * RENDER_PI / 180.0f)
#define UI_SCENE_ZOOM_STEP   1.25f
#define UI_SCENE_DRAG_SPEED  0.008f
#define UI_SCENE_PAN_SPEED   0.05f
#define UI_SCENE_KEY_PAN_SPEED 0.6f   /* of the camera distance each second while W, A, S or D is down */
#define UI_SCENE_KEY_TURN_SPEED (90.0f * RENDER_PI / 180.0f)   /* radians each second while Q, E, Left or Right is down */
#define UI_SCENE_KEY_TILT_SPEED (45.0f * RENDER_PI / 180.0f)   /* radians each second while Up or Down is down */

internal void
ui_scene_init(ui_state *ui)
{
    ui->scene.enabled     = true;
    ui->scene.grid        = true;
    ui->scene.initialized = false;
    ui->scene.has_hover   = false;
    ui->scene.has_pick    = false;      /* M11 */
    ui->scene.framed_city = 0;          /* the selection of ui_init: no frame at the start */
    ui->scene.framed_unit = ENGINE_NO_UNIT;
    ui->scene.frame_city  = false;
    ui->scene.frame_unit  = false;
    ui->scene.headless    = false;
}

internal void
ui_scene_reset_camera(ui_state *ui, engine_api *api)
{
    engine_map_size size = api->map_size(api->engine);
    hex_offset center_offset;
    center_offset.col = (s32)(size.width / 2);
    center_offset.row = (s32)(size.height / 2);
    v3 center = hex_world_center(hex_from_offset(center_offset), 0.0f);
    ui->scene.cam = camera_default(center.x, center.z);
    ui->scene.cam.distance = 0.9f * (f32)(size.width > size.height ? size.width : size.height);
    camera_clamp(&ui->scene.cam);
    ui->scene.initialized = true;
}

/* The Scene panel: camera controls that need no mouse drag. */
internal void
ui_draw_scene_panel(ui_state *ui, engine_api *api)
{
    ImGui::SetNextWindowSize(ImVec2(300, 190), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Scene", &ui->show_scene))
    {
        if (!ui->scene.initialized) ui_scene_reset_camera(ui, api);
        ImGui::Checkbox("Scene on", &ui->scene.enabled);
        ImGui::SameLine();
        ImGui::Checkbox("Grid", &ui->scene.grid);
        if (ImGui::Button("Orbit left"))  camera_orbit(&ui->scene.cam, -UI_SCENE_ORBIT_STEP, 0.0f);
        ImGui::SameLine();
        if (ImGui::Button("Orbit right")) camera_orbit(&ui->scene.cam,  UI_SCENE_ORBIT_STEP, 0.0f);
        ImGui::SameLine();
        if (ImGui::Button("Tilt up"))     camera_orbit(&ui->scene.cam, 0.0f,  UI_SCENE_TILT_STEP);
        ImGui::SameLine();
        if (ImGui::Button("Tilt down"))   camera_orbit(&ui->scene.cam, 0.0f, -UI_SCENE_TILT_STEP);
        if (ImGui::Button("Zoom in"))     camera_zoom(&ui->scene.cam, 1.0f / UI_SCENE_ZOOM_STEP);
        ImGui::SameLine();
        if (ImGui::Button("Zoom out"))    camera_zoom(&ui->scene.cam, UI_SCENE_ZOOM_STEP);
        ImGui::SameLine();
        if (ImGui::Button("Reset view"))  ui_scene_reset_camera(ui, api);
        ImGui::Text("yaw %.0f  pitch %.0f  distance %.1f",
                    (f64)(ui->scene.cam.yaw * 180.0f / RENDER_PI), (f64)(ui->scene.cam.pitch * 180.0f / RENDER_PI), (f64)ui->scene.cam.distance);
        if (ui->scene.has_hover) ImGui::Text("under the mouse: (%d, %d)", ui->scene.hover.q, ui->scene.hover.r);
        else                     ImGui::Text("under the mouse: nothing");
        ImGui::TextWrapped("Right drag orbits, middle drag pans, wheel zooms, left click picks and shows the hex. "
                           "W and S move the view up and down the screen, A and D to the left and right. "
                           "Q and Left turn the view clockwise, E and Right counter-clockwise; Up and Down tilt. "
                           "PageUp and PageDown zoom (the Hex map panel takes the keys while it has the focus).");
    }
    ImGui::End();
}

/* Mouse on the background: the scene owns it only when no ImGui window
   wants it. Picks share the selection with the 2D map panel. */
internal void
ui_scene_handle_mouse(ui_state *ui, engine_api *api, f32 viewport_width, f32 viewport_height)
{
    ImGuiIO &io = ImGui::GetIO();
    ui->scene.has_hover = false;
    if (io.WantCaptureMouse || viewport_width <= 0.0f || viewport_height <= 0.0f) return;

    hex_coord under;
    engine_map_size size = api->map_size(api->engine);
    engine_terrain_view view = api->terrain_view(api->engine);   /* the levels: the pick meets the top of a raised hex (the player, 2026-10-24) */
    if (camera_pick_hex_levels(&ui->scene.cam, io.MousePos.x, io.MousePos.y, viewport_width, viewport_height, view.level, view.width, view.height, TERRAIN_STEP, &under))
    {
        hex_offset o = hex_to_offset(under);
        if (o.col >= 0 && o.row >= 0 && (u32)o.col < size.width && (u32)o.row < size.height)
        {
            ui->scene.hover = under;
            ui->scene.has_hover = true;
        }
    }
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
    {
        camera_orbit(&ui->scene.cam, io.MouseDelta.x * UI_SCENE_DRAG_SPEED, -io.MouseDelta.y * UI_SCENE_DRAG_SPEED);
    }
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
    {
        f32 speed = UI_SCENE_PAN_SPEED * ui->scene.cam.distance / 40.0f;
        camera_pan(&ui->scene.cam, -io.MouseDelta.x * speed, io.MouseDelta.y * speed);
    }
    if (io.MouseWheel != 0.0f)
    {
        camera_zoom(&ui->scene.cam, io.MouseWheel > 0.0f ? 1.0f / UI_SCENE_ZOOM_STEP : UI_SCENE_ZOOM_STEP);
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ui->scene.has_hover)
    {
        if (!ui->map.has_from || ui->map.has_to) { ui->map.from = ui->scene.hover; ui->map.has_from = true; ui->map.has_to = false; }
        else                                     { ui->map.to = ui->scene.hover; ui->map.has_to = true; }
        if (!ui->hud_mode)                      /* the game mode opens the Hex info box from the map menu */
        {
            ui->scene.pick     = ui->scene.hover;   /* M11: the Hex info box shows the hex */
            ui->scene.has_pick = true;
        }
    }
}

/* The keys move the camera when no text box and not the Hex map panel
   take them (terrain-and-map.md, "Map display"; M11):
   - W and S move the view up and down the screen, A and D to the left and
     to the right, at every heading (camera_pan);
   - Q and Left turn the view clockwise on the screen, E and Right
     counter-clockwise, as the original does (ui/main-map.md). The view
     turns smoothly while the key is down (the choice of the user; the
     original turns by steps, Q-214);
   - Up tilts toward the top-down view, Down toward the horizon; the clamp
     keeps the view above the ground and the heading stays;
   - PageUp and PageDown zoom.
   The arrow keys do nothing while the keyboard moves the focus of a panel. */
internal void
ui_scene_handle_keys(ui_state *ui, f32 dt_seconds)
{
    ImGuiIO &io = ImGui::GetIO();
    if (io.WantTextInput || ui->map.focused) return;
    f32 step = UI_SCENE_KEY_PAN_SPEED * ui->scene.cam.distance * dt_seconds;
    f32 right = 0.0f, forward = 0.0f;
    if (ImGui::IsKeyDown(ImGuiKey_W)) forward += step;
    if (ImGui::IsKeyDown(ImGuiKey_S)) forward -= step;
    if (ImGui::IsKeyDown(ImGuiKey_D)) right   += step;
    if (ImGui::IsKeyDown(ImGuiKey_A)) right   -= step;
    if (right != 0.0f || forward != 0.0f) camera_pan(&ui->scene.cam, right, forward);
    b32 arrows = !io.NavActive;
    f32 turn = 0.0f, tilt = 0.0f;
    if (ImGui::IsKeyDown(ImGuiKey_Q) || (arrows && ImGui::IsKeyDown(ImGuiKey_LeftArrow)))  turn -= UI_SCENE_KEY_TURN_SPEED * dt_seconds;
    if (ImGui::IsKeyDown(ImGuiKey_E) || (arrows && ImGui::IsKeyDown(ImGuiKey_RightArrow))) turn += UI_SCENE_KEY_TURN_SPEED * dt_seconds;
    if (arrows && ImGui::IsKeyDown(ImGuiKey_UpArrow))   tilt += UI_SCENE_KEY_TILT_SPEED * dt_seconds;
    if (arrows && ImGui::IsKeyDown(ImGuiKey_DownArrow)) tilt -= UI_SCENE_KEY_TILT_SPEED * dt_seconds;
    if (turn != 0.0f) camera_turn(&ui->scene.cam, turn);
    if (tilt != 0.0f) camera_tilt(&ui->scene.cam, tilt);
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp))   camera_zoom(&ui->scene.cam, 1.0f / UI_SCENE_ZOOM_STEP);
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) camera_zoom(&ui->scene.cam, UI_SCENE_ZOOM_STEP);
}

internal hex_coord ui_round_unit_hex(ui_state *ui, engine_unit *unit);   /* ui_round.cpp (M20); ui_locate.cpp uses it too */
internal b32       ui_pick_allows(ui_state *ui, engine_api *api, u8 pick, u8 row, hex_coord hex);   /* ui_combat.cpp: the probe of a target */
internal u32       ui_unit_sprite(engine_api *api, engine_unit *unit);   /* ui_war.cpp: the sprite of the class of the unit */

#define UI_SCENE_UNIT_SPRITE_SIZE 1.4f   /* world units; the square picture of a unit on its hex */

/* The axis (0, 1 or 2: the directions d and d + 3) of the road through a
   pass, from the six neighbours of its hex: the axis with a passable hex
   on both sides, roads first, with the fewest passable hexes beside it.
   A pass sits in a gap of the cliffs, so the open axis is the road. */
internal u32
ui_gate_axis(const b32 passable[HEX_DIRECTION_COUNT], const b32 road[HEX_DIRECTION_COUNT])
{
    u32 best = 0;
    s32 best_score = -1000;
    for (u32 axis = 0; axis < HEX_DIRECTION_COUNT / 2; ++axis)
    {
        u32 a = axis, b = axis + HEX_DIRECTION_COUNT / 2;
        s32 score = 100 * ((passable[a] ? 1 : 0) + (passable[b] ? 1 : 0)) + 10 * ((road[a] ? 1 : 0) + (road[b] ? 1 : 0));
        for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d) if (d != a && d != b && passable[d]) score -= 1;
        if (score > best_score) { best_score = score; best = axis; }
    }
    return best;
}

/* The yaw of the gate of a pass: the shader turns the local Z axis of a
   mesh, the passage of the gate, toward (sin yaw, cos yaw) on the ground. */
internal f32
ui_gate_yaw(engine_api *api, engine_terrain_view view, hex_coord h)
{
    b32 passable[HEX_DIRECTION_COUNT] = {}, road[HEX_DIRECTION_COUNT] = {};
    for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d)
    {
        hex_offset o = hex_to_offset(hex_neighbor(h, d));
        if (o.col < 0 || o.row < 0 || (u32)o.col >= view.width || (u32)o.row >= view.height) continue;
        engine_terrain_kind terrain = api->terrain_kind_at(api->engine, view.kind[(u32)o.row * view.width + (u32)o.col]);
        passable[d] = terrain.id != ENGINE_NO_TERRAIN && terrain.cost[MOVEMENT_FOOT] != 0;
        road[d]     = passable[d] && (!strcmp(terrain.key, "main_road") || !strcmp(terrain.key, "small_path") || !strcmp(terrain.key, "plank_road") ||
                                      !strcmp(terrain.key, "ford") || !strcmp(terrain.key, "shallows"));
    }
    v3 along = v3_sub(hex_world_center(hex_neighbor(h, ui_gate_axis(passable, road)), 0.0f), hex_world_center(h, 0.0f));
    return atan2f(along.x, along.z);
}

/* The placeholder model of each slot of a city that has a facility
   (ui/main-map.md, "Map labels": the slots are building models on their
   own hexes). A built facility is the shape of its kind, in its colour. A
   build in progress is a scaffold with the shape of the kind inside,
   smaller and dimmed, so the player sees what the slot becomes. */
internal void
ui_scene_push_slots(render_commands *rc, engine_api *api, engine_terrain_view view, u8 city_id)
{
    engine_city_slots slots = api->city_slots(api->engine, city_id);
    for (u32 slot = 0; slot < slots.slot_count; ++slot)
    {
        engine_facility facility = api->facility_at(api->engine, city_id, (u8)slot);
        if (facility.city_id == ENGINE_NO_CITY || facility.facility == ENGINE_NO_FACILITY || facility.state == SIM_SLOT_EMPTY) continue;
        hex_coord h = hex_make(facility.q, facility.r);
        hex_offset o = hex_to_offset(h);
        if (o.col < 0 || o.row < 0 || (u32)o.col >= view.width || (u32)o.row >= view.height) continue;
        u32 cell = (u32)o.row * view.width + (u32)o.col;
        v3 ground = hex_world_center(h, terrain_level_y(view.level[cell]));
        engine_facility_kind kind = api->facility_kind_at(api->engine, facility.facility);
        ui_facility_look look = ui_facility_look_of(kind.key);
        if (facility.state == SIM_SLOT_BUILDING)
        {
            render_push_mesh(rc, RENDER_MESH_SCAFFOLD, ground, 0.0f, UI_FACILITY_MODEL_SCALE, UI_SCAFFOLD_RGBA);
            render_push_mesh(rc, look.mesh, ground, 0.0f, UI_FACILITY_BUILDING_SCALE, ui_rgba_dim(look.rgba, 0.55f));
        }
        else
        {
            render_push_mesh(rc, look.mesh, ground, 0.0f, UI_FACILITY_MODEL_SCALE, look.rgba);
        }
    }
}

/* Fills the frame's render commands from the engine: the terrain, the
   overlay colours for the selection and the hover, and placeholder models
   and units until the spec gives real positions. */
internal void
ui_scene_push(ui_state *ui, engine_api *api, ui_frame_info *frame)
{
    render_commands *rc = frame->render;
    if (!rc || !frame->frame_arena || !ui->scene.enabled) return;
    if (!ui->scene.initialized) ui_scene_reset_camera(ui, api);

    engine_terrain_view view = api->terrain_view(api->engine);
    rc->scene_enabled = true;
    rc->cam           = ui->scene.cam;
    rc->draw_grid     = ui->scene.grid;
    rc->terrain.width   = view.width;
    rc->terrain.height  = view.height;
    rc->terrain.kind    = view.tile ? view.tile : view.kind;   /* the renderer draws the atlas colour */
    rc->terrain.level   = view.level;
    rc->terrain.version = view.version;
    /* M11: the grid only on terrain that a unit can enter; cliffs and mountains show none (ui/main-map.md). */
    u8 *grid = PushArray(frame->frame_arena, (view.width * view.height) ? view.width * view.height : 1, u8);
    b32 enterable[256] = {};
    for (u32 kind = 0; kind < api->terrain_kind_count(api->engine) && kind < 256; ++kind) enterable[kind] = api->terrain_kind_at(api->engine, kind).enterable;
    for (u32 cell = 0; cell < view.width * view.height && view.kind; ++cell) grid[cell] = (u8)(enterable[view.kind[cell]] ? 1 : 0);
    rc->terrain.grid = grid;

    u32 cells = view.width * view.height;
    u32 *overlay = PushArray(frame->frame_arena, cells ? cells : 1, u32);
    rc->overlay = overlay;
    if (ui->scene.has_hover) ui_scene_mark(overlay, view, ui->scene.hover, 0x60FFFFFFu);
    if (ui->map.has_from)    ui_scene_mark(overlay, view, ui->map.from, 0xB03CDCFFu);
    if (ui->map.has_to)      ui_scene_mark(overlay, view, ui->map.to,   0xB03C5AFFu);
    /* G2-054: during a target pick, the hexes that the command can hit show red.
       G3-006: the map goes dark around them, as in the original. */
    if (ui->combat.pick != UI_PICK_NONE && ui->units.selected != ENGINE_NO_UNIT)
    {
        for (u32 cell = 0; cell < cells; ++cell) overlay[cell] = 0x90000000u;
        engine_unit picker = api->unit_at(api->engine, ui->units.selected);
        hex_coord center = ui_unit_menu_hex(ui, &picker);   /* G2-045: around the destination of a pending move */
        for (s32 dq = -3; picker.id != ENGINE_NO_UNIT && dq <= 3; ++dq)
        {
            for (s32 dr = -3; dr <= 3; ++dr)
            {
                hex_coord hex = hex_make(center.q + dq, center.r + dr);
                if (hex_distance(hex, center) > 3) continue;
                if (ui_pick_allows(ui, api, ui->combat.pick, ui->combat.row, hex)) ui_scene_mark(overlay, view, hex, 0xB03C3CFFu);
            }
        }
    }
    /* M10: the slots of the city that the Hex map panel selected: yellow. */
    if (ui->map.has_city)
    {
        engine_city_slots slots = api->city_slots(api->engine, ui->map.city);
        for (u32 slot = 0; slot < slots.slot_count; ++slot)
        {
            engine_facility facility = api->facility_at(api->engine, ui->map.city, (u8)slot);
            if (facility.city_id != ENGINE_NO_CITY) ui_scene_mark(overlay, view, hex_make(facility.q, facility.r), 0xA028E6FFu);
        }
    }

    /* The range of the selected unit: green; its hex cyan (ui/unit-commands.md). */
    if (ui->units.selected != ENGINE_NO_UNIT)
    {
        engine_reach_view reach = api->reachable(api->engine, ui->units.selected);
        for (u32 index = 0; index < reach.count; ++index)
        {
            ui_scene_mark(overlay, view, reach.cells[index], index == 0 ? 0xB0E6DC3Cu : 0x7050F050u);
        }
    }

    /* A placeholder model on each city and a billboard on each unit, in the
       colour of the faction. The art comes from the player's install (M9). */
    if (!cells) return;
    for (u32 index = 0; index < api->city_count(api->engine); ++index)
    {
        engine_city city = api->city_at(api->engine, index);
        engine_city_slots slots = api->city_slots(api->engine, city.city_id);
        hex_coord h = hex_make(slots.q, slots.r);
        hex_offset o = hex_to_offset(h);
        if (o.col < 0 || o.row < 0 || (u32)o.col >= view.width || (u32)o.row >= view.height) continue;
        u32 cell = (u32)o.row * view.width + (u32)o.col;
        render_push_mesh(rc, RENDER_MESH_SETTLEMENT, hex_world_center(h, terrain_level_y(view.level[cell])), 0.0f, 1.0f, ui_faction_rgba(city.owner_force_id));
        ui_scene_push_slots(rc, api, view, city.city_id);
    }
    /* A gate on each pass and a pier on each port, in the colour of the
       owner (the art of the bases comes later, docs/GRAPHICS.md). The
       passage of the gate follows the road through the pass. */
    for (u32 index = 0; index < api->base_count(api->engine); ++index)
    {
        engine_base base = api->base_at(api->engine, index);
        if (base.id == ENGINE_NO_ROW) continue;
        hex_coord h = hex_make(base.q, base.r);
        hex_offset o = hex_to_offset(h);
        if (o.col < 0 || o.row < 0 || (u32)o.col >= view.width || (u32)o.row >= view.height) continue;
        u32 cell = (u32)o.row * view.width + (u32)o.col;
        u32 mesh = base.type == BASE_PORT ? RENDER_MESH_PORT : RENDER_MESH_GATE;
        f32 yaw = base.type == BASE_PORT ? 0.0f : ui_gate_yaw(api, view, h);
        render_push_mesh(rc, mesh, hex_world_center(h, terrain_level_y(view.level[cell])), yaw, 1.0f, ui_faction_rgba(base.force));
    }
    for (u32 id = 0; id < api->unit_count(api->engine); ++id)
    {
        engine_unit unit = api->unit_at(api->engine, id);
        if (unit.id == ENGINE_NO_UNIT) continue;
        hex_coord h = ui_round_unit_hex(ui, &unit);   /* M20: a unit of the show walks hex by hex */
        hex_offset o = hex_to_offset(h);
        if (o.col < 0 || o.row < 0 || (u32)o.col >= view.width || (u32)o.row >= view.height) continue;
        u32 cell = (u32)o.row * view.width + (u32)o.col;
        render_push_billboard(rc, ui_unit_sprite(api, &unit), hex_world_center(h, terrain_level_y(view.level[cell])),
                              UI_SCENE_UNIT_SPRITE_SIZE, UI_SCENE_UNIT_SPRITE_SIZE, ui_faction_rgba(unit.force_id));
    }
}
