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

global_variable ui_command_case ui_command_cases[] = {
    { "patrol_decide_issues_one_command",    "patrol_back_issues_nothing",    "###Patrol",    "###open_patrol",    UI_COMMAND_PATROL,    ENGINE_COMMAND_PATROL },
    { "conscript_decide_issues_one_command", "conscript_back_issues_nothing", "###Conscript", "###open_conscript", UI_COMMAND_CONSCRIPT, ENGINE_COMMAND_CONSCRIPT },
    { "train_decide_issues_one_command",     "train_back_issues_nothing",     "###Train",     "###open_train",     UI_COMMAND_TRAIN,     ENGINE_COMMAND_TRAIN },
    { "merchant_decide_issues_one_command",  "merchant_back_issues_nothing",  "###Merchant",  "###open_merchant",  UI_COMMAND_MERCHANT,  ENGINE_COMMAND_MERCHANT },
    { "produce_decide_issues_one_command",   "produce_back_issues_nothing",   "###Produce",   "###open_produce",   UI_COMMAND_PRODUCE,   ENGINE_COMMAND_PRODUCE },
};

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
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_CELL_COST), 8u * 6u);   /* one per cell of the 8x6 fake map */
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_PATH_COST), 0u);        /* nothing selected yet */
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
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_OFFICER_AT), 3u);                /* the rows are still read */
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

    t = IM_REGISTER_TEST(e, "ui", "load_city_names_calls_load_name_index_once");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
        ctx->SetRef("Simulation");
        recorder_reset(&v.recorder);
        ctx->ItemClick("Load city names");
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_LOAD_NAME_INDEX), 1u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK_STR_EQ(v.recorder.last_name_index_path, UI_NAME_INDEX_PATH);
        IM_CHECK(strncmp(v.ui.names_message, "names loaded", 12) == 0);
    };

    t = IM_REGISTER_TEST(e, "ui", "load_sample_save_calls_load_save_once");
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
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 1u);
        IM_CHECK_STR_EQ(v.recorder.last_load_path, UI_SAMPLE_SAVE_PATH);
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
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_FACILITY_AT), (u32)RECORDER_SLOTS);   /* one call per slot */
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
        IM_CHECK_EQ(v.ui.develop.officers[0], 100u);                              /* the first free officer by default */
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_CAN_ISSUE), 1u);          /* the dialog asks can_issue each frame */
        ctx->ComboClick("###facility/###f2");
        ctx->ComboClick("###slot/###s4");
        ctx->ComboClick("###officer2/###o101");
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
            IM_CHECK_EQ(v.ui.commands[kind].officers[0], 100u);                         /* the dialog proposes the first free officer */
            IM_CHECK(ctx->ItemExists("###executor"));                                    /* 執行武將 */
            IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_CAN_ISSUE), 1u);
            if (kind != UI_COMMAND_MERCHANT) ctx->ComboClick("###officer2/###o101");
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
            IM_CHECK(sent.kind == test_case->engine_kind && sent.city_id == 0 && sent.officers[0] == 100);
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
        IM_CHECK_EQ(v.ui.research.officers[0], 100u);                             /* the first free officer by default */
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_FACTION_TECHNIQUES), 1u);
        IM_CHECK_GE(recorder_count(&v.recorder, RECORDER_CAN_ISSUE), 1u);
        ctx->ItemClick("###t1");                                                  /* the second technique of the first line */
        ctx->ComboClick("###officer2/###o101");
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
        ctx->ItemClick("Load city names");
        ctx->Yield();
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
