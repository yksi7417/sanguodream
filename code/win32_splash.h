#ifndef WIN32_SPLASH_H
#define WIN32_SPLASH_H
/* The splash of the load: the splash art and a progress bar, drawn with
   D3D11 alone. While the worker threads load the game (win32_loader.h),
   ImGui has no font yet and the main thread makes no ImGui call. So this
   file draws the first frames of the game without ImGui. After the load,
   the UI draws the same picture with a full bar (ui_splash.cpp).

   The window shows the bar from the first frame. The picture fades in
   after a worker decodes it. The bar sits at HEXDOM_SPLASH_BAR_* of the
   picture, on the track that the picture has (misc/make_splash.py). */

#include "win32_window.h"

#define WIN32_SPLASH_MAX_QUADS     4
#define WIN32_SPLASH_FADE_SECONDS  0.25f
#define WIN32_SPLASH_BAR_SPEED     4.0f    /* the shown bar closes this fraction of the gap each second */

struct win32_splash_vertex
{
    f32 position[2];   /* clip space */
    f32 uv[2];
    u32 colour;        /* RGBA bytes */
};

struct win32_splash
{
    ID3D11VertexShader       *vertex_shader;
    ID3D11PixelShader        *pixel_shader;
    ID3D11InputLayout        *layout;
    ID3D11Buffer             *vertices;
    ID3D11SamplerState       *sampler;
    ID3D11BlendState         *blend;
    ID3D11ShaderResourceView *white;     /* a 1 x 1 texture for the plain quads */
    ID3D11ShaderResourceView *picture;   /* zero until a worker decodes the art; the caller owns it */
    f32                       aspect;    /* the height of the picture over its width */
    f32                       picture_seconds;
    f32                       shown_progress;
    b32                       ready;
};

static const char win32_splash_shader_source[] =
"Texture2D picture : register(t0); SamplerState linear_sampler : register(s0);\n"
"struct VertexIn { float2 position : POSITION; float2 uv : TEXCOORD0; float4 colour : COLOR0; };\n"
"struct VertexOut { float4 clip : SV_POSITION; float2 uv : TEXCOORD0; float4 colour : COLOR0; };\n"
"VertexOut splash_vs(VertexIn i) { VertexOut o; o.clip = float4(i.position, 0.0, 1.0); o.uv = i.uv; o.colour = i.colour; return o; }\n"
"float4 splash_ps(VertexOut i) : SV_TARGET { return picture.Sample(linear_sampler, i.uv) * i.colour; }\n";

internal b32
win32_splash_compile(const char *entry, const char *target, ID3DBlob **blob)
{
    ID3DBlob *errors = 0;
    HRESULT result = D3DCompile(win32_splash_shader_source, sizeof(win32_splash_shader_source) - 1, "splash.hlsl", 0, 0,
                                entry, target, D3DCOMPILE_ENABLE_STRICTNESS, 0, blob, &errors);
    if (errors) errors->Release();
    return SUCCEEDED(result);
}

/* Makes the shaders and the states. If one part fails, the splash draws
   only the clear colour. */
internal void
win32_splash_start(win32_splash *splash, ID3D11Device *device)
{
    *splash = {};
    ID3DBlob *vertex_blob = 0, *pixel_blob = 0;
    if (!win32_splash_compile("splash_vs", "vs_4_0", &vertex_blob)) return;
    if (!win32_splash_compile("splash_ps", "ps_4_0", &pixel_blob)) { vertex_blob->Release(); return; }
    device->CreateVertexShader(vertex_blob->GetBufferPointer(), vertex_blob->GetBufferSize(), 0, &splash->vertex_shader);
    device->CreatePixelShader(pixel_blob->GetBufferPointer(), pixel_blob->GetBufferSize(), 0, &splash->pixel_shader);
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 8,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
    device->CreateInputLayout(layout, 3, vertex_blob->GetBufferPointer(), vertex_blob->GetBufferSize(), &splash->layout);
    vertex_blob->Release();
    pixel_blob->Release();

    D3D11_BUFFER_DESC buffer = {};
    buffer.ByteWidth      = WIN32_SPLASH_MAX_QUADS * 6 * sizeof(win32_splash_vertex);
    buffer.Usage          = D3D11_USAGE_DYNAMIC;
    buffer.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    buffer.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&buffer, 0, &splash->vertices);

    D3D11_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    device->CreateSamplerState(&sampler, &splash->sampler);

    D3D11_BLEND_DESC blend = {};
    blend.RenderTarget[0].BlendEnable = TRUE;
    blend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device->CreateBlendState(&blend, &splash->blend);

    u32 white = 0xFFFFFFFFu;
    D3D11_TEXTURE2D_DESC texture = {};
    texture.Width = 1; texture.Height = 1; texture.MipLevels = 1; texture.ArraySize = 1;
    texture.Format = DXGI_FORMAT_R8G8B8A8_UNORM; texture.SampleDesc.Count = 1;
    texture.Usage = D3D11_USAGE_IMMUTABLE; texture.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initial = {};
    initial.pSysMem = &white; initial.SysMemPitch = 4;
    ID3D11Texture2D *white_texture = 0;
    if (SUCCEEDED(device->CreateTexture2D(&texture, &initial, &white_texture)))
    {
        device->CreateShaderResourceView(white_texture, 0, &splash->white);
        white_texture->Release();
    }
    splash->ready = splash->vertex_shader && splash->pixel_shader && splash->layout && splash->vertices &&
                    splash->sampler && splash->blend && splash->white;
}

/* Releases the shaders and the states. The picture stays: the UI shows it
   after the load. */
internal void
win32_splash_stop(win32_splash *splash)
{
    IUnknown *parts[] = { splash->vertex_shader, splash->pixel_shader, splash->layout, splash->vertices,
                          splash->sampler, splash->blend, splash->white };
    for (u32 index = 0; index < ArrayCount(parts); ++index)
    {
        if (parts[index]) parts[index]->Release();
    }
    *splash = {};
}

/* Writes one quad of two triangles. The rect is in pixels of the window. */
internal void
win32_splash_quad(win32_splash_vertex *out, f32 width, f32 height, f32 left, f32 top, f32 right, f32 bottom, u32 colour)
{
    f32 x0 = left / width * 2.0f - 1.0f, x1 = right / width * 2.0f - 1.0f;
    f32 y0 = 1.0f - top / height * 2.0f, y1 = 1.0f - bottom / height * 2.0f;
    win32_splash_vertex corners[4] = {
        { { x0, y0 }, { 0.0f, 0.0f }, colour }, { { x1, y0 }, { 1.0f, 0.0f }, colour },
        { { x0, y1 }, { 0.0f, 1.0f }, colour }, { { x1, y1 }, { 1.0f, 1.0f }, colour } };
    out[0] = corners[0]; out[1] = corners[1]; out[2] = corners[2];
    out[3] = corners[1]; out[4] = corners[3]; out[5] = corners[2];
}

internal u32
win32_splash_colour(u32 red, u32 green, u32 blue, f32 alpha)
{
    f32 clamped = Minimum(Maximum(alpha, 0.0f), 1.0f);
    u32 alpha_byte = (u32)(clamped * 255.0f + 0.5f);
    return red | (green << 8) | (blue << 16) | (alpha_byte << 24);
}

/* Draws one frame: the picture, the track and the fill of the bar.
   progress is from 0 to 1. The caller presents the frame. */
internal void
win32_splash_draw(win32_splash *splash, win32_window *window, f32 progress, f32 dt_seconds)
{
    win32_apply_resize(window);
    if (!window->render_target) return;
    RECT client;
    GetClientRect(window->handle, &client);
    f32 width  = (f32)Maximum(client.right - client.left, 1);
    f32 height = (f32)Maximum(client.bottom - client.top, 1);

    ID3D11DeviceContext *context = window->context;
    const f32 clear_colour[4] = { 0.03f, 0.035f, 0.047f, 1.0f };
    context->OMSetRenderTargets(1, &window->render_target, 0);
    context->ClearRenderTargetView(window->render_target, clear_colour);
    if (!splash->ready) return;

    /* The picture fills the largest rect with its aspect, at the centre (ui_splash_fit). */
    f32 aspect = splash->picture ? splash->aspect : 720.0f / 1280.0f;
    f32 picture_width = width, picture_height = width * aspect;
    if (picture_height > height) { picture_height = height; picture_width = height / aspect; }
    f32 left = (width - picture_width) * 0.5f, top = (height - picture_height) * 0.5f;

    f32 step = Minimum(dt_seconds, 0.1f);
    if (splash->picture) splash->picture_seconds += step;
    splash->shown_progress += (progress - splash->shown_progress) * Minimum(1.0f, step * WIN32_SPLASH_BAR_SPEED);
    if (progress >= 1.0f) splash->shown_progress = 1.0f;
    f32 shown = Minimum(Maximum(splash->shown_progress, 0.0f), 1.0f);

    f32 bar_left   = left + picture_width * HEXDOM_SPLASH_BAR_LEFT;
    f32 bar_right  = left + picture_width * HEXDOM_SPLASH_BAR_RIGHT;
    f32 bar_top    = top + picture_height * HEXDOM_SPLASH_BAR_TOP;
    f32 bar_bottom = top + picture_height * HEXDOM_SPLASH_BAR_BOTTOM;
    f32 fill_right = bar_left + (bar_right - bar_left) * shown;

    /* The colours are the frame and the progress bar of the panels (misc/make_splash.py). */
    win32_splash_vertex quads[WIN32_SPLASH_MAX_QUADS * 6];
    f32 fade = splash->picture_seconds / WIN32_SPLASH_FADE_SECONDS;
    win32_splash_quad(quads + 0,  width, height, left, top, left + picture_width, top + picture_height, win32_splash_colour(255, 255, 255, fade));
    win32_splash_quad(quads + 6,  width, height, bar_left, bar_top, bar_right, bar_bottom, win32_splash_colour(29, 47, 73, 1.0f));
    win32_splash_quad(quads + 12, width, height, bar_left, bar_top, fill_right, bar_bottom, win32_splash_colour(230, 179, 0, 1.0f));

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(context->Map(splash->vertices, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
    memcpy(mapped.pData, quads, sizeof(win32_splash_vertex) * 18);
    context->Unmap(splash->vertices, 0);

    D3D11_VIEWPORT viewport = { 0.0f, 0.0f, width, height, 0.0f, 1.0f };
    context->RSSetViewports(1, &viewport);
    context->RSSetState(0);
    context->OMSetDepthStencilState(0, 0);
    const f32 blend_factor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->OMSetBlendState(splash->blend, blend_factor, 0xFFFFFFFFu);
    UINT stride = sizeof(win32_splash_vertex), offset = 0;
    context->IASetInputLayout(splash->layout);
    context->IASetVertexBuffers(0, 1, &splash->vertices, &stride, &offset);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(splash->vertex_shader, 0, 0);
    context->PSSetShader(splash->pixel_shader, 0, 0);
    context->PSSetSamplers(0, 1, &splash->sampler);
    if (splash->picture)
    {
        context->PSSetShaderResources(0, 1, &splash->picture);
        context->Draw(6, 0);
    }
    context->PSSetShaderResources(0, 1, &splash->white);
    context->Draw(12, 6);
}

#endif
