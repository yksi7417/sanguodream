/* Unit tests for code/render/terrain_mesh.h: counts, placement, normals. */

internal void
unit_terrain_mesh_tests(test_counts *counts)
{
    memory_arena arena = test_arena_create(Megabytes(8));

    u8 kind[6 * 4];
    u8 level[6 * 4];
    for (u32 i = 0; i < 24; ++i) { kind[i] = (u8)(i % 3); level[i] = (u8)(i % 2); }
    render_terrain terrain;
    terrain.width = 6; terrain.height = 4; terrain.kind = kind; terrain.level = level; terrain.version = 1;

    terrain_mesh mesh = terrain_mesh_build(&terrain, &arena);
    TEST_CHECK(counts, mesh.vertex_count == 24 * TERRAIN_VERTS_PER_CELL, "one vertex block per cell");
    TEST_CHECK(counts, mesh.index_count == 24 * TERRAIN_TRIS_PER_CELL * 3, "thirty triangles per cell");

    b32 indices_in_range = true;
    for (u32 i = 0; i < mesh.index_count; ++i) if (mesh.indices[i] >= mesh.vertex_count) indices_in_range = false;
    TEST_CHECK(counts, indices_in_range, "every index points at a vertex");

    /* the top center of cell 7 sits at its hex center and at its level height */
    hex_offset o7; o7.col = 7 % 6; o7.row = 7 / 6;
    hex_coord h7 = hex_from_offset(o7);
    v3 expected = hex_world_center(h7, terrain_level_y(level[7]));
    terrain_vertex *top = mesh.vertices + 7 * TERRAIN_VERTS_PER_CELL;
    TEST_CHECK(counts, near_f32(top->position.x, expected.x, 1e-5f) && near_f32(top->position.z, expected.z, 1e-5f) &&
                       near_f32(top->position.y, expected.y, 1e-5f), "top center of a cell matches hex_world_center");
    TEST_CHECK(counts, top->normal.y > 0.99f, "top normal points up");
    TEST_CHECK(counts, near_f32(top->tile, (f32)kind[7], 0.0f) && near_f32(top->cell, 7.0f, 0.0f), "vertex carries its tile and cell");

    /* side normals are horizontal and point away from the center */
    b32 sides_ok = true;
    for (u32 k = 0; k < 6; ++k)
    {
        terrain_vertex *side = mesh.vertices + 7 * TERRAIN_VERTS_PER_CELL + 13 + k * 3;
        v3 out = v3_sub(side->position, expected);
        out.y = 0;
        if (!near_f32(side->normal.y, 0.0f, 1e-5f) || v3_dot(side->normal, out) <= 0.0f) sides_ok = false;
    }
    TEST_CHECK(counts, sides_ok, "side normals are horizontal and outward");

    TEST_CHECK(counts, mesh.min_x < 0.0f && mesh.max_x > 8.0f && mesh.min_z < 0.0f && mesh.max_z > 4.0f, "footprint covers the map");

    render_terrain empty = {};
    terrain_mesh none = terrain_mesh_build(&empty, &arena);
    TEST_CHECK(counts, none.vertex_count == 0 && none.index_count == 0, "an empty terrain gives an empty mesh");

    test_arena_destroy(&arena);
}
