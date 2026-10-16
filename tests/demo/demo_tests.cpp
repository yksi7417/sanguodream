/* The demo walkthroughs: one test of category "demo" for each milestone.
   demo_player.exe runs them at cinematic speed in a real window, with the
   real engine behind the real panels. Each walkthrough records a video of
   the whole script and a PNG of each panel. It also checks the result of
   each step, so it is an end-to-end smoke test too.

   Rule: one demo test for each milestone. A new milestone adds one
   walkthrough here; it does not add a second test for an old milestone.

   Included by demo_player.cpp (unity build). */

#define DEMO_ENGINE_ARENA_SIZE Megabytes(32)

/* build\demos, with no last backslash. demo_player.cpp sets it. */
global_variable char g_demo_output_dir[MAX_PATH];

struct demo_vars
{
    engine_api    api;
    ui_state      ui;
    ui_frame_info frame;
    memory_arena  permanent;
    memory_arena  transient;
    engine       *backend;
};

/* The GUI of every walkthrough: the real panels over the real engine. All
   panels start closed; the script opens them through the Windows menu. */
internal void
demo_gui(ImGuiTestContext *ctx)
{
    demo_vars &v = ctx->GetVars<demo_vars>();
    if (!v.backend)
    {
        v.permanent = test_arena_create(DEMO_ENGINE_ARENA_SIZE);
        v.transient = test_arena_create(DEMO_ENGINE_ARENA_SIZE);
        engine_config config = engine_default_config();
        config.read_entire_file = test_platform_read_entire_file;
        v.backend = engine_create(&v.permanent, &v.transient, config);
        v.api     = engine_make_api(v.backend);
        ui_init(&v.ui);
        v.ui.show_demo     = false;
        v.ui.show_sim      = false;
        v.ui.show_ledger   = false;
        v.ui.show_memory   = false;
        v.ui.show_map      = false;
        v.ui.show_officers = false;
        v.ui.show_cities   = false;
    }
    v.frame.dt_seconds = ImGui::GetIO().DeltaTime;
    ui_draw(&v.ui, &v.api, &v.frame);
}

/* ---- steps shared by the walkthroughs ----------------------------------- */

/* True if the file exists and is not empty. */
internal bool
demo_file_written(const char *path)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data)) return false;
    return data.nFileSizeHigh != 0 || data.nFileSizeLow != 0;
}

/* Starts the video of the whole walkthrough: build\demos\<milestone>.mp4,
   or .gif if demo_player.cpp found no ffmpeg. It deletes the file of an
   earlier run, so that demo_end_video sees only a new file. */
internal bool
demo_begin_video(ImGuiTestContext *ctx, const char *milestone)
{
    ctx->CaptureReset();
    snprintf(ctx->CaptureArgs->InOutputFile, sizeof(ctx->CaptureArgs->InOutputFile), "%s\\%s%s",
             g_demo_output_dir, milestone, ctx->EngineIO->VideoCaptureExtension);
    DeleteFileA(ctx->CaptureArgs->InOutputFile);
    return ctx->CaptureBeginVideo();
}

/* Stops the video. The encoder has finished when CaptureEndVideo returns. */
internal bool
demo_end_video(ImGuiTestContext *ctx)
{
    IM_CHECK_RETV(ctx->CaptureEndVideo(), false);
    IM_CHECK_RETV(demo_file_written(ctx->CaptureArgs->InOutputFile), false);
    return true;
}

/* Writes build\demos\<milestone>_<file_name>.png with only this panel in it. */
internal bool
demo_screenshot_panel(ImGuiTestContext *ctx, const char *milestone, const char *window, const char *file_name)
{
    char window_ref[64];
    snprintf(window_ref, sizeof(window_ref), "//%s", window);
    ctx->CaptureReset();
    snprintf(ctx->CaptureArgs->InOutputFile, sizeof(ctx->CaptureArgs->InOutputFile), "%s\\%s_%s.png",
             g_demo_output_dir, milestone, file_name);
    DeleteFileA(ctx->CaptureArgs->InOutputFile);
    IM_CHECK_RETV(ctx->CaptureAddWindow(window_ref), false);
    IM_CHECK_RETV(ctx->CaptureScreenshot(ImGuiCaptureFlags_HideMouseCursor), false);
    IM_CHECK_RETV(demo_file_written(ctx->CaptureArgs->InOutputFile), false);
    return true;
}

/* Opens a panel through the Windows menu, then checks that its window is drawn. */
internal bool
demo_open_panel(ImGuiTestContext *ctx, const char *window, bool *shown)
{
    char menu_path[64];
    char window_ref[64];
    snprintf(menu_path, sizeof(menu_path), "Windows/%s", window);
    snprintf(window_ref, sizeof(window_ref), "//%s", window);
    ctx->SetRef("##MainMenuBar");
    ctx->MenuCheck(menu_path);
    ctx->Yield();
    IM_CHECK_RETV(*shown, false);
    ImGuiWindow *found = ctx->WindowInfo(window_ref).Window;
    IM_CHECK_RETV(found != 0 && found->WasActive, false);
    return true;
}

/* Scrolls a table to its last row, then back to its first row. A table with
   a vertical scroll bar draws its rows into a child window. */
internal bool
demo_scroll_table(ImGuiTestContext *ctx, const char *window, const char *table)
{
    char ref[64];
    snprintf(ref, sizeof(ref), "//%s/%s", window, table);
    ImGuiWindow *rows = ctx->WindowInfo(ref).Window;
    IM_CHECK_RETV(rows != 0, false);
    IM_CHECK_RETV(rows->ScrollMax.y > 0.0f, false);    /* more rows than the panel shows */
    ctx->ScrollToBottom(rows->ID);
    ctx->SleepStandard();
    IM_CHECK_RETV(rows->Scroll.y >= rows->ScrollMax.y - 1.0f, false);
    ctx->ScrollToTop(rows->ID);
    IM_CHECK_RETV(rows->Scroll.y < 1.0f, false);
    return true;
}

/* Moves the mouse to the center of a cell of the hex map and clicks it. */
internal void
demo_click_cell(ImGuiTestContext *ctx, demo_vars &v, hex_coord cell)
{
    ImGuiWindow *canvas = ctx->WindowInfo("//Hex map/map_canvas").Window;
    IM_CHECK(canvas != 0);
    ctx->SetRef(canvas);
    ImGuiTestItemInfo cells = ctx->ItemInfo("cells");
    IM_CHECK(cells.ID != 0);
    /* ui_draw_map puts the origin one cell size inside the canvas. */
    f32 zoom = v.ui.map.zoom;
    hex_point origin;
    origin.x = cells.RectFull.Min.x + zoom;
    origin.y = cells.RectFull.Min.y + zoom;
    hex_point center = hex_to_pixel(cell, zoom, origin);
    ctx->MouseMoveToPos(ImVec2(center.x, center.y));
    ctx->MouseClick(0);
    ctx->Yield();
}

internal hex_coord
demo_cell(s32 col, s32 row)
{
    hex_offset offset;
    offset.col = col;
    offset.row = row;
    return hex_from_offset(offset);
}

/* ---- the walkthroughs --------------------------------------------------- */

/* M0, the skeleton: the panels, a path on the hex map, turns and the ledger. */
internal void
demo_m0_skeleton(ImGuiTestContext *ctx)
{
    const char *milestone = "m0_skeleton";
    demo_vars &v = ctx->GetVars<demo_vars>();
    IM_CHECK(demo_begin_video(ctx, milestone));

    IM_CHECK(demo_open_panel(ctx, "Simulation", &v.ui.show_sim));
    IM_CHECK(demo_open_panel(ctx, "Hex map", &v.ui.show_map));
    IM_CHECK(demo_open_panel(ctx, "Event ledger", &v.ui.show_ledger));
    IM_CHECK(demo_open_panel(ctx, "Memory", &v.ui.show_memory));

    hex_coord from = demo_cell(2, 2);
    hex_coord to   = demo_cell(9, 7);
    demo_click_cell(ctx, v, from);
    IM_CHECK(v.ui.map.has_from && hex_equal(v.ui.map.from, from));
    demo_click_cell(ctx, v, to);
    IM_CHECK(v.ui.map.has_to && hex_equal(v.ui.map.to, to));
    u32 cost = v.api.path_cost(v.api.engine, from.q, from.r, to.q, to.r);
    IM_CHECK(cost != HEX_NO_PATH && cost > 0);
    ctx->SleepStandard();

    ctx->SetRef("Simulation");
    ctx->ItemClick("Advance turn");
    IM_CHECK_EQ(v.api.turn(v.api.engine), 1u);
    ctx->ItemClick("Advance 12 turns");
    IM_CHECK_EQ(v.api.turn(v.api.engine), 13u);
    ctx->ItemClick("Advance 12 turns");
    IM_CHECK_EQ(v.api.turn(v.api.engine), 25u);

    ctx->SetRef("Event ledger");
    IM_CHECK(ctx->ItemExists("ledger"));
    IM_CHECK_EQ(v.api.ledger_count(v.api.engine), 25u);       /* one event for each turn */
    IM_CHECK(demo_scroll_table(ctx, "Event ledger", "ledger"));
    ctx->SleepStandard();

    IM_CHECK(demo_end_video(ctx));

    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Simulation", "simulation"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Hex map", "hex_map"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Event ledger", "event_ledger"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Memory", "memory"));
}

/* M1, the loader: load the sample save and the city names, then scroll the
   Officers and Cities panels. */
internal void
demo_m1_loader(ImGuiTestContext *ctx)
{
    const char *milestone = "m1_loader";
    demo_vars &v = ctx->GetVars<demo_vars>();
    IM_CHECK(demo_begin_video(ctx, milestone));

    IM_CHECK(demo_open_panel(ctx, "Simulation", &v.ui.show_sim));
    IM_CHECK(demo_open_panel(ctx, "Officers", &v.ui.show_officers));
    IM_CHECK(demo_open_panel(ctx, "Cities", &v.ui.show_cities));
    IM_CHECK_EQ(v.api.officer_count(v.api.engine), 0u);

    ctx->SetRef("Simulation");
    ctx->ItemClick("Load sample save");
    ctx->Yield();
    IM_CHECK(strncmp(v.ui.load_message, "loaded", 6) == 0);
    IM_CHECK_GE(v.api.officer_count(v.api.engine), 852u);
    IM_CHECK_EQ(v.api.city_count(v.api.engine), 42u);
    ctx->ItemClick("Load city names");
    ctx->Yield();
    IM_CHECK(strncmp(v.ui.names_message, "names loaded", 12) == 0);
    IM_CHECK_EQ(v.api.city_name_at(v.api.engine, 36).city_id, 36u);
    ctx->SleepStandard();

    IM_CHECK(demo_scroll_table(ctx, "Officers", "officers"));
    IM_CHECK(demo_scroll_table(ctx, "Cities", "cities"));
    ctx->SleepStandard();

    IM_CHECK(demo_end_video(ctx));

    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Simulation", "simulation"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Officers", "officers"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Cities", "cities"));
}

internal void
register_demo_tests(ImGuiTestEngine *e)
{
    ImGuiTest *t = 0;

    t = IM_REGISTER_TEST(e, "demo", "m0_skeleton");
    t->SetVarsDataType<demo_vars>();
    t->GuiFunc  = demo_gui;
    t->TestFunc = demo_m0_skeleton;

    t = IM_REGISTER_TEST(e, "demo", "m1_loader");
    t->SetVarsDataType<demo_vars>();
    t->GuiFunc  = demo_gui;
    t->TestFunc = demo_m1_loader;
}
