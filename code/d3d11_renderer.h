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
#include "render/picture.h"

#define D3D11_MAX_MESHES        32     /* at least RENDER_MESH_COUNT */
#define D3D11_MAX_SPRITES       32     /* at least RENDER_SPRITE_COUNT */
#define D3D11_ATLAS_TILES       8      /* the layers of the placeholder atlas */
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
    f32 atlas_tiles;          /* the layers of the atlas: more than 8 is materials.jpg */
    f32 mode;                 /* 0 terrain, 1 grid lines */
    f32 terrain_state[4];     /* season, ripple, unused, unused */
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

/* The build of a terrain mesh on a worker thread (d3d11_ensure_terrain). */
struct d3d11_terrain_job
{
    HANDLE         thread;     /* zero when no build runs */
    volatile LONG  done;
    u8            *memory;
    memory_arena   arena;
    render_terrain input;      /* a copy: its arrays are in the arena */
    struct d3d11_renderer *renderer;
    /* The worker makes the buffers too: ID3D11Device is thread safe, only
       the device context is not. */
    ID3D11Buffer  *vertices;
    ID3D11Buffer  *indices;
    ID3D11Buffer  *grid;
    u32            index_count;
    u32            grid_count;
};

struct d3d11_renderer
{
    ID3D11Device            *device;
    ID3D11DeviceContext     *context;
    char                     data_dir[260];
    char                     pack_dir[260];   /* the art of the active pack: read before data_dir. Empty: none */
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
    b32                      terrain_on_worker;   /* a worker thread builds a new terrain mesh */
    d3d11_terrain_job        terrain_job;

    ID3D11Texture2D         *overlay_texture;
    ID3D11ShaderResourceView *overlay_view;
    u32                      overlay_width, overlay_height;

    ID3D11ShaderResourceView *atlas_view;
    u32                       atlas_layers;
    d3d11_mesh               meshes[D3D11_MAX_MESHES];
    u32                      mesh_count;
    d3d11_sprite             sprites[D3D11_MAX_SPRITES];
    u32                      sprite_count;
    d3d11_base_model         base_models[D3D11_BASE_KINDS];
    d3d11_base_model         mesh_models[RENDER_MODEL_COUNT][RENDER_MODEL_LEVELS];   /* render_model_files */
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
"  float2 overlay_size; float atlas_tiles; float mode; float4 terrain_state; };\n"
"Texture2DArray atlas : register(t0); Texture2D overlay : register(t1); Texture2D sprite : register(t2);\n"
"SamplerState linear_sampler : register(s0);\n"
"\n"
"struct TerrainIn { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD0; float cell : TEXCOORD1; float tile : TEXCOORD2; };\n"
"struct TerrainOut { float4 clip : SV_POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD0; float cell : TEXCOORD1; float tile : TEXCOORD2; };\n"
"TerrainOut terrain_vs(TerrainIn i) {\n"
"  TerrainOut o; o.clip = mul(float4(i.position, 1.0), view_projection);\n"
"  o.normal = i.normal; o.uv = i.uv; o.cell = i.cell; o.tile = i.tile; return o; }\n"
"float4 terrain_ps(TerrainOut i) : SV_TARGET {\n"
"  if (mode > 0.5) return float4(0.05, 0.05, 0.05, 1.0);\n"
"  float code = floor(i.tile + 0.5); float kind = floor(code / 256.0); float layer = code - kind * 256.0; float frame = layer;\n"
"  if (atlas_tiles > 8.5) {\n"
"    layer = kind * 4.0 + terrain_state.x; frame = layer;\n"
"    if ((kind > 5.5 && kind < 8.5) || (kind > 11.5 && kind < 13.5)) frame = 80.0 + (kind < 9.0 ? kind - 6.0 : kind - 9.0) * 4.0 + terrain_state.x; }\n"
"  float3 base = lerp(atlas.Sample(linear_sampler, float3(i.uv, layer)).rgb, atlas.Sample(linear_sampler, float3(i.uv, frame)).rgb, terrain_state.y);\n"
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

/* Reads a whole file into the arena through the file reader of the
   platform: the file of pack_dir first, then the file of data_dir. Zero
   size when both are absent. */
internal u8 *
d3d11_read_data_file(const d3d11_renderer *r, const char *relative, memory_arena *arena, u64 *size_out)
{
    char path[520];
    read_file_result file = {};
    if (r->pack_dir[0])
    {
        snprintf(path, sizeof(path), "%s/%s", r->pack_dir, relative);
        file = r->read_file(path, arena);
    }
    if (!file.contents)
    {
        snprintf(path, sizeof(path), "%s/%s", r->data_dir, relative);
        file = r->read_file(path, arena);
    }
    *size_out = file.contents ? file.size : 0;
    return *size_out ? (u8 *)file.contents : 0;
}

/* The placeholder material atlas: one flat colour layer per colour of
   terrain.tsv, one below the other, with a faint checker so zoom is
   visible. Used when materials.jpg is absent.
   Colours are ABGR in memory (R8G8B8A8_UNORM, little endian). */
internal u32 *
d3d11_placeholder_atlas(memory_arena *arena)
{
    static const u32 colors[D3D11_ATLAS_TILES] = {
        0xFF9E5A2Eu, /* 0 water: blue  */ 0xFF4E8C5Au, /* 1 plain: green */ 0xFF2E6E3Au, /* 2 forest */
        0xFF5A7D96u, /* 3 hill: tan */    0xFF8A8A8Au, /* 4 mountain: grey */ 0xFF3A6E9Eu, /* 5 road: brown */
        0xFFC8C8C8u, /* 6 snow */          0xFF7A3A9Au  /* 7 unused */ };
    u32 w = D3D11_ATLAS_TILE_SIZE, h = D3D11_ATLAS_TILES * D3D11_ATLAS_TILE_SIZE;
    u32 *rgba = PushArray(arena, (u64)w * h, u32);
    for (u32 y = 0; y < h; ++y)
    {
        for (u32 x = 0; x < w; ++x)
        {
            u32 tile = y / D3D11_ATLAS_TILE_SIZE;
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

/* A mesh of the CPU (a mesh file or a placeholder shape) as a GPU mesh. */
internal b32
d3d11_make_mesh(d3d11_renderer *r, const render_mesh_file *file, d3d11_mesh *mesh)
{
    mesh->vertices    = d3d11_make_buffer(r, file->vertices, file->vertex_count * (u32)sizeof(d3d11_mesh_vertex), D3D11_BIND_VERTEX_BUFFER, false);
    mesh->indices     = d3d11_make_buffer(r, file->indices, file->index_count * 4, D3D11_BIND_INDEX_BUFFER, false);
    mesh->index_count = file->index_count;
    return mesh->vertices && mesh->indices;
}

/* Reads and parses a mesh file of the pack (render_mesh_file_parse). It
   uses no D3D11 call, so any thread can call it. */
internal b32
d3d11_read_mesh_file(const d3d11_renderer *r, const char *relative, memory_arena *arena, render_mesh_file *file)
{
    u64 size = 0;
    u8 *data = d3d11_read_data_file(r, relative, arena, &size);
    *file = {};
    return render_mesh_file_parse(data, size, file);
}

/* Reads and decodes a PNG file of the pack. The pixels are zero if the
   file is absent or bad. It uses no D3D11 call, so any thread can call it. */
internal png_image
d3d11_read_png_file(const d3d11_renderer *r, const char *relative, memory_arena *arena)
{
    png_image image = {};
    u64 size = 0;
    u8 *data = d3d11_read_data_file(r, relative, arena, &size);
    if (data) image = png_decode(data, size, arena);
    if (image.error || !image.width) image = {};
    return image;
}

/* Reads and decodes the texture of a model: the first file of
   picture_model_texture_extensions that the pack has and that decodes. */
internal png_image
d3d11_read_model_texture(const d3d11_renderer *r, const char *name, memory_arena *arena)
{
    png_image image = {};
    for (u32 index = 0; index < PICTURE_MODEL_TEXTURE_EXTENSIONS && !image.rgba; ++index)
    {
        char texture_file[260];
        snprintf(texture_file, sizeof(texture_file), "%s.%s", name, picture_model_texture_extensions[index]);
        u64 size = 0;
        u8 *data = d3d11_read_data_file(r, texture_file, arena, &size);
        if (data) image = picture_decode(data, size, arena);
        if (image.error || !image.width) image = {};
    }
    return image;
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

/* A strip of square tiles, one below the other, as a texture array with
   the full chain of mip levels. A tile wraps without a seam, and the mips
   of a tile do not mix with the next tile. Sets the layer count, or
   returns 0 if the strip is not a whole number of tiles. */
internal ID3D11ShaderResourceView *
d3d11_make_tile_array(d3d11_renderer *r, const u32 *rgba, u32 side, u32 height, u32 *layers_out)
{
    if (!rgba || !side || height % side) return 0;
    u32 layers = height / side;
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = side; desc.Height = side; desc.MipLevels = 0; desc.ArraySize = layers;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    ID3D11Texture2D *texture = 0;
    if (FAILED(r->device->CreateTexture2D(&desc, 0, &texture))) return 0;
    texture->GetDesc(&desc);
    for (u32 layer = 0; layer < layers; ++layer)
    {
        r->context->UpdateSubresource(texture, D3D11CalcSubresource(0, layer, desc.MipLevels), 0, rgba + (u64)layer * side * side, side * 4, 0);
    }
    ID3D11ShaderResourceView *view = 0;
    if (SUCCEEDED(r->device->CreateShaderResourceView(texture, 0, &view))) r->context->GenerateMips(view);
    texture->Release();
    if (view) *layers_out = layers;
    return view;
}

/* A decoded picture as a texture. Call it on the thread of the device context. */
internal ID3D11ShaderResourceView *
d3d11_make_picture_texture(d3d11_renderer *r, const png_image *image, b32 mipmaps)
{
    if (!image->rgba) return 0;
    if (mipmaps) return d3d11_make_mipmapped_texture(r, (const u32 *)image->rgba, image->width, image->height);
    return d3d11_make_texture(r, (const u32 *)image->rgba, image->width, image->height, false, 0);
}

internal ID3D11ShaderResourceView *
d3d11_load_png_texture(d3d11_renderer *r, const char *relative, memory_arena *arena, f32 *aspect_out = 0, b32 mipmaps = false)
{
    png_image image = d3d11_read_png_file(r, relative, arena);
    if (!image.rgba) return 0;
    if (aspect_out) *aspect_out = (f32)image.height / (f32)image.width;
    return d3d11_make_picture_texture(r, &image, mipmaps);
}

/* ---- the files of the renderer ------------------------------------------ */

/* The renderer loads its files in jobs: the atlas, the settlement, one job
   for each sprite and one job for each model. A job has two halves:
   - d3d11_asset_prepare reads and decodes the files. It uses no D3D11
     call, so a worker thread can do it (win32_loader.h).
   - d3d11_asset_upload makes the GPU resources from the result. Call it
     on the thread of the device context.
   A model has two files: <name>.hxm and a texture, <name>.jpg or <name>.png.
   If one of the two is absent, the kind has no model. */
#define D3D11_ASSET_ATLAS             0
#define D3D11_ASSET_SETTLEMENT        1
#define D3D11_ASSET_FIRST_SPRITE      2
#define D3D11_ASSET_FIRST_BASE_MODEL  (D3D11_ASSET_FIRST_SPRITE + RENDER_SPRITE_COUNT)
#define D3D11_ASSET_FIRST_MESH_MODEL  (D3D11_ASSET_FIRST_BASE_MODEL + D3D11_BASE_KINDS)
#define D3D11_ASSET_COUNT             (D3D11_ASSET_FIRST_MESH_MODEL + RENDER_MODEL_COUNT * RENDER_MODEL_LEVELS)

/* The result of d3d11_asset_prepare. The pointers go into the arena of the job. */
struct d3d11_asset_data
{
    png_image        picture;   /* rgba is zero when the job has no picture */
    render_mesh_file mesh;      /* vertices is zero when the job has no mesh */
};

/* The name of the model of a model job, or zero. */
internal const char *
d3d11_asset_model_name(u32 job)
{
    if (job >= D3D11_ASSET_FIRST_MESH_MODEL && job < D3D11_ASSET_COUNT)
    {
        u32 model = job - D3D11_ASSET_FIRST_MESH_MODEL;
        return render_model_files[model / RENDER_MODEL_LEVELS][model % RENDER_MODEL_LEVELS];
    }
    if (job >= D3D11_ASSET_FIRST_BASE_MODEL && job < D3D11_ASSET_FIRST_MESH_MODEL) return render_base_model_files[job - D3D11_ASSET_FIRST_BASE_MODEL];
    return 0;
}

/* The render_model_id of a model job, or RENDER_MODEL_COUNT. */
internal u32
d3d11_asset_model_id(u32 job)
{
    if (job < D3D11_ASSET_FIRST_MESH_MODEL || job >= D3D11_ASSET_COUNT) return RENDER_MODEL_COUNT;
    return (job - D3D11_ASSET_FIRST_MESH_MODEL) / RENDER_MODEL_LEVELS;
}

/* True if a file of the pack folder exists. An empty folder has none. */
internal b32
d3d11_pack_has_file(const char *pack_dir, const char *relative)
{
    if (!pack_dir || !pack_dir[0] || !relative) return false;
    char path[520];
    snprintf(path, sizeof(path), "%s/%s", pack_dir, relative);
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

/* False if the job is a unit model that the pack must not take from the
   data folder: the pack has its own art of the class and no own model
   (render_unit_model_allowed). Other jobs are always allowed. */
internal b32
d3d11_unit_model_allowed(const d3d11_renderer *r, u32 job, const char *mesh_file)
{
    const char *art = render_unit_model_art_file(d3d11_asset_model_id(job));
    if (!art) return true;
    return render_unit_model_allowed(d3d11_pack_has_file(r->pack_dir, art), d3d11_pack_has_file(r->pack_dir, mesh_file));
}

internal void
d3d11_asset_prepare(const d3d11_renderer *r, u32 job, memory_arena *arena, d3d11_asset_data *data)
{
    *data = {};
    if (job == D3D11_ASSET_ATLAS)
    {
        u64 size = 0;
        u8 *file = d3d11_read_data_file(r, "materials.jpg", arena, &size);
        if (file)
        {
            jpeg_image image = jpeg_decode_full(file, size, arena);
            if (!image.error && image.rgba) { data->picture.width = image.width; data->picture.height = image.height; data->picture.rgba = image.rgba; }
        }
    }
    else if (job == D3D11_ASSET_SETTLEMENT)
    {
        d3d11_read_mesh_file(r, "settlement.hxm", arena, &data->mesh);
    }
    else if (job < D3D11_ASSET_FIRST_BASE_MODEL)
    {
        /* The sprite: the art first, then the placeholder picture. A base
           has no placeholder picture (docs/ASSETS.md, "Base art"). */
        u32 sprite_id = job - D3D11_ASSET_FIRST_SPRITE;
        if (render_sprite_art_files[sprite_id]) data->picture = d3d11_read_png_file(r, render_sprite_art_files[sprite_id], arena);
        if (!data->picture.rgba && sprite_id < RENDER_SPRITE_FIRST_BASE) data->picture = d3d11_read_png_file(r, render_sprite_files[sprite_id], arena);
    }
    else if (const char *name = d3d11_asset_model_name(job))
    {
        char mesh_file[260];
        snprintf(mesh_file, sizeof(mesh_file), "%s.hxm", name);
        if (!d3d11_unit_model_allowed(r, job, mesh_file)) return;
        if (d3d11_read_mesh_file(r, mesh_file, arena, &data->mesh)) data->picture = d3d11_read_model_texture(r, name, arena);
        if (!data->picture.rgba) data->mesh = {};
    }
}

/* The model of a model job, or zero. */
internal d3d11_base_model *
d3d11_asset_model(d3d11_renderer *r, u32 job)
{
    if (job >= D3D11_ASSET_FIRST_MESH_MODEL && job < D3D11_ASSET_COUNT)
    {
        u32 model = job - D3D11_ASSET_FIRST_MESH_MODEL;
        return r->mesh_models[model / RENDER_MODEL_LEVELS] + model % RENDER_MODEL_LEVELS;
    }
    if (job >= D3D11_ASSET_FIRST_BASE_MODEL && job < D3D11_ASSET_FIRST_MESH_MODEL) return r->base_models + (job - D3D11_ASSET_FIRST_BASE_MODEL);
    return 0;
}

/* Makes the GPU resources of a job. If the files are absent, the atlas,
   the settlement and the sprites of the units take a placeholder. The
   scratch arena holds a placeholder only during the call. */
internal void
d3d11_asset_upload(d3d11_renderer *r, u32 job, const d3d11_asset_data *data, memory_arena *scratch)
{
    u64 scratch_used = scratch->used;
    if (job == D3D11_ASSET_ATLAS)
    {
        r->atlas_view = d3d11_make_tile_array(r, (const u32 *)data->picture.rgba, data->picture.width, data->picture.height, &r->atlas_layers);
        if (!r->atlas_view)
        {
            u32 *atlas = d3d11_placeholder_atlas(scratch);
            r->atlas_view = d3d11_make_tile_array(r, atlas, D3D11_ATLAS_TILE_SIZE, D3D11_ATLAS_TILES * D3D11_ATLAS_TILE_SIZE, &r->atlas_layers);
        }
    }
    else if (job == D3D11_ASSET_SETTLEMENT)
    {
        /* mesh 0: the settlement, from settlement.hxm or the placeholder house */
        if (!data->mesh.vertices || !d3d11_make_mesh(r, &data->mesh, r->meshes + 0))
        {
            d3d11_placeholder_mesh(r, RENDER_MESH_SETTLEMENT, scratch, r->meshes + 0);
        }
    }
    else if (job < D3D11_ASSET_FIRST_BASE_MODEL)
    {
        u32 sprite_id = job - D3D11_ASSET_FIRST_SPRITE;
        if (sprite_id < D3D11_MAX_SPRITES)
        {
            d3d11_sprite *sprite_slot = r->sprites + sprite_id;
            sprite_slot->view   = d3d11_make_picture_texture(r, &data->picture, false);
            sprite_slot->aspect = data->picture.rgba ? (f32)data->picture.height / (f32)data->picture.width : 1.0f;
            if (!sprite_slot->view && sprite_id < RENDER_SPRITE_FIRST_BASE)
            {
                u32 *sprite = d3d11_placeholder_sprite(scratch);
                sprite_slot->view = d3d11_make_texture(r, sprite, D3D11_SPRITE_SIZE, D3D11_SPRITE_SIZE, false, 0);
            }
        }
    }
    else if (d3d11_base_model *model = d3d11_asset_model(r, job))
    {
        *model = {};
        if (data->mesh.vertices && data->picture.rgba && d3d11_make_mesh(r, &data->mesh, &model->mesh))
        {
            model->texture = d3d11_make_picture_texture(r, &data->picture, true);
        }
        if (!model->texture)
        {
            if (model->mesh.vertices) model->mesh.vertices->Release();
            if (model->mesh.indices) model->mesh.indices->Release();
            *model = {};
        }
    }
    scratch->used = scratch_used;
}

/* ---- init --------------------------------------------------------------- */

/* The first half of the init: the shaders, the states, the buffers and the
   placeholder shapes. It reads no file. The jobs of the files come after
   it (d3d11_asset_prepare and d3d11_asset_upload), then d3d11_renderer_finish. */
internal b32
d3d11_renderer_start(d3d11_renderer *r, ID3D11Device *device, ID3D11DeviceContext *context,
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

    /* Mesh 0 is the settlement: a job of the files makes it. The other
       meshes are the placeholder shapes of the facility slots. */
    r->mesh_count = 1;
    for (u32 mesh_id = 1; mesh_id < RENDER_MESH_COUNT && mesh_id < D3D11_MAX_MESHES; ++mesh_id)
    {
        u64 scratch_used = scratch->used;
        b32 made = d3d11_placeholder_mesh(r, mesh_id, scratch, r->meshes + mesh_id);
        scratch->used = scratch_used;
        if (!made) break;
        r->mesh_count = mesh_id + 1;
    }

    /* The sprites: one picture for each render_sprite. The jobs of the
       files load them. Without art or a placeholder picture, a unit gets
       the placeholder figure (docs/ASSETS.md, "Unit sprites"). A base has
       no placeholder picture: without its art, the view stays 0 and the
       renderer draws the mesh of the base (docs/ASSETS.md, "Base art"). */
    r->sprite_count = RENDER_SPRITE_COUNT < D3D11_MAX_SPRITES ? RENDER_SPRITE_COUNT : D3D11_MAX_SPRITES;
    for (u32 sprite_id = 0; sprite_id < r->sprite_count; ++sprite_id) r->sprites[sprite_id].aspect = 1.0f;

    r->instance_capacity = RENDER_MAX_MESH_INSTANCES > RENDER_MAX_BILLBOARDS ? RENDER_MAX_MESH_INSTANCES : RENDER_MAX_BILLBOARDS;
    r->instances = d3d11_make_buffer(r, 0, r->instance_capacity * (u32)sizeof(d3d11_mesh_instance), D3D11_BIND_VERTEX_BUFFER, true);

    f32 quad[] = { -0.5f, 0.0f, 0.0f, 1.0f,   0.5f, 0.0f, 1.0f, 1.0f,   -0.5f, 1.0f, 0.0f, 0.0f,
                    0.5f, 0.0f, 1.0f, 1.0f,   0.5f, 1.0f, 1.0f, 0.0f,   -0.5f, 1.0f, 0.0f, 0.0f };
    r->billboard_quad = d3d11_make_buffer(r, quad, sizeof(quad), D3D11_BIND_VERTEX_BUFFER, false);
    return true;
}

/* The last half of the init, after the upload of each job of the files.
   The renderer draws only when it is ready. */
internal b32
d3d11_renderer_finish(d3d11_renderer *r)
{
    r->ready = r->constants && r->atlas_view && r->meshes[0].vertices && r->sprites[0].view && r->instances && r->billboard_quad;
    if (!r->ready) snprintf(r->last_error, sizeof(r->last_error), "a D3D11 resource could not be created");
    return r->ready;
}

/* The full init on one thread: the start, each job of the files, then the
   finish. The arena holds the files of one job at a time. */
internal b32
d3d11_renderer_init(d3d11_renderer *r, ID3D11Device *device, ID3D11DeviceContext *context,
                    platform_read_entire_file *read_file, const char *data_dir, memory_arena *scratch)
{
    if (!d3d11_renderer_start(r, device, context, read_file, data_dir, scratch)) return false;
    for (u32 job = 0; job < D3D11_ASSET_COUNT; ++job)
    {
        u64 scratch_used = scratch->used;
        d3d11_asset_data data;
        d3d11_asset_prepare(r, job, scratch, &data);
        d3d11_asset_upload(r, job, &data, scratch);
        scratch->used = scratch_used;
    }
    return d3d11_renderer_finish(r);
}

internal void
d3d11_release(IUnknown **p)
{
    if (*p) { (*p)->Release(); *p = 0; }
}

/* ---- the art of a pack -------------------------------------------------- */

/* The files of a job, relative to the data folder. Returns their count. */
internal u32
d3d11_asset_files(u32 job, char files[2 + PICTURE_MODEL_TEXTURE_EXTENSIONS][260])
{
    u32 count = 0;
    if (job == D3D11_ASSET_ATLAS) snprintf(files[count++], 260, "materials.jpg");
    else if (job == D3D11_ASSET_SETTLEMENT) snprintf(files[count++], 260, "settlement.hxm");
    else if (job < D3D11_ASSET_FIRST_BASE_MODEL)
    {
        u32 sprite_id = job - D3D11_ASSET_FIRST_SPRITE;
        if (render_sprite_art_files[sprite_id]) snprintf(files[count++], 260, "%s", render_sprite_art_files[sprite_id]);
        if (sprite_id < RENDER_SPRITE_FIRST_BASE && render_sprite_files[sprite_id]) snprintf(files[count++], 260, "%s", render_sprite_files[sprite_id]);
    }
    else if (const char *name = d3d11_asset_model_name(job))
    {
        snprintf(files[count++], 260, "%s.hxm", name);
        for (u32 index = 0; index < PICTURE_MODEL_TEXTURE_EXTENSIONS; ++index)
        {
            snprintf(files[count++], 260, "%s.%s", name, picture_model_texture_extensions[index]);
        }
        /* The art of the class decides if a unit model loads (d3d11_unit_model_allowed). */
        if (const char *art = render_unit_model_art_file(d3d11_asset_model_id(job))) snprintf(files[count++], 260, "%s", art);
    }
    return count;
}

/* True if the folder has a file of the job. An empty folder has none. */
internal b32
d3d11_asset_in_pack(const char *pack_dir, u32 job)
{
    if (!pack_dir || !pack_dir[0]) return false;
    char files[2 + PICTURE_MODEL_TEXTURE_EXTENSIONS][260];
    u32 count = d3d11_asset_files(job, files);
    for (u32 index = 0; index < count; ++index)
    {
        char path[520];
        snprintf(path, sizeof(path), "%s/%s", pack_dir, files[index]);
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) return true;
    }
    return false;
}

internal void
d3d11_release_mesh(d3d11_mesh *mesh)
{
    d3d11_release((IUnknown **)&mesh->vertices);
    d3d11_release((IUnknown **)&mesh->indices);
    mesh->index_count = 0;
}

/* Frees the GPU resources of a job before the job loads again. */
internal void
d3d11_asset_release(d3d11_renderer *r, u32 job)
{
    if (job == D3D11_ASSET_ATLAS) d3d11_release((IUnknown **)&r->atlas_view);
    else if (job == D3D11_ASSET_SETTLEMENT) d3d11_release_mesh(r->meshes + 0);
    else if (job < D3D11_ASSET_FIRST_BASE_MODEL)
    {
        u32 sprite_id = job - D3D11_ASSET_FIRST_SPRITE;
        if (sprite_id < D3D11_MAX_SPRITES) d3d11_release((IUnknown **)&r->sprites[sprite_id].view);
    }
    else if (d3d11_base_model *model = d3d11_asset_model(r, job))
    {
        d3d11_release_mesh(&model->mesh);
        d3d11_release((IUnknown **)&model->texture);
    }
}

/* Uses the art of another pack folder, or "" for the art of data_dir
   alone. Only a job with a file in the old folder or in the new folder
   loads again. Thus a pack without art costs no time. Returns the count of
   the jobs that loaded again. Call it on the thread of the device context. */
internal u32
d3d11_renderer_use_pack(d3d11_renderer *r, const char *pack_dir, memory_arena *scratch)
{
    if (!pack_dir) pack_dir = "";
    if (strcmp(r->pack_dir, pack_dir) == 0) return 0;
    char old_dir[260];
    snprintf(old_dir, sizeof(old_dir), "%s", r->pack_dir);
    snprintf(r->pack_dir, sizeof(r->pack_dir), "%s", pack_dir);
    u32 loaded = 0;
    for (u32 job = 0; job < D3D11_ASSET_COUNT; ++job)
    {
        if (!d3d11_asset_in_pack(old_dir, job) && !d3d11_asset_in_pack(r->pack_dir, job)) continue;
        u64 scratch_used = scratch->used;
        d3d11_asset_data data;
        d3d11_asset_prepare(r, job, scratch, &data);
        d3d11_asset_release(r, job);
        d3d11_asset_upload(r, job, &data, scratch);
        scratch->used = scratch_used;
        loaded++;
    }
    return loaded;
}

internal void d3d11_terrain_job_end(d3d11_terrain_job *job);

internal void
d3d11_renderer_destroy(d3d11_renderer *r)
{
    d3d11_terrain_job_end(&r->terrain_job);
    d3d11_release((IUnknown **)&r->terrain_vs); d3d11_release((IUnknown **)&r->terrain_ps); d3d11_release((IUnknown **)&r->terrain_layout);
    d3d11_release((IUnknown **)&r->mesh_vs); d3d11_release((IUnknown **)&r->mesh_ps); d3d11_release((IUnknown **)&r->mesh_layout);
    d3d11_release((IUnknown **)&r->model_ps);
    for (u32 kind = 0; kind < D3D11_BASE_KINDS; ++kind)
    {
        d3d11_base_model *model = r->base_models + kind;
        d3d11_release((IUnknown **)&model->mesh.vertices); d3d11_release((IUnknown **)&model->mesh.indices); d3d11_release((IUnknown **)&model->texture);
    }
    for (u32 model_index = 0; model_index < RENDER_MODEL_COUNT * RENDER_MODEL_LEVELS; ++model_index)
    {
        d3d11_base_model *model = r->mesh_models[0] + model_index;
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

/* The grid lines: the six top edges of each cell. */
internal u32 *
d3d11_terrain_lines(const render_terrain *terrain, memory_arena *arena, u32 *count_out)
{
    u32 cells = terrain->width * terrain->height;
    u32 *lines = PushArray(arena, cells * 12, u32);
    u32 count = 0;
    for (u32 cell = 0; cell < cells; ++cell)
    {
        if (terrain->grid && !terrain->grid[cell]) continue;   /* M11: no grid on cliffs (ui/main-map.md) */
        u32 base = cell * TERRAIN_VERTS_PER_CELL;
        for (u32 edge = 0; edge < 6; ++edge) { lines[count++] = base + 1 + edge; lines[count++] = base + 1 + (edge + 1) % 6; }
    }
    *count_out = count;
    return lines;
}

/* Makes the buffers of a built terrain: the cells, then the grid lines. */
internal void
d3d11_terrain_buffers(d3d11_renderer *r, render_terrain *terrain, memory_arena *arena, d3d11_terrain_job *out)
{
    terrain_mesh mesh = terrain_mesh_build(terrain, arena);
    u32 line_count = 0;
    u32 *lines = d3d11_terrain_lines(terrain, arena, &line_count);
    out->vertices    = d3d11_make_buffer(r, mesh.vertices, mesh.vertex_count * (u32)sizeof(terrain_vertex), D3D11_BIND_VERTEX_BUFFER, false);
    out->indices     = d3d11_make_buffer(r, mesh.indices, mesh.index_count * 4, D3D11_BIND_INDEX_BUFFER, false);
    out->index_count = mesh.index_count;
    out->grid        = d3d11_make_buffer(r, lines, line_count * 4, D3D11_BIND_INDEX_BUFFER, false);
    out->grid_count  = line_count;
}

/* Puts the buffers of a built terrain in place of the old ones. The
   renderer takes the buffers of the job. */
internal void
d3d11_terrain_apply(d3d11_renderer *r, const render_terrain *terrain, d3d11_terrain_job *built)
{
    d3d11_release((IUnknown **)&r->terrain_vertices);
    d3d11_release((IUnknown **)&r->terrain_indices);
    d3d11_release((IUnknown **)&r->grid_indices);
    r->terrain_vertices    = built->vertices;
    r->terrain_indices     = built->indices;
    r->terrain_index_count = built->index_count;
    r->grid_indices        = built->grid;
    r->grid_index_count    = built->grid_count;
    built->vertices = built->indices = built->grid = 0;
    r->terrain_version = terrain->version;
    r->terrain_cells   = terrain->width * terrain->height;
    if (!r->terrain_cells) return;

    d3d11_release((IUnknown **)&r->overlay_view);
    d3d11_release((IUnknown **)&r->overlay_texture);
    r->overlay_view = d3d11_make_texture(r, 0, terrain->width, terrain->height, true, &r->overlay_texture);
    r->overlay_width = terrain->width; r->overlay_height = terrain->height;
}

/* The build of a terrain on a worker thread. The job has a copy of the
   cells, because the arrays of the command live only for one frame. */
internal DWORD WINAPI
d3d11_terrain_worker(LPVOID parameter)
{
    d3d11_terrain_job *job = (d3d11_terrain_job *)parameter;
    d3d11_terrain_buffers(job->renderer, &job->input, &job->arena, job);
    InterlockedExchange(&job->done, 1);
    return 0;
}

/* Starts the build of a terrain on a worker thread. False if the memory
   or the thread is not available. */
internal b32
d3d11_terrain_job_start(d3d11_renderer *r, d3d11_terrain_job *job, const render_terrain *terrain)
{
    u64 cells = (u64)terrain->width * terrain->height;
    u64 size = cells * (4 + TERRAIN_VERTS_PER_CELL * sizeof(terrain_vertex) + TERRAIN_TRIS_PER_CELL * 3 * 4 + 12 * 4) + Megabytes(1);
    *job = {};
    job->memory = (u8 *)VirtualAlloc(0, (SIZE_T)size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!job->memory) return false;
    arena_init(&job->arena, job->memory, size);
    job->renderer = r;
    job->input = *terrain;
    u8 *kind  = PushArray(&job->arena, cells, u8);
    u8 *level = PushArray(&job->arena, cells, u8);
    memcpy(kind, terrain->kind, (size_t)cells);
    memcpy(level, terrain->level, (size_t)cells);
    job->input.kind  = kind;
    job->input.level = level;
    if (terrain->art_kind)
    {
        u8 *art_kind = PushArray(&job->arena, cells, u8);
        memcpy(art_kind, terrain->art_kind, (size_t)cells);
        job->input.art_kind = art_kind;
    }
    if (terrain->grid)
    {
        u8 *grid = PushArray(&job->arena, cells, u8);
        memcpy(grid, terrain->grid, (size_t)cells);
        job->input.grid = grid;
    }
    job->thread = CreateThread(0, 0, d3d11_terrain_worker, job, 0, 0);
    if (job->thread) return true;
    VirtualFree(job->memory, 0, MEM_RELEASE);
    *job = {};
    return false;
}

/* Waits for the worker of the job and frees its memory. */
internal void
d3d11_terrain_job_end(d3d11_terrain_job *job)
{
    if (job->thread)
    {
        WaitForSingleObject(job->thread, INFINITE);
        CloseHandle(job->thread);
    }
    if (job->memory) VirtualFree(job->memory, 0, MEM_RELEASE);
    d3d11_release((IUnknown **)&job->vertices);
    d3d11_release((IUnknown **)&job->indices);
    d3d11_release((IUnknown **)&job->grid);
    *job = {};
}

/* Rebuilds the terrain buffers from the command's terrain when its version
   changed. With terrain_on_worker, a worker thread builds the mesh, and the
   old terrain stays on the screen until the new one is ready. The first map
   of han_196 takes approximately 1 s to build, so the window does not
   freeze. Without it, the build is on this thread. */
internal void
d3d11_ensure_terrain(d3d11_renderer *r, render_terrain *terrain, memory_arena *scratch)
{
    d3d11_terrain_job *job = &r->terrain_job;
    if (job->thread)
    {
        if (!job->done) return;
        WaitForSingleObject(job->thread, INFINITE);
        d3d11_terrain_apply(r, &job->input, job);
        d3d11_terrain_job_end(job);
    }
    u32 cells = terrain->width * terrain->height;
    if (r->terrain_version == terrain->version && r->terrain_cells == cells && (r->terrain_vertices || !cells)) return;
    if (cells && r->terrain_on_worker && terrain->kind && terrain->level && d3d11_terrain_job_start(r, job, terrain)) return;

    d3d11_terrain_job built = {};
    if (cells) d3d11_terrain_buffers(r, terrain, scratch, &built);
    d3d11_terrain_apply(r, terrain, &built);
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
   the model of its kind. Another instance uses the model of its kind and
   its level, or of the next lower level with a model, if the camera is
   near (render_shape_models_on). A kind without a model uses the model of
   its shape. */
internal d3d11_base_model *
d3d11_level_model_of(d3d11_renderer *r, u32 model_id, u32 level)
{
    if (model_id >= RENDER_MODEL_COUNT) return 0;
    for (s32 column = (s32)render_model_level_index(level); column >= 0; --column)
    {
        d3d11_base_model *model = r->mesh_models[model_id] + column;
        if (model->texture) return model;
    }
    return 0;
}

internal d3d11_base_model *
d3d11_model_of(d3d11_renderer *r, render_mesh_instance *m, const camera *cam)
{
    if (m->art_sprite >= RENDER_SPRITE_FIRST_BASE && m->art_sprite < RENDER_SPRITE_COUNT)
    {
        d3d11_base_model *model = r->base_models + (m->art_sprite - RENDER_SPRITE_FIRST_BASE);
        return model->texture ? model : 0;
    }
    if (m->mesh_id == RENDER_MESH_UNIT_FIGURE) return d3d11_level_model_of(r, m->model, 0);   /* render_place_units checked the camera */
    if (m->mesh_id >= RENDER_MESH_COUNT || !render_shape_models_on(cam)) return 0;
    d3d11_base_model *model = d3d11_level_model_of(r, m->model, m->level);
    return model ? model : d3d11_level_model_of(r, m->mesh_id, m->level);
}

/* One instanced draw of a model, with its texture, for each instance
   that d3d11_model_of gives to that model and that is in the view. The
   model stays fixed when the view turns. */
internal void
d3d11_draw_model(d3d11_renderer *r, render_commands *rc, m4 view_projection, d3d11_base_model *model, d3d11_mesh_instance *batch)
{
    if (!model->texture) return;
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
    fc.atlas_tiles = (f32)r->atlas_layers;
    fc.terrain_state[0] = rc->terrain_season; fc.terrain_state[1] = rc->terrain_ripple;
    fc.mode = 0.0f;
    d3d11_upload(r, r->constants, &fc, sizeof(fc), 0);
    ctx->VSSetConstantBuffers(0, 1, &r->constants);
    ctx->PSSetConstantBuffers(0, 1, &r->constants);

    /* overlay colours */
    if (r->overlay_texture && r->overlay_width * r->overlay_height)
    {
        u32 cells = r->overlay_width * r->overlay_height;
        u32 *colors = PushArray(scratch, cells, u32);
        memset(colors, 0, (size_t)cells * 4);
        /* While a worker builds a terrain of another size, the old terrain shows without overlay. */
        b32 same_size = rc->terrain.width == r->overlay_width && rc->terrain.height == r->overlay_height;
        if (rc->overlay && same_size) memcpy(colors, rc->overlay, (u64)cells * 4);
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

    /* The units: the figures of each class with a model, else the
       billboard (render_place_units). A unit model loads only at level 1. */
    b32 unit_ready[RENDER_MODEL_COUNT] = {};
    for (u32 model_id = RENDER_MODEL_UNIT_SWORD; model_id < RENDER_MODEL_COUNT; ++model_id) unit_ready[model_id] = r->mesh_models[model_id][0].texture != 0;
    render_place_units(rc, unit_ready);

    /* One batch of instances for each draw of a mesh or a model, in turn. */
    d3d11_mesh_instance *batch = PushArray(scratch, r->instance_capacity, d3d11_mesh_instance);

    /* meshes: one instanced draw per mesh id */
    for (u32 mesh_id = 0; mesh_id < r->mesh_count; ++mesh_id)
    {
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
    for (u32 kind = 0; kind < D3D11_BASE_KINDS; ++kind) d3d11_draw_model(r, rc, fc.view_projection, r->base_models + kind, batch);
    for (u32 model_index = 0; model_index < RENDER_MODEL_COUNT * RENDER_MODEL_LEVELS; ++model_index) d3d11_draw_model(r, rc, fc.view_projection, r->mesh_models[0] + model_index, batch);

    /* billboards: one instanced draw per sprite id, alpha tested, drawn last.
       The quad winds counter-clockwise on the screen, so the back-face cull
       of the meshes drops it: the pass culls nothing. */
    ctx->OMSetBlendState(r->blend_alpha, 0, 0xFFFFFFFF);
    ctx->RSSetState(r->rasterizer_no_cull);
    for (u32 sprite_id = 0; sprite_id < r->sprite_count; ++sprite_id)
    {
        d3d11_billboard_instance *cards = PushArray(scratch, r->instance_capacity, d3d11_billboard_instance);
        u32 n = 0;
        for (u32 i = 0; i < rc->billboard_count && n < r->instance_capacity; ++i)
        {
            render_billboard *b = rc->billboards + i;
            if (b->sprite_id != sprite_id) continue;
            cards[n].position[0] = b->position.x; cards[n].position[1] = b->position.y; cards[n].position[2] = b->position.z;
            cards[n].width = b->width; cards[n].height = b->height; cards[n].tint = b->tint;
            n++;
        }
        /* The art of a base stands in place of its mesh, on a card that faces the camera. */
        for (u32 i = 0; i < rc->mesh_instance_count && n < r->instance_capacity && sprite_id >= RENDER_SPRITE_FIRST_BASE; ++i)
        {
            render_mesh_instance *m = rc->mesh_instances + i;
            if (m->art_sprite != sprite_id || !d3d11_has_art(r, m) || d3d11_model_of(r, m, &rc->cam)) continue;
            render_billboard card = render_base_billboard(m, &rc->cam, r->sprites[sprite_id].aspect);
            cards[n].position[0] = card.position.x; cards[n].position[1] = card.position.y; cards[n].position[2] = card.position.z;
            cards[n].width = card.width; cards[n].height = card.height; cards[n].tint = card.tint;
            n++;
        }
        if (!n) continue;
        f32 mode = sprite_id >= RENDER_SPRITE_FIRST_BASE ? 2.0f : 0.0f;
        if (fc.mode != mode)
        {
            fc.mode = mode;
            d3d11_upload(r, r->constants, &fc, sizeof(fc), 0);
        }
        d3d11_upload(r, r->instances, cards, n * (u32)sizeof(d3d11_billboard_instance), 0);
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
