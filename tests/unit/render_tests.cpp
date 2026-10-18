/* Unit tests for code/render/render.h: matrices, the orbit camera, the
   projection and the pick, all without a window. */

internal b32
near_f32(f32 a, f32 b, f32 tolerance)
{
    f32 d = a - b;
    if (d < 0) d = -d;
    return d <= tolerance;
}

internal void
unit_render_tests(test_counts *counts)
{
    /* matrices */
    m4 t = m4_translation(v3_make(1, 2, 3));
    v3 p = m4_transform_point(t, v3_make(1, 1, 1), 0);
    TEST_CHECK(counts, near_f32(p.x, 2, 1e-5f) && near_f32(p.y, 3, 1e-5f) && near_f32(p.z, 4, 1e-5f), "translation moves a point");

    m4 ry = m4_rotation_y(RENDER_PI * 0.5f);
    v3 r = m4_transform_point(ry, v3_make(1, 0, 0), 0);
    TEST_CHECK(counts, near_f32(r.x, 0, 1e-5f) && near_f32(r.z, -1, 1e-5f), "rotation about Y turns +X toward -Z");

    m4 inv;
    m4 combo = m4_mul(m4_mul(t, ry), m4_scale(2.0f));
    TEST_CHECK(counts, m4_inverse(combo, &inv), "a rigid transform is invertible");
    v3 back = m4_transform_point(inv, m4_transform_point(combo, v3_make(3, -1, 7), 0), 0);
    TEST_CHECK(counts, near_f32(back.x, 3, 1e-4f) && near_f32(back.y, -1, 1e-4f) && near_f32(back.z, 7, 1e-4f), "inverse undoes the transform");
    m4 singular = {};
    TEST_CHECK(counts, !m4_inverse(singular, &inv), "a singular matrix has no inverse");

    /* camera clamps */
    camera c = camera_default(10.0f, 8.0f);
    camera_orbit(&c, 0.0f, 10.0f);
    TEST_CHECK(counts, near_f32(c.pitch, CAMERA_PITCH_MAX, 1e-6f), "pitch clamps at the top");
    camera_orbit(&c, 0.0f, -10.0f);
    TEST_CHECK(counts, near_f32(c.pitch, CAMERA_PITCH_MIN, 1e-6f), "pitch clamps at the bottom");
    camera_zoom(&c, 0.0001f);
    TEST_CHECK(counts, near_f32(c.distance, CAMERA_DISTANCE_MIN, 1e-6f), "zoom clamps near");
    camera_zoom(&c, 1e6f);
    TEST_CHECK(counts, near_f32(c.distance, CAMERA_DISTANCE_MAX, 1e-6f), "zoom clamps far");
    camera_orbit(&c, 4.0f * RENDER_PI, 0.0f);
    TEST_CHECK(counts, c.yaw >= -RENDER_PI && c.yaw <= RENDER_PI, "yaw wraps into one turn");

    /* the eye is distance away from the target and above it */
    c = camera_default(0.0f, 0.0f);
    v3 eye = camera_eye(&c);
    TEST_CHECK(counts, near_f32(v3_length(v3_sub(eye, c.target)), c.distance, 1e-3f), "eye is at the orbit distance");
    TEST_CHECK(counts, eye.y > 0.0f, "eye is above the ground");

    /* the target projects to the screen center */
    f32 vw = 1280.0f, vh = 720.0f;
    m4 vp = camera_view_projection(&c, vw, vh);
    f32 px = 0, py = 0;
    TEST_CHECK(counts, camera_project(vp, c.target, vw, vh, &px, &py), "target is in front of the camera");
    TEST_CHECK(counts, near_f32(px, vw * 0.5f, 0.5f) && near_f32(py, vh * 0.5f, 0.5f), "target projects to the screen center");
    f32 bx = 0, by = 0;
    TEST_CHECK(counts, !camera_project(vp, v3_add(eye, v3_sub(eye, c.target)), vw, vh, &bx, &by), "a point behind the camera does not project");

    /* a pick through the center lands on the target's hex */
    hex_coord picked;
    TEST_CHECK(counts, camera_pick_hex(&c, vw * 0.5f, vh * 0.5f, vw, vh, &picked), "pick through the center hits the plane");
    TEST_CHECK(counts, hex_equal(picked, hex_from_world(c.target)), "pick through the center gives the target's hex");

    /* round trip: every hex of a small map projects and picks back to itself */
    b32 round_trip = true;
    camera far_cam = camera_default(8.0f, 6.0f);
    far_cam.yaw = 0.7f;
    far_cam.pitch = 55.0f * RENDER_PI / 180.0f;
    far_cam.distance = 24.0f;
    m4 far_vp = camera_view_projection(&far_cam, vw, vh);
    for (s32 row = 0; row < 8; ++row)
    {
        for (s32 col = 0; col < 10; ++col)
        {
            hex_offset o; o.col = col; o.row = row;
            hex_coord h = hex_from_offset(o);
            f32 sx, sy;
            if (!camera_project(far_vp, hex_world_center(h, 0.0f), vw, vh, &sx, &sy)) { round_trip = false; continue; }
            hex_coord back_hex;
            if (!camera_pick_hex(&far_cam, sx, sy, vw, vh, &back_hex) || !hex_equal(back_hex, h)) round_trip = false;
        }
    }
    TEST_CHECK(counts, round_trip, "project then pick returns the same hex for every cell");

    /* a ray that points above the horizon never meets the ground */
    ray upward;
    upward.origin = v3_make(0.0f, 10.0f, 0.0f);
    upward.direction = v3_normalize(v3_make(0.0f, 0.1f, -1.0f));
    v3 miss;
    TEST_CHECK(counts, !ray_hit_plane_y(upward, 0.0f, &miss), "a ray above the horizon misses the ground");
    ray downward;
    downward.origin = v3_make(0.0f, 10.0f, 0.0f);
    downward.direction = v3_normalize(v3_make(0.0f, -1.0f, -1.0f));
    TEST_CHECK(counts, ray_hit_plane_y(downward, 0.0f, &miss) && near_f32(miss.z, -10.0f, 1e-4f), "a ray down at 45 degrees lands ten units out");

    /* pan follows the camera's yaw */
    camera panned = camera_default(0.0f, 0.0f);
    panned.yaw = RENDER_PI * 0.5f;
    camera_pan(&panned, 1.0f, 0.0f);
    TEST_CHECK(counts, near_f32(panned.target.x, 0.0f, 1e-5f) && near_f32(panned.target.z, 1.0f, 1e-5f), "pan right at yaw 90 degrees moves along +Z");

    /* M11: the keys move and turn the view as the screen shows it. At each
       yaw, a pan forward moves the target up the screen and a pan right
       moves it to the right; a positive turn rotates the map counter-
       clockwise; a tilt keeps the heading. */
    b32 forward_up = true, right_right = true, turn_ccw = true;
    for (s32 step = -4; step <= 4; ++step)
    {
        camera view = camera_default(0.0f, 0.0f);
        view.yaw = (f32)step * 0.7f;
        camera_clamp(&view);
        m4 view_vp = camera_view_projection(&view, vw, vh);
        f32 cx = 0, cy = 0, fx = 0, fy = 0, right_x = 0, right_y = 0;
        v3 ahead, side;
        camera_ground_axes(&view, &ahead, &side);
        camera_project(view_vp, view.target, vw, vh, &cx, &cy);
        camera_project(view_vp, v3_add(view.target, ahead), vw, vh, &fx, &fy);
        camera_project(view_vp, v3_add(view.target, side), vw, vh, &right_x, &right_y);
        if (!(fy < cy - 1.0f && fabsf(fx - cx) < 1.0f)) forward_up = false;
        if (!(right_x > cx + 1.0f && fabsf(right_y - cy) < 1.0f)) right_right = false;
        v3 east = v3_add(view.target, v3_make(3.0f, 0.0f, 0.0f));
        f32 ex = 0, ey = 0, tx = 0, ty = 0;
        camera_project(view_vp, east, vw, vh, &ex, &ey);
        camera_turn(&view, 0.2f);
        m4 turned = camera_view_projection(&view, vw, vh);
        camera_project(turned, east, vw, vh, &tx, &ty);
        /* screen y grows down: flip it, then the cross product is positive for a counter-clockwise turn */
        f32 before_x = ex - cx, before_y = cy - ey, after_x = tx - cx, after_y = cy - ty;
        if (before_x * after_y - before_y * after_x <= 0.0f) turn_ccw = false;
    }
    TEST_CHECK(counts, forward_up, "a pan forward (W) moves the view up the screen at every yaw");
    TEST_CHECK(counts, right_right, "a pan right (D) moves the view to the right of the screen at every yaw");
    TEST_CHECK(counts, turn_ccw, "a positive turn (E, right arrow) rotates the map counter-clockwise on the screen");
    camera tilted = camera_default(0.0f, 0.0f);
    tilted.yaw = 1.0f;
    for (u32 step = 0; step < 50; ++step) camera_tilt(&tilted, 0.1f);
    b32 top_clamped = near_f32(tilted.pitch, CAMERA_PITCH_MAX, 1e-5f) && near_f32(tilted.yaw, 1.0f, 1e-5f);
    for (u32 step = 0; step < 50; ++step) camera_tilt(&tilted, -0.1f);
    TEST_CHECK(counts, top_clamped && near_f32(tilted.pitch, CAMERA_PITCH_MIN, 1e-5f) && near_f32(tilted.yaw, 1.0f, 1e-5f) &&
                       CAMERA_PITCH_MIN > 0.0f && CAMERA_PITCH_MAX < RENDER_PI * 0.5f,
               "a tilt stops above the ground and short of top down, and keeps the heading");

    /* M12: the mini map turns with the view. The forward direction of the
       camera is up on the mini map at every yaw, and the inverse gives the
       ground point back. */
    b32 minimap_up = true, minimap_back = true;
    for (s32 step = -4; step <= 4; ++step)
    {
        camera view = camera_default(0.0f, 0.0f);
        view.yaw = (f32)step * 0.7f;
        camera_clamp(&view);
        v3 ahead, side;
        camera_ground_axes(&view, &ahead, &side);
        f32 mx = 0, my = 0;
        camera_ground_to_minimap(&view, ahead.x, ahead.z, &mx, &my);
        if (!near_f32(mx, 0.0f, 1e-5f) || !near_f32(my, -1.0f, 1e-5f)) minimap_up = false;
        camera_ground_to_minimap(&view, side.x, side.z, &mx, &my);
        if (!near_f32(mx, 1.0f, 1e-5f) || !near_f32(my, 0.0f, 1e-5f)) minimap_up = false;
        f32 dx = 0, dz = 0;
        camera_ground_to_minimap(&view, 3.0f, -2.0f, &mx, &my);
        camera_minimap_to_ground(&view, mx, my, &dx, &dz);
        if (!near_f32(dx, 3.0f, 1e-4f) || !near_f32(dz, -2.0f, 1e-4f)) minimap_back = false;
    }
    camera north_up = camera_default(0.0f, 0.0f);
    f32 north_x = 0, north_y = 0;
    camera_ground_to_minimap(&north_up, 0.0f, -1.0f, &north_x, &north_y);
    TEST_CHECK(counts, minimap_up && near_f32(north_x, 0.0f, 1e-5f) && near_f32(north_y, -1.0f, 1e-5f),
               "the forward direction is up and the right direction is right on the mini map; at yaw 0 north is up");
    TEST_CHECK(counts, minimap_back, "a mini map point goes back to its ground point at every yaw");

    /* the command buffer */
    memory_arena arena = test_arena_create(Megabytes(2));
    render_commands *rc = render_commands_begin(&arena, vw, vh);
    TEST_CHECK(counts, rc && rc->mesh_instance_count == 0 && rc->billboard_count == 0 && !rc->scene_enabled, "fresh commands are empty");
    render_push_mesh(rc, 1, v3_make(1, 0, 2), 0.5f, 1.0f, 0xFFFFFFFF);
    render_push_billboard(rc, 0, v3_make(3, 0, 4), 1.0f, 2.0f, 0xFFFFFFFF);
    TEST_CHECK(counts, rc->mesh_instance_count == 1 && rc->mesh_instances[0].mesh_id == 1 && rc->billboard_count == 1, "pushes record instances");
    for (u32 i = 0; i < RENDER_MAX_BILLBOARDS + 10; ++i) render_push_billboard(rc, 0, v3_make(0, 0, 0), 1, 1, 0);
    TEST_CHECK(counts, rc->billboard_count == RENDER_MAX_BILLBOARDS, "a full buffer drops pushes");
    test_arena_destroy(&arena);
}
