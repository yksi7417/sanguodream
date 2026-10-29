/* wasm_hexdom: the web platform layer (docs/WEB.md). It is the port of
   win32_hexdom.cpp to Emscripten. It owns the ImGui context, the WebGL2
   context, the fonts and the game memory, and it calls GameUpdateAndRender
   one time for each frame of the browser.

   The differences from the Windows build:
   - The game is in this unit (HEXDOM_STATIC_GAME). There is no DLL and no
     hot reload.
   - The browser calls wasm_frame through emscripten_set_main_loop. This
     file has no loop of its own.
   - The files of the pack are in MEMFS: build_web.bat packs data/ into
     /data, and the current folder is /build. Thus the relative paths of
     the game, for example "../data/placeholder", are the same as on
     Windows.
   - The saves are in IndexedDB (IDBFS, mounted at /build/saves). The
     platform starts the game after the first read of IndexedDB, and it
     writes IndexedDB again after each save.
   - If the server keeps the saves (web/server.py), the page reads them
     before the start and sends each new save (web/shell.html). */

#define HEXDOM_STATIC_GAME 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#include <GLFW/glfw3.h>

#include "hexdom.cpp"             /* the game: ecs, sim, engine, text and ui */
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ui/ui_fonts.h"
#include "webgl2_renderer.h"
#include "wasm_portraits.h"

/* The memory blocks. The peaks of the title screen are 16 MB, 4 MB,
   67 MB and 36 MB (docs/WEB.md). */
#define WASM_PERMANENT_SIZE        Megabytes(128)
#define WASM_TRANSIENT_SIZE        Megabytes(128)
#define WASM_FRAME_SIZE            Megabytes(160)
#define WASM_RENDERER_SCRATCH_SIZE Megabytes(64)
#define WASM_SAVE_FOLDER           "saves"          /* engine_config.save_folder: the mount of IDBFS */
#define WASM_SYNC_FRAMES           600              /* the platform writes IndexedDB at least this often */

struct wasm_state
{
    GLFWwindow     *window;
    ImGuiContext   *imgui;
    game_memory     memory;
    memory_arena    frame;
    memory_arena    renderer_scratch;
    webgl2_renderer renderer;
    u32             font_pixels;
    u32             frame_index;
    f64             last_time;
    b32             sync_pending;
    u64             peak_permanent, peak_transient, peak_frame;
};

global_variable wasm_state g_wasm;

#if HEXDOM_WEB_TEST
#include "wasm_test_hooks.h"       /* only in "build_web.bat test-game"; it uses g_wasm */
#endif

/* ---- logging and files -------------------------------------------------- */

internal void
wasm_log(const char *message)
{
    printf("%s\n", message);
}

internal PLATFORM_LOG(wasm_platform_log)
{
    wasm_log(message);
}

/* A relative path is relative to /build, the current folder, as a relative
   path on Windows is relative to the folder of the executable. */
internal PLATFORM_READ_ENTIRE_FILE(wasm_read_entire_file)
{
    read_file_result result = {};
    FILE *file = fopen(path, "rb");
    if (!file) return result;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    /* A file that does not fit in the arena is not read: the caller sees no contents. */
    if (size >= 0 && (u64)size + 16 <= arena->size - arena->used)
    {
        u64   bytes  = (u64)size;
        void *buffer = arena_push_size(arena, bytes ? bytes : 1);
        if (fread(buffer, 1, (size_t)bytes, file) == (size_t)bytes)
        {
            result.size     = bytes;
            result.contents = buffer;
        }
    }
    fclose(file);
    return result;
}

/* Writes IndexedDB from the MEMFS copy of the saves. The write is
   asynchronous; the platform starts no second write before the first one
   ends. */
internal void
wasm_sync_saves(void)
{
    if (g_wasm.sync_pending) return;
    g_wasm.sync_pending = true;
    EM_ASM({
        FS.syncfs(false, function(error) {
            if (error) console.log("platform: the saves do not write to IndexedDB: " + error);
            _wasm_sync_done();
        });
    });
}

extern "C" EMSCRIPTEN_KEEPALIVE void
wasm_sync_done(void)
{
    g_wasm.sync_pending = false;
}

/* As the reader, a relative path is relative to /build. The folder of the
   file is made if it does not exist. After a write, the platform writes
   IndexedDB, so the save stays after the page closes. */
internal PLATFORM_WRITE_ENTIRE_FILE(wasm_write_entire_file)
{
    char folder[512];
    snprintf(folder, sizeof(folder), "%s", path);
    char *last = strrchr(folder, '/');
    if (last) { *last = 0; mkdir(folder, 0777); }
    FILE *file = fopen(path, "wb");
    if (!file) return false;
    b32 ok = fwrite(data, 1, (size_t)size, file) == (size_t)size;
    fclose(file);
    wasm_sync_saves();
    /* The page sends the save to the server, if the server keeps the saves (web/shell.html). */
    if (ok)
    {
        EM_ASM({ if (Module.hexdomPushSave) Module.hexdomPushSave(UTF8ToString($0)); }, path);
    }
    return ok;
}

/* Opens a link of the English Wikipedia in a new tab. The platform checks
   the link again, so the page never opens another host. */
internal PLATFORM_OPEN_URL(wasm_open_url)
{
    if (!text_link_is_allowed(url)) return false;
    EM_ASM({ window.open(UTF8ToString($0), "_blank", "noopener"); }, url);
    return true;
}

internal void *
wasm_imgui_alloc(size_t size, void *user)
{
    (void)user;
    return malloc(size);
}

internal void
wasm_imgui_free(void *ptr, void *user)
{
    (void)user;
    free(ptr);
}

/* ---- a shared game (docs/MULTIPLAYER.md, MP3) ----------------------------- */

/* The page owns the socket (web/shell.html). Module.hexdomNetPush copies an
   event of the page into one block of the heap: the bytes, then the text.
   The game takes the events with net_poll. An event stays valid until the
   next poll, and then its block goes back to the heap. */
#define WASM_NET_QUEUE 1024

struct wasm_net_item
{
    platform_net_event event;
    void              *block;
};

global_variable wasm_net_item g_net_queue[WASM_NET_QUEUE];
global_variable u32           g_net_first, g_net_count;
global_variable void         *g_net_current;

extern "C" EMSCRIPTEN_KEEPALIVE void *
wasm_net_alloc(int size)
{
    return malloc(size > 0 ? (size_t)size : 1);
}

extern "C" EMSCRIPTEN_KEEPALIVE void
wasm_net_push(int kind, int step, int seat, int host, u8 *block, int size, char *text)
{
    if (g_net_count == WASM_NET_QUEUE || kind <= PLATFORM_NET_NONE || kind >= PLATFORM_NET_KIND_COUNT || size < 0)
    {
        wasm_log("platform: the queue of the network is full; an event of the page goes away");
        free(block);
        return;
    }
    wasm_net_item *item = g_net_queue + (g_net_first + g_net_count++) % WASM_NET_QUEUE;
    *item = {};
    item->event.kind = (u32)kind;
    item->event.step = step;
    item->event.seat = (u8)seat;
    item->event.host = host != 0;
    item->event.size = (u32)size;
    item->event.data = block;
    item->event.text = text;
    item->block      = block;
}

internal PLATFORM_NET_POLL(wasm_net_poll)
{
    free(g_net_current);
    g_net_current = 0;
    if (!g_net_count) return false;
    wasm_net_item *item = g_net_queue + g_net_first;
    g_net_first = (g_net_first + 1) % WASM_NET_QUEUE;
    g_net_count--;
    *event        = item->event;
    g_net_current = item->block;
    return true;
}

internal PLATFORM_NET_SEND(wasm_net_send)
{
    EM_ASM({
        if (Module.hexdomNetSend)
            Module.hexdomNetSend($0, $1, $2 >>> 0, $3 >>> 0, $4, HEAPU8.slice($5, $5 + $6), $7 ? UTF8ToString($7) : "",
                                 $8 ? UTF8ToString($8) : "");
    }, event->kind, event->step, (u32)(event->checksum & 0xFFFFFFFFull), (u32)(event->checksum >> 32), event->acting,
       event->data, event->size, event->text, event->pack);
}

/* The bridge of the page into the queue. The page calls
   Module.hexdomNetPush(kind, step, seat, host, bytes, text). */
internal void
wasm_net_install(void)
{
    EM_ASM({
        Module.hexdomNetPush = function(kind, step, seat, host, bytes, text) {
            var data = bytes || new Uint8Array(0);
            var words = new TextEncoder().encode(text || "");
            var block = _wasm_net_alloc(data.length + words.length + 1);
            HEAPU8.set(data, block);
            HEAPU8.set(words, block + data.length);
            HEAPU8[block + data.length + words.length] = 0;
            _wasm_net_push(kind, step, seat, host ? 1 : 0, block, data.length, block + data.length);
        };
        if (Module.hexdomNetReady) Module.hexdomNetReady();
    });
}

/* ---- frame -------------------------------------------------------------- */

/* Logs a new peak use of a memory block when it grows by 8 MB or more. */
internal void
wasm_track_peak(const char *name, u64 used, u64 size, u64 *peak)
{
    if (used < *peak + Megabytes(8) && *peak) { if (used > *peak) *peak = used; return; }
    if (used <= *peak) return;
    *peak = used;
    char line[160];
    snprintf(line, sizeof(line), "platform: peak memory of the %s block: %.1f of %.0f MB", name, used / 1048576.0, size / 1048576.0);
    wasm_log(line);
}

internal void
wasm_frame(void)
{
    wasm_state *state = &g_wasm;
    f64 now = emscripten_get_now();
    game_input input = {};
    input.dt_seconds  = (f32)((now - state->last_time) / 1000.0);
    input.frame_index = state->frame_index;
    state->last_time  = now;

    glfwPollEvents();
    /* The canvas fills the page. When the page size changes, the GLFW window
       (and so the ImGui display and the scene viewport) takes the new size. */
    double css_width = 0.0, css_height = 0.0;
    int window_width = 0, window_height = 0;
    emscripten_get_element_css_size("#canvas", &css_width, &css_height);
    glfwGetWindowSize(state->window, &window_width, &window_height);
    if ((int)css_width > 0 && (int)css_height > 0 && ((int)css_width != window_width || (int)css_height != window_height))
    {
        glfwSetWindowSize(state->window, (int)css_width, (int)css_height);
    }
    wasm_images_begin_frame();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
#if HEXDOM_WEB_TEST
    wasm_test_begin_frame(state->imgui);
#endif
    ImGui::NewFrame();

    arena_reset(&state->frame);
    ImVec2 display = ImGui::GetIO().DisplaySize;
    render_commands *commands = render_commands_begin(&state->frame, display.x, display.y);
    input.render = commands;
    GameUpdateAndRender(&state->memory, &input, state->imgui, &state->frame);
    if (state->memory.quit_requested)   /* M9: 結束遊戲. The page asks the user, then signs out (web/shell.html). */
    {
        EM_ASM({
            if (Module.hexdomQuit) Module.hexdomQuit();
            else console.log("platform: the game asks to close; a web page stays open");
        });
    }

    ImGui::Render();
    int width = 0, height = 0;
    glfwGetFramebufferSize(state->window, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.09f, 0.10f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    webgl2_renderer_draw(&state->renderer, commands, &state->frame);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#if HEXDOM_WEB_TEST
    wasm_test_end_frame();
#endif

    wasm_track_peak("permanent", state->memory.permanent_used, state->memory.permanent_size, &state->peak_permanent);
    wasm_track_peak("transient", state->memory.transient_used, state->memory.transient_size, &state->peak_transient);
    wasm_track_peak("frame", state->frame.used, state->frame.size, &state->peak_frame);

    /* A new font size from the Settings panel: the atlas is built again
       between two frames, and the backend makes its font texture again. */
    u32 wanted = state->memory.font_pixels;
    if (wanted >= HEXDOM_FONT_MIN_PIXELS && wanted <= HEXDOM_FONT_MAX_PIXELS && wanted != state->font_pixels)
    {
        state->font_pixels = wanted;
        ImGui_ImplOpenGL3_DestroyFontsTexture();
        ImGui::GetIO().Fonts->Clear();
        wasm_log(ui_fonts_load(wasm_read_entire_file, HEXDOM_DATA_ROOT, HEXDOM_CONTENT_PACK, wanted).message);
        ImGui_ImplOpenGL3_CreateFontsTexture();
    }

    /* ImGui writes saves/imgui.ini by itself; this write keeps it. */
    if (state->frame_index % WASM_SYNC_FRAMES == WASM_SYNC_FRAMES - 1) wasm_sync_saves();
    state->frame_index++;
}

/* ---- start -------------------------------------------------------------- */

/* The second half of the start, after the first read of IndexedDB. */
extern "C" EMSCRIPTEN_KEEPALIVE void
wasm_start(void)
{
    wasm_state *state = &g_wasm;
    wasm_log("platform: the saves are read from IndexedDB");

    IMGUI_CHECKVERSION();
    ImGui::SetAllocatorFunctions(wasm_imgui_alloc, wasm_imgui_free, 0);
    state->imgui = ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = WASM_SAVE_FOLDER "/imgui.ini";
    ImGui::StyleColorsDark();

    if (!glfwInit()) { wasm_log("platform: GLFW does not start"); return; }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    int width  = EM_ASM_INT({ return window.innerWidth; });
    int height = EM_ASM_INT({ return window.innerHeight; });
    state->window = glfwCreateWindow(width, height, "hexdom", 0, 0);
    if (!state->window) { wasm_log("platform: the browser has no WebGL2"); return; }
    glfwMakeContextCurrent(state->window);
    ImGui_ImplGlfw_InitForOpenGL(state->window, true);
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(state->window, "#canvas");
    ImGui_ImplOpenGL3_Init("#version 300 es");

    /* The scene renderer: shaders, materials, models and the 3D models of
       the bases, from the content pack. */
    arena_init(&state->renderer_scratch, malloc((size_t)WASM_RENDERER_SCRATCH_SIZE), WASM_RENDERER_SCRATCH_SIZE);
    if (!webgl2_renderer_init(&state->renderer, wasm_read_entire_file, HEXDOM_DATA_ROOT "/" HEXDOM_CONTENT_PACK, &state->renderer_scratch))
    {
        wasm_log("platform: scene renderer failed to start, drawing panels only");
        wasm_log(state->renderer.last_error);
    }

    state->font_pixels = HEXDOM_FONT_DEFAULT_PIXELS;
    wasm_log(ui_fonts_load(wasm_read_entire_file, HEXDOM_DATA_ROOT, HEXDOM_CONTENT_PACK, state->font_pixels).message);

    game_memory *memory = &state->memory;
    memory->permanent_size = WASM_PERMANENT_SIZE;
    memory->transient_size = WASM_TRANSIENT_SIZE;
    memory->permanent      = calloc(1, (size_t)WASM_PERMANENT_SIZE);
    memory->transient      = calloc(1, (size_t)WASM_TRANSIENT_SIZE);
    void *frame_memory     = malloc((size_t)WASM_FRAME_SIZE);
    if (!memory->permanent || !memory->transient || !frame_memory)
    {
        wasm_log("platform: the browser gives no memory for the game");
        return;
    }
    arena_init(&state->frame, frame_memory, WASM_FRAME_SIZE);
    memory->platform.read_entire_file  = wasm_read_entire_file;
    memory->platform.write_entire_file = wasm_write_entire_file;
    memory->platform.log               = wasm_platform_log;
    memory->platform.imgui_alloc       = wasm_imgui_alloc;
    memory->platform.imgui_free        = wasm_imgui_free;
    memory->platform.imgui_alloc_user  = 0;

    char portrait_message[512];
    if (wasm_portraits_open(&g_wasm_portraits, HEXDOM_PORTRAIT_ARCHIVE, wasm_log, portrait_message, sizeof(portrait_message)))
    {
        memory->platform.portraits        = &g_wasm_portraits.list;
        memory->platform.portrait_texture = wasm_portrait_texture;
    }
    wasm_log(portrait_message);
    memory->platform.face_texture = wasm_face_texture;
    memory->platform.net_send     = wasm_net_send;   /* MP3: the socket of a shared game is in the page */
    memory->platform.net_poll     = wasm_net_poll;
    wasm_net_install();
    memory->platform.open_url     = wasm_open_url;

    state->last_time = emscripten_get_now();
    wasm_log("platform: the game starts");
    emscripten_set_main_loop(wasm_frame, 0, false);
}

int
main(void)
{
    wasm_log("platform: starting");
    /* /build is the folder of the executable; the pack is in /data. */
    mkdir("/build", 0777);
    chdir("/build");
    mkdir(WASM_SAVE_FOLDER, 0777);
    EM_ASM({
        FS.mount(IDBFS, {}, "/build/saves");
        FS.syncfs(true, function(error) {
            if (error) console.log("platform: IndexedDB does not read: " + error);
            /* The page reads the saves of the server first, if the server keeps them. */
            if (Module.hexdomPullSaves) Module.hexdomPullSaves(_wasm_start);
            else _wasm_start();
        });
    });
    return 0;
}
