#ifndef D3D11_RENDERER_H
#define D3D11_RENDERER_H
/* The scene renderer of the platform layer: draws a render_commands with
   D3D11. Terrain is one mesh rebuilt when the terrain version changes;
   models and billboards are instanced. The shaders are HLSL compiled at
   startup. Only the platform includes this file; the game DLL pushes
   commands and never sees a D3D11 type. */

#include <d3d11.h>
#include <d3dcompiler.h>
#include "base.h"
#include "hexdom.h"             /* the file reader of the platform */
#include "render/render.h"
#include "render/terrain_mesh.h"
#include "render/placeholder_meshes.h"
#include "render/png.h"

#define D3D11_MAX_MESHES        32     /* at least RENDER_MESH_COUNT */
#define D3D11_MAX_SPRITES       32     /* at least RENDER_SPRITE_COUNT */
#define D3D11_ATLAS_TILES       8      /* material tiles in one row of the atlas */
#define D3D11_ATLAS_TILE_SIZE   64     /* pixels, for the generated placeholder atlas */
#define D3D11_SPRITE_SIZE       64

struct d3d11_mesh
{
    ID3D11Buffer *vertices;
    ID3D11Buffer *indices;
    u32           index_count;
};

struct d3d11_sprite
{
    ID3D11ShaderResourceView *view;     /* 0 for a base without art: the mesh stays */
    f32                       aspect;   /* the height of the picture over its width */
};

/* The 3D model of a base kind or of a placeholder shape: a mesh file and
   its colour texture. Both are 0 when the pack has no model of the kind. */
struct d3d11_base_model
{
    d3d11_mesh                mesh;
    ID3D11ShaderResourceView *texture;
};

#define D3D11_BASE_KINDS (RENDER_SPRITE_COUNT - RENDER_SPRITE_FIRST_BASE)

struct d3d11_frame_constants
{
    m4  view_projection;
    f32 light_direction[4];   /* xyz, w unused */
    f32 camera_right[4];
    f32 camera_up[4];
    f32 overlay_size[2];      /* cells wide, cells high */
    f32 atlas_tiles;
    f32 mode;                 /* 0 terrain, 1 grid lines */
};

typedef render_mesh_vertex d3d11_mesh_vertex;   /* the layout of placeholder_meshes.h and of the mesh files */

struct d3d11_mesh_instance
{
    f32 position[3];
    f32 yaw;
    f32 scale;
    u32 tint;
};

struct d3d11_billboard_instance
{
    f32 position[3];
    f32 width;
    f32 height;
    u32 tint;
};

struct d3d11_renderer
{
    ID3D11Device            *device;
    ID3D11DeviceContext     *context;
    char                     data_dir[260];
    platform_read_entire_file *read_file;   /* the file reader of the platform */

    ID3D11VertexShader      *terrain_vs;
    ID3D11PixelShader       *terrain_ps;
    ID3D11InputLayout       *terrain_layout;
    ID3D11VertexShader      *mesh_vs;
    ID3D11PixelShader       *mesh_ps;
    ID3D11PixelShader       *model_ps;          /* the textured model of a base, with mesh_vs */
    ID3D11InputLayout       *mesh_layout;
    ID3D11VertexShader      *billboard_vs;
    ID3D11PixelShader       *billboard_ps;
    ID3D11InputLayout       *billboard_layout;
    ID3D11Buffer            *constants;
    ID3D11SamplerState      *sampler;
    ID3D11RasterizerState   *rasterizer;
    ID3D11DepthStencilState *depth_on;
    ID3D11BlendState        *blend_alpha;

    ID3D11Texture2D         *depth_texture;
    ID3D11DepthStencilView  *depth_view;
    u32                      depth_width, depth_height;

    ID3D11Buffer            *terrain_vertices;
    ID3D11Buffer            *terrain_indices;
    ID3D11Buffer            *grid_indices;
    u32                      terrain_index_count;
    u32                      grid_index_count;
    u32                      terrain_version;
    u32                      terrain_cells;

    ID3D11Texture2D         *overlay_texture;
    ID3D11ShaderResourceView *overlay_view;
    u32                      overlay_width, overlay_height;

    ID3D11ShaderResourceView *atlas_view;
    d3d11_mesh               meshes[D3D11_MAX_MESHES];
    u32                      mesh_count;
    d3d11_sprite             sprites[D3D11_MAX_SPRITES];
    u32                      sprite_count;
    d3d11_base_model         base_models[D3D11_BASE_KINDS];
    d3d11_base_model         mesh_models[RENDER_MESH_COUNT];   /* render_mesh_model_files */
    ID3D11Buffer            *instances;        /* dynamic, shared by meshes and billboards */
    u32                      instance_capacity;
    ID3D11Buffer            *billboard_quad;
    ID3D11RasterizerState   *rasterizer_no_cull;   /* the billboards: the quad faces the camera from both sides */
    b32                      ready;
    char                     last_error[256];
};

/* ---- shaders ------------------------------------------------------------ */

static const char d3d11_shader_source[] =
"#pragma pack_matrix(row_major)\n"
"cbuffer Frame : register(b0) {\n"
"  float4x4 view_projection; float4 light_direction; float4 camera_right; float4 camera_up;\n"
"  float2 overlay_size; float atlas_tiles; float mode; };\n"
"Texture2D atlas : register(t0); Texture2D overlay : register(t1); Texture2D sprite : register(t2);\n"
"SamplerState linear_sampler : register(s0);\n"
"\n"
"struct TerrainIn { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD0; float cell : TEXCOORD1; float tile : TEXCOORD2; };\n"
"struct TerrainOut { float4 clip : SV_POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD0; float cell : TEXCOORD1; float tile : TEXCOORD2; };\n"
"TerrainOut terrain_vs(TerrainIn i) {\n"
"  TerrainOut o; o.clip = mul(float4(i.position, 1.0), view_projection);\n"
"  o.normal = i.normal; o.uv = i.uv; o.cell = i.cell; o.tile = i.tile; return o; }\n"
"float4 terrain_ps(TerrainOut i) : SV_TARGET {\n"
"  if (mode > 0.5) return float4(0.05, 0.05, 0.05, 1.0);\n"
"  float2 uv = float2((i.tile + frac(i.uv.x)) / atlas_tiles, frac(i.uv.y));\n"
"  float3 base = atlas.Sample(linear_sampler, uv).rgb;\n"
"  int w = (int)overlay_size.x; int c = (int)(i.cell + 0.5);\n"
"  float4 tint = overlay.Load(int3(c % w, c / w, 0));\n"
"  float light = 0.35 + 0.65 * saturate(dot(normalize(i.normal), -light_direction.xyz));\n"
"  float3 color = lerp(base, tint.rgb, tint.a) * light;\n"
"  return float4(color, 1.0); }\n"
"\n"
"struct MeshIn { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD0;\n"
"  float3 ipos : TEXCOORD3; float iyaw : TEXCOORD4; float iscale : TEXCOORD5; float4 itint : COLOR0; };\n"
"struct MeshOut { float4 clip : SV_POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD0; float4 tint : COLOR0; };\n"
"MeshOut mesh_vs(MeshIn i) {\n"
"  float c = cos(i.iyaw), s = sin(i.iyaw);\n"
"  float3 p = i.position * i.iscale; float3 r = float3(p.x * c + p.z * s, p.y, -p.x * s + p.z * c);\n"
"  float3 n = float3(i.normal.x * c + i.normal.z * s, i.normal.y, -i.normal.x * s + i.normal.z * c);\n"
"  MeshOut o; o.clip = mul(float4(r + i.ipos, 1.0), view_projection); o.normal = n; o.uv = i.uv; o.tint = i.itint; return o; }\n"
"float4 mesh_ps(MeshOut i) : SV_TARGET {\n"
"  float light = 0.35 + 0.65 * saturate(dot(normalize(i.normal), -light_direction.xyz));\n"
"  return float4(i.tint.rgb * light, 1.0); }\n"
"float4 model_ps(MeshOut i) : SV_TARGET {\n"     /* the model of a base: its texture, and the flat red of the banners in the faction colour */
"  float3 c = sprite.Sample(linear_sampler, i.uv).rgb;\n"
"  float mask = saturate((c.r - max(c.g, c.b) - 0.45) / 0.15);\n"
"  float3 base = lerp(c, i.tint.rgb * saturate(c.r / 0.82), mask);\n"
"  float light = 0.55 + 0.45 * saturate(dot(normalize(i.normal), -light_direction.xyz));\n"
"  return float4(base * light, 1.0); }\n"
"\n"
"struct BillboardIn { float2 corner : POSITION; float2 uv : TEXCOORD0;\n"
"  float3 ipos : TEXCOORD3; float iwidth : TEXCOORD4; float iheight : TEXCOORD5; float4 itint : COLOR0; };\n"
"struct BillboardOut { float4 clip : SV_POSITION; float2 uv : TEXCOORD0; float4 tint : COLOR0; };\n"
"BillboardOut billboard_vs(BillboardIn i) {\n"
"  float3 up = mode > 1.5 ? camera_up.xyz : float3(0, 1, 0);\n"     /* mode 2: the card of a base faces the camera */
"  float3 world = i.ipos + camera_right.xyz * (i.corner.x * i.iwidth) + up * (i.corner.y * i.iheight);\n"
"  BillboardOut o; o.clip = mul(float4(world, 1.0), view_projection); o.uv = i.uv; o.tint = i.itint; return o; }\n"
"float4 billboard_ps(BillboardOut i) : SV_TARGET {\n"
"  float4 c = sprite.Sample(linear_sampler, i.uv); clip(c.a - 0.5);\n"
"  float mask = saturate((1.0 - c.a) / 0.37);\n"                    /* alpha 160 of 255: the pixel takes the faction colour */
"  float shade = saturate(dot(c.rgb, float3(0.3, 0.59, 0.11)) / 0.31);\n"   /* the flat red of the mask shades to 1 */
"  return float4(lerp(c.rgb, i.tint.rgb * shade, mask), 1.0); }\n";

internal b32
d3d11_compile(d3d11_renderer *r, const char *entry, const char *target, ID3DBlob **blob)
{
    ID3DBlob *errors = 0;
    HRESULT hr = D3DCompile(d3d11_shader_source, sizeof(d3d11_shader_source) - 1, "hexdom.hlsl", 0, 0,
                            entry, target, D3DCOMPILE_ENABLE_STRICTNESS, 0, blob, &errors);
    if (FAILED(hr))
    {
        const char *message = errors ? (const char *)errors->GetBufferPointer() : "unknown error";
        snprintf(r->last_error, sizeof(r->last_error), "shader %s: %.200s", entry, message);
        if (errors) errors->Release();
        return false;
    }
    if (errors) errors->Release();
    return true;
}

/* ---- resources ---------------------------------------------------------- */

internal ID3D11Buffer *
d3d11_make_buffer(d3d11_renderer *r, const void *data, u32 size, UINT bind, b32 dynamic)
{
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth      = size;
    desc.Usage          = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
    desc.BindFlags      = bind;
    desc.CPUAccessFlags = dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
    D3D11_SUBRESOURCE_DATA initial = {};
    initial.pSysMem = data;
    ID3D11Buffer *buffer = 0;
    if (FAILED(r->device->CreateBuffer(&desc, data ? &initial : 0, &buffer))) return 0;
    return buffer;
}

internal ID3D11ShaderResourceView *
d3d11_make_texture(d3d11_renderer *r, const u32 *rgba, u32 width, u32 height, b32 dynamic, ID3D11Texture2D **texture_out)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width; desc.Height = height; desc.MipLevels = 1; desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.Usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
    D3D11_SUBRESOURCE_DATA initial = {};
    initial.pSysMem = rgba; initial.SysMemPitch = width * 4;
    ID3D11Texture2D *texture = 0;
    if (FAILED(r->device->CreateTexture2D(&desc, rgba ? &initial : 0, &texture))) return 0;
    ID3D11ShaderResourceView *view = 0;
    if (FAILED(r->device->CreateShaderResourceView(texture, 0, &view))) { texture->Release(); return 0; }
    if (texture_out) *texture_out = texture; else texture->Release();
    return view;
}

/* Reads a whole file of data_dir into the arena through the file reader
   of the platform; zero size when absent. */
internal u8 *
d3d11_read_data_file(d3d11_renderer *r, const char *relative, memory_arena *arena, u64 *size_out)
{
    char path[520];
    snprintf(path, sizeof(path), "%s/%s", r->data_dir, relative);
    read_file_result file = r->read_file(path, arena);
    *size_out = file.contents ? file.size : 0;
    return *size_out ? (u8 *)file.contents : 0;
}

/* The placeholder material atlas: one flat colour tile per terrain kind,
   with a faint checker so zoom is visible. Used when materials.png is absent.
   Colours are ABGR in memory (R8G8B8A8_UNORM, little endian). */
internal u32 *
d3d11_placeholder_atlas(memory_arena *arena)
{
    static const u32 colors[D3D11_ATLAS_TILES] = {
        0xFF9E5A2Eu, /* 0 water: blue  */ 0xFF4E8C5Au, /* 1 plain: green */ 0xFF2E6E3Au, /* 2 forest */
        0xFF5A7D96u, /* 3 hill: tan */    0xFF8A8A8Au, /* 4 mountain: grey */ 0xFF3A6E9Eu, /* 5 road: brown */
        0xFFC8C8C8u, /* 6 snow */          0xFF7A3A9Au  /* 7 unused */ };
    u32 w = D3D11_ATLAS_TILES * D3D11_ATLAS_TILE_SIZE, h = D3D11_ATLAS_TILE_SIZE;
    u32 *rgba = PushArray(arena, (u64)w * h, u32);
    for (u32 y = 0; y < h; ++y)
    {
        for (u32 x = 0; x < w; ++x)
        {
            u32 tile = x / D3D11_ATLAS_TILE_SIZE;
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
   160 makes the figure take the colour of the faction (billboard_ps). */
internal u32 *
d3d11_placeholder_sprite(memory_arena *arena)
{
    u32 s = D3D11_SPRITE_SIZE;
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

/* A placeholder shape of placeholder_meshes.h as a GPU mesh. */
internal b32
d3d11_placeholder_mesh(d3d11_renderer *r, u32 mesh_id, memory_arena *arena, d3d11_mesh *mesh)
{
    placeholder_mesh shape = {};
    if (!placeholder_mesh_build(mesh_id, arena, &shape)) return false;
    mesh->vertices    = d3d11_make_buffer(r, shape.vertices, shape.vertex_count * (u32)sizeof(d3d11_mesh_vertex), D3D11_BIND_VERTEX_BUFFER, false);
    mesh->indices     = d3d11_make_buffer(r, shape.indices, shape.index_count * 4, D3D11_BIND_INDEX_BUFFER, false);
    mesh->index_count = shape.index_count;
    return mesh->vertices && mesh->indices;
}

/* A mesh file of the pack (render_mesh_file_parse) as a GPU mesh. */
internal b32
d3d11_load_mesh_file(d3d11_renderer *r, const char *relative, memory_arena *arena, d3d11_mesh *mesh)
{
    u64 size = 0;
    u8 *data = d3d11_read_data_file(r, relative, arena, &size);
    render_mesh_file file = {};
    if (!render_mesh_file_parse(data, size, &file)) return false;
    mesh->vertices = d3d11_make_buffer(r, file.vertices, file.vertex_count * (u32)sizeof(d3d11_mesh_vertex), D3D11_BIND_VERTEX_BUFFER, false);
    mesh->indices  = d3d11_make_buffer(r, file.indices, file.index_count * 4, D3D11_BIND_INDEX_BUFFER, false);
    mesh->index_count = file.index_count;
    return mesh->vertices && mesh->indices;
}

/* A texture with the full chain of mip levels: the GPU makes the smaller
   levels from the picture. A model seen from far away needs them, or its
   texture shimmers when the view moves. */
internal ID3D11ShaderResourceView *
d3d11_make_mipmapped_texture(d3d11_renderer *r, const u32 *rgba, u32 width, u32 height)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width; desc.Height = height; desc.MipLevels = 0; desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    ID3D11Texture2D *texture = 0;
    if (FAILED(r->device->CreateTexture2D(&desc, 0, &texture))) return 0;
    r->context->UpdateSubresource(texture, 0, 0, rgba, width * 4, 0);
    ID3D11ShaderResourceView *view = 0;
    if (SUCCEEDED(r->device->CreateShaderResourceView(texture, 0, &view))) r->context->GenerateMips(view);
    texture->Release();
    return view;
}

internal ID3D11ShaderResourceView *
d3d11_load_png_texture(d3d11_renderer *r, const char *relative, memory_arena *arena, f32 *aspect_out = 0, b32 mipmaps = false)
{
    u64 size = 0;
    u8 *data = d3d11_read_data_file(r, relative, arena, &size);
    if (!data) return 0;
    png_image image = png_decode(data, size, arena);
    if (image.error || !image.width) return 0;
    if (aspect_out) *aspect_out = (f32)image.height / (f32)image.width;
    if (mipmaps) return d3d11_make_mipmapped_texture(r, (const u32 *)image.rgba, image.width, image.height);
    return d3d11_make_texture(r, (const u32 *)image.rgba, image.width, image.height, false, 0);
}

/* The model of one base kind or shape: <name>.hxm and <name>.png of the
   pack. If one of the two files is absent, the kind has no model. The GPU
   keeps the mesh and the texture, so the scratch arena gets its bytes back. */
internal void
d3d11_load_base_model(d3d11_renderer *r, const char *name, memory_arena *arena, d3d11_base_model *model)
{
    *model = {};
    if (!name) return;
    char mesh_file[260], texture_file[260];
    snprintf(mesh_file, sizeof(mesh_file), "%s.hxm", name);
    snprintf(texture_file, sizeof(texture_file), "%s.png", name);
    u64 arena_used = arena->used;
    if (d3d11_load_mesh_file(r, mesh_file, arena, &model->mesh)) model->texture = d3d11_load_png_texture(r, texture_file, arena, 0, true);
    arena->used = arena_used;
    if (model->texture) return;
    if (model->mesh.vertices) model->mesh.vertices->Release();
    if (model->mesh.indices) model->mesh.indices->Release();
    *model = {};
}

/* ---- init --------------------------------------------------------------- */

internal b32
d3d11_renderer_init(d3d11_renderer *r, ID3D11Device *device, ID3D11DeviceContext *context,
                    platform_read_entire_file *read_file, const char *data_dir, memory_arena *scratch)
{
    *r = {};
    r->device = device; r->context = context;
    r->read_file = read_file;
    snprintf(r->data_dir, sizeof(r->data_dir), "%s", data_dir);

    ID3DBlob *vs_blob = 0, *ps_blob = 0;
    if (!d3d11_compile(r, "terrain_vs", "vs_4_0", &vs_blob)) return false;
    device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), 0, &r->terrain_vs);
    D3D11_INPUT_ELEMENT_DESC terrain_layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT,       0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 2, DXGI_FORMAT_R32_FLOAT,       0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
    device->CreateInputLayout(terrain_layout, 5, vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), &r->terrain_layout);
    vs_blob->Release();
    if (!d3d11_compile(r, "terrain_ps", "ps_4_0", &ps_blob)) return false;
    device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), 0, &r->terrain_ps);
    ps_blob->Release();

    if (!d3d11_compile(r, "mesh_vs", "vs_4_0", &vs_blob)) return false;
    device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), 0, &r->mesh_vs);
    D3D11_INPUT_ELEMENT_DESC mesh_layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 3, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 4, DXGI_FORMAT_R32_FLOAT,       1, 12, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 5, DXGI_FORMAT_R32_FLOAT,       1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  1, 20, D3D11_INPUT_PER_INSTANCE_DATA, 1 } };
    device->CreateInputLayout(mesh_layout, 7, vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), &r->mesh_layout);
    vs_blob->Release();
    if (!d3d11_compile(r, "mesh_ps", "ps_4_0", &ps_blob)) return false;
    device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), 0, &r->mesh_ps);
    ps_blob->Release();
    if (!d3d11_compile(r, "model_ps", "ps_4_0", &ps_blob)) return false;
    device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), 0, &r->model_ps);
    ps_blob->Release();

    if (!d3d11_compile(r, "billboard_vs", "vs_4_0", &vs_blob)) return false;
    device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), 0, &r->billboard_vs);
    D3D11_INPUT_ELEMENT_DESC billboard_layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 8,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 3, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 4, DXGI_FORMAT_R32_FLOAT,       1, 12, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 5, DXGI_FORMAT_R32_FLOAT,       1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  1, 20, D3D11_INPUT_PER_INSTANCE_DATA, 1 } };
    device->CreateInputLayout(billboard_layout, 6, vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), &r->billboard_layout);
    vs_blob->Release();
    if (!d3d11_compile(r, "billboard_ps", "ps_4_0", &ps_blob)) return false;
    device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), 0, &r->billboard_ps);
    ps_blob->Release();

    r->constants = d3d11_make_buffer(r, 0, sizeof(d3d11_frame_constants), D3D11_BIND_CONSTANT_BUFFER, true);

    D3D11_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    device->CreateSamplerState(&sampler, &r->sampler);

    D3D11_RASTERIZER_DESC raster = {};
    raster.FillMode = D3D11_FILL_SOLID; raster.CullMode = D3D11_CULL_BACK; raster.DepthClipEnable = TRUE;
    device->CreateRasterizerState(&raster, &r->rasterizer);
    raster.CullMode = D3D11_CULL_NONE;
    device->CreateRasterizerState(&raster, &r->rasterizer_no_cull);

    D3D11_DEPTH_STENCIL_DESC depth = {};
    depth.DepthEnable = TRUE; depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; depth.DepthFunc = D3D11_COMPARISON_LESS;
    device->CreateDepthStencilState(&depth, &r->depth_on);

    D3D11_BLEND_DESC blend = {};
    blend.RenderTarget[0].BlendEnable = TRUE;
    blend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device->CreateBlendState(&blend, &r->blend_alpha);

    /* the material atlas: the pack's file, or the placeholder */
    r->atlas_view = d3d11_load_png_texture(r, "materials.png", scratch);
    if (!r->atlas_view)
    {
        u32 *atlas = d3d11_placeholder_atlas(scratch);
        r->atlas_view = d3d11_make_texture(r, atlas, D3D11_ATLAS_TILES * D3D11_ATLAS_TILE_SIZE, D3D11_ATLAS_TILE_SIZE, false, 0);
    }

    /* mesh 0: the settlement, from settlement.hxm or the placeholder house.
       The other meshes are the placeholder shapes of the facility slots. */
    if (!d3d11_load_mesh_file(r, "settlement.hxm", scratch, r->meshes + 0))
    {
        d3d11_placeholder_mesh(r, RENDER_MESH_SETTLEMENT, scratch, r->meshes + 0);
    }
    r->mesh_count = 1;
    for (u32 mesh_id = 1; mesh_id < RENDER_MESH_COUNT && mesh_id < D3D11_MAX_MESHES; ++mesh_id)
    {
        if (!d3d11_placeholder_mesh(r, mesh_id, scratch, r->meshes + mesh_id)) break;
        r->mesh_count = mesh_id + 1;
    }

    /* The sprites: one picture for each render_sprite, from the sprites
       folder of the content pack: the art first, then the placeholder
       picture. If both are absent, the renderer uses the placeholder
       figure (docs/ASSETS.md, "Unit sprites"). A base has no placeholder
       picture: without its art, the view stays 0 and the renderer draws
       the mesh of the base (docs/ASSETS.md, "Base art"). */
    for (u32 sprite_id = 0; sprite_id < RENDER_SPRITE_COUNT && sprite_id < D3D11_MAX_SPRITES; ++sprite_id)
    {
        d3d11_sprite *sprite_slot = r->sprites + sprite_id;
        sprite_slot->view   = 0;
        sprite_slot->aspect = 1.0f;
        r->sprite_count = sprite_id + 1;
        if (render_sprite_art_files[sprite_id]) sprite_slot->view = d3d11_load_png_texture(r, render_sprite_art_files[sprite_id], scratch, &sprite_slot->aspect);
        if (sprite_id >= RENDER_SPRITE_FIRST_BASE) continue;
        if (!sprite_slot->view) sprite_slot->view = d3d11_load_png_texture(r, render_sprite_files[sprite_id], scratch, &sprite_slot->aspect);
        if (!sprite_slot->view)
        {
            u32 *sprite = d3d11_placeholder_sprite(scratch);
            sprite_slot->view = d3d11_make_texture(r, sprite, D3D11_SPRITE_SIZE, D3D11_SPRITE_SIZE, false, 0);
        }
    }

    /* The 3D models of the base kinds (docs/ASSETS.md, "Base models"). A
       kind with a model shows it in place of its art card. */
    for (u32 kind = 0; kind < D3D11_BASE_KINDS; ++kind)
    {
        d3d11_load_base_model(r, render_base_model_files[kind], scratch, r->base_models + kind);
    }
    /* The models of the placeholder shapes, for example the market. */
    for (u32 mesh_id = 0; mesh_id < RENDER_MESH_COUNT; ++mesh_id)
    {
        d3d11_load_base_model(r, render_mesh_model_files[mesh_id], scratch, r->mesh_models + mesh_id);
    }

    r->instance_capacity = RENDER_MAX_MESH_INSTANCES > RENDER_MAX_BILLBOARDS ? RENDER_MAX_MESH_INSTANCES : RENDER_MAX_BILLBOARDS;
    r->instances = d3d11_make_buffer(r, 0, r->instance_capacity * (u32)sizeof(d3d11_mesh_instance), D3D11_BIND_VERTEX_BUFFER, true);

    f32 quad[] = { -0.5f, 0.0f, 0.0f, 1.0f,   0.5f, 0.0f, 1.0f, 1.0f,   -0.5f, 1.0f, 0.0f, 0.0f,
                    0.5f, 0.0f, 1.0f, 1.0f,   0.5f, 1.0f, 1.0f, 0.0f,   -0.5f, 1.0f, 0.0f, 0.0f };
    r->billboard_quad = d3d11_make_buffer(r, quad, sizeof(quad), D3D11_BIND_VERTEX_BUFFER, false);

    r->ready = r->constants && r->atlas_view && r->meshes[0].vertices && r->sprites[0].view && r->instances && r->billboard_quad;
    if (!r->ready) snprintf(r->last_error, sizeof(r->last_error), "a D3D11 resource could not be created");
    return r->ready;
}

internal void
d3d11_release(IUnknown **p)
{
    if (*p) { (*p)->Release(); *p = 0; }
}

internal void
d3d11_renderer_destroy(d3d11_renderer *r)
{
    d3d11_release((IUnknown **)&r->terrain_vs); d3d11_release((IUnknown **)&r->terrain_ps); d3d11_release((IUnknown **)&r->terrain_layout);
    d3d11_release((IUnknown **)&r->mesh_vs); d3d11_release((IUnknown **)&r->mesh_ps); d3d11_release((IUnknown **)&r->mesh_layout);
    d3d11_release((IUnknown **)&r->model_ps);
    for (u32 kind = 0; kind < D3D11_BASE_KINDS; ++kind)
    {
        d3d11_base_model *model = r->base_models + kind;
        d3d11_release((IUnknown **)&model->mesh.vertices); d3d11_release((IUnknown **)&model->mesh.indices); d3d11_release((IUnknown **)&model->texture);
    }
    for (u32 mesh_id = 0; mesh_id < RENDER_MESH_COUNT; ++mesh_id)
    {
        d3d11_base_model *model = r->mesh_models + mesh_id;
        d3d11_release((IUnknown **)&model->mesh.vertices); d3d11_release((IUnknown **)&model->mesh.indices); d3d11_release((IUnknown **)&model->texture);
    }
    d3d11_release((IUnknown **)&r->billboard_vs); d3d11_release((IUnknown **)&r->billboard_ps); d3d11_release((IUnknown **)&r->billboard_layout);
    d3d11_release((IUnknown **)&r->constants); d3d11_release((IUnknown **)&r->sampler); d3d11_release((IUnknown **)&r->rasterizer);
    d3d11_release((IUnknown **)&r->rasterizer_no_cull);
    d3d11_release((IUnknown **)&r->depth_on); d3d11_release((IUnknown **)&r->blend_alpha);
    d3d11_release((IUnknown **)&r->depth_view); d3d11_release((IUnknown **)&r->depth_texture);
    d3d11_release((IUnknown **)&r->terrain_vertices); d3d11_release((IUnknown **)&r->terrain_indices); d3d11_release((IUnknown **)&r->grid_indices);
    d3d11_release((IUnknown **)&r->overlay_view); d3d11_release((IUnknown **)&r->overlay_texture); d3d11_release((IUnknown **)&r->atlas_view);
    for (u32 i = 0; i < r->mesh_count; ++i) { d3d11_release((IUnknown **)&r->meshes[i].vertices); d3d11_release((IUnknown **)&r->meshes[i].indices); }
    for (u32 i = 0; i < r->sprite_count; ++i) d3d11_release((IUnknown **)&r->sprites[i].view);
    d3d11_release((IUnknown **)&r->instances); d3d11_release((IUnknown **)&r->billboard_quad);
    r->ready = false;
}

/* ---- per frame ---------------------------------------------------------- */

internal void
d3d11_ensure_depth(d3d11_renderer *r, u32 width, u32 height)
{
    if (r->depth_view && r->depth_width == width && r->depth_height == height) return;
    d3d11_release((IUnknown **)&r->depth_view);
    d3d11_release((IUnknown **)&r->depth_texture);
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width; desc.Height = height; desc.MipLevels = 1; desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (SUCCEEDED(r->device->CreateTexture2D(&desc, 0, &r->depth_texture)))
    {
        r->device->CreateDepthStencilView(r->depth_texture, 0, &r->depth_view);
    }
    r->depth_width = width; r->depth_height = height;
}

/* Rebuilds the terrain buffers from the command's terrain when its version changed. */
internal void
d3d11_ensure_terrain(d3d11_renderer *r, render_terrain *terrain, memory_arena *scratch)
{
    u32 cells = terrain->width * terrain->height;
    if (r->terrain_vertices && r->terrain_version == terrain->version && r->terrain_cells == cells) return;
    d3d11_release((IUnknown **)&r->terrain_vertices);
    d3d11_release((IUnknown **)&r->terrain_indices);
    d3d11_release((IUnknown **)&r->grid_indices);
    r->terrain_index_count = r->grid_index_count = 0;
    r->terrain_version = terrain->version;
    r->terrain_cells = cells;
    if (!cells) return;

    terrain_mesh mesh = terrain_mesh_build(terrain, scratch);
    r->terrain_vertices = d3d11_make_buffer(r, mesh.vertices, mesh.vertex_count * (u32)sizeof(terrain_vertex), D3D11_BIND_VERTEX_BUFFER, false);
    r->terrain_indices  = d3d11_make_buffer(r, mesh.indices, mesh.index_count * 4, D3D11_BIND_INDEX_BUFFER, false);
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
    r->grid_indices = d3d11_make_buffer(r, lines, n * 4, D3D11_BIND_INDEX_BUFFER, false);
    r->grid_index_count = n;

    d3d11_release((IUnknown **)&r->overlay_view);
    d3d11_release((IUnknown **)&r->overlay_texture);
    r->overlay_view = d3d11_make_texture(r, 0, terrain->width, terrain->height, true, &r->overlay_texture);
    r->overlay_width = terrain->width; r->overlay_height = terrain->height;
}

internal void
d3d11_upload(d3d11_renderer *r, ID3D11Resource *resource, const void *data, u32 size, u32 row_pitch)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(r->context->Map(resource, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
    if (row_pitch == 0 || row_pitch == mapped.RowPitch) memcpy(mapped.pData, data, size);
    else
    {
        u32 rows = size / row_pitch;
        for (u32 y = 0; y < rows; ++y) memcpy((u8 *)mapped.pData + (u64)y * mapped.RowPitch, (const u8 *)data + (u64)y * row_pitch, row_pitch);
    }
    r->context->Unmap(resource, 0);
}

/* True when the platform draws the art of a base in place of the mesh:
   the instance names a base sprite, and the pack has its picture. */
internal b32
d3d11_has_art(d3d11_renderer *r, render_mesh_instance *m)
{
    return m->art_sprite >= RENDER_SPRITE_FIRST_BASE && m->art_sprite < r->sprite_count && r->sprites[m->art_sprite].view != 0;
}

/* The 3D model that the platform draws for an instance, or 0. A model goes
   before the art card and before the placeholder mesh. A base uses only
   the model of its kind. Another instance uses the model of its shape,
   if the camera is near (render_shape_models_on). */
internal d3d11_base_model *
d3d11_model_of(d3d11_renderer *r, render_mesh_instance *m, const camera *cam)
{
    d3d11_base_model *model = 0;
    if (m->art_sprite >= RENDER_SPRITE_FIRST_BASE && m->art_sprite < RENDER_SPRITE_COUNT) model = r->base_models + (m->art_sprite - RENDER_SPRITE_FIRST_BASE);
    else if (m->mesh_id < RENDER_MESH_COUNT && render_shape_models_on(cam)) model = r->mesh_models + m->mesh_id;
    return model && model->texture ? model : 0;
}

/* One instanced draw of a model, with its texture, for each instance
   that d3d11_model_of gives to that model and that is in the view. The
   model stays fixed when the view turns. */
internal void
d3d11_draw_model(d3d11_renderer *r, render_commands *rc, m4 view_projection, d3d11_base_model *model, memory_arena *scratch)
{
    if (!model->texture) return;
    d3d11_mesh_instance *batch = PushArray(scratch, r->instance_capacity, d3d11_mesh_instance);
    u32 n = 0;
    for (u32 i = 0; i < rc->mesh_instance_count && n < r->instance_capacity; ++i)
    {
        render_mesh_instance *m = rc->mesh_instances + i;
        if (d3d11_model_of(r, m, &rc->cam) != model) continue;
        render_mesh_instance placed = render_model_instance(m);
        if (!render_model_in_view(view_projection, &placed)) continue;
        batch[n].position[0] = placed.position.x; batch[n].position[1] = placed.position.y; batch[n].position[2] = placed.position.z;
        batch[n].yaw = placed.yaw; batch[n].scale = placed.scale; batch[n].tint = placed.tint;
        n++;
    }
    if (!n) return;
    ID3D11DeviceContext *ctx = r->context;
    d3d11_upload(r, r->instances, batch, n * (u32)sizeof(d3d11_mesh_instance), 0);
    UINT strides[2] = { sizeof(d3d11_mesh_vertex), sizeof(d3d11_mesh_instance) }, offsets[2] = { 0, 0 };
    ID3D11Buffer *buffers[2] = { model->mesh.vertices, r->instances };
    ctx->IASetInputLayout(r->mesh_layout);
    ctx->IASetVertexBuffers(0, 2, buffers, strides, offsets);
    ctx->IASetIndexBuffer(model->mesh.indices, DXGI_FORMAT_R32_UINT, 0);
    ctx->VSSetShader(r->mesh_vs, 0, 0);
    ctx->PSSetShader(r->model_ps, 0, 0);
    ctx->PSSetShaderResources(2, 1, &model->texture);
    ctx->DrawIndexedInstanced(model->mesh.index_count, n, 0, 0, 0);
}

/* Draws the scene into the current render target. Call it after the clear
   and before ImGui. Nothing happens when the commands have no scene. */
internal void
d3d11_renderer_draw(d3d11_renderer *r, render_commands *rc, ID3D11RenderTargetView *target, memory_arena *scratch)
{
    if (!r->ready || !rc || !rc->scene_enabled) return;
    u32 width = (u32)rc->viewport_width, height = (u32)rc->viewport_height;
    if (!width || !height) return;
    d3d11_ensure_depth(r, width, height);
    d3d11_ensure_terrain(r, &rc->terrain, scratch);
    if (!r->depth_view) return;

    ID3D11DeviceContext *ctx = r->context;
    ctx->OMSetRenderTargets(1, &target, r->depth_view);
    ctx->ClearDepthStencilView(r->depth_view, D3D11_CLEAR_DEPTH, 1.0f, 0);
    D3D11_VIEWPORT viewport = {};
    viewport.Width = (f32)width; viewport.Height = (f32)height; viewport.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &viewport);
    ctx->RSSetState(r->rasterizer);
    ctx->OMSetDepthStencilState(r->depth_on, 0);
    ctx->OMSetBlendState(0, 0, 0xFFFFFFFF);
    ctx->PSSetSamplers(0, 1, &r->sampler);

    /* constants */
    d3d11_frame_constants fc = {};
    fc.view_projection = camera_view_projection(&rc->cam, rc->viewport_width, rc->viewport_height);
    v3 light = v3_normalize(v3_make(0.4f, -1.0f, 0.3f));
    fc.light_direction[0] = light.x; fc.light_direction[1] = light.y; fc.light_direction[2] = light.z;
    m4 view = camera_view(&rc->cam);
    fc.camera_right[0] = view.m[0][0]; fc.camera_right[1] = view.m[1][0]; fc.camera_right[2] = view.m[2][0];
    fc.camera_up[0]    = view.m[0][1]; fc.camera_up[1]    = view.m[1][1]; fc.camera_up[2]    = view.m[2][1];
    fc.overlay_size[0] = (f32)r->overlay_width; fc.overlay_size[1] = (f32)r->overlay_height;
    fc.atlas_tiles = (f32)D3D11_ATLAS_TILES;
    fc.mode = 0.0f;
    d3d11_upload(r, r->constants, &fc, sizeof(fc), 0);
    ctx->VSSetConstantBuffers(0, 1, &r->constants);
    ctx->PSSetConstantBuffers(0, 1, &r->constants);

    /* overlay colours */
    if (r->overlay_texture && r->overlay_width * r->overlay_height)
    {
        u32 cells = r->overlay_width * r->overlay_height;
        u32 *colors = PushArray(scratch, cells, u32);
        if (rc->overlay) memcpy(colors, rc->overlay, (u64)cells * 4);
        d3d11_upload(r, r->overlay_texture, colors, cells * 4, r->overlay_width * 4);
    }

    /* terrain */
    if (r->terrain_index_count)
    {
        UINT stride = sizeof(terrain_vertex), offset = 0;
        ctx->IASetInputLayout(r->terrain_layout);
        ctx->IASetVertexBuffers(0, 1, &r->terrain_vertices, &stride, &offset);
        ctx->IASetIndexBuffer(r->terrain_indices, DXGI_FORMAT_R32_UINT, 0);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->VSSetShader(r->terrain_vs, 0, 0);
        ctx->PSSetShader(r->terrain_ps, 0, 0);
        ID3D11ShaderResourceView *views[2] = { r->atlas_view, r->overlay_view };
        ctx->PSSetShaderResources(0, 2, views);
        ctx->DrawIndexed(r->terrain_index_count, 0, 0);

        if (rc->draw_grid && r->grid_index_count)
        {
            fc.mode = 1.0f;
            d3d11_upload(r, r->constants, &fc, sizeof(fc), 0);
            ctx->IASetIndexBuffer(r->grid_indices, DXGI_FORMAT_R32_UINT, 0);
            ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
            ctx->DrawIndexed(r->grid_index_count, 0, 0);
            fc.mode = 0.0f;
            d3d11_upload(r, r->constants, &fc, sizeof(fc), 0);
            ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        }
    }

    /* meshes: one instanced draw per mesh id */
    for (u32 mesh_id = 0; mesh_id < r->mesh_count; ++mesh_id)
    {
        d3d11_mesh_instance *batch = PushArray(scratch, r->instance_capacity, d3d11_mesh_instance);
        u32 n = 0;
        for (u32 i = 0; i < rc->mesh_instance_count && n < r->instance_capacity; ++i)
        {
            render_mesh_instance *m = rc->mesh_instances + i;
            if (m->mesh_id != mesh_id || d3d11_has_art(r, m) || d3d11_model_of(r, m, &rc->cam)) continue;
            batch[n].position[0] = m->position.x; batch[n].position[1] = m->position.y; batch[n].position[2] = m->position.z;
            batch[n].yaw = m->yaw; batch[n].scale = m->scale; batch[n].tint = m->tint;
            n++;
        }
        if (!n) continue;
        d3d11_upload(r, r->instances, batch, n * (u32)sizeof(d3d11_mesh_instance), 0);
        UINT strides[2] = { sizeof(d3d11_mesh_vertex), sizeof(d3d11_mesh_instance) }, offsets[2] = { 0, 0 };
        ID3D11Buffer *buffers[2] = { r->meshes[mesh_id].vertices, r->instances };
        ctx->IASetInputLayout(r->mesh_layout);
        ctx->IASetVertexBuffers(0, 2, buffers, strides, offsets);
        ctx->IASetIndexBuffer(r->meshes[mesh_id].indices, DXGI_FORMAT_R32_UINT, 0);
        ctx->VSSetShader(r->mesh_vs, 0, 0);
        ctx->PSSetShader(r->mesh_ps, 0, 0);
        ctx->DrawIndexedInstanced(r->meshes[mesh_id].index_count, n, 0, 0, 0);
    }

    /* the 3D models: one instanced draw for each base kind and each shape
       that has a model in the pack */
    for (u32 kind = 0; kind < D3D11_BASE_KINDS; ++kind) d3d11_draw_model(r, rc, fc.view_projection, r->base_models + kind, scratch);
    for (u32 mesh_id = 0; mesh_id < RENDER_MESH_COUNT; ++mesh_id) d3d11_draw_model(r, rc, fc.view_projection, r->mesh_models + mesh_id, scratch);

    /* billboards: one instanced draw per sprite id, alpha tested, drawn last.
       The quad winds counter-clockwise on the screen, so the back-face cull
       of the meshes drops it: the pass culls nothing. */
    ctx->OMSetBlendState(r->blend_alpha, 0, 0xFFFFFFFF);
    ctx->RSSetState(r->rasterizer_no_cull);
    for (u32 sprite_id = 0; sprite_id < r->sprite_count; ++sprite_id)
    {
        d3d11_billboard_instance *batch = PushArray(scratch, r->instance_capacity, d3d11_billboard_instance);
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
            if (m->art_sprite != sprite_id || !d3d11_has_art(r, m) || d3d11_model_of(r, m, &rc->cam)) continue;
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
            d3d11_upload(r, r->constants, &fc, sizeof(fc), 0);
        }
        d3d11_upload(r, r->instances, batch, n * (u32)sizeof(d3d11_billboard_instance), 0);
        UINT strides[2] = { 16, sizeof(d3d11_billboard_instance) }, offsets[2] = { 0, 0 };
        ID3D11Buffer *buffers[2] = { r->billboard_quad, r->instances };
        ctx->IASetInputLayout(r->billboard_layout);
        ctx->IASetVertexBuffers(0, 2, buffers, strides, offsets);
        ctx->VSSetShader(r->billboard_vs, 0, 0);
        ctx->PSSetShader(r->billboard_ps, 0, 0);
        ctx->PSSetShaderResources(2, 1, &r->sprites[sprite_id].view);
        ctx->DrawInstanced(6, n, 0, 0);
    }
    if (fc.mode != 0.0f)
    {
        fc.mode = 0.0f;
        d3d11_upload(r, r->constants, &fc, sizeof(fc), 0);
    }
    ctx->OMSetBlendState(0, 0, 0xFFFFFFFF);
    ctx->RSSetState(r->rasterizer);
    ctx->OMSetRenderTargets(1, &target, 0);   /* ImGui draws without depth */
}

#endif /* D3D11_RENDERER_H */
