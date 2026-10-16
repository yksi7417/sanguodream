/* win32_hexdom.exe: the platform layer. Creates the window and the D3D11
   device, owns the ImGui context and backends, allocates the single game
   memory block, and hot-reloads hexdom.dll. Nothing about the game lives
   here; it only calls GameUpdateAndRender once per frame. */

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include <stdlib.h>

#include "base.h"
#include "hexdom.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct win32_d3d
{
    ID3D11Device           *device;
    ID3D11DeviceContext    *context;
    IDXGISwapChain         *swap_chain;
    ID3D11RenderTargetView *render_target;
};

struct win32_game_code
{
    HMODULE                 dll;
    FILETIME                write_time;
    game_update_and_render *update_and_render;
    b32                     is_valid;
};

struct win32_state
{
    b32   running;
    b32   resize_pending;
    u32   resize_width;
    u32   resize_height;
    char  exe_dir[MAX_PATH];
    char  dll_path[MAX_PATH];
    char  temp_dll_path[MAX_PATH];
    char  lock_path[MAX_PATH];
    char  log_path[MAX_PATH];
    char  ini_path[MAX_PATH];
    FILE *log_file;
};

global_variable win32_state g_win32;
global_variable win32_d3d   g_d3d;

/* ---- logging and files -------------------------------------------------- */

internal void
win32_log(const char *message)
{
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
    if (g_win32.log_file)
    {
        fprintf(g_win32.log_file, "%s\n", message);
        fflush(g_win32.log_file);
    }
}

internal PLATFORM_LOG(win32_platform_log)
{
    win32_log(message);
}

/* A relative path is relative to the folder of the executable, not to the
   current directory, so the game finds its data however it was started. */
internal PLATFORM_READ_ENTIRE_FILE(win32_read_entire_file)
{
    read_file_result result = {};
    char resolved[MAX_PATH];
    b32 is_absolute = path[0] == '\\' || path[0] == '/' || (path[0] && path[1] == ':');
    if (is_absolute) snprintf(resolved, MAX_PATH, "%s", path);
    else             snprintf(resolved, MAX_PATH, "%s\\%s", g_win32.exe_dir, path);
    HANDLE file = CreateFileA(resolved, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (file == INVALID_HANDLE_VALUE) return result;

    LARGE_INTEGER size;
    if (GetFileSizeEx(file, &size) && size.QuadPart >= 0 && size.QuadPart < 0xFFFFFFFF)
    {
        u64   bytes  = (u64)size.QuadPart;
        void *buffer = arena_push_size(arena, bytes ? bytes : 1);
        DWORD read   = 0;
        if (ReadFile(file, buffer, (DWORD)bytes, &read, 0) && read == (DWORD)bytes)
        {
            result.size     = bytes;
            result.contents = buffer;
        }
    }
    CloseHandle(file);
    return result;
}

internal void *
win32_imgui_alloc(size_t size, void *user)
{
    (void)user;
    return malloc(size);
}

internal void
win32_imgui_free(void *ptr, void *user)
{
    (void)user;
    free(ptr);
}

internal FILETIME
win32_file_write_time(const char *path)
{
    FILETIME result = {};
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &data))
    {
        result = data.ftLastWriteTime;
    }
    return result;
}

internal b32
win32_file_exists(const char *path)
{
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

/* ---- hot reload --------------------------------------------------------- */

internal win32_game_code
win32_load_game_code(win32_state *state)
{
    win32_game_code result = {};
    if (win32_file_exists(state->lock_path)) return result;   /* build.bat is mid-write */

    result.write_time = win32_file_write_time(state->dll_path);
    /* Load a copy so the compiler can overwrite the original while we run. */
    if (!CopyFileA(state->dll_path, state->temp_dll_path, FALSE)) return result;

    result.dll = LoadLibraryA(state->temp_dll_path);
    if (result.dll)
    {
        result.update_and_render = (game_update_and_render *)GetProcAddress(result.dll, "GameUpdateAndRender");
        result.is_valid = (result.update_and_render != 0);
    }
    if (!result.is_valid)
    {
        if (result.dll) FreeLibrary(result.dll);
        result.dll = 0;
        result.update_and_render = 0;
    }
    return result;
}

internal void
win32_unload_game_code(win32_game_code *code)
{
    if (code->dll) FreeLibrary(code->dll);
    code->dll = 0;
    code->update_and_render = 0;
    code->is_valid = false;
}

/* ---- D3D11 -------------------------------------------------------------- */

internal void
win32_create_render_target(void)
{
    ID3D11Texture2D *back_buffer = 0;
    if (SUCCEEDED(g_d3d.swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&back_buffer)) && back_buffer)
    {
        g_d3d.device->CreateRenderTargetView(back_buffer, 0, &g_d3d.render_target);
        back_buffer->Release();
    }
}

internal void
win32_destroy_render_target(void)
{
    if (g_d3d.render_target)
    {
        g_d3d.render_target->Release();
        g_d3d.render_target = 0;
    }
}

internal b32
win32_create_d3d(HWND window)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = window;
    sd.SampleDesc.Count                   = 1;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = 0;
    D3D_FEATURE_LEVEL level;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(0, D3D_DRIVER_TYPE_HARDWARE, 0, flags, levels, 2,
                                               D3D11_SDK_VERSION, &sd, &g_d3d.swap_chain,
                                               &g_d3d.device, &level, &g_d3d.context);
    if (hr == DXGI_ERROR_UNSUPPORTED)
    {
        win32_log("platform: hardware D3D11 unavailable, using WARP");
        hr = D3D11CreateDeviceAndSwapChain(0, D3D_DRIVER_TYPE_WARP, 0, flags, levels, 2,
                                           D3D11_SDK_VERSION, &sd, &g_d3d.swap_chain,
                                           &g_d3d.device, &level, &g_d3d.context);
    }
    if (FAILED(hr)) return false;

    win32_create_render_target();
    return true;
}

internal void
win32_destroy_d3d(void)
{
    win32_destroy_render_target();
    if (g_d3d.swap_chain) { g_d3d.swap_chain->Release(); g_d3d.swap_chain = 0; }
    if (g_d3d.context)    { g_d3d.context->Release();    g_d3d.context = 0; }
    if (g_d3d.device)     { g_d3d.device->Release();     g_d3d.device = 0; }
}

/* ---- window ------------------------------------------------------------- */

internal LRESULT CALLBACK
win32_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (ImGui_ImplWin32_WndProcHandler(window, message, wparam, lparam)) return 1;

    switch (message)
    {
        case WM_SIZE:
        {
            if (wparam != SIZE_MINIMIZED)
            {
                g_win32.resize_pending = true;
                g_win32.resize_width   = LOWORD(lparam);
                g_win32.resize_height  = HIWORD(lparam);
            }
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
            g_win32.running = false;
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcA(window, message, wparam, lparam);
}

internal void
win32_build_paths(win32_state *state)
{
    DWORD length = GetModuleFileNameA(0, state->exe_dir, MAX_PATH);
    for (DWORD i = length; i > 0; --i)
    {
        if (state->exe_dir[i - 1] == '\\')
        {
            state->exe_dir[i - 1] = 0;
            break;
        }
    }
    snprintf(state->dll_path,      MAX_PATH, "%s\\hexdom.dll",      state->exe_dir);
    snprintf(state->temp_dll_path, MAX_PATH, "%s\\hexdom_temp.dll", state->exe_dir);
    snprintf(state->lock_path,     MAX_PATH, "%s\\lock.tmp",        state->exe_dir);
    snprintf(state->log_path,      MAX_PATH, "%s\\hexdom.log",      state->exe_dir);
    snprintf(state->ini_path,      MAX_PATH, "%s\\imgui.ini",       state->exe_dir);
}

/* "--frames N" on the command line exits after N frames: a smoke run that
   needs no human to close the window. */
internal u32
win32_parse_frames_arg(void)
{
    const char *command_line = GetCommandLineA();
    const char *found = strstr(command_line, "--frames");
    if (!found) return 0;
    return (u32)strtoul(found + 8, 0, 10);
}

/* ---- main --------------------------------------------------------------- */

int WINAPI
WinMain(HINSTANCE instance, HINSTANCE previous_instance, LPSTR command_line, int show_command)
{
    (void)previous_instance; (void)command_line; (void)show_command;

    win32_build_paths(&g_win32);
    g_win32.log_file = fopen(g_win32.log_path, "w");
    win32_log("platform: starting");
    u32 max_frames = win32_parse_frames_arg();

    WNDCLASSEXA window_class = {};
    window_class.cbSize        = sizeof(window_class);
    window_class.style         = CS_CLASSDC;
    window_class.lpfnWndProc   = win32_window_proc;
    window_class.hInstance     = instance;
    window_class.hCursor       = LoadCursorA(0, IDC_ARROW);
    window_class.lpszClassName = "HexdomWindowClass";
    RegisterClassExA(&window_class);

    HWND window = CreateWindowExA(0, window_class.lpszClassName, "hexdom", WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800, 0, 0, instance, 0);
    if (!window || !win32_create_d3d(window))
    {
        win32_log("platform: failed to create window or D3D11 device");
        return 1;
    }
    ShowWindow(window, SW_SHOWDEFAULT);
    UpdateWindow(window);

    /* ImGui: the exe owns the context and the allocators; the DLL binds to both. */
    IMGUI_CHECKVERSION();
    ImGui::SetAllocatorFunctions(win32_imgui_alloc, win32_imgui_free, 0);
    ImGuiContext *imgui = ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = g_win32.ini_path;
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(g_d3d.device, g_d3d.context);

    /* One memory block for the whole game, carved into permanent, transient
       and frame regions. A fixed base address in internal builds keeps
       pointers stable across runs for debugging. */
    game_memory memory = {};
    memory.permanent_size = Megabytes(256);
    memory.transient_size = Megabytes(256);
    u64 frame_size        = Megabytes(64);
    u64 total_size        = memory.permanent_size + memory.transient_size + frame_size;

    LPVOID base_address = 0;
#if HEXDOM_INTERNAL
    base_address = (LPVOID)Terabytes(2);
#endif
    void *block = VirtualAlloc(base_address, (SIZE_T)total_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!block) block = VirtualAlloc(0, (SIZE_T)total_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!block)
    {
        win32_log("platform: VirtualAlloc failed");
        return 1;
    }
    memory.permanent = block;
    memory.transient = (u8 *)block + memory.permanent_size;
    memory_arena frame;
    arena_init(&frame, (u8 *)block + memory.permanent_size + memory.transient_size, frame_size);

    memory.platform.read_entire_file = win32_read_entire_file;
    memory.platform.log              = win32_platform_log;
    memory.platform.imgui_alloc      = win32_imgui_alloc;
    memory.platform.imgui_free       = win32_imgui_free;
    memory.platform.imgui_alloc_user = 0;

    win32_game_code game = win32_load_game_code(&g_win32);
    win32_log(game.is_valid ? "platform: loaded hexdom.dll" : "platform: hexdom.dll not loaded yet");

    game_input input = {};
    LARGE_INTEGER counter_frequency, last_counter;
    QueryPerformanceFrequency(&counter_frequency);
    QueryPerformanceCounter(&last_counter);

    g_win32.running = true;
    u32 frame_index = 0;
    while (g_win32.running)
    {
        MSG message;
        while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageA(&message);
            if (message.message == WM_QUIT) g_win32.running = false;
        }
        if (!g_win32.running) break;

        /* Hot reload: when build.bat has written a new DLL (and finished), swap it in. */
        input.dll_reloaded = false;
        if (!win32_file_exists(g_win32.lock_path))
        {
            FILETIME current_write_time = win32_file_write_time(g_win32.dll_path);
            b32 changed = CompareFileTime(&current_write_time, &game.write_time) != 0;
            if (changed || !game.is_valid)
            {
                win32_unload_game_code(&game);
                for (u32 attempt = 0; attempt < 10 && !game.is_valid; ++attempt)
                {
                    game = win32_load_game_code(&g_win32);
                    if (!game.is_valid) Sleep(50);
                }
                if (game.is_valid)
                {
                    input.dll_reloaded = true;
                    win32_log("platform: reloaded hexdom.dll");
                }
            }
        }

        if (g_win32.resize_pending && g_d3d.swap_chain)
        {
            win32_destroy_render_target();
            g_d3d.swap_chain->ResizeBuffers(0, g_win32.resize_width, g_win32.resize_height, DXGI_FORMAT_UNKNOWN, 0);
            win32_create_render_target();
            g_win32.resize_pending = false;
        }

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        input.dt_seconds  = (f32)(now.QuadPart - last_counter.QuadPart) / (f32)counter_frequency.QuadPart;
        input.frame_index = frame_index;
        last_counter = now;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        arena_reset(&frame);
        if (game.is_valid)
        {
            game.update_and_render(&memory, &input, imgui, &frame);
        }
        else
        {
            ImGui::Begin("hexdom");
            ImGui::Text("hexdom.dll is not loaded. Run build.bat; this window reloads it automatically.");
            ImGui::End();
        }

        ImGui::Render();
        const f32 clear_color[4] = { 0.09f, 0.10f, 0.12f, 1.0f };
        g_d3d.context->OMSetRenderTargets(1, &g_d3d.render_target, 0);
        g_d3d.context->ClearRenderTargetView(g_d3d.render_target, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_d3d.swap_chain->Present(1, 0);

        frame_index++;
        if (max_frames && frame_index >= max_frames)
        {
            win32_log("platform: --frames reached, exiting");
            g_win32.running = false;
        }
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    win32_destroy_d3d();
    DestroyWindow(window);
    UnregisterClassA(window_class.lpszClassName, instance);
    win32_unload_game_code(&game);
    win32_log("platform: clean exit");
    if (g_win32.log_file) fclose(g_win32.log_file);
    return 0;
}
