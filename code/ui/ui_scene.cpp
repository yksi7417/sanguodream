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
                           "Q and Left turn the map counter-clockwise, E and Right clockwise; Up and Down tilt. "
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
    if (camera_pick_hex(&ui->scene.cam, io.MousePos.x, io.MousePos.y, viewport_width, viewport_height, &under))
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
        ui->scene.pick     = ui->scene.hover;   /* M11: the Hex info box shows the hex */
        ui->scene.has_pick = true;
    }
}

/* The keys move the camera when no text box and not the Hex map panel
   take them (terrain-and-map.md, "Map display"; M11):
   - W and S move the view up and down the screen, A and D to the left and
     to the right, at every heading (camera_pan);
   - Q and Left turn the map counter-clockwise, E and Right clockwise;
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
    if (ImGui::IsKeyDown(ImGuiKey_Q) || (arrows && ImGui::IsKeyDown(ImGuiKey_LeftArrow)))  turn += UI_SCENE_KEY_TURN_SPEED * dt_seconds;
    if (ImGui::IsKeyDown(ImGuiKey_E) || (arrows && ImGui::IsKeyDown(ImGuiKey_RightArrow))) turn -= UI_SCENE_KEY_TURN_SPEED * dt_seconds;
    if (arrows && ImGui::IsKeyDown(ImGuiKey_UpArrow))   tilt += UI_SCENE_KEY_TILT_SPEED * dt_seconds;
    if (arrows && ImGui::IsKeyDown(ImGuiKey_DownArrow)) tilt -= UI_SCENE_KEY_TILT_SPEED * dt_seconds;
    if (turn != 0.0f) camera_turn(&ui->scene.cam, turn);
    if (tilt != 0.0f) camera_tilt(&ui->scene.cam, tilt);
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp))   camera_zoom(&ui->scene.cam, 1.0f / UI_SCENE_ZOOM_STEP);
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) camera_zoom(&ui->scene.cam, UI_SCENE_ZOOM_STEP);
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

    u32 cells = view.width * view.height;
    u32 *overlay = PushArray(frame->frame_arena, cells ? cells : 1, u32);
    rc->overlay = overlay;
    if (ui->scene.has_hover) ui_scene_mark(overlay, view, ui->scene.hover, 0x60FFFFFFu);
    if (ui->map.has_from)    ui_scene_mark(overlay, view, ui->map.from, 0xB03CDCFFu);
    if (ui->map.has_to)      ui_scene_mark(overlay, view, ui->map.to,   0xB03C5AFFu);
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
        render_push_mesh(rc, 0, hex_world_center(h, terrain_level_y(view.level[cell])), 0.0f, 1.0f, ui_faction_rgba(city.owner_force_id));
    }
    for (u32 id = 0; id < api->unit_count(api->engine); ++id)
    {
        engine_unit unit = api->unit_at(api->engine, id);
        if (unit.id == ENGINE_NO_UNIT) continue;
        hex_coord h = hex_make(unit.q, unit.r);
        hex_offset o = hex_to_offset(h);
        if (o.col < 0 || o.row < 0 || (u32)o.col >= view.width || (u32)o.row >= view.height) continue;
        u32 cell = (u32)o.row * view.width + (u32)o.col;
        render_push_billboard(rc, 0, hex_world_center(h, terrain_level_y(view.level[cell])), 1.0f, 1.6f, ui_faction_rgba(unit.force_id));
    }
}
