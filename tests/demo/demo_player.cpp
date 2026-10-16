/* demo_player.exe: the milestone demo recorder and the end-to-end smoke test.

   It opens a real window with D3D11 (win32_window.h, the same code as
   win32_hexdom.exe), compiles the engine and the UI into this exe (not
   through hexdom.dll), and runs the tests of category "demo" at cinematic
   speed. The capture tool of the Dear ImGui Test Engine records each
   walkthrough into build\demos: an MP4 through ffmpeg, or a GIF through
   gif_encode.exe if ffmpeg is not found, and a PNG of each panel.

   Usage: demo_player.exe [test-name] [--list] [--no-ffmpeg] [--ffmpeg <path>]
     test-name     one walkthrough, for example "m0_skeleton"; default: all
     --list        prints the name of each demo test, one on each line
     --no-ffmpeg   records a GIF even if ffmpeg is installed
     --ffmpeg      the path of ffmpeg.exe; default: PATH, then winget */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_capture_tool.h"

#include "base.h"
#include "win32_window.h"
#include "ecs/ecs.h"
#include "ecs/ecs.cpp"
#include "sim/sim.h"
#include "sim/sim.cpp"
#include "engine/engine.h"
#include "engine/engine.cpp"
#include "text/text.h"
#include "text/text.cpp"
#include "ui/ui.h"
#include "ui/ui.cpp"
#include "ui/ui_fonts.h"
#include "test_framework.h"
#include "test_files.h"          /* after win32_window.h: it removes the Yield macro of windows.h */
#include "demo_tests.cpp"

#define DEMO_WIDTH       1280
#define DEMO_HEIGHT      720
#define DEMO_MAX_FRAMES  100000

#define DEMO_FFMPEG_PARAMS "-hide_banner -loglevel error -r $FPS -f rawvideo -pix_fmt rgba -s $WIDTHx$HEIGHT " \
                           "-i - -threads 0 -y -preset ultrafast -pix_fmt yuv420p -crf 20 \"$OUTPUT\""
#define DEMO_GIF_PARAMS    "$WIDTH $HEIGHT $FPS \"$OUTPUT\""

/* The place and size of each panel in the 1280x720 window. The Hex map and
   the Officers panel share a place, because no walkthrough shows both. */
static const char demo_layout_ini[] =
    "[Window][Simulation]\nPos=10,30\nSize=490,220\nCollapsed=0\n\n"
    "[Window][Event ledger]\nPos=10,260\nSize=490,450\nCollapsed=0\n\n"
    "[Window][Hex map]\nPos=510,30\nSize=760,510\nCollapsed=0\n\n"
    "[Window][Memory]\nPos=510,550\nSize=420,160\nCollapsed=0\n\n"
    "[Window][###Officers]\nPos=510,30\nSize=760,400\nCollapsed=0\n\n"
    "[Window][###Cities]\nPos=510,440\nSize=760,270\nCollapsed=0\n\n"
    "[Window][###City]\nPos=510,30\nSize=380,680\nCollapsed=0\n\n"
    "[Window][###Develop]\nPos=900,30\nSize=370,330\nCollapsed=0\n\n";

struct demo_options
{
    const char *test_name;     /* zero: every demo test */
    b32         list;
    b32         no_ffmpeg;
    const char *ffmpeg_path;
};

internal demo_options
demo_parse_options(int argc, char **argv)
{
    demo_options options = {};
    for (int index = 1; index < argc; ++index)
    {
        if      (strcmp(argv[index], "--list") == 0)                        options.list = true;
        else if (strcmp(argv[index], "--no-ffmpeg") == 0)                   options.no_ffmpeg = true;
        else if (strcmp(argv[index], "--ffmpeg") == 0 && index + 1 < argc)  options.ffmpeg_path = argv[++index];
        else                                                                options.test_name = argv[index];
    }
    return options;
}

internal b32
demo_file_exists(const char *path)
{
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

/* Finds the first directory that matches pattern (with one '*') and writes
   <that directory>\<rest> to out. */
internal b32
demo_find_in_directory(char *out, u32 out_size, const char *folder, const char *pattern, const char *rest)
{
    char search[MAX_PATH];
    snprintf(search, sizeof(search), "%s\\%s", folder, pattern);
    WIN32_FIND_DATAA found;
    HANDLE handle = FindFirstFileA(search, &found);
    if (handle == INVALID_HANDLE_VALUE) return false;
    b32 result = false;
    do
    {
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            snprintf(out, out_size, "%s\\%s\\%s", folder, found.cFileName, rest);
            if (demo_file_exists(out)) { result = true; break; }
        }
    } while (FindNextFileA(handle, &found));
    FindClose(handle);
    return result;
}

/* Finds ffmpeg.exe: on the PATH, then in the folders of winget (Gyan.FFmpeg).
   A shell that was open before the install does not have the new PATH. */
internal b32
demo_find_ffmpeg(char *out, u32 out_size)
{
    if (SearchPathA(0, "ffmpeg.exe", 0, out_size, out, 0)) return true;

    char local_app_data[MAX_PATH];
    if (!GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, MAX_PATH)) return false;
    snprintf(out, out_size, "%s\\Microsoft\\WinGet\\Links\\ffmpeg.exe", local_app_data);
    if (demo_file_exists(out)) return true;

    char packages[MAX_PATH];
    char package[MAX_PATH];
    snprintf(packages, sizeof(packages), "%s\\Microsoft\\WinGet\\Packages", local_app_data);
    WIN32_FIND_DATAA found;
    char search[MAX_PATH];
    snprintf(search, sizeof(search), "%s\\Gyan.FFmpeg*", packages);
    HANDLE handle = FindFirstFileA(search, &found);
    if (handle == INVALID_HANDLE_VALUE) return false;
    b32 result = false;
    do
    {
        snprintf(package, sizeof(package), "%s\\%s", packages, found.cFileName);
        if (demo_find_in_directory(out, out_size, package, "ffmpeg-*", "bin\\ffmpeg.exe")) { result = true; break; }
    } while (FindNextFileA(handle, &found));
    FindClose(handle);
    return result;
}

/* Sets the video encoder of the capture tool: ffmpeg for MP4 if it exists,
   otherwise gif_encode.exe (next to this exe) for GIF. */
internal void
demo_configure_encoder(ImGuiTestEngineIO *test_io, demo_options *options, const char *exe_dir)
{
    char ffmpeg[MAX_PATH] = {};
    b32 has_ffmpeg = false;
    if (options->ffmpeg_path)
    {
        snprintf(ffmpeg, sizeof(ffmpeg), "%s", options->ffmpeg_path);
        has_ffmpeg = demo_file_exists(ffmpeg);
    }
    else if (!options->no_ffmpeg)
    {
        has_ffmpeg = demo_find_ffmpeg(ffmpeg, sizeof(ffmpeg));
    }
    if (has_ffmpeg)
    {
        snprintf(test_io->VideoCaptureEncoderPath, sizeof(test_io->VideoCaptureEncoderPath), "%s", ffmpeg);
        snprintf(test_io->VideoCaptureExtension, sizeof(test_io->VideoCaptureExtension), ".mp4");
    }
    else
    {
        snprintf(test_io->VideoCaptureEncoderPath, sizeof(test_io->VideoCaptureEncoderPath), "%s\\gif_encode.exe", exe_dir);
        snprintf(test_io->VideoCaptureExtension, sizeof(test_io->VideoCaptureExtension), ".gif");
    }
    snprintf(test_io->VideoCaptureEncoderParams, sizeof(test_io->VideoCaptureEncoderParams), "%s", DEMO_FFMPEG_PARAMS);
    snprintf(test_io->GifCaptureEncoderParams, sizeof(test_io->GifCaptureEncoderParams), "%s", DEMO_GIF_PARAMS);
    printf("demo_player: video encoder %s\n", test_io->VideoCaptureEncoderPath);
}

/* The ScreenCaptureFunc of the capture tool: the back buffer as RGBA. */
internal bool
demo_capture_screen(ImGuiID viewport_id, int x, int y, int width, int height, unsigned int *pixels, void *user_data)
{
    (void)viewport_id;
    return win32_read_back_buffer((win32_window *)user_data, x, y, width, height, pixels) != 0;
}

internal void
demo_print_test_names(ImGuiTestEngine *engine)
{
    ImVector<ImGuiTest *> tests;
    ImGuiTestEngine_GetTestList(engine, &tests);
    for (int index = 0; index < tests.Size; ++index)
    {
        if (strcmp(tests[index]->Category, "demo") == 0) printf("%s\n", tests[index]->Name);
    }
}

int
main(int argc, char **argv)
{
    setvbuf(stdout, 0, _IONBF, 0);
    demo_options options = demo_parse_options(argc, argv);

    char exe_dir[MAX_PATH];
    test_exe_dir(exe_dir, MAX_PATH);
    snprintf(g_demo_output_dir, MAX_PATH, "%s\\demos", exe_dir);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = 0;
    ImGui::StyleColorsDark();
    ImGui::LoadIniSettingsFromMemory(demo_layout_ini, sizeof(demo_layout_ini) - 1);

    ImGuiTestEngine *engine = ImGuiTestEngine_CreateContext();
    ImGuiTestEngineIO &test_io = ImGuiTestEngine_GetIO(engine);
    register_demo_tests(engine);
    if (options.list)
    {
        demo_print_test_names(engine);
        ImGui::DestroyContext();
        ImGuiTestEngine_DestroyContext(engine);
        return 0;
    }

    win32_window window = {};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;   /* fixed size: the video size */
    if (!win32_open_window(&window, GetModuleHandleA(0), "hexdom demo", DEMO_WIDTH, DEMO_HEIGHT, style))
    {
        printf("demo_player: failed to create the window or the D3D11 device\nRESULT FAIL\n");
        return 1;
    }
    CreateDirectoryA(g_demo_output_dir, 0);

    /* the scene renderer and the frame arena for its command buffers */
    char data_dir[MAX_PATH];
    snprintf(data_dir, MAX_PATH, "%s\\..\\data\\placeholder", exe_dir);
    memory_arena frame_arena = test_arena_create(Megabytes(64));
    d3d11_renderer renderer;
    b32 renderer_ready = d3d11_renderer_init(&renderer, window.device, window.context, data_dir, &frame_arena);
    if (!renderer_ready) printf("demo_player: scene renderer failed: %s\n", renderer.last_error);
    arena_reset(&frame_arena);

    /* The same fonts as the game window, so the video shows the decoded names. */
    char font_data_dir[MAX_PATH];
    snprintf(font_data_dir, MAX_PATH, "%s\\%s", exe_dir, HEXDOM_DATA_ROOT);
    ui_font_result fonts = ui_fonts_load(font_data_dir, HEXDOM_CONTENT_PACK);
    printf("%s\n", fonts.message);

    test_io.ConfigRunSpeed            = ImGuiTestRunSpeed_Cinematic;
    test_io.ConfigVerboseLevel        = ImGuiTestVerboseLevel_Info;
    test_io.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;
    test_io.ConfigLogToTTY            = true;
    test_io.ConfigCaptureEnabled      = true;
    test_io.ScreenCaptureFunc         = demo_capture_screen;
    test_io.ScreenCaptureUserData     = &window;
    demo_configure_encoder(&test_io, &options, exe_dir);
    ImGuiTestEngine_Start(engine, ImGui::GetCurrentContext());

    char filter[128];
    if (options.test_name) snprintf(filter, sizeof(filter), "^%s$", options.test_name);
    else                   snprintf(filter, sizeof(filter), "demo");
    ImGuiTestEngine_QueueTests(engine, ImGuiTestGroup_Tests, filter, ImGuiTestRunFlags_RunFromCommandLine);

    /* The capture tool reads the back buffer in PostSwap, so PostSwap comes
       before the present: the DISCARD swap effect drops the buffer. */
    b32 closed_by_user = false;
    u32 frames = 0;
    while (!ImGuiTestEngine_IsTestQueueEmpty(engine) && frames < DEMO_MAX_FRAMES)
    {
        if (!win32_pump_messages(&window) && !closed_by_user)
        {
            closed_by_user = true;
            ImGuiTestEngine_AbortCurrentTest(engine);
        }
        win32_begin_frame(&window);
        arena_reset(&frame_arena);
        ImVec2 display = ImGui::GetIO().DisplaySize;
        g_demo_render = render_commands_begin(&frame_arena, display.x, display.y);
        g_demo_frame_arena = &frame_arena;
        /* the test engine runs the GuiFunc inside NewFrame; the commands it pushed are drawn here */
        win32_render_frame(&window, renderer_ready ? &renderer : 0, g_demo_render, &frame_arena);
        ImGuiTestEngine_PostSwap(engine);
        win32_present_frame(&window);
        frames++;
    }

    int tested = 0, passed = 0;
    ImGuiTestEngine_GetResult(engine, tested, passed);
    b32 success = tested > 0 && tested == passed && !closed_by_user && frames < DEMO_MAX_FRAMES;
    printf("%s %d/%d demo (%u frames)\n", success ? "PASS" : "FAIL", passed, tested, frames);

    ImGuiTestEngine_Stop(engine);
    win32_close_window(&window);
    ImGui::DestroyContext();
    ImGuiTestEngine_DestroyContext(engine);
    printf(success ? "RESULT PASS\n" : "RESULT FAIL\n");
    return success ? 0 : 1;
}
