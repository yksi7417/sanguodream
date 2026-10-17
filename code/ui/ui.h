#ifndef UI_H
#define UI_H
/* Debug UI. It drives the backend only through engine_api
   (code/engine/engine.h); it never sees sim_state, the ECS or the ledger.
   It gets its strings and names from the text module (code/text/text.h),
   a part of the front end. ui_state lives in the permanent block so it
   survives DLL reloads. bool rather than b32 because ImGui writes these. */

#include "base.h"
#include "hexdom.h"               /* the font size limits that the platform shares */
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

/* The 出征 and 輸送 dialogs (M5): the order that 決定 sends. */
struct ui_march_dialog
{
    u8  city;
    u8  prepared_city;                   /* ENGINE_NO_CITY: choose again */
    engine_sortie_order order;
    int soldiers, gold, grain;           /* ImGui writes ints */
    int cargo[4];                        /* 輸送: pikes, halberds, crossbows, war horses */
};

/* The unit commands of M6 that wait for a target hex on the map
   (ui/unit-commands.md). */
enum ui_pick
{
    UI_PICK_NONE,
    UI_PICK_ATTACK,      /* 攻擊 */
    UI_PICK_JOINT,       /* 齊攻 */
    UI_PICK_TACTIC,      /* 戰法 */
    UI_PICK_PLOT,        /* 計略 */
    UI_PICK_BUILD,       /* 設置 */
    UI_PICK_REPAIR,      /* 修補 */
    UI_PICK_DISMANTLE,   /* 撤除 */
    UI_PICK_DUEL,        /* 單挑 (M7) */
    UI_PICK_COUNT
};

/* ---- table views (ui_table.cpp) ---------------------------------------- */

#define UI_TABLE_FILTER_SIZE   48
#define UI_TABLE_FILTER_WIDTH  150.0f
#define UI_TABLE_MAX_COLUMNS   12
#define UI_SORT_TEXT_SIZE      32
#define UI_SORT_NO_COLUMN      -1
#define UI_LEDGER_VIEW_CAPACITY 65536   /* the ledger capacity of engine_default_config */

/* One cell of a row: its text for the filter, its number for the sort. */
struct ui_sort_key
{
    s64  number;
    b32  numeric;
    char text[UI_SORT_TEXT_SIZE];
};

/* The filter and the sort of one table, and the facts its order was built from. */
struct ui_table_view
{
    char filter[UI_TABLE_FILTER_SIZE];
    s32  sort_column;                 /* UI_SORT_NO_COLUMN: the source order */
    bool sort_descending;
    u32  count;                       /* the rows that pass the filter */
    bool built;
    char built_filter[UI_TABLE_FILTER_SIZE];
    char built_locale[TEXT_LOCALE_SIZE];
    s32  built_sort_column;
    bool built_sort_descending;
    bool built_raw;
    u8   built_force;
    u32  built_source_count;
    u32  built_turn;
    u32  built_ledger_count;
};

struct ui_state;

/* Fills one cell for each column of the row source, and returns the force
   of the row (ENGINE_NO_FACTION when the table has no force filter). */
typedef u8 ui_table_row_fn(ui_state *ui, engine_api *api, void *user, u32 source, ui_sort_key *cells);

/* The rows of one table for a build: the count, the cells, and the arrays
   of the view (rows: the display order; keys: the sort key by source index). */
struct ui_table_source
{
    u32              source_count;
    u32              column_count;
    u8               only_force;      /* ENGINE_NO_FACTION: every row */
    ui_table_row_fn *row;
    void            *user;
    u32             *rows;
    ui_sort_key     *keys;
    u32              capacity;
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
    bool show_all_cities;        /* the Cities panel and the City combo list every city, not only the player's */
    int  font_size;              /* pixels; the platform builds the font atlas again when it changes */
    struct
    {
        u8 selected;             /* the city id that the City panel shows */
    } city;
    /* the filter and the sort of the big tables (ui_table.cpp) */
    ui_table_view officers_view;
    ui_table_view cities_view;
    ui_table_view ledger_view;
    u32           officers_rows[SIM_OFFICER_CAPACITY];
    ui_sort_key   officers_keys[SIM_OFFICER_CAPACITY];
    u32           cities_rows[SIM_CITY_CAPACITY];
    ui_sort_key   cities_keys[SIM_CITY_CAPACITY];
    u32           ledger_rows[UI_LEDGER_VIEW_CAPACITY];
    ui_sort_key   ledger_keys[UI_LEDGER_VIEW_CAPACITY];
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
    /* M4: the 研究技巧 and 吸收合併 dialogs and the technique table (技巧表) */
    bool show_research;
    struct
    {
        u8  city;
        u8  prepared_city;               /* ENGINE_NO_CITY: choose again */
        u8  technique;                   /* ENGINE_NO_TECHNIQUE until a click */
        u16 officers[SIM_DEVELOP_OFFICERS];
    } research;
    bool show_merge;
    struct
    {
        u8  city;
        u8  prepared_city;
        u8  absorber;                    /* ENGINE_ANY_SLOT until a choice */
        u8  absorbed;
        u16 officer;
    } merge;
    bool show_techniques;
    /* M5: the 出征 and 輸送 dialogs, the selected unit, its menu and panel, the mini map */
    bool show_sortie;
    bool show_transport;
    ui_march_dialog sortie;
    ui_march_dialog transport;
    bool show_unit;
    bool show_unit_menu;
    bool show_minimap;
    struct
    {
        u32 selected;            /* ENGINE_NO_UNIT if none */
        u32 supply_target;       /* the unit that 補給 gives to */
        int supply_gold, supply_grain, supply_soldiers;
        char message[96];        /* the result of the last unit command */
    } units;
    /* M6: the unit command that waits for a target hex, and the list of
       tactics, plots or buildings that chooses its row */
    bool show_actions;
    struct
    {
        u8 pick;                 /* ui_pick: the command that the next map click issues */
        u8 row;                  /* the row of the tactic, plot or building of the pick */
        u8 list;                 /* ui_pick of the Actions list: UI_PICK_TACTIC, _PLOT or _BUILD */
    } combat;
    /* M7: the Court and Diplomacy panels, the duel and debate screens */
    bool show_court;
    bool show_diplomacy;
    bool show_duel;
    bool show_debate;
    struct
    {
        u16 officer;             /* the officer that the panel shows and that acts */
        u16 target;              /* recruit: the officer to recruit */
        u16 partner;             /* matchmake */
        u16 captive;             /* release */
        u8  post;                /* appoint */
        u8  item;                /* give */
        int match_kind;          /* sim_match_kind */
    } court;
    struct
    {
        u8  force;               /* the target faction */
        u8  second;              /* 二虎競食: the second faction */
        u8  city;                /* 驅虎吞狼, 流言: the target city */
        u16 captive;             /* 交換俘虜: the own officer that the target holds */
        u16 envoy;               /* the officer that goes */
        int gold;
        int months;
    } diplomacy;
    /* M8: the Ruler panel (軍團, 軍師, 評定), the Bases panel, and the event dialog */
    bool show_ruler;
    bool show_bases;
    struct
    {
        u16 viceroy;             /* 軍團: the 都督 of a new or changed corps */
        u64 cities;              /* bit c: the city c goes to the corps */
        u8  corps;               /* change, dissolve: the selected corps */
        int policy;              /* sim_corps_policy */
        u32 flags;               /* sim_corps_flag */
        u16 adviser;             /* 軍師: the officer to appoint */
    } ruler;
    /* M9: the game screens (ui/title-and-load.md, ui/screen-flow.md) */
    bool show_title;
    bool show_scenarios;
    bool show_rulers;
    bool show_start_settings;
    bool show_topbar;            /* the game mode: the top bar and the history window */
    bool show_history;
    bool show_saveload;
    bool show_options;
    bool show_new_officer;
    bool show_editor;
    bool show_battles;
    bool show_abilities;
    bool show_info;
    bool quit_requested;         /* 結束遊戲: the platform closes the window */
    struct
    {
        u64  rulers;             /* bit f: the player plays the faction f */
        bool saving;             /* the save/load screen saves */
        u32  page;               /* 0 .. 3: slots 1-8 .. 25-32 */
        u32  hovered;            /* the slot of the detail panel */
        u32  chosen;             /* the slot that waits for its comment; ENGINE_SAVE_SLOTS if none */
        char comment[ENGINE_SAVE_COMMENT_SIZE];
        char message[96];
        char family[32];         /* 新武將: UTF-8 */
        char given[32];
        bool female;
        int  type;               /* sim_officer_type */
        int  personality;
        int  birth_year;
        u8   city;
        bool found_force;
        u8   ancient;            /* SIM_NO_ANCIENT */
        u16  editor_officer;
        int  editor_loyalty, editor_merit, editor_technique, editor_gold, editor_order, editor_soldiers;
        int  editor_unit_soldiers, editor_unit_morale;
        u16  trainee;            /* 培育武將: the officer */
        engine_settings start;   /* the settings that the start screen edits; 決定 applies them */
        bool start_loaded;       /* start holds the settings of the engine */
    } game;
    u8   ledger_city;           /* the ledger filter: ENGINE_ANY_CITY or a city id */
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
        bool      focused;       /* the Hex map panel has the keyboard: W A S D scroll it, PageUp and PageDown zoom it */
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
