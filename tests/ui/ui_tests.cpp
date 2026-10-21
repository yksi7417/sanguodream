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
    ui_draw(&v.ui, &v.api, &v.frame);
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
    { "base_menu_people_move_is_grey",               "###menu_people",    "###base_move",      UI_EXPECT_GREY,      0 },
    { "base_menu_people_summon_is_grey",             "###menu_people",    "###base_summon",    UI_EXPECT_GREY,      0 },
    { "base_menu_people_search_opens_the_court",     "###menu_people",    "###base_search",    UI_EXPECT_COURT,     0 },
    { "base_menu_people_recruit_opens_the_court",    "###menu_people",    "###base_recruit",   UI_EXPECT_COURT,     0 },
    { "base_menu_people_reward_opens_the_court",     "###menu_people",    "###base_reward",    UI_EXPECT_COURT,     0 },
    { "base_menu_people_give_opens_the_court",       "###menu_people",    "###base_give",      UI_EXPECT_COURT,     0 },
    { "base_menu_diplomacy_opens_its_panel",         "###menu_diplomacy", "###menu_diplomacy", UI_EXPECT_DIPLOMACY, 0 },
    { "base_menu_plot_opens_the_diplomacy_panel",    "###menu_plot",      "###menu_plot",      UI_EXPECT_DIPLOMACY, 0 },
    { "base_menu_ruler_opens_its_panel",             "###menu_ruler",     "###menu_ruler",     UI_EXPECT_RULER,     0 },
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
        u32 counts[256];
        ui_count_built_facilities(&v.api, 0, counts, ArrayCount(counts));
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
            if (kind == UI_COMMAND_MERCHANT)
            {
                ctx->ItemClick("###sell");
                ctx->ItemInputValue("###amount", 500);
            }
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
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_SORTIE_PROPOSAL), 1u);       /* the dialog starts from 自動編制 */
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_UNIT_VALUES), 1u);           /* it shows the values of the unit */
        IM_CHECK_EQ(v.ui.sortie.order.officers[0], 100u);
        ctx->ComboClick("###unit_type/###u4");
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
        IM_CHECK(sent.city_id == 0 && sent.unit_type == 4 && sent.soldiers == 2000 && sent.officers[0] == 100 && sent.officers[1] == 101);
        IM_CHECK(!v.ui.show_sortie);
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

    t = IM_REGISTER_TEST(e, "ui", "map_click_selects_a_unit_and_moves_it_with_one_command");
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
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_MOVE), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.recorder.last_unit_command.unit == 0 && v.recorder.last_unit_command.q == 3 && v.recorder.last_unit_command.r == 2);
        IM_CHECK(v.ui.show_unit_menu);                                                    /* the command menu opens after the move */
    };

    t = IM_REGISTER_TEST(e, "ui", "unit_menu_wait_and_retreat_issue_one_command_each");
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
        v.ui.units.selected      = 0;
        v.ui.show_unit_menu      = true;
        ctx->SetRef("###UnitMenu");
        ctx->Yield(2);
        recorder_reset(&v.recorder);
        ctx->ItemClick("###tactic");
        ctx->Yield(2);
        IM_CHECK(v.ui.show_actions && v.ui.combat.list == UI_PICK_TACTIC);
        ctx->SetRef("###Actions");
        IM_CHECK(ctx->ItemExists("###a0") && !ctx->ItemExists("###a1"));               /* only the tactics of the class of the unit */
        ctx->ItemClick("###a0");
        ctx->Yield();
        IM_CHECK(v.ui.combat.pick == UI_PICK_TACTIC && v.ui.combat.row == 0 && !v.ui.show_actions);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        ui_test_click_cell(ctx, v, hex_make(2, 2));
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_TACTIC), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(v.recorder.last_unit_command.row == 0 && v.recorder.last_unit_command.q == 2);
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

    t = IM_REGISTER_TEST(e, "ui", "unit_menu_duel_waits_for_a_hex_and_issues_one_command_duel");
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
        ctx->ItemClick("###duel");
        ctx->Yield();
        IM_CHECK(v.ui.combat.pick == UI_PICK_DUEL);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        ui_test_click_cell(ctx, v, hex_make(2, 2));
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_COMMAND_DUEL), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
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
        IM_CHECK(v.recorder.last_court.officers[0] == 101 && v.recorder.last_court.city_id == 0);
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
        IM_CHECK_STR_EQ(v.recorder.last_replay_before, "pairs\\develop\\001\\before.s11");
        IM_CHECK_STR_EQ(v.recorder.last_replay_after, "pairs\\develop\\001\\after.s11");
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(ctx->ItemExists("fidelity_diffs"));
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_REPLAY_DIFF_AT), 1u);
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
                IM_CHECK_EQ(v.ui.city.menu, (u8)(1 + (strstr(test_case->menu, "military") ? 1 : strstr(test_case->menu, "people") ? 2 : 0)));
            }
            if (test_case->expect == UI_EXPECT_GREY)
            {
                IM_CHECK((ctx->ItemInfo(test_case->item).ItemFlags & ImGuiItemFlags_Disabled) != 0);
                IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
                return;
            }
            ctx->ItemClick(test_case->item);
            ctx->Yield();
            switch (test_case->expect)
            {
                case UI_EXPECT_DEVELOP:   IM_CHECK(v.ui.show_develop && v.ui.develop.city == 1); break;
                case UI_EXPECT_MERGE:     IM_CHECK(v.ui.show_merge && v.ui.merge.city == 1); break;
                case UI_EXPECT_COMMAND:   IM_CHECK(v.ui.show_command[test_case->ui_kind] && v.ui.commands[test_case->ui_kind].city == 1); break;
                case UI_EXPECT_RESEARCH:  IM_CHECK(v.ui.show_research && v.ui.research.city == 1); break;
                case UI_EXPECT_SORTIE:    IM_CHECK(v.ui.show_sortie && v.ui.sortie.city == 1); break;
                case UI_EXPECT_TRANSPORT: IM_CHECK(v.ui.show_transport && v.ui.transport.city == 1); break;
                case UI_EXPECT_COURT:     IM_CHECK(v.ui.show_court); break;
                case UI_EXPECT_DIPLOMACY: IM_CHECK(v.ui.show_diplomacy); break;
                case UI_EXPECT_RULER:     IM_CHECK(v.ui.show_ruler); break;
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
        ctx->ItemClick("###proceed");
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
        IM_CHECK_EQ(count, 1u);                                           /* the recorder has two markets of level 1 */
        IM_CHECK(rows[0].facility == 0 && rows[0].level == 1 && rows[0].count == 2);
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
        ui_enter_game(&v.ui);
        ctx->Yield(3);
        IM_CHECK_EQ(v.ui.feel.message_count, 2u);
        IM_CHECK_EQ(v.ui.feel.messages[0], UI_MESSAGE_HEADING);
        IM_CHECK_EQ(v.ui.feel.messages[1], 0u);
        IM_CHECK(ui_feel_messages_open(&v.ui));
        IM_CHECK(!ctx->ItemExists("//###HUD/###hud_tabs/###hud_bases"));   /* the list panel hides */
        recorder_reset(&v.recorder);
        ctx->SetRef("//###Messages");
        ctx->ItemClick("###next");
        ctx->Yield();
        IM_CHECK_EQ(v.ui.feel.message_shown, 1u);
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
        ui_enter_game(&v.ui);
        ctx->Yield(3);
        IM_CHECK_EQ(v.ui.feel.message_count, 2u);              /* the heading and the report */
        ctx->ItemClick("//###Messages/###next");
        ctx->ItemClick("//###Messages/###next");
        ctx->Yield(2);
        IM_CHECK(!ui_feel_messages_open(&v.ui));
        v.recorder.events[v.recorder.event_count++] = done;    /* the report of the next turn */
        recorder_reset(&v.recorder);
        ui_context_open(&v.ui, &v.api, UI_CONTEXT_MAP_MENU, 400.0f, 300.0f);
        ctx->Yield(2);
        ctx->ItemClick("//###MapMenu/###proceed");
        ctx->Yield(2);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_END_TURN), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK(ui_feel_messages_open(&v.ui));
        IM_CHECK_EQ(v.ui.feel.message_count, 2u);              /* the heading and the new report only */
        IM_CHECK_EQ(v.ui.feel.turn, 1u);
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
        rival.faction = 2; rival.cause = 8;                    /* another faction: a log line, no bubble */
        v.recorder.events[v.recorder.event_count++] = conscripted;
        v.recorder.events[v.recorder.event_count++] = rival;
        ctx->Yield(2);
        IM_CHECK_EQ(v.ui.report.feedback_count, 1u);
        IM_CHECK_GT(v.ui.report.bubble_seconds, 0.0f);
        IM_CHECK_EQ(v.ui.report.float_count, 1u);
        IM_CHECK_EQ(v.ui.report.floats[0].delta, 1383);
        IM_CHECK(ctx->WindowInfo("//###Speech", ImGuiTestOpFlags_NoError).Window != 0);
        IM_CHECK(ctx->WindowInfo("//###Log", ImGuiTestOpFlags_NoError).Window != 0);
        IM_CHECK_EQ(v.ui.report.line_count, 3u);                /* the heading and two lines */
        IM_CHECK((v.ui.report.lines[v.ui.report.line_first] & UI_LOG_HEADING) != 0);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
        v.ui.report.bubble_seconds = 0.001f;
        ctx->Yield(3);
        ImGuiWindow *bubble = ctx->WindowInfo("//###Speech", ImGuiTestOpFlags_NoError).Window;
        IM_CHECK(bubble == 0 || !bubble->Active);
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
        ctx->ItemClick("###proceed");
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
                ctx->ItemClick("//###MapMenu/###proceed");
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
