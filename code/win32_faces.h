#ifndef WIN32_FACES_H
#define WIN32_FACES_H
/* The generated faces for the platform layer (docs/ASSETS.md). The UI asks
   for the face of an officer with its id and a packed description
   (render/faces.h). The platform paints the face one time and keeps its
   D3D11 texture by officer id. If the description of an officer changes,
   for example when the officer gets old, the platform paints it again.
   Include it after win32_window.h. */

#include "hexdom.h"               /* PLATFORM_FACE_TEXTURE */
#include "render/faces.h"

#define WIN32_FACES_PER_FRAME 64   /* a paint and a texture take approximately 0.1 ms at /Od */

struct win32_faces
{
    ID3D11Device             *device;
    ID3D11ShaderResourceView *textures[FACE_SLOTS];
    u32                       keys[FACE_SLOTS];
    u32                       made_this_frame;
    u8                        pixels[FACE_WIDTH * FACE_HEIGHT * 4];
};

global_variable win32_faces g_win32_faces;

/* An immutable RGBA8 texture from pixels of width * height * 4 bytes. Zero on failure. */
internal ID3D11ShaderResourceView *
win32_make_rgba_texture(ID3D11Device *device, u32 width, u32 height, const u8 *rgba)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width            = width;
    desc.Height           = height;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initial = {};
    initial.pSysMem     = rgba;
    initial.SysMemPitch = width * 4;
    ID3D11Texture2D *texture = 0;
    ID3D11ShaderResourceView *view = 0;
    if (FAILED(device->CreateTexture2D(&desc, &initial, &texture)) ||
        FAILED(device->CreateShaderResourceView(texture, 0, &view))) view = 0;
    if (texture) texture->Release();
    return view;
}

internal void
win32_faces_init(win32_faces *faces, ID3D11Device *device)
{
    memset(faces, 0, sizeof(*faces));
    faces->device = device;
}

internal void
win32_faces_begin_frame(win32_faces *faces)
{
    faces->made_this_frame = 0;
}

/* The texture of the face of an officer. If the texture is not ready in
   this frame, the result is the old face of the officer, or zero. */
internal u64
win32_faces_texture(win32_faces *faces, u32 officer_id, u32 key)
{
    if (!faces->device || officer_id >= FACE_SLOTS || !(key & FACE_KEY_VALID)) return 0;
    if (faces->textures[officer_id] && faces->keys[officer_id] == key) return (u64)faces->textures[officer_id];
    if (faces->made_this_frame >= WIN32_FACES_PER_FRAME) return (u64)faces->textures[officer_id];
    faces->made_this_frame += 1;
    if (faces->textures[officer_id]) faces->textures[officer_id]->Release();
    face_description face = face_key_unpack(key);
    face_paint(&face, faces->pixels);
    faces->textures[officer_id] = win32_make_rgba_texture(faces->device, FACE_WIDTH, FACE_HEIGHT, faces->pixels);
    faces->keys[officer_id]     = key;
    return (u64)faces->textures[officer_id];
}

internal PLATFORM_FACE_TEXTURE(win32_face_texture)
{
    return win32_faces_texture(&g_win32_faces, officer_id, key);
}

internal void
win32_faces_close(win32_faces *faces)
{
    for (u32 slot = 0; slot < FACE_SLOTS; ++slot)
    {
        if (faces->textures[slot]) faces->textures[slot]->Release();
    }
    memset(faces, 0, sizeof(*faces));
}

#endif /* WIN32_FACES_H */
