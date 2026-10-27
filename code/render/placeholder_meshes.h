#ifndef PLACEHOLDER_MESHES_H
#define PLACEHOLDER_MESHES_H
/* The placeholder models of the scene: one shape for each render_mesh_id,
   built from boxes, roofs and prisms. The platform makes a GPU buffer of
   each shape at startup, and the scene instances them with a tint. A
   mesh file of the content pack replaces the settlement. The art of the
   facilities comes later (docs/GRAPHICS.md).

   Every shape is one unit wide at most, with Y up and the ground at y = 0.
   A face lists its corners counter-clockwise, seen from the outside, so
   the normal points out and the back face culls. */

#include "render/render.h"

struct render_mesh_vertex
{
    v3  position;
    v3  normal;
    f32 u, v;
};

/* The mesh file of a model (misc/glb_import.py): "HXM1", the u32 vertex
   count, the u32 index count, the vertices as render_mesh_vertex, then
   the u32 indices. The parser points into the data and copies nothing.
   Each renderer makes its GPU buffers from the result. */
struct render_mesh_file
{
    const render_mesh_vertex *vertices;
    u32                       vertex_count;
    const u32                *indices;
    u32                       index_count;
};

#define RENDER_MESH_FILE_HEADER_SIZE 12

internal b32
render_mesh_file_parse(const u8 *data, u64 size, render_mesh_file *out)
{
    if (!data || size < RENDER_MESH_FILE_HEADER_SIZE || memcmp(data, "HXM1", 4) != 0) return false;
    u32 vertex_count = 0, index_count = 0;
    memcpy(&vertex_count, data + 4, 4);
    memcpy(&index_count, data + 8, 4);
    u64 vertex_bytes = (u64)vertex_count * sizeof(render_mesh_vertex);
    u64 needed = RENDER_MESH_FILE_HEADER_SIZE + vertex_bytes + (u64)index_count * 4;
    if (size < needed || !vertex_count || !index_count) return false;
    out->vertices     = (const render_mesh_vertex *)(data + RENDER_MESH_FILE_HEADER_SIZE);
    out->vertex_count = vertex_count;
    out->indices      = (const u32 *)(data + RENDER_MESH_FILE_HEADER_SIZE + vertex_bytes);
    out->index_count  = index_count;
    return true;
}

#define PLACEHOLDER_MESH_MAX_VERTICES 1024
#define PLACEHOLDER_MESH_MAX_INDICES  2048

struct placeholder_mesh
{
    render_mesh_vertex *vertices;
    u32                 vertex_count;
    u32                *indices;
    u32                 index_count;
};

internal void
placeholder_tri(placeholder_mesh *mesh, v3 a, v3 b, v3 c)
{
    if (mesh->vertex_count + 3 > PLACEHOLDER_MESH_MAX_VERTICES || mesh->index_count + 3 > PLACEHOLDER_MESH_MAX_INDICES) return;
    v3 normal = v3_normalize(v3_cross(v3_sub(b, a), v3_sub(c, a)));
    v3 corners[3] = { a, b, c };
    u32 base = mesh->vertex_count;
    for (u32 k = 0; k < 3; ++k)
    {
        render_mesh_vertex *vertex = mesh->vertices + mesh->vertex_count++;
        vertex->position = corners[k];
        vertex->normal   = normal;
        vertex->u        = k == 1 ? 1.0f : 0.0f;
        vertex->v        = k == 2 ? 1.0f : 0.0f;
    }
    mesh->indices[mesh->index_count++] = base;
    mesh->indices[mesh->index_count++] = base + 1;
    mesh->indices[mesh->index_count++] = base + 2;
}

internal void
placeholder_quad(placeholder_mesh *mesh, v3 a, v3 b, v3 c, v3 d)
{
    placeholder_tri(mesh, a, b, c);
    placeholder_tri(mesh, a, c, d);
}

/* A box without its bottom face. */
internal void
placeholder_box(placeholder_mesh *mesh, f32 x0, f32 y0, f32 z0, f32 x1, f32 y1, f32 z1)
{
    placeholder_quad(mesh, v3_make(x1, y0, z0), v3_make(x1, y1, z0), v3_make(x1, y1, z1), v3_make(x1, y0, z1));   /* +X */
    placeholder_quad(mesh, v3_make(x0, y0, z1), v3_make(x0, y1, z1), v3_make(x0, y1, z0), v3_make(x0, y0, z0));   /* -X */
    placeholder_quad(mesh, v3_make(x1, y0, z1), v3_make(x1, y1, z1), v3_make(x0, y1, z1), v3_make(x0, y0, z1));   /* +Z */
    placeholder_quad(mesh, v3_make(x0, y0, z0), v3_make(x0, y1, z0), v3_make(x1, y1, z0), v3_make(x1, y0, z0));   /* -Z */
    placeholder_quad(mesh, v3_make(x1, y1, z1), v3_make(x1, y1, z0), v3_make(x0, y1, z0), v3_make(x0, y1, z1));   /* top */
}

/* A pyramid roof over a rectangle, with the apex above its centre. */
internal void
placeholder_pyramid(placeholder_mesh *mesh, f32 x0, f32 z0, f32 x1, f32 z1, f32 y_base, f32 y_apex)
{
    v3 apex = v3_make((x0 + x1) * 0.5f, y_apex, (z0 + z1) * 0.5f);
    placeholder_tri(mesh, v3_make(x1, y_base, z1), apex, v3_make(x0, y_base, z1));   /* +Z */
    placeholder_tri(mesh, v3_make(x1, y_base, z0), apex, v3_make(x1, y_base, z1));   /* +X */
    placeholder_tri(mesh, v3_make(x0, y_base, z0), apex, v3_make(x1, y_base, z0));   /* -Z */
    placeholder_tri(mesh, v3_make(x0, y_base, z1), apex, v3_make(x0, y_base, z0));   /* -X */
}

/* A gable roof with the ridge along X. */
internal void
placeholder_gable(placeholder_mesh *mesh, f32 x0, f32 z0, f32 x1, f32 z1, f32 y_base, f32 y_ridge)
{
    f32 zc = (z0 + z1) * 0.5f;
    placeholder_quad(mesh, v3_make(x1, y_base, z1), v3_make(x1, y_ridge, zc), v3_make(x0, y_ridge, zc), v3_make(x0, y_base, z1));   /* +Z slope */
    placeholder_quad(mesh, v3_make(x0, y_base, z0), v3_make(x0, y_ridge, zc), v3_make(x1, y_ridge, zc), v3_make(x1, y_base, z0));   /* -Z slope */
    placeholder_tri(mesh, v3_make(x1, y_base, z0), v3_make(x1, y_ridge, zc), v3_make(x1, y_base, z1));                             /* +X end */
    placeholder_tri(mesh, v3_make(x0, y_base, z1), v3_make(x0, y_ridge, zc), v3_make(x0, y_base, z0));                             /* -X end */
}

/* A prism with "sides" sides about the Y axis, closed by a cone to y_apex,
   or by a flat cap when y_apex equals y1. */
internal void
placeholder_prism(placeholder_mesh *mesh, f32 cx, f32 cz, f32 radius, u32 sides, f32 y0, f32 y1, f32 y_apex)
{
    v3 apex = v3_make(cx, y_apex, cz);
    for (u32 k = 0; k < sides; ++k)
    {
        f32 angle0 = 2.0f * RENDER_PI * (f32)k / (f32)sides;
        f32 angle1 = 2.0f * RENDER_PI * (f32)(k + 1) / (f32)sides;
        f32 x0 = cx + radius * cosf(angle0), z0 = cz + radius * sinf(angle0);
        f32 x1 = cx + radius * cosf(angle1), z1 = cz + radius * sinf(angle1);
        placeholder_quad(mesh, v3_make(x0, y0, z0), v3_make(x0, y1, z0), v3_make(x1, y1, z1), v3_make(x1, y0, z1));
        placeholder_tri(mesh, v3_make(x0, y1, z0), apex, v3_make(x1, y1, z1));
    }
}

/* The settlement: a box with a pyramid roof, one unit wide. */
internal void
placeholder_settlement(placeholder_mesh *mesh)
{
    placeholder_box(mesh, -0.45f, 0.0f, -0.45f, 0.45f, 0.5f, 0.45f);
    placeholder_pyramid(mesh, -0.45f, -0.45f, 0.45f, 0.45f, 0.5f, 0.85f);
}

/* A build in progress: four posts and the rails between them. */
internal void
placeholder_scaffold(placeholder_mesh *mesh)
{
    f32 half = 0.42f, post = 0.035f, top = 0.7f;
    f32 corners[4][2] = { { -half, -half }, { half, -half }, { half, half }, { -half, half } };
    for (u32 k = 0; k < 4; ++k)
    {
        f32 x = corners[k][0], z = corners[k][1];
        placeholder_box(mesh, x - post, 0.0f, z - post, x + post, top, z + post);
    }
    f32 levels[2] = { 0.36f, top - 0.04f };
    for (u32 level = 0; level < 2; ++level)
    {
        f32 y0 = levels[level], y1 = levels[level] + 0.04f;
        placeholder_box(mesh, -half, y0, -half - post, half, y1, -half + post);
        placeholder_box(mesh, -half, y0,  half - post, half, y1,  half + post);
        placeholder_box(mesh, -half - post, y0, -half, -half + post, y1, half);
        placeholder_box(mesh,  half - post, y0, -half,  half + post, y1, half);
    }
}

/* Fills one placeholder shape. Returns false for an unknown id. */
internal b32
placeholder_mesh_build(u32 mesh_id, memory_arena *arena, placeholder_mesh *out)
{
    placeholder_mesh mesh = {};
    mesh.vertices = PushArray(arena, PLACEHOLDER_MESH_MAX_VERTICES, render_mesh_vertex);
    mesh.indices  = PushArray(arena, PLACEHOLDER_MESH_MAX_INDICES, u32);
    switch (mesh_id)
    {
        case RENDER_MESH_SETTLEMENT:
            placeholder_settlement(&mesh);
            break;
        case RENDER_MESH_SCAFFOLD:
            placeholder_scaffold(&mesh);
            break;
        case RENDER_MESH_STALL:
            placeholder_box(&mesh, -0.22f, 0.0f, -0.22f, 0.22f, 0.28f, 0.22f);
            placeholder_box(&mesh, -0.45f, 0.28f, -0.45f, 0.45f, 0.32f, 0.45f);
            placeholder_pyramid(&mesh, -0.45f, -0.45f, 0.45f, 0.45f, 0.32f, 0.55f);
            break;
        case RENDER_MESH_FIELD:
            placeholder_box(&mesh, -0.45f, 0.0f, -0.45f, 0.45f, 0.05f, 0.45f);
            for (u32 ridge = 0; ridge < 3; ++ridge)
            {
                f32 z = -0.28f + 0.28f * (f32)ridge;
                placeholder_box(&mesh, -0.4f, 0.05f, z - 0.05f, 0.4f, 0.16f, z + 0.05f);
            }
            break;
        case RENDER_MESH_HALL:
            placeholder_box(&mesh, -0.45f, 0.0f, -0.25f, 0.45f, 0.32f, 0.25f);
            placeholder_gable(&mesh, -0.48f, -0.28f, 0.48f, 0.28f, 0.32f, 0.58f);
            break;
        case RENDER_MESH_FORGE:
            placeholder_box(&mesh, -0.35f, 0.0f, -0.35f, 0.35f, 0.38f, 0.35f);
            placeholder_pyramid(&mesh, -0.38f, -0.38f, 0.38f, 0.38f, 0.38f, 0.6f);
            placeholder_box(&mesh, 0.18f, 0.0f, -0.32f, 0.3f, 0.82f, -0.2f);
            break;
        case RENDER_MESH_DOCK:
            placeholder_box(&mesh, -0.45f, 0.0f, -0.45f, 0.45f, 0.07f, 0.45f);
            placeholder_box(&mesh, -0.32f, 0.07f, -0.12f, 0.32f, 0.3f, 0.12f);
            placeholder_box(&mesh, -0.03f, 0.3f, -0.03f, 0.03f, 0.9f, 0.03f);
            placeholder_box(&mesh, 0.03f, 0.5f, -0.01f, 0.3f, 0.78f, 0.01f);
            break;
        case RENDER_MESH_SILO:
            placeholder_prism(&mesh, 0.0f, 0.0f, 0.36f, 8, 0.0f, 0.5f, 0.8f);
            break;
        case RENDER_MESH_TOWER:
            placeholder_box(&mesh, -0.4f, 0.0f, -0.4f, 0.4f, 0.22f, 0.4f);
            placeholder_box(&mesh, -0.22f, 0.22f, -0.22f, 0.22f, 0.9f, 0.22f);
            placeholder_pyramid(&mesh, -0.3f, -0.3f, 0.3f, 0.3f, 0.9f, 1.15f);
            break;
        case RENDER_MESH_OFFICE:
            placeholder_box(&mesh, -0.4f, 0.0f, -0.4f, 0.4f, 0.3f, 0.4f);
            placeholder_box(&mesh, -0.28f, 0.3f, -0.28f, 0.28f, 0.52f, 0.28f);
            placeholder_pyramid(&mesh, -0.34f, -0.34f, 0.34f, 0.34f, 0.52f, 0.78f);
            break;
        case RENDER_MESH_SHED:
            placeholder_box(&mesh, -0.45f, 0.0f, -0.3f, 0.45f, 0.22f, 0.3f);
            placeholder_box(&mesh, -0.5f, 0.22f, -0.36f, 0.5f, 0.28f, 0.36f);
            break;
        case RENDER_MESH_GATE:
            /* Two pillars with a wall stub on each side, a lintel across the
               passage, and a gable roof on top. The passage between the
               pillars stays open down to the ground. */
            placeholder_box(&mesh, -0.42f, 0.0f, -0.12f, -0.26f, 0.7f, 0.12f);
            placeholder_box(&mesh,  0.26f, 0.0f, -0.12f,  0.42f, 0.7f, 0.12f);
            placeholder_box(&mesh, -0.52f, 0.0f, -0.08f, -0.42f, 0.4f, 0.08f);
            placeholder_box(&mesh,  0.42f, 0.0f, -0.08f,  0.52f, 0.4f, 0.08f);
            placeholder_box(&mesh, -0.48f, 0.7f, -0.16f,  0.48f, 0.82f, 0.16f);
            placeholder_gable(&mesh, -0.52f, -0.22f, 0.52f, 0.22f, 0.82f, 1.05f);
            break;
        case RENDER_MESH_PORT:
            /* A low quay, a pier that reaches over the water, two mooring
               posts and a hull moored beside the pier. */
            placeholder_box(&mesh, -0.5f, 0.0f, -0.5f, 0.1f, 0.08f, 0.5f);
            placeholder_box(&mesh,  0.1f, 0.04f, -0.08f, 0.5f, 0.1f, 0.08f);
            placeholder_box(&mesh,  0.2f, 0.1f, -0.13f, 0.26f, 0.32f, -0.07f);
            placeholder_box(&mesh,  0.4f, 0.1f, -0.13f, 0.46f, 0.32f, -0.07f);
            placeholder_box(&mesh,  0.15f, 0.0f, 0.14f, 0.5f, 0.14f, 0.3f);
            placeholder_box(&mesh,  0.31f, 0.14f, 0.21f, 0.34f, 0.6f, 0.23f);
            break;
        case RENDER_MESH_CAMP:
            /* Eight stakes on a ring around a tent. */
            for (u32 stake = 0; stake < 8; ++stake)
            {
                f32 angle = 2.0f * RENDER_PI * (f32)stake / 8.0f;
                f32 x = 0.42f * cosf(angle), z = 0.42f * sinf(angle);
                placeholder_box(&mesh, x - 0.04f, 0.0f, z - 0.04f, x + 0.04f, 0.36f, z + 0.04f);
            }
            placeholder_box(&mesh, -0.2f, 0.0f, -0.2f, 0.2f, 0.18f, 0.2f);
            placeholder_pyramid(&mesh, -0.24f, -0.24f, 0.24f, 0.24f, 0.18f, 0.52f);
            break;
        case RENDER_MESH_WATCHTOWER:
            /* Four legs, a platform with a low rail, and a pyramid roof. */
            for (u32 leg = 0; leg < 4; ++leg)
            {
                f32 x = (leg & 1) ? 0.24f : -0.24f, z = (leg & 2) ? 0.24f : -0.24f;
                placeholder_box(&mesh, x - 0.04f, 0.0f, z - 0.04f, x + 0.04f, 0.7f, z + 0.04f);
            }
            placeholder_box(&mesh, -0.32f, 0.7f, -0.32f, 0.32f, 0.78f, 0.32f);
            placeholder_box(&mesh, -0.32f, 0.78f, -0.32f, 0.32f, 0.86f, -0.28f);
            placeholder_box(&mesh, -0.32f, 0.78f, 0.28f, 0.32f, 0.86f, 0.32f);
            placeholder_pyramid(&mesh, -0.36f, -0.36f, 0.36f, 0.36f, 0.86f, 1.1f);
            break;
        case RENDER_MESH_STAGE:
            /* A raised square platform with a round drum on it. */
            placeholder_box(&mesh, -0.42f, 0.0f, -0.42f, 0.42f, 0.2f, 0.42f);
            placeholder_prism(&mesh, 0.0f, 0.0f, 0.2f, 8, 0.2f, 0.5f, 0.5f);
            placeholder_box(&mesh, 0.3f, 0.2f, 0.3f, 0.36f, 0.6f, 0.36f);
            break;
        case RENDER_MESH_WALL:
            /* A long low wall with four merlons on top. */
            placeholder_box(&mesh, -0.48f, 0.0f, -0.12f, 0.48f, 0.32f, 0.12f);
            for (u32 merlon = 0; merlon < 4; ++merlon)
            {
                f32 x = -0.39f + 0.26f * (f32)merlon;
                placeholder_box(&mesh, x - 0.07f, 0.32f, -0.12f, x + 0.07f, 0.42f, 0.12f);
            }
            break;
        case RENDER_MESH_STONES:
            /* Six standing stones on a ring and one in the centre. */
            for (u32 stone = 0; stone < 6; ++stone)
            {
                f32 angle = 2.0f * RENDER_PI * (f32)stone / 6.0f;
                f32 x = 0.34f * cosf(angle), z = 0.34f * sinf(angle);
                placeholder_box(&mesh, x - 0.06f, 0.0f, z - 0.06f, x + 0.06f, 0.36f + 0.04f * (f32)(stone % 3), z + 0.06f);
            }
            placeholder_box(&mesh, -0.08f, 0.0f, -0.08f, 0.08f, 0.55f, 0.08f);
            break;
        case RENDER_MESH_TRAP:
            /* A low mound with a pot on it. */
            placeholder_prism(&mesh, 0.0f, 0.0f, 0.36f, 6, 0.0f, 0.06f, 0.16f);
            placeholder_prism(&mesh, 0.0f, 0.0f, 0.12f, 8, 0.1f, 0.28f, 0.34f);
            break;
        default:
            return false;
    }
    *out = mesh;
    return mesh.vertex_count > 0 && mesh.index_count > 0;
}

#endif /* PLACEHOLDER_MESHES_H */
