#ifndef UI_H
#define UI_H
/* Debug UI. It drives the backend only through engine_api
   (code/engine/engine.h); it never sees sim_state, the ECS or the ledger.
   It gets its strings and names from the text module (code/text/text.h),
   a part of the front end. ui_state lives in the permanent block so it
   survives DLL reloads. bool rather than b32 because ImGui writes these. */

#include "base.h"
#include "engine/engine.h"
#include "render/render.h"
#include "render/terrain_mesh.h"   /* terrain_level_y: the scene places models on the cell tops */
#include "text/text.h"

/* The five city command dialogs of M3 (ui/city-commands.md). */
enum ui_city_command
{
    UI_COMMAND_PATROL,      /* 巡察 */
    UI_COMMAND_CONSCRIPT,   /* 徵兵 */
    UI_COMMAND_TRAIN,       /* 訓練 */
    UI_COMMAND_MERCHANT,    /* 商人 */
    UI_COMMAND_PRODUCE,     /* 生產兵裝 */
    UI_COMMAND_COUNT
};

struct ui_command_dialog
{
    u8  city;                            /* the city of the dialog */
    u8  prepared_city;                   /* the city of the default choices; ENGINE_NO_CITY: choose again */
    u16 officers[SIM_DEVELOP_OFFICERS];  /* 執行武將 */
    u8  weapon;                          /* produce */
    int merchant_mode;                   /* merchant: sim_merchant_mode */
    int amount;                          /* merchant: the gold to spend or the grain to sell */
};

struct ui_state
{
    bool show_demo;
    bool show_sim;
    bool show_ledger;
    bool show_memory;
    bool show_map;
    bool show_officers;
    bool show_cities;
    bool show_scene;
    struct
    {
        bool      enabled;       /* the 2.5D map draws as the window background */
        bool      grid;
        bool      initialized;   /* the camera was centred on the map once */
        bool      has_hover;
        hex_coord hover;
        camera    cam;
    } scene;
    bool show_settings;
    bool show_city;
    bool show_develop;
    bool show_raw;               /* the id and code columns show the raw numbers, not the names */
    struct
    {
        u8 selected;             /* the city id that the City panel shows */
    } city;
    struct
    {
        u8   city;               /* the city of the dialog */
        u8   prepared_city;      /* the city of the default choices; ENGINE_NO_CITY: choose again */
        u8   facility;
        u8   slot;               /* ENGINE_ANY_SLOT: the first free slot */
        u16  officers[SIM_DEVELOP_OFFICERS];
        char message[96];        /* result of the last Execute click */
    } develop;
    bool show_economy;
    bool show_command[UI_COMMAND_COUNT];
    ui_command_dialog commands[UI_COMMAND_COUNT];
    char command_message[96];    /* result of the last 決定 of a city command; the City panel shows it */
    u8   ledger_city;            /* the ledger filter: ENGINE_ANY_CITY or a city id */
    u8   ledger_resource;        /* the ledger filter: ENGINE_ANY_RESOURCE or a sim_resource */
    text_catalog *text;      /* the UI strings and the names; owned by the caller */
    char pending_locale[TEXT_LOCALE_SIZE];   /* set by the Language combo; ui_draw loads it at the start of the next frame */
    char load_message[96];   /* result of the last "Load sample save" click */
    char names_message[96];  /* result of the last "Load city names" click */
    struct
    {
        bool      has_from;
        bool      has_to;
        hex_coord from;
        hex_coord to;
        f32       zoom;
    } map;
};

/* Per-frame facts that belong to the front end, not the engine. render and
   frame_arena are zero in the headless ui tests: the scene then pushes
   nothing and the panels still draw. */
struct ui_frame_info
{
    f32 dt_seconds;
    u32 dll_reload_count;
    u64 frame_arena_used;
    u64 frame_arena_size;
    render_commands *render;        /* this frame's command buffer, from the platform */
    memory_arena    *frame_arena;   /* for per-frame arrays such as the overlay colours */
};

/* The caller loads a locale into the catalog before the first frame. */
internal void ui_init(ui_state *ui, text_catalog *text);
internal void ui_draw(ui_state *ui, engine_api *api, ui_frame_info *frame);

#endif /* UI_H */
