/* Unit tests for code/render/render.h: matrices, the orbit camera, the
   projection and the pick, all without a window. */

internal b32
near_f32(f32 a, f32 b, f32 tolerance)
{
    f32 d = a - b;
    if (d < 0) d = -d;
    return d <= tolerance;
}

internal void unit_render_unit_tests(test_counts *counts);

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

    /* The raised ground: the pixel of the top of a raised hex picks that hex,
       where the plane alone picks a hex behind it (the player, 2026-10-24). */
    {
        u8 levels[10 * 8] = {};
        hex_offset raised_offset; raised_offset.col = 5; raised_offset.row = 4;
        hex_coord raised = hex_from_offset(raised_offset);
        levels[4 * 10 + 5] = 6;
        f32 step = 0.35f;
        f32 sx = 0, sy = 0;
        hex_coord plane_hex = {}, level_hex = {};
        b32 projected = camera_project(far_vp, hex_world_center(raised, 6.0f * step), vw, vh, &sx, &sy);
        b32 plane_pick = camera_pick_hex(&far_cam, sx, sy, vw, vh, &plane_hex);
        b32 level_pick = camera_pick_hex_levels(&far_cam, sx, sy, vw, vh, levels, 10, 8, step, &level_hex);
        TEST_CHECK(counts, projected && plane_pick && !hex_equal(plane_hex, raised), "the plane pick misses the top of a raised hex");
        TEST_CHECK(counts, level_pick && hex_equal(level_hex, raised), "the pick with the levels finds the raised hex under its top");
        hex_offset flat_offset; flat_offset.col = 2; flat_offset.row = 2;
        hex_coord flat = hex_from_offset(flat_offset);
        projected = camera_project(far_vp, hex_world_center(flat, 0.0f), vw, vh, &sx, &sy);
        level_pick = camera_pick_hex_levels(&far_cam, sx, sy, vw, vh, levels, 10, 8, step, &level_hex);
        TEST_CHECK(counts, projected && level_pick && hex_equal(level_hex, flat), "the pick with the levels keeps a flat hex");
    }

    /* The art of a base (docs/ASSETS.md, "Base art"): a plain mesh names no
       art, a base names its sprite, and a full buffer changes no instance. */
    {
        memory_arena frame = test_arena_create(Megabytes(1));
        render_commands *rc = render_commands_begin(&frame, vw, vh);
        render_push_mesh(rc, RENDER_MESH_HALL, v3_make(0, 0, 0), 0.0f, 1.0f, 0xFFFFFFFFu);
        render_push_base(rc, RENDER_MESH_GATE, RENDER_SPRITE_BASE_GATE, v3_make(1, 0, 0), 0.5f, 1.0f, 0xFF0000FFu);
        TEST_CHECK(counts, rc->mesh_instance_count == 2 && rc->mesh_instances[0].art_sprite == RENDER_NO_ART, "a plain mesh names no art");
        TEST_CHECK(counts, rc->mesh_instances[1].mesh_id == RENDER_MESH_GATE && rc->mesh_instances[1].art_sprite == RENDER_SPRITE_BASE_GATE &&
                           near_f32(rc->mesh_instances[1].yaw, 0.5f, 1e-6f), "a base keeps its placeholder mesh and names its art");
        rc->mesh_instance_count = RENDER_MAX_MESH_INSTANCES;
        render_push_base(rc, RENDER_MESH_PORT, RENDER_SPRITE_BASE_PORT, v3_make(2, 0, 0), 0.0f, 1.0f, 0xFFFFFFFFu);
        TEST_CHECK(counts, rc->mesh_instance_count == RENDER_MAX_MESH_INSTANCES && rc->mesh_instances[RENDER_MAX_MESH_INSTANCES - 1].art_sprite != RENDER_SPRITE_BASE_PORT,
                   "a base on a full buffer changes no instance");
        test_arena_destroy(&frame);
    }

    /* The card of a base: a city covers its flower of 7 hexes, 5 world
       units wide; a pass and a port cover one hex. The card is as high as
       the aspect of the picture. Its bottom edge stands toward the camera
       from the centre, at the height of the ground. */
    {
        TEST_CHECK(counts, near_f32(render_base_art_size_of(RENDER_SPRITE_BASE_CITY).width, 5.0f, 1e-6f) &&
                           near_f32(render_base_art_size_of(RENDER_SPRITE_BASE_BIG_CITY).width, 5.0f, 1e-6f),
                   "the art of a city is as wide as the flower of 7 hexes");
        TEST_CHECK(counts, render_base_art_size_of(RENDER_SPRITE_BASE_GATE).width < 3.0f && render_base_art_size_of(RENDER_SPRITE_BASE_PORT).width < 3.0f &&
                           near_f32(render_base_art_size_of(RENDER_SPRITE_SWORD).width, 2.0f, 1e-6f),
                   "the art of a pass and of a port covers one hex, and a sprite that is no base gets one hex");
        render_base_art_size city = render_base_art_size_of(RENDER_SPRITE_BASE_CITY);
        render_mesh_instance big = {};
        big.mesh_id = RENDER_MESH_SETTLEMENT; big.art_sprite = RENDER_SPRITE_BASE_BIG_CITY;
        big.position = v3_make(3.0f, 0.7f, 4.0f); big.yaw = 0.25f; big.scale = 1.5f; big.tint = 0xFF3050C0u;
        render_mesh_instance model = render_base_model_instance(&big);
        TEST_CHECK(counts, near_f32(model.scale, 5.0f * 1.5f, 1e-5f) && model.position.x == 3.0f && model.position.y == 0.7f &&
                           model.position.z == 4.0f && model.yaw == 0.25f && model.tint == big.tint,
                   "the model of a big city covers the flower at the centre of the city and keeps its yaw");
        render_mesh_instance gate = big;
        gate.art_sprite = RENDER_SPRITE_BASE_GATE; gate.scale = 1.0f;
        f32 hex_pitch = v3_length(v3_sub(hex_world_center(hex_make(1, 0), 0.0f), hex_world_center(hex_make(0, 0), 0.0f)));
        TEST_CHECK(counts, near_f32(RENDER_HEX_PITCH, hex_pitch, 1e-4f) && near_f32(RENDER_GATE_WALL_WIDTH, 3.0f * hex_pitch, 1e-4f),
                   "the wall of a pass spans three hexes: the hex of the pass and one hex on each side");
        TEST_CHECK(counts, near_f32(render_base_model_instance(&gate).scale, render_base_model_widths[RENDER_SPRITE_BASE_GATE - RENDER_SPRITE_FIRST_BASE], 1e-5f),
                   "the model of a pass gets the width of its kind in the table");
        render_mesh_instance port = big;
        port.art_sprite = RENDER_SPRITE_BASE_PORT; port.scale = 1.0f;
        TEST_CHECK(counts, near_f32(render_base_model_instance(&port).scale, render_base_art_size_of(RENDER_SPRITE_BASE_PORT).width, 1e-5f),
                   "the model of a port covers the width of the art of a port");
        render_mesh_instance market = {};
        market.mesh_id = RENDER_MESH_STALL; market.art_sprite = RENDER_NO_ART;
        market.position = v3_make(1.0f, 0.2f, 2.0f); market.yaw = 0.5f; market.scale = 1.6f; market.tint = 0xFF3CB4F0u;
        render_mesh_instance shape_model = render_model_instance(&market);
        TEST_CHECK(counts, shape_model.scale == market.scale && shape_model.position.x == 1.0f && shape_model.position.z == 2.0f &&
                           shape_model.yaw == 0.5f && shape_model.tint == market.tint && near_f32(render_model_instance(&big).scale, 5.0f * 1.5f, 1e-5f),
                   "the model of a shape keeps the instance of the shape, and the model of a base covers the base");
        TEST_CHECK(counts, render_model_files[RENDER_MESH_STALL][0] && render_model_files[RENDER_MESH_FIELD][0] &&
                           !render_model_files[RENDER_MESH_SETTLEMENT][0] &&
                           !render_model_files[RENDER_MESH_GATE][0] && !render_model_files[RENDER_MESH_PORT][0],
                   "the market and the farm shapes name a model, and the shapes of the bases name none");
        TEST_CHECK(counts, render_model_files[RENDER_MESH_SCAFFOLD][0] && render_model_files[RENDER_MESH_SCAFFOLD][1] &&
                           render_model_files[RENDER_MESH_SCAFFOLD][2],
                   "the scaffold names a small, a medium and a large model");
        TEST_CHECK(counts, render_model_files[RENDER_MESH_STAGE][0] && render_model_files[RENDER_MESH_WALL][0] &&
                           render_model_files[RENDER_MESH_STONES][0] && render_model_files[RENDER_MESH_TRAP][0],
                   "the shapes of the field buildings name a model for level 1");
        {
            b32 kinds_named = true;
            for (u32 model_id = RENDER_MESH_COUNT; model_id < RENDER_MODEL_COUNT; ++model_id)
            {
                kinds_named &= render_model_files[model_id][0] != 0 && render_model_files[model_id][1] == 0;
            }
            TEST_CHECK(counts, kinds_named && RENDER_MODEL_UNIT_BANNER == RENDER_MODEL_COUNT - 1,
                       "each kind with its own model names one model for level 1");
        }
        TEST_CHECK(counts, render_model_files[RENDER_MESH_STALL][1] && render_model_files[RENDER_MESH_STALL][2] &&
                           render_model_files[RENDER_MESH_FIELD][1] && render_model_files[RENDER_MESH_FIELD][2],
                   "the market and the farm shapes name a model for the levels 2 and 3");
        TEST_CHECK(counts, render_model_files[RENDER_MESH_HALL][0] && render_model_files[RENDER_MESH_HALL][1] &&
                           render_model_files[RENDER_MESH_HALL][2],
                   "the hall shape names a model for the levels 1 to 3");
        TEST_CHECK(counts, render_model_files[RENDER_MESH_FORGE][0] && render_model_files[RENDER_MESH_FORGE][1] &&
                           render_model_files[RENDER_MESH_FORGE][2] && render_model_files[RENDER_MESH_SHED][0] &&
                           render_model_files[RENDER_MESH_SHED][1] && render_model_files[RENDER_MESH_SHED][2],
                   "the forge and the shed shapes name a model for the levels 1 to 3");
        TEST_CHECK(counts, render_model_files[RENDER_MESH_TOWER][0] && !render_model_files[RENDER_MESH_TOWER][1],
                   "the tower shape names a model for level 1: the bronze sparrow tower");
        TEST_CHECK(counts, render_model_files[RENDER_MESH_DOCK][0] && !render_model_files[RENDER_MESH_DOCK][1],
                   "the dock shape names a model for level 1: the shipyard");
        TEST_CHECK(counts, render_model_files[RENDER_MESH_SILO][0] && !render_model_files[RENDER_MESH_SILO][1] &&
                           render_model_files[RENDER_MESH_OFFICE][0] && !render_model_files[RENDER_MESH_OFFICE][1],
                   "the silo and the office shapes name a model for level 1: the granary and the tally office");
        TEST_CHECK(counts, render_model_files[RENDER_MESH_CAMP][0] && !render_model_files[RENDER_MESH_CAMP][1] &&
                           render_model_files[RENDER_MESH_WATCHTOWER][0] && !render_model_files[RENDER_MESH_WATCHTOWER][1] &&
                           !render_model_files[RENDER_MESH_GATE][0] && !render_model_files[RENDER_MESH_PORT][0],
                   "the camp and the watchtower shapes name a model for level 1, and the pass and the port shapes name none");
        TEST_CHECK(counts, render_model_level_index(0) == 0 && render_model_level_index(1) == 0 && render_model_level_index(2) == 1 &&
                           render_model_level_index(3) == 2 && render_model_level_index(9) == RENDER_MODEL_LEVELS - 1,
                   "level 0 and 1 use the first model column, and a level above the table uses the last");
        {
            memory_arena level_frame = test_arena_create(Megabytes(1));
            render_commands *level_rc = render_commands_begin(&level_frame, vw, vh);
            render_push_facility(level_rc, RENDER_MESH_STALL, RENDER_MODEL_GREAT_MARKET, 3, v3_make(1, 0, 1), 0.0f, 1.6f, 0xFFFFFFFFu);
            render_push_mesh(level_rc, RENDER_MESH_HALL, v3_make(2, 0, 1), 0.0f, 1.0f, 0xFFFFFFFFu);
            TEST_CHECK(counts, level_rc->mesh_instance_count == 2 && level_rc->mesh_instances[0].level == 3 &&
                               level_rc->mesh_instances[0].mesh_id == RENDER_MESH_STALL && level_rc->mesh_instances[0].art_sprite == RENDER_NO_ART &&
                               level_rc->mesh_instances[0].model == RENDER_MODEL_GREAT_MARKET &&
                               level_rc->mesh_instances[1].level == 0 && level_rc->mesh_instances[1].model == RENDER_MESH_HALL,
                       "a facility keeps its level and the model of its kind, and a plain mesh has no level and the model of its shape");
            test_arena_destroy(&level_frame);
        }
        camera near_cam = camera_default(10.0f, 10.0f);
        TEST_CHECK(counts, render_shape_models_on(&near_cam), "the camera at the default distance shows the models of the shapes");
        near_cam.distance = RENDER_SHAPE_MODEL_MAX_DISTANCE + 1.0f;
        TEST_CHECK(counts, !render_shape_models_on(&near_cam), "a camera farther than the limit shows the placeholder shapes");
        camera view_cam = camera_default(10.0f, 10.0f);   /* yaw 0: the eye is on the +Z side of the target */
        m4 view_vp = camera_view_projection(&view_cam, vw, vh);
        render_mesh_instance seen = market;
        seen.position = v3_make(10.0f, 0.0f, 10.0f);
        render_mesh_instance behind = seen, aside = seen, at_edge = seen;
        behind.position = v3_add(camera_eye(&view_cam), v3_make(0.0f, 0.0f, 20.0f));
        aside.position.x = 10.0f + 200.0f;
        f32 edge_x = 0.0f;
        for (f32 x = 0.0f; x < 200.0f; x += 0.25f)
        {
            render_mesh_instance probe = seen;
            probe.position.x = 10.0f + x;
            f32 edge_px = 0.0f, edge_py = 0.0f;
            if (camera_project(view_vp, probe.position, vw, vh, &edge_px, &edge_py) && edge_px > vw) { edge_x = 10.0f + x; break; }
        }
        at_edge.position.x = edge_x + 0.5f * at_edge.scale;   /* the centre is off the screen, the sphere still crosses the edge */
        TEST_CHECK(counts, render_model_in_view(view_vp, &seen) && !render_model_in_view(view_vp, &behind) && !render_model_in_view(view_vp, &aside),
                   "a model at the target is in the view, and a model behind the camera or far to the side is not");
        TEST_CHECK(counts, edge_x > 0.0f && render_model_in_view(view_vp, &at_edge),
                   "a model with its centre just off the screen but its sphere across the edge stays in the view");
        render_mesh_instance base = {};
        base.mesh_id = RENDER_MESH_SETTLEMENT; base.art_sprite = RENDER_SPRITE_BASE_CITY;
        base.position = v3_make(3.0f, 0.7f, 4.0f); base.scale = 1.0f; base.tint = 0xFF3050C0u;
        camera front = camera_default(3.0f, 4.0f);   /* yaw 0: the eye is on the +Z side */
        render_billboard card = render_base_billboard(&base, &front, 0.75f);
        TEST_CHECK(counts, card.sprite_id == RENDER_SPRITE_BASE_CITY && card.tint == 0xFF3050C0u, "the card shows the art of the base in the colour of the owner");
        TEST_CHECK(counts, near_f32(card.width, city.width, 1e-5f) && near_f32(card.height, city.width * 0.75f, 1e-5f),
                   "the card has the width of its kind and keeps the aspect of the picture");
        TEST_CHECK(counts, near_f32(card.position.x, 3.0f, 1e-4f) && near_f32(card.position.y, 0.7f, 1e-5f) && near_f32(card.position.z, 4.0f + city.front, 1e-4f),
                   "the bottom edge stands on the ground at the front of the city, toward the camera");
        b32 toward_eye = true;
        for (s32 step = -4; step <= 4; ++step)
        {
            camera turned = camera_default(3.0f, 4.0f);
            turned.yaw = (f32)step * 0.7f;
            camera_clamp(&turned);
            base.scale = 1.5f;
            card = render_base_billboard(&base, &turned, 1.0f);
            v3 shift = v3_sub(card.position, base.position);
            v3 eye_side = v3_sub(camera_eye(&turned), base.position);
            eye_side.y = 0.0f;
            toward_eye &= near_f32(v3_length(shift), city.front * 1.5f, 1e-4f) && near_f32(shift.y, 0.0f, 1e-6f) && v3_dot(shift, eye_side) > 0.0f;
            toward_eye &= near_f32(card.width, city.width * 1.5f, 1e-5f);
        }
        TEST_CHECK(counts, toward_eye, "at each yaw the card moves toward the camera, and the scale sizes it");
    }

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

    /* the placeholder shapes of the facility slots: each mesh id has a
       closed list of triangles inside one hex, on the ground */
    b32 shapes_ok = true;
    for (u32 mesh_id = 0; mesh_id < RENDER_MESH_COUNT; ++mesh_id)
    {
        placeholder_mesh shape = {};
        if (!placeholder_mesh_build(mesh_id, &arena, &shape) || shape.index_count % 3 != 0) { shapes_ok = false; continue; }
        for (u32 i = 0; i < shape.index_count; ++i) if (shape.indices[i] >= shape.vertex_count) shapes_ok = false;
        for (u32 i = 0; i < shape.vertex_count; ++i)
        {
            v3 at = shape.vertices[i].position;
            if (at.x < -0.55f || at.x > 0.55f || at.z < -0.55f || at.z > 0.55f || at.y < 0.0f || at.y > 1.2f) shapes_ok = false;
            f32 length = v3_dot(shape.vertices[i].normal, shape.vertices[i].normal);
            if (length < 0.99f || length > 1.01f) shapes_ok = false;
        }
    }
    TEST_CHECK(counts, shapes_ok, "every placeholder mesh builds inside its hex with unit normals");
    placeholder_mesh unknown = {};
    TEST_CHECK(counts, !placeholder_mesh_build(RENDER_MESH_COUNT, &arena, &unknown), "an unknown mesh id builds nothing");
    placeholder_mesh scaffold = {}, stall = {};
    placeholder_mesh_build(RENDER_MESH_SCAFFOLD, &arena, &scaffold);
    placeholder_mesh_build(RENDER_MESH_STALL, &arena, &stall);
    TEST_CHECK(counts, scaffold.index_count != stall.index_count, "the scaffold and a facility are different shapes");
    /* The gate of a pass has a passage: nothing stands between the pillars
       below the lintel. The port has a quay on the ground. */
    placeholder_mesh gate = {}, port = {};
    b32 passage = placeholder_mesh_build(RENDER_MESH_GATE, &arena, &gate) && gate.vertex_count > 0;
    f32 top = 0.0f;
    for (u32 i = 0; i < gate.vertex_count; ++i)
    {
        v3 at = gate.vertices[i].position;
        if (at.x > -0.2f && at.x < 0.2f && at.y < 0.6f) passage = false;   /* a corner inside the passage, below the lintel */
        if (at.y > top) top = at.y;
    }
    TEST_CHECK(counts, passage && top > 0.8f, "the gate of a pass is open below its lintel and has a roof");
    b32 quay = placeholder_mesh_build(RENDER_MESH_PORT, &arena, &port) && port.vertex_count > 0;
    u32 ground = 0;
    for (u32 i = 0; i < port.vertex_count; ++i) ground += port.vertices[i].position.y == 0.0f;
    TEST_CHECK(counts, quay && ground >= 8 && gate.index_count != port.index_count, "the port has a quay on the ground and differs from the gate");

    /* The mesh file of a model (misc/glb_import.py): "HXM1", the vertex
       count, the index count, the vertices and the indices. Each renderer
       reads it through this parser. */
    struct { char magic[4]; u32 vertex_count, index_count; render_mesh_vertex vertices[3]; u32 indices[3]; } file = {};
    memcpy(file.magic, "HXM1", 4);
    file.vertex_count = 3;
    file.index_count  = 3;
    file.vertices[1].position = v3_make(1, 0, 0);
    file.vertices[2].position = v3_make(0, 1, 0);
    file.indices[0] = 0; file.indices[1] = 1; file.indices[2] = 2;
    render_mesh_file parsed = {};
    b32 parsed_ok = render_mesh_file_parse((const u8 *)&file, sizeof(file), &parsed);
    TEST_CHECK(counts, parsed_ok && parsed.vertex_count == 3 && parsed.index_count == 3 &&
                       parsed.vertices[2].position.y == 1.0f && parsed.indices[2] == 2,
               "the mesh file parser finds the vertices and the indices");
    TEST_CHECK(counts, !render_mesh_file_parse((const u8 *)&file, sizeof(file) - 4, &parsed),
               "the mesh file parser refuses a file shorter than its counts");
    file.index_count = 0x40000000u;
    TEST_CHECK(counts, !render_mesh_file_parse((const u8 *)&file, sizeof(file), &parsed),
               "the mesh file parser refuses an index count past the end of the file");
    file.index_count = 3;
    file.magic[3] = '2';
    TEST_CHECK(counts, !render_mesh_file_parse((const u8 *)&file, sizeof(file), &parsed),
               "the mesh file parser refuses a file without the HXM1 tag");
    test_arena_destroy(&arena);
    unit_render_unit_tests(counts);
}

/* The figures of a unit (docs/GRAPHICS.md 5.1): the model of each class
   of unit, the crowd of a soldier class, the one model of a siege weapon
   and a ship, and the billboard when the pack has no model. */
internal b32
render_test_figures_in_hex(const render_mesh_instance *figures, u32 count, v3 centre)
{
    for (u32 index = 0; index < count; ++index)
    {
        f32 dx = figures[index].position.x - centre.x, dz = figures[index].position.z - centre.z;
        if (sqrtf(dx * dx + dz * dz) > 0.8f || figures[index].position.y != centre.y) return false;
    }
    return true;
}

internal void
unit_render_unit_tests(test_counts *counts)
{
    TEST_CHECK(counts, render_unit_model_of(RENDER_SPRITE_SWORD) == RENDER_MODEL_UNIT_SWORD &&
                       render_unit_model_of(RENDER_SPRITE_CAVALRY) == RENDER_MODEL_UNIT_CAVALRY &&
                       render_unit_model_of(RENDER_SPRITE_TRANSPORT) == RENDER_MODEL_UNIT_TRANSPORT &&
                       render_unit_model_of(RENDER_SPRITE_UNIT) == RENDER_MODEL_COUNT &&
                       render_unit_model_of(RENDER_SPRITE_BASE_CITY) == RENDER_MODEL_COUNT,
               "each class sprite has the model of its class; the figure without a class and a base have none");
    TEST_CHECK(counts, !strcmp(render_model_files[RENDER_MODEL_UNIT_SWORD][0], "models/unit_sword") &&
                       !strcmp(render_model_files[RENDER_MODEL_UNIT_BATTLE_SHIP][0], "models/unit_battle_ship") &&
                       !strcmp(render_model_files[RENDER_MODEL_UNIT_BANNER][0], "models/unit_banner"),
               "the unit models are models/unit_<class> of the pack");
    TEST_CHECK(counts, !strcmp(render_unit_model_art_file(RENDER_MODEL_UNIT_PIKE), "sprites/art/pike.png") &&
                       !strcmp(render_unit_model_art_file(RENDER_MODEL_UNIT_BANNER), "sprites/art/banner.png") &&
                       render_unit_model_art_file(RENDER_MODEL_FORT) == 0,
               "each unit model names the art sprite of its class");
    TEST_CHECK(counts, render_unit_model_allowed(false, false) && render_unit_model_allowed(true, true) &&
                       render_unit_model_allowed(false, true) && !render_unit_model_allowed(true, false),
               "a pack with its own unit art and no unit model keeps its art");

    render_mesh_instance figures[64];
    v3 centre = v3_make(3.0f, 0.5f, -2.0f);
    render_unit sword = {};
    sword.sprite_id = RENDER_SPRITE_SWORD; sword.position = centre; sword.yaw = 0.7f; sword.tint = 0x112233FFu; sword.seed = 7;
    sword.soldiers = 1000;
    u32 small = render_unit_figures(&sword, figures, ArrayCount(figures));
    TEST_CHECK(counts, small == 4 + 1 && figures[small - 1].model == RENDER_MODEL_UNIT_BANNER && figures[0].model == RENDER_MODEL_UNIT_SWORD,
               "1000 soldiers are 4 figures and a banner");
    b32 alike = true;
    for (u32 index = 0; index < small; ++index)
    {
        alike &= figures[index].mesh_id == RENDER_MESH_UNIT_FIGURE && figures[index].tint == sword.tint && figures[index].yaw == sword.yaw &&
                 figures[index].art_sprite == RENDER_NO_ART && figures[index].level == 0;
    }
    TEST_CHECK(counts, alike && near_f32(figures[0].scale, RENDER_UNIT_FIGURE_HEIGHT, 1e-6f) && figures[small - 1].scale > figures[0].scale,
               "each figure has the tint and the yaw of the unit; the banner is taller than a soldier");
    sword.soldiers = 0;
    TEST_CHECK(counts, render_unit_figures(&sword, figures, ArrayCount(figures)) == RENDER_UNIT_MIN_FIGURES + 1, "a unit without soldiers keeps the least figures");
    sword.soldiers = 5000;
    TEST_CHECK(counts, render_unit_figures(&sword, figures, ArrayCount(figures)) == 20 + 1, "5000 soldiers are 20 figures and a banner");
    sword.soldiers = 1000000;
    u32 full = render_unit_figures(&sword, figures, ArrayCount(figures));
    TEST_CHECK(counts, full == RENDER_UNIT_MAX_FIGURES + 1 && render_test_figures_in_hex(figures, full, centre),
               "a large unit stops at the most figures, and each figure stands inside the hex");
    render_mesh_instance again[64];
    render_unit_figures(&sword, again, ArrayCount(again));
    b32 same = true;
    for (u32 index = 0; index < full; ++index) same &= v3_length(v3_sub(again[index].position, figures[index].position)) == 0.0f;
    sword.seed = 8;
    render_unit_figures(&sword, again, ArrayCount(again));
    b32 moved = false;
    for (u32 index = 0; index < full; ++index) moved |= v3_length(v3_sub(again[index].position, figures[index].position)) > 0.0f;
    TEST_CHECK(counts, same && moved, "the same seed gives the same figures, another seed moves them a little");
    TEST_CHECK(counts, render_unit_figures(&sword, figures, 3) == 3, "the figures stop at the space of the buffer");

    render_unit cavalry = sword;
    cavalry.sprite_id = RENDER_SPRITE_CAVALRY;
    u32 riders = render_unit_figures(&cavalry, figures, ArrayCount(figures));
    TEST_CHECK(counts, riders >= RENDER_UNIT_MIN_FIGURES + 1 && riders < full && render_test_figures_in_hex(figures, riders, centre),
               "the cavalry has fewer and larger figures, inside the hex");

    render_unit ship = sword;
    ship.sprite_id = RENDER_SPRITE_TOWER_SHIP;
    TEST_CHECK(counts, render_unit_figures(&ship, figures, ArrayCount(figures)) == 1 && figures[0].model == RENDER_MODEL_UNIT_TOWER_SHIP &&
                       v3_length(v3_sub(figures[0].position, centre)) == 0.0f && figures[0].scale >= 1.0f,
               "a ship is one model at the centre of the hex");
    render_unit ram = sword;
    ram.sprite_id = RENDER_SPRITE_RAM;
    TEST_CHECK(counts, render_unit_figures(&ram, figures, ArrayCount(figures)) == 1 && figures[0].model == RENDER_MODEL_UNIT_RAM,
               "a siege weapon is one model");
    render_unit plain = sword;
    plain.sprite_id = RENDER_SPRITE_UNIT;
    TEST_CHECK(counts, render_unit_figures(&plain, figures, ArrayCount(figures)) == 0, "the figure without a class has no model");

    /* The platform places the units: the figures when the pack has the
       model of the class, else the billboard of today. */
    memory_arena arena = test_arena_create(Megabytes(4));
    b32 ready[RENDER_MODEL_COUNT] = {};
    render_commands *rc = render_commands_begin(&arena, 800.0f, 600.0f);
    rc->cam = camera_default(0.0f, 0.0f);
    render_push_unit(rc, sword.sprite_id, centre, sword.yaw, 1000, sword.tint, 1);
    render_push_unit(rc, ship.sprite_id, centre, 0.0f, 1000, sword.tint, 2);
    TEST_CHECK(counts, rc->unit_count == 2 && rc->mesh_instance_count == 0 && rc->billboard_count == 0, "a pushed unit waits for the platform");
    render_place_units(rc, ready);
    TEST_CHECK(counts, rc->mesh_instance_count == 0 && rc->billboard_count == 2 && rc->billboards[0].sprite_id == RENDER_SPRITE_SWORD &&
                       near_f32(rc->billboards[0].width, RENDER_UNIT_SPRITE_SIZE, 1e-6f) && rc->billboards[1].sprite_id == RENDER_SPRITE_TOWER_SHIP,
               "without a model each unit is its billboard");

    rc = render_commands_begin(&arena, 800.0f, 600.0f);
    rc->cam = camera_default(0.0f, 0.0f);
    ready[RENDER_MODEL_UNIT_SWORD] = true;
    render_push_unit(rc, sword.sprite_id, centre, sword.yaw, 1000, sword.tint, 1);
    render_push_unit(rc, ship.sprite_id, centre, 0.0f, 1000, sword.tint, 2);
    render_place_units(rc, ready);
    TEST_CHECK(counts, rc->mesh_instance_count == 4 && rc->billboard_count == 1 && rc->billboards[0].sprite_id == RENDER_SPRITE_TOWER_SHIP,
               "a class with a model is its figures, without the banner when the banner has no model");

    rc = render_commands_begin(&arena, 800.0f, 600.0f);
    rc->cam = camera_default(0.0f, 0.0f);
    rc->cam.distance = RENDER_SHAPE_MODEL_MAX_DISTANCE + 10.0f;
    render_push_unit(rc, sword.sprite_id, centre, sword.yaw, 1000, sword.tint, 1);
    render_place_units(rc, ready);
    TEST_CHECK(counts, rc->mesh_instance_count == 0 && rc->billboard_count == 1, "a far camera shows the billboard");

    rc = render_commands_begin(&arena, 800.0f, 600.0f);
    rc->cam = camera_default(0.0f, 0.0f);
    rc->mesh_instance_count = RENDER_MAX_MESH_INSTANCES - 2;
    render_push_unit(rc, sword.sprite_id, centre, sword.yaw, 1000, sword.tint, 1);
    render_place_units(rc, ready);
    TEST_CHECK(counts, rc->mesh_instance_count == RENDER_MAX_MESH_INSTANCES - 2 && rc->billboard_count == 1,
               "a unit whose figures do not fit is its billboard");
    TEST_CHECK(counts, RENDER_MAX_MESH_INSTANCES >= 16384, "the buffer holds the figures of many units");
    test_arena_destroy(&arena);
}
