/* win32_hexdom.exe: the platform layer. Creates the window and the D3D11
   device (win32_window.h), owns the ImGui context, backends and fonts
   (ui/ui_fonts.h), allocates the single game memory block, and
   hot-reloads hexdom.dll. Nothing about
   the game lives here; it only calls GameUpdateAndRender once per frame. */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>

#include "base.h"
#include "hexdom.h"
#include "win32_window.h"
#include "win32_portraits.h"
#include "text/text.h"
#include "text/text.cpp"
#include "ui/ui_fonts.h"

struct win32_game_code
{
    HMODULE                 dll;
    FILETIME                write_time;
    game_update_and_render *update_and_render;
    b32                     is_valid;
};

struct win32_state
{
    char  exe_dir[MAX_PATH];
    char  dll_path[MAX_PATH];
    char  temp_dll_path[MAX_PATH];
    char  lock_path[MAX_PATH];
    char  log_path[MAX_PATH];
    char  ini_path[MAX_PATH];
    FILE *log_file;
};

global_variable win32_state g_win32;

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
    /* A file that does not fit in the arena is not read: the caller sees no contents. */
    u64 room = arena->size - arena->used;
    if (GetFileSizeEx(file, &size) && size.QuadPart >= 0 && size.QuadPart < 0xFFFFFFFF &&
        (u64)size.QuadPart + 16 <= room)
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

/* As the reader, a relative path is relative to the folder of the
   executable. The folder of the file is made if it does not exist. */
internal PLATFORM_WRITE_ENTIRE_FILE(win32_write_entire_file)
{
    char resolved[MAX_PATH];
    b32 is_absolute = path[0] == '\\' || path[0] == '/' || (path[0] && path[1] == ':');
    if (is_absolute) snprintf(resolved, MAX_PATH, "%s", path);
    else             snprintf(resolved, MAX_PATH, "%s\\%s", g_win32.exe_dir, path);
    char folder[MAX_PATH];
    snprintf(folder, MAX_PATH, "%s", resolved);
    char *last = strrchr(folder, '\\');
    if (last) { *last = 0; CreateDirectoryA(folder, 0); }
    HANDLE file = CreateFileA(resolved, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    b32 ok = size < 0xFFFFFFFF && WriteFile(file, data, (DWORD)size, &written, 0) && written == (DWORD)size;
    CloseHandle(file);
    return ok;
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

/* ---- paths and arguments ---------------------------------------------- */

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

    /* ImGui: the exe owns the context and the allocators; the DLL binds to both. */
    IMGUI_CHECKVERSION();
    ImGui::SetAllocatorFunctions(win32_imgui_alloc, win32_imgui_free, 0);
    ImGuiContext *imgui = ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = g_win32.ini_path;
    ImGui::StyleColorsDark();

    win32_window window = {};
    if (!win32_open_window(&window, instance, "hexdom", 1280, 800, WS_OVERLAPPEDWINDOW))
    {
        win32_log("platform: failed to create window or D3D11 device");
        return 1;
    }
    if (window.used_warp) win32_log("platform: hardware D3D11 unavailable, using WARP");

    /* The scene renderer: shaders, placeholder materials and models. Data
       comes from <exe dir>\..\data\placeholder when the files exist. */
    char data_dir[MAX_PATH];
    snprintf(data_dir, MAX_PATH, "%s\\..\\data\\placeholder", g_win32.exe_dir);
    memory_arena renderer_scratch;
    arena_init(&renderer_scratch, malloc((size_t)Megabytes(64)), Megabytes(64));
    d3d11_renderer renderer;
    if (!d3d11_renderer_init(&renderer, window.device, window.context, data_dir, &renderer_scratch))
    {
        win32_log("platform: scene renderer failed to start, drawing panels only");
        win32_log(renderer.last_error);
    }

    /* The fonts go into the atlas before the first frame; the DX11 backend
       builds the atlas texture in its first NewFrame. */
    char font_data_dir[MAX_PATH];
    snprintf(font_data_dir, MAX_PATH, "%s\\%s", g_win32.exe_dir, HEXDOM_DATA_ROOT);
    u32 font_pixels = HEXDOM_FONT_DEFAULT_PIXELS;
    ui_font_result fonts = ui_fonts_load(font_data_dir, HEXDOM_CONTENT_PACK, font_pixels);
    win32_log(fonts.message);

    /* One memory block for the whole game, carved into permanent, transient
       and frame regions. A fixed base address in internal builds keeps
       pointers stable across runs for debugging. */
    game_memory memory = {};
    memory.permanent_size = Megabytes(256);
    memory.transient_size = Megabytes(256);
    u64 frame_size        = Megabytes(160);   /* M10: the mesh of the 200 x 200 map needs approximately 66 MB */
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
    memory.platform.write_entire_file = win32_write_entire_file;
    memory.platform.log            = win32_platform_log;
    memory.platform.imgui_alloc      = win32_imgui_alloc;
    memory.platform.imgui_free       = win32_imgui_free;
    memory.platform.imgui_alloc_user = 0;

    /* The default portraits (docs/ASSETS.md). An officer without a picture
       in the archive gets a generated face (win32_faces.h). */
    char portrait_path[MAX_PATH], portrait_message[MAX_PATH + 128];
    snprintf(portrait_path, MAX_PATH, "%s\\%s", g_win32.exe_dir, HEXDOM_PORTRAIT_ARCHIVE);
    if (win32_portraits_open(&g_win32_portraits, window.device, portrait_path, win32_log, portrait_message, sizeof(portrait_message)))
    {
        memory.platform.portraits        = &g_win32_portraits.list;
        memory.platform.portrait_texture = win32_portrait_texture;
    }
    win32_log(portrait_message);
    win32_faces_init(&g_win32_faces, window.device);
    memory.platform.face_texture = win32_face_texture;

    win32_game_code game = win32_load_game_code(&g_win32);
    win32_log(game.is_valid ? "platform: loaded hexdom.dll" : "platform: hexdom.dll not loaded yet");

    game_input input = {};
    LARGE_INTEGER counter_frequency, last_counter;
    QueryPerformanceFrequency(&counter_frequency);
    QueryPerformanceCounter(&last_counter);

    u32 frame_index = 0;
    while (win32_pump_messages(&window))
    {
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

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        input.dt_seconds  = (f32)(now.QuadPart - last_counter.QuadPart) / (f32)counter_frequency.QuadPart;
        input.frame_index = frame_index;
        last_counter = now;

        win32_begin_frame(&window);
        win32_portraits_begin_frame(&g_win32_portraits);
        win32_faces_begin_frame(&g_win32_faces);

        arena_reset(&frame);
        ImVec2 display = ImGui::GetIO().DisplaySize;
        render_commands *commands = render_commands_begin(&frame, display.x, display.y);
        input.render = commands;
        if (game.is_valid)
        {
            game.update_and_render(&memory, &input, imgui, &frame);
            if (memory.quit_requested) PostQuitMessage(0);   /* M9: 結束遊戲 on the title menu */
        }
        else
        {
            ImGui::Begin("hexdom");
            ImGui::Text("hexdom.dll is not loaded. Run build.bat; this window reloads it automatically.");
            ImGui::End();
        }

        win32_render_frame(&window, renderer.ready ? &renderer : 0, commands, &frame);
        win32_present_frame(&window);

        /* A new font size from the Settings panel: the atlas is built again
           between two frames. The DX11 backend makes its font texture again
           in its next NewFrame. */
        if (memory.font_pixels >= HEXDOM_FONT_MIN_PIXELS && memory.font_pixels <= HEXDOM_FONT_MAX_PIXELS &&
            memory.font_pixels != font_pixels)
        {
            font_pixels = memory.font_pixels;
            ImGui_ImplDX11_InvalidateDeviceObjects();
            ImGui::GetIO().Fonts->Clear();
            fonts = ui_fonts_load(font_data_dir, HEXDOM_CONTENT_PACK, font_pixels);
            win32_log(fonts.message);
        }

        frame_index++;
        if (max_frames && frame_index >= max_frames)
        {
            win32_log("platform: --frames reached, exiting");
            window.running = false;
        }
    }

    d3d11_renderer_destroy(&renderer);
    win32_portraits_close(&g_win32_portraits);
    win32_faces_close(&g_win32_faces);
    win32_close_window(&window);
    ImGui::DestroyContext();
    win32_unload_game_code(&game);
    win32_log("platform: clean exit");
    if (g_win32.log_file) fclose(g_win32.log_file);
    return 0;
}
