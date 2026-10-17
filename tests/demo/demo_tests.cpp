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
/* This frame's render commands and arena; demo_player.cpp sets them each frame. */
global_variable render_commands *g_demo_render;
global_variable memory_arena    *g_demo_frame_arena;

struct demo_vars
{
    engine_api    api;
    text_catalog  text;
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
        text_catalog_init(&v.text, arena_sub(&v.permanent, Megabytes(4)), test_platform_read_entire_file,
                          HEXDOM_DATA_ROOT, HEXDOM_CONTENT_PACK, "en");
        text_catalog_set_locale(&v.text, "zh-TW");
        ui_init(&v.ui, &v.text);
        v.ui.show_demo     = false;
        v.ui.show_sim      = false;
        v.ui.show_ledger   = false;
        v.ui.show_memory   = false;
        v.ui.show_map      = false;
        v.ui.show_officers = false;
        v.ui.show_cities   = false;
        v.ui.show_scene    = false;
        v.ui.show_city     = false;
        v.ui.show_develop  = false;
        v.ui.show_economy  = false;
        v.ui.show_techniques = false;
    }
    v.frame.dt_seconds  = ImGui::GetIO().DeltaTime;
    v.frame.render      = g_demo_render;
    v.frame.frame_arena = g_demo_frame_arena;
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

/* Opens a panel through the Windows menu, then checks that its window is drawn.
   A panel with a localized title has a stable id: pass "###Officers". */
internal bool
demo_open_panel(ImGuiTestContext *ctx, const char *window, bool *shown)
{
    char menu_path[64];
    char window_ref[64];
    snprintf(menu_path, sizeof(menu_path), "###Windows/%s", window);
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

    /* the 2.5D scene: orbit, tilt and zoom with the Scene panel's buttons */
    IM_CHECK(demo_open_panel(ctx, "Scene", &v.ui.show_scene));
    ctx->SetRef("Scene");
    f32 yaw_before = v.ui.scene.cam.yaw;
    f32 distance_before = v.ui.scene.cam.distance;
    ctx->ItemClick("Orbit right");
    ctx->ItemClick("Orbit right");
    ctx->ItemClick("Orbit right");
    IM_CHECK(v.ui.scene.cam.yaw != yaw_before);
    ctx->ItemClick("Zoom in");
    ctx->ItemClick("Zoom in");
    IM_CHECK(v.ui.scene.cam.distance < distance_before);
    ctx->ItemClick("Tilt up");
    ctx->ItemClick("Orbit left");
    ctx->SleepStandard();
    ctx->ItemClick("Reset view");
    IM_CHECK(v.ui.scene.cam.distance == distance_before);
    ctx->SleepStandard();

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
    u32 turn_events = 0;
    for (u32 index = 0; index < v.api.ledger_count(v.api.engine); ++index)
    {
        if (v.api.ledger_at(v.api.engine, index).kind == SIM_EVENT_TURN_ADVANCED) turn_events++;
    }
    IM_CHECK_EQ(turn_events, 25u);                            /* one turn event for each turn; the calendar adds month events */
    IM_CHECK(demo_scroll_table(ctx, "Event ledger", "ledger"));
    ctx->SleepStandard();

    IM_CHECK(demo_end_video(ctx));

    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Scene", "scene"));
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
    IM_CHECK(demo_open_panel(ctx, "###Officers", &v.ui.show_officers));
    IM_CHECK(demo_open_panel(ctx, "###Cities", &v.ui.show_cities));
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

    IM_CHECK(demo_scroll_table(ctx, "###Officers", "officers"));
    IM_CHECK(demo_scroll_table(ctx, "###Cities", "cities"));
    ctx->SleepStandard();

    IM_CHECK(demo_end_video(ctx));

    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Simulation", "simulation"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###Officers", "officers"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###Cities", "cities"));
}

/* The first city of the faction that acts with a free officer, or ENGINE_NO_CITY. */
internal u8
demo_city_of_player(engine_api *api)
{
    u8 player = api->whose_turn(api->engine);
    for (u32 city = 0; city < api->city_count(api->engine); ++city)
    {
        if (api->city_at(api->engine, city).owner_force_id != player) continue;
        u16 officer = ENGINE_NO_OFFICER;
        if (ui_free_officers(api, (u8)city, &officer, 1)) return (u8)city;
    }
    return ENGINE_NO_CITY;
}

/* M2, turns and the develop command: load the sample save, open a city of
   the player, develop a market, then advance turns until the market is
   complete, and show the ledger. */
internal void
demo_m2_turns(ImGuiTestContext *ctx)
{
    const char *milestone = "m2_turns";
    demo_vars &v = ctx->GetVars<demo_vars>();
    engine_api *api = &v.api;
    IM_CHECK(demo_begin_video(ctx, milestone));

    IM_CHECK(demo_open_panel(ctx, "Simulation", &v.ui.show_sim));
    IM_CHECK(demo_open_panel(ctx, "Event ledger", &v.ui.show_ledger));
    ctx->SetRef("Simulation");
    ctx->ItemClick("Load sample save");
    ctx->ItemClick("Load city names");
    ctx->Yield();
    IM_CHECK_EQ(api->city_count(api->engine), 42u);
    IM_CHECK(api->whose_turn(api->engine) != ENGINE_NO_FACTION);              /* the player's faction acts */
    ctx->SleepStandard();

    u8 city = demo_city_of_player(api);
    IM_CHECK(city != ENGINE_NO_CITY);
    IM_CHECK(demo_open_panel(ctx, "###City", &v.ui.show_city));
    ctx->SetRef("###City");
    char item[32];
    snprintf(item, sizeof(item), "###city_select/###c%u", city);
    ctx->ComboClick(item);
    IM_CHECK_EQ(v.ui.city.selected, city);
    ctx->ItemClick("###open_develop");
    ctx->Yield();
    IM_CHECK(v.ui.show_develop);

    ctx->SetRef("###Develop");
    ctx->ComboClick("###facility/###f0");                                    /* market */
    ctx->Yield();
    IM_CHECK(v.ui.develop.officers[0] != ENGINE_NO_OFFICER);
    engine_command command = {};
    command.kind = ENGINE_COMMAND_DEVELOP;
    command.city_id = city; command.facility = 0; command.slot = v.ui.develop.slot;
    for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place) command.officers[place] = v.ui.develop.officers[place];
    engine_check check = api->can_issue(api->engine, &command);
    IM_CHECK_EQ(check.refusal, SIM_REFUSAL_NONE);
    ctx->SleepStandard();
    ctx->ItemClick("###execute");
    ctx->Yield();
    engine_facility market = api->facility_at(api->engine, city, check.slot);
    IM_CHECK(market.state == SIM_SLOT_BUILDING && market.turns_left == check.turns);

    ctx->SetRef("Simulation");
    for (u32 turn = 0; turn < check.turns; ++turn)
    {
        ctx->ItemClick("Advance turn");
        ctx->Yield();
    }
    market = api->facility_at(api->engine, city, check.slot);
    IM_CHECK_EQ(market.state, (u8)SIM_SLOT_BUILT);                           /* complete after the period */
    b32 completed = false;
    for (u32 index = 0; index < api->ledger_count(api->engine); ++index)
    {
        if (api->ledger_at(api->engine, index).kind == SIM_EVENT_FACILITY_COMPLETED) completed = true;
    }
    IM_CHECK(completed);
    ctx->SleepStandard();

    ctx->SetRef("Event ledger");
    IM_CHECK(demo_scroll_table(ctx, "Event ledger", "ledger"));
    ctx->SleepStandard();

    IM_CHECK(demo_end_video(ctx));

    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Simulation", "simulation"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###City", "city"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###Develop", "develop"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Event ledger", "event_ledger"));
}

/* The owned city of the faction that acts with the most free officers. */
internal u8
demo_busiest_city_of_player(engine_api *api)
{
    u8 player = api->whose_turn(api->engine);
    u8 best = ENGINE_NO_CITY;
    u32 best_count = 0;
    for (u32 city = 0; city < api->city_count(api->engine); ++city)
    {
        if (api->city_at(api->engine, city).owner_force_id != player) continue;
        u16 ids[64];
        u32 count = ui_free_officers(api, (u8)city, ids, ArrayCount(ids));
        if (count > best_count) { best = (u8)city; best_count = count; }
    }
    return best;
}

/* Develops a facility in the Develop dialog with the first three free
   officers. Returns the slot. */
internal u8
demo_develop(ImGuiTestContext *ctx, demo_vars &v, u8 city, u8 facility)
{
    engine_api *api = &v.api;
    ctx->SetRef("###City");
    ctx->ItemClick("###open_develop");
    ctx->Yield();
    ctx->SetRef("###Develop");
    char item[32];
    snprintf(item, sizeof(item), "###facility/###f%u", facility);
    ctx->ComboClick(item);
    u16 ids[3] = { ENGINE_NO_OFFICER, ENGINE_NO_OFFICER, ENGINE_NO_OFFICER };
    ui_free_officers(api, city, ids, 3);
    for (u32 place = 1; place < 3; ++place)
    {
        if (ids[place] == ENGINE_NO_OFFICER) continue;
        snprintf(item, sizeof(item), "###officer%u/###o%u", place + 1, ids[place]);
        ctx->ComboClick(item);
    }
    ctx->Yield();
    engine_command command = {};
    command.kind = ENGINE_COMMAND_DEVELOP;
    command.city_id = city; command.facility = facility; command.slot = v.ui.develop.slot;
    for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place) command.officers[place] = v.ui.develop.officers[place];
    engine_check check = api->can_issue(api->engine, &command);
    IM_CHECK_RETV(check.refusal == SIM_REFUSAL_NONE, ENGINE_ANY_SLOT);
    ctx->ItemClick("###execute");
    ctx->Yield();
    IM_CHECK_RETV(api->facility_at(api->engine, city, check.slot).state == SIM_SLOT_BUILDING, ENGINE_ANY_SLOT);
    return check.slot;
}

/* Opens a city command dialog from the City panel and clicks 決定. */
internal void
demo_city_command(ImGuiTestContext *ctx, demo_vars &v, const char *open_button, const char *window, u32 kind, s32 weapon_button)
{
    ctx->SetRef("###City");
    ctx->ItemClick(open_button);
    ctx->Yield();
    IM_CHECK(v.ui.show_command[kind]);
    ctx->SetRef(window);
    if (weapon_button >= 0)
    {
        char item[16];
        snprintf(item, sizeof(item), "###w%d", weapon_button);
        ctx->ItemClick(item);
    }
    ctx->SleepStandard();
    ctx->ItemClick("###decide");
    ctx->Yield();
    IM_CHECK(!v.ui.show_command[kind]);                                      /* 決定 ran the command and closed the dialog */
}

/* M3, the economy: load the sample save, open a city of the player, build a
   barracks and a smithy, run patrol, conscript, train and produce, advance
   three months, then show the economy summary and the ledger of the city
   filtered to its gold. */
internal void
demo_m3_economy(ImGuiTestContext *ctx)
{
    const char *milestone = "m3_economy";
    demo_vars &v = ctx->GetVars<demo_vars>();
    engine_api *api = &v.api;
    IM_CHECK(demo_begin_video(ctx, milestone));

    IM_CHECK(demo_open_panel(ctx, "Simulation", &v.ui.show_sim));
    IM_CHECK(demo_open_panel(ctx, "Event ledger", &v.ui.show_ledger));
    ctx->SetRef("Simulation");
    ctx->ItemClick("Load sample save");
    ctx->ItemClick("Load city names");
    ctx->Yield();
    IM_CHECK_EQ(api->city_count(api->engine), 42u);
    u8 player = api->whose_turn(api->engine);
    IM_CHECK(player != ENGINE_NO_FACTION);

    u8 city = demo_busiest_city_of_player(api);
    IM_CHECK(city != ENGINE_NO_CITY);
    IM_CHECK(demo_open_panel(ctx, "###City", &v.ui.show_city));
    ctx->SetRef("###City");
    char item[32];
    snprintf(item, sizeof(item), "###city_select/###c%u", city);
    ctx->ComboClick(item);
    IM_CHECK_EQ(v.ui.city.selected, city);
    ctx->SleepStandard();

    /* Conscription needs a barracks and the pikes need a smithy. */
    u8 barracks = demo_develop(ctx, v, city, 2);
    u8 smithy   = demo_develop(ctx, v, city, 3);
    IM_CHECK(barracks != ENGINE_ANY_SLOT && smithy != ENGINE_ANY_SLOT);
    v.ui.show_develop = false;
    ctx->SetRef("Simulation");
    for (u32 turn = 0; turn < 12; ++turn)
    {
        if (api->facility_at(api->engine, city, barracks).state == SIM_SLOT_BUILT &&
            api->facility_at(api->engine, city, smithy).state == SIM_SLOT_BUILT) break;
        ctx->ItemClick("Advance turn");
        ctx->Yield();
    }
    IM_CHECK_EQ(api->facility_at(api->engine, city, barracks).state, (u8)SIM_SLOT_BUILT);
    IM_CHECK_EQ(api->facility_at(api->engine, city, smithy).state, (u8)SIM_SLOT_BUILT);

    engine_city_economy before = api->city_economy(api->engine, city);
    u32 mark = api->ledger_count(api->engine);
    demo_city_command(ctx, v, "###open_patrol", "###Patrol", UI_COMMAND_PATROL, -1);
    demo_city_command(ctx, v, "###open_conscript", "###Conscript", UI_COMMAND_CONSCRIPT, -1);
    demo_city_command(ctx, v, "###open_train", "###Train", UI_COMMAND_TRAIN, -1);
    demo_city_command(ctx, v, "###open_produce", "###Produce", UI_COMMAND_PRODUCE, 0);
    engine_city_economy after = api->city_economy(api->engine, city);
    b32 patrolled = false, conscripted = false, trained = false, produced = false;
    for (u32 index = mark; index < api->ledger_count(api->engine); ++index)
    {
        sim_event_kind kind = api->ledger_at(api->engine, index).kind;
        patrolled   |= kind == SIM_EVENT_PATROLLED;
        conscripted |= kind == SIM_EVENT_CONSCRIPTED;
        trained     |= kind == SIM_EVENT_TRAINED;
        produced    |= kind == SIM_EVENT_WEAPONS_PRODUCED;
    }
    IM_CHECK(patrolled && conscripted && trained && produced);              /* the four commands are in the ledger */
    IM_CHECK(after.soldiers > before.soldiers && after.stocks[0] > before.stocks[0]);
    ctx->SleepStandard();

    ctx->SetRef("Simulation");
    s64 start_day = api->date(api->engine).days_since_start;
    for (u32 turn = 0; turn < 9; ++turn)
    {
        ctx->ItemClick("Advance turn");
        ctx->Yield();
    }
    IM_CHECK_EQ(api->date(api->engine).days_since_start, start_day + 90);  /* three months */

    IM_CHECK(demo_open_panel(ctx, "###Economy", &v.ui.show_economy));
    ctx->SetRef("###Economy");
    IM_CHECK(ctx->ItemExists("economy"));
    ctx->SleepStandard();

    ctx->SetRef("Event ledger");
    snprintf(item, sizeof(item), "###ledger_city/###lc%u", city);
    ctx->ComboClick(item);
    snprintf(item, sizeof(item), "###ledger_resource/###lr%u", (u32)SIM_RESOURCE_GOLD);
    ctx->ComboClick(item);
    ctx->Yield();
    IM_CHECK(v.ui.ledger_city == city && v.ui.ledger_resource == SIM_RESOURCE_GOLD);
    engine_ledger_view gold = api->ledger_filter(api->engine, city, SIM_RESOURCE_GOLD);
    IM_CHECK_GT(gold.count, 3u);                                             /* income, salaries and the costs of the commands */
    ctx->SleepStandard();

    IM_CHECK(demo_end_video(ctx));

    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Simulation", "simulation"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###City", "city"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###Economy", "economy"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Event ledger", "event_ledger"));
}

/* M4, growth: build two markets side by side, merge them to level 2,
   research the veterans technique, watch the technique table, start a
   disaster in the city and read it in the City panel and the ledger. The
   save has no technique points that the loader can read, so the walkthrough
   gives the faction 3000 with the editor call. */
internal void
demo_m4_growth(ImGuiTestContext *ctx)
{
    const char *milestone = "m4_growth";
    demo_vars &v = ctx->GetVars<demo_vars>();
    engine_api *api = &v.api;
    IM_CHECK(demo_begin_video(ctx, milestone));

    IM_CHECK(demo_open_panel(ctx, "Simulation", &v.ui.show_sim));
    IM_CHECK(demo_open_panel(ctx, "Event ledger", &v.ui.show_ledger));
    ctx->SetRef("Simulation");
    ctx->ItemClick("Load sample save");
    ctx->ItemClick("Load city names");
    ctx->Yield();
    IM_CHECK_EQ(api->city_count(api->engine), 42u);
    u8 player = api->whose_turn(api->engine);
    IM_CHECK(player != ENGINE_NO_FACTION);
    api->set_technique_points(api->engine, player, 3000);

    u8 city = demo_busiest_city_of_player(api);
    IM_CHECK(city != ENGINE_NO_CITY);
    IM_CHECK(demo_open_panel(ctx, "###City", &v.ui.show_city));
    ctx->SetRef("###City");
    char item[32];
    snprintf(item, sizeof(item), "###city_select/###c%u", city);
    ctx->ComboClick(item);
    IM_CHECK_EQ(v.ui.city.selected, city);

    /* Two markets in the first two slots: the two slots are neighbours. */
    u8 first  = demo_develop(ctx, v, city, 0);
    u8 second = demo_develop(ctx, v, city, 0);
    IM_CHECK(first != ENGINE_ANY_SLOT && second != ENGINE_ANY_SLOT);
    v.ui.show_develop = false;
    ctx->SetRef("Simulation");
    for (u32 turn = 0; turn < 12; ++turn)
    {
        if (api->facility_at(api->engine, city, first).state == SIM_SLOT_BUILT &&
            api->facility_at(api->engine, city, second).state == SIM_SLOT_BUILT) break;
        ctx->ItemClick("Advance turn");
        ctx->Yield();
    }
    IM_CHECK_EQ(api->facility_at(api->engine, city, second).state, (u8)SIM_SLOT_BUILT);

    /* The merge: the first market absorbs the second. */
    u32 mark = api->ledger_count(api->engine);
    ctx->SetRef("###City");
    ctx->ItemClick("###open_merge");
    ctx->Yield();
    ctx->SetRef("###Merge");
    snprintf(item, sizeof(item), "###absorber/###a%u", first);
    ctx->ComboClick(item);
    snprintf(item, sizeof(item), "###absorbed/###b%u", second);
    ctx->ComboClick(item);
    ctx->SleepStandard();
    ctx->ItemClick("###decide");
    ctx->Yield();
    IM_CHECK(!v.ui.show_merge);
    IM_CHECK_EQ(api->facility_at(api->engine, city, second).state, (u8)SIM_SLOT_MERGING);

    /* The research: the veterans technique of the drill line. */
    ctx->SetRef("###City");
    ctx->ItemClick("###open_research");
    ctx->Yield();
    ctx->SetRef("###Research");
    ctx->ItemClick("###t16");
    ctx->SleepStandard();
    ctx->ItemClick("###decide");
    ctx->Yield();
    IM_CHECK(!v.ui.show_research);
    IM_CHECK_EQ(api->faction_techniques(api->engine, player).researching, (u8)16);
    IM_CHECK(demo_open_panel(ctx, "###Techniques", &v.ui.show_techniques));
    ctx->SleepStandard();

    ctx->SetRef("Simulation");
    for (u32 turn = 0; turn < 12; ++turn)
    {
        if (api->faction_techniques(api->engine, player).researching == ENGINE_NO_TECHNIQUE &&
            api->facility_at(api->engine, city, first).level == 2) break;
        ctx->ItemClick("Advance turn");
        ctx->Yield();
    }
    IM_CHECK_EQ(api->facility_at(api->engine, city, first).level, (u8)2);           /* the merge gave level 2 */
    IM_CHECK((api->faction_techniques(api->engine, player).owned >> 16) & 1);         /* the faction has the veterans technique */
    IM_CHECK_EQ(api->city_economy(api->engine, city).morale_max, 120u);               /* morale up to 120 */
    ctx->SleepStandard();

    /* A disaster: locusts in the city (the calendar starts them at random). */
    api->start_disaster(api->engine, city, 0);
    ctx->SetRef("Simulation");
    ctx->ItemClick("Advance turn");
    ctx->Yield();
    IM_CHECK_GT(api->city_disasters(api->engine, city).running, 0u);
    b32 merged = false, researched = false, disaster = false;
    for (u32 index = mark; index < api->ledger_count(api->engine); ++index)
    {
        sim_event_kind kind = api->ledger_at(api->engine, index).kind;
        merged     |= kind == SIM_EVENT_FACILITY_MERGED;
        researched |= kind == SIM_EVENT_TECHNIQUE_RESEARCHED;
        disaster   |= kind == SIM_EVENT_DISASTER_STARTED;
    }
    IM_CHECK(merged && researched && disaster);                                       /* the ledger has each step */
    ctx->SleepStandard();

    IM_CHECK(demo_end_video(ctx));

    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Simulation", "simulation"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###City", "city"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###Techniques", "techniques"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Event ledger", "event_ledger"));
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

    t = IM_REGISTER_TEST(e, "demo", "m2_turns");
    t->SetVarsDataType<demo_vars>();
    t->GuiFunc  = demo_gui;
    t->TestFunc = demo_m2_turns;

    t = IM_REGISTER_TEST(e, "demo", "m3_economy");
    t->SetVarsDataType<demo_vars>();
    t->GuiFunc  = demo_gui;
    t->TestFunc = demo_m3_economy;

    t = IM_REGISTER_TEST(e, "demo", "m4_growth");
    t->SetVarsDataType<demo_vars>();
    t->GuiFunc  = demo_gui;
    t->TestFunc = demo_m4_growth;
}
