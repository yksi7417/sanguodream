#ifndef UI_H
#define UI_H
/* Debug UI. It drives the backend only through engine_api
   (code/engine/engine.h); it never sees sim_state, the ECS or the ledger.
   It gets its strings and names from the text module (code/text/text.h),
   a part of the front end. ui_state lives in the permanent block so it
   survives DLL reloads. bool rather than b32 because ImGui writes these. */

#include "base.h"
#include "hexdom.h"               /* the font size limits that the platform shares */
#include "render/portraits.h"     /* the default portraits: the picture of an officer */
#include "render/faces.h"         /* the generated face of an officer without a picture */
#include "engine/engine.h"
#include "render/render.h"
#include "render/terrain_mesh.h"   /* terrain_level_y: the scene places models on the cell tops */
#include "text/text.h"

/* The dialogs on demand of the game mode (ui_context.cpp): only one is open. */
enum ui_context_kind
{
    UI_CONTEXT_NONE,
    UI_CONTEXT_CITY,          /* the City panel and the command dialogs that it opens */
    UI_CONTEXT_UNIT,          /* the Unit panel and the unit command menu */
    UI_CONTEXT_MAP_MENU,      /* a right click on the empty map: 進行, 情報, 儲存, 載入 */
    UI_CONTEXT_SYSTEM_MENU,   /* the settings button of the HUD */
    UI_CONTEXT_HEX,           /* the Hex info box of one hex */
    UI_CONTEXT_INFO,          /* 情報: the faction information */
    UI_CONTEXT_FULL_MAP,      /* 中地圖: the Hex map panel of the whole map */
    UI_CONTEXT_SAVELOAD,
    UI_CONTEXT_SETTINGS,
    UI_CONTEXT_OPTIONS,
    UI_CONTEXT_HELP,          /* M22: the help window of the "?" button */
    UI_CONTEXT_EDITOR,        /* M22: 編輯 of the system menu */
    UI_CONTEXT_UNIT_POPUP,    /* M22: a right click on an own unit: 詳細情報, 所屬武將 */
    UI_CONTEXT_COUNT
};

/* The target menu of a selected unit after a click out of its range
   (clone gap report 2: G2-052 on an enemy unit, the follow order on an
   own unit, G2-058 on a far hex). */
enum ui_target_kind { UI_TARGET_NONE, UI_TARGET_ENEMY, UI_TARGET_FOLLOW, UI_TARGET_FAR, UI_TARGET_FACILITY, UI_TARGET_BASE };   /* UI_TARGET_FACILITY: G3-020, the question before an attack order on a facility; UI_TARGET_BASE: the question before the entry of an own port or pass */
#define UI_PLATE_CAPACITY 128   /* the unit plates of one frame that a click can hit (G2-042) */

/* M22, screen-flow.md, rule 7: the pitch of the rows of a list, of the
   items of a menu, and of the tabs. */
#define UI_LIST_PITCH  20.0f
#define UI_MENU_PITCH  34.0f
#define UI_TAB_PITCH   76.0f

/* The tabs of the list panel of the HUD (main-map.md, "List panel"). */
enum ui_hud_tab { UI_HUD_TAB_BASES, UI_HUD_TAB_OFFICERS, UI_HUD_TAB_UNITS, UI_HUD_TAB_COUNT };

/* M17: the times of the first screens (ui/title-and-load.md). The capture
   gives "approximately" for both. */
#define UI_TITLE_SELECT_SECONDS    1.0f   /* the rider after a click on a title button */
#define UI_LOAD_TRANSITION_SECONDS 4.0f   /* the dark landscape while a save loads */
#define UI_SPLASH_SECONDS          3.0f   /* the splash art before the title menu; a click or a key closes it before */
#define UI_SPLASH_LONGEST_STEP     0.1f   /* one frame adds at most this time to the splash, also the long frame after the start */
#define UI_MESSAGE_CAPACITY        32     /* the messages of one turn start; the log keeps the rest */
#define UI_CAPTIVE_QUEUE           16            /* the captives of one battle that wait for the dialog 俘虜處置 */
#define UI_MESSAGE_HEADING         0xFFFFFFFFu   /* a message slot that holds the period heading */
#define UI_MESSAGE_THREAT          0xFFFFFFFEu   /* G2-006, G2-007: the adviser names the enemy city that threatens an own city */
#define UI_MESSAGE_NEXT_ORDER      0xFFFFFFFDu   /* G2-007: the adviser asks for the next order; the first turn says that it is our turn */
#define UI_MESSAGE_UNIT_THREAT     0xFFFF0000u   /* G3-009: UI_MESSAGE_UNIT_THREAT | unit id: an enemy unit marches on an own city */
#define UI_MESSAGE_UNIT_THREAT_MASK 0xFFFF0000u
#define UI_MESSAGE_WIDTH           689.0f /* main-map.md, "Message window": the frame at the bottom centre */
#define UI_MESSAGE_HEIGHT          197.0f
#define UI_MESSAGE_MARGIN          12.0f
#define UI_FEEL_PASS_LIMIT         300    /* the AI turns that one 進行 can end before a player acts */
#define UI_REPLAY_EVENT_LINES      256    /* the log lines of one step that the Replay panel shows */
/* M18: the feedback of a command (ui_report.cpp; main-map.md, "Bottom-left
   panel", "Speech bubble", "Floating value"). */
#define UI_LOG_CAPACITY            4096   /* the lines of the message log; the oldest go first */
#define UI_LOG_HEADING             0x80000000u   /* a log line that holds the heading of a turn: | the turn */
#define UI_LOG_WIDTH               442.0f /* main-map.md: 4, 516, 442, 497 */
#define UI_LOG_HEIGHT_SHARE        0.34f  /* of the height of the window; the HUD list above it is taller than in the original */
#define UI_LOG_MARGIN              4.0f
#define UI_BUBBLE_SECONDS          3.0f   /* the speech bubble closes by itself after this time */
#define UI_BUBBLE_WIDTH            294.0f /* main-map.md: 819, 521, 294, 74 */
#define UI_FLOAT_SECONDS           1.6f   /* a value rises over the base for this time */
#define UI_FLOAT_RISE              40.0f  /* pixels */
#define UI_FLOAT_CAPACITY          8
/* The feedback of war (ui_war.cpp): the walk of a unit of the player, the
   strike effect on a target hex and the hits (the values over a unit). */
#define UI_WALK_CAPACITY           64     /* the hexes of one walk, with the start hex */
#define UI_WALK_HEX_SECONDS        0.22f  /* the pause on each hex of a walk */
#define UI_STRIKE_CAPACITY         4
#define UI_STRIKE_SECONDS          2.5f   /* a strike effect shows this long */
#define UI_HIT_CAPACITY            16
#define UI_HIT_FLOAT_SECONDS       2.2f   /* a value rises over the unit this long */
#define UI_HIT_MARK_SECONDS        2.0f   /* then the plate marks the changed value this long */
enum ui_hit_kind    { UI_HIT_SOLDIERS, UI_HIT_MORALE };
enum ui_strike_kind { UI_STRIKE_ATTACK, UI_STRIKE_TACTIC, UI_STRIKE_PLOT, UI_STRIKE_KIND_COUNT };
/* M19: the command dialogs of the game mode (ui/city-commands.md). */
#define UI_DIALOG_DIM_ALPHA        110    /* the dark map behind a dialog, of 255 */
#define UI_SLOT_PORTRAIT_LINES     4.0f   /* the height of a portrait slot, in text lines */

/* The title button that the player clicked (title-and-load.md, "Title menu"). */
enum ui_title_choice
{
    UI_TITLE_NONE,
    UI_TITLE_LOAD,
    UI_TITLE_NEW,
    UI_TITLE_BATTLE,
    UI_TITLE_OPTIONS,
    UI_TITLE_QUIT
};

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

#define UI_ROUND_FACTION_CAPACITY 64    /* M20: the AI factions of one round that the tests read */
#define UI_ROUND_SHOWN_CAPACITY   256   /* M20: the values of one round that the tests read */

#define UI_PICKER_FOR_DEVELOP UI_COMMAND_COUNT   /* the target of the officer selection: the 開發 dialog */
#define UI_PICKER_CAPACITY    64                 /* the free officers that the officer selection lists */

struct ui_command_dialog
{
    u8  city;                            /* the city of the dialog */
    u8  prepared_city;                   /* the city of the default choices; ENGINE_NO_CITY: choose again */
    u16 officers[SIM_DEVELOP_OFFICERS];  /* 執行武將 */
    u8  weapon;                          /* produce */
    int merchant_mode;                   /* merchant: sim_merchant_mode */
    int amount;                          /* merchant: the gold to spend or the grain to sell */
    int trade;                           /* merchant: the slider, below 0 a sale of grain, above 0 a purchase with gold (G2-031) */
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

/* The screen size when a window of the game mode last went to its corner
   (ui_corner_cond). */
struct ui_corner
{
    f32 width, height;
};

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

#define UI_NET_SEATS     8    /* the human seats of a shared game (docs/MULTIPLAYER.md, Q-1) */
#define UI_NET_NAME_SIZE 64
#define UI_NET_CHAT_LINES 32  /* MP7: the lines of the chat that the panel keeps */
#define UI_NET_CHAT_SIZE  384 /* a name, a tab and a line of at most 300 characters of the room (web/rooms.py) */

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
        /* M11 */
        bool      has_pick;      /* a left click picked a hex: the Hex info box shows it */
        hex_coord pick;
        u8        framed_city;   /* the last city that the camera framed; a new ui->city.selected frames again */
        u32       framed_unit;
        bool      frame_city;    /* a list asked to frame ui->city.selected, also when it did not change */
        bool      frame_unit;    /* a list asked to frame ui->units.selected */
        bool      headless;      /* the scene input, the labels and the frames run without a render target (the M11 ui tests) */
    } scene;
    bool show_units;             /* M11: the Units panel, a list of the units to frame */
    bool show_settings;
    bool show_credits;           /* the Credits window, a second window of the Settings dialog */
    bool show_city;
    bool show_develop;
    bool show_raw;               /* the id and code columns show the raw numbers, not the names */
    bool show_all_cities;        /* the Cities panel and the City combo list every city, not only the player's */
    int  font_size;              /* pixels; the platform builds the font atlas again when it changes */
    struct
    {
        u8 selected;             /* the city id that the City panel shows */
        u8 menu;                 /* M14: the open submenu of the base command menu (ui_base_menu) */
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
    ui_corner minimap_corner;    /* the minimap goes back to the bottom right after a change of the screen size */
    struct
    {
        u32 selected;            /* ENGINE_NO_UNIT if none */
        u32 supply_target;       /* the unit that 補給 gives to */
        int supply_gold, supply_grain, supply_soldiers;
        char message[96];        /* the result of the last unit command */
        u8  target_kind;         /* ui_target_kind: the menu at target_hex (G2-052, G2-058) */
        hex_coord target_hex;
        u32 target_unit;         /* the unit at target_hex, or ENGINE_NO_UNIT */
        b32 has_pending_move;    /* G2-045: the command menu opened at pending_move; the unit moves there with the choice */
        hex_coord pending_move;
        struct { u32 unit; f32 x0, y0, x1, y1; } plates[UI_PLATE_CAPACITY];   /* the plate rectangles of the last frame (G2-042) */
        u32 plate_count;
    } units;
    /* M6: the unit command that waits for a target hex, and the list of
       tactics, plots or buildings that chooses its row */
    bool show_actions;
    struct
    {
        u8 pick;                 /* ui_pick: the command that the next map click issues */
        u8 row;                  /* the row of the tactic, plot or building of the pick */
        u8 list;                 /* ui_pick of the Actions list: UI_PICK_TACTIC, _PLOT or _BUILD */
        bool      duel_pick;     /* G3-008: the list of the officers of the unit with 成功率, after the click on the target of 單挑 */
        hex_coord duel_hex;      /* the target of that 單挑 */
    } combat;
    /* 俘虜處置 (ui/city-commands.md): after a battle, one dialog for each
       officer that the player captured, with 登庸, 俘虜, 釋放 and 處決 */
    bool show_captive;
    struct
    {
        u16  queue[UI_CAPTIVE_QUEUE];   /* the captives that wait for a choice; [0] shows */
        u32  count;
        u32  ledger_mark;        /* the ledger count that the queue has read */
        bool recruit_failed;     /* 登庸 failed on the captive that shows: the button is grey */
    } captive;
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
    /* The dialogs of group 3 of the clone gap report 2 (ui_people.cpp):
       移動, 召喚, 官職, 流放, and the plots with a target base list. */
    bool show_move_officers, show_summon, show_posts, show_exile, show_plot;
    struct
    {
        u8  city;                /* the city of the command */
        u8  target;              /* 移動: the own city to go to; ENGINE_NO_CITY */
        u16 officers[SIM_MOVE_OFFICERS];
        u32 count;
    } move, summon;
    struct
    {
        u8  post;                /* the selected row of the post table; ENGINE_NO_ROW */
        u16 officer;             /* the selected officer without a post; ENGINE_NO_OFFICER */
        u16 staged_officer[32];  /* the changes before 決定 */
        u8  staged_post[32];     /* ENGINE_NO_ROW: a dismissal */
        u32 staged_count;
    } posts;
    struct { u16 officer; } exile;
    /* 褒賞 (G2-040, ui_court.cpp): the checked officers of the force */
    bool show_reward;
    struct
    {
        u8  city;                            /* the city that pays */
        u16 checked[SIM_REWARD_OFFICERS];
        u32 count;
        b32 fill;                            /* true after the open: the first draw checks every row */
    } reward;
    struct
    {
        u8  city;
        u8  kind;                /* sim_faction_plot_kind */
        u8  target_city;         /* ENGINE_NO_CITY until a click */
        u16 officer;             /* ENGINE_NO_OFFICER until a click */
    } plot;
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
    /* The splash art before the title menu (ui_splash.cpp). */
    bool show_splash;
    f32  splash_seconds;         /* the time since the splash opened */
    /* M9: the game screens (ui/title-and-load.md, ui/screen-flow.md) */
    bool show_title;
    bool show_scenarios;
    bool show_rulers;
    bool show_start_settings;
    bool show_story;             /* E7: the pages of the story mode, from the list of the rulers */
    struct
    {
        u32 page;                /* the page that the panel shows; 0 is Europe in 1200 */
    } story;
    bool show_topbar;            /* the game mode: the top bar and the history window */
    bool show_history;
    bool show_saveload;
    bool show_options;
    bool show_new_officer;
    bool show_editor;
    bool show_battles;
    bool show_abilities;
    bool show_info;
    bool show_help;              /* M22: the help window of the "?" button */
    bool show_fidelity;         /* M13: the replay of the save pairs of the original */
    struct
    {
        bool                 has_report;   /* a replay ran from the panel */
        u32                  pair;         /* the row of the last replay */
        engine_replay_report report;
    } fidelity;
    bool show_replay;            /* the replay of the game, step by step (ui_game_replay.cpp) */
    struct
    {
        bool viewing;            /* the engine shows a past step: the updates of the turn feedback wait */
        bool matches;            /* the last view has the checksum of the live game */
        u32  position;           /* the steps that the view applied; the step count when live */
        int  turn;               /* the turn of the jump */
    } replay;
    bool show_ai;                /* M15: the AI view: the options of the last AI turn of a faction */
    struct
    {
        u8 faction;              /* the faction of the view; ENGINE_NO_FACTION takes the first AI faction */
    } ai;
    bool show_content;           /* M16: the install folder and the source of each asset */
    struct
    {
        char                  folder[ENGINE_PATH_SIZE];   /* the folder that the player types */
        bool                  has_result;
        engine_install_result result;                     /* the answer of the last Set or Clear */
    } content;
    /* M17: the feel of the first hour. With original_timing off, each
       screen opens at once, as in M9 to M16. With turn_messages off, no
       message window opens. The game exe turns both on. */
    struct
    {
        bool original_timing;
        bool turn_messages;
        u32  title_choice;        /* ui_title_choice: the clicked button while the rider runs */
        f32  title_seconds;       /* the time since the click */
        f32  transition_seconds;  /* above 0: the load transition shows for this time */
        bool seen_turn;           /* turn and force hold the last turn start of a player */
        u32  turn;
        u8   force;
        u32  ledger_mark;         /* the ledger count at the last turn start */
        u32  messages[UI_MESSAGE_CAPACITY];   /* ledger indices, or UI_MESSAGE_HEADING */
        u32  message_count;
        u32  message_shown;       /* the message on the screen; message_count closes the window */
        bool first_turn;          /* G2-006: the first turn start after the game opened: the camera goes to the capital, 可終於輪到我們了啊 */
        bool loaded_game;         /* G3-023: the game came from a save: the first line is 劉表，可終於輪到你了啊 with no portrait */
        u8   threat_city;         /* G2-007: the enemy city of the threat message; ENGINE_NO_CITY if none */
        u32  framed_message;      /* the message that moved the camera; U32_MAX for none */
    } feel;
    /* M18: the words of the events and the feedback of a command
       (ui_report.cpp). The provider is a plug-in: ui_set_text_provider
       replaces the template provider. */
    struct
    {
        text_provider provider;
        bool show_log;            /* the message log at the bottom left of the game mode */
        bool mine_only;           /* the log shows only the events of the player */
        u32  ledger_mark;         /* the ledger count that the log has read */
        u32  lines[UI_LOG_CAPACITY];    /* ledger indices, or UI_LOG_HEADING | turn */
        s64  values[UI_LOG_CAPACITY];   /* the value after the event, or TEXT_NO_VALUE */
        engine_place places[UI_LOG_CAPACITY];   /* the place of the event when it came in; a click frames its hex */
        ui_corner corner;         /* the screen size of the last corner place of the log */
        u32  line_count;          /* the lines in the ring */
        u32  lines_seen;          /* G2-019: the line_count that the log drew last; a new line scrolls to the end */
        u32  line_first;          /* the place of the oldest line */
        bool has_heading;
        u32  heading_turn;        /* the turn of the last heading */
        /* the speech bubble over the base of the last command of a player */
        u32  bubble_event;        /* the ledger index; ENGINE_NO_EVENT if none */
        u8   bubble_city;
        f32  bubble_seconds;      /* above 0 while the bubble shows */
        s64  bubble_value;
        /* the values that rise over the base after a command */
        struct
        {
            u8  city;
            s32 delta;
            u8  sign;             /* engine_event_sign */
            f32 seconds;
            u8  resource;         /* sim_resource: the field of the value; the label names it */
            const char *resource_key;   /* "resource.<key>" is its text id */
        } floats[UI_FLOAT_CAPACITY];
        u32  float_count;
        u32  feedback_count;      /* the bubbles that the UI opened; the tests read it */
    } report;
    /* M19: the officer selection window 選擇執行武將 (ui_officer_pick.cpp).
       It lists the free officers of the city of a dialog. 決定 writes the
       checked officers into the slots of that dialog. */
    bool show_picker;
    struct
    {
        u8            target;        /* a ui_city_command, or UI_PICKER_FOR_DEVELOP */
        u8            city;
        u8            places;        /* the officers that the command takes */
        u16           checked[SIM_DEVELOP_OFFICERS];
        u32           checked_count;
        u16           ids[UI_PICKER_CAPACITY];   /* the free officers at the open */
        u32           id_count;
        u16           hovered;       /* the officer of the portrait at the left */
        ui_table_view view;
        u32           rows[UI_PICKER_CAPACITY];
        ui_sort_key   keys[UI_PICKER_CAPACITY];
    } picker;
    /* M19: the placement mode after 開發 in the game mode (ui_placement.cpp).
       The next left click on an empty slot of the city sends the command. */
    struct
    {
        bool active;
        u8   city;
        u8   facility;
        u8   slot;                   /* the slot that the 場所 arrows chose; ENGINE_ANY_SLOT for none */
        u16  officers[SIM_DEVELOP_OFFICERS];
        bool hover_valid;            /* the hex under the mouse is an empty slot of the city */
    } placement;
    /* M20: the show of the AI round after 進行 (ui_round.cpp). The setting
       is off by default (the decision of the player, 2026-10-20). */
    struct
    {
        bool      show;              /* the setting of the settings screen */
        bool      playing;           /* the events of a round play now */
        bool      confirm;           /* M22: 進行 waits for 是 or 否 (units that have not acted) */
        u32       next;              /* the next ledger index of the round */
        u32       end;               /* the ledger count: the end of the round */
        u8        force;             /* the AI faction that the HUD names; ENGINE_NO_FACTION */
        f32       wait;              /* the seconds until the next step */
        u8        named[UI_ROUND_FACTION_CAPACITY];   /* the factions that the HUD named, in order */
        u32       named_count;
        u32       shown[UI_ROUND_SHOWN_CAPACITY];     /* the ledger indices of the values, in order */
        u32       shown_count;
        hex_coord before[SIM_UNIT_CAPACITY];          /* the hex of each unit before its move of the round */
        bool      known[SIM_UNIT_CAPACITY];
        u32       move_unit;         /* the unit that walks now; ENGINE_NO_UNIT */
        hex_coord move_from, move_to;
        u32       move_step, move_steps;
    } round;
    /* M21: the screens of the information menu (ui_information.cpp). */
    struct
    {
        u8            screen;        /* ui_info_screen; UI_INFO_NONE: closed */
        bool          own;           /* only the faction of the player */
        int           tab;           /* the view tab of the table */
        bool          detail;        /* the detail panel of detail_id shows */
        u32           detail_id;     /* an officer id or a city id */
        u8            faction;       /* 勢力情報: the faction that shows */
        u32           unit_filter;   /* M22: 所屬武將 of a unit; ENGINE_NO_UNIT for all */
        u16           ids[SIM_OFFICER_CAPACITY];   /* the officer or city ids of the rows */
        u32           id_count;
        ui_table_view view;
        u32           rows[SIM_OFFICER_CAPACITY];
        ui_sort_key   keys[SIM_OFFICER_CAPACITY];
    } info;
    /* M21: the political map 中地圖 (ui_political.cpp). */
    bool show_political;
    struct
    {
        int  background;             /* ui_political_background */
        bool cities, bases, units;   /* 顯示 */
    } political;
    bool quit_requested;       /* 結束遊戲: the platform closes the window */
    /* The feedback of war (ui_war.cpp). The walk shows a move of the
       player hex by hex. A strike shows an attack, a tactic or a plot on
       its target hex. A hit is a change of the soldiers or the morale of
       a unit: the value rises over the unit, then the plate marks it. */
    struct
    {
        struct
        {
            u32       unit;      /* the unit that walks; ENGINE_NO_UNIT */
            hex_coord cells[UI_WALK_CAPACITY];   /* [0] the start hex */
            u32       count;
            u32       step;      /* the hex that the unit stands on now */
            f32       wait;      /* the seconds until the next step */
            bool      planned;   /* a plan waits for the result of its command_move */
            bool      open_menu; /* the command menu opens when the walk ends */
            bool      has_strike; /* G2-045: the strike of the action after the move plays when the walk ends */
            u8        strike_kind;
            hex_coord strike_hex;
        } walk;
        struct { u8 kind; hex_coord hex; f32 seconds; } strikes[UI_STRIKE_CAPACITY];   /* ui_strike_kind */
        u32 strike_count;
        struct { u32 unit; u8 kind; s32 delta; f32 seconds; } hits[UI_HIT_CAPACITY];   /* ui_hit_kind */
        u32 hit_count;
        u32 ledger_mark;         /* the ledger count that the hits have read */
    } war;
    /* The default portraits and the generated faces of this frame
       (ui_frame_info). Without both, the panels show the first character
       of the name in place of a portrait. */
    const portrait_list       *portraits;
    platform_portrait_texture *portrait_texture;
    platform_face_texture     *face_texture;
    platform_splash_picture   *splash_picture;   /* the splash art; zero in the headless tests */
    platform_open_url         *open_url;   /* E4: the Wikipedia button of the officer panel; zero in the headless tests */
    /* The game mode (main-map.md): the HUD at the top left with the list of
       the bases, officers and units, the mini map at the bottom right, and
       one dialog on demand at the entity that the player clicks. */
    bool hud_mode;
    bool show_debug_panels;      /* in the game mode: the debug menu bar and tools (F12) */
    int  hud_tab;                /* ui_hud_tab */
    struct
    {
        u8        kind;          /* ui_context_kind */
        f32       x, y;          /* the anchor: the cursor or the button that opened the dialog */
        int       place_frame;   /* the ImGui frame of the open; the windows move to the anchor after it */
        bool      hovered;       /* the mouse is over a window of the dialog in this frame */
        bool      consumed;      /* this right click closed the dialog: the scene does not use it */
        u32       ledger_mark;   /* the ledger count at the open: a new event closes a City dialog */
        bool      unit_acted;    /* the selected unit had acted at the open */
        hex_coord hex;           /* the map menu: the hex of the right click */
    } context;
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
        char pack_request[CONTENT_KEY_SIZE];    /* the pack of a chosen scenario of another pack: the game starts the engine again with it, then clears it */
        char pack_scenario[CONTENT_KEY_SIZE];   /* the key of that scenario: the new engine loads it (ui_start_pack_scenario) */
        u32  pack_slot;                         /* 1 + the save slot of another pack that the new engine loads; 0 for none */
    } game;
    u8   ledger_city;           /* the ledger filter: ENGINE_ANY_CITY or a city id */
    u8   ledger_resource;        /* the ledger filter: ENGINE_ANY_RESOURCE or a sim_resource */
    text_catalog *text;      /* the UI strings and the names; owned by the caller */
    char pending_locale[TEXT_LOCALE_SIZE];   /* set by the Language combo; ui_draw loads it at the start of the next frame */
    char load_message[96];   /* result of the last "Load sample save" click */
    char names_message[96];  /* result of the city names load of the last "Load sample save" click */
    struct
    {
        bool      has_from;
        bool      has_to;
        hex_coord from;
        hex_coord to;
        f32       zoom;
        bool      focused;       /* the Hex map panel has the keyboard: W A S D scroll it, PageUp and PageDown zoom it */
        bool      has_city;      /* M10: a click on a city hex selects the city; its slots show on the map and the scene */
        u8        city;
        bool      scroll_request; /* M11: the canvas scrolls to scroll_to in the next frame */
        hex_coord scroll_to;
    } map;
    /* MP3: a shared game (ui_multiplayer.cpp). The page owns the room; this is what the game knows of it. */
    struct
    {
        bool available;          /* the platform has a network: the start settings show "Play online" */
        bool host_request;       /* "Play online": the next frame sends the start image and the seats to the page */
        bool waiting;            /* the host waits for the start of the room */
        bool joined;             /* the start image loaded, and multiplayer_begin ran */
        u32  seat_count;
        u8   seat_force[UI_NET_SEATS];
        char seat_player[UI_NET_SEATS][UI_NET_NAME_SIZE];   /* "" for a faction of the AI */
        bool seat_online[UI_NET_SEATS];
        s32  desync_step;        /* -1: none */
        char error[96];
        bool timer_on;           /* MP4: the room gave a timer of the faction turn */
        bool timer_paused;       /* the timer stops while a player is absent */
        f32  timer_seconds;      /* the seconds left; the banner counts down */
        u32  snapshot_turn;      /* MP5: the game turn of the last snapshot */
        bool snapshot_author;    /* the local engine was the author of a step in this frame */
        bool snapshot_request;   /* 儲存 in a shared game: the next frame sends a snapshot */
        char room_code[8];       /* the room code of the title menu */
        bool open_room;          /* the next frame sends the room code to the page */
        /* MP7: the chat. A ring of the last lines "name<TAB>line" of the page. */
        char chat_lines[UI_NET_CHAT_LINES][UI_NET_CHAT_SIZE];
        u32  chat_count;         /* all lines so far; the ring keeps the last UI_NET_CHAT_LINES */
        u32  chat_unread;
        bool show_chat;
        bool chat_send;          /* the next frame sends chat_text to the page */
        char chat_text[UI_NET_CHAT_SIZE];
    } net;
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
    const portrait_list       *portraits;          /* the default portraits; zero without the archive */
    platform_portrait_texture *portrait_texture;   /* zero in the headless tests: no texture */
    platform_face_texture     *face_texture;       /* the generated faces (render/faces.h); zero in the headless tests */
    platform_splash_picture   *splash_picture;     /* the splash art; zero in the headless tests */
    platform_net_send        *net_send;           /* MP3: the socket of a shared game; zero without a network */
    platform_net_poll         *net_poll;
    platform_open_url         *open_url;           /* opens a link of the English Wikipedia; zero in the headless tests */
};

/* The caller loads a locale into the catalog before the first frame. */
internal void ui_init(ui_state *ui, text_catalog *text);
/* M18: replaces the text provider of the messages. A provider without a
   line function gives back the template provider. */
internal void ui_set_text_provider(ui_state *ui, text_provider provider);
internal void ui_draw(ui_state *ui, engine_api *api, ui_frame_info *frame);

#endif /* UI_H */
