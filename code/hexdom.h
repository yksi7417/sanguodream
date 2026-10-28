#ifndef HEXDOM_H
#define HEXDOM_H
/* The contract between the platform layer (win32_hexdom.cpp) and the game
   DLL (hexdom.cpp). The DLL owns no state outside game_memory, so it can be
   unloaded and reloaded while the game keeps running. */

#include "base.h"
#include "version.h"   /* HEXDOM_VERSION and HEXDOM_BUILD_TEXT for the title and the help window */

struct ImGuiContext;

/* The data folder and the content pack. A relative path is relative to the
   folder of the executable (build/), so "../data" is data/ in the repository.
   The DLL reads its text files there, and the platform layer reads the
   fonts in data/fonts. */
#define HEXDOM_DATA_ROOT    "../data"
#define HEXDOM_CONTENT_PACK "placeholder"

/* The font size in pixels. The UI keeps the setting; the platform layer
   builds the atlas at that size (ui/ui_fonts.h). */
#define HEXDOM_FONT_DEFAULT_PIXELS 19
#define HEXDOM_FONT_MIN_PIXELS     8
#define HEXDOM_FONT_MAX_PIXELS     32

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

/* The default portraits (docs/ASSETS.md): a zip archive of JPEG files. The
   path is relative to the folder of the executable. Each content pack uses
   these portraits. The archive is not in git. If the file is absent, the
   UI shows the first character of the name in place of a portrait. */
#define HEXDOM_PORTRAIT_ARCHIVE "../data/civil_war/Portraits.zip"

struct portrait_list;   /* render/portraits.h */

/* The texture of a picture of the portrait list, as an ImTextureID. The
   platform decodes a picture when the game first asks for it. Zero if the
   picture is not ready in this frame or does not decode. */
#define PLATFORM_PORTRAIT_TEXTURE(name) u64 name(u32 picture)
typedef PLATFORM_PORTRAIT_TEXTURE(platform_portrait_texture);

/* The texture of the generated face of an officer without a picture
   (render/faces.h), as an ImTextureID. key is the packed description from
   face_key_pack. The platform paints the face one time for each officer and
   keeps the texture by officer id. Zero if the face is not ready in this
   frame. */
#define PLATFORM_FACE_TEXTURE(name) u64 name(u32 officer_id, u32 key)
typedef PLATFORM_FACE_TEXTURE(platform_face_texture);

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
    const portrait_list       *portraits;        /* the default portraits; zero without the archive */
    platform_portrait_texture *portrait_texture;
    platform_face_texture     *face_texture;     /* the generated faces; zero without a device */
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
    b32   quit_requested;   /* M9: the game asks the platform to close the window (結束遊戲), for one frame */
    u32   font_pixels;      /* the font size that the UI wants; the platform builds the atlas again when it differs (0: the default) */
    u64   permanent_used;   /* the use of the two blocks after each frame. The platform logs */
    u64   transient_used;   /* the peak, to size the blocks of each platform (docs/WEB.md)  */
    char  content_pack[64]; /* "--pack NAME": the folder under HEXDOM_DATA_ROOT of the rules and the map. Empty: HEXDOM_CONTENT_PACK (docs/MAPS.md) */
};

#define GAME_UPDATE_AND_RENDER(name) void name(game_memory *memory, game_input *input, \
                                              ImGuiContext *imgui, memory_arena *frame)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

/* The DLL exports the entry point. With HEXDOM_STATIC_GAME, the platform
   layer includes hexdom.cpp in its own unit and calls the entry point
   directly. The web build does this, because it has no DLL and no hot
   reload. */
#if defined(HEXDOM_STATIC_GAME)
#define HEXDOM_EXPORT extern "C"
#elif defined(_WIN32)
#define HEXDOM_EXPORT extern "C" __declspec(dllexport)
#else
#define HEXDOM_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#endif /* HEXDOM_H */
