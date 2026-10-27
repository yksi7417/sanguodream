/* ui_tests.exe: the top of the pyramid, run headless (no window, no GPU).

   Dear ImGui Test Engine drives the real panels in code/ui against a
   recording double of engine_api. Each "ui" test asserts which API calls a
   user action produced, and that nothing else was commanded. The few "e2e"
   tests wire the real engine behind the same panels; keep them to a handful.

   Usage: ui_tests.exe [filter]     filter examples: "ui", "e2e", "ui/advance" */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "imgui.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_context.h"

#include "base.h"
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
#include "test_framework.h"
#include "test_files.h"
#include "engine_recorder.h"

/* The memory of the text catalog. Each test loads its locale again, and
   the load resets the arena, so the tests share it. */
#define UI_TEST_TEXT_ARENA_SIZE Megabytes(4)
global_variable memory_arena g_ui_test_text_arena;

/* Per-test state. Zeroed by the test engine before each test. */
struct ui_test_vars
{
    engine_recorder recorder;
    engine_api      api;
    text_catalog    text;     /* the real data\placeholder files, zh-TW */
    ui_state        ui;
    ui_frame_info   frame;
    /* e2e only */
    bool            use_real_engine;
    memory_arena    permanent;
    memory_arena    transient;
    engine         *backend;
};

internal void
ui_test_gui(ImGuiTestContext *ctx)
{
    ui_test_vars &v = ctx->GetVars<ui_test_vars>();
    if (!v.api.engine)
    {
        if (v.use_real_engine)
        {
            v.permanent = test_arena_create(Megabytes(8));
            v.transient = test_arena_create(Megabytes(8));
            engine_config config = engine_default_config();
            config.ledger_capacity = 1024;
            config.use_pack_map    = false;   /* the e2e tests use the placeholder board of M0 to M9 */
            config.read_entire_file = test_platform_read_entire_file;
            config.write_entire_file = test_platform_write_entire_file;
            config.save_folder      = "test_saves";
            v.backend = engine_create(&v.permanent, &v.transient, config);
            v.api     = engine_make_api(v.backend);
        }
        else
        {
            v.api = recorder_make_api(&v.recorder);
        }
        text_catalog_init(&v.text, g_ui_test_text_arena, test_platform_read_entire_file,
                          HEXDOM_DATA_ROOT, HEXDOM_CONTENT_PACK, "en");
        text_catalog_set_locale(&v.text, "zh-TW");
        ui_init(&v.ui, &v.text);
        v.ui.show_demo = false;   /* the demo window is not under test */
        v.ui.show_map  = true;    /* the 2D map panel is closed by default now; its tests need it open */
        v.ui.show_city = false;   /* the City panel asks for city names too; its tests open it */
    }
    v.frame.dt_seconds = ImGui::GetIO().DeltaTime;
    if (v.frame.frame_arena)   /* a test of the scene: fresh render commands each frame, as the platform makes them */
    {
        v.frame.frame_arena->used = 0;
        v.frame.render = render_commands_begin(v.frame.frame_arena, 800.0f, 600.0f);
    }
    ui_draw(&v.ui, &v.api, &v.frame);
}

/* The mesh instances of one mesh id in the frame's render commands. */
internal u32
ui_test_count_meshes(render_commands *rc, u32 mesh_id, f32 *scale_out)
{
    u32 count = 0;
    for (u32 index = 0; rc && index < rc->mesh_instance_count; ++index)
    {
        if (rc->mesh_instances[index].mesh_id != mesh_id) continue;
        if (scale_out) *scale_out = rc->mesh_instances[index].scale;
        count++;
    }
    return count;
}

/* The yaw of the first instance of one mesh id, or -100 when none. */
internal f32
ui_test_mesh_yaw(render_commands *rc, u32 mesh_id)
{
    for (u32 index = 0; rc && index < rc->mesh_instance_count; ++index)
    {
        if (rc->mesh_instances[index].mesh_id == mesh_id) return rc->mesh_instances[index].yaw;
    }
    return -100.0f;
}

/* The tint of the first instance of one mesh id, or 0 when none. */
internal u32
ui_test_mesh_tint(render_commands *rc, u32 mesh_id)
{
    for (u32 index = 0; rc && index < rc->mesh_instance_count; ++index)
    {
        if (rc->mesh_instances[index].mesh_id == mesh_id) return rc->mesh_instances[index].tint;
    }
    return 0;
}

/* The count of the instances of one mesh id that name one art sprite. */
internal u32
ui_test_mesh_art(render_commands *rc, u32 mesh_id, u32 art_sprite)
{
    u32 count = 0;
    for (u32 index = 0; rc && index < rc->mesh_instance_count; ++index)
    {
        if (rc->mesh_instances[index].mesh_id == mesh_id && rc->mesh_instances[index].art_sprite == art_sprite) count++;
    }
    return count;
}

/* The five city command dialogs of M3: the window, the City panel button,
   the UI kind and the engine command that 決定 issues. */
struct ui_command_case
{
    const char         *decide_test;
    const char         *back_test;
    const char         *window;
    const char         *open_button;
    u32                 ui_kind;
    engine_command_kind engine_kind;
};

/* M14: one item of the base command menu of the game mode
   (city-commands.md, "Base command menu"): the main item, the submenu
   item, and the dialog that the item opens. An item without an api is
   grey. */
enum ui_base_menu_expect
{
    UI_EXPECT_DEVELOP,
    UI_EXPECT_MERGE,
    UI_EXPECT_COMMAND,     /* show_command[ui_kind] */
    UI_EXPECT_RESEARCH,
    UI_EXPECT_SORTIE,
    UI_EXPECT_TRANSPORT,
    UI_EXPECT_COURT,
    UI_EXPECT_DIPLOMACY,
    UI_EXPECT_RULER,
    UI_EXPECT_MOVE,        /* G2-036: show_move_officers */
    UI_EXPECT_SUMMON,      /* G2-037 */
    UI_EXPECT_PLOT,        /* G2-065: show_plot with the kind ui_kind */
    UI_EXPECT_POSTS,       /* G2-060 */
    UI_EXPECT_EXILE,
    UI_EXPECT_ABILITIES,
    UI_EXPECT_REWARD,      /* G2-040: show_reward */
    UI_EXPECT_GREY
};

struct ui_base_menu_case
{
    const char *test;
    const char *menu;      /* the main item; the same as item for a main item without a submenu */
    const char *item;
    u32         expect;    /* ui_base_menu_expect */
    u32         ui_kind;   /* UI_EXPECT_COMMAND: the ui_command_kind */
};

global_variable ui_base_menu_case ui_base_menu_cases[] = {
    { "base_menu_city_develop_opens_its_dialog",     "###menu_city",      "###base_develop",   UI_EXPECT_DEVELOP,   0 },
    { "base_menu_city_merge_opens_its_dialog",       "###menu_city",      "###base_merge",     UI_EXPECT_MERGE,     0 },
    { "base_menu_city_conscript_opens_its_dialog",   "###menu_city",      "###base_conscript", UI_EXPECT_COMMAND,   UI_COMMAND_CONSCRIPT },
    { "base_menu_city_produce_opens_its_dialog",     "###menu_city",      "###base_produce",   UI_EXPECT_COMMAND,   UI_COMMAND_PRODUCE },
    { "base_menu_city_patrol_opens_its_dialog",      "###menu_city",      "###base_patrol",    UI_EXPECT_COMMAND,   UI_COMMAND_PATROL },
    { "base_menu_city_merchant_opens_its_dialog",    "###menu_city",      "###base_merchant",  UI_EXPECT_COMMAND,   UI_COMMAND_MERCHANT },
    { "base_menu_city_research_opens_its_dialog",    "###menu_city",      "###base_research",  UI_EXPECT_RESEARCH,  0 },
    { "base_menu_military_train_opens_its_dialog",   "###menu_military",  "###base_train",     UI_EXPECT_COMMAND,   UI_COMMAND_TRAIN },
    { "base_menu_military_sortie_opens_its_dialog",  "###menu_military",  "###base_sortie",    UI_EXPECT_SORTIE,    0 },
    { "base_menu_military_transport_opens_its_dialog", "###menu_military", "###base_transport", UI_EXPECT_TRANSPORT, 0 },
    { "base_menu_people_move_opens_its_dialog",      "###menu_people",    "###base_move",      UI_EXPECT_MOVE,      0 },
    { "base_menu_people_summon_opens_its_dialog",    "###menu_people",    "###base_summon",    UI_EXPECT_SUMMON,    0 },
    { "base_menu_people_search_opens_the_court",     "###menu_people",    "###base_search",    UI_EXPECT_COURT,     0 },
    { "base_menu_people_recruit_opens_the_court",    "###menu_people",    "###base_recruit",   UI_EXPECT_COURT,     0 },
    { "base_menu_people_reward_opens_its_dialog",    "###menu_people",    "###base_reward",    UI_EXPECT_REWARD,    0 },
    { "base_menu_people_give_opens_the_court",       "###menu_people",    "###base_give",      UI_EXPECT_COURT,     0 },
    { "base_menu_diplomacy_opens_its_panel",         "###menu_diplomacy", "###menu_diplomacy", UI_EXPECT_DIPLOMACY, 0 },
    { "base_menu_plot_two_tigers_opens_the_diplomacy_panel", "###menu_plot", "###base_two_tigers", UI_EXPECT_DIPLOMACY, 0 },
    { "base_menu_plot_rumour_opens_its_dialog",      "###menu_plot",      "###base_rumour",    UI_EXPECT_PLOT,      SIM_PLOT_RUMOUR },
    { "base_menu_ruler_corps_opens_the_ruler_panel", "###menu_ruler",     "###base_corps",     UI_EXPECT_RULER,     0 },
    { "base_menu_ruler_posts_opens_its_window",      "###menu_ruler",     "###base_posts",     UI_EXPECT_POSTS,     0 },
    { "base_menu_ruler_exile_opens_its_dialog",      "###menu_ruler",     "###base_exile",     UI_EXPECT_EXILE,     0 },
    { "base_menu_ruler_matchmake_is_grey",           "###menu_ruler",     "###base_matchmake", UI_EXPECT_GREY,      0 },
    { "base_menu_ruler_train_officer_opens_the_abilities", "###menu_ruler", "###base_train_officer", UI_EXPECT_ABILITIES, 0 },
};

global_variable ui_command_case ui_command_cases[] = {
    { "patrol_decide_issues_one_command",    "patrol_back_issues_nothing",    "###Patrol",    "###open_patrol",    UI_COMMAND_PATROL,    ENGINE_COMMAND_PATROL },
    { "conscript_decide_issues_one_command", "conscript_back_issues_nothing", "###Conscript", "###open_conscript", UI_COMMAND_CONSCRIPT, ENGINE_COMMAND_CONSCRIPT },
    { "train_decide_issues_one_command",     "train_back_issues_nothing",     "###Train",     "###open_train",     UI_COMMAND_TRAIN,     ENGINE_COMMAND_TRAIN },
    { "merchant_decide_issues_one_command",  "merchant_back_issues_nothing",  "###Merchant",  "###open_merchant",  UI_COMMAND_MERCHANT,  ENGINE_COMMAND_MERCHANT },
    { "produce_decide_issues_one_command",   "produce_back_issues_nothing",   "###Produce",   "###open_produce",   UI_COMMAND_PRODUCE,   ENGINE_COMMAND_PRODUCE },
};

/* A double of the portrait texture of the platform: it records the
   picture that the UI asks for. */
global_variable u32 g_ui_test_portrait_requests;
global_variable u32 g_ui_test_portrait_last;
global_variable portrait_list g_ui_test_portraits;

internal PLATFORM_PORTRAIT_TEXTURE(ui_test_portrait_texture)
{
    g_ui_test_portrait_requests += 1;
    g_ui_test_portrait_last      = picture;
    return 1;   /* any nonzero id: the headless tests draw no texture */
}

/* A double of the generated faces of the platform: it records the officer
   and the key that the UI asks for. */
global_variable u32 g_ui_test_face_requests;
global_variable u32 g_ui_test_face_last_officer;
global_variable u32 g_ui_test_face_last_key;

internal PLATFORM_FACE_TEXTURE(ui_test_face_texture)
{
    g_ui_test_face_requests    += 1;
    g_ui_test_face_last_officer = officer_id;
    g_ui_test_face_last_key     = key;
    return 2;
}

/* Clicks the header of a column of a sortable table. The column labels of
   the panels have a "###" stable id, which the test engine's own
   TableClickHeader does not hash the way ImGui does; this helper does. */
internal void
ui_test_click_header(ImGuiTestContext *ctx, const char *table_ref, int column_n, const char *label)
{
    ImGuiTable *table = ImGui::TableFindByID(ctx->GetID(table_ref));
    IM_CHECK_SILENT(table != 0);
    ImGuiID instance  = ImGui::TableGetInstanceID(table, 0);
    ImGuiID column_id = ImHashData(&column_n, sizeof(column_n), instance);   /* TableHeadersRow: PushID(column_n) */
    ctx->ItemClick(ImHashStr(label, 0, column_id));                            /* TableHeader: GetID(label) */
}

/* Moves the mouse to the center of a hex of the Hex map panel and clicks it. */
internal void
ui_test_click_cell(ImGuiTestContext *ctx, ui_test_vars &v, hex_coord cell)
{
    ctx->WindowFocus("//Hex map");   /* in front of the other panels, so that the click reaches the canvas */
    ctx->Yield();
    ImGuiWindow *canvas = ctx->WindowInfo("//Hex map/map_canvas").Window;
    IM_CHECK(canvas != 0);
    ctx->SetRef(canvas);
    ImGuiTestItemInfo cells = ctx->ItemInfo("cells");
    IM_CHECK(cells.ID != 0);
    f32 zoom = v.ui.map.zoom;
    hex_point origin;
    origin.x = cells.RectFull.Min.x + zoom;
    origin.y = cells.RectFull.Min.y + zoom;
    hex_point center = hex_to_pixel(cell, zoom, origin);
    ctx->MouseMoveToPos(ImVec2(center.x, center.y));
    ctx->MouseClick(0);
    ctx->Yield();
}

/* True if the window was drawn in the last frame. A window that never
   showed, or that hides now, gives false. */
internal bool
ui_test_window_active(ImGuiTestContext *ctx, const char *ref)
{
    ImGuiWindow *window = ctx->WindowInfo(ref, ImGuiTestOpFlags_NoError).Window;
    return window != 0 && window->Active;
}

/* 進行 of the map menu. M22: if units of the player have not acted, the
   game asks first, and the helper answers 是. */
internal void
ui_test_proceed(ImGuiTestContext *ctx)
{
    ui_test_vars &v = ctx->GetVars<ui_test_vars>();
    ctx->ItemClick("//###MapMenu/###proceed");
    if (!v.ui.round.confirm) return;
    ctx->Yield();
    ctx->ItemClick("//###ProceedConfirm/###confirm_yes");
}

/* M12: the signed angle from one heading to the next, in one turn. */
internal f32
ui_test_turn_between(f32 from, f32 to)
{
    f32 turn = to - from;
    while (turn >  RENDER_PI) turn -= 2.0f * RENDER_PI;
    while (turn < -RENDER_PI) turn += 2.0f * RENDER_PI;
    return turn;
}

internal void
register_ui_tests(ImGuiTestEngine *e)
{
    ImGuiTest *t = 0;

    t = IM_REGISTER_TEST(e, "ui", "advance_turn_calls_end_turn");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        ctx->SetRef("Simulation");
        recorder_reset(&v.recorder);
        ctx->ItemClick("Advance turn");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_END_TURN), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK_EQ(v.recorder.dropped, 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "advance_12_calls_advance_turns_12");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        ctx->SetRef("Simulation");
        recorder_reset(&v.recorder);
        ctx->ItemClick("Advance 12 turns");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_ADVANCE_TURNS), 1u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_ADVANCE_TURNS), 12u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
    };

    t = IM_REGISTER_TEST(e, "ui", "ledger_clear_calls_ledger_clear");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        recorder_seed_events(&v.recorder, 5);
        ctx->SetRef("Event ledger");
        ctx->Yield();
        IM_CHECK(ctx->ItemExists("ledger"));                 /* the table is drawn */
        recorder_reset(&v.recorder);
        ctx->ItemClick("Clear");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_LEDGER_CLEAR), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
    };

    t = IM_REGISTER_TEST(e, "ui", "ledger_reads_every_event_through_api");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        recorder_seed_events(&v.recorder, 7);
        ctx->SetRef("Event ledger");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_LEDGER_AT), 7u);   /* one row per event */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);             /* drawing commands nothing */
    };

    t = IM_REGISTER_TEST(e, "ui", "window_menu_toggles_without_commands");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        recorder_reset(&v.recorder);
        ctx->SetRef("##MainMenuBar");
        ctx->MenuUncheck("###Windows/Simulation");
        ctx->Yield();
        IM_CHECK(!v.ui.show_sim);
        IM_CHECK(!ctx->WindowInfo("//Simulation", ImGuiTestOpFlags_NoError).Window ||
                 !ctx->WindowInfo("//Simulation", ImGuiTestOpFlags_NoError).Window->WasActive);
        ctx->MenuCheck("###Windows/Simulation");
        ctx->Yield();
        IM_CHECK(v.ui.show_sim);
        ctx->MenuCheck("###Windows/Memory");
        ctx->Yield();
        IM_CHECK(v.ui.show_memory);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_MEMORY), 1u);    /* memory panel asks the api (every frame) */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "map_panel_reads_every_cell_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        ctx->SetRef("Hex map");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_MAP_SIZE), 1u);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_TERRAIN_VIEW), 1u);     /* M10: one terrain view gives the tile of every cell */
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_CELL_COST), 0u);        /* the tiles replace a cost query for each cell */
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_PATH_COST), 0u);        /* nothing selected yet */
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_HEX_AT), 0u);           /* no hex picked yet */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        ctx->ItemClick("Clear selection");
        ctx->Yield();
        IM_CHECK(!v.ui.map.has_from && !v.ui.map.has_to);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "officers_panel_reads_every_row_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 5;
        ctx->SetRef("###Officers");
        ctx->Yield(2);
        IM_CHECK(ctx->ItemExists("officers"));                                 /* the table is drawn */
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_OFFICER_COUNT), 1u);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_OFFICER_AT), 5u);      /* one call per row */
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_OFFICER_AT), 0u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "cities_panel_reads_every_row_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.city_count = 4;
        v.ui.show_map         = false;   /* the Hex map panel reads the cities for its overlay; this test counts the Cities panel */
        ctx->SetRef("###Cities");
        ctx->Yield(2);
        IM_CHECK(ctx->ItemExists("cities"));
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_CITY_COUNT), 1u);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_CITY_AT), 4u);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_CITY_NAME_AT), 4u);   /* asked even before names load */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "cities_panel_reads_each_name_through_the_api_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.city_count = 3;
        v.recorder.has_names  = true;
        ctx->SetRef("###Cities");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_CITY_NAME_AT), 3u);   /* one call per row */
        u32 name_calls = 0;
        for (u32 index = 0; index < v.recorder.call_count; ++index)
        {
            recorder_call *call = v.recorder.calls + index;
            if (call->kind != RECORDER_CITY_NAME_AT) continue;
            IM_CHECK_EQ(call->arg, name_calls);                                 /* the id that city_at returned for the row */
            IM_CHECK(index > 0 && v.recorder.calls[index - 1].kind == RECORDER_CITY_AT &&
                     v.recorder.calls[index - 1].arg == name_calls);            /* asked right after its row */
            name_calls++;
        }
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "officers_panel_shows_names_through_the_text_module_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 4;
        v.ui.show_sim            = false;  /* count only the names of the Officers panel */
        ctx->SetRef("###Officers");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        text_catalog_stats before = v.text.stats;
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_OFFICER_AT), 4u);              /* the names come from engine_api */
        IM_CHECK_EQ(v.text.stats.name_lookups - before.name_lookups, 16ull);           /* family and given name of each row and of its ruler */
        IM_CHECK_EQ(v.text.stats.decoded_names - before.decoded_names, 8ull);          /* zh-TW has no names file: Big5 decode; the given names are empty */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "cities_panel_shows_names_through_the_text_module_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.city_count = 3;
        v.recorder.has_names  = true;
        v.ui.show_officers    = false;     /* count only the names of the Cities panel */
        v.ui.show_sim         = false;
        ctx->SetRef("###Cities");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        text_catalog_stats before = v.text.stats;
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_CITY_NAME_AT), 3u);
        IM_CHECK_EQ(v.text.stats.name_lookups - before.name_lookups, 3ull);            /* one name for each row; no officers, so no ruler names */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "officers_panel_names_force_status_and_location_through_queries_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 4;
        v.recorder.has_names     = true;
        v.ui.show_sim            = false;
        v.ui.show_cities         = false;
        ctx->SetRef("###Officers");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        text_catalog_stats before = v.text.stats;
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_FORCE_RULER), 4u);               /* the force column of each row */
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_FORCE_RULER), 1u);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_OFFICER_BY_ID), 4u);             /* the name of the ruler */
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_OFFICER_BY_ID), 100u);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_LOCATION_NAME_AT), 4u);          /* the location column of each row */
        u32 location_calls = 0;
        for (u32 index = 0; index < v.recorder.call_count; ++index)
        {
            if (v.recorder.calls[index].kind != RECORDER_LOCATION_NAME_AT) continue;
            IM_CHECK_EQ(v.recorder.calls[index].arg, location_calls);                     /* officer i has its base in city i */
            location_calls++;
        }
        IM_CHECK_EQ(v.text.stats.label_lookups - before.label_lookups, 4ull);              /* the status label of each row */
        IM_CHECK_EQ(v.text.stats.unknown_labels - before.unknown_labels, 0ull);            /* status 3 has a label */
        IM_CHECK_EQ(v.text.stats.missing_strings - before.missing_strings, 0ull);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "cities_panel_names_owner_and_commander_through_queries_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 1;      /* officer 100 rules force 1 and commands city 0 */
        v.recorder.city_count    = 3;
        v.ui.show_sim            = false;
        v.ui.show_officers       = false;
        ctx->SetRef("###Cities");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_FORCE_RULER), 3u);               /* the owner column of each row */
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_OFFICER_BY_ID), 3u + 1u);        /* the ruler of each owner, and the one commander */
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_OFFICER_AT), 0u);                /* no scan of the officer table */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "raw_check_box_shows_numbers_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.recorder.has_names     = true;
        v.ui.show_sim            = false;
        v.ui.show_city           = true;
        ctx->SetRef("###Officers");
        ctx->Yield(2);
        IM_CHECK(!v.ui.show_raw);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###raw");
        ctx->Yield(2);
        IM_CHECK(v.ui.show_raw);                                                          /* one setting for all panels */
        recorder_reset(&v.recorder);
        text_catalog_stats before = v.text.stats;
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_FORCE_RULER), 0u);               /* raw numbers need no names */
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_OFFICER_BY_ID), 0u);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_LOCATION_NAME_AT), 0u);
        IM_CHECK_EQ(v.text.stats.label_lookups - before.label_lookups, 0ull);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_OFFICER_AT), 3u + 1u);           /* the rows are still read; the City panel asks its first free officer */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        ctx->SetRef("###City");
        ctx->ItemClick("###raw");
        ctx->Yield(2);
        IM_CHECK(!v.ui.show_raw);                                                         /* the City panel clears it again */
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_FORCE_RULER), 3u + 2u);          /* the names are back: officers, cities, city */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "city_panel_names_owner_governor_and_specialty_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 1;
        v.recorder.city_count    = 2;
        v.ui.show_sim            = false;
        v.ui.show_officers       = false;
        v.ui.show_cities         = false;
        v.ui.show_city           = true;
        ctx->SetRef("###City");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        text_catalog_stats before = v.text.stats;
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_FORCE_RULER), 1u);               /* the owner */
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_FORCE_RULER), 1u);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_OFFICER_BY_ID), 2u);             /* the ruler and the governor */
        IM_CHECK_EQ(v.text.stats.label_lookups - before.label_lookups, 1ull);              /* the specialty */
        IM_CHECK_EQ(v.text.stats.unknown_labels - before.unknown_labels, 0ull);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "each_locale_has_each_status_and_specialty_label");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        const u32 statuses[] = { 0, 1, 2, 3, 6, 8 };       /* the labelled values of save-file.md, officer +100 */
        char label[64];
        for (u32 index = 0; index < v.text.locale_count; ++index)
        {
            IM_CHECK(text_catalog_set_locale(&v.text, v.text.locales[index].code));
            u64 unknown = v.text.stats.unknown_labels;
            for (u32 status = 0; status < ArrayCount(statuses); ++status) text_label(&v.text, "status", statuses[status], label, sizeof(label));
            for (u32 specialty = 0; specialty < CITY_SPECIALTY_COUNT; ++specialty) text_label(&v.text, "specialty", specialty, label, sizeof(label));
            IM_CHECK_EQ(v.text.stats.unknown_labels, unknown);                            /* each code has a label in this locale */
            IM_CHECK_STR_EQ(text_label(&v.text, "status", 4, label, sizeof(label)), "?4"); /* status 4 has no label in the spec */
        }
        IM_CHECK(text_catalog_set_locale(&v.text, "zh-TW"));
        IM_CHECK_STR_EQ(text_label(&v.text, "status", 0, label, sizeof(label)), "\xE5\x90\x9B\xE4\xB8\xBB");   /* zh-TW: the game term */
        IM_CHECK(text_catalog_set_locale(&v.text, "en"));
        IM_CHECK_STR_EQ(text_label(&v.text, "status", 0, label, sizeof(label)), "ruler");                      /* en: the gloss */
    };

    /* The log has a form of its own for every event kind that reaches it.
       The generic forms "{subject}: {delta}" and "{subject}: news" say
       nothing about the field (the player asked what "+11" means). */
    /* G2-024: the city window opens at its top each time, so the menu
       buttons stand at the same place at each open. */
    t = IM_REGISTER_TEST(e, "ui", "city_window_opens_at_the_top_each_time");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.hud_mode            = true;
        v.ui.show_city           = true;
        v.ui.city.selected       = 0;
        ctx->Yield(2);
        ctx->WindowResize("###City", ImVec2(300.0f, 120.0f));   /* smaller than its content */
        ctx->Yield();
        ImGuiWindow *window = ctx->WindowInfo("###City").Window;
        IM_CHECK(window != 0);
        ImGui::SetScrollY(window, 60.0f);
        ctx->Yield(2);
        IM_CHECK_GT(window->Scroll.y, 0.0f);
        v.ui.show_city = false;
        ctx->Yield(2);
        v.ui.show_city = true;
        ctx->Yield(3);
        window = ctx->WindowInfo("###City").Window;
        IM_CHECK(window != 0);
        IM_CHECK_EQ(window->Scroll.y, 0.0f);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        ctx->WindowResize("###City", ImVec2(440.0f, 420.0f));   /* ImGui keeps the size by name: give the later tests the default back */
        ctx->Yield();
    };

    t = IM_REGISTER_TEST(e, "ui", "each_locale_has_a_log_line_for_each_event_kind");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        for (u32 index = 0; index < v.text.locale_count; ++index)
        {
            IM_CHECK(text_catalog_set_locale(&v.text, v.text.locales[index].code));
            if (!text_catalog_has_strings(&v.text)) continue;
            for (u32 kind = 1; kind < SIM_EVENT_COUNT; ++kind)
            {
                if (ui_feel_bookkeeping((sim_event_kind)kind)) continue;
                char id[64];
                snprintf(id, sizeof(id), "log.%s", sim_event_name((sim_event_kind)kind));
                if (!text_find_string(&v.text, id)) ctx->LogError("%s lacks %s", v.text.locales[index].code, id);
                IM_CHECK(text_find_string(&v.text, id) != 0);
            }
        }
        IM_CHECK(text_catalog_set_locale(&v.text, "zh-TW"));
    };

    /* A value that rises over a city names its field (資金收入 +2047, not
       +2047 alone), and a log line of an officer names the field too. */
    t = IM_REGISTER_TEST(e, "ui", "a_rising_value_and_a_log_line_name_the_field");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        ui_enter_game(&v.ui);
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        sim_event income = {};
        income.kind = SIM_EVENT_GOLD_INCOME; income.faction = 1; income.subject = 1; income.delta = 2047; income.cause = 7;
        sim_event loyalty = {};
        loyalty.kind = SIM_EVENT_LOYALTY_CHANGED; loyalty.faction = 1; loyalty.subject = 9; loyalty.delta = 11; loyalty.cause = 8;
        v.recorder.events[v.recorder.event_count++] = income;
        v.recorder.events[v.recorder.event_count++] = loyalty;
        ctx->Yield(2);
        engine_event_report report = v.api.event_report(v.api.engine, 0);
        IM_CHECK_EQ(report.resource, (u8)SIM_RESOURCE_GOLD);
        IM_CHECK_STR_EQ(report.resource_key, "gold");
        IM_CHECK_EQ(v.ui.report.float_count, 1u);
        IM_CHECK_EQ(v.ui.report.floats[0].resource, (u8)SIM_RESOURCE_GOLD);
        char line[256];
        ui_report_line(&v.ui, &v.api, 1, TEXT_NO_VALUE, TEXT_LINE_LOG, line, sizeof(line));
        IM_CHECK(strstr(line, "\xE5\xBF\xA0\xE8\xAA\xA0") != 0);   /* zh-TW: 忠誠 */
        IM_CHECK(strstr(line, "+11") != 0);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "language_combo_changes_the_locale_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        ctx->SetRef("##MainMenuBar");
        ctx->MenuCheck("###Windows/###Settings");
        ctx->Yield();
        IM_CHECK(v.ui.show_settings);
        IM_CHECK_STR_EQ(v.text.locale, "zh-TW");
        u32 loads = v.text.load_count;
        recorder_reset(&v.recorder);
        ctx->SetRef("###Settings");
        ctx->ComboClick("###Language/###en");
        ctx->Yield(2);
        IM_CHECK_STR_EQ(v.text.locale, "en");
        IM_CHECK_EQ(v.text.load_count, loads + 1);                                     /* one load for one click */
        IM_CHECK(ctx->WindowInfo("//###Officers").Window != 0);                        /* the panels keep their ids */
        ctx->ComboClick("###Language/###zh-TW");
        ctx->Yield(2);
        IM_CHECK_STR_EQ(v.text.locale, "zh-TW");
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);                         /* the locale is a front end setting */
    };

    t = IM_REGISTER_TEST(e, "ui", "each_locale_has_each_ui_string");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 2;
        v.recorder.city_count    = 2;
        v.ui.show_settings       = true;
        v.ui.show_credits        = true;
        v.ui.show_city           = true;
        v.ui.show_develop        = true;
        v.ui.show_economy        = true;
        v.ui.show_research       = true;
        v.ui.show_merge          = true;
        v.ui.show_techniques     = true;
        v.ui.show_sortie         = true;
        v.ui.show_transport      = true;
        v.ui.show_unit           = true;
        v.ui.show_unit_menu      = true;
        v.ui.show_minimap        = true;
        v.ui.show_ruler          = true;
        v.ui.show_bases          = true;
        v.recorder.pending_active = true;
        v.ui.show_title = v.ui.show_scenarios = v.ui.show_rulers = v.ui.show_start_settings = true;   /* M9 */
        v.ui.show_topbar = v.ui.show_history = v.ui.show_saveload = v.ui.show_options = true;
        v.ui.show_new_officer = v.ui.show_editor = v.ui.show_battles = v.ui.show_abilities = v.ui.show_info = true;
        v.ui.game.chosen = 1;
        v.ui.game.ancient = 0;
        v.ui.units.selected      = 0;
        for (u32 kind = 0; kind < UI_COMMAND_COUNT; ++kind) v.ui.show_command[kind] = true;
        v.ui.ledger_city         = 1;
        v.ui.ledger_resource     = SIM_RESOURCE_GOLD;
        IM_CHECK_GE(v.text.locale_count, 2u);
        for (u32 index = 0; index < v.text.locale_count; ++index)
        {
            IM_CHECK(text_catalog_set_locale(&v.text, v.text.locales[index].code));
            IM_CHECK(text_catalog_has_strings(&v.text));
            u64 missing = v.text.stats.missing_strings;
            ctx->Yield(2);
            IM_CHECK_EQ(v.text.stats.missing_strings, missing);                       /* the file has each id that the panels ask for */
        }
    };

    t = IM_REGISTER_TEST(e, "ui", "load_sample_save_loads_the_save_and_the_city_names");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.ui.show_map = false;   /* the Hex map panel reads the cities for its overlay; this test counts the table panels */
        ctx->SetRef("Simulation");
        recorder_reset(&v.recorder);
        ctx->ItemClick("Load sample save");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_LOAD_SAVE), 1u);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_LOAD_NAME_INDEX), 1u);   /* one step: the save, then the city names */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 2u);
        IM_CHECK_STR_EQ(v.recorder.last_load_path, UI_SAMPLE_SAVE_PATH);
        IM_CHECK_STR_EQ(v.recorder.last_name_index_path, UI_NAME_INDEX_PATH);
        IM_CHECK(strncmp(v.ui.names_message, "names loaded", 12) == 0);
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_OFFICER_AT), 3u);     /* the panels show the loaded rows */
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_CITY_AT), 2u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "refused_load_shows_the_reason");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.load_fails = true;
        ctx->SetRef("Simulation");
        recorder_reset(&v.recorder);
        ctx->ItemClick("Load sample save");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_LOAD_SAVE), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK_STR_EQ(v.ui.load_message, "not loaded: not a save");
    };

    t = IM_REGISTER_TEST(e, "ui", "city_panel_reads_every_slot_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.city_count = 2;
        v.ui.show_city = true;
        ctx->SetRef("###City");
        ctx->Yield(2);
        IM_CHECK(ctx->ItemExists("slots"));                                       /* the slot table is drawn */
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_CITY_SLOTS), 1u);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_FACILITY_AT), 2u * RECORDER_SLOTS);   /* each slot for its row and for the counts (M11) */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        ctx->ComboClick("###city_select/###c1");
        ctx->Yield();
        IM_CHECK_EQ(v.ui.city.selected, 1u);
        ctx->ItemClick("###open_develop");
        ctx->Yield();
        IM_CHECK(v.ui.show_develop && v.ui.develop.city == 1);                    /* the button opens the dialog for this city */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);                    /* opening a dialog commands nothing */
    };

    t = IM_REGISTER_TEST(e, "ui", "develop_dialog_issues_one_command_develop");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_develop        = true;
        ctx->SetRef("###Develop");
        ctx->Yield(2);
        /* M19: three officers by politics; the script ties, so the table order */
        IM_CHECK(v.ui.develop.officers[0] == 100 && v.ui.develop.officers[1] == 101 && v.ui.develop.officers[2] == 102);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_CAN_ISSUE), 1u);          /* the dialog asks can_issue each frame */
        ctx->ComboClick("###facility/###f2");
        ctx->ComboClick("###slot/###s4");
        ctx->ComboClick("###officer2/###o101");
        ctx->ComboClick("###officer3/###o_none");                                  /* the player can still change the proposal */
        ctx->Yield();
        IM_CHECK(v.ui.develop.facility == 2 && v.ui.develop.slot == 4 && v.ui.develop.officers[1] == 101);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);                    /* choices command nothing */
        recorder_reset(&v.recorder);
        ctx->ItemClick("###execute");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_DEVELOP), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        engine_command sent = v.recorder.last_develop;
        IM_CHECK(sent.city_id == 0 && sent.facility == 2 && sent.slot == 4);
        IM_CHECK(sent.officers[0] == 100 && sent.officers[1] == 101 && sent.officers[2] == ENGINE_NO_OFFICER);
        IM_CHECK(v.ui.develop.message[0] != 0);                                   /* the dialog shows the result */
    };

    t = IM_REGISTER_TEST(e, "ui", "refused_develop_is_disabled_and_issues_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count     = 3;
        v.recorder.city_count        = 2;
        v.recorder.can_issue_refusal = SIM_REFUSAL_NO_GOLD;
        v.ui.show_develop            = true;
        ctx->SetRef("###Develop");
        ctx->Yield(2);
        ImGuiTestItemInfo execute = ctx->ItemInfo("###execute");
        IM_CHECK(execute.ID != 0 && (execute.ItemFlags & ImGuiItemFlags_Disabled) != 0);   /* the refusal disables the button */
        recorder_reset(&v.recorder);
        ctx->ItemClick("###execute", 0, ImGuiTestOpFlags_NoAutoOpenFullPath);
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_DEVELOP), 0u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* ---- M22: the global rules of screen-flow.md, one test each, and the fit of the HUD */

    /* Rule 1: a right click goes back one level. A command dialog goes
       back to the city menu, then the menu closes. The target pick goes
       back to the unit menu, the menu to the selection, the selection to
       the idle map. */
    t = IM_REGISTER_TEST(e, "ui", "flow_rule1_right_click_goes_back_one_level");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.hud_mode            = true;
        v.ui.show_map            = false;
        ctx->Yield(2);
        ImVec2 display = ImGui::GetIO().DisplaySize;
        ImVec2 empty(display.x * 0.8f, display.y * 0.3f);
        ui_context_open(&v.ui, &v.api, UI_CONTEXT_CITY, 300.0f, 200.0f);
        v.ui.show_command[UI_COMMAND_CONSCRIPT] = true;
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->MouseMoveToPos(empty);
        ctx->MouseClick(ImGuiMouseButton_Right);
        ctx->Yield(2);
        IM_CHECK(!v.ui.show_command[UI_COMMAND_CONSCRIPT] && v.ui.show_city && v.ui.context.kind == UI_CONTEXT_CITY);
        ctx->MouseClick(ImGuiMouseButton_Right);
        ctx->Yield(2);
        IM_CHECK(!v.ui.show_city && v.ui.context.kind == UI_CONTEXT_NONE);
        ui_select_unit(&v.ui, 0);
        ui_context_open(&v.ui, &v.api, UI_CONTEXT_UNIT, 300.0f, 200.0f);
        v.ui.show_unit_menu = true;
        v.ui.combat.pick    = UI_PICK_ATTACK;
        ctx->Yield(2);
        ctx->MouseMoveToPos(empty);
        ctx->MouseClick(ImGuiMouseButton_Right);
        ctx->Yield(2);
        IM_CHECK(v.ui.combat.pick == UI_PICK_NONE && v.ui.show_unit_menu && v.ui.context.kind == UI_CONTEXT_UNIT);
        ctx->MouseClick(ImGuiMouseButton_Right);
        ctx->Yield(2);
        IM_CHECK(!v.ui.show_unit_menu && v.ui.show_unit && v.ui.units.selected == 0);
        ctx->MouseClick(ImGuiMouseButton_Right);
        ctx->Yield(2);
        IM_CHECK(!v.ui.show_unit && v.ui.units.selected == ENGINE_NO_UNIT && v.ui.context.kind == UI_CONTEXT_NONE);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* Rule 2: a left click anywhere advances the message window. */
    t = IM_REGISTER_TEST(e, "ui", "flow_rule2_left_click_anywhere_advances_a_message");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.ui.hud_mode            = true;
        v.ui.show_map            = false;
        v.ui.feel.turn_messages  = true;
        ctx->Yield(3);
        v.ui.feel.messages[0]    = UI_MESSAGE_HEADING;
        v.ui.feel.messages[1]    = UI_MESSAGE_HEADING;
        v.ui.feel.message_count  = 2;
        v.ui.feel.message_shown  = 0;
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ImVec2 display = ImGui::GetIO().DisplaySize;
        ctx->MouseMoveToPos(ImVec2(display.x * 0.8f, display.y * 0.3f));   /* the map, away from the window */
        ctx->MouseClick(ImGuiMouseButton_Left);
        ctx->Yield(2);
        IM_CHECK_EQ(v.ui.feel.message_shown, 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* Rule 3: a selection or an open menu hides the list panel, the log
       and the mini map. They return when the selection ends. */
    t = IM_REGISTER_TEST(e, "ui", "flow_rule3_a_selection_or_a_menu_hides_the_list_the_log_and_the_minimap");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.hud_mode            = true;
        v.ui.show_map            = false;
        v.ui.show_minimap        = true;
        v.ui.report.show_log     = true;
        ctx->Yield(2);
        IM_CHECK(!ui_hud_panels_hidden(&v.ui));
        IM_CHECK(ctx->ItemInfo("//###HUD/**/###hud_row0_0", ImGuiTestOpFlags_NoError).ID != 0);
        IM_CHECK(ui_test_window_active(ctx, "//###Log") && ui_test_window_active(ctx, "//###MiniMap"));
        ui_context_open(&v.ui, &v.api, UI_CONTEXT_MAP_MENU, 400.0f, 300.0f);
        ctx->Yield(2);
        IM_CHECK(ui_hud_panels_hidden(&v.ui));
        IM_CHECK(ctx->ItemInfo("//###HUD/**/###hud_row0_0", ImGuiTestOpFlags_NoError).ID == 0);
        IM_CHECK(!ui_test_window_active(ctx, "//###Log") && !ui_test_window_active(ctx, "//###MiniMap"));
        IM_CHECK(ui_test_window_active(ctx, "//###HUD"));   /* the status area stays */
        ui_context_close(&v.ui);
        ui_select_unit(&v.ui, 0);
        ctx->Yield(2);
        IM_CHECK(ui_hud_panels_hidden(&v.ui) && !ui_test_window_active(ctx, "//###MiniMap"));
        ui_select_unit(&v.ui, ENGINE_NO_UNIT);
        ctx->Yield(2);
        IM_CHECK(!ui_hud_panels_hidden(&v.ui) && ui_test_window_active(ctx, "//###Log") && ui_test_window_active(ctx, "//###MiniMap"));
        IM_CHECK(v.ui.show_minimap && v.ui.report.show_log);   /* the panels hide; the settings of the player stay */
    };

    /* Rule 4: a camera key never closes a panel. */
    t = IM_REGISTER_TEST(e, "ui", "flow_rule4_a_camera_key_never_closes_a_panel");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.hud_mode            = true;
        v.ui.show_map            = false;
        v.ui.scene.headless      = true;
        ctx->Yield(2);
        ui_context_open(&v.ui, &v.api, UI_CONTEXT_CITY, 300.0f, 200.0f);
        ctx->Yield(2);
        ImGuiKey keys[6] = { ImGuiKey_W, ImGuiKey_A, ImGuiKey_S, ImGuiKey_D, ImGuiKey_Q, ImGuiKey_E };
        for (u32 index = 0; index < ArrayCount(keys); ++index) ctx->KeyPress(keys[index]);
        ctx->Yield(2);
        IM_CHECK(v.ui.show_city && v.ui.context.kind == UI_CONTEXT_CITY);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* Rule 5: every dialog has 決定 at the bottom left and 返回 at its
       right. 決定 is grey until the dialog is valid. */
    t = IM_REGISTER_TEST(e, "ui", "flow_rule5_decide_is_left_of_back_and_grey_until_valid");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.hud_mode            = true;
        v.ui.show_map            = false;
        for (u32 index = 0; index < ArrayCount(ui_command_cases); ++index)
        {
            const ui_command_case *test = ui_command_cases + index;
            v.recorder.can_issue_refusal = SIM_REFUSAL_NONE;
            for (u32 kind = 0; kind < UI_COMMAND_COUNT; ++kind) v.ui.show_command[kind] = kind == test->ui_kind;
            ctx->Yield(2);
            ctx->SetRef(test->window);
            ImGuiTestItemInfo decide = ctx->ItemInfo("###decide");
            ImGuiTestItemInfo back   = ctx->ItemInfo("###back");
            IM_CHECK(decide.ID != 0 && back.ID != 0);
            IM_CHECK(decide.RectFull.Min.x < back.RectFull.Min.x && fabsf(decide.RectFull.Min.y - back.RectFull.Min.y) < 1.0f);
            IM_CHECK((decide.ItemFlags & ImGuiItemFlags_Disabled) == 0);
            v.recorder.can_issue_refusal = SIM_REFUSAL_NO_GOLD;
            ctx->Yield(2);
            IM_CHECK((ctx->ItemInfo("###decide").ItemFlags & ImGuiItemFlags_Disabled) != 0);
        }
    };

    /* Rule 6: a menu follows the clicked object. A dialog sits at a fixed place. */
    t = IM_REGISTER_TEST(e, "ui", "flow_rule6_menus_follow_the_object_and_dialogs_stay");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.hud_mode            = true;
        v.ui.show_map            = false;
        ImVec2 points[2] = { ImVec2(300.0f, 200.0f), ImVec2(600.0f, 420.0f) };
        ImVec2 menu_at[2], dialog_at[2];
        for (u32 index = 0; index < 2; ++index)
        {
            ui_context_open(&v.ui, &v.api, UI_CONTEXT_MAP_MENU, points[index].x, points[index].y);
            ctx->Yield(3);
            menu_at[index] = ctx->WindowInfo("//###MapMenu").Window->Pos;
            ui_context_close(&v.ui);
            ctx->Yield(2);
            ui_context_open(&v.ui, &v.api, UI_CONTEXT_CITY, points[index].x, points[index].y);
            v.ui.show_command[UI_COMMAND_CONSCRIPT] = true;
            ctx->Yield(3);
            dialog_at[index] = ctx->WindowInfo("//###Conscript").Window->Pos;
            ui_context_close(&v.ui);
            ctx->Yield(2);
        }
        IM_CHECK(fabsf((menu_at[1].x - menu_at[0].x) - (points[1].x - points[0].x)) < 1.0f);
        IM_CHECK(fabsf((menu_at[1].y - menu_at[0].y) - (points[1].y - points[0].y)) < 1.0f);
        IM_CHECK(fabsf(dialog_at[1].x - dialog_at[0].x) < 1.0f && fabsf(dialog_at[1].y - dialog_at[0].y) < 1.0f);
    };

    /* Rule 7: the row pitch of a list is 20 px, the pitch of a menu is
       34 px, and the pitch of the tabs is 76 px. */
    t = IM_REGISTER_TEST(e, "ui", "flow_rule7_lists_menus_and_tabs_have_the_pitches_of_the_original");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.hud_mode            = true;
        v.ui.show_map            = false;
        ctx->Yield(2);
        ImGuiTestItemInfo row0 = ctx->ItemInfo("//###HUD/**/###hud_row0_0");
        ImGuiTestItemInfo row1 = ctx->ItemInfo("//###HUD/**/###hud_row0_1");
        IM_CHECK(row0.ID != 0 && row1.ID != 0);
        IM_CHECK(fabsf(row1.RectFull.Min.y - row0.RectFull.Min.y - UI_LIST_PITCH) < 0.5f);
        ImGuiTestItemInfo tab0 = ctx->ItemInfo("//###HUD/**/###hud_bases");
        ImGuiTestItemInfo tab1 = ctx->ItemInfo("//###HUD/**/###hud_officers");
        IM_CHECK(tab0.ID != 0 && tab1.ID != 0);
        IM_CHECK(fabsf(tab1.RectFull.Min.x - tab0.RectFull.Min.x - UI_TAB_PITCH) < 0.5f);
        ui_context_open(&v.ui, &v.api, UI_CONTEXT_MAP_MENU, 400.0f, 300.0f);
        ctx->Yield(2);
        const char *items[5] = { "###proceed", "###delegate", "###open_info", "###open_save", "###open_load" };
        ctx->SetRef("//###MapMenu");
        for (u32 index = 1; index < ArrayCount(items); ++index)
        {
            f32 above = ctx->ItemInfo(items[index - 1]).RectFull.Min.y;
            f32 below = ctx->ItemInfo(items[index]).RectFull.Min.y;
            IM_CHECK(fabsf(below - above - UI_MENU_PITCH) < 0.5f);
        }
    };

    /* The map menu has the five items of the original: no 此處情報. */
    t = IM_REGISTER_TEST(e, "ui", "map_menu_has_the_five_items_of_the_original");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.ui.hud_mode = true;
        ctx->Yield();
        ui_context_open(&v.ui, &v.api, UI_CONTEXT_MAP_MENU, 400.0f, 300.0f);
        ctx->Yield(2);
        ctx->SetRef("//###MapMenu");
        IM_CHECK(ctx->ItemInfo("###hex_info", ImGuiTestOpFlags_NoError).ID == 0);
        IM_CHECK(ctx->ItemInfo("###proceed").ID != 0 && ctx->ItemInfo("###open_load").ID != 0);
    };

    /* The system menu has the six items of the original. 除錯面板 is F12 only. */
    t = IM_REGISTER_TEST(e, "ui", "system_menu_has_the_six_items_of_the_original");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.ui.hud_mode = true;
        ctx->Yield();
        ui_context_open(&v.ui, &v.api, UI_CONTEXT_SYSTEM_MENU, 400.0f, 100.0f);
        ctx->Yield(2);
        ctx->SetRef("//###SystemMenu");
        const char *items[6] = { "###open_save", "###open_load", "###settings", "###edit", "###to_title", "###quit" };
        for (u32 index = 0; index < ArrayCount(items); ++index) IM_CHECK(ctx->ItemInfo(items[index]).ID != 0);
        IM_CHECK(ctx->ItemInfo("###options", ImGuiTestOpFlags_NoError).ID == 0);
        IM_CHECK(ctx->ItemInfo("###debug", ImGuiTestOpFlags_NoError).ID == 0);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###edit");
        ctx->Yield(2);
        IM_CHECK(v.ui.show_editor && v.ui.context.kind == UI_CONTEXT_EDITOR);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* The mini map has no close button in the game mode. */
    t = IM_REGISTER_TEST(e, "ui", "minimap_has_no_close_button_in_the_game_mode");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.ui.show_minimap = true;
        ctx->Yield(2);
        IM_CHECK(ctx->WindowInfo("//###MiniMap").Window->HasCloseButton);   /* the debug layout */
        v.ui.hud_mode = true;
        v.ui.show_map = false;
        ctx->Yield(2);
        IM_CHECK(!ctx->WindowInfo("//###MiniMap").Window->HasCloseButton);
    };

    /* A left click on a unit row moves the camera to the unit. It opens no
       window and selects nothing. */
    t = IM_REGISTER_TEST(e, "ui", "hud_unit_row_moves_the_camera_and_opens_no_window");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.hud_mode            = true;
        v.ui.show_map            = false;
        v.ui.scene.headless      = true;
        ctx->Yield(2);
        ctx->ItemClick("//###HUD/**/###hud_units");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("//###HUD/**/###hud_row3_1");
        ctx->Yield(2);
        IM_CHECK(!v.ui.show_unit && v.ui.context.kind == UI_CONTEXT_NONE && v.ui.units.selected == ENGINE_NO_UNIT);
        v3 expected = hex_world_center(hex_make(1, 3), v.ui.scene.cam.target.y);   /* unit 1 of the recorder */
        IM_CHECK(fabsf(v.ui.scene.cam.target.x - expected.x) < 1e-3f && fabsf(v.ui.scene.cam.target.z - expected.z) < 1e-3f);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        ctx->ItemClick("//###HUD/**/###hud_bases");   /* ImGui keeps the tab for the next tests */
    };

    /* The status bar shows 不可 in red on a city hex. */
    t = IM_REGISTER_TEST(e, "ui", "status_bar_shows_red_not_possible_on_a_city_hex");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        ctx->Yield();
        hex_offset city_cell = { RECORDER_CITY_COL, RECORDER_CITY_ROW };
        hex_coord city_hex = hex_from_offset(city_cell);
        engine_hex data = v.api.hex_at(v.api.engine, city_hex.q, city_hex.r);
        IM_CHECK(data.city_id != ENGINE_NO_CITY);
        char place[128];
        IM_CHECK(ui_status_place(&v.ui, &v.api, &data, place, sizeof(place)));
        IM_CHECK_STR_EQ(place, text_get(v.ui.text, "place.none"));
        data.city_id = ENGINE_NO_CITY;
        data.base_id = ENGINE_NO_BASE;
        IM_CHECK(!ui_status_place(&v.ui, &v.api, &data, place, sizeof(place)));
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* The help button "?" of the HUD opens a short help window. */
    t = IM_REGISTER_TEST(e, "ui", "help_button_opens_the_help_window");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.ui.hud_mode = true;
        v.ui.show_map = false;
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("//###HUD/###hud_help");
        ctx->Yield(2);
        IM_CHECK(v.ui.show_help && v.ui.context.kind == UI_CONTEXT_HELP);
        IM_CHECK(ctx->WindowInfo("//###Help").Window != 0);
        ctx->ItemClick("//###HUD/###hud_help");   /* a second click closes it */
        ctx->Yield(2);
        IM_CHECK(!v.ui.show_help && v.ui.context.kind == UI_CONTEXT_NONE);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* 進行 with own units that have not acted asks first. 否 keeps the
       turn, 是 ends it. Without such units the turn ends at once. */
    t = IM_REGISTER_TEST(e, "ui", "proceed_with_units_that_have_not_acted_asks_first");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.ui.hud_mode = true;
        v.ui.show_map = false;
        ctx->Yield();
        ui_context_open(&v.ui, &v.api, UI_CONTEXT_MAP_MENU, 400.0f, 300.0f);
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("//###MapMenu/###proceed");
        ctx->Yield(2);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_END_TURN), 0u);
        IM_CHECK(v.ui.round.confirm && ctx->WindowInfo("//###ProceedConfirm").Window != 0);
        ctx->ItemClick("//###ProceedConfirm/###confirm_no");
        ctx->Yield(2);
        IM_CHECK(!v.ui.round.confirm);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        ui_context_open(&v.ui, &v.api, UI_CONTEXT_MAP_MENU, 400.0f, 300.0f);
        ctx->Yield(2);
        ctx->ItemClick("//###MapMenu/###proceed");
        ctx->Yield(2);
        ctx->ItemClick("//###ProceedConfirm/###confirm_yes");
        ctx->Yield(2);
        IM_CHECK(!v.ui.round.confirm);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_END_TURN), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        v.recorder.units_acted = true;
        ui_context_open(&v.ui, &v.api, UI_CONTEXT_MAP_MENU, 400.0f, 300.0f);
        ctx->Yield(2);
        ctx->ItemClick("//###MapMenu/###proceed");
        ctx->Yield(2);
        IM_CHECK(!v.ui.round.confirm);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_END_TURN), 2u);
    };

    /* A right click on an own unit opens a popup: 詳細情報 opens the unit
       panel, 所屬武將 opens 武將一覽 with the officers of the unit. */
    t = IM_REGISTER_TEST(e, "ui", "unit_popup_opens_the_detail_or_the_officers_of_the_unit");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.ui.hud_mode            = true;
        v.ui.show_map            = false;
        ctx->Yield();
        v.ui.units.selected = 0;
        ui_context_open(&v.ui, &v.api, UI_CONTEXT_UNIT_POPUP, 400.0f, 300.0f);
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("//###UnitPopup/###unit_detail");
        ctx->Yield(2);
        IM_CHECK(v.ui.show_unit && v.ui.context.kind == UI_CONTEXT_UNIT && v.ui.units.selected == 0);
        ui_context_close(&v.ui);
        v.ui.units.selected = 0;
        ui_context_open(&v.ui, &v.api, UI_CONTEXT_UNIT_POPUP, 400.0f, 300.0f);
        ctx->Yield(2);
        ctx->ItemClick("//###UnitPopup/###unit_officers");
        ctx->Yield(2);
        IM_CHECK(v.ui.info.screen == UI_INFO_OFFICERS && v.ui.info.unit_filter == 0 && v.ui.context.kind == UI_CONTEXT_NONE);
        IM_CHECK_EQ(v.ui.info.id_count, 2u);   /* 100 and 101 lead unit 0 */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* ---- M21: the information menu, the tables, the political map, the hover panels */

    /* Each item of the information menu opens its screen; the items that
       the spec does not describe are grey. The menu commands nothing. */
    t = IM_REGISTER_TEST(e, "ui", "info_menu_items_open_their_screens_and_grey_the_others");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.hud_mode            = true;
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        u32 opened = 0;
        for (u32 index = 0; index < ArrayCount(ui_info_items); ++index)
        {
            const ui_info_item *item = ui_info_items + index;
            v.ui.info.screen = UI_INFO_NONE;
            ui_context_open(&v.ui, &v.api, UI_CONTEXT_INFO, 300.0f, 100.0f);
            ctx->Yield(2);
            char ref[64];
            snprintf(ref, sizeof(ref), "//###InfoMenu/###%s", item->stable_id);
            ImGuiTestItemInfo info = ctx->ItemInfo(ref);
            IM_CHECK(info.ID != 0);
            if (item->screen == UI_INFO_NONE)
            {
                IM_CHECK((info.ItemFlags & ImGuiItemFlags_Disabled) != 0);
                continue;
            }
            ctx->ItemClick(ref);
            ctx->Yield(2);
            IM_CHECK(v.ui.info.screen == item->screen && v.ui.info.own == item->own && !v.ui.show_info);
            opened++;
        }
        IM_CHECK_EQ(opened, 6u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* 武將一覽: a header click sorts, a row click opens 武將情報, 返回 closes
       the detail, then the table. */
    t = IM_REGISTER_TEST(e, "ui", "info_table_header_sorts_and_a_row_opens_the_detail");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.hud_mode            = true;
        ctx->Yield();
        ui_info_open(&v.ui, &v.api, UI_INFO_OFFICERS, false);
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->SetRef("//###InfoTable");
        ctx->ItemClick("**/###tab_ability");
        ctx->Yield(2);
        IM_CHECK_EQ(v.ui.info.tab, (int)UI_INFO_TAB_ABILITY);
        IM_CHECK_EQ(v.ui.info.id_count, 3u);
        char header[96];
        snprintf(header, sizeof(header), "%s###charm", text_get(&v.text, "picker.charm"));
        ui_test_click_header(ctx, "info_table_2_1", 6, header);
        ctx->Yield(2);
        IM_CHECK(v.ui.info.view.sort_column == 6 && v.ui.info.view.sort_descending);
        IM_CHECK_EQ(v.ui.info.ids[v.ui.info.rows[0]], 102u);              /* charm 60 first */
        ctx->ItemClick("**/###info_row101");
        ctx->Yield(2);
        IM_CHECK(v.ui.info.detail && v.ui.info.detail_id == 101);
        IM_CHECK(ctx->WindowInfo("//###InfoDetail").Window != 0);
        ctx->ItemClick("//###InfoDetail/**/###list100");                   /* the list at the left shows another officer */
        IM_CHECK_EQ(v.ui.info.detail_id, 100u);
        ctx->ItemClick("//###InfoDetail/###detail_back");
        ctx->Yield();
        IM_CHECK(!v.ui.info.detail && v.ui.info.screen == UI_INFO_OFFICERS);
        ctx->ItemClick("//###InfoTable/###info_back");
        ctx->Yield();
        IM_CHECK_EQ(v.ui.info.screen, (u8)UI_INFO_NONE);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* 都市一覽 and 勢力情報: a row opens 都市情報, a right click closes it;
       the faction screen lists the factions. */
    t = IM_REGISTER_TEST(e, "ui", "city_table_and_faction_screen_show_and_close");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.hud_mode            = true;
        ctx->Yield();
        ui_info_open(&v.ui, &v.api, UI_INFO_CITIES, false);
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        IM_CHECK_EQ(v.ui.info.id_count, 2u);
        ctx->ItemClick("//###InfoTable/**/###info_row1");
        ctx->Yield(2);
        IM_CHECK(v.ui.info.detail && v.ui.info.detail_id == 1);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_CITY_ECONOMY), 1u);   /* the fields of the city */
        ImVec2 display = ImGui::GetIO().DisplaySize;
        ctx->MouseMoveToPos(ImVec2(display.x * 0.02f, display.y * 0.98f));
        ctx->MouseClick(ImGuiMouseButton_Right);
        ctx->Yield(2);
        IM_CHECK(!v.ui.info.detail && v.ui.info.screen == UI_INFO_CITIES);  /* a right click closes the top window */
        ui_info_open(&v.ui, &v.api, UI_INFO_FACTION, false);
        ctx->Yield(2);
        IM_CHECK(ctx->WindowInfo("//###FactionInfo").Window != 0);
        ctx->ItemClick("//###FactionInfo/**/###faction1");
        IM_CHECK_EQ(v.ui.info.faction, 1u);
        ctx->ItemClick("//###FactionInfo/###faction_back");
        ctx->Yield();
        IM_CHECK_EQ(v.ui.info.screen, (u8)UI_INFO_NONE);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* 地圖 of the HUD opens 中地圖. The background changes, the region view
       draws the territory, and 返回 closes the map. */
    t = IM_REGISTER_TEST(e, "ui", "political_map_opens_from_the_hud_and_changes_its_background");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.hud_mode            = true;
        v.ui.show_map            = false;   /* the 2D panel of the debug layout */
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("//###HUD/###hud_map");
        ctx->Yield(2);
        IM_CHECK(v.ui.show_political && !v.ui.show_map);   /* the game mode opens 中地圖, not the 2D panel */
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_REGION_VIEW), 1u);
        ctx->SetRef("//###PoliticalMap");
        ctx->ItemClick("###bg_gold");
        ctx->ItemUncheck("###show_units");
        ctx->Yield();
        IM_CHECK(v.ui.political.background == UI_POLITICAL_GOLD && !v.ui.political.units);
        ctx->ItemClick("###political_back");
        ctx->Yield(2);
        IM_CHECK(!v.ui.show_political);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* main-map.md, "Hover panels": the panel of the city under the mouse. */
    t = IM_REGISTER_TEST(e, "ui", "hover_panel_shows_the_city_under_the_mouse");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.hud_mode            = true;
        v.ui.scene.headless      = true;
        v.ui.show_city = v.ui.show_map = v.ui.show_officers = v.ui.show_cities = false;
        ctx->Yield(2);
        hex_offset city_cell = { RECORDER_CITY_COL, RECORDER_CITY_ROW };
        ui_frame_hex(&v.ui, &v.api, hex_from_offset(city_cell));
        v.ui.scene.cam.target.y = 0.0f;
        ctx->Yield(3);
        recorder_reset(&v.recorder);
        ImVec2 display = ImGui::GetIO().DisplaySize;
        ctx->MouseMoveToPos(ImVec2(display.x * 0.5f, display.y * 0.5f));
        ctx->Yield(2);
        IM_CHECK(ctx->WindowInfo("//###HoverCity", ImGuiTestOpFlags_NoError).Window != 0);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_CITY_ECONOMY), 1u);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_CITY_SIEGE), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* ---- M19: the officer selection 選擇執行武將, the 開發 grid, the placement mode */

    /* 執行武將 opens the list with the proposal checked. A row click checks
       or clears the row. 決定 and 一併解除 are grey with no check. 決定
       writes the checks into the slots, and the dialog keeps them. */
    t = IM_REGISTER_TEST(e, "ui", "officer_pick_checks_rows_and_fills_the_command_dialog");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_command[UI_COMMAND_PATROL] = true;
        ctx->SetRef("###Patrol");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###executor");
        ctx->Yield(2);
        IM_CHECK(v.ui.show_picker && v.ui.picker.target == UI_COMMAND_PATROL && v.ui.picker.id_count == 3);
        IM_CHECK_EQ(v.ui.picker.checked_count, 3u);                                   /* the proposal is checked */
        ctx->SetRef("//###OfficerPick");
        ctx->ItemClick("###clear_all");
        ctx->Yield();
        IM_CHECK_EQ(v.ui.picker.checked_count, 0u);
        IM_CHECK((ctx->ItemInfo("###pick_decide").ItemFlags & ImGuiItemFlags_Disabled) != 0);
        IM_CHECK((ctx->ItemInfo("###clear_all").ItemFlags & ImGuiItemFlags_Disabled) != 0);
        ctx->ItemClick("**/###pick_row102");
        ctx->ItemClick("**/###pick_row100");
        ctx->ItemClick("**/###pick_row100");                                         /* a second click clears the row */
        ctx->ItemClick("**/###pick_row101");
        ctx->Yield();
        IM_CHECK(v.ui.picker.checked_count == 2 && v.ui.picker.checked[0] == 102 && v.ui.picker.checked[1] == 101);
        ctx->ItemClick("###pick_decide");
        ctx->Yield(2);
        u16 *slots = v.ui.commands[UI_COMMAND_PATROL].officers;
        IM_CHECK(!v.ui.show_picker);
        IM_CHECK(slots[0] == 102 && slots[1] == 101 && slots[2] == ENGINE_NO_OFFICER);   /* the dialog keeps the choice */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* A city with fewer free officers than slots: the proposal leaves the
       other slots empty, so that 開發 names no officer 0. */
    t = IM_REGISTER_TEST(e, "ui", "develop_proposal_with_one_free_officer_leaves_the_other_slots_empty");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 1;
        v.recorder.city_count    = 2;
        v.ui.show_develop        = true;
        ctx->Yield(2);
        IM_CHECK(v.ui.develop.officers[0] == 100 && v.ui.develop.officers[1] == ENGINE_NO_OFFICER && v.ui.develop.officers[2] == ENGINE_NO_OFFICER);
    };

    /* A header click sorts. 一併 checks the rows from the top, as many as the
       command takes: one for 商人. */
    t = IM_REGISTER_TEST(e, "ui", "officer_pick_header_sorts_and_check_all_takes_the_top_rows");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_command[UI_COMMAND_MERCHANT] = true;
        ctx->SetRef("###Merchant");
        ctx->Yield(2);
        ctx->ItemClick("###executor");
        ctx->Yield(2);
        ctx->SetRef("//###OfficerPick");
        IM_CHECK_EQ(v.ui.picker.places, 1u);
        char header[96];
        snprintf(header, sizeof(header), "%s###charm", text_get(&v.text, "picker.charm"));
        ui_test_click_header(ctx, "pick_table", UI_PICKER_COLUMN_CHARM, header);
        ctx->Yield(2);
        IM_CHECK(v.ui.picker.view.sort_column == UI_PICKER_COLUMN_CHARM && v.ui.picker.view.sort_descending);   /* the top value first */
        IM_CHECK_EQ(v.ui.picker.ids[v.ui.picker.rows[0]], 102u);                      /* charm 60, 50, 40 */
        recorder_reset(&v.recorder);
        ctx->ItemClick("###check_all");
        ctx->Yield();
        IM_CHECK(v.ui.picker.checked_count == 1 && v.ui.picker.checked[0] == 102);
        ctx->ItemClick("###pick_decide");
        ctx->Yield(2);
        IM_CHECK_EQ(v.ui.commands[UI_COMMAND_MERCHANT].officers[0], 102u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* The game mode: the 開發 grid shows the built count of each facility.
       決定 sends nothing and starts the placement. The 場所 arrows step
       through the empty slots. A click on an empty slot of the city sends
       one command_develop for that slot. */
    t = IM_REGISTER_TEST(e, "ui", "develop_grid_starts_the_placement_and_a_slot_click_sends_one_develop");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.hud_mode            = true;
        v.ui.scene.headless      = true;
        v.ui.show_city = v.ui.show_map = v.ui.show_officers = v.ui.show_cities = false;   /* nothing covers the map */
        v.ui.show_develop        = true;
        v.ui.develop.city        = 0;
        ctx->Yield(2);
        u32 counts[256], in_progress[256];
        ui_count_built_facilities(&v.api, 0, counts, in_progress, ArrayCount(counts));
        IM_CHECK_EQ(counts[0], 2u);                                                   /* the two built markets of the script */
        ctx->SetRef("###Develop");
        IM_CHECK(ctx->ItemExists("###fac0") && ctx->ItemExists("###fac2"));
        ctx->ItemClick("###fac2");
        ctx->Yield();
        IM_CHECK_EQ(v.ui.develop.facility, 2u);
        ctx->ItemClick("###executor");
        ctx->Yield(2);
        IM_CHECK(v.ui.show_picker && v.ui.picker.target == UI_PICKER_FOR_DEVELOP && v.ui.picker.places == 3);
        ctx->SetRef("//###OfficerPick");
        ctx->ItemClick("###pick_back");                                               /* 返回 keeps the slots */
        ctx->Yield();
        IM_CHECK(!v.ui.show_picker && v.ui.develop.officers[0] == 100);
        recorder_reset(&v.recorder);
        ctx->SetRef("###Develop");
        ctx->ItemClick("###decide");
        ctx->Yield(2);
        IM_CHECK(v.ui.placement.active && !v.ui.show_develop && v.ui.placement.facility == 2 && v.ui.placement.city == 0);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);                        /* 決定 waits for the slot */
        IM_CHECK_EQ(v.ui.placement.slot, 0u);                                         /* the first empty slot */
        ctx->SetRef("//###Placement");
        ctx->ItemClick("###slot_next");
        ctx->ItemClick("###slot_next");
        IM_CHECK_EQ(v.ui.placement.slot, 4u);                                         /* 2 and 3 hold markets */
        ctx->ItemClick("###slot_prev");
        IM_CHECK_EQ(v.ui.placement.slot, 1u);
        hex_offset slot_cell = { RECORDER_SLOT_COL, RECORDER_SLOT_ROW };
        ui_frame_hex(&v.ui, &v.api, hex_from_offset(slot_cell));                      /* the slot hex is at the centre of the screen */
        v.ui.scene.cam.target.y = 0.0f;
        ctx->Yield(3);
        recorder_reset(&v.recorder);
        ImVec2 display = ImGui::GetIO().DisplaySize;
        ctx->MouseMoveToPos(ImVec2(display.x * 0.5f, display.y * 0.5f));
        ctx->Yield(2);
        IM_CHECK(v.ui.placement.hover_valid);                                         /* the panel turns green */
        ctx->MouseClick(0);
        ctx->Yield(2);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_DEVELOP), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        engine_command sent = v.recorder.last_develop;
        IM_CHECK(sent.city_id == 0 && sent.facility == 2 && sent.slot == RECORDER_SLOT_INDEX && sent.officers[0] == 100);
        IM_CHECK(!v.ui.placement.active);
    };

    /* A click off the empty slots sends nothing and the placement waits.
       返回 stops it. */
    t = IM_REGISTER_TEST(e, "ui", "placement_ignores_other_hexes_and_back_stops_it");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.hud_mode            = true;
        v.ui.scene.headless      = true;
        v.ui.show_city = v.ui.show_map = v.ui.show_officers = v.ui.show_cities = false;
        v.ui.placement.active    = true;
        v.ui.placement.city      = 0;
        v.ui.placement.facility  = 1;
        v.ui.placement.slot      = ENGINE_ANY_SLOT;
        v.ui.placement.officers[0] = 100;
        v.ui.placement.officers[1] = v.ui.placement.officers[2] = ENGINE_NO_OFFICER;
        ctx->Yield(2);
        hex_offset city_cell = { RECORDER_CITY_COL, RECORDER_CITY_ROW };
        ui_frame_hex(&v.ui, &v.api, hex_from_offset(city_cell));                      /* the city hex, not a slot */
        v.ui.scene.cam.target.y = 0.0f;
        ctx->Yield(3);
        recorder_reset(&v.recorder);
        ImVec2 display = ImGui::GetIO().DisplaySize;
        ctx->MouseMoveToPos(ImVec2(display.x * 0.5f, display.y * 0.5f));
        ctx->MouseClick(0);
        ctx->Yield(2);
        IM_CHECK(v.ui.placement.active && !v.ui.placement.hover_valid && !v.ui.show_city);   /* no City dialog either */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        ctx->SetRef("//###Placement");
        ctx->ItemClick("###place_back");
        ctx->Yield();
        IM_CHECK(!v.ui.placement.active);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* ---- M3: the five city command dialogs, the economy panel, the ledger filter */

    t = IM_REGISTER_TEST(e, "ui", "city_panel_opens_each_command_dialog_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_city           = true;
        ctx->SetRef("###City");
        ctx->Yield();
        ctx->ComboClick("###city_select/###c1");
        recorder_reset(&v.recorder);
        for (u32 index = 0; index < ArrayCount(ui_command_cases); ++index)
        {
            ui_command_case *test_case = ui_command_cases + index;
            ctx->SetRef("###City");
            ctx->ItemClick(test_case->open_button);
            ctx->Yield();
            IM_CHECK(v.ui.show_command[test_case->ui_kind] && v.ui.commands[test_case->ui_kind].city == 1);   /* the dialog of this city */
        }
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    for (u32 index = 0; index < ArrayCount(ui_command_cases); ++index)
    {
        t = IM_REGISTER_TEST(e, "ui", ui_command_cases[index].decide_test);
        t->SetVarsDataType<ui_test_vars>();
        t->GuiFunc    = ui_test_gui;
        t->ArgVariant = (int)index;
        t->TestFunc   = [](ImGuiTestContext *ctx)
        {
            ui_test_vars &v = ctx->GetVars<ui_test_vars>();
            ui_command_case *test_case = ui_command_cases + ctx->Test->ArgVariant;
            u32 kind = test_case->ui_kind;
            v.recorder.officer_count = 3;
            v.recorder.city_count    = 2;
            v.ui.show_command[kind]  = true;
            ctx->SetRef(test_case->window);
            ctx->Yield(2);
            /* M19: the dialog proposes the most suitable officers, as many as
               the command takes. 徵兵 ranks by charm (102 has the most); the
               other abilities of the script tie or fall, so 100 comes first. */
            u16 first = kind == UI_COMMAND_CONSCRIPT ? 102 : 100;
            IM_CHECK_EQ(v.ui.commands[kind].officers[0], (u32)first);
            if (kind != UI_COMMAND_MERCHANT) IM_CHECK(v.ui.commands[kind].officers[1] == 101 && v.ui.commands[kind].officers[2] != ENGINE_NO_OFFICER);
            else                             IM_CHECK(v.ui.commands[kind].officers[1] == ENGINE_NO_OFFICER);
            IM_CHECK(ctx->ItemExists("###executor"));                                    /* 執行武將 */
            IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_CAN_ISSUE), 1u);
            if (kind != UI_COMMAND_MERCHANT) ctx->ComboClick("###officer3/###o_none");   /* the player can still change the proposal */
            if (kind == UI_COMMAND_PRODUCE)  ctx->ItemClick("###w2");
            if (kind == UI_COMMAND_MERCHANT) ctx->ItemInputValue("###trade", -500);   /* G2-031: left of the centre sells grain */
            ctx->Yield();
            IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);                      /* choices command nothing */
            recorder_reset(&v.recorder);
            ctx->ItemClick("###decide");                                                  /* 決定 */
            ctx->Yield();
            IM_CHECK_EQ(recorder_count(&v.recorder, recorder_call_of(test_case->engine_kind)), 1u);
            IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
            engine_command sent = v.recorder.last_city_command;
            IM_CHECK(sent.kind == test_case->engine_kind && sent.city_id == 0 && sent.officers[0] == first);
            if (kind != UI_COMMAND_MERCHANT) IM_CHECK(sent.officers[1] == 101 && sent.officers[2] == ENGINE_NO_OFFICER);
            if (kind == UI_COMMAND_PRODUCE)  IM_CHECK_EQ(sent.weapon, 2u);
            if (kind == UI_COMMAND_MERCHANT) IM_CHECK(sent.merchant_mode == SIM_MERCHANT_SELL && sent.amount == 500);
            IM_CHECK(!v.ui.show_command[kind]);                                          /* the dialog closes after 決定 */
            IM_CHECK(v.ui.command_message[0] != 0);                                      /* the City panel shows the result */
        };

        t = IM_REGISTER_TEST(e, "ui", ui_command_cases[index].back_test);
        t->SetVarsDataType<ui_test_vars>();
        t->GuiFunc    = ui_test_gui;
        t->ArgVariant = (int)index;
        t->TestFunc   = [](ImGuiTestContext *ctx)
        {
            ui_test_vars &v = ctx->GetVars<ui_test_vars>();
            ui_command_case *test_case = ui_command_cases + ctx->Test->ArgVariant;
            v.recorder.officer_count = 3;
            v.recorder.city_count    = 2;
            v.ui.show_command[test_case->ui_kind] = true;
            ctx->SetRef(test_case->window);
            ctx->Yield(2);
            recorder_reset(&v.recorder);
            ctx->ItemClick("###back");                                                    /* 返回 */
            ctx->Yield();
            IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
            IM_CHECK(!v.ui.show_command[test_case->ui_kind]);
        };
    }

    t = IM_REGISTER_TEST(e, "ui", "refused_city_command_is_disabled_and_issues_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count     = 3;
        v.recorder.city_count        = 2;
        v.recorder.can_issue_refusal = SIM_REFUSAL_LIMIT_REACHED;
        for (u32 index = 0; index < ArrayCount(ui_command_cases); ++index) v.ui.show_command[ui_command_cases[index].ui_kind] = true;
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        for (u32 index = 0; index < ArrayCount(ui_command_cases); ++index)
        {
            ctx->SetRef(ui_command_cases[index].window);
            ImGuiTestItemInfo decide = ctx->ItemInfo("###decide");
            IM_CHECK(decide.ID != 0 && (decide.ItemFlags & ImGuiItemFlags_Disabled) != 0);   /* 決定 is grey */
            ctx->ItemClick("###decide", 0, ImGuiTestOpFlags_NoAutoOpenFullPath);
        }
        ctx->Yield();
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* ---- M4: research, merge, the technique table, the settings ----------- */

    t = IM_REGISTER_TEST(e, "ui", "city_panel_opens_research_and_merge_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_city           = true;
        ctx->SetRef("###City");
        ctx->Yield();
        ctx->ComboClick("###city_select/###c1");
        recorder_reset(&v.recorder);
        ctx->ItemClick("###open_research");
        ctx->SetRef("###City");
        ctx->ItemClick("###open_merge");
        ctx->Yield();
        IM_CHECK(v.ui.show_research && v.ui.research.city == 1 && v.ui.show_merge && v.ui.merge.city == 1);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_CITY_DISASTERS), 1u);   /* the City panel shows the disasters of the city */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* G2-064: the research window stays on the screen in the game mode,
       also when the city menu opened at the left edge. Each technique
       shows its state: the faction has it (grey, green), its line needs
       the level before (grey), or it is open to research (white). */
    t = IM_REGISTER_TEST(e, "ui", "research_window_stays_on_screen_and_shows_the_state_of_each_technique");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.hud_mode            = true;
        ui_context_open(&v.ui, &v.api, UI_CONTEXT_MAP_MENU, 10.0f, 300.0f);   /* a click at the left edge */
        v.ui.show_research       = true;
        v.ui.research.city       = 0;
        ctx->Yield(3);
        ImGuiWindow *window = ctx->WindowInfo("###Research").Window;
        IM_CHECK(window != 0);
        IM_CHECK_GE(window->Pos.x, 0.0f);
        IM_CHECK_LE(window->Pos.x + window->Size.x, ImGui::GetIO().DisplaySize.x + 0.5f);
        ctx->SetRef("###Research");
        IM_CHECK((ctx->ItemInfo("###t0").ItemFlags & ImGuiItemFlags_Disabled) != 0);   /* the faction has technique 0 */
        IM_CHECK((ctx->ItemInfo("###t1").ItemFlags & ImGuiItemFlags_Disabled) == 0);   /* level 2 after level 1: open */
        IM_CHECK((ctx->ItemInfo("###t2").ItemFlags & ImGuiItemFlags_Disabled) != 0);   /* level 3 needs level 2: locked */
        IM_CHECK((ctx->ItemInfo("###t4").ItemFlags & ImGuiItemFlags_Disabled) == 0);   /* level 1 of the other line: open */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "research_decide_issues_one_command_research");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_research       = true;
        ctx->SetRef("###Research");
        ctx->Yield(2);
        /* M19: three officers by intellect; the script ties, so the table order */
        IM_CHECK(v.ui.research.officers[0] == 100 && v.ui.research.officers[1] == 101 && v.ui.research.officers[2] == 102);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_FACTION_TECHNIQUES), 1u);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_CAN_ISSUE), 1u);
        ctx->ItemClick("###t1");                                                  /* the second technique of the first line */
        ctx->ComboClick("###officer2/###o101");
        ctx->ComboClick("###officer3/###o_none");
        ctx->Yield();
        IM_CHECK(v.ui.research.technique == 1 && v.ui.research.officers[1] == 101);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);                    /* choices command nothing */
        recorder_reset(&v.recorder);
        ctx->ItemClick("###decide");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_RESEARCH), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        engine_command sent = v.recorder.last_research;
        IM_CHECK(sent.city_id == 0 && sent.technique == 1 && sent.officers[0] == 100 && sent.officers[1] == 101 &&
                 sent.officers[2] == ENGINE_NO_OFFICER);
        IM_CHECK(!v.ui.show_research && v.ui.command_message[0] != 0);           /* the dialog closes and the result shows */
    };

    t = IM_REGISTER_TEST(e, "ui", "merge_decide_issues_one_command_merge");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_merge          = true;
        ctx->SetRef("###Merge");
        ctx->Yield(2);
        IM_CHECK_EQ(v.ui.merge.officer, 100u);
        ctx->ComboClick("###absorber/###a2");
        ctx->ComboClick("###absorbed/###b3");
        ctx->ComboClick("###officer1/###o102");
        ctx->Yield();
        IM_CHECK(v.ui.merge.absorber == 2 && v.ui.merge.absorbed == 3 && v.ui.merge.officer == 102);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###decide");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_MERGE), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        engine_command sent = v.recorder.last_merge;
        IM_CHECK(sent.city_id == 0 && sent.slot == 2 && sent.absorbed == 3 && sent.officers[0] == 102);
        IM_CHECK(!v.ui.show_merge);
    };

    t = IM_REGISTER_TEST(e, "ui", "research_and_merge_back_issue_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_research       = true;
        v.ui.show_merge          = true;
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->SetRef("###Research");
        ctx->ItemClick("###back");
        ctx->SetRef("###Merge");
        ctx->ItemClick("###back");
        ctx->Yield();
        IM_CHECK(!v.ui.show_research && !v.ui.show_merge);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "refused_research_and_merge_are_disabled_and_issue_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count     = 3;
        v.recorder.city_count        = 2;
        v.recorder.can_issue_refusal = SIM_REFUSAL_NEEDS_TECHNIQUE;
        v.ui.show_research           = true;
        v.ui.show_merge              = true;
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        const char *windows[2] = { "###Research", "###Merge" };
        for (u32 index = 0; index < 2; ++index)
        {
            ctx->SetRef(windows[index]);
            ImGuiTestItemInfo decide = ctx->ItemInfo("###decide");
            IM_CHECK(decide.ID != 0 && (decide.ItemFlags & ImGuiItemFlags_Disabled) != 0);   /* 決定 is grey */
            ctx->ItemClick("###decide", 0, ImGuiTestOpFlags_NoAutoOpenFullPath);
        }
        ctx->Yield();
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "techniques_panel_reads_the_table_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_techniques     = true;
        ctx->SetRef("###Techniques");
        recorder_reset(&v.recorder);
        ctx->Yield(2);
        IM_CHECK(ctx->ItemExists("techniques"));
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_FACTION_TECHNIQUES), 1u);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_TECHNIQUE_KIND_AT), 8u);   /* each row of the table */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "disasters_check_box_calls_set_settings_once");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.settings.disasters = true;
        v.ui.show_settings            = true;
        ctx->SetRef("###Settings");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###disasters");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_SET_SETTINGS), 1u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_SET_SETTINGS), 0u);   /* the click turns the disasters off */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
    };

    t = IM_REGISTER_TEST(e, "ui", "simulation_panel_shows_the_outcome_through_the_query");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.outcome_kind = SIM_OUTCOME_VICTORY;
        recorder_reset(&v.recorder);
        ctx->Yield(2);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_OUTCOME), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* ---- M5: sortie, transport, the units on the map, the menu, the mini map */

    t = IM_REGISTER_TEST(e, "ui", "city_panel_opens_sortie_and_transport_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_city           = true;
        ctx->SetRef("###City");
        ctx->Yield();
        ctx->ComboClick("###city_select/###c1");
        recorder_reset(&v.recorder);
        ctx->ItemClick("###open_sortie");
        ctx->SetRef("###City");
        ctx->ItemClick("###open_transport");
        ctx->Yield();
        IM_CHECK(v.ui.show_sortie && v.ui.sortie.city == 1 && v.ui.show_transport && v.ui.transport.city == 1);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* The play report of 2026-10-23: a port or a pass of the player is a
       base of the spec. The City window with the location id of the port
       draws the port panel (depot_at) with 出征, and the sortie dialog of
       the base sends command_sortie with the location id of the base. The
       click on the port hex (ui_hud_scene_clicks) opens this window. */
    t = IM_REGISTER_TEST(e, "ui", "the_panel_of_an_own_port_offers_a_sortie_that_names_the_port");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.city.selected       = 42;   /* the port of recorder_base_at(0) */
        v.ui.show_city           = true;
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_DEPOT_AT), 1u);                   /* the panel of the port */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        ctx->SetRef("###City");
        IM_CHECK(ctx->ItemExists("###open_sortie"));
        ctx->ItemClick("###open_sortie");
        ctx->Yield();
        IM_CHECK(v.ui.show_sortie && v.ui.sortie.city == 42);
        ctx->SetRef("###Sortie");
        ctx->Yield(2);
        ctx->ItemClick("###preset1");
        ctx->Yield();
        IM_CHECK_EQ(v.ui.sortie.order.officers[0], 100u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###decide");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_SORTIE), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK_EQ(v.recorder.last_sortie.city_id, 42u);
    };

    t = IM_REGISTER_TEST(e, "ui", "sortie_decide_issues_one_command_sortie");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_sortie         = true;
        ctx->SetRef("###Sortie");
        ctx->Yield(2);
        /* G2-044: the slots are empty at the open, and 決定 is grey. The preset of a class fills them. */
        IM_CHECK_EQ(v.ui.sortie.order.officers[0], (u16)ENGINE_NO_OFFICER);
        IM_CHECK((ctx->ItemInfo("###decide").ItemFlags & ImGuiItemFlags_Disabled) != 0);
        ctx->ItemClick("###preset1");
        ctx->Yield();
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_SORTIE_PROPOSAL), 1u);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_UNIT_VALUES), 2u);           /* the values on land and on the water */
        IM_CHECK_EQ(v.ui.sortie.order.officers[0], 100u);
        IM_CHECK(v.ui.sortie.order.soldiers == 5000 && v.ui.sortie.order.grain == 1500 && v.ui.sortie.order.gold == 1000);
        ctx->ItemClick("###u2");                                                        /* the halberd class: 1000 halberds in the stock */
        ctx->ComboClick("###officer2/###o101");
        ctx->ItemInputValue("###soldiers", 2000);
        ctx->Yield();
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###decide");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_SORTIE), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        engine_sortie_order sent = v.recorder.last_sortie;
        IM_CHECK(sent.city_id == 0 && sent.unit_type == 2 && sent.soldiers == 2000 && sent.officers[0] == 100 && sent.officers[1] == 101);
        IM_CHECK(sent.ship == 0);
        IM_CHECK(!v.ui.show_sortie);
    };

    /* G2-044, G2-057: a class without weapons in the city is grey, and so
       is its preset. The ship row offers the boat always and a ship when
       the city holds one; the chosen ship goes into the order. */
    t = IM_REGISTER_TEST(e, "ui", "sortie_greys_a_class_without_weapons_and_sends_the_chosen_ship");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_sortie         = true;
        ctx->SetRef("###Sortie");
        ctx->Yield(2);
        IM_CHECK((ctx->ItemInfo("###u1").ItemFlags & ImGuiItemFlags_Disabled) == 0);        /* pikes: 3000 in the stock */
        IM_CHECK((ctx->ItemInfo("###u3").ItemFlags & ImGuiItemFlags_Disabled) != 0);        /* crossbows: none */
        IM_CHECK((ctx->ItemInfo("###preset3").ItemFlags & ImGuiItemFlags_Disabled) != 0);   /* and its preset */
        IM_CHECK((ctx->ItemInfo("###ship9").ItemFlags & ImGuiItemFlags_Disabled) == 0);     /* the boat, always */
        IM_CHECK((ctx->ItemInfo("###ship10").ItemFlags & ImGuiItemFlags_Disabled) == 0);    /* a tower ship in the stock */
        ctx->ItemClick("###preset1");
        ctx->ItemClick("###ship10");
        ctx->Yield();
        IM_CHECK_EQ(v.ui.sortie.order.ship, 10u);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###decide");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_SORTIE), 1u);
        IM_CHECK_EQ(v.recorder.last_sortie.ship, 10u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
    };

    t = IM_REGISTER_TEST(e, "ui", "transport_decide_issues_one_command_transport");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_transport      = true;
        ctx->SetRef("###Transport");
        ctx->Yield(2);
        ctx->ComboClick("###destination/###d1");
        ctx->ItemInputValue("###grain", 800);
        ctx->ItemInputValue("###cargo0", 300);
        ctx->Yield();
        recorder_reset(&v.recorder);
        ctx->ItemClick("###decide");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_TRANSPORT), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        engine_sortie_order sent = v.recorder.last_sortie;
        IM_CHECK(sent.city_id == 0 && sent.destination == 1 && sent.grain == 800 && sent.cargo[0] == 300 && sent.officers[0] == 100);
    };

    /* G2-045: a click on a hex of the range opens the command menu at that
       hex, with no command yet. The choice (待命) sends the move and the
       action together, and the unit walks the plan of path_to hex by hex. */
    t = IM_REGISTER_TEST(e, "ui", "map_click_opens_the_menu_at_the_destination_and_the_choice_moves_the_unit");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        ctx->Yield();
        recorder_reset(&v.recorder);
        ui_test_click_cell(ctx, v, hex_make(1, 2));
        IM_CHECK(v.ui.units.selected == 0 && v.ui.show_unit && !v.ui.show_unit_menu);   /* a click on an own unit selects it */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        ctx->Yield();
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_REACHABLE), 1u);                /* the map shows its range */
        recorder_reset(&v.recorder);
        ui_test_click_cell(ctx, v, hex_make(3, 2));
        ctx->Yield();
        IM_CHECK(v.ui.show_unit_menu && v.ui.units.has_pending_move);                    /* the menu, at the destination */
        IM_CHECK(v.ui.units.pending_move.q == 3 && v.ui.units.pending_move.r == 2);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);                            /* no move yet */
        IM_CHECK(!ui_walk_active(&v.ui));
        ctx->SetRef("//###UnitMenu");
        ctx->ItemClick("###wait");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_MOVE), 1u);              /* the move, then 待命 */
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_WAIT), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 2u);
        IM_CHECK(v.recorder.last_unit_command.unit == 0 && v.recorder.last_unit_command.q == 3 && v.recorder.last_unit_command.r == 2);
        IM_CHECK(!v.ui.units.has_pending_move && !v.ui.show_unit_menu);
        /* The unit walks the plan of path_to hex by hex: (1,2), (2,2), (3,2). */
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_PATH_TO), 1u);
        IM_CHECK(ui_walk_active(&v.ui) && v.ui.war.walk.unit == 0 && v.ui.war.walk.count == 3);
        hex_coord shown = hex_make(0, 0);
        IM_CHECK(ui_walk_hex(&v.ui, 0, &shown) && shown.q == 1 && shown.r == 2);   /* the start hex first */
        u32 frames = 0;
        for (; frames < 600 && ui_walk_active(&v.ui); ++frames) ctx->Yield();
        IM_CHECK(frames > 2 && !ui_walk_active(&v.ui));                                   /* the pause of the steps took frames */
        IM_CHECK(!v.ui.show_unit_menu);                                                   /* the turn of the unit is done */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 2u);                             /* the walk commanded nothing */
    };

    /* G2-045: a right click on the menu at the destination leaves the unit in place. */
    t = IM_REGISTER_TEST(e, "ui", "right_click_on_the_menu_at_the_destination_cancels_the_move");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        ctx->Yield();
        ui_test_click_cell(ctx, v, hex_make(1, 2));
        ui_test_click_cell(ctx, v, hex_make(3, 2));
        ctx->Yield();
        IM_CHECK(v.ui.show_unit_menu && v.ui.units.has_pending_move);
        /* The game mode: the right click of ui_context_update goes back one level. */
        v.ui.hud_mode = true;
        ui_context_open(&v.ui, &v.api, UI_CONTEXT_UNIT, 300.0f, 200.0f);
        v.ui.show_unit_menu = true;
        ctx->Yield(3);
        recorder_reset(&v.recorder);
        ImVec2 display = ImGui::GetIO().DisplaySize;
        ctx->MouseMoveToPos(ImVec2(display.x * 0.8f, display.y * 0.3f));
        IM_CHECK_EQ(v.ui.context.kind, (u8)UI_CONTEXT_UNIT);
        ctx->MouseClick(ImGuiMouseButton_Right);
        ctx->Yield(2);
        IM_CHECK(!v.ui.show_unit_menu);
        IM_CHECK(!v.ui.units.has_pending_move);
        IM_CHECK(v.ui.units.selected == 0);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);                            /* the unit did not move */
        IM_CHECK(!ui_walk_active(&v.ui));
    };

    /* G2-045: 攻擊 from the menu at the destination: the target click sends
       the move and then the attack. The strike waits for the walk. */
    t = IM_REGISTER_TEST(e, "ui", "attack_from_the_menu_at_the_destination_moves_then_strikes");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.recorder.unit1_rival   = true;   /* the rival unit 1 at (1,3): two hexes from (3,2) */
        ctx->Yield();
        ui_test_click_cell(ctx, v, hex_make(1, 2));
        ui_test_click_cell(ctx, v, hex_make(3, 2));
        ctx->Yield();
        IM_CHECK(v.ui.show_unit_menu && v.ui.units.has_pending_move);
        recorder_reset(&v.recorder);
        ctx->SetRef("//###UnitMenu");
        ctx->ItemClick("###attack");
        ctx->Yield();
        IM_CHECK(v.ui.combat.pick == UI_PICK_ATTACK && v.ui.units.has_pending_move);     /* the pick waits; no move yet */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        ui_test_click_cell(ctx, v, hex_make(1, 3));
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_MOVE), 1u);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_ATTACK), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 2u);
        IM_CHECK(v.ui.combat.pick == UI_PICK_NONE && !v.ui.units.has_pending_move);
        IM_CHECK(ui_walk_active(&v.ui) && v.ui.war.walk.has_strike);                      /* the strike plays after the walk */
        IM_CHECK_EQ(v.ui.war.strike_count, 0u);
        u32 frames = 0;
        for (; frames < 600 && ui_walk_active(&v.ui); ++frames) ctx->Yield();
        IM_CHECK(!ui_walk_active(&v.ui) && v.ui.war.strike_count == 1u);
    };

    /* G3-009: an enemy unit that marches on an own city gives a line of the
       turn start, after the reports and before the closing line. The
       governor of the target city speaks, and the camera goes to the unit. */
    t = IM_REGISTER_TEST(e, "ui", "turn_start_reports_an_enemy_unit_that_marches_on_an_own_city");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count  = 3;
        v.recorder.city_count     = 2;
        v.recorder.unit1_rival    = true;
        v.recorder.unit1_goal     = true;
        v.recorder.unit1_goal_city = 0;   /* the own city 0 */
        v.ui.feel.turn_messages = true;
        v.ui.round.show         = true;
        ui_enter_game(&v.ui);
        ctx->Yield(3);
        IM_CHECK_EQ(v.ui.feel.message_count, 3u);   /* the heading, the unit threat, the closing line */
        IM_CHECK_EQ(v.ui.feel.messages[1], UI_MESSAGE_UNIT_THREAT | 1u);
        IM_CHECK_EQ(ui_feel_message_speaker(&v.ui, &v.api, v.ui.feel.messages[1]), 100);   /* the governor of city 0 */
        char line[256];
        ui_feel_message_text(&v.ui, &v.api, v.ui.feel.messages[1], line, sizeof(line));
        IM_CHECK(strstr(line, "\xE6\x80\xA5\xE5\xA0\xB1") != 0);   /* zh-TW: 急報 */
    };

    /* The player, 2026-10-24: a click on an own port or pass in the range
       asks 是否進入 instead of the command menu; 是 sends one command_move
       onto the base, and the selection ends. */
    t = IM_REGISTER_TEST(e, "ui", "a_click_on_an_own_port_in_the_range_asks_before_the_entry");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.recorder.own_base_hex  = true;   /* the port of recorder_base_at(0) stands at (3, 2), in the range of unit 0 */
        ctx->Yield();
        ui_test_click_cell(ctx, v, hex_make(1, 2));
        recorder_reset(&v.recorder);
        ui_test_click_cell(ctx, v, hex_make(3, 2));
        ctx->Yield();
        IM_CHECK(v.ui.units.target_kind == UI_TARGET_BASE && !v.ui.show_unit_menu && !v.ui.units.has_pending_move);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        ctx->SetRef("//###UnitTarget");
        ctx->ItemClick("###base_yes");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_MOVE), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.recorder.last_unit_command.q == 3 && v.recorder.last_unit_command.r == 2);
        IM_CHECK(v.ui.units.target_kind == UI_TARGET_NONE && v.ui.units.selected == ENGINE_NO_UNIT);
    };

    /* The player, 2026-10-24: a left click on an own port in the scene
       opens its panel. hex_at gives the location id 42, and base_at needs
       the index 0 of base_at_hex. */
    t = IM_REGISTER_TEST(e, "ui", "a_scene_click_on_an_own_port_opens_its_panel");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.recorder.own_base_hex  = true;
        v.ui.hud_mode            = true;
        v.ui.scene.headless      = true;
        v.ui.show_city = v.ui.show_map = v.ui.show_officers = v.ui.show_cities = false;
        ctx->Yield(2);
        ui_frame_hex(&v.ui, &v.api, hex_make(3, 2));
        v.ui.scene.cam.target.y = 0.0f;
        ctx->Yield(3);
        ImVec2 display = ImGui::GetIO().DisplaySize;
        ctx->MouseMoveToPos(ImVec2(display.x * 0.5f, display.y * 0.5f));
        ctx->Yield(2);
        IM_CHECK(v.ui.scene.has_hover && hex_equal(v.ui.scene.hover, hex_make(3, 2)));
        recorder_reset(&v.recorder);
        ctx->MouseClick(ImGuiMouseButton_Left);
        ctx->Yield(2);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_BASE_AT_HEX), 1u);
        IM_CHECK(v.ui.city.selected == 42 && v.ui.context.kind == UI_CONTEXT_CITY);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* The player, 2026-10-24: after the selected unit acted, a click on
       another own unit selects that unit and asks for no follow order. */
    t = IM_REGISTER_TEST(e, "ui", "a_click_on_another_own_unit_after_the_action_selects_it");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.recorder.units_acted   = true;
        ctx->Yield();
        ui_test_click_cell(ctx, v, hex_make(1, 2));
        IM_CHECK_EQ(v.ui.units.selected, 0u);
        recorder_reset(&v.recorder);
        ui_test_click_cell(ctx, v, hex_make(1, 3));   /* unit 1 of the same force */
        ctx->Yield();
        IM_CHECK_EQ(v.ui.units.selected, 1u);
        IM_CHECK(v.ui.units.target_kind == UI_TARGET_NONE);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* G3-020: a click on an enemy facility out of the range asks the question
       of the attack order; 是 sends one command_attack_facility on the hex. */
    t = IM_REGISTER_TEST(e, "ui", "far_click_on_an_enemy_facility_asks_and_orders_the_attack");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.recorder.foreign_city  = 1;   /* the city 1 belongs to the force 2, and its market stands at (6,4) */
        ctx->Yield();
        ui_test_click_cell(ctx, v, hex_make(1, 2));
        hex_offset market_offset;
        market_offset.col = 6;
        market_offset.row = 4;
        hex_coord market = hex_from_offset(market_offset);
        ui_test_click_cell(ctx, v, market);
        ctx->Yield();
        IM_CHECK(v.ui.units.target_kind == UI_TARGET_FACILITY);
        recorder_reset(&v.recorder);
        ctx->SetRef("//###UnitTarget");
        ctx->ItemClick("###facility_yes");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_ATTACK_FACILITY), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.recorder.last_unit_command.q == market.q && v.recorder.last_unit_command.r == market.r);
        IM_CHECK(v.ui.units.target_kind == UI_TARGET_NONE);
    };

    /* 俘虜處置: a capture by the player opens the dialog for the officer;
       the choice issues one command_captive, and the dialog closes. */
    t = IM_REGISTER_TEST(e, "ui", "capture_opens_the_prisoner_dialog_and_the_choice_issues_one_command");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count   = 3;
        v.recorder.city_count      = 2;
        v.recorder.captive_officer = 102;
        sim_event captured = {};
        captured.kind = SIM_EVENT_OFFICER_CAPTURED; captured.faction = 1; captured.subject = 1002;   /* the officer 102, by the player 1 */
        v.recorder.events[0] = captured;
        v.recorder.event_count = 1;
        ctx->Yield(3);
        IM_CHECK(v.ui.show_captive && v.ui.captive.count == 1 && v.ui.captive.queue[0] == 102);
        recorder_reset(&v.recorder);
        ctx->SetRef("//###Captive");
        ctx->ItemClick("###release");
        ctx->Yield(2);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_CAPTIVE), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.recorder.last_captive_choice == ENGINE_CAPTIVE_RELEASE);
        IM_CHECK(!v.ui.show_captive && v.ui.captive.count == 0);
    };

    /* The sprite of a unit follows the key of its class (ui_war.cpp). Each
       class has its own sprite since the art of 2026-10-24. */
    t = IM_REGISTER_TEST(e, "ui", "unit_sprite_follows_the_class_of_the_unit");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        ctx->Yield();
        u32 expected[] = { RENDER_SPRITE_SWORD, RENDER_SPRITE_PIKE, RENDER_SPRITE_HALBERD, RENDER_SPRITE_CROSSBOW, RENDER_SPRITE_CAVALRY,
                           RENDER_SPRITE_RAM, RENDER_SPRITE_WOOD_BEAST, RENDER_SPRITE_TOWER, RENDER_SPRITE_CATAPULT,
                           RENDER_SPRITE_BOAT, RENDER_SPRITE_TOWER_SHIP, RENDER_SPRITE_BATTLE_SHIP, RENDER_SPRITE_TRANSPORT };
        engine_unit unit = {};
        for (u32 type = 0; type < ArrayCount(expected); ++type)
        {
            unit.unit_type = (u8)type;
            IM_CHECK_EQ(ui_unit_sprite(&v.api, &unit), expected[type]);
        }
        unit.unit_type = ENGINE_NO_UNIT_TYPE;
        IM_CHECK_EQ(ui_unit_sprite(&v.api, &unit), (u32)RENDER_SPRITE_UNIT);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_UNIT_TYPE_AT), (u32)ArrayCount(expected));
        /* A land unit on water is a 走舸 (18-battle.md): its sprite is the
           boat, not its weapon. A ship on water keeps its own sprite, and
           a land unit on land keeps its weapon. */
        v.recorder.has_water = true;
        v.recorder.water_q = 5; v.recorder.water_r = 1;
        unit.q = 5; unit.r = 1;
        unit.unit_type = 1;   /* pike */
        IM_CHECK_EQ(ui_unit_sprite(&v.api, &unit), (u32)RENDER_SPRITE_BOAT);
        unit.unit_type = 4;   /* cavalry */
        IM_CHECK_EQ(ui_unit_sprite(&v.api, &unit), (u32)RENDER_SPRITE_BOAT);
        unit.ship_type = 10;  /* a land unit that rows a 樓船 from the stock shows the ship (2026-10-24) */
        IM_CHECK_EQ(ui_unit_sprite(&v.api, &unit), (u32)RENDER_SPRITE_TOWER_SHIP);
        unit.ship_type = ENGINE_NO_UNIT_TYPE;
        unit.unit_type = 10;  /* tower ship */
        IM_CHECK_EQ(ui_unit_sprite(&v.api, &unit), (u32)RENDER_SPRITE_TOWER_SHIP);
        unit.q = 2; unit.r = 2;
        unit.unit_type = 1;
        IM_CHECK_EQ(ui_unit_sprite(&v.api, &unit), (u32)RENDER_SPRITE_PIKE);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* A left click on the plate of an own unit (the portrait and the name)
       selects it, also when the plate covers the sky. A second click on
       the plate opens the command menu. The clicks command nothing. */
    t = IM_REGISTER_TEST(e, "ui", "a_click_on_a_unit_plate_selects_the_unit_and_a_second_click_opens_its_menu");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.hud_mode         = true;
        v.ui.scene.headless   = true;
        v.ui.show_city        = false;
        v.ui.show_map         = false;
        v.ui.show_officers    = v.ui.show_cities = false;   /* nothing covers the map */
        ctx->Yield(2);
        ui_frame_unit(&v.ui, 0);   /* the camera looks at unit 0, so its plate is on the screen */
        ctx->Yield(3);
        ui_select_unit(&v.ui, ENGINE_NO_UNIT);   /* then nothing is selected; the camera stays */
        v.ui.scene.framed_unit = ENGINE_NO_UNIT;
        ctx->Yield(3);
        /* The plate of unit 0, as the last frame drew it. The portrait is at its left edge. */
        u32 plate = UI_PLATE_CAPACITY;
        for (u32 index = 0; index < v.ui.units.plate_count; ++index) if (v.ui.units.plates[index].unit == 0) plate = index;
        IM_CHECK(plate != UI_PLATE_CAPACITY);
        ImVec2 portrait(v.ui.units.plates[plate].x0 + 10.0f, (v.ui.units.plates[plate].y0 + v.ui.units.plates[plate].y1) * 0.5f);
        ImVec2 name(v.ui.units.plates[plate].x1 - 30.0f, v.ui.units.plates[plate].y0 + 6.0f);
        ctx->MouseMoveToPos(portrait);
        ctx->Yield(2);
        IM_CHECK(!ImGui::GetIO().WantCaptureMouse);
        recorder_reset(&v.recorder);
        ctx->MouseClick(ImGuiMouseButton_Left);
        IM_CHECK(v.ui.units.selected == 0 && v.ui.show_unit && !v.ui.show_unit_menu);   /* the portrait selects */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        ctx->Yield(2);
        ctx->MouseMoveToPos(name);
        ctx->Yield(2);
        IM_CHECK(!ImGui::GetIO().WantCaptureMouse);   /* the Unit panel does not cover the plate */
        ctx->MouseClick(ImGuiMouseButton_Left);
        IM_CHECK(v.ui.units.selected == 0 && v.ui.show_unit_menu);                    /* the name opens the menu */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* An attack pick sends one command_attack and shows a strike on the
       target hex for a few seconds (ui_war.cpp). */
    t = IM_REGISTER_TEST(e, "ui", "an_attack_shows_a_strike_on_the_target_hex_for_a_few_seconds");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.recorder.unit1_rival   = true;
        v.ui.units.selected      = 0;
        v.ui.combat.pick         = UI_PICK_ATTACK;
        ctx->Yield();
        recorder_reset(&v.recorder);
        ui_test_click_cell(ctx, v, hex_make(1, 3));
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_ATTACK), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.ui.war.strike_count == 1 && v.ui.war.strikes[0].kind == UI_STRIKE_ATTACK);
        IM_CHECK(v.ui.war.strikes[0].hex.q == 1 && v.ui.war.strikes[0].hex.r == 3);
        IM_CHECK(v.ui.war.strikes[0].seconds > UI_STRIKE_SECONDS - 0.1f);
        u32 frames = 0;
        for (; frames < 1200 && v.ui.war.strike_count; ++frames) ctx->Yield();
        IM_CHECK(frames > 60 && v.ui.war.strike_count == 0);   /* it stays for seconds and then ends */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK_EQ(ui_strike_of_pick(UI_PICK_TACTIC), (u8)UI_STRIKE_TACTIC);
        IM_CHECK_EQ(ui_strike_of_pick(UI_PICK_PLOT), (u8)UI_STRIKE_PLOT);
        IM_CHECK_EQ(ui_strike_of_pick(UI_PICK_BUILD), (u8)UI_STRIKE_KIND_COUNT);
    };

    /* The soldiers and the morale that a unit loses or gains rise as values
       over the unit, read from the ledger. After the value, the plate marks
       the changed soldiers for a few seconds. */
    t = IM_REGISTER_TEST(e, "ui", "unit_hits_rise_from_the_ledger_and_then_mark_the_plate");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.scene.headless      = true;
        ui_frame_unit(&v.ui, 0);   /* the camera looks at unit 0 */
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        sim_event damaged = {};
        damaged.kind = SIM_EVENT_UNIT_DAMAGED; damaged.faction = 1; damaged.detail = 0; damaged.delta = -495; damaged.cause = 7; damaged.turn = 1;
        sim_event morale = damaged;
        morale.kind = SIM_EVENT_MORALE_CHANGED; morale.delta = -10;
        sim_event quiet = damaged;
        quiet.kind = SIM_EVENT_UNIT_MOVED; quiet.delta = 12;   /* no hit */
        v.recorder.events[v.recorder.event_count++] = damaged;
        v.recorder.events[v.recorder.event_count++] = morale;
        v.recorder.events[v.recorder.event_count++] = quiet;
        ctx->Yield(2);
        IM_CHECK_EQ(v.ui.war.hit_count, 2u);
        IM_CHECK(v.ui.war.hits[0].unit == 0 && v.ui.war.hits[0].kind == UI_HIT_SOLDIERS && v.ui.war.hits[0].delta == -495);
        IM_CHECK(v.ui.war.hits[1].unit == 0 && v.ui.war.hits[1].kind == UI_HIT_MORALE && v.ui.war.hits[1].delta == -10);
        IM_CHECK_EQ(ui_hit_mark(&v.ui, 0, (u8)UI_HIT_SOLDIERS), 0);   /* the value rises first; the plate marks nothing yet */
        u32 frames = 0;
        for (; frames < 1200 && ui_hit_mark(&v.ui, 0, (u8)UI_HIT_SOLDIERS) == 0 && v.ui.war.hit_count; ++frames) ctx->Yield();
        IM_CHECK(frames > 30 && ui_hit_mark(&v.ui, 0, (u8)UI_HIT_SOLDIERS) == -495);   /* then the plate marks the loss */
        IM_CHECK_EQ(ui_hit_mark(&v.ui, 0, (u8)UI_HIT_MORALE), -10);
        IM_CHECK(ui_hit_colour(-495, IM_COL32_WHITE) != IM_COL32_WHITE && ui_hit_colour(10, IM_COL32_WHITE) != ui_hit_colour(-10, IM_COL32_WHITE));
        IM_CHECK_EQ(ui_hit_colour(0, IM_COL32_WHITE), (ImU32)IM_COL32_WHITE);
        for (frames = 0; frames < 1200 && v.ui.war.hit_count; ++frames) ctx->Yield();
        IM_CHECK(frames > 30 && v.ui.war.hit_count == 0);                             /* and the mark ends */
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_UNIT_AT), 1u);                /* the value sits over the unit */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* ---- clone gap report 2, group 2: the unit orders ---------------------- */

    /* G2-052: a click on an enemy unit out of the range opens the target
       menu; 殲滅 sends one command_destroy and ends the selection. */
    t = IM_REGISTER_TEST(e, "ui", "target_menu_on_an_enemy_unit_sends_one_destroy_command");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.recorder.unit0_rival   = true;   /* unit 0 at 1,2 belongs to force 2 */
        ctx->Yield();
        ui_test_click_cell(ctx, v, hex_make(1, 3));   /* selects the own unit 1; its range is its hex */
        IM_CHECK(v.ui.units.selected == 1);
        recorder_reset(&v.recorder);
        ui_test_click_cell(ctx, v, hex_make(1, 2));
        ctx->Yield(2);
        IM_CHECK(v.ui.units.target_kind == UI_TARGET_ENEMY && v.ui.units.target_unit == 0);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        IM_CHECK(ctx->WindowInfo("//###UnitTarget").Window != 0);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_PATH_TO), 1u);   /* the panel shows the plan */
        ctx->ItemClick("//###UnitTarget/###destroy");
        ctx->Yield(2);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_DESTROY), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.recorder.last_unit_command.unit == 1 && v.recorder.last_unit_command.target_unit == 0);
        IM_CHECK(v.ui.units.target_kind == UI_TARGET_NONE && v.ui.units.selected == ENGINE_NO_UNIT);
    };

    /* G2-052: a click on an own unit with a unit selected asks for the
       follow order; 是 sends one command_follow, 否 keeps the selection. */
    t = IM_REGISTER_TEST(e, "ui", "a_click_on_an_own_unit_asks_for_the_follow_order");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        ctx->Yield();
        ui_test_click_cell(ctx, v, hex_make(1, 2));   /* selects the own unit 0 */
        IM_CHECK(v.ui.units.selected == 0);
        recorder_reset(&v.recorder);
        ui_test_click_cell(ctx, v, hex_make(1, 3));   /* unit 1, own */
        ctx->Yield(2);
        IM_CHECK(v.ui.units.target_kind == UI_TARGET_FOLLOW && v.ui.units.target_unit == 1 && v.ui.units.selected == 0);
        ctx->ItemClick("//###UnitTarget/###follow_no");
        ctx->Yield(2);
        IM_CHECK(v.ui.units.target_kind == UI_TARGET_NONE && v.ui.units.selected == 0);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        ui_test_click_cell(ctx, v, hex_make(1, 3));
        ctx->Yield(2);
        ctx->ItemClick("//###UnitTarget/###follow_yes");
        ctx->Yield(2);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_FOLLOW), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.recorder.last_unit_command.unit == 0 && v.recorder.last_unit_command.target_unit == 1);
    };

    /* G2-058: a click on a far hex opens 移動 and 設置; 移動 sends one
       command_move to that hex. */
    t = IM_REGISTER_TEST(e, "ui", "target_menu_on_a_far_hex_sends_one_move_command");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        ctx->Yield();
        ui_test_click_cell(ctx, v, hex_make(1, 2));   /* selects the own unit 0 */
        IM_CHECK(v.ui.units.selected == 0);
        recorder_reset(&v.recorder);
        ui_test_click_cell(ctx, v, hex_make(6, 2));   /* out of the range 1,2 .. 3,2 */
        ctx->Yield(2);
        IM_CHECK(v.ui.units.target_kind == UI_TARGET_FAR && v.ui.units.target_hex.q == 6 && v.ui.units.target_hex.r == 2);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        ImGuiTestItemInfo build = ctx->ItemInfo("//###UnitTarget/###far_build");
        IM_CHECK(build.ID != 0 && (build.ItemFlags & ImGuiItemFlags_Disabled) != 0);
        ctx->ItemClick("//###UnitTarget/###far_move");
        ctx->Yield(2);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_MOVE), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.recorder.last_unit_command.q == 6 && v.recorder.last_unit_command.r == 2);
        IM_CHECK(v.ui.units.target_kind == UI_TARGET_NONE && v.ui.units.selected == ENGINE_NO_UNIT);
    };


    t = IM_REGISTER_TEST(e, "ui", "unit_menu_wait_and_retreat_issue_one_command_each");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.recorder.unit1_rival   = true;   /* G2-045: unit 0 of force 2 stands next to unit 1: the attack items are white */
        v.ui.units.selected      = 0;
        v.ui.show_unit_menu      = true;
        ctx->SetRef("###UnitMenu");
        ctx->Yield(2);
        ImGuiTestItemInfo attack = ctx->ItemInfo("###attack"), duel = ctx->ItemInfo("###duel");
        IM_CHECK(attack.ID != 0 && (attack.ItemFlags & ImGuiItemFlags_Disabled) == 0);   /* 攻擊 works since M6 */
        IM_CHECK(duel.ID != 0 && (duel.ItemFlags & ImGuiItemFlags_Disabled) == 0);       /* 單挑 works since M7 */
        recorder_reset(&v.recorder);
        ctx->ItemClick("###wait");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_WAIT), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(!v.ui.show_unit_menu);
        v.ui.show_unit_menu = true;
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->SetRef("###UnitMenu");
        ctx->ItemClick("###retreat");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_RETREAT), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
    };

    t = IM_REGISTER_TEST(e, "ui", "unit_menu_attack_waits_for_a_hex_and_issues_one_command_attack");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.recorder.unit1_rival   = true;   /* G2-045: an enemy next to the unit makes the attack white */
        v.ui.units.selected      = 0;
        v.ui.show_unit_menu      = true;
        ctx->SetRef("###UnitMenu");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###attack");
        ctx->Yield();
        IM_CHECK(v.ui.combat.pick == UI_PICK_ATTACK && !v.ui.show_unit_menu);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);                          /* the menu only starts the pick */
        ctx->Yield();
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_CAN_ISSUE), 1u);                /* the map marks the targets */
        recorder_reset(&v.recorder);
        ui_test_click_cell(ctx, v, hex_make(2, 2));
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_ATTACK), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.recorder.last_unit_command.unit == 0 && v.recorder.last_unit_command.q == 2 && v.recorder.last_unit_command.r == 2);
        IM_CHECK(v.ui.combat.pick == UI_PICK_NONE);
    };

    t = IM_REGISTER_TEST(e, "ui", "unit_menu_tactic_list_and_hex_issue_one_command_tactic");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.recorder.unit1_rival   = true;   /* G2-045: an enemy next to the unit makes 戰法 white */
        v.ui.units.selected      = 0;
        v.ui.show_unit_menu      = true;
        ctx->SetRef("###UnitMenu");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###tactic");
        ctx->Yield(2);
        IM_CHECK(v.ui.show_actions && v.ui.combat.list == UI_PICK_TACTIC);
        IM_CHECK(v.ui.show_unit_menu);                                                    /* G2-054: the tactics are a column next to the menu */
        ctx->SetRef("###Actions");
        IM_CHECK(ctx->ItemExists("###a0") && !ctx->ItemExists("###a1"));               /* only the tactics of the class of the unit */
        ctx->ItemClick("###a0");
        ctx->Yield();
        IM_CHECK(v.ui.combat.pick == UI_PICK_TACTIC && v.ui.combat.row == 0 && !v.ui.show_actions && !v.ui.show_unit_menu);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        ui_test_click_cell(ctx, v, hex_make(1, 3));                                        /* the rival unit 1 */
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_TACTIC), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.recorder.last_unit_command.row == 0 && v.recorder.last_unit_command.q == 1 && v.recorder.last_unit_command.r == 3);
    };

    /* G2-054: a tactic without a target in its pattern is grey. The row
       shows the cost in brackets. During the pick, the scene marks the
       hexes that the tactic can hit, and the status bar names the pick. */
    t = IM_REGISTER_TEST(e, "ui", "tactic_column_greys_a_tactic_without_a_target_and_marks_the_targets");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        memory_arena frame_arena = test_arena_create(Megabytes(2));
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.recorder.unit1_rival   = true;
        v.ui.scene.headless      = true;
        v.frame.frame_arena      = &frame_arena;
        v.ui.units.selected      = 0;
        v.ui.show_unit_menu      = true;
        ctx->SetRef("###UnitMenu");
        ctx->Yield(2);
        v.recorder.can_issue_refusal = SIM_REFUSAL_UNREACHABLE;   /* no hex of the pattern holds a target */
        ctx->ItemClick("###tactic");
        ctx->Yield(2);
        ctx->SetRef("###Actions");
        IM_CHECK(ctx->ItemExists("###a0"));
        IM_CHECK((ctx->ItemInfo("###a0").ItemFlags & ImGuiItemFlags_Disabled) != 0);   /* grey */
        v.recorder.can_issue_refusal = SIM_REFUSAL_NONE;             /* the rival at 1,3 is a target */
        ctx->Yield(2);
        IM_CHECK((ctx->ItemInfo("###a0").ItemFlags & ImGuiItemFlags_Disabled) == 0);
        ctx->ItemClick("###a0");
        ctx->Yield(2);
        IM_CHECK(v.ui.combat.pick == UI_PICK_TACTIC);
        IM_CHECK(v.frame.render && v.frame.render->overlay);
        hex_offset rival = hex_to_offset(hex_make(1, 3)), empty = hex_to_offset(hex_make(2, 2));
        IM_CHECK_EQ(v.frame.render->overlay[(u32)rival.row * 8 + (u32)rival.col], 0xB03C3CFFu);   /* the target hex shows red */
        IM_CHECK_NE(v.frame.render->overlay[(u32)empty.row * 8 + (u32)empty.col], 0xB03C3CFFu);   /* an empty hex keeps the range colour */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        v.frame.frame_arena = 0;
        test_arena_destroy(&frame_arena);
    };

    t = IM_REGISTER_TEST(e, "ui", "unit_menu_build_list_and_hex_issue_one_command_build");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.units.selected      = 0;
        v.ui.show_unit_menu      = true;
        ctx->SetRef("###UnitMenu");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###build");
        ctx->Yield(2);
        ctx->SetRef("###Actions");
        IM_CHECK(ctx->ItemExists("###a0") && !ctx->ItemExists("###a1"));               /* no natural trap in the list */
        ctx->ItemClick("###a0");
        ctx->Yield();
        ui_test_click_cell(ctx, v, hex_make(3, 2));
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_BUILD), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.recorder.last_unit_command.row == 0 && v.recorder.last_unit_command.q == 3 && v.recorder.last_unit_command.r == 2);
    };

    /* U-002 (play report 2026-10-24): a building that the engine refuses
       on every hex next to the unit is grey. The list asks can_issue, and
       holds no copy of the rule. */
    t = IM_REGISTER_TEST(e, "ui", "build_list_greys_a_building_that_the_engine_refuses");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.units.selected      = 0;
        v.ui.show_unit_menu      = true;
        ctx->SetRef("###UnitMenu");
        ctx->Yield(2);
        v.recorder.can_issue_refusal = SIM_REFUSAL_NEEDS_TECHNIQUE;   /* no technique, no gold or no buildable hex */
        ctx->ItemClick("###build");
        ctx->Yield(2);
        ctx->SetRef("###Actions");
        IM_CHECK(ctx->ItemExists("###a0"));
        IM_CHECK((ctx->ItemInfo("###a0").ItemFlags & ImGuiItemFlags_Disabled) != 0);   /* grey */
        recorder_reset(&v.recorder);
        v.recorder.can_issue_refusal = SIM_REFUSAL_NONE;
        ctx->Yield(2);
        IM_CHECK((ctx->ItemInfo("###a0").ItemFlags & ImGuiItemFlags_Disabled) == 0);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_CAN_ISSUE), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* U-003 (play report 2026-10-24): the tactics follow the class that the
       unit fights with on its hex. A land unit on a ship on water lists the
       tactics of the ship, not the tactics of its land class. */
    t = IM_REGISTER_TEST(e, "ui", "tactic_list_follows_the_class_that_the_unit_fights_with");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.recorder.unit1_rival   = true;
        v.ui.units.selected      = 0;
        v.ui.show_unit_menu      = true;
        ctx->SetRef("###UnitMenu");
        ctx->Yield(2);
        ctx->ItemClick("###tactic");
        ctx->Yield(2);
        ctx->SetRef("###Actions");
        IM_CHECK(ctx->ItemExists("###a0") && !ctx->ItemExists("###a2"));               /* on land: 突刺 of the pike, no ship tactic */
        v.recorder.unit0_effective_type = 10;                                            /* the unit rows a 樓船 on water */
        ctx->Yield(4);                                                                   /* past the cache of the item infos */
        IM_CHECK(ctx->ItemExists("###a2"));                                              /* on water: the 火矢 of the ship */
        IM_CHECK(!ctx->ItemExists("###a0"));                                             /* and no 突刺 of the pike */
        recorder_reset(&v.recorder);
        ctx->ItemClick("###a2");
        ctx->Yield();
        IM_CHECK(v.ui.combat.pick == UI_PICK_TACTIC && v.ui.combat.row == 2);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "unit_menu_duel_waits_for_a_hex_and_issues_one_command_duel");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.recorder.unit1_rival   = true;   /* G2-045: an enemy next to the unit makes 單挑 white */
        v.ui.units.selected      = 0;
        v.ui.show_unit_menu      = true;
        ctx->SetRef("###UnitMenu");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###duel");
        ctx->Yield();
        IM_CHECK(v.ui.combat.pick == UI_PICK_DUEL);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        /* G3-008: the click on the target opens the list of the officers with 成功率; a row sends the command. */
        ui_test_click_cell(ctx, v, hex_make(1, 3));
        ctx->Yield();
        IM_CHECK(v.ui.combat.duel_pick && v.ui.combat.pick == UI_PICK_NONE);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        ctx->SetRef("//###DuelPick");
        ctx->ItemClick("###duel_officer_1");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_DUEL), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.recorder.last_unit_command.row == 1 && v.recorder.last_unit_command.q == 1 && v.recorder.last_unit_command.r == 3);
        IM_CHECK(!v.ui.combat.duel_pick);
    };

    t = IM_REGISTER_TEST(e, "ui", "court_officer_view_asks_for_the_portrait_of_the_officer");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_court          = true;
        v.ui.court.officer       = 101;
        /* Officer 101 of the recorder has the Big5 name B1 41. A picture
           with that name comes after a numbered picture of another name. */
        const u8 big5_name[2] = { 0xB1, 0x41 };
        char name[16], path[64];
        text_decode_big5_into(big5_name, 2, name, sizeof(name));
        memset(&g_ui_test_portraits, 0, sizeof(g_ui_test_portraits));
        const char *other = "Test/01 Other \xE7\x84\xA1.jpg";
        portrait_list_add(&g_ui_test_portraits, 0, other, (u32)strlen(other));
        snprintf(path, sizeof(path), "Test/02 Officer %s.jpg", name);
        portrait_list_add(&g_ui_test_portraits, 1, path, (u32)strlen(path));
        IM_CHECK_EQ(g_ui_test_portraits.count, 2u);
        g_ui_test_portrait_requests = 0;
        g_ui_test_portrait_last     = PORTRAIT_NONE;
        v.frame.portraits        = &g_ui_test_portraits;
        v.frame.portrait_texture = ui_test_portrait_texture;
        ctx->SetRef("###Court");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK(g_ui_test_portrait_requests > 0);
        IM_CHECK_EQ(g_ui_test_portrait_last, 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* An officer without a picture of its name gets a generated face
       (render/faces.h): the UI asks the platform for the face of the
       officer with the key of its description, and for no picture. */
    t = IM_REGISTER_TEST(e, "ui", "court_officer_view_asks_for_a_generated_face_without_a_picture");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_court          = true;
        v.ui.court.officer       = 101;
        memset(&g_ui_test_portraits, 0, sizeof(g_ui_test_portraits));
        const char *other = "Test/01 Other \xE7\x84\xA1.jpg";
        portrait_list_add(&g_ui_test_portraits, 0, other, (u32)strlen(other));
        g_ui_test_portrait_requests = 0;
        g_ui_test_face_requests     = 0;
        v.frame.portraits        = &g_ui_test_portraits;
        v.frame.portrait_texture = ui_test_portrait_texture;
        v.frame.face_texture     = ui_test_face_texture;
        ctx->SetRef("###Court");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK_EQ(g_ui_test_portrait_requests, 0u);
        IM_CHECK(g_ui_test_face_requests > 0);
        IM_CHECK_EQ(g_ui_test_face_last_officer, 101u);
        /* The recorder gives officer 101 the age 30, a man, with no personality tint. */
        face_description expected = face_describe(101, false, 30, 0);
        IM_CHECK_EQ(g_ui_test_face_last_key, face_key_pack(&expected));
        IM_CHECK(recorder_count(&v.recorder, RECORDER_OFFICER_DETAIL) > 0);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* Without the archive, the face stands in for every officer. */
    t = IM_REGISTER_TEST(e, "ui", "court_officer_view_asks_for_a_generated_face_without_the_archive");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_court          = true;
        v.ui.court.officer       = 102;
        g_ui_test_face_requests  = 0;
        v.frame.portraits        = 0;
        v.frame.portrait_texture = 0;
        v.frame.face_texture     = ui_test_face_texture;
        ctx->SetRef("###Court");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK(g_ui_test_face_requests > 0);
        IM_CHECK_EQ(g_ui_test_face_last_officer, 102u);
        IM_CHECK(g_ui_test_face_last_key & FACE_KEY_VALID);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "court_reward_and_appoint_issue_one_command_court_each");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_court          = true;
        v.ui.court.officer       = 101;
        ctx->SetRef("###Court");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###reward");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_COURT), 1u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_COMMAND_COURT), (u32)ENGINE_COMMAND_REWARD);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.recorder.last_court.rewards[0] == 101 && v.recorder.last_court.reward_count == 1 && v.recorder.last_court.city_id == 0);
        ctx->ComboClick("###post/###p1");
        ctx->Yield();
        recorder_reset(&v.recorder);
        ctx->ItemClick("###appoint");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_COURT), 1u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_COMMAND_COURT), (u32)ENGINE_COMMAND_APPOINT);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.recorder.last_court.target == 101 && v.recorder.last_court.post == 1);
    };

    t = IM_REGISTER_TEST(e, "ui", "diplomacy_goodwill_issues_one_command_court");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_diplomacy      = true;
        v.ui.diplomacy.envoy     = 101;
        v.ui.diplomacy.force     = 2;
        ctx->SetRef("###Diplomacy");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###goodwill");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_COURT), 1u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_COMMAND_COURT), (u32)ENGINE_COMMAND_DIPLOMACY);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.recorder.last_court.kind == SIM_DIPLOMACY_GOODWILL && v.recorder.last_court.force == 2 && v.recorder.last_court.officer == 101);
    };

    /* G2-063: the envoy picker lists the free officers of the selected
       city. The scripted officers stand in city 0, so city 1 lists none. */
    t = IM_REGISTER_TEST(e, "ui", "diplomacy_envoy_picker_lists_the_free_officers_of_the_selected_city");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_diplomacy      = true;
        v.ui.city.selected       = 0;
        ctx->SetRef("###Diplomacy");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ComboClick("###envoy/###o101");
        IM_CHECK_EQ(v.ui.diplomacy.envoy, (u16)101);
        v.ui.city.selected = 1;
        ctx->Yield(2);
        ctx->ItemClick("###envoy");
        ctx->Yield();
        IM_CHECK(ctx->ItemInfo("//##Combo_00/**/###o101", ImGuiTestOpFlags_NoError).ID == 0);
        ctx->KeyPress(ImGuiKey_Escape);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "duel_screen_policy_special_and_round_issue_one_command_each");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.recorder.duel_active   = true;
        ctx->Yield(2);
        IM_CHECK(v.ui.show_duel);                                       /* the screen opens when a duel runs */
        ctx->SetRef("###Duel");
        static const char *items[3] = { "###policy0", "###special0", "###duel_round" };
        static const u32 args[3] = { SIM_DUEL_POLICY | (DUEL_ATTACK << 8), SIM_DUEL_SPECIAL | (DUEL_FINISHER << 8), SIM_DUEL_ROUND };
        for (u32 index = 0; index < 3; ++index)
        {
            recorder_reset(&v.recorder);
            ctx->ItemClick(items[index]);
            ctx->Yield();
            IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_DUEL_ACTION), 1u);
            IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_COMMAND_DUEL_ACTION), args[index]);
            IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        }
        ImGuiTestItemInfo peerless = ctx->ItemInfo("###special5");
        IM_CHECK(peerless.ID != 0 && (peerless.ItemFlags & ImGuiItemFlags_Disabled) != 0);   /* 無雙 is grey without spirit 300 */
    };

    t = IM_REGISTER_TEST(e, "ui", "debate_screen_card_issues_one_command_debate_action");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.recorder.debate_active = true;
        ctx->Yield(2);
        IM_CHECK(v.ui.show_debate);
        ctx->SetRef("###Debate");
        recorder_reset(&v.recorder);
        ctx->ItemClick("###card1");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_DEBATE_ACTION), 1u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_COMMAND_DEBATE_ACTION), (u32)(SIM_DEBATE_PLAY | (1 << 8)));
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
    };

    t = IM_REGISTER_TEST(e, "ui", "ruler_corps_create_and_dissolve_issue_one_command_corps_each");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_ruler          = true;
        v.ui.ruler.viceroy       = 101;
        v.ui.ruler.cities        = 1ull << 1;
        ctx->SetRef("###Ruler");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###corps_create");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_CORPS), 1u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_COMMAND_CORPS), (u32)CORPS_CREATE);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.recorder.last_corps.viceroy == 101 && v.recorder.last_corps.cities == (1ull << 1) && v.recorder.last_corps.flags == CORPS_FLAG_ALL);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###corps_dissolve");
        ctx->Yield();
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_COMMAND_CORPS), (u32)CORPS_DISSOLVE);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.recorder.last_corps.corps == 1);
    };

    t = IM_REGISTER_TEST(e, "ui", "ruler_adviser_and_council_issue_one_command_each");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_ruler          = true;
        v.ui.ruler.adviser       = 100;
        ctx->SetRef("###Ruler");
        ctx->Yield(2);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_ADVICE), 1u);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###appoint_adviser");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_ADVISER), 1u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_COMMAND_ADVISER), 100u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###council1");   /* the ruler (officer 100) stands in city 0 */
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_COUNCIL), 1u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_COMMAND_COUNCIL), 1u | (0u << 8));
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
    };

    t = IM_REGISTER_TEST(e, "ui", "event_dialog_choice_issues_one_command_event_choice");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count  = 3;
        v.recorder.city_count     = 2;
        v.recorder.pending_active = true;
        ctx->Yield(2);
        ctx->SetRef("###Event");
        IM_CHECK(ctx->ItemExists("###choice0") && ctx->ItemExists("###choice1") && !ctx->ItemExists("###choice2"));
        recorder_reset(&v.recorder);
        ctx->ItemClick("###choice1");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_EVENT_CHOICE), 1u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_COMMAND_EVENT_CHOICE), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
    };

    t = IM_REGISTER_TEST(e, "ui", "bases_panel_and_minimap_read_bases_and_command_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_bases          = true;
        v.ui.show_minimap        = true;
        recorder_reset(&v.recorder);
        ctx->Yield(2);
        ctx->SetRef("###Bases");
        IM_CHECK(ctx->ItemExists("bases_table"));
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_BASE_AT), 4u);   /* the table and the mini map */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* M13: the Fidelity panel lists the pairs; Replay sends one replay_pair with the two saves of the row. */
    t = IM_REGISTER_TEST(e, "ui", "fidelity_panel_replays_the_pair_of_the_row");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_fidelity       = true;
        recorder_reset(&v.recorder);
        ctx->Yield(2);
        ctx->SetRef("###Fidelity");
        IM_CHECK(ctx->ItemExists("fidelity_pairs/###replay1"));
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_REPLAY_PAIR_AT), 2u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        recorder_reset(&v.recorder);
        ctx->ItemClick("fidelity_pairs/###replay0");
        ctx->Yield(2);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_REPLAY_PAIR), 1u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_REPLAY_PAIR), (u32)ENGINE_REPLAY_DEVELOP);
        IM_CHECK_STR_EQ(v.recorder.last_replay_before, "pairs/develop/001/before.s11");
        IM_CHECK_STR_EQ(v.recorder.last_replay_after, "pairs/develop/001/after.s11");
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(ctx->ItemExists("fidelity_diffs"));
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_REPLAY_DIFF_AT), 1u);
    };

    /* The Replay panel lists the steps of the game. The buttons, the rows
       and the turn jump each ask for one view. Live closes the view. No
       control commands the game. */
    t = IM_REGISTER_TEST(e, "ui", "replay_panel_steps_through_the_game");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_replay         = true;
        recorder_reset(&v.recorder);
        ctx->Yield(2);
        ctx->SetRef("###GameReplay");
        IM_CHECK(ctx->ItemExists("replay_steps/###replay_step5"));
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_GAME_REPLAY_STEP_AT), (u32)RECORDER_REPLAY_STEPS);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_GAME_REPLAY_VIEW), 0u);   /* the open shows the live game */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);

        recorder_reset(&v.recorder);
        ctx->ItemClick("###replay_back");
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_GAME_REPLAY_VIEW), 1u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_GAME_REPLAY_VIEW), 5u);   /* the live game is step 6 */
        ctx->ItemClick("###replay_back");
        ctx->ItemClick("###replay_forward");
        IM_CHECK_EQ(recorder_count_arg(&v.recorder, RECORDER_GAME_REPLAY_VIEW, 4), 1u);
        IM_CHECK_EQ(recorder_count_arg(&v.recorder, RECORDER_GAME_REPLAY_VIEW, 5), 2u);
        IM_CHECK(v.ui.replay.viewing && v.ui.replay.position == 5);

        recorder_reset(&v.recorder);
        ctx->ItemClick("###replay_first");
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_GAME_REPLAY_VIEW), 0u);
        recorder_reset(&v.recorder);
        ctx->ItemClick("replay_steps/###replay_step1");
        ctx->Yield(2);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_GAME_REPLAY_VIEW), 2u);   /* a row shows the state after its step */
        IM_CHECK_GE(recorder_count_arg(&v.recorder, RECORDER_EVENT_REPORT, 0), 1u);    /* the log lines of the step */

        recorder_reset(&v.recorder);
        ctx->ItemInputValue("###replay_turn", 3);
        ctx->ItemClick("###replay_go");
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_GAME_REPLAY_TURN_STEP), 3u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_GAME_REPLAY_VIEW), 4u);   /* the first step of turn 3 */

        recorder_reset(&v.recorder);
        ctx->ItemClick("###replay_live");
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_GAME_REPLAY_CLOSE), 1u);
        IM_CHECK(!v.ui.replay.viewing);
        ctx->ItemClick("###replay_last");
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_GAME_REPLAY_VIEW), 6u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);

        /* A close of the panel returns to the live game. */
        recorder_reset(&v.recorder);
        v.ui.show_replay = false;
        ctx->Yield(2);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_GAME_REPLAY_CLOSE), 1u);
        IM_CHECK(!v.recorder.replay_viewing);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* M15: the AI panel reads the record of the last AI turn of the chosen
       faction. It shows one row for each option and commands nothing. */
    t = IM_REGISTER_TEST(e, "ui", "ai_panel_reads_the_record_of_the_faction");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_ai             = true;
        v.ui.ai.faction          = 2;
        recorder_reset(&v.recorder);
        ctx->Yield(2);
        ctx->SetRef("###AI");
        IM_CHECK(ctx->ItemExists("ai_options"));
        IM_CHECK(ctx->ItemExists("ai_options/###ai_option0"));
        IM_CHECK(ctx->ItemExists("ai_options/###ai_option2"));
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_AI_EXPLAIN), 1u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_AI_EXPLAIN), 2u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* M15: a row of the AI panel frames its unit or its city; it commands nothing. */
    t = IM_REGISTER_TEST(e, "ui", "ai_panel_row_frames_the_unit_or_the_city");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_ai             = true;
        v.ui.ai.faction          = 2;
        ctx->Yield(2);
        ctx->SetRef("###AI");
        recorder_reset(&v.recorder);
        ctx->ItemClick("ai_options/###ai_option1");
        ctx->Yield(2);
        IM_CHECK_EQ(v.ui.units.selected, 3u);
        ctx->ItemClick("ai_options/###ai_option0");
        ctx->Yield(2);
        IM_CHECK_EQ(v.ui.city.selected, (u8)27);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* M16: the Content panel lists the source of each asset. Set sends one
       set_install_folder with the typed folder; Clear sends one with "". */
    t = IM_REGISTER_TEST(e, "ui", "content_panel_sets_and_clears_the_install_folder");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_content        = true;
        recorder_reset(&v.recorder);
        ctx->Yield(2);
        ctx->SetRef("###Content");
        IM_CHECK(ctx->ItemExists("###set_install") && ctx->ItemExists("###clear_install"));
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_CONTENT_SOURCE), (u32)ENGINE_ASSET_KIND_COUNT);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        ctx->ItemInputValue("###install_folder", "D:/San11WPK");
        ctx->Yield();
        recorder_reset(&v.recorder);
        ctx->ItemClick("###set_install");
        ctx->Yield(2);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_SET_INSTALL_FOLDER), 1u);
        IM_CHECK_STR_EQ(v.recorder.last_install_folder, "D:/San11WPK");
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###clear_install");
        ctx->Yield(2);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_SET_INSTALL_FOLDER), 1u);
        IM_CHECK_STR_EQ(v.recorder.last_install_folder, "");
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
    };

    /* M16: a scenario of the install has a Start button that sends one
       load_scenario with the file of the row. */
    t = IM_REGISTER_TEST(e, "ui", "content_panel_starts_a_scenario_of_the_install");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_content        = true;
        recorder_reset(&v.recorder);
        ctx->Yield(2);
        ctx->SetRef("###Content");
        IM_CHECK(ctx->ItemExists("install_scenarios/###start0"));
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_INSTALL_SCENARIO_AT), 1u);
        recorder_reset(&v.recorder);
        ctx->ItemClick("install_scenarios/###start0");
        ctx->Yield(2);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_LOAD_SCENARIO), 1u);
        IM_CHECK_STR_EQ(v.recorder.last_scenario_path, RECORDER_INSTALL_SCENARIO_FILE);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
    };

    /* M16: the officer view of the Court panel shows the portrait of the
       source that content_source gives. Each source has its own frame. The
       view commands nothing. */
    t = IM_REGISTER_TEST(e, "ui", "officer_view_shows_a_portrait_of_each_source");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_court          = true;
        v.ui.court.officer       = 101;
        ctx->SetRef("###Court");
        for (u8 source = 0; source < ENGINE_SOURCE_KIND_COUNT; ++source)
        {
            v.recorder.portrait_source = source;
            ctx->Yield(2);
            recorder_reset(&v.recorder);
            ctx->Yield(2);
            char item[32];
            snprintf(item, sizeof(item), "###portrait_%u", source);
            IM_CHECK(ctx->ItemExists(item));
            IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_CONTENT_SOURCE), 1u);
            IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_CONTENT_SOURCE), (u32)ENGINE_ASSET_PORTRAITS);
            IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        }
    };

    /* M14: in the game mode the City dialog shows the base command menu.
       Each item opens the dialog of its command for the selected city, or
       it is grey. Opening a dialog commands nothing. */
    for (u32 index = 0; index < ArrayCount(ui_base_menu_cases); ++index)
    {
        t = IM_REGISTER_TEST(e, "ui", ui_base_menu_cases[index].test);
        t->SetVarsDataType<ui_test_vars>();
        t->GuiFunc    = ui_test_gui;
        t->ArgVariant = (int)index;
        t->TestFunc   = [](ImGuiTestContext *ctx)
        {
            ui_test_vars &v = ctx->GetVars<ui_test_vars>();
            ui_base_menu_case *test_case = ui_base_menu_cases + ctx->Test->ArgVariant;
            v.recorder.officer_count = 3;
            v.recorder.city_count    = 2;
            v.ui.hud_mode            = true;
            v.ui.city.selected       = 1;
            ctx->Yield();
            ui_context_open(&v.ui, &v.api, UI_CONTEXT_CITY, 300.0f, 200.0f);
            ctx->Yield(2);
            recorder_reset(&v.recorder);
            ctx->SetRef("//###City");
            IM_CHECK(!ctx->ItemExists("###open_develop"));                     /* the game mode has no flat buttons */
            if (strcmp(test_case->menu, test_case->item) != 0)
            {
                ctx->ItemClick(test_case->menu);
                ctx->Yield();
                u8 expected_menu = (u8)(strstr(test_case->menu, "military") ? UI_BASE_MENU_MILITARY : strstr(test_case->menu, "people") ? UI_BASE_MENU_PEOPLE :
                                        strstr(test_case->menu, "plot") ? UI_BASE_MENU_PLOT : strstr(test_case->menu, "ruler") ? UI_BASE_MENU_RULER : UI_BASE_MENU_CITY);
                IM_CHECK_EQ(v.ui.city.menu, expected_menu);
            }
            if (test_case->expect == UI_EXPECT_GREY)
            {
                IM_CHECK((ctx->ItemInfo(test_case->item).ItemFlags & ImGuiItemFlags_Disabled) != 0);
                IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
                return;
            }
            /* M19: an earlier open of city 1 left officer 0 in the slots; each
               open proposes again (city 1 has no free officer). */
            v.ui.develop.city = v.ui.develop.prepared_city = 1;
            v.ui.develop.officers[0] = 0;
            for (u32 kind = 0; kind < UI_COMMAND_COUNT; ++kind)
            {
                v.ui.commands[kind].city = v.ui.commands[kind].prepared_city = 1;
                v.ui.commands[kind].officers[0] = 0;
            }
            ctx->ItemClick(test_case->item);
            ctx->Yield(2);
            switch (test_case->expect)
            {
                case UI_EXPECT_DEVELOP:   IM_CHECK(v.ui.show_develop && v.ui.develop.city == 1 && v.ui.develop.officers[0] == ENGINE_NO_OFFICER); break;
                case UI_EXPECT_MERGE:     IM_CHECK(v.ui.show_merge && v.ui.merge.city == 1); break;
                case UI_EXPECT_COMMAND:   IM_CHECK(v.ui.show_command[test_case->ui_kind] && v.ui.commands[test_case->ui_kind].city == 1 &&
                                                   v.ui.commands[test_case->ui_kind].officers[0] == ENGINE_NO_OFFICER); break;
                case UI_EXPECT_RESEARCH:  IM_CHECK(v.ui.show_research && v.ui.research.city == 1); break;
                case UI_EXPECT_SORTIE:    IM_CHECK(v.ui.show_sortie && v.ui.sortie.city == 1); break;
                case UI_EXPECT_TRANSPORT: IM_CHECK(v.ui.show_transport && v.ui.transport.city == 1); break;
                case UI_EXPECT_COURT:     IM_CHECK(v.ui.show_court); break;
                case UI_EXPECT_DIPLOMACY: IM_CHECK(v.ui.show_diplomacy); break;
                case UI_EXPECT_RULER:     IM_CHECK(v.ui.show_ruler); break;
                case UI_EXPECT_MOVE:      IM_CHECK(v.ui.show_move_officers && v.ui.move.city == 1 && v.ui.move.count == 0); break;
                case UI_EXPECT_SUMMON:    IM_CHECK(v.ui.show_summon && v.ui.summon.city == 1); break;
                case UI_EXPECT_PLOT:      IM_CHECK(v.ui.show_plot && v.ui.plot.city == 1 && v.ui.plot.kind == test_case->ui_kind &&
                                                   v.ui.plot.target_city == ENGINE_NO_CITY); break;
                case UI_EXPECT_POSTS:     IM_CHECK(v.ui.show_posts && v.ui.posts.staged_count == 0); break;
                case UI_EXPECT_EXILE:     IM_CHECK(v.ui.show_exile && v.ui.exile.officer == ENGINE_NO_OFFICER); break;
                case UI_EXPECT_ABILITIES: IM_CHECK(v.ui.show_abilities); break;
                case UI_EXPECT_REWARD:    IM_CHECK(v.ui.show_reward && v.ui.reward.city == 1 && v.ui.reward.count == 2); break;   /* the open checks 101 and 102 */
                default:                  IM_CHECK(false); break;
            }
            IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        };
    }

    t = IM_REGISTER_TEST(e, "ui", "title_new_game_flow_loads_chooses_rulers_and_sets_settings");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_title          = true;
        ctx->SetRef("###Title");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###title_new");
        ctx->Yield(2);
        IM_CHECK(v.ui.show_scenarios && recorder_count_commands(&v.recorder) == 0u);
        ctx->SetRef("###Scenarios");
        IM_CHECK((ctx->ItemInfo("###sc1").ItemFlags & ImGuiItemFlags_Disabled) != 0);   /* a PK scenario without its file */
        ctx->ItemClick("###sc0");
        ctx->Yield(2);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_LOAD_SAVE), 1u);
        IM_CHECK(v.ui.show_rulers && !v.ui.show_scenarios);
        ctx->SetRef("###Rulers");
        ctx->ItemClick("###ruler1");
        recorder_reset(&v.recorder);
        ctx->ItemClick("###rulers_ok");
        ctx->Yield(2);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_SET_FACTION_HUMAN), 1u);
        IM_CHECK(v.ui.show_start_settings);
        ctx->SetRef("###StartSettings");
        ctx->ItemClick("###level2");
        recorder_reset(&v.recorder);
        ctx->ItemClick("###start_ok");
        ctx->Yield(2);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_SET_SETTINGS), 1u);
        IM_CHECK(v.recorder.settings.difficulty == DIFFICULTY_HARD && v.ui.hud_mode && v.ui.show_minimap && !v.ui.show_title);   /* the game mode */
    };

    t = IM_REGISTER_TEST(e, "ui", "topbar_proceed_issues_one_end_turn");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_topbar         = true;
        v.ui.show_history        = true;
        ctx->SetRef("###TopBar");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###proceed");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_END_TURN), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
    };

    t = IM_REGISTER_TEST(e, "ui", "save_screen_slot_and_comment_issue_one_save_game");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_topbar         = true;
        ctx->SetRef("###TopBar");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###open_save");
        ctx->Yield(2);
        IM_CHECK(v.ui.show_saveload && recorder_count(&v.recorder, RECORDER_REFRESH_SAVE_SLOTS) == 1u && recorder_count_commands(&v.recorder) == 0u);
        ctx->SetRef("###SaveLoad");
        ctx->ItemClick("###slot2");
        ctx->Yield();
        ctx->ItemInputValue("###comment", "spring");
        recorder_reset(&v.recorder);
        ctx->ItemClick("###save_ok");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_SAVE_GAME), 1u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_SAVE_GAME), 2u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK_STR_EQ(v.recorder.last_save_comment, "spring");
    };

    t = IM_REGISTER_TEST(e, "ui", "load_screen_slot_issues_one_load_game");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_title          = true;
        ctx->SetRef("###Title");
        ctx->Yield(2);
        ctx->ItemClick("###title_load");
        ctx->Yield(2);
        ctx->SetRef("###SaveLoad");
        IM_CHECK((ctx->ItemInfo("###slot0").ItemFlags & ImGuiItemFlags_Disabled) != 0);   /* an empty slot does not load */
        recorder_reset(&v.recorder);
        ctx->ItemClick("###slot1");
        ctx->Yield(2);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_LOAD_GAME), 1u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_LOAD_GAME), 1u);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_LOAD_NAME_INDEX), 1u);   /* the game has no names yet: they load in the same step */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 2u);
        IM_CHECK(v.ui.hud_mode && !v.ui.show_saveload);   /* the load starts the game mode */
        v.recorder.has_names = true;                       /* a second load keeps the names */
        recorder_reset(&v.recorder);
        ui_open_saveload(&v.ui, &v.api, false);
        ctx->Yield(2);
        ctx->ItemClick("###slot1");
        ctx->Yield(2);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_LOAD_NAME_INDEX), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "new_officer_dialog_issues_one_create_officer");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_new_officer    = true;
        v.ui.game.city           = 1;
        ctx->SetRef("###NewOfficer");
        ctx->Yield(2);
        ctx->ItemInputValue("###family", "\xE5\xBC\xB5");   /* 張 */
        ctx->ItemInputValue("###given", "\xE4\xB8\x80");    /* 一 */
        ctx->ItemClick("###type1");
        recorder_reset(&v.recorder);
        ctx->ItemClick("###create_officer");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_CREATE_OFFICER), 1u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_CREATE_OFFICER), (u32)OFFICER_TYPE_STRATEGIST | (1u << 8));
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.recorder.last_newcomer.family_name[0] == 0xB1 && v.recorder.last_newcomer.family_name[1] == 0x69 &&
                 v.recorder.last_newcomer.given_name[0] == 0xA4 && v.recorder.last_newcomer.ancient == SIM_NO_ANCIENT);
    };

    t = IM_REGISTER_TEST(e, "ui", "ability_chart_research_and_train_issue_one_command_each");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_abilities      = true;
        v.ui.city.selected       = 0;
        v.ui.game.trainee        = 101;
        ctx->SetRef("###Abilities");
        ctx->Yield(2);
        IM_CHECK((ctx->ItemInfo("chart_table/###train1").ItemFlags & ImGuiItemFlags_Disabled) != 0);   /* no use of row 1 */
        recorder_reset(&v.recorder);
        ctx->ItemClick("chart_table/###research1");
        ctx->Yield();
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_COMMAND_RESEARCH_ABILITY), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        recorder_reset(&v.recorder);
        ctx->ItemClick("chart_table/###train0");
        ctx->Yield();
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_COMMAND_TRAIN_OFFICER), 0u | (101u << 8));
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
    };

    t = IM_REGISTER_TEST(e, "ui", "editor_apply_buttons_issue_the_editor_commands");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_editor         = true;
        v.ui.game.editor_officer = 101;
        v.ui.city.selected       = 1;
        ctx->SetRef("###Editor");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###editor_apply_officer");
        ctx->Yield();
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_SET_OFFICER), 101u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###editor_apply_city");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_SET_CITY_GOLD), 1u);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_SET_CITY_RESOURCE), 2u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 3u);
    };

    t = IM_REGISTER_TEST(e, "ui", "battle_list_and_info_screen_read_and_command_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_battles        = true;
        v.ui.show_info           = true;
        v.ui.show_options        = true;
        recorder_reset(&v.recorder);
        ctx->Yield(2);
        ctx->SetRef("###Battles");
        IM_CHECK((ctx->ItemInfo("###battle0").ItemFlags & ImGuiItemFlags_Disabled) != 0);   /* no battle data in the clean room */
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_BATTLE_AT), 2u);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_FACTION_RANK), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        ctx->SetRef("###Options");
        ctx->ItemClick("###open_new_officer");
        ctx->Yield();
        IM_CHECK(v.ui.show_new_officer);
    };

    t = IM_REGISTER_TEST(e, "ui", "hex_map_draws_buildings_and_fire_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        recorder_reset(&v.recorder);
        ctx->Yield(2);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_BUILDING_AT), 1u);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_FIRE_VIEW), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "transport_supply_issues_one_command_supply");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.units.selected      = 1;
        v.ui.units.supply_target = ENGINE_NO_UNIT;
        v.ui.show_unit           = true;
        ctx->SetRef("###Unit");
        ctx->Yield(2);
        ctx->ComboClick("###supply_target/###s0");
        ctx->ItemInputValue("###supply_gold", 200);
        ctx->Yield();
        recorder_reset(&v.recorder);
        ctx->ItemClick("###supply");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_SUPPLY), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.recorder.last_unit_command.unit == 1 && v.recorder.last_unit_command.target_unit == 0 && v.recorder.last_unit_command.gold == 200);
    };

    t = IM_REGISTER_TEST(e, "ui", "unit_panel_and_minimap_read_and_command_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.units.selected      = 0;
        v.ui.show_unit           = true;
        v.ui.show_minimap        = true;
        recorder_reset(&v.recorder);
        ctx->Yield(2);
        ctx->SetRef("###MiniMap");
        IM_CHECK(ctx->ItemExists("minimap"));
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_UNIT_AT), 2u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "economy_panel_reads_the_forecast_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_economy        = true;
        ctx->SetRef("###Economy");
        recorder_reset(&v.recorder);
        ctx->Yield(2);
        IM_CHECK(ctx->ItemExists("economy"));
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_CITY_INCOME_FORECAST), 2u);   /* one row for each city */
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_CITY_ECONOMY), 2u);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_TECHNIQUE_POINTS), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "ledger_filter_calls_only_queries");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.city_count = 2;
        recorder_seed_events(&v.recorder, 5);
        ctx->SetRef("Event ledger");
        ctx->Yield();
        recorder_reset(&v.recorder);
        ctx->ComboClick("###ledger_city/###lc1");
        ctx->ComboClick("###ledger_resource/###lr1");                                 /* gold */
        ctx->Yield();
        IM_CHECK(v.ui.ledger_city == 1 && v.ui.ledger_resource == SIM_RESOURCE_GOLD);
        recorder_reset(&v.recorder);
        ctx->Yield(2);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_LEDGER_FILTER), 1u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_LEDGER_FILTER), 1u | ((u32)SIM_RESOURCE_GOLD << 8));
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_LEDGER_AT), 1u);             /* the rows still come from ledger_at */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        ctx->ComboClick("###ledger_city/###lc_any");
        ctx->ComboClick("###ledger_resource/###lr_any");
        recorder_reset(&v.recorder);
        ctx->Yield(2);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_LEDGER_FILTER), 0u);         /* no filter: every event */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "idle_frames_command_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        recorder_reset(&v.recorder);
        ctx->Yield(10);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        IM_CHECK_GT(recorder_count(&v.recorder, RECORDER_TURN), 0u);      /* but it does read state */
    };

    /* ---- the table views, the own-faction filter, the City buttons, the font size, the keys */

    t = IM_REGISTER_TEST(e, "ui", "officers_table_filter_and_sort_change_the_order_and_command_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        ctx->SetRef("###Officers");
        ctx->Yield(2);
        IM_CHECK_EQ(v.ui.officers_view.count, 3u);
        ctx->ItemInputValue("###filter_officers", "102");
        ctx->Yield(2);
        IM_CHECK_EQ(v.ui.officers_view.count, 1u);                                       /* the id column matches */
        IM_CHECK_EQ(v.ui.officers_rows[0], 2u);
        v.ui.officers_view.filter[0] = 0;
        ctx->Yield(2);
        IM_CHECK_EQ(v.ui.officers_view.count, 3u);
        char header[96];
        snprintf(header, sizeof(header), "%s###id", text_get(&v.text, "column.id"));
        ui_test_click_header(ctx, "officers", 0, header);                                  /* ascending */
        ctx->Yield(2);
        ui_test_click_header(ctx, "officers", 0, header);                                  /* descending */
        ctx->Yield(2);
        IM_CHECK(v.ui.officers_view.sort_column == 0 && v.ui.officers_view.sort_descending);
        IM_CHECK_EQ(v.ui.officers_rows[0], 2u);
        IM_CHECK_EQ(v.ui.officers_rows[2], 0u);
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_OFFICER_AT), 2u);            /* the rows are drawn in the sorted order */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "cities_panel_lists_the_cities_of_the_player_until_all_cities");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.city_count   = 3;
        v.recorder.foreign_city = 2;         /* city 2 belongs to force 2; the player is force 1 */
        v.ui.show_map           = false;
        ctx->SetRef("###Cities");
        ctx->Yield(2);
        IM_CHECK(!v.ui.show_all_cities);
        IM_CHECK_EQ(v.ui.cities_view.count, 2u);
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_CITY_AT), 2u);                  /* only the rows of the player are drawn */
        ctx->ItemClick("###all_cities");
        ctx->Yield(2);
        IM_CHECK(v.ui.show_all_cities);
        IM_CHECK_EQ(v.ui.cities_view.count, 3u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "city_panel_hides_the_command_buttons_of_a_city_of_another_force");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 3;
        v.recorder.foreign_city  = 2;
        v.ui.show_city           = true;
        ctx->SetRef("###City");
        ctx->Yield(2);
        IM_CHECK(ctx->ItemExists("###open_develop"));                                      /* city 0 is the player's */
        ctx->ItemClick("###all_cities");
        ctx->ComboClick("###city_select/###c2");
        ctx->Yield(2);
        IM_CHECK_EQ(v.ui.city.selected, 2u);
        IM_CHECK(!ctx->ItemExists("###open_develop"));                                     /* no button for a city of another force */
        IM_CHECK(!ctx->ItemExists("###open_patrol"));
        IM_CHECK(!ctx->ItemExists("###open_sortie"));
        ctx->ItemClick("###all_cities");                                                    /* off again: the panel returns to a city of the player */
        ctx->Yield(2);
        IM_CHECK(!v.ui.show_all_cities);
        IM_CHECK_EQ(v.ui.city.selected, 0u);
        IM_CHECK(ctx->ItemExists("###open_develop"));
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "city_panel_greys_a_button_that_the_action_points_refuse");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count     = 3;
        v.recorder.city_count        = 2;
        v.recorder.can_issue_refusal = SIM_REFUSAL_NO_ACTION_POINTS;
        v.ui.show_city               = true;
        ctx->SetRef("###City");
        ctx->Yield(2);
        ImGuiTestItemInfo develop = ctx->ItemInfo("###open_develop");
        IM_CHECK(develop.ID != 0 && (develop.ItemFlags & ImGuiItemFlags_Disabled) != 0);   /* a cost refusal greys the button */
        ImGuiTestItemInfo merge = ctx->ItemInfo("###open_merge");
        IM_CHECK(merge.ID != 0 && (merge.ItemFlags & ImGuiItemFlags_Disabled) == 0);       /* merge asks nothing before its dialog */
        recorder_reset(&v.recorder);
        ctx->ItemClick("###open_develop", 0, ImGuiTestOpFlags_NoAutoOpenFullPath);
        ctx->Yield();
        IM_CHECK(!v.ui.show_develop);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        v.recorder.can_issue_refusal = SIM_REFUSAL_NO_OFFICER;                               /* not a cost: the dialog explains it */
        ctx->Yield(2);
        develop = ctx->ItemInfo("###open_develop");
        IM_CHECK(develop.ID != 0 && (develop.ItemFlags & ImGuiItemFlags_Disabled) == 0);
    };

    t = IM_REGISTER_TEST(e, "ui", "font_size_slider_changes_the_setting_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.ui.show_settings = true;
        ctx->SetRef("###Settings");
        ctx->Yield(2);
        IM_CHECK_EQ(v.ui.font_size, HEXDOM_FONT_DEFAULT_PIXELS);
        recorder_reset(&v.recorder);
        ctx->ItemInputValue("###font_size", 12);
        ctx->Yield();
        IM_CHECK_EQ(v.ui.font_size, 12);
        ctx->ItemInputValue("###font_size", 99);                                            /* out of range: the panel clamps */
        ctx->Yield();
        IM_CHECK_EQ(v.ui.font_size, HEXDOM_FONT_MAX_PIXELS);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "credits_button_opens_each_credit_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.ui.show_settings = true;
        ctx->SetRef("###Settings");
        ctx->Yield(2);
        IM_CHECK(!v.ui.show_credits);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###open_credits");
        ctx->Yield(2);
        IM_CHECK(v.ui.show_credits);
        IM_CHECK(ctx->WindowInfo("//###Credits").Window != 0);
        for (u32 index = 0; index < ArrayCount(ui_credits); ++index)
        {
            char path[96];
            snprintf(path, sizeof(path), "//###Credits/###credit_%s", ui_credits[index].id);
            IM_CHECK(ctx->ItemExists(path));                                             /* each credit has its row */
        }
        ctx->WindowClose("//###Credits");
        ctx->Yield();
        IM_CHECK(!v.ui.show_credits);
        IM_CHECK(v.ui.show_settings);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "hex_map_keys_zoom_and_scroll_and_command_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.ui.map.zoom = 20.0f;
        ctx->WindowFocus("//Hex map");
        ctx->WindowResize("//Hex map", ImVec2(240, 200));                                  /* smaller than the canvas, so that it scrolls */
        ctx->Yield(2);
        IM_CHECK(v.ui.map.focused);
        recorder_reset(&v.recorder);
        ctx->KeyPress(ImGuiKey_PageUp);
        ctx->Yield();
        IM_CHECK_EQ(v.ui.map.zoom, 22.0f);
        ctx->KeyPress(ImGuiKey_PageDown);
        ctx->Yield();
        IM_CHECK_EQ(v.ui.map.zoom, 20.0f);
        ImGuiWindow *canvas = ctx->WindowInfo("//Hex map/map_canvas").Window;
        IM_CHECK(canvas != 0);
        f32 scroll_before = canvas->Scroll.y;
        ctx->KeyDown(ImGuiKey_S);
        ctx->Yield(5);
        ctx->KeyUp(ImGuiKey_S);
        ctx->Yield();
        IM_CHECK_GT(canvas->Scroll.y, scroll_before);
        ctx->KeyDown(ImGuiKey_D);
        ctx->Yield(5);
        ctx->KeyUp(ImGuiKey_D);
        ctx->Yield();
        IM_CHECK_GT(canvas->Scroll.x, 0.0f);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* M11: the keys of the scene. W moves the view up the screen and S back,
       Q and Left turn the view one step clockwise and E and Right one step
       counter-clockwise (M12), Up tilts toward top down and Down back; the
       keys command nothing. */
    t = IM_REGISTER_TEST(e, "ui", "scene_keys_move_turn_and_tilt_and_command_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.ui.scene.headless = true;
        ctx->Yield(2);
        ImGui::FocusWindow(0);   /* no panel takes the keys */
        ctx->Yield();
        recorder_reset(&v.recorder);
        camera start = v.ui.scene.cam;
        v3 ahead, side;
        camera_ground_axes(&start, &ahead, &side);
        ctx->KeyDown(ImGuiKey_W); ctx->Yield(4); ctx->KeyUp(ImGuiKey_W); ctx->Yield();
        v3 moved = v3_sub(v.ui.scene.cam.target, start.target);
        IM_CHECK_GT(v3_dot(moved, ahead), 0.0f);                          /* W: up the screen */
        camera after_w = v.ui.scene.cam;
        ctx->KeyDown(ImGuiKey_S); ctx->Yield(4); ctx->KeyUp(ImGuiKey_S); ctx->Yield();
        IM_CHECK_LT(v3_dot(v3_sub(v.ui.scene.cam.target, after_w.target), ahead), 0.0f);   /* S: back */
        f32 yaw = v.ui.scene.cam.yaw, pitch = v.ui.scene.cam.pitch;
        /* The view turns smoothly while the key is down, and stops when it goes up. */
        ctx->KeyDown(ImGuiKey_Q); ctx->Yield(3); ctx->KeyUp(ImGuiKey_Q); ctx->Yield();
        f32 turned = ui_test_turn_between(yaw, v.ui.scene.cam.yaw);
        IM_CHECK_LT(turned, 0.0f);                                          /* Q: clockwise on the screen (ui/main-map.md) */
        IM_CHECK_GT(turned, -0.5f);                                         /* a small turn in a few frames, not a step */
        IM_CHECK_EQ(v.ui.scene.cam.pitch, pitch);
        yaw = v.ui.scene.cam.yaw;
        ctx->Yield(10);
        IM_CHECK_EQ(v.ui.scene.cam.yaw, yaw);                               /* no turn after the key goes up */
        ctx->KeyDown(ImGuiKey_E); ctx->Yield(3); ctx->KeyUp(ImGuiKey_E); ctx->Yield();
        IM_CHECK_GT(ui_test_turn_between(yaw, v.ui.scene.cam.yaw), 0.0f);   /* E: counter-clockwise */
        yaw = v.ui.scene.cam.yaw;
        ctx->KeyDown(ImGuiKey_LeftArrow); ctx->Yield(3); ctx->KeyUp(ImGuiKey_LeftArrow); ctx->Yield();
        IM_CHECK_LT(ui_test_turn_between(yaw, v.ui.scene.cam.yaw), 0.0f);   /* Left: as Q */
        ctx->KeyDown(ImGuiKey_RightArrow); ctx->Yield(3); ctx->KeyUp(ImGuiKey_RightArrow); ctx->Yield();
        IM_CHECK(fabsf(ui_test_turn_between(yaw, v.ui.scene.cam.yaw)) < 1e-4f);   /* Right: as E, back */
        yaw = v.ui.scene.cam.yaw;
        ctx->KeyDown(ImGuiKey_DownArrow); ctx->Yield(3); ctx->KeyUp(ImGuiKey_DownArrow); ctx->Yield();
        IM_CHECK_LT(v.ui.scene.cam.pitch, pitch);                         /* Down: toward the horizon */
        IM_CHECK_EQ(v.ui.scene.cam.yaw, yaw);                             /* a tilt keeps the heading */
        pitch = v.ui.scene.cam.pitch;
        ctx->KeyDown(ImGuiKey_UpArrow); ctx->Yield(3); ctx->KeyUp(ImGuiKey_UpArrow); ctx->Yield();
        IM_CHECK_GT(v.ui.scene.cam.pitch, pitch);                         /* Up: toward top down */
        IM_CHECK_LE(v.ui.scene.cam.pitch, CAMERA_PITCH_MAX);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* M11: a row of the Cities panel selects the city and the camera frames
       its centre; the names of the cities draw from queries only. */
    t = IM_REGISTER_TEST(e, "ui", "cities_row_frames_the_city_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.city_count = 2;
        v.ui.scene.headless   = true;
        v.ui.show_cities      = true;
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->SetRef("###Cities");
        ctx->ItemClick("**/1##city_row");
        ctx->Yield(2);
        IM_CHECK_EQ(v.ui.city.selected, 1);
        v3 expected = hex_world_center(hex_make(3, 1), v.ui.scene.cam.target.y);   /* the scripted centre of city 1 */
        IM_CHECK(fabsf(v.ui.scene.cam.target.x - expected.x) < 1e-3f && fabsf(v.ui.scene.cam.target.z - expected.z) < 1e-3f);
        IM_CHECK_EQ(v.ui.scene.cam.distance, UI_FRAME_DISTANCE);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_CITY_SLOTS), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* M11: the Units panel lists the units; a row frames the unit. */
    t = IM_REGISTER_TEST(e, "ui", "units_panel_frames_a_unit_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.ui.scene.headless = true;
        v.ui.show_units     = true;
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->SetRef("###Units");
        ImGuiTestItemInfo row = ctx->ItemInfo("**/###unit1", ImGuiTestOpFlags_NoError);
        IM_CHECK(row.ID != 0);
        ctx->ItemClick(row.ID);
        ctx->Yield(2);
        IM_CHECK_EQ(v.ui.units.selected, 1u);
        v3 expected = hex_world_center(hex_make(1, 3), v.ui.scene.cam.target.y);   /* the scripted hex of unit 1 */
        IM_CHECK(fabsf(v.ui.scene.cam.target.x - expected.x) < 1e-3f && fabsf(v.ui.scene.cam.target.z - expected.z) < 1e-3f);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_UNIT_AT), 2u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* M12: a click on the round mini map moves the camera to that point.
       The centre of the disc is the centre of the map; at yaw 0 the right
       of the disc is east. The distance and the heading stay. */
    t = IM_REGISTER_TEST(e, "ui", "minimap_click_moves_the_camera_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.ui.show_minimap = true;
        ctx->Yield(2);
        ctx->SetRef("###MiniMap");
        ImGuiTestItemInfo disc = ctx->ItemInfo("minimap");
        IM_CHECK(disc.ID != 0);
        v.ui.scene.cam.yaw    = 0.0f;
        v.ui.scene.cam.target = hex_world_center(hex_make(0, 0), 0.0f);
        f32 distance = v.ui.scene.cam.distance;
        recorder_reset(&v.recorder);
        ctx->ItemClick(disc.ID);
        ctx->Yield();
        hex_offset first = { 0, 0 }, last = { 7, 5 };   /* the 8 x 6 map of the recorder */
        v3 low = hex_world_center(hex_from_offset(first), 0.0f), high = hex_world_center(hex_from_offset(last), 0.0f);
        f32 middle_x = (low.x + high.x) * 0.5f, middle_z = (low.z + high.z) * 0.5f;
        v3 centred = v.ui.scene.cam.target;
        IM_CHECK(fabsf(centred.x - middle_x) < 1.5f && fabsf(centred.z - middle_z) < 1.5f);
        IM_CHECK_EQ(v.ui.scene.cam.distance, distance);
        IM_CHECK_EQ(v.ui.scene.cam.yaw, 0.0f);
        ImVec2 centre = disc.RectFull.GetCenter();
        f32 radius = disc.RectFull.GetWidth() * 0.5f - UI_MINIMAP_RING_WIDTH;
        ctx->WindowFocus("//###MiniMap");
        ctx->MouseMoveToPos(ImVec2(centre.x + radius * 0.7f, centre.y));
        ctx->MouseClick(0);
        ctx->Yield();
        IM_CHECK_GT(v.ui.scene.cam.target.x, centred.x + 1.0f);   /* east */
        IM_CHECK_LT(fabsf(v.ui.scene.cam.target.z - centred.z), 1.5f);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_TERRAIN_KIND_AT), 1u);   /* the road kinds */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* Q-217: the Unit panel of a transport shows its cargo in two rows (槍 戟
       弩 軍馬; 木獸 井闌 樓船) and asks for the weapon names; it commands nothing. */
    t = IM_REGISTER_TEST(e, "ui", "unit_panel_shows_the_cargo_of_a_transport_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.ui.units.selected = 1;   /* the transport of the recorder */
        v.ui.show_unit      = true;
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_WEAPON_KIND_AT), 7u);   /* the names of the seven cells */
        v.ui.units.selected = 0;   /* a battle unit: no cargo */
        ctx->Yield();
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_WEAPON_KIND_AT), 0u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* The game mode: the HUD at the top left shows the date, the points and
       the list; the debug menu bar hides until F12. The HUD commands nothing. */
    t = IM_REGISTER_TEST(e, "ui", "hud_shows_the_status_and_the_list_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.hud_mode            = true;
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK(ctx->WindowInfo("//###HUD").Window != 0);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_ACTIONS_LEFT), 1u);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_TECHNIQUE_POINTS), 1u);
        ImGuiWindow *bar = ctx->WindowInfo("//##MainMenuBar", ImGuiTestOpFlags_NoError).Window;
        IM_CHECK(bar == 0 || !bar->Active);                                   /* no debug menu bar */
        ctx->SetRef("//###HUD");
        IM_CHECK(ctx->ItemInfo("**/###hud_row0_0", ImGuiTestOpFlags_NoError).ID != 0);   /* city 0: its officers can act */
        IM_CHECK(ctx->ItemInfo("**/###hud_row0_1", ImGuiTestOpFlags_NoError).ID != 0);   /* city 1: grey */
        ctx->KeyPress(ImGuiKey_F12);
        ctx->Yield(2);
        IM_CHECK(v.ui.show_debug_panels);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* The game mode: a click on a row of the list frames the city; a right
       click opens its dialog; a right click outside the dialog closes it. */
    t = IM_REGISTER_TEST(e, "ui", "hud_row_frames_and_right_click_opens_and_closes_the_city_dialog");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.hud_mode            = true;
        v.ui.scene.headless      = true;   /* the camera follows the list */
        v.ui.show_city           = false;
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->SetRef("//###HUD");
        ImGuiTestItemInfo row = ctx->ItemInfo("**/###hud_row0_1");
        IM_CHECK(row.ID != 0);
        ctx->ItemClick(row.ID);
        ctx->Yield(2);
        IM_CHECK_EQ(v.ui.city.selected, 1);
        v3 expected = hex_world_center(hex_make(3, 1), v.ui.scene.cam.target.y);   /* the scripted centre of city 1 */
        IM_CHECK(fabsf(v.ui.scene.cam.target.x - expected.x) < 1e-3f && fabsf(v.ui.scene.cam.target.z - expected.z) < 1e-3f);
        IM_CHECK(!v.ui.show_city);                                            /* a left click only frames */
        ctx->ItemClick(row.ID, ImGuiMouseButton_Right);
        ctx->Yield(2);
        IM_CHECK(v.ui.show_city && v.ui.context.kind == UI_CONTEXT_CITY);
        ImVec2 display = ImGui::GetIO().DisplaySize;
        ctx->MouseMoveToPos(ImVec2(display.x * 0.8f, display.y * 0.5f));   /* the empty map */
        ctx->MouseClick(ImGuiMouseButton_Right);
        ctx->Yield(2);
        IM_CHECK(!v.ui.show_city && v.ui.context.kind == UI_CONTEXT_NONE);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* The game mode: 進行 of the map menu ends the turn with one command and closes the menu. */
    t = IM_REGISTER_TEST(e, "ui", "map_menu_proceed_issues_one_end_turn");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.ui.hud_mode = true;
        ctx->Yield();
        ui_context_open(&v.ui, &v.api, UI_CONTEXT_MAP_MENU, 400.0f, 300.0f);
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->SetRef("//###MapMenu");
        ui_test_proceed(ctx);
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_END_TURN), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK_EQ(v.ui.context.kind, (u8)UI_CONTEXT_NONE);
    };

    /* The game mode: a right click on a city of the scene opens the City
       dialog; a right click on the empty map opens the map menu. */
    t = IM_REGISTER_TEST(e, "ui", "scene_right_click_opens_the_city_dialog_or_the_map_menu");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.city_count = 2;
        v.ui.hud_mode         = true;
        v.ui.scene.headless   = true;
        v.ui.show_city        = false;
        v.ui.show_map         = false;
        v.ui.show_officers    = v.ui.show_cities = false;   /* nothing covers the map */
        ctx->Yield(2);
        hex_offset city_cell = { RECORDER_CITY_COL, RECORDER_CITY_ROW };
        ui_frame_hex(&v.ui, &v.api, hex_from_offset(city_cell));   /* the city hex is at the centre of the screen */
        v.ui.scene.cam.target.y = 0.0f;                            /* the pick meets the ground plane */
        v.ui.scene.framed_city = v.ui.city.selected;
        ctx->Yield(3);
        recorder_reset(&v.recorder);
        ImVec2 display = ImGui::GetIO().DisplaySize;
        ImVec2 centre(display.x * 0.5f, display.y * 0.5f);
        ctx->MouseMoveToPos(centre);
        ctx->MouseClick(ImGuiMouseButton_Right);
        ctx->Yield(2);
        IM_CHECK(v.ui.context.kind == UI_CONTEXT_CITY && v.ui.show_city && v.ui.city.selected == 0);
        ctx->MouseMoveToPos(ImVec2(display.x * 0.1f, display.y * 0.9f));
        ctx->MouseClick(ImGuiMouseButton_Right);                             /* outside: closes, and opens nothing */
        ctx->Yield(2);
        IM_CHECK(v.ui.context.kind == UI_CONTEXT_NONE && !v.ui.show_city);
        ctx->MouseMoveToPos(ImVec2(centre.x + 60.0f, centre.y));             /* a hex of the map without a city */
        ctx->MouseClick(ImGuiMouseButton_Right);                             /* the empty map: the map menu */
        ctx->Yield(2);
        IM_CHECK_EQ(v.ui.context.kind, (u8)UI_CONTEXT_MAP_MENU);
        IM_CHECK(ctx->WindowInfo("//###MapMenu").Window != 0);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* M12: each unit plate asks for the commander (the portrait and the
       name) and for the corps of the home city (委); the marks and the
       supply colours follow the unit. The plates command nothing. */
    t = IM_REGISTER_TEST(e, "ui", "unit_plates_read_the_commander_and_the_corps_and_command_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.ui.scene.headless = true;
        ui_frame_unit(&v.ui, 0);   /* the camera looks at unit 0, so its plate is on the screen */
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_CITY_CORPS), 1u);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_OFFICER_BY_ID), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        engine_unit unit = {};
        IM_CHECK_EQ(ui_unit_mark_of(&unit, 0), (u32)UI_UNIT_MARK_READY);       /* 未 */
        IM_CHECK_EQ(ui_unit_mark_of(&unit, 2), (u32)UI_UNIT_MARK_DELEGATED);   /* 委 */
        unit.acted = true;
        IM_CHECK_EQ(ui_unit_mark_of(&unit, 2), (u32)UI_UNIT_MARK_DONE);        /* 完 */
        IM_CHECK(ui_grain_colour(ENGINE_GRAIN_TURNS_UNKNOWN) != ui_grain_colour(20));
        IM_CHECK(ui_grain_colour(1) != ui_grain_colour(5) && ui_grain_colour(5) != ui_grain_colour(20));
        IM_CHECK_EQ(ui_grain_colour(ENGINE_GRAIN_TURNS_ENDLESS), ui_grain_colour(20));
    };

    /* The flag of each unit (the player, 2026-10-25): the flag asks for the
       ruler of the faction of the unit, for the family name of that ruler
       and for the colour of the faction. It commands nothing. A light flag
       gets a dark character, a dark flag a white one. */
    t = IM_REGISTER_TEST(e, "ui", "unit_flags_read_the_ruler_and_the_colour_of_the_faction_and_command_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.ui.scene.headless    = true;
        v.recorder.officer_count = 2;   /* officer 100 rules force 1 */
        ui_frame_unit(&v.ui, 0);   /* the camera looks at unit 0 of force 1, so its plate and its flag are on the screen */
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK_GE(recorder_count_arg(&v.recorder, RECORDER_FORCE_RULER, 1), 1u);        /* the ruler of force 1 */
        IM_CHECK_GE(recorder_count_arg(&v.recorder, RECORDER_OFFICER_BY_ID, 100), 1u);    /* officer 100, for the family name */
        IM_CHECK_GE(recorder_count_arg(&v.recorder, RECORDER_FACTION_COLOUR, 1), 1u);     /* the colour of force 1 */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        char family[UI_FLAG_NAME_SIZE];
        ui_force_family_name(&v.api, 1, family, sizeof(family));
        IM_CHECK(family[0] != 0);                                                           /* the Big5 bytes B1 40 of officer 100 */
        ui_force_family_name(&v.api, 3, family, sizeof(family));
        IM_CHECK(family[0] == 0);                                                           /* force 3 has no ruler and no leader */
        ImU32 ink = 0, outline = 0;
        ui_flag_ink(IM_COL32(240, 230, 160, 255), &ink, &outline);
        IM_CHECK_EQ(ink, IM_COL32(25, 20, 20, 255));
        IM_CHECK_EQ(outline, 0u);                                                           /* no outline round a dark character */
        ui_flag_ink(IM_COL32(36, 36, 40, 255), &ink, &outline);
        IM_CHECK_EQ(ink, IM_COL32(255, 255, 255, 255));
        IM_CHECK_NE(outline, 0u);                                                           /* a dark outline round a white one */
    };

    /* M11: the status bar shows the hex under the mouse (ui/main-map.md). */
    t = IM_REGISTER_TEST(e, "ui", "status_bar_reads_the_hex_under_the_mouse_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        hex_offset cell = { 3, 2 };
        v.ui.scene.hover     = hex_from_offset(cell);
        v.ui.scene.has_hover = true;   /* without a render target the scene keeps the hover */
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK(ctx->WindowInfo("//###StatusBar").Window != 0);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_HEX_AT), 3u | (2u << 16));
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_TERRAIN_KIND_AT), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* M11: a picked hex opens the Hex info box, which asks hex_at for it. */
    t = IM_REGISTER_TEST(e, "ui", "hex_info_box_reads_the_picked_hex_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        hex_offset city_cell = { RECORDER_CITY_COL, RECORDER_CITY_ROW };
        v.ui.scene.pick     = hex_from_offset(city_cell);
        v.ui.scene.has_pick = true;
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK(ctx->WindowInfo("//###HexInfo").Window != 0);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_HEX_AT), (u32)RECORDER_CITY_COL | ((u32)RECORDER_CITY_ROW << 16));
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_TERRAIN_KIND_AT), 1u);   /* the name of the terrain */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* M11: the City panel counts the built facilities by kind and level (都市情報). */
    t = IM_REGISTER_TEST(e, "ui", "city_panel_counts_facilities_by_kind_and_level");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.city_count = 2;
        v.ui.show_city = true;
        ctx->Yield(2);
        ui_facility_count rows[UI_FACILITY_COUNT_CAPACITY];
        u32 count = ui_count_facilities(&v.api, 0, rows, ArrayCount(rows));
        IM_CHECK_EQ(count, 2u);                                           /* two markets of level 1, and a farm under construction */
        IM_CHECK(rows[0].facility == 0 && rows[0].level == 1 && rows[0].count == 2 && rows[0].building == 0);
        IM_CHECK(rows[1].facility == 1 && rows[1].level == 1 && rows[1].count == 0 && rows[1].building == 1);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* The counts show the builds in progress next to the built facilities:
       "5 (+2)". The 開發 grid counts each kind the same way. */
    t = IM_REGISTER_TEST(e, "ui", "facility_counts_show_the_builds_in_progress_and_command_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.city_count = 2;
        v.ui.show_city = true;
        ctx->Yield(2);
        char number[32];
        ui_format_facility_count(number, sizeof(number), 5, 2);
        IM_CHECK_STR_EQ(number, "5 (+2)");
        ui_format_facility_count(number, sizeof(number), 5, 0);
        IM_CHECK_STR_EQ(number, "5");
        u32 built[256], building[256];
        ui_count_built_facilities(&v.api, 0, built, building, ArrayCount(built));
        IM_CHECK(built[0] == 2 && building[0] == 0);                      /* the markets */
        IM_CHECK(built[1] == 0 && building[1] == 1);                      /* the farm under construction */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* The axis of the passage of a gate: the axis with a passable hex on
       both sides wins; a road on both sides wins between two open axes;
       the axis with fewer passable hexes beside it wins between two open
       axes without a road (a gap in the cliffs). */
    t = IM_REGISTER_TEST(e, "ui", "gate_axis_follows_the_open_axis_the_road_and_the_gap");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        (void)ctx;
        b32 passable[HEX_DIRECTION_COUNT], road[HEX_DIRECTION_COUNT] = {};
        for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d) passable[d] = d == 1 || d == 4;   /* cliffs except the axis 1 */
        IM_CHECK_EQ(ui_gate_axis(passable, road), 1u);
        for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d) passable[d] = true;                /* open ground: the road decides */
        road[2] = road[5] = true;
        IM_CHECK_EQ(ui_gate_axis(passable, road), 2u);
        road[2] = road[5] = false;
        passable[1] = passable[4] = false;                                               /* two open axes, cliffs on axis 1: the gap is axis 0 or 2 */
        passable[2] = false;                                                             /* axis 2 loses one side */
        IM_CHECK_EQ(ui_gate_axis(passable, road), 0u);
    };

    /* The scene draws a gate on each pass and a pier on each port, in the
       colour of the owner. The recorder has one pass and one port. The
       scene asks base_at and commands nothing. */
    t = IM_REGISTER_TEST(e, "ui", "scene_draws_a_gate_on_each_pass_and_a_pier_on_each_port");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        memory_arena frame_arena = test_arena_create(Megabytes(2));
        v.ui.scene.headless = true;
        v.frame.frame_arena = &frame_arena;
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK(v.frame.render && v.frame.render->scene_enabled);
        f32 scale = 0.0f;
        IM_CHECK_EQ(ui_test_count_meshes(v.frame.render, RENDER_MESH_GATE, &scale), 1u);
        IM_CHECK_EQ(scale, 1.0f);
        IM_CHECK_EQ(ui_test_count_meshes(v.frame.render, RENDER_MESH_PORT, 0), 1u);
        /* The pass of the recorder stands at column 4, next to the column 3
           of unknown terrain that no unit can enter. The only axis with a
           passable hex on both sides runs along the column, so the passage
           of the gate points along the world Z axis. */
        f32 yaw = ui_test_mesh_yaw(v.frame.render, RENDER_MESH_GATE);
        IM_CHECK_LT(fabsf(sinf(yaw)), 1e-3f);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_BASE_AT), 2u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        v.frame.frame_arena = 0;
        test_arena_destroy(&frame_arena);
    };

    /* The scene names the art of each base kind (docs/ASSETS.md, "Base
       art"). Each base keeps its placeholder mesh for a pack without the
       picture. The cities of the recorder have no specialty, so each one
       names the art of a city. A big city names its own art. The models of
       the slots name no art. The scene commands nothing. */
    t = IM_REGISTER_TEST(e, "ui", "scene_names_the_art_of_each_base_kind");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        memory_arena frame_arena = test_arena_create(Megabytes(2));
        v.recorder.city_count = 2;
        v.ui.scene.headless   = true;
        v.frame.frame_arena   = &frame_arena;
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->Yield();
        render_commands *rc = v.frame.render;
        IM_CHECK(rc && rc->scene_enabled);
        IM_CHECK_EQ(ui_test_count_meshes(rc, RENDER_MESH_SETTLEMENT, 0), 2u);
        IM_CHECK_EQ(ui_test_mesh_art(rc, RENDER_MESH_SETTLEMENT, RENDER_SPRITE_BASE_CITY), 2u);
        IM_CHECK_EQ(ui_test_mesh_art(rc, RENDER_MESH_GATE, RENDER_SPRITE_BASE_GATE), 1u);
        IM_CHECK_EQ(ui_test_mesh_art(rc, RENDER_MESH_PORT, RENDER_SPRITE_BASE_PORT), 1u);
        IM_CHECK_EQ(ui_test_mesh_art(rc, RENDER_MESH_STALL, RENDER_NO_ART), ui_test_count_meshes(rc, RENDER_MESH_STALL, 0));
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);

        engine_city_slots slots = {};
        IM_CHECK_EQ(ui_city_art(&slots), (u32)RENDER_SPRITE_BASE_CITY);
        slots.specialty = CITY_SPECIALTY_BIG_CITY;
        IM_CHECK_EQ(ui_city_art(&slots), (u32)RENDER_SPRITE_BASE_BIG_CITY);
        slots.specialty = CITY_SPECIALTY_NAVY;
        IM_CHECK_EQ(ui_city_art(&slots), (u32)RENDER_SPRITE_BASE_CITY);
        IM_CHECK_EQ(ui_base_art(BASE_PASS), (u32)RENDER_SPRITE_BASE_GATE);
        IM_CHECK_EQ(ui_base_art(BASE_PORT), (u32)RENDER_SPRITE_BASE_PORT);
        v.frame.frame_arena = 0;
        test_arena_destroy(&frame_arena);
    };

    /* The scene draws a model on each slot with a facility: the shape of
       the kind for a built one, and a scaffold with a smaller shape inside
       for a build in progress. The scene asks city_slots and facility_at
       and commands nothing. */
    t = IM_REGISTER_TEST(e, "ui", "scene_draws_a_model_on_each_facility_and_a_scaffold_on_a_build_in_progress");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        memory_arena frame_arena = test_arena_create(Megabytes(2));
        v.recorder.city_count = 2;
        v.ui.scene.headless   = true;
        v.frame.frame_arena   = &frame_arena;
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK(v.frame.render && v.frame.render->scene_enabled);
        IM_CHECK_EQ(ui_test_count_meshes(v.frame.render, RENDER_MESH_SETTLEMENT, 0), 2u);   /* one settlement for each city */
        f32 built_scale = 0.0f, building_scale = 0.0f;
        IM_CHECK_EQ(ui_test_count_meshes(v.frame.render, RENDER_MESH_STALL, &built_scale), 4u);        /* two markets in each city */
        v.recorder.building_complete = true;                                                          /* U-001: no scaffold for the field camp */
        ctx->Yield();
        IM_CHECK_EQ(ui_test_count_meshes(v.frame.render, RENDER_MESH_SCAFFOLD, 0), 2u);                /* the farm under construction of each city */
        IM_CHECK_EQ(ui_test_count_meshes(v.frame.render, RENDER_MESH_FIELD, &building_scale), 2u);     /* the farm itself, smaller */
        IM_CHECK_LT(building_scale, built_scale);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_CITY_SLOTS), 2u);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_FACILITY_AT), 2u * RECORDER_SLOTS);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        v.frame.frame_arena = 0;
        v.frame.render      = 0;
        test_arena_destroy(&frame_arena);
    };

    /* U-001 (play report 2026-10-24): the scene draws each field building
       of building_at with the placeholder model of its kind, in the colour
       of its owner. A build in progress is a scaffold with the smaller,
       dimmed model inside, as on a slot. */
    t = IM_REGISTER_TEST(e, "ui", "scene_draws_each_field_building_in_the_colour_of_its_owner");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        memory_arena frame_arena = test_arena_create(Megabytes(2));
        v.recorder.city_count = 2;
        v.ui.scene.headless   = true;
        v.frame.frame_arena   = &frame_arena;
        v.recorder.building_complete = true;
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->Yield();
        f32 built_scale = 0.0f, building_scale = 0.0f;
        IM_CHECK_EQ(ui_test_count_meshes(v.frame.render, RENDER_MESH_CAMP, &built_scale), 1u);        /* the camp at (2, 3) */
        IM_CHECK_EQ(ui_test_mesh_tint(v.frame.render, RENDER_MESH_CAMP), ui_faction_rgba(&v.api, 1));         /* in the colour of force 1 */
        IM_CHECK_EQ(ui_test_count_meshes(v.frame.render, RENDER_MESH_SCAFFOLD, 0), 2u);                /* the two farms only */
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_BUILDING_AT), 1u);
        v.recorder.building_complete = false;                                                          /* a build in progress */
        ctx->Yield();
        IM_CHECK_EQ(ui_test_count_meshes(v.frame.render, RENDER_MESH_CAMP, &building_scale), 1u);
        IM_CHECK_EQ(ui_test_count_meshes(v.frame.render, RENDER_MESH_SCAFFOLD, 0), 3u);                /* the farms and the camp */
        IM_CHECK_LT(building_scale, built_scale);
        IM_CHECK_NE(ui_test_mesh_tint(v.frame.render, RENDER_MESH_CAMP), ui_faction_rgba(&v.api, 1));         /* dimmed */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        v.frame.frame_arena = 0;
        v.frame.render      = 0;
        test_arena_destroy(&frame_arena);
    };

    /* U-001: the hover panel of a field building gives its name, its owner
       and 耐久 current / maximum, as G3-020 does for a facility. */
    t = IM_REGISTER_TEST(e, "ui", "hover_panel_shows_the_field_building_under_the_mouse");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.hud_mode            = true;
        v.ui.scene.headless      = true;
        v.ui.show_city = v.ui.show_map = v.ui.show_officers = v.ui.show_cities = false;
        ctx->Yield(2);
        ui_frame_hex(&v.ui, &v.api, hex_make(2, 3));
        v.ui.scene.cam.target.y = 0.0f;
        ctx->Yield(3);
        recorder_reset(&v.recorder);
        ImVec2 display = ImGui::GetIO().DisplaySize;
        ctx->MouseMoveToPos(ImVec2(display.x * 0.5f, display.y * 0.5f));
        ctx->Yield(2);
        IM_CHECK(ctx->WindowInfo("//###HoverBuilding", ImGuiTestOpFlags_NoError).Window != 0);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_BUILDING_AT), 1u);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_BUILDING_KIND_AT), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* M10: a click on a city hex selects the city; the panel frames its
       slots from city_slots and facility_at, and commands nothing. */
    t = IM_REGISTER_TEST(e, "ui", "map_click_on_a_city_frames_its_slots_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.city_count = 2;
        v.ui.show_map = true;
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        hex_offset city_cell = { RECORDER_CITY_COL, RECORDER_CITY_ROW };
        ui_test_click_cell(ctx, v, hex_from_offset(city_cell));
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_HEX_AT), (u32)RECORDER_CITY_COL | ((u32)RECORDER_CITY_ROW << 16));
        IM_CHECK(v.ui.map.has_city && v.ui.map.city == 0);
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_CITY_SLOTS), 1u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_CITY_SLOTS), 0u);   /* the slots of city 0 */
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_FACILITY_AT), (u32)RECORDER_SLOTS);   /* one query for each slot */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        ctx->SetRef("Hex map");
        ctx->ItemClick("Clear city");
        ctx->Yield();
        IM_CHECK(!v.ui.map.has_city);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "ledger_names_the_subject_faction_and_detail_through_queries_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 2;
        v.recorder.city_count    = 2;
        v.recorder.has_names     = true;
        recorder_seed_events(&v.recorder, 3);
        v.recorder.events[0].subject = 2;                       /* the scripted city 1 */
        v.recorder.events[0].faction = 1;
        v.recorder.events[1].subject = 1000;                    /* the scripted officer 100 */
        v.recorder.events[1].faction = ENGINE_NO_FACTION;
        v.recorder.events[2].kind    = SIM_EVENT_OFFICER_CREATED;
        v.recorder.events[2].detail  = 1;                       /* detail = the city */
        v.recorder.events[2].faction = ENGINE_NO_FACTION;
        v.ui.show_sim = v.ui.show_officers = v.ui.show_cities = v.ui.show_map = false;
        ctx->SetRef("Event ledger");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_SUBJECT_OF), 2u);                /* the two events with a subject */
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_CITY_NAME_AT), 2u);              /* the city subject and the city detail */
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_FORCE_RULER), 1u);               /* the one event with a faction */
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_OFFICER_BY_ID), 2u);             /* the ruler and the officer subject */
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_LEDGER_AT), 3u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        ctx->ItemClick("###raw");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_SUBJECT_OF), 0u);                /* raw numbers need no names */
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_CITY_NAME_AT), 0u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* ---- M17: the feel of the first hour ----------------------------------- */

    /* title-and-load.md: after a click, the other five buttons disappear.
       The rider runs approximately 1 s, then the screen opens. A second
       click during the rider has no visible effect. */
    t = IM_REGISTER_TEST(e, "ui", "title_click_hides_the_other_buttons_and_opens_after_the_rider");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_title          = true;
        v.ui.feel.original_timing = true;
        ctx->SetRef("###Title");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###title_new");
        ctx->Yield();
        IM_CHECK(!v.ui.show_scenarios && v.ui.feel.title_choice == UI_TITLE_NEW);
        IM_CHECK(ctx->ItemExists("###title_new"));
        IM_CHECK(!ctx->ItemExists("###title_load") && !ctx->ItemExists("###title_battle") && !ctx->ItemExists("###title_quit"));
        ctx->ItemClick("###title_new");                        /* the second click */
        IM_CHECK(!v.ui.show_scenarios && v.ui.feel.title_choice == UI_TITLE_NEW);
        ctx->Yield((int)(UI_TITLE_SELECT_SECONDS * 60.0f) + 10);
        IM_CHECK(v.ui.show_scenarios && v.ui.feel.title_choice == UI_TITLE_NONE);
        IM_CHECK(ctx->ItemExists("###title_load"));            /* the title is whole again under the new screen */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* title-and-load.md: the load shows a dark landscape with no controls
       for approximately 4 s. Then the main map opens. */
    t = IM_REGISTER_TEST(e, "ui", "load_transition_hides_the_map_and_takes_no_input");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.feel.original_timing = true;
        ui_enter_game(&v.ui);
        ui_feel_begin_transition(&v.ui);
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ImGuiWindow *dark = ctx->WindowInfo("//###LoadTransition", ImGuiTestOpFlags_NoError).Window;
        ImGuiWindow *hud  = ctx->WindowInfo("//###HUD", ImGuiTestOpFlags_NoError).Window;
        IM_CHECK(dark != 0 && dark->Active);
        IM_CHECK(hud == 0 || !hud->Active);
        ui_context_open(&v.ui, &v.api, UI_CONTEXT_MAP_MENU, 400.0f, 300.0f);   /* a right click on the map */
        ctx->Yield(2);
        IM_CHECK_EQ(v.ui.context.kind, (u8)UI_CONTEXT_NONE);
        ctx->Yield((int)(UI_LOAD_TRANSITION_SECONDS * 60.0f) + 10);
        dark = ctx->WindowInfo("//###LoadTransition", ImGuiTestOpFlags_NoError).Window;
        hud  = ctx->WindowInfo("//###HUD", ImGuiTestOpFlags_NoError).Window;
        IM_CHECK(dark == 0 || !dark->Active);
        IM_CHECK(hud != 0 && hud->Active);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* main-map.md, "Message window": at the start of a turn the reports
       show one at a time. The period heading comes first, then the reports
       of the player in ledger order. M18: only a kind that the pack marks as
       a report (the recorder: detail 1) opens a message. A left click
       advances. The list panel hides while the window shows. */
    t = IM_REGISTER_TEST(e, "ui", "turn_messages_show_the_heading_then_the_reports_and_a_click_advances");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        sim_event income = {};
        income.kind = SIM_EVENT_GOLD_INCOME; income.faction = 1; income.delta = 120; income.subject = 1;
        income.detail = 1;                                     /* the recorder reports it */
        sim_event rival = income;
        rival.faction = 2;                                     /* another faction: no message */
        sim_event start = {};
        start.kind = SIM_EVENT_FACTION_TURN_STARTED; start.faction = 1;   /* bookkeeping: no message */
        v.recorder.events[0] = income;
        v.recorder.events[1] = rival;
        v.recorder.events[2] = start;
        v.recorder.event_count = 3;
        v.ui.feel.turn_messages = true;
        v.ui.round.show         = true;   /* M20: the message window opens only with the show on */
        ui_enter_game(&v.ui);
        ctx->Yield(3);
        /* G2-007: the heading, the report, then the closing line of the adviser. The recorder has no threat. */
        IM_CHECK_EQ(v.ui.feel.message_count, 3u);
        IM_CHECK_EQ(v.ui.feel.messages[0], UI_MESSAGE_HEADING);
        IM_CHECK_EQ(v.ui.feel.messages[1], 0u);
        IM_CHECK_EQ(v.ui.feel.messages[2], UI_MESSAGE_NEXT_ORDER);
        IM_CHECK(ui_feel_messages_open(&v.ui));
        IM_CHECK(!ctx->ItemExists("//###HUD/###hud_tabs/###hud_bases"));   /* the list panel hides */
        recorder_reset(&v.recorder);
        ctx->SetRef("//###Messages");
        ctx->ItemClick("###next");
        ctx->Yield();
        IM_CHECK_EQ(v.ui.feel.message_shown, 1u);
        ctx->ItemClick("###next");
        ctx->Yield();
        IM_CHECK_EQ(v.ui.feel.message_shown, 2u);
        char line[256];
        ui_feel_message_text(&v.ui, &v.api, UI_MESSAGE_NEXT_ORDER, line, sizeof(line));
        IM_CHECK(strstr(line, "\xE5\xA4\xA7\xE4\xBA\xBA") != 0);            /* zh-TW: 大人, the line speaks to the ruler */
        ctx->ItemClick("###next");
        ctx->Yield(2);
        IM_CHECK(!ui_feel_messages_open(&v.ui));
        IM_CHECK(ctx->ItemExists("//###HUD/###hud_tabs/###hud_bases"));    /* the list returns */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* screen-flow.md: 進行 ends the turn, the other factions act, then the
       messages of the new turn show. The window opens again with the
       heading of the new turn. */
    t = IM_REGISTER_TEST(e, "ui", "turn_messages_open_again_after_proceed");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        sim_event done = {};
        done.kind = SIM_EVENT_FACILITY_COMPLETED; done.faction = 1; done.subject = 1; done.detail = 1;   /* a report */
        v.recorder.events[0] = done;
        v.recorder.event_count = 1;
        v.ui.feel.turn_messages = true;
        v.ui.round.show         = true;   /* M20: the message window opens only with the show on */
        ui_enter_game(&v.ui);
        ctx->Yield(3);
        IM_CHECK_EQ(v.ui.feel.message_count, 3u);              /* the heading, the report and the closing line (G2-007) */
        ctx->ItemClick("//###Messages/###next");
        ctx->ItemClick("//###Messages/###next");
        ctx->ItemClick("//###Messages/###next");
        ctx->Yield(2);
        IM_CHECK(!ui_feel_messages_open(&v.ui));
        v.recorder.events[v.recorder.event_count++] = done;    /* the report of the next turn */
        recorder_reset(&v.recorder);
        ui_context_open(&v.ui, &v.api, UI_CONTEXT_MAP_MENU, 400.0f, 300.0f);
        ctx->Yield(2);
        ui_test_proceed(ctx);
        ctx->Yield(2);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_END_TURN), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(ui_feel_messages_open(&v.ui));
        IM_CHECK_EQ(v.ui.feel.message_count, 3u);              /* the heading, the new report only, and the closing line */
        IM_CHECK_EQ(v.ui.feel.turn, 1u);
    };

    /* M20, the decision of the player: with the default setting, 進行 opens
       no message window, and the log holds the public events of the round.
       G3-017: the city commands of the other forces (a conscription with
       its number) stay out of the log. */
    t = IM_REGISTER_TEST(e, "ui", "proceed_with_the_default_setting_opens_no_message_and_the_log_keeps_the_round");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        sim_event rival = {};
        rival.kind = SIM_EVENT_CONSCRIPTED; rival.faction = 2; rival.subject = 1; rival.delta = 500; rival.turn = 1;
        sim_event report = {};
        report.kind = SIM_EVENT_FACILITY_COMPLETED; report.faction = 1; report.subject = 1; report.detail = 1; report.turn = 1;   /* a report */
        v.recorder.round_events[0] = rival;
        v.recorder.round_events[1] = report;
        v.recorder.round_event_count = 2;
        v.ui.feel.turn_messages = true;   /* as the game exe */
        IM_CHECK(!v.ui.round.show);       /* the default */
        ui_enter_game(&v.ui);
        ctx->Yield(3);
        recorder_reset(&v.recorder);
        ui_context_open(&v.ui, &v.api, UI_CONTEXT_MAP_MENU, 400.0f, 300.0f);
        ctx->Yield(2);
        ui_test_proceed(ctx);
        ctx->Yield(3);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_END_TURN), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(!ui_feel_messages_open(&v.ui) && !v.ui.round.playing);
        IM_CHECK(ctx->ItemExists("//###HUD/###hud_tabs/###hud_bases"));   /* the list stays */
        bool rival_line = false, report_line = false;
        for (u32 offset = 0; offset < v.ui.report.line_count; ++offset)
        {
            u32 line = v.ui.report.lines[(v.ui.report.line_first + offset) % UI_LOG_CAPACITY];
            rival_line  = rival_line || line == 0;
            report_line = report_line || line == 1;
        }
        IM_CHECK(!rival_line && report_line);   /* the conscription of the rival stays out (G3-017); the report of the player shows */
    };

    /* M20, with the show on: the HUD names each AI faction in the order of
       the round, the values rise in the order of the events, and no click
       is necessary. The messages of the player follow the show. */
    t = IM_REGISTER_TEST(e, "ui", "ai_show_names_each_faction_and_floats_the_events_in_order");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        sim_event first = {};
        first.kind = SIM_EVENT_CONSCRIPTED; first.faction = 2; first.subject = 1; first.delta = 500; first.turn = 1;
        sim_event second = first;
        second.kind = SIM_EVENT_MORALE_LOST; second.subject = 2; second.delta = -5;
        sim_event quiet = first;
        quiet.kind = SIM_EVENT_FACTION_TURN_ENDED;   /* the books of the turn: no step */
        sim_event third = first;
        third.faction = 3; third.delta = 120;
        sim_event report = {};
        report.kind = SIM_EVENT_FACILITY_COMPLETED; report.faction = 1; report.subject = 1; report.detail = 1; report.turn = 1;
        v.recorder.round_events[0] = first;
        v.recorder.round_events[1] = second;
        v.recorder.round_events[2] = quiet;
        v.recorder.round_events[3] = third;
        v.recorder.round_events[4] = report;
        v.recorder.round_event_count = 5;
        v.ui.feel.turn_messages = true;
        v.ui.round.show = true;
        ui_enter_game(&v.ui);
        ctx->Yield(3);
        recorder_reset(&v.recorder);
        ui_context_open(&v.ui, &v.api, UI_CONTEXT_MAP_MENU, 400.0f, 300.0f);
        ctx->Yield(2);
        ui_test_proceed(ctx);
        ctx->Yield();
        IM_CHECK(v.ui.round.playing);
        bool named_rival = false;
        for (u32 frame = 0; frame < 600 && v.ui.round.playing; ++frame)
        {
            ctx->Yield();
            named_rival = named_rival || ui_round_hud_force(&v.ui, &v.api) == 2;
            IM_CHECK(!ui_feel_messages_open(&v.ui));   /* no window during the show */
        }
        IM_CHECK(!v.ui.round.playing && named_rival);
        IM_CHECK(v.ui.round.named_count == 2 && v.ui.round.named[0] == 2 && v.ui.round.named[1] == 3);
        IM_CHECK_EQ(v.ui.round.shown_count, 3u);
        IM_CHECK(v.ui.round.shown[0] == 0 && v.ui.round.shown[1] == 1 && v.ui.round.shown[2] == 3);   /* the order of the events */
        ctx->Yield(2);
        IM_CHECK(ui_feel_messages_open(&v.ui));         /* the report of the player follows the show */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
    };

    /* M20, with the show on: a unit of an AI faction walks hex by hex from
       its hex before 進行 to its new hex. The camera stays. */
    t = IM_REGISTER_TEST(e, "ui", "ai_show_moves_a_unit_hex_by_hex_and_the_camera_stays");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.recorder.unit0_rival   = true;
        v.recorder.round_unit_shift = 3;                 /* unit 0 goes from q 1 to q 4 */
        sim_event moved = {};
        moved.kind = SIM_EVENT_UNIT_MOVED; moved.faction = 2; moved.detail = 0; moved.delta = 12; moved.turn = 1;
        v.recorder.round_events[0] = moved;
        v.recorder.round_event_count = 1;
        v.ui.scene.headless = true;
        v.ui.round.show     = true;
        ui_enter_game(&v.ui);
        ctx->Yield(3);
        v3 camera = v.ui.scene.cam.target;
        ui_context_open(&v.ui, &v.api, UI_CONTEXT_MAP_MENU, 400.0f, 300.0f);
        ctx->Yield(2);
        ui_test_proceed(ctx);
        ctx->Yield();
        IM_CHECK(v.ui.round.playing);
        s32 path[8];
        u32 path_count = 0;
        for (u32 frame = 0; frame < 600 && v.ui.round.playing; ++frame)
        {
            engine_unit unit = v.api.unit_at(v.api.engine, 0);
            hex_coord drawn = ui_round_unit_hex(&v.ui, &unit);
            if ((path_count == 0 || path[path_count - 1] != drawn.q) && path_count < ArrayCount(path)) path[path_count++] = drawn.q;
            ctx->Yield();
        }
        IM_CHECK(!v.ui.round.playing);
        IM_CHECK_EQ(path_count, 4u);                      /* q 1, 2, 3, 4: one hex in each step */
        IM_CHECK(path[0] == 1 && path[1] == 2 && path[2] == 3 && path[3] == 4);
        IM_CHECK(v.ui.scene.cam.target.x == camera.x && v.ui.scene.cam.target.z == camera.z);
    };

    /* M18, main-map.md "Speech bubble", "Floating value", "Bottom-left
       panel": a command of the player opens a bubble over the base, a value
       rises, and the log gets a heading and a line. The feedback commands
       nothing. The bubble closes by itself. */
    t = IM_REGISTER_TEST(e, "ui", "command_feedback_opens_the_bubble_the_value_and_a_log_line");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        ui_enter_game(&v.ui);
        ctx->Yield(2);
        IM_CHECK_EQ(v.ui.report.feedback_count, 0u);
        recorder_reset(&v.recorder);
        sim_event conscripted = {};
        conscripted.kind = SIM_EVENT_CONSCRIPTED; conscripted.faction = 1; conscripted.subject = 1;
        conscripted.delta = 1383; conscripted.cause = 7;
        sim_event rival = conscripted;
        rival.faction = 2; rival.cause = 8;                    /* another faction: no bubble, and no log line (G3-017: its number stays out) */
        v.recorder.events[v.recorder.event_count++] = conscripted;
        v.recorder.events[v.recorder.event_count++] = rival;
        ctx->Yield(2);
        IM_CHECK_EQ(v.ui.report.feedback_count, 1u);
        IM_CHECK_GT(v.ui.report.bubble_seconds, 0.0f);
        IM_CHECK_EQ(v.ui.report.float_count, 1u);
        IM_CHECK_EQ(v.ui.report.floats[0].delta, 1383);
        IM_CHECK(ctx->WindowInfo("//###Speech", ImGuiTestOpFlags_NoError).Window != 0);
        IM_CHECK(ctx->WindowInfo("//###Log", ImGuiTestOpFlags_NoError).Window != 0);
        IM_CHECK_EQ(v.ui.report.line_count, 2u);                /* the heading and the line of the player */
        IM_CHECK((v.ui.report.lines[v.ui.report.line_first] & UI_LOG_HEADING) != 0);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        v.ui.report.bubble_seconds = 0.001f;
        ctx->Yield(3);
        ImGuiWindow *bubble = ctx->WindowInfo("//###Speech", ImGuiTestOpFlags_NoError).Window;
        IM_CHECK(bubble == 0 || !bubble->Active);
    };

    /* The player, 2026-10-24: a log line names the facility where the
       event happens, and a click on the line frames the hex of its place.
       The click commands nothing. */
    t = IM_REGISTER_TEST(e, "ui", "a_log_line_names_its_facility_and_a_click_frames_it");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        ui_enter_game(&v.ui);
        ctx->Yield(2);
        sim_event damaged = {};
        damaged.kind = SIM_EVENT_FACILITY_DAMAGED; damaged.faction = 1; damaged.subject = 1;   /* city 0 */
        damaged.delta = -129; damaged.detail = RECORDER_SLOT_INDEX;
        v.recorder.events[v.recorder.event_count++] = damaged;
        ctx->Yield(2);
        IM_CHECK_EQ(v.ui.report.line_count, 2u);   /* the heading and the line */
        u32 place = (v.ui.report.line_first + 1) % UI_LOG_CAPACITY;
        hex_offset slot_cell = { RECORDER_SLOT_COL, RECORDER_SLOT_ROW };
        hex_coord slot_hex = hex_from_offset(slot_cell);
        IM_CHECK(v.ui.report.places[place].kind == ENGINE_PLACE_FACILITY && v.ui.report.places[place].q == slot_hex.q && v.ui.report.places[place].r == slot_hex.r);
        char line[512];
        ui_report_line_at(&v.ui, &v.api, v.ui.report.lines[place], v.ui.report.places + place, TEXT_NO_VALUE, TEXT_LINE_LOG, line, sizeof(line));
        IM_CHECK(strstr(line, ui_facility_name(&v.ui, &v.api, 0)) != 0);   /* 長安：市場受損, not 設施受損 */
        recorder_reset(&v.recorder);
        v.ui.map.scroll_request = false;
        ctx->ItemClick("//###Log/**/###log_line_1");
        ctx->Yield();
        IM_CHECK(v.ui.map.scroll_request && hex_equal(v.ui.map.scroll_to, slot_hex));
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* The player, 2026-10-24: a tactic line names the tactic and says if it
       worked. The event carries + the row + 1 for a success and - for a
       failure. */
    t = IM_REGISTER_TEST(e, "ui", "a_tactic_line_names_the_tactic_and_its_result");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        sim_event worked = {};
        worked.kind = SIM_EVENT_TACTIC_USED; worked.faction = 1; worked.delta = 1;   /* row 0 of recorder_tactic_kind_at: pierce */
        sim_event failed = worked;
        failed.delta = -1;
        v.recorder.events[v.recorder.event_count++] = worked;
        v.recorder.events[v.recorder.event_count++] = failed;
        ui_enter_game(&v.ui);
        ctx->Yield(2);
        char good[256], bad[256];
        ui_report_line(&v.ui, &v.api, 0, TEXT_NO_VALUE, TEXT_LINE_LOG, good, sizeof(good));
        ui_report_line(&v.ui, &v.api, 1, TEXT_NO_VALUE, TEXT_LINE_LOG, bad, sizeof(bad));
        const char *name = ui_text_with_key(&v.ui, "tactic", "pierce");
        IM_CHECK(strstr(good, name) != 0 && strstr(bad, name) != 0);
        IM_CHECK(strcmp(good, bad) != 0);   /* 突刺成功 and 突刺失敗，改為普通攻擊 */
        IM_CHECK(strstr(bad, "\xE5\xA4\xB1\xE6\x95\x97") != 0);   /* zh-TW: 失敗 */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* The player, 2026-10-24: the log goes to the bottom left and the
       minimap to the bottom right at the start, and again after a change of
       the screen size. A changed corner size stands for the new screen. */
    t = IM_REGISTER_TEST(e, "ui", "the_log_and_the_minimap_go_back_to_their_corners_after_a_resize");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        ui_enter_game(&v.ui);
        v.ui.show_minimap = true;
        ctx->Yield(3);
        ImVec2 display = ImGui::GetIO().DisplaySize;
        ImGuiWindow *log = ctx->WindowInfo("//###Log").Window;
        ImGuiWindow *minimap = ctx->WindowInfo("//###MiniMap").Window;
        IM_CHECK(log != 0 && minimap != 0);
        IM_CHECK(fabsf(log->Pos.x - UI_LOG_MARGIN) < 1.0f && fabsf(log->Pos.y + log->Size.y - (display.y - UI_LOG_MARGIN)) < 1.0f);
        IM_CHECK(fabsf(minimap->Pos.x + minimap->Size.x - (display.x - UI_MINIMAP_MARGIN)) < 1.0f &&
                 fabsf(minimap->Pos.y + minimap->Size.y - (display.y - UI_MINIMAP_MARGIN)) < 1.0f);
        ImGui::SetWindowPos(log, ImVec2(300.0f, 60.0f));
        ImGui::SetWindowPos(minimap, ImVec2(200.0f, 80.0f));
        ctx->Yield(2);
        IM_CHECK(fabsf(log->Pos.x - 300.0f) < 1.0f && fabsf(minimap->Pos.x - 200.0f) < 1.0f);   /* no resize: the windows stay */
        v.ui.report.corner.width = v.ui.minimap_corner.width = display.x - 100.0f;
        ctx->Yield(2);
        IM_CHECK(fabsf(log->Pos.x - UI_LOG_MARGIN) < 1.0f && fabsf(log->Pos.y + log->Size.y - (display.y - UI_LOG_MARGIN)) < 1.0f);
        IM_CHECK(fabsf(minimap->Pos.x + minimap->Size.x - (display.x - UI_MINIMAP_MARGIN)) < 1.0f &&
                 fabsf(minimap->Pos.y + minimap->Size.y - (display.y - UI_MINIMAP_MARGIN)) < 1.0f);
    };

    /* M18: the text provider is a plug-in (spec DECISIONS.md, 2026-10-20).
       A second provider writes the words of the log and of the bubble. A
       provider without a line function gives back the templates. */
    t = IM_REGISTER_TEST(e, "ui", "a_second_text_provider_replaces_the_words");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        sim_event trained = {};
        trained.kind = SIM_EVENT_TRAINED; trained.faction = 1; trained.subject = 1; trained.delta = 4; trained.cause = 3;
        v.recorder.events[v.recorder.event_count++] = trained;
        ui_enter_game(&v.ui);
        ctx->Yield(2);
        char before[256], after[256];
        ui_report_line(&v.ui, &v.api, 0, TEXT_NO_VALUE, TEXT_LINE_LOG, before, sizeof(before));
        text_provider model = {};
        model.name = "model";
        model.line = [](void *, text_catalog *, const text_event_facts *facts, u32 form, char *out, u32 out_size) -> u32
        {
            return text_copy_utf8(form == TEXT_LINE_SPEECH ? "the model speaks" : facts->key, out, out_size);
        };
        ui_set_text_provider(&v.ui, model);
        ui_report_line(&v.ui, &v.api, 0, TEXT_NO_VALUE, TEXT_LINE_LOG, after, sizeof(after));
        IM_CHECK_STR_EQ(after, "trained");
        IM_CHECK(strcmp(before, after) != 0);
        ui_report_line(&v.ui, &v.api, 0, TEXT_NO_VALUE, TEXT_LINE_SPEECH, after, sizeof(after));
        IM_CHECK_STR_EQ(after, "the model speaks");
        text_provider empty = {};
        ui_set_text_provider(&v.ui, empty);
        IM_CHECK_STR_EQ(v.ui.report.provider.name, "template");
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* G2-042: a point on a plate finds its unit; a unit with a long order
       shows 委 before 完. The map menu offers 委任部隊行動 only with an
       ordered unit, and the item sends one command_delegate. */
    t = IM_REGISTER_TEST(e, "ui", "plate_hit_and_the_mark_of_an_ordered_unit_and_the_delegate_item");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.ui.units.plate_count = 1;
        v.ui.units.plates[0].unit = 7;
        v.ui.units.plates[0].x0 = 10.0f; v.ui.units.plates[0].y0 = 10.0f;
        v.ui.units.plates[0].x1 = 50.0f; v.ui.units.plates[0].y1 = 30.0f;
        IM_CHECK_EQ(ui_plate_unit_at(&v.ui, 20.0f, 20.0f), 7u);
        IM_CHECK_EQ(ui_plate_unit_at(&v.ui, 60.0f, 20.0f), (u32)ENGINE_NO_UNIT);
        engine_unit unit = {};
        unit.order = UNIT_ORDER_MOVE;
        IM_CHECK_EQ(ui_unit_mark_of(&unit, 0), (u32)UI_UNIT_MARK_DELEGATED);   /* 委 before its step */
        unit.moved = true;
        IM_CHECK_EQ(ui_unit_mark_of(&unit, 0), (u32)UI_UNIT_MARK_DONE);        /* 完 after its step of the turn (2026-10-24) */
        unit.moved = false;
        unit.acted = true;
        IM_CHECK_EQ(ui_unit_mark_of(&unit, 0), (u32)UI_UNIT_MARK_DONE);

        v.ui.hud_mode = true;
        ctx->Yield();
        ui_context_open(&v.ui, &v.api, UI_CONTEXT_MAP_MENU, 400.0f, 300.0f);
        ctx->Yield(2);
        ImGuiTestItemInfo grey = ctx->ItemInfo("//###MapMenu/###delegate");
        IM_CHECK(grey.ID != 0 && (grey.ItemFlags & ImGuiItemFlags_Disabled) == 0);   /* G2-033: no order, but the units have not acted: white */
        ui_context_close(&v.ui);
        v.recorder.unit_order = UNIT_ORDER_MOVE;
        ctx->Yield();
        ui_context_open(&v.ui, &v.api, UI_CONTEXT_MAP_MENU, 400.0f, 300.0f);
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("//###MapMenu/###delegate");
        ctx->Yield(2);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_DELEGATE), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
    };

    /* G2-042: after 決定 of 出征 the new unit is selected, so that its
       range shows at once. The recorder reports unit 0 as the unit of the
       commander 100. */
    t = IM_REGISTER_TEST(e, "ui", "sortie_decide_selects_the_new_unit");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_sortie         = true;
        ctx->SetRef("###Sortie");
        ctx->Yield(2);
        ctx->ItemClick("###preset1");                                                   /* G2-044: the preset fills the empty slots */
        ctx->Yield();
        IM_CHECK_EQ(v.ui.sortie.order.officers[0], 100u);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###decide");
        ctx->Yield(2);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_SORTIE), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.ui.units.selected == 0 && !v.ui.show_sortie);
    };

    /* G2-046: the unit panel shows the state, the skill of each officer,
       the ship with its navy grade and the captives. It asks the values
       of the ship class and commands nothing. */
    t = IM_REGISTER_TEST(e, "ui", "unit_panel_shows_the_state_the_ship_and_the_captives_and_commands_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.units.selected      = 0;
        v.ui.show_unit           = true;
        ctx->SetRef("###Unit");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_UNIT_VALUES), 1u);          /* the grade of the ship class */
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_UNIT_VALUES), 9u);     /* the boat: unit 0 has no ship of its own */
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* ---- clone gap report 2, group 3: the dialogs of ui_people.cpp -------- */

    /* G2-036: 決定 of 移動 sends one command_move_officers with the city,
       the target and the checked officers. */
    t = IM_REGISTER_TEST(e, "ui", "move_officers_decide_issues_one_command");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_move_officers  = true;
        v.ui.move.city           = 0;
        v.ui.move.target         = 1;
        v.ui.move.officers[0]    = 100;
        v.ui.move.count          = 1;
        ctx->SetRef("###MoveOfficers");
        ctx->Yield(2);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_TRAVEL_DAYS), 0u);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###decide");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_MOVE_OFFICERS), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.recorder.last_move.city_id == 0 && v.recorder.last_move.target_city == 1 && v.recorder.last_move.officers[0] == 100 &&
                 v.recorder.last_move.officers[1] == ENGINE_NO_OFFICER);
        IM_CHECK(!v.ui.show_move_officers);
    };

    /* G2-060: 決定 of 官職 sends one command_court APPOINT for each staged change. */
    t = IM_REGISTER_TEST(e, "ui", "posts_decide_issues_one_command_court_for_each_staged_change");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_posts          = true;
        v.ui.posts.post          = ENGINE_NO_ROW;
        v.ui.posts.officer       = ENGINE_NO_OFFICER;
        v.ui.posts.staged_officer[0] = 101; v.ui.posts.staged_post[0] = 1;
        v.ui.posts.staged_officer[1] = 102; v.ui.posts.staged_post[1] = ENGINE_NO_ROW;
        v.ui.posts.staged_count  = 2;
        ctx->SetRef("###Posts");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###decide");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_COURT), 2u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_COMMAND_COURT), (u32)ENGINE_COMMAND_APPOINT);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 2u);
        IM_CHECK(v.recorder.last_court.target == 102 && v.recorder.last_court.post == ENGINE_NO_ROW);
        IM_CHECK(!v.ui.show_posts && v.ui.posts.staged_count == 0);
    };

    /* G2-060: 決定 of 流放 sends one command_court EXILE. */
    t = IM_REGISTER_TEST(e, "ui", "exile_decide_issues_one_command_court");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_exile          = true;
        v.ui.exile.officer       = 101;
        ctx->SetRef("###Exile");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###decide");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_COURT), 1u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_COMMAND_COURT), (u32)ENGINE_COMMAND_EXILE);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.recorder.last_court.target == 101 && !v.ui.show_exile);
    };

    /* G2-040: the 褒賞 dialog checks every officer that the recorder marks
       with a rise (101 and 102) when it opens. A click on a row clears it.
       決定 sends one command_court REWARD with the checked set. */
    t = IM_REGISTER_TEST(e, "ui", "reward_dialog_decide_issues_one_command_court_with_the_checked_set");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_reward         = true;
        v.ui.reward.city         = 0;
        v.ui.reward.count        = 0;
        v.ui.reward.fill         = true;
        ctx->SetRef("###Reward");
        ctx->Yield(2);
        IM_CHECK(v.ui.reward.count == 2 && v.ui.reward.checked[0] == 101 && v.ui.reward.checked[1] == 102);
        IM_CHECK(ctx->ItemExists("reward_rows/###reward_row101") && ctx->ItemExists("reward_rows/###reward_row102"));
        IM_CHECK(!ctx->ItemExists("reward_rows/###reward_row100"));   /* the ruler is not in the list */
        ctx->ItemClick("reward_rows/###reward_row101");
        ctx->Yield();
        IM_CHECK(v.ui.reward.count == 1 && v.ui.reward.checked[0] == 102);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###decide");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_COURT), 1u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_COMMAND_COURT), (u32)ENGINE_COMMAND_REWARD);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.recorder.last_court.reward_count == 1 && v.recorder.last_court.rewards[0] == 102 && v.recorder.last_court.city_id == 0);
        IM_CHECK(!v.ui.show_reward);
    };

    /* G2-040: 一併解除 clears the set and greys 決定, 一併 checks every row,
       and 返回 closes the dialog. None of them commands anything. */
    t = IM_REGISTER_TEST(e, "ui", "reward_dialog_check_all_clear_all_and_back_command_nothing");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_reward         = true;
        v.ui.reward.city         = 0;
        v.ui.reward.count        = 0;
        v.ui.reward.fill         = true;
        ctx->SetRef("###Reward");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###reward_clear_all");
        ctx->Yield();
        IM_CHECK(v.ui.reward.count == 0);
        IM_CHECK((ctx->ItemInfo("###decide").ItemFlags & ImGuiItemFlags_Disabled) != 0);
        ctx->ItemClick("###reward_check_all");
        ctx->Yield();
        IM_CHECK(v.ui.reward.count == 2);
        ctx->ItemClick("###back");
        ctx->Yield();
        IM_CHECK(!v.ui.show_reward);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    /* The Court panel asks court_advice for 探索 and for 登庸 and shows the
       clue of the adviser next to each, without a command. */
    t = IM_REGISTER_TEST(e, "ui", "court_search_and_recruit_show_the_clue_of_the_adviser");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_court          = true;
        v.ui.court.officer       = 101;
        v.ui.court.target        = 102;
        ctx->SetRef("###Court");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->Yield(2);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_COURT_ADVICE), 2u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_COURT_ADVICE), (u32)ENGINE_COMMAND_SEARCH);
        IM_CHECK(ctx->ItemExists("###advice_search") && ctx->ItemExists("###advice_recruit"));
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        v.ui.court.target = ENGINE_NO_OFFICER;   /* no target: the recruit clue goes away */
        ctx->Yield(6);                           /* the item table of the test engine forgets an item after some frames */
        IM_CHECK(ctx->ItemExists("###advice_search") && !ctx->ItemExists("###advice_recruit"));
    };

    /* G2-065: 決定 of the plot dialog sends one command_court FACTION_PLOT
       with the kind, the target base and the executor. */
    t = IM_REGISTER_TEST(e, "ui", "plot_dialog_decide_issues_one_command_faction_plot");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        v.recorder.officer_count = 3;
        v.recorder.city_count    = 2;
        v.ui.show_plot           = true;
        v.ui.plot.city           = 0;
        v.ui.plot.kind           = SIM_PLOT_RUMOUR;
        v.ui.plot.target_city    = 1;
        v.ui.plot.officer        = 100;
        ctx->SetRef("###PlotDialog");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###decide");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_COURT), 1u);
        IM_CHECK_EQ(recorder_first_arg(&v.recorder, RECORDER_COMMAND_COURT), (u32)ENGINE_COMMAND_FACTION_PLOT);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.recorder.last_court.kind == SIM_PLOT_RUMOUR && v.recorder.last_court.target_city == 1 && v.recorder.last_court.officer == 100 &&
                 v.recorder.last_court.city_id == 0);
        IM_CHECK(!v.ui.show_plot);
    };

    /* ---- end to end: real engine behind the real panels ------------------- */

    t = IM_REGISTER_TEST(e, "e2e", "advance_turn_reaches_the_engine");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->VarsConstructor = [](void *vars) { ((ui_test_vars *)vars)->use_real_engine = true; };
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        ctx->SetRef("Simulation");
        IM_CHECK_EQ(v.api.turn(v.api.engine), 0u);
        ctx->ItemClick("Advance turn");
        ctx->Yield();
        IM_CHECK_EQ(v.api.turn(v.api.engine), 1u);
        IM_CHECK_EQ(v.api.ledger_count(v.api.engine), 1u);
        sim_event ev = v.api.ledger_at(v.api.engine, 0);
        IM_CHECK_EQ(ev.kind, SIM_EVENT_TURN_ADVANCED);
        IM_CHECK_EQ(ev.cause, 1u);
        ctx->SetRef("Event ledger");
        ctx->ItemClick("Clear");
        ctx->Yield();
        IM_CHECK_EQ(v.api.ledger_count(v.api.engine), 0u);
    };

    /* M14: a scripted turn of 柴桑 from sample.s11 with the game screens
       only. The debug panels stay hidden. 都市 then 巡察 runs the command,
       and 進行 of the map menu ends the turn. */
    t = IM_REGISTER_TEST(e, "e2e", "a_turn_of_chaisang_runs_from_the_game_screens");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->VarsConstructor = [](void *vars) { ((ui_test_vars *)vars)->use_real_engine = true; };
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        engine_api *api = &v.api;
        void *e = api->engine;
        ctx->SetRef("Simulation");
        ctx->ItemClick("Load sample save");
        ctx->Yield();
        IM_CHECK_EQ(api->city_count(e), 42u);
        ui_enter_game(&v.ui);   /* as a load from the title */
        ctx->Yield(2);
        IM_CHECK(!v.ui.show_debug_panels);
        u8 force = api->whose_turn(e);
        const u8 chaisang = 26;
        engine_city before = api->city_at(e, chaisang);
        IM_CHECK(before.city_id == chaisang && before.owner_force_id == force);

        v.ui.city.selected = chaisang;
        ui_context_open(&v.ui, api, UI_CONTEXT_CITY, 300.0f, 200.0f);
        ctx->Yield(2);
        ctx->SetRef("//###City");
        ctx->ItemClick("###menu_city");
        ctx->Yield();
        ctx->ItemClick("###base_patrol");
        ctx->Yield();
        IM_CHECK(v.ui.show_command[UI_COMMAND_PATROL] && v.ui.commands[UI_COMMAND_PATROL].city == chaisang);
        u32 actions = api->actions_left(e, force);
        ctx->SetRef("//###Patrol");
        ctx->ItemClick("###decide");
        ctx->Yield(2);
        IM_CHECK_LT(api->actions_left(e, force), actions);                   /* the patrol ran */
        IM_CHECK_EQ(v.ui.context.kind, (u8)UI_CONTEXT_NONE);                 /* and the City dialog closed */

        u32 turn = api->turn(e);
        ui_context_open(&v.ui, api, UI_CONTEXT_MAP_MENU, 400.0f, 300.0f);
        ctx->Yield(2);
        ctx->SetRef("//###MapMenu");
        ui_test_proceed(ctx);
        ctx->Yield(2);
        IM_CHECK(api->turn(e) != turn || api->whose_turn(e) != force);
        ImGuiWindow *bar = ctx->WindowInfo("//##MainMenuBar", ImGuiTestOpFlags_NoError).Window;
        IM_CHECK(!v.ui.show_debug_panels && (bar == 0 || !bar->Active));     /* no debug panel in the whole turn */
    };

    t = IM_REGISTER_TEST(e, "e2e", "load_sample_save_and_city_names_fill_the_panels");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->VarsConstructor = [](void *vars) { ((ui_test_vars *)vars)->use_real_engine = true; };
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        ctx->SetRef("Simulation");
        IM_CHECK_EQ(v.api.officer_count(v.api.engine), 0u);
        ctx->ItemClick("Load sample save");
        ctx->Yield();
        IM_CHECK_GE(v.api.officer_count(v.api.engine), 852u);
        IM_CHECK_EQ(v.api.city_count(v.api.engine), 42u);
        IM_CHECK_EQ(v.api.ledger_at(v.api.engine, 0).kind, SIM_EVENT_SAVE_LOADED);
        IM_CHECK(strncmp(v.ui.load_message, "loaded", 6) == 0);
        IM_CHECK_EQ(v.api.city_name_at(v.api.engine, 36).city_id, 36u);
        u32 last = v.api.ledger_count(v.api.engine) - 1;                  /* after the turn start of the load */
        IM_CHECK_EQ(v.api.ledger_at(v.api.engine, last).kind, SIM_EVENT_NAME_INDEX_LOADED);
        IM_CHECK(strncmp(v.ui.names_message, "names loaded", 12) == 0);

        /* The text module decodes each name of the real files: no U+FFFD. */
        text_utf8 replacement = text_utf8_encode(TEXT_REPLACEMENT_CHARACTER);
        char needle[TEXT_UTF8_MAX_BYTES + 1] = {};
        memcpy(needle, replacement.bytes, replacement.length);
        char name[64];
        u32 bad_names = 0;
        for (u32 index = 0; index < v.api.city_count(v.api.engine); ++index)
        {
            engine_city_name city = v.api.city_name_at(v.api.engine, index);
            text_name(&v.text, city.name, sizeof(city.name), name, sizeof(name));
            if (!name[0] || strstr(name, needle)) bad_names++;
        }
        for (u32 index = 0; index < v.api.officer_count(v.api.engine); ++index)
        {
            engine_officer officer = v.api.officer_at(v.api.engine, index);
            text_name(&v.text, officer.family_name, sizeof(officer.family_name), name, sizeof(name));
            if (strstr(name, needle)) bad_names++;
            text_name(&v.text, officer.given_name, sizeof(officer.given_name), name, sizeof(name));
            if (strstr(name, needle)) bad_names++;
        }
        IM_CHECK_EQ(bad_names, 0u);
        /* G2-010: the user-defined code FA45, the given name of the officer 249, reads as its glyph through names/zh-TW.tsv. */
        text_catalog_set_locale(&v.text, "zh-TW");
        const u8 jun[5] = { 0xFA, 0x45, 0, 0, 0 };
        text_name(&v.text, jun, sizeof(jun), name, sizeof(name));
        IM_CHECK(strcmp(name, "\xE5\x84\x81") == 0);
    };

    /* M17: the first hour from the title with the original timing and the
       message window. 重新開始新遊戲, 群雄割據, one ruler, the start, then
       six turns of 進行. Each turn start shows its messages, and the clicks
       close them. No step stalls: each wait has a frame limit. */
    t = IM_REGISTER_TEST(e, "e2e", "the_first_hour_runs_from_the_title_without_a_stall");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->VarsConstructor = [](void *vars) { ((ui_test_vars *)vars)->use_real_engine = true; };
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        engine_api *api = &v.api;
        void *e = api->engine;
        v.ui.show_title           = true;
        v.ui.feel.original_timing = true;
        v.ui.feel.turn_messages   = true;
        ctx->Yield(2);
        ctx->SetRef("//###Title");
        ctx->ItemClick("###title_new");
        for (u32 frame = 0; frame < 120 && !v.ui.show_scenarios; ++frame) ctx->Yield();
        IM_CHECK(v.ui.show_scenarios);
        ctx->SetRef("//###Scenarios");
        ctx->ItemClick("###sc1");
        ctx->Yield(2);
        IM_CHECK(v.ui.show_rulers);
        u8 player = ENGINE_NO_FACTION;
        for (u32 index = 0; index < api->faction_count(e) && player == ENGINE_NO_FACTION; ++index)
        {
            u8 force = api->faction_at(e, index).force_id;
            if (api->force_ruler(e, force) != ENGINE_NO_OFFICER) player = force;
        }
        IM_CHECK(player != ENGINE_NO_FACTION);
        char item[64];
        snprintf(item, sizeof(item), "//###Rulers/###ruler%u", player);
        ctx->ItemClick(item);
        ctx->ItemClick("//###Rulers/###rulers_ok");
        ctx->Yield(2);
        ctx->ItemClick("//###StartSettings/###start_ok");
        ctx->Yield(2);
        IM_CHECK(v.ui.hud_mode && api->whose_turn(e) == player);
        IM_CHECK_EQ(api->city_name_at(e, 0).city_id, 0u);   /* the start loaded the city names in the same step */

        for (u32 turn = 0; turn <= 6; ++turn)
        {
            if (turn > 0)
            {
                s64 before = api->date(e).days_since_start;
                ui_context_open(&v.ui, api, UI_CONTEXT_MAP_MENU, 400.0f, 300.0f);
                ctx->Yield(2);
                ui_test_proceed(ctx);
                for (u32 frame = 0; frame < 30 && !ui_feel_messages_open(&v.ui); ++frame) ctx->Yield();
                IM_CHECK_GT(api->date(e).days_since_start, before);   /* the other factions acted */
            }
            /* M18: a turn without a report of the player opens no window. */
            IM_CHECK_EQ(api->whose_turn(e), player);
            if (ui_feel_messages_open(&v.ui)) IM_CHECK_EQ(v.ui.feel.messages[0], UI_MESSAGE_HEADING);
            for (u32 click = 0; click < UI_MESSAGE_CAPACITY + 1 && ui_feel_messages_open(&v.ui); ++click)
            {
                ctx->ItemClick("//###Messages/###next");
                ctx->Yield();
            }
            IM_CHECK(!ui_feel_messages_open(&v.ui));
        }
        ImGuiWindow *bar = ctx->WindowInfo("//##MainMenuBar", ImGuiTestOpFlags_NoError).Window;
        IM_CHECK(!v.ui.show_debug_panels && (bar == 0 || !bar->Active));
    };
}

int
main(int argc, char **argv)
{
    setvbuf(stdout, 0, _IONBF, 0);
    const char *filter = argc > 1 ? argv[1] : 0;

    g_ui_test_text_arena = test_arena_create(UI_TEST_TEXT_ARENA_SIZE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1280, 800);
    io.IniFilename = 0;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_HasMouseCursors;
    io.Fonts->Build();

    ImGuiTestEngine *engine = ImGuiTestEngine_CreateContext();
    ImGuiTestEngineIO &test_io = ImGuiTestEngine_GetIO(engine);
    test_io.ConfigRunSpeed           = ImGuiTestRunSpeed_Fast;
    test_io.ConfigNoThrottle         = true;
    test_io.ConfigVerboseLevel       = ImGuiTestVerboseLevel_Warning;
    test_io.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;
    test_io.ConfigLogToTTY           = true;
    ImGuiTestEngine_Start(engine, ImGui::GetCurrentContext());
    register_ui_tests(engine);
    ImGuiTestEngine_QueueTests(engine, ImGuiTestGroup_Tests, filter, ImGuiTestRunFlags_RunFromCommandLine);

    u32 frames = 0;
    while (!ImGuiTestEngine_IsTestQueueEmpty(engine) && frames < 100000)
    {
        io.DeltaTime = 1.0f / 60.0f;
        ImGui::NewFrame();
        ImGui::Render();
        ImGuiTestEngine_PostSwap(engine);
        frames++;
    }

    int tested = 0, passed = 0;
    ImGuiTestEngine_GetResult(engine, tested, passed);
    printf("%s %d/%d ui (%u frames)\n", tested == passed ? "PASS" : "FAIL", passed, tested, frames);

    ImGuiTestEngine_Stop(engine);
    ImGui::DestroyContext();
    ImGuiTestEngine_DestroyContext(engine);
    printf(tested == passed ? "RESULT PASS\n" : "RESULT FAIL\n");
    return tested == passed ? 0 : 1;
}
