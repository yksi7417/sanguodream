#ifndef WEBGL2_RENDERER_H
#define WEBGL2_RENDERER_H
/* The scene renderer of the web platform layer: draws a render_commands
   with WebGL2 (OpenGL ES 3.0). It is a port of d3d11_renderer.h and draws
   the same scene: the terrain, the hex grid, the instanced meshes, the 3D
   models of the bases and the billboards. A change to one renderer goes
   into the other renderer in the same commit (docs/WEB.md).

   The differences from D3D11:
   - The uniform block Frame is row_major, and the shaders multiply a row
     vector by the matrix, as mul(p, M) in the HLSL. Thus the CPU sends
     the matrix of camera_view_projection without a change.
   - camera_view_projection gives the depth of D3D, 0 to 1. WebGL2 clips
     the depth at -1 to 1, so each vertex shader moves z to that range.
   - The front faces turn clockwise on the screen, as in D3D11. */

#include <GLES3/gl3.h>
#include "base.h"
#include "hexdom.h"             /* the file reader of the platform */
#include "render/render.h"
#include "render/terrain_mesh.h"
#include "render/placeholder_meshes.h"
#include "render/png.h"

#define GL_MAX_MESHES        32     /* at least RENDER_MESH_COUNT */
#define GL_MAX_SPRITES       32     /* at least RENDER_SPRITE_COUNT */
#define GL_ATLAS_TILES       8      /* material tiles in one row of the atlas */
#define GL_ATLAS_TILE_SIZE   64     /* pixels, for the generated placeholder atlas */
#define GL_SPRITE_SIZE       64
#define GL_BASE_KINDS        (RENDER_SPRITE_COUNT - RENDER_SPRITE_FIRST_BASE)

/* The attribute locations of the shaders. */
enum
{
    GL_ATTRIBUTE_POSITION = 0,
    GL_ATTRIBUTE_NORMAL   = 1,
    GL_ATTRIBUTE_UV       = 2,
    GL_ATTRIBUTE_CELL     = 3,     /* the terrain */
    GL_ATTRIBUTE_TILE     = 4,     /* the terrain */
    GL_ATTRIBUTE_PLACE    = 3,     /* an instance: the position */
    GL_ATTRIBUTE_YAW      = 4,     /* an instance: the yaw, or the width of a billboard */
    GL_ATTRIBUTE_SCALE    = 5,     /* an instance: the scale, or the height of a billboard */
    GL_ATTRIBUTE_TINT     = 6,     /* an instance: RGBA8 */
    GL_ATTRIBUTE_COUNT    = 7,
};

struct gl_mesh
{
    GLuint vertices;
    GLuint indices;
    u32    index_count;
};

struct gl_sprite
{
    GLuint texture;     /* 0 for a base without art: the mesh stays */
    f32    aspect;      /* the height of the picture over its width */
};

/* The 3D model of a base kind or of a placeholder shape: a mesh file and
   its colour texture. Both are 0 when the pack has no model of the kind. */
struct gl_base_model
{
    gl_mesh mesh;
    GLuint  texture;
};

/* The uniform block Frame. Its std140 layout is the layout of
   d3d11_frame_constants. */
struct gl_frame_constants
{
    m4  view_projection;
    f32 light_direction[4];   /* xyz, w unused */
    f32 camera_right[4];
    f32 camera_up[4];
    f32 overlay_size[2];      /* cells wide, cells high */
    f32 atlas_tiles;
    f32 mode;                 /* 0 terrain, 1 grid lines, 2 the card of a base faces the camera */
};

struct gl_mesh_instance
{
    f32 position[3];
    f32 yaw;
    f32 scale;
    u32 tint;
};

struct gl_billboard_instance
{
    f32 position[3];
    f32 width;
    f32 height;
    u32 tint;
};

struct webgl2_renderer
{
    char                       data_dir[260];
    platform_read_entire_file *read_file;   /* the file reader of the platform */

    GLuint  terrain_program;
    GLuint  mesh_program;
    GLuint  model_program;          /* the textured model of a base */
    GLuint  billboard_program;
    GLuint  vertex_array;           /* one VAO: each draw sets its attributes */
    GLuint  constants;              /* the uniform buffer of Frame */

    GLuint  terrain_vertices;
    GLuint  terrain_indices;
    GLuint  grid_indices;
    u32     terrain_index_count;
    u32     grid_index_count;
    u32     terrain_version;
    u32     terrain_cells;

    GLuint  overlay_texture;
    u32     overlay_width, overlay_height;

    GLuint        atlas_texture;
    gl_mesh       meshes[GL_MAX_MESHES];
    u32           mesh_count;
    gl_sprite     sprites[GL_MAX_SPRITES];
    u32           sprite_count;
    gl_base_model base_models[GL_BASE_KINDS];
    gl_base_model mesh_models[RENDER_MESH_COUNT][RENDER_MODEL_LEVELS];   /* render_mesh_model_files */
    GLuint        instances;       /* shared by the meshes and the billboards */
    u32           instance_capacity;
    GLuint        billboard_quad;
    b32           ready;
    char          last_error[256];
};

/* ---- shaders ------------------------------------------------------------ */

#define GL_SHADER_HEADER \
"#version 300 es\n" \
"precision highp float;\n" \
"precision highp int;\n" \
"layout(std140, row_major) uniform Frame {\n" \
"  mat4 view_projection; vec4 light_direction; vec4 camera_right; vec4 camera_up;\n" \
"  vec2 overlay_size; float atlas_tiles; float mode; };\n" \
"vec4 to_clip(vec3 world) {\n"     /* mul(p, M) of the HLSL, then the depth from 0..1 to -1..1 */ \
"  vec4 clip = vec4(world, 1.0) * view_projection;\n" \
"  clip.z = clip.z * 2.0 - clip.w; return clip; }\n"

static const char gl_terrain_vs[] = GL_SHADER_HEADER
"layout(location = 0) in vec3 a_position; layout(location = 1) in vec3 a_normal; layout(location = 2) in vec2 a_uv;\n"
"layout(location = 3) in float a_cell; layout(location = 4) in float a_tile;\n"
"out vec3 v_normal; out vec2 v_uv; out float v_cell; out float v_tile;\n"
"void main() { gl_Position = to_clip(a_position); v_normal = a_normal; v_uv = a_uv; v_cell = a_cell; v_tile = a_tile; }\n";

static const char gl_terrain_fs[] = GL_SHADER_HEADER
"uniform sampler2D u_atlas; uniform highp sampler2D u_overlay;\n"
"in vec3 v_normal; in vec2 v_uv; in float v_cell; in float v_tile; out vec4 o_color;\n"
"void main() {\n"
"  if (mode > 0.5) { o_color = vec4(0.05, 0.05, 0.05, 1.0); return; }\n"
"  vec2 uv = vec2((v_tile + fract(v_uv.x)) / atlas_tiles, fract(v_uv.y));\n"
"  vec3 base = texture(u_atlas, uv).rgb;\n"
"  int w = int(overlay_size.x); int c = int(v_cell + 0.5);\n"
"  vec4 tint = texelFetch(u_overlay, ivec2(c % w, c / w), 0);\n"
"  float light = 0.35 + 0.65 * clamp(dot(normalize(v_normal), -light_direction.xyz), 0.0, 1.0);\n"
"  o_color = vec4(mix(base, tint.rgb, tint.a) * light, 1.0); }\n";

static const char gl_mesh_vs[] = GL_SHADER_HEADER
"layout(location = 0) in vec3 a_position; layout(location = 1) in vec3 a_normal; layout(location = 2) in vec2 a_uv;\n"
"layout(location = 3) in vec3 i_position; layout(location = 4) in float i_yaw; layout(location = 5) in float i_scale;\n"
"layout(location = 6) in vec4 i_tint;\n"
"out vec3 v_normal; out vec2 v_uv; out vec4 v_tint;\n"
"void main() {\n"
"  float c = cos(i_yaw), s = sin(i_yaw);\n"
"  vec3 p = a_position * i_scale; vec3 r = vec3(p.x * c + p.z * s, p.y, -p.x * s + p.z * c);\n"
"  v_normal = vec3(a_normal.x * c + a_normal.z * s, a_normal.y, -a_normal.x * s + a_normal.z * c);\n"
"  gl_Position = to_clip(r + i_position); v_uv = a_uv; v_tint = i_tint; }\n";

static const char gl_mesh_fs[] = GL_SHADER_HEADER
"in vec3 v_normal; in vec2 v_uv; in vec4 v_tint; out vec4 o_color;\n"
"void main() {\n"
"  float light = 0.35 + 0.65 * clamp(dot(normalize(v_normal), -light_direction.xyz), 0.0, 1.0);\n"
"  o_color = vec4(v_tint.rgb * light, 1.0); }\n";

/* the model of a base: its texture, and the flat red of the banners in the faction colour */
static const char gl_model_fs[] = GL_SHADER_HEADER
"uniform sampler2D u_sprite;\n"
"in vec3 v_normal; in vec2 v_uv; in vec4 v_tint; out vec4 o_color;\n"
"void main() {\n"
"  vec3 c = texture(u_sprite, v_uv).rgb;\n"
"  float mask = clamp((c.r - max(c.g, c.b) - 0.45) / 0.15, 0.0, 1.0);\n"
"  vec3 base = mix(c, v_tint.rgb * clamp(c.r / 0.82, 0.0, 1.0), mask);\n"
"  float light = 0.55 + 0.45 * clamp(dot(normalize(v_normal), -light_direction.xyz), 0.0, 1.0);\n"
"  o_color = vec4(base * light, 1.0); }\n";

static const char gl_billboard_vs[] = GL_SHADER_HEADER
"layout(location = 0) in vec2 a_corner; layout(location = 2) in vec2 a_uv;\n"
"layout(location = 3) in vec3 i_position; layout(location = 4) in float i_width; layout(location = 5) in float i_height;\n"
"layout(location = 6) in vec4 i_tint;\n"
"out vec2 v_uv; out vec4 v_tint;\n"
"void main() {\n"
"  vec3 up = mode > 1.5 ? camera_up.xyz : vec3(0.0, 1.0, 0.0);\n"     /* mode 2: the card of a base faces the camera */
"  vec3 world = i_position + camera_right.xyz * (a_corner.x * i_width) + up * (a_corner.y * i_height);\n"
"  gl_Position = to_clip(world); v_uv = a_uv; v_tint = i_tint; }\n";

static const char gl_billboard_fs[] = GL_SHADER_HEADER
"uniform sampler2D u_sprite;\n"
"in vec2 v_uv; in vec4 v_tint; out vec4 o_color;\n"
"void main() {\n"
"  vec4 c = texture(u_sprite, v_uv); if (c.a < 0.5) discard;\n"
"  float mask = clamp((1.0 - c.a) / 0.37, 0.0, 1.0);\n"                    /* alpha 160 of 255: the pixel takes the faction colour */
"  float shade = clamp(dot(c.rgb, vec3(0.3, 0.59, 0.11)) / 0.31, 0.0, 1.0);\n"   /* the flat red of the mask shades to 1 */
"  o_color = vec4(mix(c.rgb, v_tint.rgb * shade, mask), 1.0); }\n";

internal GLuint
gl_compile_shader(webgl2_renderer *r, GLenum kind, const char *source, const char *name)
{
    GLuint shader = glCreateShader(kind);
    glShaderSource(shader, 1, &source, 0);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok) return shader;
    char log[200] = {};
    glGetShaderInfoLog(shader, sizeof(log), 0, log);
    snprintf(r->last_error, sizeof(r->last_error), "shader %s: %s", name, log);
    glDeleteShader(shader);
    return 0;
}

/* A program from two shaders. It binds the block Frame to binding 0 and
   the samplers to their units: atlas 0, overlay 1, sprite 2. */
internal GLuint
gl_make_program(webgl2_renderer *r, const char *vertex_source, const char *fragment_source, const char *name)
{
    GLuint vertex = gl_compile_shader(r, GL_VERTEX_SHADER, vertex_source, name);
    GLuint fragment = vertex ? gl_compile_shader(r, GL_FRAGMENT_SHADER, fragment_source, name) : 0;
    if (!fragment) { if (vertex) glDeleteShader(vertex); return 0; }
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[200] = {};
        glGetProgramInfoLog(program, sizeof(log), 0, log);
        snprintf(r->last_error, sizeof(r->last_error), "program %s: %s", name, log);
        glDeleteProgram(program);
        return 0;
    }
    GLuint block = glGetUniformBlockIndex(program, "Frame");
    if (block != GL_INVALID_INDEX) glUniformBlockBinding(program, block, 0);
    glUseProgram(program);
    GLint atlas = glGetUniformLocation(program, "u_atlas");
    GLint overlay = glGetUniformLocation(program, "u_overlay");
    GLint sprite = glGetUniformLocation(program, "u_sprite");
    if (atlas >= 0) glUniform1i(atlas, 0);
    if (overlay >= 0) glUniform1i(overlay, 1);
    if (sprite >= 0) glUniform1i(sprite, 2);
    glUseProgram(0);
    return program;
}

/* ---- resources ---------------------------------------------------------- */

internal GLuint
gl_make_buffer(GLenum target, const void *data, u64 size)
{
    GLuint buffer = 0;
    glGenBuffers(1, &buffer);
    glBindBuffer(target, buffer);
    glBufferData(target, (GLsizeiptr)size, data, GL_STATIC_DRAW);
    glBindBuffer(target, 0);
    return buffer;
}

/* An RGBA8 texture. With mipmaps, the GPU makes the smaller levels: a
   model seen from far away needs them, or its texture shimmers when the
   view moves. */
internal GLuint
gl_make_texture(const u32 *rgba, u32 width, u32 height, b32 mipmaps)
{
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)width, (GLsizei)height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    if (mipmaps) glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

/* Reads a whole file of data_dir into the arena through the file reader
   of the platform; zero size when absent. */
internal u8 *
gl_read_data_file(webgl2_renderer *r, const char *relative, memory_arena *arena, u64 *size_out)
{
    char path[520];
    snprintf(path, sizeof(path), "%s/%s", r->data_dir, relative);
    read_file_result file = r->read_file(path, arena);
    *size_out = file.contents ? file.size : 0;
    return *size_out ? (u8 *)file.contents : 0;
}

/* The placeholder material atlas: one flat colour tile per terrain kind,
   with a faint checker so zoom is visible. Used when materials.png is absent.
   Colours are ABGR in memory (RGBA8, little endian). */
internal u32 *
gl_placeholder_atlas(memory_arena *arena)
{
    static const u32 colors[GL_ATLAS_TILES] = {
        0xFF9E5A2Eu, /* 0 water: blue  */ 0xFF4E8C5Au, /* 1 plain: green */ 0xFF2E6E3Au, /* 2 forest */
        0xFF5A7D96u, /* 3 hill: tan */    0xFF8A8A8Au, /* 4 mountain: grey */ 0xFF3A6E9Eu, /* 5 road: brown */
        0xFFC8C8C8u, /* 6 snow */          0xFF7A3A9Au  /* 7 unused */ };
    u32 w = GL_ATLAS_TILES * GL_ATLAS_TILE_SIZE, h = GL_ATLAS_TILE_SIZE;
    u32 *rgba = PushArray(arena, (u64)w * h, u32);
    for (u32 y = 0; y < h; ++y)
    {
        for (u32 x = 0; x < w; ++x)
        {
            u32 tile = x / GL_ATLAS_TILE_SIZE;
            u32 c = colors[tile];
            b32 dark = ((x / 8) + (y / 8)) & 1;
            if (dark)
            {
                u32 rr = (c & 0xFF) * 9 / 10, gg = ((c >> 8) & 0xFF) * 9 / 10, bb = ((c >> 16) & 0xFF) * 9 / 10;
                c = 0xFF000000u | (bb << 16) | (gg << 8) | rr;
            }
            rgba[y * w + x] = c;
        }
    }
    return rgba;
}

/* A placeholder sprite: a figure on a transparent background. The alpha
   160 makes the figure take the colour of the faction (gl_billboard_fs). */
internal u32 *
gl_placeholder_sprite(memory_arena *arena)
{
    u32 s = GL_SPRITE_SIZE;
    u32 *rgba = PushArray(arena, (u64)s * s, u32);
    for (u32 y = 0; y < s; ++y)
    {
        for (u32 x = 0; x < s; ++x)
        {
            f32 fx = ((f32)x - 32.0f) / 14.0f, fy_head = ((f32)y - 16.0f) / 10.0f, fy_body = ((f32)y - 44.0f) / 18.0f;
            b32 head = fx * fx + fy_head * fy_head < 1.0f;
            b32 body = fx * fx + fy_body * fy_body < 1.0f && y > 20;
            rgba[y * s + x] = head ? 0xA050C8F0u : (body ? 0xA02050C0u : 0x00000000u);
        }
    }
    return rgba;
}

internal b32
gl_make_mesh(const void *vertices, u32 vertex_count, const u32 *indices, u32 index_count, gl_mesh *mesh)
{
    mesh->vertices    = gl_make_buffer(GL_ARRAY_BUFFER, vertices, (u64)vertex_count * sizeof(render_mesh_vertex));
    mesh->indices     = gl_make_buffer(GL_ELEMENT_ARRAY_BUFFER, indices, (u64)index_count * 4);
    mesh->index_count = index_count;
    return mesh->vertices && mesh->indices;
}

/* A placeholder shape of placeholder_meshes.h as a GPU mesh. */
internal b32
gl_placeholder_mesh(u32 mesh_id, memory_arena *arena, gl_mesh *mesh)
{
    placeholder_mesh shape = {};
    if (!placeholder_mesh_build(mesh_id, arena, &shape)) return false;
    return gl_make_mesh(shape.vertices, shape.vertex_count, shape.indices, shape.index_count, mesh);
}

/* A mesh file of the pack (render_mesh_file_parse) as a GPU mesh. */
internal b32
gl_load_mesh_file(webgl2_renderer *r, const char *relative, memory_arena *arena, gl_mesh *mesh)
{
    u64 size = 0;
    u8 *data = gl_read_data_file(r, relative, arena, &size);
    render_mesh_file file = {};
    if (!render_mesh_file_parse(data, size, &file)) return false;
    return gl_make_mesh(file.vertices, file.vertex_count, file.indices, file.index_count, mesh);
}

internal GLuint
gl_load_png_texture(webgl2_renderer *r, const char *relative, memory_arena *arena, f32 *aspect_out = 0, b32 mipmaps = false)
{
    u64 size = 0;
    u8 *data = gl_read_data_file(r, relative, arena, &size);
    if (!data) return 0;
    png_image image = png_decode(data, size, arena);
    if (image.error || !image.width) return 0;
    if (aspect_out) *aspect_out = (f32)image.height / (f32)image.width;
    return gl_make_texture((const u32 *)image.rgba, image.width, image.height, mipmaps);
}

internal void
gl_delete_mesh(gl_mesh *mesh)
{
    if (mesh->vertices) glDeleteBuffers(1, &mesh->vertices);
    if (mesh->indices) glDeleteBuffers(1, &mesh->indices);
    *mesh = {};
}

/* The model of one base kind or shape: <name>.hxm and <name>.png of the
   pack. If one of the two files is absent, the kind has no model. The GPU
   keeps the mesh and the texture, so the scratch arena gets its bytes back. */
internal void
gl_load_base_model(webgl2_renderer *r, const char *name, memory_arena *arena, gl_base_model *model)
{
    *model = {};
    if (!name) return;
    char mesh_file[260], texture_file[260];
    snprintf(mesh_file, sizeof(mesh_file), "%s.hxm", name);
    snprintf(texture_file, sizeof(texture_file), "%s.png", name);
    u64 arena_used = arena->used;
    if (gl_load_mesh_file(r, mesh_file, arena, &model->mesh)) model->texture = gl_load_png_texture(r, texture_file, arena, 0, true);
    arena->used = arena_used;
    if (!model->texture) gl_delete_mesh(&model->mesh);
}

/* ---- init --------------------------------------------------------------- */

internal b32
webgl2_renderer_init(webgl2_renderer *r, platform_read_entire_file *read_file, const char *data_dir, memory_arena *scratch)
{
    *r = {};
    r->read_file = read_file;
    snprintf(r->data_dir, sizeof(r->data_dir), "%s", data_dir);

    r->terrain_program   = gl_make_program(r, gl_terrain_vs, gl_terrain_fs, "terrain");
    r->mesh_program      = r->terrain_program ? gl_make_program(r, gl_mesh_vs, gl_mesh_fs, "mesh") : 0;
    r->model_program     = r->mesh_program ? gl_make_program(r, gl_mesh_vs, gl_model_fs, "model") : 0;
    r->billboard_program = r->model_program ? gl_make_program(r, gl_billboard_vs, gl_billboard_fs, "billboard") : 0;
    if (!r->billboard_program) return false;

    glGenVertexArrays(1, &r->vertex_array);
    r->constants = gl_make_buffer(GL_UNIFORM_BUFFER, 0, sizeof(gl_frame_constants));

    /* the material atlas: the pack's file, or the placeholder */
    r->atlas_texture = gl_load_png_texture(r, "materials.png", scratch);
    if (!r->atlas_texture)
    {
        u32 *atlas = gl_placeholder_atlas(scratch);
        r->atlas_texture = gl_make_texture(atlas, GL_ATLAS_TILES * GL_ATLAS_TILE_SIZE, GL_ATLAS_TILE_SIZE, false);
    }

    /* mesh 0: the settlement, from settlement.hxm or the placeholder house.
       The other meshes are the placeholder shapes of the facility slots. */
    if (!gl_load_mesh_file(r, "settlement.hxm", scratch, r->meshes + 0))
    {
        gl_placeholder_mesh(RENDER_MESH_SETTLEMENT, scratch, r->meshes + 0);
    }
    r->mesh_count = 1;
    for (u32 mesh_id = 1; mesh_id < RENDER_MESH_COUNT && mesh_id < GL_MAX_MESHES; ++mesh_id)
    {
        if (!gl_placeholder_mesh(mesh_id, scratch, r->meshes + mesh_id)) break;
        r->mesh_count = mesh_id + 1;
    }

    /* The sprites, as in d3d11_renderer_init: the art first, then the
       placeholder picture, then the placeholder figure. A base without
       art keeps texture 0, and the renderer draws the mesh of the base. */
    for (u32 sprite_id = 0; sprite_id < RENDER_SPRITE_COUNT && sprite_id < GL_MAX_SPRITES; ++sprite_id)
    {
        gl_sprite *sprite_slot = r->sprites + sprite_id;
        sprite_slot->texture = 0;
        sprite_slot->aspect  = 1.0f;
        r->sprite_count = sprite_id + 1;
        if (render_sprite_art_files[sprite_id]) sprite_slot->texture = gl_load_png_texture(r, render_sprite_art_files[sprite_id], scratch, &sprite_slot->aspect);
        if (sprite_id >= RENDER_SPRITE_FIRST_BASE) continue;
        if (!sprite_slot->texture) sprite_slot->texture = gl_load_png_texture(r, render_sprite_files[sprite_id], scratch, &sprite_slot->aspect);
        if (!sprite_slot->texture)
        {
            u32 *sprite = gl_placeholder_sprite(scratch);
            sprite_slot->texture = gl_make_texture(sprite, GL_SPRITE_SIZE, GL_SPRITE_SIZE, false);
        }
    }

    /* The 3D models of the base kinds (docs/ASSETS.md, "Base models"). */
    for (u32 kind = 0; kind < GL_BASE_KINDS; ++kind)
    {
        gl_load_base_model(r, render_base_model_files[kind], scratch, r->base_models + kind);
    }
    /* The models of the placeholder shapes for each level, for example the market. */
    for (u32 mesh_id = 0; mesh_id < RENDER_MESH_COUNT; ++mesh_id)
    {
        for (u32 level = 0; level < RENDER_MODEL_LEVELS; ++level)
        {
            gl_load_base_model(r, render_mesh_model_files[mesh_id][level], scratch, r->mesh_models[mesh_id] + level);
        }
    }

    r->instance_capacity = RENDER_MAX_MESH_INSTANCES > RENDER_MAX_BILLBOARDS ? RENDER_MAX_MESH_INSTANCES : RENDER_MAX_BILLBOARDS;
    glGenBuffers(1, &r->instances);

    f32 quad[] = { -0.5f, 0.0f, 0.0f, 1.0f,   0.5f, 0.0f, 1.0f, 1.0f,   -0.5f, 1.0f, 0.0f, 0.0f,
                    0.5f, 0.0f, 1.0f, 1.0f,   0.5f, 1.0f, 1.0f, 0.0f,   -0.5f, 1.0f, 0.0f, 0.0f };
    r->billboard_quad = gl_make_buffer(GL_ARRAY_BUFFER, quad, sizeof(quad));

    r->ready = r->constants && r->atlas_texture && r->meshes[0].vertices && r->sprites[0].texture && r->instances && r->billboard_quad;
    if (!r->ready && !r->last_error[0]) snprintf(r->last_error, sizeof(r->last_error), "a WebGL2 resource could not be created");
    return r->ready;
}

/* ---- per frame ---------------------------------------------------------- */

/* Rebuilds the terrain buffers from the command's terrain when its version changed. */
internal void
gl_ensure_terrain(webgl2_renderer *r, render_terrain *terrain, memory_arena *scratch)
{
    u32 cells = terrain->width * terrain->height;
    if (r->terrain_vertices && r->terrain_version == terrain->version && r->terrain_cells == cells) return;
    if (r->terrain_vertices) glDeleteBuffers(1, &r->terrain_vertices);
    if (r->terrain_indices) glDeleteBuffers(1, &r->terrain_indices);
    if (r->grid_indices) glDeleteBuffers(1, &r->grid_indices);
    r->terrain_vertices = r->terrain_indices = r->grid_indices = 0;
    r->terrain_index_count = r->grid_index_count = 0;
    r->terrain_version = terrain->version;
    r->terrain_cells = cells;
    if (!cells) return;

    terrain_mesh mesh = terrain_mesh_build(terrain, scratch);
    r->terrain_vertices = gl_make_buffer(GL_ARRAY_BUFFER, mesh.vertices, (u64)mesh.vertex_count * sizeof(terrain_vertex));
    r->terrain_indices  = gl_make_buffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indices, (u64)mesh.index_count * 4);
    r->terrain_index_count = mesh.index_count;

    /* grid lines: the six top edges of each cell */
    u32 *lines = PushArray(scratch, cells * 12, u32);
    u32 n = 0;
    for (u32 c = 0; c < cells; ++c)
    {
        if (terrain->grid && !terrain->grid[c]) continue;   /* M11: no grid on cliffs (ui/main-map.md) */
        u32 base = c * TERRAIN_VERTS_PER_CELL;
        for (u32 k = 0; k < 6; ++k) { lines[n++] = base + 1 + k; lines[n++] = base + 1 + (k + 1) % 6; }
    }
    r->grid_indices = gl_make_buffer(GL_ELEMENT_ARRAY_BUFFER, lines, (u64)n * 4);
    r->grid_index_count = n;

    /* The overlay: one RGBA8 texel for each cell. The shader reads it with
       texelFetch, so it has no filter and no mipmaps. */
    if (r->overlay_texture) glDeleteTextures(1, &r->overlay_texture);
    glGenTextures(1, &r->overlay_texture);
    glBindTexture(GL_TEXTURE_2D, r->overlay_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)terrain->width, (GLsizei)terrain->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    r->overlay_width = terrain->width; r->overlay_height = terrain->height;
}

/* Sets the attributes of one vertex buffer: a float attribute of count
   components at offset, or an RGBA8 colour. divisor 1 is one value for
   each instance. */
internal void
gl_attribute(u32 location, s32 count, u32 stride, u32 offset, u32 divisor)
{
    glEnableVertexAttribArray(location);
    glVertexAttribPointer(location, count, GL_FLOAT, GL_FALSE, (GLsizei)stride, (const void *)(uintptr_t)offset);
    glVertexAttribDivisor(location, divisor);
}

internal void
gl_attribute_colour(u32 location, u32 stride, u32 offset)
{
    glEnableVertexAttribArray(location);
    glVertexAttribPointer(location, 4, GL_UNSIGNED_BYTE, GL_TRUE, (GLsizei)stride, (const void *)(uintptr_t)offset);
    glVertexAttribDivisor(location, 1);
}

internal void
gl_disable_attributes(void)
{
    for (u32 location = 0; location < GL_ATTRIBUTE_COUNT; ++location)
    {
        glDisableVertexAttribArray(location);
        glVertexAttribDivisor(location, 0);
    }
}

/* The mesh attributes, then the instance attributes of gl_mesh_instance. */
internal void
gl_bind_mesh(webgl2_renderer *r, gl_mesh *mesh, const gl_mesh_instance *batch, u32 count)
{
    gl_disable_attributes();
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vertices);
    gl_attribute(GL_ATTRIBUTE_POSITION, 3, sizeof(render_mesh_vertex), 0, 0);
    gl_attribute(GL_ATTRIBUTE_NORMAL,   3, sizeof(render_mesh_vertex), 12, 0);
    gl_attribute(GL_ATTRIBUTE_UV,       2, sizeof(render_mesh_vertex), 24, 0);
    glBindBuffer(GL_ARRAY_BUFFER, r->instances);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(count * sizeof(gl_mesh_instance)), batch, GL_STREAM_DRAW);
    gl_attribute(GL_ATTRIBUTE_PLACE, 3, sizeof(gl_mesh_instance), 0, 1);
    gl_attribute(GL_ATTRIBUTE_YAW,   1, sizeof(gl_mesh_instance), 12, 1);
    gl_attribute(GL_ATTRIBUTE_SCALE, 1, sizeof(gl_mesh_instance), 16, 1);
    gl_attribute_colour(GL_ATTRIBUTE_TINT, sizeof(gl_mesh_instance), 20);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->indices);
}

/* True when the platform draws the art of a base in place of the mesh:
   the instance names a base sprite, and the pack has its picture. */
internal b32
gl_has_art(webgl2_renderer *r, render_mesh_instance *m)
{
    return m->art_sprite >= RENDER_SPRITE_FIRST_BASE && m->art_sprite < r->sprite_count && r->sprites[m->art_sprite].texture != 0;
}

/* The 3D model that the platform draws for an instance, or 0. A model goes
   before the art card and before the placeholder mesh. A base uses only
   the model of its kind. Another instance uses the model of its shape and
   its level, or of the next lower level with a model, if the camera is
   near (render_shape_models_on). */
internal gl_base_model *
gl_model_of(webgl2_renderer *r, render_mesh_instance *m, const camera *cam)
{
    if (m->art_sprite >= RENDER_SPRITE_FIRST_BASE && m->art_sprite < RENDER_SPRITE_COUNT)
    {
        gl_base_model *model = r->base_models + (m->art_sprite - RENDER_SPRITE_FIRST_BASE);
        return model->texture ? model : 0;
    }
    if (m->mesh_id >= RENDER_MESH_COUNT || !render_shape_models_on(cam)) return 0;
    for (s32 level = (s32)render_model_level_index(m->level); level >= 0; --level)
    {
        gl_base_model *model = r->mesh_models[m->mesh_id] + level;
        if (model->texture) return model;
    }
    return 0;
}

/* One instanced draw of a model, with its texture, for each instance
   that gl_model_of gives to that model and that is in the view. The
   model stays fixed when the view turns. The caller binds the model program. */
internal void
gl_draw_model(webgl2_renderer *r, render_commands *rc, m4 view_projection, gl_base_model *model, memory_arena *scratch)
{
    if (!model->texture) return;
    gl_mesh_instance *batch = PushArray(scratch, r->instance_capacity, gl_mesh_instance);
    u32 n = 0;
    for (u32 i = 0; i < rc->mesh_instance_count && n < r->instance_capacity; ++i)
    {
        render_mesh_instance *m = rc->mesh_instances + i;
        if (gl_model_of(r, m, &rc->cam) != model) continue;
        render_mesh_instance placed = render_model_instance(m);
        if (!render_model_in_view(view_projection, &placed)) continue;
        batch[n].position[0] = placed.position.x; batch[n].position[1] = placed.position.y; batch[n].position[2] = placed.position.z;
        batch[n].yaw = placed.yaw; batch[n].scale = placed.scale; batch[n].tint = placed.tint;
        n++;
    }
    if (!n) return;
    gl_bind_mesh(r, &model->mesh, batch, n);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, model->texture);
    glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)model->mesh.index_count, GL_UNSIGNED_INT, 0, (GLsizei)n);
}

internal void
gl_upload_constants(webgl2_renderer *r, gl_frame_constants *fc)
{
    glBindBuffer(GL_UNIFORM_BUFFER, r->constants);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(*fc), fc);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

/* Draws the scene into the default framebuffer. Call it after the clear
   and before ImGui. Nothing happens when the commands have no scene. */
internal void
webgl2_renderer_draw(webgl2_renderer *r, render_commands *rc, memory_arena *scratch)
{
    if (!r->ready || !rc || !rc->scene_enabled) return;
    u32 width = (u32)rc->viewport_width, height = (u32)rc->viewport_height;
    if (!width || !height) return;
    gl_ensure_terrain(r, &rc->terrain, scratch);

    glViewport(0, 0, (GLsizei)width, (GLsizei)height);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glBindVertexArray(r->vertex_array);

    /* constants */
    gl_frame_constants fc = {};
    fc.view_projection = camera_view_projection(&rc->cam, rc->viewport_width, rc->viewport_height);
    v3 light = v3_normalize(v3_make(0.4f, -1.0f, 0.3f));
    fc.light_direction[0] = light.x; fc.light_direction[1] = light.y; fc.light_direction[2] = light.z;
    m4 view = camera_view(&rc->cam);
    fc.camera_right[0] = view.m[0][0]; fc.camera_right[1] = view.m[1][0]; fc.camera_right[2] = view.m[2][0];
    fc.camera_up[0]    = view.m[0][1]; fc.camera_up[1]    = view.m[1][1]; fc.camera_up[2]    = view.m[2][1];
    fc.overlay_size[0] = (f32)r->overlay_width; fc.overlay_size[1] = (f32)r->overlay_height;
    fc.atlas_tiles = (f32)GL_ATLAS_TILES;
    fc.mode = 0.0f;
    gl_upload_constants(r, &fc);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, r->constants);

    /* overlay colours */
    if (r->overlay_texture && r->overlay_width * r->overlay_height)
    {
        u32 cells = r->overlay_width * r->overlay_height;
        u32 *colors = PushArray(scratch, cells, u32);
        if (rc->overlay) memcpy(colors, rc->overlay, (u64)cells * 4);
        else memset(colors, 0, (u64)cells * 4);
        glBindTexture(GL_TEXTURE_2D, r->overlay_texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)r->overlay_width, (GLsizei)r->overlay_height, GL_RGBA, GL_UNSIGNED_BYTE, colors);
    }

    /* terrain */
    if (r->terrain_index_count)
    {
        gl_disable_attributes();
        glBindBuffer(GL_ARRAY_BUFFER, r->terrain_vertices);
        gl_attribute(GL_ATTRIBUTE_POSITION, 3, sizeof(terrain_vertex), 0, 0);
        gl_attribute(GL_ATTRIBUTE_NORMAL,   3, sizeof(terrain_vertex), 12, 0);
        gl_attribute(GL_ATTRIBUTE_UV,       2, sizeof(terrain_vertex), 24, 0);
        gl_attribute(GL_ATTRIBUTE_CELL,     1, sizeof(terrain_vertex), 32, 0);
        gl_attribute(GL_ATTRIBUTE_TILE,     1, sizeof(terrain_vertex), 36, 0);
        glUseProgram(r->terrain_program);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, r->atlas_texture);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, r->overlay_texture);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->terrain_indices);
        glDrawElements(GL_TRIANGLES, (GLsizei)r->terrain_index_count, GL_UNSIGNED_INT, 0);

        if (rc->draw_grid && r->grid_index_count)
        {
            fc.mode = 1.0f;
            gl_upload_constants(r, &fc);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->grid_indices);
            glDrawElements(GL_LINES, (GLsizei)r->grid_index_count, GL_UNSIGNED_INT, 0);
            fc.mode = 0.0f;
            gl_upload_constants(r, &fc);
        }
    }

    /* meshes: one instanced draw per mesh id */
    glUseProgram(r->mesh_program);
    for (u32 mesh_id = 0; mesh_id < r->mesh_count; ++mesh_id)
    {
        gl_mesh_instance *batch = PushArray(scratch, r->instance_capacity, gl_mesh_instance);
        u32 n = 0;
        for (u32 i = 0; i < rc->mesh_instance_count && n < r->instance_capacity; ++i)
        {
            render_mesh_instance *m = rc->mesh_instances + i;
            if (m->mesh_id != mesh_id || gl_has_art(r, m) || gl_model_of(r, m, &rc->cam)) continue;
            batch[n].position[0] = m->position.x; batch[n].position[1] = m->position.y; batch[n].position[2] = m->position.z;
            batch[n].yaw = m->yaw; batch[n].scale = m->scale; batch[n].tint = m->tint;
            n++;
        }
        if (!n) continue;
        gl_bind_mesh(r, r->meshes + mesh_id, batch, n);
        glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)r->meshes[mesh_id].index_count, GL_UNSIGNED_INT, 0, (GLsizei)n);
    }

    /* the 3D models: one instanced draw for each base kind and each shape
       that has a model in the pack */
    glUseProgram(r->model_program);
    for (u32 kind = 0; kind < GL_BASE_KINDS; ++kind) gl_draw_model(r, rc, fc.view_projection, r->base_models + kind, scratch);
    for (u32 model_index = 0; model_index < RENDER_MESH_COUNT * RENDER_MODEL_LEVELS; ++model_index) gl_draw_model(r, rc, fc.view_projection, r->mesh_models[0] + model_index, scratch);

    /* billboards: one instanced draw per sprite id, alpha tested, drawn last.
       The quad winds counter-clockwise on the screen, so the back-face cull
       of the meshes drops it: the pass culls nothing. */
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
    glDisable(GL_CULL_FACE);
    glUseProgram(r->billboard_program);
    for (u32 sprite_id = 0; sprite_id < r->sprite_count; ++sprite_id)
    {
        gl_billboard_instance *batch = PushArray(scratch, r->instance_capacity, gl_billboard_instance);
        u32 n = 0;
        for (u32 i = 0; i < rc->billboard_count && n < r->instance_capacity; ++i)
        {
            render_billboard *b = rc->billboards + i;
            if (b->sprite_id != sprite_id) continue;
            batch[n].position[0] = b->position.x; batch[n].position[1] = b->position.y; batch[n].position[2] = b->position.z;
            batch[n].width = b->width; batch[n].height = b->height; batch[n].tint = b->tint;
            n++;
        }
        /* The art of a base stands in place of its mesh, on a card that faces the camera. */
        for (u32 i = 0; i < rc->mesh_instance_count && n < r->instance_capacity && sprite_id >= RENDER_SPRITE_FIRST_BASE; ++i)
        {
            render_mesh_instance *m = rc->mesh_instances + i;
            if (m->art_sprite != sprite_id || !gl_has_art(r, m) || gl_model_of(r, m, &rc->cam)) continue;
            render_billboard card = render_base_billboard(m, &rc->cam, r->sprites[sprite_id].aspect);
            batch[n].position[0] = card.position.x; batch[n].position[1] = card.position.y; batch[n].position[2] = card.position.z;
            batch[n].width = card.width; batch[n].height = card.height; batch[n].tint = card.tint;
            n++;
        }
        if (!n) continue;
        f32 mode = sprite_id >= RENDER_SPRITE_FIRST_BASE ? 2.0f : 0.0f;
        if (fc.mode != mode)
        {
            fc.mode = mode;
            gl_upload_constants(r, &fc);
        }
        gl_disable_attributes();
        glBindBuffer(GL_ARRAY_BUFFER, r->billboard_quad);
        gl_attribute(GL_ATTRIBUTE_POSITION, 2, 16, 0, 0);
        gl_attribute(GL_ATTRIBUTE_UV,       2, 16, 8, 0);
        glBindBuffer(GL_ARRAY_BUFFER, r->instances);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(n * sizeof(gl_billboard_instance)), batch, GL_STREAM_DRAW);
        gl_attribute(GL_ATTRIBUTE_PLACE, 3, sizeof(gl_billboard_instance), 0, 1);
        gl_attribute(GL_ATTRIBUTE_YAW,   1, sizeof(gl_billboard_instance), 12, 1);
        gl_attribute(GL_ATTRIBUTE_SCALE, 1, sizeof(gl_billboard_instance), 16, 1);
        gl_attribute_colour(GL_ATTRIBUTE_TINT, sizeof(gl_billboard_instance), 20);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, r->sprites[sprite_id].texture);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)n);
    }

    /* ImGui draws without depth and sets its own state. */
    gl_disable_attributes();
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

#endif /* WEBGL2_RENDERER_H */
