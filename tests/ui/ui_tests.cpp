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
#include "ui/ui.h"
#include "ui/ui.cpp"
#include "test_framework.h"
#include "test_files.h"
#include "engine_recorder.h"

/* Per-test state. Zeroed by the test engine before each test. */
struct ui_test_vars
{
    engine_recorder recorder;
    engine_api      api;
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
            v.backend = engine_create(&v.permanent, &v.transient, config);
            v.api     = engine_make_api(v.backend);
        }
        else
        {
            v.api = recorder_make_api(&v.recorder);
        }
        ui_init(&v.ui);
        v.ui.show_demo = false;   /* the demo window is not under test */
    }
    v.frame.dt_seconds = ImGui::GetIO().DeltaTime;
    ui_draw(&v.ui, &v.api, &v.frame);
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
        ctx->MenuUncheck("Windows/Simulation");
        ctx->Yield();
        IM_CHECK(!v.ui.show_sim);
        IM_CHECK(!ctx->WindowInfo("//Simulation", ImGuiTestOpFlags_NoError).Window ||
                 !ctx->WindowInfo("//Simulation", ImGuiTestOpFlags_NoError).Window->WasActive);
        ctx->MenuCheck("Windows/Simulation");
        ctx->Yield();
        IM_CHECK(v.ui.show_sim);
        ctx->MenuCheck("Windows/Memory");
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
        ctx->SetRef("Officers");
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
        ctx->SetRef("Cities");
        ctx->Yield(2);
        IM_CHECK(ctx->ItemExists("cities"));
        recorder_reset(&v.recorder);
        ctx->Yield();
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_CITY_COUNT), 1u);
        IM_CHECK_EQ(recorder_count(&v.recorder, RECORDER_CITY_AT), 4u);
        IM_CHECK_EQ(recorder_count_commands(&v.recorder), 0u);
    };

    t = IM_REGISTER_TEST(e, "ui", "load_sample_save_calls_load_save_once");
    t->SetVarsDataType<ui_test_vars>();
    t->GuiFunc  = ui_test_gui;
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ui_test_vars &v = ctx->GetVars<ui_test_vars>();
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

    t = IM_REGISTER_TEST(e, "e2e", "load_sample_save_fills_the_panels");
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
    };
}

int
main(int argc, char **argv)
{
    setvbuf(stdout, 0, _IONBF, 0);
    const char *filter = argc > 1 ? argv[1] : 0;

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
