#ifndef TERRAIN_MESH_H
#define TERRAIN_MESH_H
/* Builds the terrain mesh: one hex prism per cell, with a bevelled top, from
   the per-cell kind and level of a render_terrain. Pure code over arenas; the
   platform uploads the result. Vertex attributes carry the cell index (for
   the overlay texture) and the material tile (for the atlas). */

#include "render/render.h"

#define TERRAIN_STEP        0.35f    /* world units per level */
#define TERRAIN_TOP_INSET   0.88f    /* radius of the flat top, as a fraction of the cell */
#define TERRAIN_BEVEL_DROP  0.12f    /* how far the bevel ring sits below the top */
#define TERRAIN_FLOOR_Y    -0.6f     /* the sides go down to here */
#define TERRAIN_TRIS_PER_CELL  30    /* 6 top + 12 bevel + 12 sides */
#define TERRAIN_VERTS_PER_CELL 31    /* 1 center + 6 top + 6 bevel + 6*3 side corners */

struct terrain_vertex
{
    v3  position;
    v3  normal;
    f32 u, v;         /* within the material tile */
    f32 cell;         /* cell index, read back as a float in the shader */
    f32 tile;         /* material tile index */
};

struct terrain_mesh
{
    terrain_vertex *vertices;
    u32             vertex_count;
    u32            *indices;
    u32             index_count;
    f32             min_x, max_x, min_z, max_z;   /* footprint on the ground */
};

internal f32
terrain_level_y(u8 level)
{
    return (f32)level * TERRAIN_STEP;
}

/* Corner k of a flat-top hex of the given radius around center. */
internal v3
terrain_corner(v3 center, f32 radius, u32 k, f32 y)
{
    f32 angle = (60.0f * (f32)k) * RENDER_PI / 180.0f;
    return v3_make(center.x + radius * cosf(angle), y, center.z + radius * sinf(angle));
}

internal void
terrain_put(terrain_mesh *mesh, u32 at, v3 p, v3 n, f32 u, f32 v, f32 cell, f32 tile)
{
    terrain_vertex *tv = mesh->vertices + at;
    tv->position = p; tv->normal = n; tv->u = u; tv->v = v; tv->cell = cell; tv->tile = tile;
}

internal void
terrain_tri(terrain_mesh *mesh, u32 a, u32 b, u32 c)
{
    mesh->indices[mesh->index_count++] = a;
    mesh->indices[mesh->index_count++] = b;
    mesh->indices[mesh->index_count++] = c;
}

internal terrain_mesh
terrain_mesh_build(render_terrain *terrain, memory_arena *arena)
{
    terrain_mesh mesh = {};
    u32 cell_count = terrain->width * terrain->height;
    if (!cell_count) return mesh;
    mesh.vertices = PushArray(arena, cell_count * TERRAIN_VERTS_PER_CELL, terrain_vertex);
    mesh.indices  = PushArray(arena, cell_count * TERRAIN_TRIS_PER_CELL * 3, u32);
    mesh.min_x = mesh.min_z =  1e9f;
    mesh.max_x = mesh.max_z = -1e9f;

    v3 up = v3_make(0, 1, 0);
    for (u32 index = 0; index < cell_count; ++index)
    {
        hex_offset o;
        o.col = (s32)(index % terrain->width);
        o.row = (s32)(index / terrain->width);
        hex_coord h = hex_from_offset(o);
        f32 top_y  = terrain_level_y(terrain->level[index]);
        f32 tile   = (f32)terrain->kind[index];
        f32 cellf  = (f32)index;
        v3 center  = hex_world_center(h, top_y);
        u32 base   = mesh.vertex_count;

        /* top: center + 6 corners at the inset radius */
        terrain_put(&mesh, base, center, up, 0.5f, 0.5f, cellf, tile);
        for (u32 k = 0; k < 6; ++k)
        {
            v3 c = terrain_corner(center, TERRAIN_TOP_INSET, k, top_y);
            terrain_put(&mesh, base + 1 + k, c, up, 0.5f + (c.x - center.x) * 0.5f, 0.5f + (c.z - center.z) * 0.5f, cellf, tile);
        }
        for (u32 k = 0; k < 6; ++k) terrain_tri(&mesh, base, base + 1 + k, base + 1 + (k + 1) % 6);

        /* bevel ring: full radius, a little lower; normals lean outward */
        for (u32 k = 0; k < 6; ++k)
        {
            v3 c = terrain_corner(center, 1.0f, k, top_y - TERRAIN_BEVEL_DROP);
            v3 n = v3_normalize(v3_make(c.x - center.x, 0.7f, c.z - center.z));
            terrain_put(&mesh, base + 7 + k, c, n, 0.5f + (c.x - center.x) * 0.5f, 0.5f + (c.z - center.z) * 0.5f, cellf, tile);
        }
        for (u32 k = 0; k < 6; ++k)
        {
            u32 k1 = (k + 1) % 6;
            terrain_tri(&mesh, base + 1 + k, base + 7 + k, base + 7 + k1);
            terrain_tri(&mesh, base + 1 + k, base + 7 + k1, base + 1 + k1);
        }

        /* sides: each edge is a quad from the bevel ring down to the floor */
        u32 side_base = base + 13;
        for (u32 k = 0; k < 6; ++k)
        {
            u32 k1 = (k + 1) % 6;
            v3 a = terrain_corner(center, 1.0f, k,  top_y - TERRAIN_BEVEL_DROP);
            v3 b = terrain_corner(center, 1.0f, k1, top_y - TERRAIN_BEVEL_DROP);
            v3 edge_mid = v3_scale(v3_add(a, b), 0.5f);
            v3 n = v3_normalize(v3_make(edge_mid.x - center.x, 0.0f, edge_mid.z - center.z));
            v3 a_low = v3_make(a.x, TERRAIN_FLOOR_Y, a.z);
            v3 b_low = v3_make(b.x, TERRAIN_FLOOR_Y, b.z);
            u32 v0 = side_base + k * 3;
            terrain_put(&mesh, v0,     a,     n, 0.0f, 0.0f, cellf, tile);
            terrain_put(&mesh, v0 + 1, b,     n, 1.0f, 0.0f, cellf, tile);
            terrain_put(&mesh, v0 + 2, a_low, n, 0.0f, 1.0f, cellf, tile);
            /* b_low is shared with the next edge's a_low only by value; one extra vertex keeps it simple */
            (void)b_low;
        }
        /* the side quads need b_low too: reuse the next edge's a_low vertex */
        for (u32 k = 0; k < 6; ++k)
        {
            u32 k1 = (k + 1) % 6;
            u32 a_top = side_base + k * 3, b_top = side_base + k * 3 + 1;
            u32 a_low = side_base + k * 3 + 2, b_low = side_base + k1 * 3 + 2;
            terrain_tri(&mesh, a_top, b_top, b_low);
            terrain_tri(&mesh, a_top, b_low, a_low);
        }
        mesh.vertex_count += TERRAIN_VERTS_PER_CELL;

        if (center.x - 1.0f < mesh.min_x) mesh.min_x = center.x - 1.0f;
        if (center.x + 1.0f > mesh.max_x) mesh.max_x = center.x + 1.0f;
        if (center.z - 1.0f < mesh.min_z) mesh.min_z = center.z - 1.0f;
        if (center.z + 1.0f > mesh.max_z) mesh.max_z = center.z + 1.0f;
    }
    return mesh;
}

#endif /* TERRAIN_MESH_H */
