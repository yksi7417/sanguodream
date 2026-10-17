#ifndef HEXDOM_H
#define HEXDOM_H
/* The contract between the platform layer (win32_hexdom.cpp) and the game
   DLL (hexdom.cpp). The DLL owns no state outside game_memory, so it can be
   unloaded and reloaded while the game keeps running. */

#include "base.h"

struct ImGuiContext;

/* The data folder and the content pack. A relative path is relative to the
   folder of the executable (build\), so "..\data" is data\ in the repository.
   The DLL reads its text files there, and the platform layer reads the
   fonts in data\fonts. */
#define HEXDOM_DATA_ROOT    "..\\data"
#define HEXDOM_CONTENT_PACK "placeholder"

struct read_file_result
{
    u64   size;
    void *contents;   /* pushed into the arena the caller supplied; zero on failure */
};

#define PLATFORM_READ_ENTIRE_FILE(name) read_file_result name(const char *path, memory_arena *arena)
typedef PLATFORM_READ_ENTIRE_FILE(platform_read_entire_file);

/* Writes the whole file; creates its folder. False if the file is not written. */
#define PLATFORM_WRITE_ENTIRE_FILE(name) b32 name(const char *path, const void *data, u64 size)
typedef PLATFORM_WRITE_ENTIRE_FILE(platform_write_entire_file);

#define PLATFORM_LOG(name) void name(const char *message)
typedef PLATFORM_LOG(platform_log);

typedef void *imgui_alloc_fn(size_t size, void *user);
typedef void  imgui_free_fn(void *ptr, void *user);

struct platform_api
{
    platform_read_entire_file *read_entire_file;
    platform_write_entire_file *write_entire_file;   /* M9: the save slots */
    platform_log              *log;
    imgui_alloc_fn            *imgui_alloc;      /* ImGui allocates through the exe's */
    imgui_free_fn             *imgui_free;       /* heap so a DLL reload never frees   */
    void                      *imgui_alloc_user; /* memory the exe still owns          */
};

struct render_commands;

struct game_input
{
    f32 dt_seconds;
    u32 frame_index;
    b32 dll_reloaded;   /* true for exactly one frame after a hot reload */
    struct render_commands *render;   /* this frame's scene commands, made by the platform in the frame arena; the DLL fills it */
};

struct game_memory
{
    b32   is_initialized;
    u64   permanent_size;
    void *permanent;    /* game_state, sim_state, ecs tables, loaded scenario */
    u64   transient_size;
    void *transient;    /* per-session scratch */
    platform_api platform;
};

#define GAME_UPDATE_AND_RENDER(name) void name(game_memory *memory, game_input *input, \
                                              ImGuiContext *imgui, memory_arena *frame)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#endif /* HEXDOM_H */
