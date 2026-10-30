#ifndef WIN32_WINDOW_H
#define WIN32_WINDOW_H
/* The window and the D3D11 swap chain of the platform layer. Two executables
   use this file: win32_hexdom.exe (the game) and demo_player.exe (the demo
   recorder). Each one includes it once, in its only translation unit.

   The file owns the window procedure, the device, the swap chain and the
   render target. It also reads the back buffer for screenshots. It knows
   nothing about the game or about the memory block. */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <string.h>

#include "base.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "d3d11_renderer.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct win32_window
{
    HINSTANCE               instance;
    HWND                    handle;
    const char             *class_name;
    b32                     running;
    b32                     used_warp;        /* true if no hardware D3D11 device was available */
    b32                     resize_pending;
    u32                     resize_width;
    u32                     resize_height;
    ID3D11Device           *device;
    ID3D11DeviceContext    *context;
    IDXGISwapChain         *swap_chain;
    ID3D11RenderTargetView *render_target;
    ID3D11Texture2D        *staging;          /* CPU copy of the back buffer, made on the first read */
    u32                     staging_width;
    u32                     staging_height;
    b32                     no_vsync;         /* the present does not wait for the vertical blank */
    /* While a worker thread loads the game (win32_loader.h), the main
       thread makes no ImGui call: the input does not go to ImGui, and
       modal_frame draws a frame while the user moves or sizes the window. */
    b32                     imgui_paused;
    void                  (*modal_frame)(void *user);
    void                   *modal_user;
};

#define WIN32_MODAL_FRAME_TIMER 1

/* The window procedure has no user pointer, so it finds the window here. */
global_variable win32_window *g_window;

/* ---- D3D11 -------------------------------------------------------------- */

internal void
win32_create_render_target(win32_window *window)
{
    ID3D11Texture2D *back_buffer = 0;
    if (SUCCEEDED(window->swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&back_buffer)) && back_buffer)
    {
        window->device->CreateRenderTargetView(back_buffer, 0, &window->render_target);
        back_buffer->Release();
    }
}

internal void
win32_destroy_render_target(win32_window *window)
{
    if (window->render_target)
    {
        window->render_target->Release();
        window->render_target = 0;
    }
}

internal void
win32_destroy_staging(win32_window *window)
{
    if (window->staging)
    {
        window->staging->Release();
        window->staging = 0;
    }
}

internal b32
win32_create_d3d(win32_window *window)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = window->handle;
    sd.SampleDesc.Count                   = 1;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = 0;
    D3D_FEATURE_LEVEL level;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(0, D3D_DRIVER_TYPE_HARDWARE, 0, flags, levels, 2,
                                               D3D11_SDK_VERSION, &sd, &window->swap_chain,
                                               &window->device, &level, &window->context);
    if (hr == DXGI_ERROR_UNSUPPORTED)
    {
        window->used_warp = true;
        hr = D3D11CreateDeviceAndSwapChain(0, D3D_DRIVER_TYPE_WARP, 0, flags, levels, 2,
                                           D3D11_SDK_VERSION, &sd, &window->swap_chain,
                                           &window->device, &level, &window->context);
    }
    if (FAILED(hr)) return false;

    win32_create_render_target(window);
    return true;
}

internal void
win32_destroy_d3d(win32_window *window)
{
    win32_destroy_staging(window);
    win32_destroy_render_target(window);
    if (window->swap_chain) { window->swap_chain->Release(); window->swap_chain = 0; }
    if (window->context)    { window->context->Release();    window->context = 0; }
    if (window->device)     { window->device->Release();     window->device = 0; }
}

/* ---- window ------------------------------------------------------------- */

internal LRESULT CALLBACK
win32_window_proc(HWND handle, UINT message, WPARAM wparam, LPARAM lparam)
{
    b32 imgui_paused = g_window && g_window->imgui_paused;
    if (!imgui_paused && ImGui_ImplWin32_WndProcHandler(handle, message, wparam, lparam)) return 1;

    switch (message)
    {
        case WM_SIZE:
        {
            if (wparam != SIZE_MINIMIZED && g_window)
            {
                g_window->resize_pending = true;
                g_window->resize_width   = LOWORD(lparam);
                g_window->resize_height  = HIWORD(lparam);
            }
            return 0;
        }
        /* Windows runs its own message loop while the user drags or sizes
           the window. A timer then keeps the frames of the load going. */
        case WM_ENTERSIZEMOVE:
        {
            if (g_window && g_window->modal_frame) SetTimer(handle, WIN32_MODAL_FRAME_TIMER, 16, 0);
            break;
        }
        case WM_EXITSIZEMOVE:
        {
            KillTimer(handle, WIN32_MODAL_FRAME_TIMER);
            break;
        }
        case WM_TIMER:
        {
            if (wparam == WIN32_MODAL_FRAME_TIMER && g_window && g_window->modal_frame) g_window->modal_frame(g_window->modal_user);
            return 0;
        }
        case WM_SYSCOMMAND:
        {
            if ((wparam & 0xFFF0) == SC_KEYMENU) return 0;   /* no menu on Alt */
            break;
        }
        case WM_CLOSE:
        case WM_DESTROY:
        {
            if (g_window) g_window->running = false;
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcA(handle, message, wparam, lparam);
}

/* Opens a window with a client area of width x height pixels, creates the
   D3D11 device, and starts the ImGui Win32 and DX11 backends. Create the
   ImGui context before this call. */
internal b32
win32_open_window(win32_window *window, HINSTANCE instance, const char *title,
                  u32 width, u32 height, DWORD style)
{
    g_window = window;
    window->instance   = instance;
    window->class_name = "HexdomWindowClass";

    WNDCLASSEXA window_class = {};
    window_class.cbSize        = sizeof(window_class);
    window_class.style         = CS_CLASSDC;
    window_class.lpfnWndProc   = win32_window_proc;
    window_class.hInstance     = instance;
    window_class.hCursor       = LoadCursorA(0, IDC_ARROW);
    window_class.hIcon         = LoadIconA(instance, MAKEINTRESOURCEA(1));   /* code/win32_hexdom.rc; zero without the resource */
    window_class.lpszClassName = window->class_name;
    RegisterClassExA(&window_class);

    RECT rect = { 0, 0, (LONG)width, (LONG)height };
    AdjustWindowRect(&rect, style, FALSE);
    window->handle = CreateWindowExA(0, window->class_name, title, style, CW_USEDEFAULT, CW_USEDEFAULT,
                                     rect.right - rect.left, rect.bottom - rect.top, 0, 0, instance, 0);
    if (!window->handle || !win32_create_d3d(window)) return false;

    ShowWindow(window->handle, SW_SHOWDEFAULT);
    UpdateWindow(window->handle);
    ImGui_ImplWin32_Init(window->handle);
    ImGui_ImplDX11_Init(window->device, window->context);
    window->running = true;
    return true;
}

internal void
win32_close_window(win32_window *window)
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    win32_destroy_d3d(window);
    if (window->handle) DestroyWindow(window->handle);
    UnregisterClassA(window->class_name, window->instance);
    window->handle = 0;
    g_window = 0;
}

/* Handles the pending messages. Returns false after the user closes the window. */
internal b32
win32_pump_messages(win32_window *window)
{
    MSG message;
    while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&message);
        DispatchMessageA(&message);
        if (message.message == WM_QUIT) window->running = false;
    }
    return window->running;
}

/* Applies a pending resize to the swap chain. */
internal void
win32_apply_resize(win32_window *window)
{
    if (window->resize_pending && window->swap_chain)
    {
        win32_destroy_render_target(window);
        win32_destroy_staging(window);
        window->swap_chain->ResizeBuffers(0, window->resize_width, window->resize_height, DXGI_FORMAT_UNKNOWN, 0);
        win32_create_render_target(window);
        window->resize_pending = false;
    }
}

/* Starts a frame: applies a pending resize, then starts the ImGui backends
   and the ImGui frame. */
internal void
win32_begin_frame(win32_window *window)
{
    win32_apply_resize(window);
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

/* Renders the scene (when renderer and commands are given) and then the
   ImGui draw data into the back buffer. It does not present. */
internal void
win32_render_frame(win32_window *window, d3d11_renderer *renderer, render_commands *commands, memory_arena *scratch)
{
    ImGui::Render();
    const f32 clear_color[4] = { 0.09f, 0.10f, 0.12f, 1.0f };
    window->context->OMSetRenderTargets(1, &window->render_target, 0);
    window->context->ClearRenderTargetView(window->render_target, clear_color);
    if (renderer && commands) d3d11_renderer_draw(renderer, commands, window->render_target, scratch);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

internal void
win32_present_frame(win32_window *window)
{
    window->swap_chain->Present(window->no_vsync ? 0 : 1, 0);
}

/* ---- back buffer read --------------------------------------------------- */

/* Copies a rectangle of the back buffer into pixels, as RGBA bytes, one row
   after the other. Call it after win32_render_frame and before
   win32_present_frame: the DISCARD swap effect does not keep the back
   buffer after a present. */
internal b32
win32_read_back_buffer(win32_window *window, s32 x, s32 y, s32 width, s32 height, u32 *pixels)
{
    ID3D11Texture2D *back_buffer = 0;
    if (FAILED(window->swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&back_buffer)) || !back_buffer)
    {
        return false;
    }
    D3D11_TEXTURE2D_DESC desc;
    back_buffer->GetDesc(&desc);
    b32 inside = x >= 0 && y >= 0 && width > 0 && height > 0 &&
                 (u32)(x + width) <= desc.Width && (u32)(y + height) <= desc.Height;
    if (!inside || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM)
    {
        back_buffer->Release();
        return false;
    }

    if (window->staging && (window->staging_width != desc.Width || window->staging_height != desc.Height))
    {
        win32_destroy_staging(window);
    }
    if (!window->staging)
    {
        D3D11_TEXTURE2D_DESC staging_desc = desc;
        staging_desc.BindFlags      = 0;
        staging_desc.MiscFlags      = 0;
        staging_desc.Usage          = D3D11_USAGE_STAGING;
        staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (FAILED(window->device->CreateTexture2D(&staging_desc, 0, &window->staging)))
        {
            back_buffer->Release();
            return false;
        }
        window->staging_width  = desc.Width;
        window->staging_height = desc.Height;
    }
    window->context->CopyResource(window->staging, back_buffer);
    back_buffer->Release();

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(window->context->Map(window->staging, 0, D3D11_MAP_READ, 0, &mapped))) return false;
    for (s32 row = 0; row < height; ++row)
    {
        u8 *source = (u8 *)mapped.pData + (u64)(y + row) * mapped.RowPitch + (u64)x * 4;
        memcpy(pixels + (u64)row * (u64)width, source, (size_t)width * 4);
    }
    window->context->Unmap(window->staging, 0);
    return true;
}

#endif /* WIN32_WINDOW_H */
