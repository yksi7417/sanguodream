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

/* The splash art of the start (docs/ASSETS.md, "The splash art"). The
   file is HEXDOM_SPLASH_FILE in the folder of the default pack. The
   platform loads it one time, at the start. */
#define HEXDOM_SPLASH_FILE "sprites/art/splash.png"

/* The progress bar of the splash, as fractions of the width and the
   height of the picture. The picture has the track of the bar at this
   place (misc/make_splash.py). The platform draws the fill while it
   loads, and the UI draws the full bar after the load (ui_splash.cpp).
   web/shell.html uses the same numbers. */
#define HEXDOM_SPLASH_BAR_LEFT   0.30f
#define HEXDOM_SPLASH_BAR_RIGHT  0.70f
#define HEXDOM_SPLASH_BAR_TOP    0.65f
#define HEXDOM_SPLASH_BAR_BOTTOM 0.68f
#define HEXDOM_SPLASH_HINT_TOP   0.70f   /* the top of the hint under the bar */

struct platform_picture
{
    u64 texture;   /* an ImTextureID; zero if the file is absent or does not decode */
    f32 aspect;    /* the height of the picture divided by its width */
};

#define PLATFORM_SPLASH_PICTURE(name) platform_picture name(void)
typedef PLATFORM_SPLASH_PICTURE(platform_splash_picture);

/* docs/EUROPE.md, E5: a PNG file under the data folder, for example the
   coat of arms "medieval_europe/arms/france.png" (text_key_arms). The
   platform loads a file one time, when the game first asks for it, and
   keeps at most PLATFORM_DATA_PICTURE_CAPACITY files. It refuses a path
   that text_arms_file_is_allowed refuses. The texture is zero if the file
   is absent or does not decode. */
#define PLATFORM_DATA_PICTURE_CAPACITY 64
#define PLATFORM_DATA_PICTURE(name) platform_picture name(const char *file)
typedef PLATFORM_DATA_PICTURE(platform_data_picture);

/* docs/EUROPE.md, E4: opens a link of the English Wikipedia. The platform
   opens it in the default browser, or in a new tab on the web. The platform
   refuses a URL for which text_link_is_allowed (text/text.h) gives false.
   False if it does not open the link. */
#define PLATFORM_OPEN_URL(name) b32 name(const char *url)
typedef PLATFORM_OPEN_URL(platform_open_url);

typedef void *imgui_alloc_fn(size_t size, void *user);
typedef void  imgui_free_fn(void *ptr, void *user);

/* A shared game (docs/MULTIPLAYER.md, MP3). The web page owns the lobby and
   the socket of the room. The game sends its steps and checksums to the
   page, and it takes the events of the page one at a time. A platform
   without a network leaves net_send and net_poll zero. */
enum platform_net_kind
{
    PLATFORM_NET_NONE,
    /* from the game to the page */
    PLATFORM_NET_HOST,      /* data: the start image. text: one line "force<TAB>name" for each seat. pack: the content pack */
    PLATFORM_NET_STEP,      /* step: the number. data: the bytes of the engine */
    PLATFORM_NET_CHECKSUM,  /* step, checksum and acting: the state after the step; step -1 is the start */
    /* from the page to the game */
    PLATFORM_NET_JOIN,      /* data: the start image. seat, host. text: one line "force<TAB>player" for each seat.
                               step: 0, or the first step after a snapshot (MP5): data is then that snapshot */
    PLATFORM_NET_SEATS,     /* text: one line "force<TAB>player<TAB>online" for each seat with a player */
    PLATFORM_NET_DESYNC,    /* step; -1: the room decided the desync, and the game goes on (MP6) */
    PLATFORM_NET_ERROR,     /* text: the reason */
    PLATFORM_NET_TIMER,     /* MP4. acting: the faction that acts. step: the seconds left; -1 if the timer stops */
    PLATFORM_NET_PACK,      /* MP4. text: the content pack of the room. It comes before the join */
    PLATFORM_NET_SNAPSHOT,  /* MP5, to the page. data: the image of the state after step. checksum: the state */
    PLATFORM_NET_OPEN_ROOM, /* MP5, to the page. text: the room code of the title menu */
    PLATFORM_NET_STATE_IMAGE, /* MP6, to the page. data: the image of the state after the step of a desync */
    PLATFORM_NET_CHAT,      /* MP7. To the page, text: a line of the player. From the page, text: "name<TAB>line" */
    PLATFORM_NET_KIND_COUNT
};

#define PLATFORM_NET_NO_SEAT 0xFF

struct platform_net_event
{
    u32         kind;       /* platform_net_kind */
    s32         step;
    u64         checksum;
    u8          acting;
    u8          seat;       /* PLATFORM_NET_NO_SEAT for a spectator */
    b32         host;
    u32         size;       /* the bytes of data */
    const u8   *data;       /* an event of the page: valid until the next net_poll */
    const char *text;
    const char *pack;       /* PLATFORM_NET_HOST: the content pack of the game */
};

#define PLATFORM_NET_SEND(name) void name(const platform_net_event *event)
typedef PLATFORM_NET_SEND(platform_net_send);
#define PLATFORM_NET_POLL(name) b32 name(platform_net_event *event)
typedef PLATFORM_NET_POLL(platform_net_poll);

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
    platform_splash_picture   *splash_picture;   /* the splash art; zero without a device */
    platform_data_picture     *data_picture;     /* E5: the coats of arms; zero without a device */
    platform_net_send        *net_send;         /* MP3: zero without a network (the Windows build) */
    platform_net_poll         *net_poll;
    platform_open_url         *open_url;         /* E4: the Wikipedia button of the officer panel */
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
    char  art_pack[64];     /* the game writes it: the folder under HEXDOM_DATA_ROOT of the art of the active pack. The
                               renderer reads a file there first, then in HEXDOM_CONTENT_PACK. Empty: the art of
                               HEXDOM_CONTENT_PACK alone (docs/ASSETS.md, "The art of a pack") */
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
