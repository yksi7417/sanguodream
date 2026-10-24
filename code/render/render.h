#ifndef RENDER_H
#define RENDER_H
/* The render command buffer and the camera: pure data and pure math, no
   graphics API. The game DLL fills a render_commands each frame; the
   platform layer draws it (code/d3d11_renderer.h). Tests check the math
   without a window.

   World units: one unit is one hex circumradius. The hex grid lies in the
   XZ plane with hex_to_pixel giving (x, z) for size 1 and origin (0, 0).
   Y is up. Cell heights rise along Y. */

#include <math.h>
#include "base.h"
#include "sim/hex.h"

#define RENDER_PI 3.14159265f

struct v3
{
    f32 x, y, z;
};

/* Row-major: m[row][col]. A point transforms as row vector times matrix,
   which matches the HLSL of the renderer (pack_matrix row_major, mul(p, M)). */
struct m4
{
    f32 m[4][4];
};

internal v3  v3_make(f32 x, f32 y, f32 z) { v3 r; r.x = x; r.y = y; r.z = z; return r; }
internal v3  v3_add(v3 a, v3 b)           { return v3_make(a.x + b.x, a.y + b.y, a.z + b.z); }
internal v3  v3_sub(v3 a, v3 b)           { return v3_make(a.x - b.x, a.y - b.y, a.z - b.z); }
internal v3  v3_scale(v3 a, f32 s)        { return v3_make(a.x * s, a.y * s, a.z * s); }
internal f32 v3_dot(v3 a, v3 b)           { return a.x * b.x + a.y * b.y + a.z * b.z; }
internal v3  v3_cross(v3 a, v3 b)         { return v3_make(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
internal f32 v3_length(v3 a)              { return sqrtf(v3_dot(a, a)); }
internal v3  v3_normalize(v3 a)
{
    f32 length = v3_length(a);
    return length > 0.0f ? v3_scale(a, 1.0f / length) : a;
}

internal m4
m4_identity(void)
{
    m4 r = {};
    r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.0f;
    return r;
}

internal m4
m4_mul(m4 a, m4 b)
{
    m4 r;
    for (u32 i = 0; i < 4; ++i)
    {
        for (u32 j = 0; j < 4; ++j)
        {
            r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j] + a.m[i][3] * b.m[3][j];
        }
    }
    return r;
}

/* Row vector times matrix, with the perspective divide. Returns w in w_out
   when the caller needs it (a point behind the camera has w <= 0). */
internal v3
m4_transform_point(m4 m, v3 p, f32 *w_out)
{
    f32 x = p.x * m.m[0][0] + p.y * m.m[1][0] + p.z * m.m[2][0] + m.m[3][0];
    f32 y = p.x * m.m[0][1] + p.y * m.m[1][1] + p.z * m.m[2][1] + m.m[3][1];
    f32 z = p.x * m.m[0][2] + p.y * m.m[1][2] + p.z * m.m[2][2] + m.m[3][2];
    f32 w = p.x * m.m[0][3] + p.y * m.m[1][3] + p.z * m.m[2][3] + m.m[3][3];
    if (w_out) *w_out = w;
    if (w != 0.0f && w != 1.0f) { x /= w; y /= w; z /= w; }
    return v3_make(x, y, z);
}

internal m4
m4_translation(v3 t)
{
    m4 r = m4_identity();
    r.m[3][0] = t.x; r.m[3][1] = t.y; r.m[3][2] = t.z;
    return r;
}

internal m4
m4_rotation_y(f32 radians)
{
    m4 r = m4_identity();
    f32 c = cosf(radians), s = sinf(radians);
    r.m[0][0] = c;  r.m[0][2] = -s;
    r.m[2][0] = s;  r.m[2][2] = c;
    return r;
}

internal m4
m4_scale(f32 s)
{
    m4 r = m4_identity();
    r.m[0][0] = r.m[1][1] = r.m[2][2] = s;
    return r;
}

/* Right-handed look-at for a row-vector convention. Camera space looks down -Z. */
internal m4
m4_look_at(v3 eye, v3 target, v3 up)
{
    v3 f = v3_normalize(v3_sub(target, eye));
    v3 s = v3_normalize(v3_cross(f, up));
    v3 u = v3_cross(s, f);
    m4 r = m4_identity();
    r.m[0][0] = s.x; r.m[1][0] = s.y; r.m[2][0] = s.z;
    r.m[0][1] = u.x; r.m[1][1] = u.y; r.m[2][1] = u.z;
    r.m[0][2] = -f.x; r.m[1][2] = -f.y; r.m[2][2] = -f.z;
    r.m[3][0] = -v3_dot(s, eye);
    r.m[3][1] = -v3_dot(u, eye);
    r.m[3][2] = v3_dot(f, eye);
    return r;
}

/* Perspective with depth in [0, 1] (Direct3D). */
internal m4
m4_perspective(f32 fov_y_radians, f32 aspect, f32 near_z, f32 far_z)
{
    f32 f = 1.0f / tanf(fov_y_radians * 0.5f);
    m4 r = {};
    r.m[0][0] = f / aspect;
    r.m[1][1] = f;
    r.m[2][2] = far_z / (near_z - far_z);
    r.m[2][3] = -1.0f;
    r.m[3][2] = (near_z * far_z) / (near_z - far_z);
    return r;
}

/* General 4x4 inverse (cofactors). Returns false for a singular matrix. */
internal b32
m4_inverse(m4 in, m4 *out)
{
    const f32 *m = &in.m[0][0];
    f32 inv[16];
    inv[0]  =  m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
    inv[4]  = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
    inv[8]  =  m[4]*m[9]*m[15]  - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
    inv[12] = -m[4]*m[9]*m[14]  + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
    inv[1]  = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
    inv[5]  =  m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
    inv[9]  = -m[0]*m[9]*m[15]  + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
    inv[13] =  m[0]*m[9]*m[14]  - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
    inv[2]  =  m[1]*m[6]*m[15]  - m[1]*m[7]*m[14]  - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7]  - m[13]*m[3]*m[6];
    inv[6]  = -m[0]*m[6]*m[15]  + m[0]*m[7]*m[14]  + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7]  + m[12]*m[3]*m[6];
    inv[10] =  m[0]*m[5]*m[15]  - m[0]*m[7]*m[13]  - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7]  - m[12]*m[3]*m[5];
    inv[14] = -m[0]*m[5]*m[14]  + m[0]*m[6]*m[13]  + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6]  + m[12]*m[2]*m[5];
    inv[3]  = -m[1]*m[6]*m[11]  + m[1]*m[7]*m[10]  + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7]   + m[9]*m[3]*m[6];
    inv[7]  =  m[0]*m[6]*m[11]  - m[0]*m[7]*m[10]  - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7]   - m[8]*m[3]*m[6];
    inv[11] = -m[0]*m[5]*m[11]  + m[0]*m[7]*m[9]   + m[4]*m[1]*m[11] - m[4]*m[3]*m[9]  - m[8]*m[1]*m[7]   + m[8]*m[3]*m[5];
    inv[15] =  m[0]*m[5]*m[10]  - m[0]*m[6]*m[9]   - m[4]*m[1]*m[10] + m[4]*m[2]*m[9]  + m[8]*m[1]*m[6]   - m[8]*m[2]*m[5];
    f32 det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
    if (det == 0.0f) return false;
    f32 inv_det = 1.0f / det;
    f32 *o = &out->m[0][0];
    for (u32 i = 0; i < 16; ++i) o[i] = inv[i] * inv_det;
    return true;
}

/* ---- the orbit camera --------------------------------------------------- */

#define CAMERA_PITCH_MIN     (30.0f * RENDER_PI / 180.0f)
#define CAMERA_PITCH_MAX     (75.0f * RENDER_PI / 180.0f)
#define CAMERA_DISTANCE_MIN  4.0f
#define CAMERA_DISTANCE_MAX  120.0f
#define CAMERA_FOV_Y         (45.0f * RENDER_PI / 180.0f)
#define CAMERA_NEAR          0.5f
#define CAMERA_FAR           400.0f

struct camera
{
    v3  target;      /* the point the camera orbits, on or above the terrain */
    f32 yaw;         /* radians around Y */
    f32 pitch;       /* radians above the horizon, clamped */
    f32 distance;    /* from eye to target, clamped */
};

internal f32
clamp_f32(f32 v, f32 lo, f32 hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

internal camera
camera_default(f32 map_center_x, f32 map_center_z)
{
    camera c;
    c.target   = v3_make(map_center_x, 0.0f, map_center_z);
    c.yaw      = 0.0f;
    c.pitch    = 50.0f * RENDER_PI / 180.0f;
    c.distance = 40.0f;
    return c;
}

internal void
camera_clamp(camera *c)
{
    c->pitch    = clamp_f32(c->pitch, CAMERA_PITCH_MIN, CAMERA_PITCH_MAX);
    c->distance = clamp_f32(c->distance, CAMERA_DISTANCE_MIN, CAMERA_DISTANCE_MAX);
    while (c->yaw >  RENDER_PI) c->yaw -= 2.0f * RENDER_PI;
    while (c->yaw < -RENDER_PI) c->yaw += 2.0f * RENDER_PI;
}

internal void
camera_orbit(camera *c, f32 yaw_delta, f32 pitch_delta)
{
    c->yaw   += yaw_delta;
    c->pitch += pitch_delta;
    camera_clamp(c);
}

/* factor > 1 moves the eye away, factor < 1 moves it closer. */
internal void
camera_zoom(camera *c, f32 factor)
{
    c->distance *= factor;
    camera_clamp(c);
}

/* The two directions of the ground plane as the camera sees them: forward
   goes away from the eye (up the screen), right goes to the right of the
   screen. The eye sits at target + (-sin yaw, -, +cos yaw) (camera_eye),
   so the view looks along (sin yaw, -cos yaw). */
internal void
camera_ground_axes(camera *c, v3 *forward, v3 *right)
{
    f32 s = sinf(c->yaw), k = cosf(c->yaw);
    *forward = v3_make(s, 0.0f, -k);
    *right   = v3_make(k, 0.0f, s);
}

/* Moves the target in the ground plane, in the camera's own left/right and
   forward/back directions (so a drag and the keys feel the same at every
   yaw). M11: forward moves up the screen. */
internal void
camera_pan(camera *c, f32 right, f32 forward)
{
    v3 ahead, side;
    camera_ground_axes(c, &ahead, &side);
    c->target.x += right * side.x + forward * ahead.x;
    c->target.z += right * side.z + forward * ahead.z;
}

/* Turns the view around the target. A positive angle turns the map counter-
   clockwise on the screen (render_tests.cpp checks the direction). The
   pitch does not change. */
internal void
camera_turn(camera *c, f32 counter_clockwise)
{
    camera_orbit(c, counter_clockwise, 0.0f);
}

/* Tilts the view: a positive angle goes toward the top-down view. The
   clamp of camera_clamp keeps the eye above the ground and short of top
   down; the yaw does not change. */
internal void
camera_tilt(camera *c, f32 toward_top_down)
{
    camera_orbit(c, 0.0f, toward_top_down);
}

/* ---- M12: the mini map turns with the view (ui/main-map.md, "Mini map") -- */

/* A ground offset (dx east, dz south, from the centre of the mini map) to
   an offset on the mini map (x right, y down). The forward direction of
   the camera is up, so the mini map shows the heading of the main view. */
internal void
camera_ground_to_minimap(camera *c, f32 dx, f32 dz, f32 *mx, f32 *my)
{
    v3 ahead, side;
    camera_ground_axes(c, &ahead, &side);
    *mx = dx * side.x + dz * side.z;
    *my = -(dx * ahead.x + dz * ahead.z);
}

/* The inverse of camera_ground_to_minimap: the two axes are orthonormal. */
internal void
camera_minimap_to_ground(camera *c, f32 mx, f32 my, f32 *dx, f32 *dz)
{
    v3 ahead, side;
    camera_ground_axes(c, &ahead, &side);
    *dx = mx * side.x - my * ahead.x;
    *dz = mx * side.z - my * ahead.z;
}

internal v3
camera_eye(camera *c)
{
    f32 horizontal = c->distance * cosf(c->pitch);
    f32 vertical   = c->distance * sinf(c->pitch);
    return v3_make(c->target.x - horizontal * sinf(c->yaw),
                   c->target.y + vertical,
                   c->target.z + horizontal * cosf(c->yaw));
}

internal m4
camera_view(camera *c)
{
    return m4_look_at(camera_eye(c), c->target, v3_make(0.0f, 1.0f, 0.0f));
}

internal m4
camera_projection(f32 viewport_width, f32 viewport_height)
{
    f32 aspect = viewport_height > 0.0f ? viewport_width / viewport_height : 1.0f;
    return m4_perspective(CAMERA_FOV_Y, aspect, CAMERA_NEAR, CAMERA_FAR);
}

internal m4
camera_view_projection(camera *c, f32 viewport_width, f32 viewport_height)
{
    return m4_mul(camera_view(c), camera_projection(viewport_width, viewport_height));
}

/* World point to pixel (origin top-left). Returns false for a point behind
   the camera. */
internal b32
camera_project(m4 view_projection, v3 world, f32 viewport_width, f32 viewport_height, f32 *px, f32 *py)
{
    f32 w = 1.0f;
    v3 ndc = m4_transform_point(view_projection, world, &w);
    if (w <= 0.0f) return false;
    *px = (ndc.x * 0.5f + 0.5f) * viewport_width;
    *py = (0.5f - ndc.y * 0.5f) * viewport_height;
    return true;
}

struct ray
{
    v3 origin;
    v3 direction;   /* unit length */
};

/* Pixel to a world-space ray through the near and far planes. */
internal ray
camera_ray(m4 view_projection, f32 px, f32 py, f32 viewport_width, f32 viewport_height)
{
    ray r = {};
    m4 inverse;
    if (!m4_inverse(view_projection, &inverse)) return r;
    f32 nx = (px / viewport_width) * 2.0f - 1.0f;
    f32 ny = 1.0f - (py / viewport_height) * 2.0f;
    v3 near_point = m4_transform_point(inverse, v3_make(nx, ny, 0.0f), 0);
    v3 far_point  = m4_transform_point(inverse, v3_make(nx, ny, 1.0f), 0);
    r.origin    = near_point;
    r.direction = v3_normalize(v3_sub(far_point, near_point));
    return r;
}

/* Intersects the ray with the horizontal plane y = plane_y. */
internal b32
ray_hit_plane_y(ray r, f32 plane_y, v3 *hit)
{
    if (fabsf(r.direction.y) < 1e-6f) return false;
    f32 t = (plane_y - r.origin.y) / r.direction.y;
    if (t < 0.0f) return false;
    *hit = v3_add(r.origin, v3_scale(r.direction, t));
    return true;
}

/* World (x, z) of a cell center: the hex layout with size 1 at the origin. */
internal v3
hex_world_center(hex_coord h, f32 height_y)
{
    hex_point o; o.x = 0.0f; o.y = 0.0f;
    hex_point p = hex_to_pixel(h, 1.0f, o);
    return v3_make(p.x, height_y, p.y);
}

internal hex_coord
hex_from_world(v3 world)
{
    hex_point o; o.x = 0.0f; o.y = 0.0f;
    hex_point p; p.x = world.x; p.y = world.z;
    return hex_from_pixel(p, 1.0f, o);
}

/* The pick: a pixel to the hex under it on the ground plane. Returns false
   when the ray misses the plane. The caller checks the map bounds. */
internal b32
camera_pick_hex(camera *c, f32 px, f32 py, f32 viewport_width, f32 viewport_height, hex_coord *out)
{
    m4 vp = camera_view_projection(c, viewport_width, viewport_height);
    ray r = camera_ray(vp, px, py, viewport_width, viewport_height);
    v3 hit;
    if (!ray_hit_plane_y(r, 0.0f, &hit)) return false;
    *out = hex_from_world(hit);
    return true;
}

/* The pick on the raised ground: the ray meets the top plane of each level
   from the highest down, and the first hex whose own level is that of the
   plane is the pick. levels holds the level of each cell by its offset
   (row * width + col), and level_step is the height of one level. The
   ground plane alone picked the hex under the top of a raised column, one
   or two hexes off (the player, 2026-10-24). Off the raised ground the
   pick falls back to the plane. */
internal b32
camera_pick_hex_levels(camera *c, f32 px, f32 py, f32 viewport_width, f32 viewport_height,
                       const u8 *levels, u32 width, u32 height, f32 level_step, hex_coord *out)
{
    if (!levels || !width || !height) return camera_pick_hex(c, px, py, viewport_width, viewport_height, out);
    m4 vp = camera_view_projection(c, viewport_width, viewport_height);
    ray r = camera_ray(vp, px, py, viewport_width, viewport_height);
    u8 top = 0;
    for (u32 cell = 0; cell < width * height; ++cell) if (levels[cell] > top) top = levels[cell];
    for (s32 level = (s32)top; level >= 0; --level)
    {
        v3 hit;
        if (!ray_hit_plane_y(r, (f32)level * level_step, &hit)) continue;
        hex_coord h = hex_from_world(hit);
        hex_offset o = hex_to_offset(h);
        if (o.col < 0 || o.row < 0 || (u32)o.col >= width || (u32)o.row >= height) continue;
        if (levels[(u32)o.row * width + (u32)o.col] == (u8)level) { *out = h; return true; }
    }
    return camera_pick_hex(c, px, py, viewport_width, viewport_height, out);
}

/* ---- the command buffer ------------------------------------------------- */

/* One set of terrain cells for the mesh builder and the overlay texture.
   The DLL points these at its own arrays; the platform rebuilds the mesh
   when terrain_version changes. */
struct render_terrain
{
    u32  width;
    u32  height;
    const u8 *kind;        /* per cell, row-major by offset: atlas tile index */
    const u8 *level;       /* per cell: height in steps of TERRAIN_STEP */
    u32  version;          /* bumped by the DLL when kind or level change */
    const u8 *grid;        /* M11: per cell, 1 = draw the grid outline; 0 (no pointer) = every cell */
};

/* The mesh table of the platform. Mesh 0 is the settlement. The other ids
   are the placeholder models of the facility slots (placeholder_meshes.h).
   The scene picks a shape for each facility kind and tints it. */
enum render_mesh_id
{
    RENDER_MESH_SETTLEMENT,   /* settlement.hxm, or the placeholder house */
    RENDER_MESH_SCAFFOLD,     /* four posts with rails: a build in progress */
    RENDER_MESH_STALL,        /* a tent: the markets */
    RENDER_MESH_FIELD,        /* a plate with ridges: the farms */
    RENDER_MESH_HALL,         /* a long house with a gable roof: the barracks, the drill ground */
    RENDER_MESH_FORGE,        /* a house with a chimney: the smithy, the workshop */
    RENDER_MESH_DOCK,         /* a plank with a hull and a mast: the shipyard */
    RENDER_MESH_SILO,         /* a round tower with a cone: the mint, the granary */
    RENDER_MESH_TOWER,        /* a tall tower on a base: the bronze sparrow tower */
    RENDER_MESH_OFFICE,       /* a two-tier house: the offices */
    RENDER_MESH_SHED,         /* a low open house: the stable */
    RENDER_MESH_GATE,         /* two pillars, a lintel and a roof over a passage: a pass (關) */
    RENDER_MESH_PORT,         /* a quay with a pier, two mooring posts and a hull: a port (港) */
    RENDER_MESH_COUNT
};

struct render_mesh_instance
{
    u32 mesh_id;           /* render_mesh_id: index into the platform's mesh table */
    v3  position;
    f32 yaw;
    f32 scale;
    u32 tint;              /* RGBA8, 0xFFFFFFFF for none */
};

/* The sprites of the platform's sprite table. Sprite 0 is the figure of a
   unit without a class. The others are one picture for each class of unit
   (docs/ASSETS.md, "Unit sprites"). The platform loads the art of
   render_sprite_art_files[id] first, then the placeholder picture of
   render_sprite_files[id], from the content pack. If both are absent, the
   platform uses the figure of sprite 0. A pixel with the alpha 160 takes
   the colour of the faction (the red mask of docs/GRAPHICS.md). */
enum render_sprite
{
    RENDER_SPRITE_UNIT,
    RENDER_SPRITE_SWORD,
    RENDER_SPRITE_PIKE,
    RENDER_SPRITE_HALBERD,
    RENDER_SPRITE_CROSSBOW,
    RENDER_SPRITE_CAVALRY,
    RENDER_SPRITE_RAM,
    RENDER_SPRITE_WOOD_BEAST,
    RENDER_SPRITE_TOWER,
    RENDER_SPRITE_CATAPULT,
    RENDER_SPRITE_BOAT,
    RENDER_SPRITE_TOWER_SHIP,
    RENDER_SPRITE_BATTLE_SHIP,
    RENDER_SPRITE_TRANSPORT,
    RENDER_SPRITE_COUNT
};

/* The placeholder pictures (misc/make_unit_sprites.py). The four siege
   weapons share one, and the two war ships share one. */
global_variable const char *render_sprite_files[RENDER_SPRITE_COUNT] = {
    "sprites\\unit.png", "sprites\\sword.png", "sprites\\pike.png", "sprites\\halberd.png", "sprites\\crossbow.png",
    "sprites\\cavalry.png", "sprites\\siege.png", "sprites\\siege.png", "sprites\\siege.png", "sprites\\siege.png",
    "sprites\\boat.png", "sprites\\ship.png", "sprites\\ship.png", "sprites\\transport.png",
};

/* The art of each class (misc/art_import.py, from the model sheets). */
global_variable const char *render_sprite_art_files[RENDER_SPRITE_COUNT] = {
    0, "sprites\\art\\sword.png", "sprites\\art\\pike.png", "sprites\\art\\halberd.png", "sprites\\art\\crossbow.png",
    "sprites\\art\\cavalry.png", "sprites\\art\\ram.png", "sprites\\art\\wood_beast.png", "sprites\\art\\tower.png", "sprites\\art\\catapult.png",
    "sprites\\art\\boat.png", "sprites\\art\\tower_ship.png", "sprites\\art\\battle_ship.png", "sprites\\art\\transport.png",
};

struct render_billboard
{
    u32 sprite_id;         /* render_sprite: index into the platform's sprite table */
    v3  position;          /* bottom center */
    f32 width;
    f32 height;
    u32 tint;
};

#define RENDER_MAX_MESH_INSTANCES 4096
#define RENDER_MAX_BILLBOARDS     4096

struct render_commands
{
    b32    scene_enabled;          /* false: the platform draws only ImGui */
    camera cam;
    f32    viewport_width;
    f32    viewport_height;
    render_terrain terrain;
    const u32 *overlay;            /* per cell RGBA8, alpha blends over the material; 0 = none */
    b32    draw_grid;              /* hex outlines on the terrain */
    render_mesh_instance *mesh_instances;
    u32    mesh_instance_count;
    render_billboard     *billboards;
    u32    billboard_count;
};

/* The platform makes one of these in the frame arena every frame. */
internal render_commands *
render_commands_begin(memory_arena *frame, f32 viewport_width, f32 viewport_height)
{
    render_commands *rc = PushStruct(frame, render_commands);
    rc->viewport_width  = viewport_width;
    rc->viewport_height = viewport_height;
    rc->mesh_instances  = PushArray(frame, RENDER_MAX_MESH_INSTANCES, render_mesh_instance);
    rc->billboards      = PushArray(frame, RENDER_MAX_BILLBOARDS, render_billboard);
    return rc;
}

internal void
render_push_mesh(render_commands *rc, u32 mesh_id, v3 position, f32 yaw, f32 scale, u32 tint)
{
    if (rc->mesh_instance_count >= RENDER_MAX_MESH_INSTANCES) return;
    render_mesh_instance *m = rc->mesh_instances + rc->mesh_instance_count++;
    m->mesh_id = mesh_id; m->position = position; m->yaw = yaw; m->scale = scale; m->tint = tint;
}

internal void
render_push_billboard(render_commands *rc, u32 sprite_id, v3 position, f32 width, f32 height, u32 tint)
{
    if (rc->billboard_count >= RENDER_MAX_BILLBOARDS) return;
    render_billboard *b = rc->billboards + rc->billboard_count++;
    b->sprite_id = sprite_id; b->position = position; b->width = width; b->height = height; b->tint = tint;
}

#endif /* RENDER_H */
