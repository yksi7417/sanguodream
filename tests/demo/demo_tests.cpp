/* The demo walkthroughs: one test of category "demo" for each milestone.
   demo_player.exe runs them at cinematic speed in a real window, with the
   real engine behind the real panels. Each walkthrough records a video of
   the whole script and a PNG of each panel. It also checks the result of
   each step, so it is an end-to-end smoke test too.

   Rule: one demo test for each milestone. A new milestone adds one
   walkthrough here; it does not add a second test for an old milestone.

   Included by demo_player.cpp (unity build). */

#define DEMO_ENGINE_ARENA_SIZE Megabytes(48)
#define DEMO_M10_NAME          "m10_map"
#define DEMO_M11_NAME          "m11_save_map"
#define DEMO_M12_NAME          "m12_main_map"
#define DEMO_M14_NAME          "m14_game_screens"

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
        config.write_entire_file = test_platform_write_entire_file;
        config.save_folder      = "demo_saves";
        /* The walkthroughs of M0 to M9 play on the placeholder board that
           they were made for; the walkthrough of M10 shows the real map. */
        config.use_pack_map     = strcmp(ctx->Test->Name, DEMO_M10_NAME) == 0 || strcmp(ctx->Test->Name, DEMO_M11_NAME) == 0 ||
                                  strcmp(ctx->Test->Name, DEMO_M12_NAME) == 0 || strcmp(ctx->Test->Name, DEMO_M14_NAME) == 0;
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

/* The first live unit of a force, or ENGINE_NO_UNIT. */
internal u32
demo_unit_of(engine_api *api, u8 force)
{
    for (u32 id = 0; id < api->unit_count(api->engine); ++id)
    {
        engine_unit unit = api->unit_at(api->engine, id);
        if (unit.id != ENGINE_NO_UNIT && unit.force_id == force) return id;
    }
    return ENGINE_NO_UNIT;
}

/* The hex of the range that costs the most: the end of a long march. */
internal hex_coord
demo_far_hex(engine_api *api, u32 unit_id)
{
    engine_reach_view reach = api->reachable(api->engine, unit_id);
    u32 best = 0;
    for (u32 index = 1; index < reach.count; ++index)
    {
        if (reach.costs[index] > reach.costs[best]) best = index;
    }
    return reach.cells[best];
}

/* M5, march: a sortie from the formation dialog, the unit on the Hex map
   with its move range, a move, the command menu (待命), the mini map, and a
   second march in the next turn. */
internal void
demo_m5_march(ImGuiTestContext *ctx)
{
    const char *milestone = "m5_march";
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
    u8 city = demo_busiest_city_of_player(api);
    IM_CHECK(player != ENGINE_NO_FACTION && city != ENGINE_NO_CITY);
    api->set_city_resource(api->engine, city, SIM_RESOURCE_WEAPONS, 0, 10000);   /* pikes for the march */

    IM_CHECK(demo_open_panel(ctx, "###City", &v.ui.show_city));
    ctx->SetRef("###City");
    char item[32];
    snprintf(item, sizeof(item), "###city_select/###c%u", city);
    ctx->ComboClick(item);
    ctx->ItemClick("###open_sortie");
    ctx->Yield();
    ctx->SetRef("###Sortie");
    ctx->ComboClick("###unit_type/###u1");                   /* 槍兵 */
    ctx->ItemClick("###auto");                               /* 自動編制 */
    ctx->SleepStandard();
    ctx->ItemClick("###decide");
    ctx->Yield();
    u32 unit_id = demo_unit_of(api, player);
    IM_CHECK(unit_id != ENGINE_NO_UNIT && !v.ui.show_sortie);
    v.ui.show_city = false;

    v.ui.map.zoom = 8.0f;                                    /* the 40 x 30 map fits the panel */
    IM_CHECK(demo_open_panel(ctx, "Hex map", &v.ui.show_map));
    ctx->WindowFocus("//Hex map");
    engine_unit unit = api->unit_at(api->engine, unit_id);
    demo_click_cell(ctx, v, hex_make(unit.q, unit.r));
    IM_CHECK_EQ(v.ui.units.selected, unit_id);               /* selected: the range shows */
    ctx->SleepStandard();
    hex_coord target = demo_far_hex(api, unit_id);
    ctx->WindowFocus("//Hex map");
    demo_click_cell(ctx, v, target);
    unit = api->unit_at(api->engine, unit_id);
    IM_CHECK(unit.q == target.q && unit.r == target.r && unit.moved);
    IM_CHECK(v.ui.show_unit_menu);
    ctx->SetRef("###UnitMenu");
    ctx->SleepStandard();
    ctx->ItemClick("###wait");                               /* 待命 */
    IM_CHECK(api->unit_at(api->engine, unit_id).acted);

    IM_CHECK(demo_open_panel(ctx, "###MiniMap", &v.ui.show_minimap));
    ctx->SleepStandard();

    ctx->SetRef("Simulation");
    ctx->ItemClick("Advance turn");
    ctx->Yield();
    for (u32 step = 0; step < 10 && api->whose_turn(api->engine) != player; ++step)
    {
        ctx->ItemClick("Advance turn");
        ctx->Yield();
    }
    IM_CHECK_EQ(api->whose_turn(api->engine), player);
    target = demo_far_hex(api, unit_id);
    ctx->WindowFocus("//Hex map");
    demo_click_cell(ctx, v, target);
    IM_CHECK(api->unit_at(api->engine, unit_id).q == target.q);
    ctx->SetRef("###UnitMenu");
    ctx->ItemClick("###wait");
    b32 sortied = false, moved = false, eaten = false;
    for (u32 index = 0; index < api->ledger_count(api->engine); ++index)
    {
        sim_event_kind kind = api->ledger_at(api->engine, index).kind;
        sortied |= kind == SIM_EVENT_UNIT_SORTIED;
        moved   |= kind == SIM_EVENT_UNIT_MOVED;
        eaten   |= kind == SIM_EVENT_UNIT_GRAIN_EATEN;
    }
    IM_CHECK(sortied && moved && eaten);                     /* the ledger has the sortie, the moves and the grain */
    ctx->SleepStandard();

    IM_CHECK(demo_end_video(ctx));

    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Hex map", "hex_map"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###Unit", "unit"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###MiniMap", "minimap"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Event ledger", "event_ledger"));
}

/* A free hex next to the unit that a foot unit can enter, or the unit's hex. */
internal hex_coord
demo_free_neighbor(engine_api *api, engine_unit *unit, hex_coord avoid)
{
    hex_coord center = hex_make(unit->q, unit->r);
    for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d)
    {
        hex_coord hex = hex_neighbor(center, d);
        if (hex_equal(hex, avoid) || api->cell_cost(api->engine, hex.q, hex.r) == 0) continue;
        if (api->unit_at_hex(api->engine, hex.q, hex.r) != ENGINE_NO_UNIT || api->building_at_hex(api->engine, hex.q, hex.r) != ENGINE_NO_BUILDING) continue;
        b32 city = false;
        for (u32 index = 0; index < api->city_count(api->engine); ++index)
        {
            engine_city_slots slots = api->city_slots(api->engine, api->city_at(api->engine, index).city_id);
            city |= slots.q == hex.q && slots.r == hex.r;
        }
        if (!city) return hex;
    }
    return center;
}

/* Clicks "Advance turn" until the faction acts again. */
internal void
demo_next_turn_of(ImGuiTestContext *ctx, engine_api *api, u8 player)
{
    ctx->SetRef("Simulation");
    ctx->ItemClick("Advance turn");
    ctx->Yield();
    for (u32 step = 0; step < 10 && api->whose_turn(api->engine) != player; ++step)
    {
        ctx->ItemClick("Advance turn");
        ctx->Yield();
    }
}

/* Opens the command menu of the selected unit, clicks an item, and for a
   list item clicks the first row of the Actions list. */
internal void
demo_unit_order(ImGuiTestContext *ctx, demo_vars &v, engine_unit *unit, const char *item, b32 list)
{
    ctx->WindowFocus("//Hex map");
    demo_click_cell(ctx, v, hex_make(unit->q, unit->r));      /* select */
    if (!v.ui.show_unit_menu)
    {
        ctx->WindowFocus("//Hex map");
        demo_click_cell(ctx, v, hex_make(unit->q, unit->r));  /* its own hex opens the menu */
    }
    IM_CHECK(v.ui.show_unit_menu);
    ctx->SetRef("###UnitMenu");
    ctx->SleepStandard();
    ctx->ItemClick(item);
    ctx->Yield();
    if (list)
    {
        IM_CHECK(v.ui.show_actions);
        ctx->SetRef("###Actions");
        ctx->SleepStandard();
        ctx->ItemClick("###a0");
        ctx->Yield();
    }
    IM_CHECK(v.ui.combat.pick != UI_PICK_NONE);               /* the map now marks the targets */
    ctx->SleepStandard();
}

/* M6, battle: a sortie, an enemy unit next to it, 攻擊 with the
   counterattack, 火計 that sets a hex on fire, and in the next turns the
   fire, a second attack that destroys the enemy, and the captives. */
internal void
demo_m6_battle(ImGuiTestContext *ctx)
{
    const char *milestone = "m6_battle";
    demo_vars &v = ctx->GetVars<demo_vars>();
    engine_api *api = &v.api;
    void *e = api->engine;
    IM_CHECK(demo_begin_video(ctx, milestone));

    IM_CHECK(demo_open_panel(ctx, "Simulation", &v.ui.show_sim));
    IM_CHECK(demo_open_panel(ctx, "Event ledger", &v.ui.show_ledger));
    ctx->SetRef("Simulation");
    ctx->ItemClick("Load sample save");
    ctx->ItemClick("Load city names");
    ctx->Yield();
    u8 player = api->whose_turn(e);
    u8 city = demo_busiest_city_of_player(api);
    IM_CHECK(player != ENGINE_NO_FACTION && city != ENGINE_NO_CITY);
    u8 enemy = ENGINE_NO_FACTION;
    for (u32 index = 0; index < api->city_count(e) && enemy == ENGINE_NO_FACTION; ++index)
    {
        engine_city other = api->city_at(e, index);
        if (other.owner_force_id != player && other.owner_force_id != ENGINE_NO_FACTION) enemy = other.owner_force_id;
    }
    IM_CHECK(enemy != ENGINE_NO_FACTION);
    api->set_city_resource(e, city, SIM_RESOURCE_WEAPONS, 0, 20000);   /* pikes for two units */
    api->set_city_resource(e, city, SIM_RESOURCE_SOLDIERS, 0, 30000);

    /* 出征 from the formation dialog. */
    IM_CHECK(demo_open_panel(ctx, "###City", &v.ui.show_city));
    ctx->SetRef("###City");
    char item[32];
    snprintf(item, sizeof(item), "###city_select/###c%u", city);
    ctx->ComboClick(item);
    ctx->ItemClick("###open_sortie");
    ctx->Yield();
    ctx->SetRef("###Sortie");
    ctx->ComboClick("###unit_type/###u1");                   /* 槍兵 */
    ctx->ItemClick("###auto");
    ctx->ItemClick("###decide");
    ctx->Yield();
    u32 unit_id = demo_unit_of(api, player);
    IM_CHECK(unit_id != ENGINE_NO_UNIT);
    v.ui.show_city = false;

    /* The scenario editor puts an enemy unit next to it (a second sortie
       that the editor gives to the enemy). */
    engine_sortie_order order = api->sortie_proposal(e, city, 1);
    order.soldiers = 3000;
    order.grain    = 3000;
    IM_CHECK(api->command_sortie(e, &order).refusal == SIM_REFUSAL_NONE);
    u32 foe = ENGINE_NO_UNIT;
    for (u32 id = 0; id < api->unit_count(e); ++id)
    {
        engine_unit other = api->unit_at(e, id);
        if (other.id != ENGINE_NO_UNIT && other.id != unit_id && other.force_id == player) foe = id;
    }
    engine_unit unit = api->unit_at(e, unit_id);
    hex_coord foe_hex = demo_free_neighbor(api, &unit, hex_make(unit.q, unit.r));
    IM_CHECK(foe != ENGINE_NO_UNIT && api->set_unit_place(e, foe, enemy, foe_hex.q, foe_hex.r));
    api->set_unit(e, unit_id, unit.soldiers, unit.morale < 60 ? 60 : unit.morale);

    v.ui.map.zoom = 8.0f;
    IM_CHECK(demo_open_panel(ctx, "Hex map", &v.ui.show_map));
    IM_CHECK(demo_open_panel(ctx, "###Unit", &v.ui.show_unit));

    /* 攻擊: the menu, then the enemy hex. */
    u32 before = api->unit_at(e, foe).soldiers;
    demo_unit_order(ctx, v, &unit, "###attack", false);
    ctx->WindowFocus("//Hex map");
    demo_click_cell(ctx, v, foe_hex);
    IM_CHECK(api->unit_at(e, foe).soldiers < before && api->unit_at(e, unit_id).acted);
    ctx->SleepStandard();

    /* Next turn: 火計 on the hex beside the enemy. */
    demo_next_turn_of(ctx, api, player);
    IM_CHECK_EQ(api->whose_turn(e), player);
    unit = api->unit_at(e, unit_id);
    hex_coord fire_hex = demo_free_neighbor(api, &unit, foe_hex);
    demo_unit_order(ctx, v, &unit, "###plot", true);
    ctx->WindowFocus("//Hex map");
    demo_click_cell(ctx, v, fire_hex);
    engine_fire_view fire = api->fire_view(e);
    hex_offset o = hex_to_offset(fire_hex);
    IM_CHECK(fire.turns[(u32)o.row * fire.width + (u32)o.col] > 0);   /* the hex burns */
    ctx->SleepStandard();

    /* Next turn: the enemy is weak; a second attack destroys it. */
    demo_next_turn_of(ctx, api, player);
    IM_CHECK_EQ(api->whose_turn(e), player);
    api->set_unit(e, foe, 20, 30);
    unit = api->unit_at(e, unit_id);
    demo_unit_order(ctx, v, &unit, "###attack", false);
    ctx->WindowFocus("//Hex map");
    demo_click_cell(ctx, v, foe_hex);
    IM_CHECK(api->unit_at(e, foe).id == ENGINE_NO_UNIT);
    b32 damaged = false, countered = false, fire_started = false, destroyed = false, fate = false;
    for (u32 index = 0; index < api->ledger_count(e); ++index)
    {
        sim_event_kind kind = api->ledger_at(e, index).kind;
        damaged      |= kind == SIM_EVENT_UNIT_DAMAGED;
        countered    |= kind == SIM_EVENT_COUNTERATTACK;
        fire_started |= kind == SIM_EVENT_FIRE_STARTED;
        destroyed    |= kind == SIM_EVENT_UNIT_DESTROYED;
        fate         |= kind == SIM_EVENT_OFFICER_CAPTURED || kind == SIM_EVENT_OFFICER_ESCAPED;
    }
    IM_CHECK(damaged && countered && fire_started && destroyed && fate);   /* the ledger has each step */
    ctx->SleepStandard();

    /* The Actions list of the unit for the screenshot. */
    v.ui.combat.list  = UI_PICK_TACTIC;
    v.ui.show_actions = true;
    ctx->Yield(2);

    IM_CHECK(demo_end_video(ctx));

    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Hex map", "hex_map"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###Unit", "unit"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###Actions", "actions"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Event ledger", "event_ledger"));
}

/* An active officer of the faction in the city that is not the ruler and
   not one of the excluded ids. */
internal u16
demo_court_officer(engine_api *api, u8 force, u8 city, u16 exclude)
{
    for (u32 index = 0; index < api->officer_count(api->engine); ++index)
    {
        engine_officer officer = api->officer_at(api->engine, index);
        if (officer.force_id == force && officer.status == 3 && officer.current_position == city && officer.officer_id != exclude) return officer.officer_id;
    }
    return ENGINE_NO_OFFICER;
}

/* M7, court: the Court panel (褒賞, 官職), the Diplomacy panel (親善), a
   duel from the unit menu on its screen, and a debate on its screen. */
internal void
demo_m7_court(ImGuiTestContext *ctx)
{
    const char *milestone = "m7_court";
    demo_vars &v = ctx->GetVars<demo_vars>();
    engine_api *api = &v.api;
    void *e = api->engine;
    IM_CHECK(demo_begin_video(ctx, milestone));

    IM_CHECK(demo_open_panel(ctx, "Simulation", &v.ui.show_sim));
    IM_CHECK(demo_open_panel(ctx, "Event ledger", &v.ui.show_ledger));
    ctx->SetRef("Simulation");
    ctx->ItemClick("Load sample save");
    ctx->ItemClick("Load city names");
    ctx->Yield();
    u8 player = api->whose_turn(e);
    u8 city = demo_busiest_city_of_player(api);
    IM_CHECK(player != ENGINE_NO_FACTION && city != ENGINE_NO_CITY);
    v.ui.city.selected = city;
    api->set_city_gold(e, city, 30000);

    /* The Court panel: an officer, 褒賞, then 官職 (the editor gives the merit). */
    u16 officer = demo_court_officer(api, player, city, ENGINE_NO_OFFICER);
    IM_CHECK(officer != ENGINE_NO_OFFICER);
    IM_CHECK(demo_open_panel(ctx, "###Court", &v.ui.show_court));
    ctx->SetRef("###Court");
    char item[64];
    snprintf(item, sizeof(item), "###court_officer/###o%u", officer);
    ctx->ComboClick(item);
    ctx->SleepStandard();
    u32 loyalty = api->officer_detail(e, officer).loyalty;
    ctx->ItemClick("###reward");
    IM_CHECK(api->officer_detail(e, officer).loyalty > loyalty);
    engine_officer_detail detail = api->officer_detail(e, officer);
    engine_officer record = api->officer_by_id(e, officer);
    engine_officer_edit edit = {};
    edit.loyalty = detail.loyalty; edit.ambition = detail.ambition; edit.justice = detail.justice; edit.personality = detail.personality;
    edit.growth = detail.growth; edit.female = detail.female; edit.merit = 4000; edit.captor = ENGINE_NO_FACTION;
    (void)record;
    api->set_officer(e, officer, &edit);
    ctx->ComboClick("###post/###p64");                       /* 主簿 */
    ctx->ItemClick("###appoint");
    IM_CHECK(api->officer_detail(e, officer).post == 64 && api->officer_detail(e, officer).soldier_cap == 6000);
    ctx->SleepStandard();

    /* The Diplomacy panel: 親善 with another faction. */
    u8 other = ENGINE_NO_FACTION;
    for (u32 index = 0; index < api->faction_count(e) && other == ENGINE_NO_FACTION; ++index)
    {
        engine_faction faction = api->faction_at(e, index);
        if (faction.force_id != player && api->force_ruler(e, faction.force_id) != ENGINE_NO_OFFICER) other = faction.force_id;
    }
    IM_CHECK(other != ENGINE_NO_FACTION);
    IM_CHECK(demo_open_panel(ctx, "###Diplomacy", &v.ui.show_diplomacy));
    ctx->SetRef("###Diplomacy");
    snprintf(item, sizeof(item), "factions/###f%u", other);
    ctx->ItemClick(item);
    u16 envoy = demo_court_officer(api, player, city, officer);
    snprintf(item, sizeof(item), "###envoy/###o%u", envoy);
    ctx->ComboClick(item);
    u32 friendliness = api->diplomacy(e, player, other).friendliness;
    ctx->ItemClick("###goodwill");
    IM_CHECK(api->diplomacy(e, player, other).friendliness > friendliness);
    ctx->SleepStandard();
    v.ui.show_court = v.ui.show_diplomacy = false;

    /* A duel: a sortie, an enemy unit next to it (the editor), 單挑 from the
       unit menu, then the duel screen. */
    api->set_city_resource(e, city, SIM_RESOURCE_WEAPONS, 0, 20000);
    api->set_city_resource(e, city, SIM_RESOURCE_SOLDIERS, 0, 30000);
    engine_sortie_order order = api->sortie_proposal(e, city, 1);
    order.soldiers = 3000; order.grain = 3000;
    IM_CHECK(api->command_sortie(e, &order).refusal == SIM_REFUSAL_NONE);
    u32 unit_id = demo_unit_of(api, player);
    order = api->sortie_proposal(e, city, 1);
    order.soldiers = 3000; order.grain = 3000;
    IM_CHECK(api->command_sortie(e, &order).refusal == SIM_REFUSAL_NONE);
    u32 foe = ENGINE_NO_UNIT;
    for (u32 id = 0; id < api->unit_count(e); ++id)
    {
        engine_unit other_unit = api->unit_at(e, id);
        if (other_unit.id != ENGINE_NO_UNIT && other_unit.id != unit_id && other_unit.force_id == player) foe = id;
    }
    engine_unit unit = api->unit_at(e, unit_id);
    hex_coord foe_hex = demo_free_neighbor(api, &unit, hex_make(unit.q, unit.r));
    IM_CHECK(foe != ENGINE_NO_UNIT && api->set_unit_place(e, foe, other, foe_hex.q, foe_hex.r));
    u16 foe_commander = api->unit_at(e, foe).officers[0];
    engine_officer_detail foe_detail = api->officer_detail(e, foe_commander);
    edit = {};
    edit.loyalty = foe_detail.loyalty; edit.ambition = foe_detail.ambition; edit.justice = foe_detail.justice;
    edit.personality = PERSONALITY_RECKLESS; edit.growth = foe_detail.growth; edit.female = foe_detail.female;
    edit.merit = api->officer_by_id(e, foe_commander).merit; edit.captor = ENGINE_NO_FACTION;
    api->set_officer(e, foe_commander, &edit);
    v.ui.map.zoom = 8.0f;
    IM_CHECK(demo_open_panel(ctx, "Hex map", &v.ui.show_map));
    for (u32 attempt = 0; attempt < 6 && !api->duel_view(e).active; ++attempt)
    {
        if (attempt) demo_next_turn_of(ctx, api, player);
        unit = api->unit_at(e, unit_id);
        demo_unit_order(ctx, v, &unit, "###duel", false);
        ctx->WindowFocus("//Hex map");
        demo_click_cell(ctx, v, foe_hex);
        ctx->Yield(2);
    }
    IM_CHECK(api->duel_view(e).active && v.ui.show_duel);
    ctx->SetRef("###Duel");
    ctx->ItemClick("###policy2");                            /* 重視鬥志 */
    ctx->ItemClick("###duel_round");
    ctx->ItemClick("###duel_round");
    IM_CHECK(api->duel_view(e).round >= 1 || !api->duel_view(e).active);
    ctx->SleepStandard();
    ctx->ItemClick("###duel_auto");
    IM_CHECK(!api->duel_view(e).active && api->duel_view(e).result != DUEL_RESULT_NONE);
    ctx->SleepStandard();

    /* A debate of two officers (the editor starts it), on its screen. */
    u16 a = demo_court_officer(api, player, city, ENGINE_NO_OFFICER);
    u16 b = demo_court_officer(api, player, city, a);
    IM_CHECK(api->start_debate(e, a, b).refusal == SIM_REFUSAL_NONE);
    ctx->Yield(2);
    IM_CHECK(v.ui.show_debate);
    ctx->SetRef("###Debate");
    if (api->debate_view(e).active && !api->debate_view(e).finished)
    {
        if (api->debate_view(e).turn == 0) ctx->ItemClick("###card0");
        else                               ctx->ItemClick("###debate_round");
    }
    ctx->SleepStandard();
    if (api->debate_view(e).active && !api->debate_view(e).finished) ctx->ItemClick("###debate_auto");
    IM_CHECK(api->debate_view(e).finished);
    ctx->SleepStandard();
    if (api->debate_view(e).winner == 0) ctx->ItemClick("###sympathy");
    else                                 ctx->ItemClick("###debate_close");
    IM_CHECK(!api->debate_view(e).active);
    b32 rewarded = false, appointed = false, goodwill = false, duel = false, debate = false;
    for (u32 index = 0; index < api->ledger_count(e); ++index)
    {
        sim_event_kind kind = api->ledger_at(e, index).kind;
        rewarded  |= kind == SIM_EVENT_LOYALTY_CHANGED;
        appointed |= kind == SIM_EVENT_POST_APPOINTED;
        goodwill  |= kind == SIM_EVENT_FRIENDLINESS_CHANGED;
        duel      |= kind == SIM_EVENT_DUEL_ENDED;
        debate    |= kind == SIM_EVENT_DEBATE_ENDED;
    }
    IM_CHECK(rewarded && appointed && goodwill && duel && debate);   /* the ledger has each step */
    v.ui.show_court = v.ui.show_diplomacy = true;
    ctx->Yield(2);

    IM_CHECK(demo_end_video(ctx));

    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###Court", "court"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###Diplomacy", "diplomacy"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###Duel", "duel"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###Debate", "debate"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Event ledger", "event_ledger"));
}

/* The faction with the most cities whose ruler stands in one of them: the
   council meets where the ruler stands, and a corps takes another city. */
internal u8
demo_rival_faction(engine_api *api)
{
    void *e = api->engine;
    u8 best = ENGINE_NO_FACTION;
    u32 best_cities = 0;
    for (u32 index = 0; index < api->faction_count(e); ++index)
    {
        u8 force = api->faction_at(e, index).force_id;
        engine_officer ruler = api->officer_by_id(e, api->force_ruler(e, force));
        if (ruler.officer_id == ENGINE_NO_OFFICER || ruler.current_position >= SAVE_CITY_COUNT) continue;
        if (api->city_at(e, ruler.current_position).owner_force_id != force) continue;
        u32 cities = 0;
        for (u32 city = 0; city < api->city_count(e); ++city) cities += api->city_at(e, city).owner_force_id == force;
        if (cities > best_cities) { best = force; best_cities = cities; }
    }
    return best;
}

/* An active officer of the faction that is not the ruler, with intellect at
   least min_intellect, and not the excluded one. */
internal u16
demo_rival_officer(engine_api *api, u8 force, u32 min_intellect, u16 exclude)
{
    for (u32 index = 0; index < api->officer_count(api->engine); ++index)
    {
        engine_officer officer = api->officer_at(api->engine, index);
        if (officer.force_id != force || officer.status == 0 || officer.status > 3 || officer.current_position >= SAVE_CITY_COUNT) continue;
        if (officer.intelligence >= min_intellect && officer.officer_id != exclude) return officer.officer_id;
    }
    return ENGINE_NO_OFFICER;
}

/* M8, rivals: the ports and passes on the Bases panel and the mini map, a
   new corps, a 軍師 and his advice, a council proposal, the treason event
   and its dialog, and a turn in which the computer factions and the
   delegated corps act. */
internal void
demo_m8_rivals(ImGuiTestContext *ctx)
{
    const char *milestone = "m8_rivals";
    demo_vars &v = ctx->GetVars<demo_vars>();
    engine_api *api = &v.api;
    void *e = api->engine;
    IM_CHECK(demo_begin_video(ctx, milestone));

    IM_CHECK(demo_open_panel(ctx, "Simulation", &v.ui.show_sim));
    IM_CHECK(demo_open_panel(ctx, "Event ledger", &v.ui.show_ledger));
    ctx->SetRef("Simulation");
    ctx->ItemClick("Load sample save");
    ctx->ItemClick("Load city names");
    ctx->Yield();
    engine_settings settings = api->settings(e);
    settings.events = false;                                 /* the demo fires its event itself */
    api->set_settings(e, settings);

    /* Play the faction with the most cities; the other factions are the AI. */
    u8 player = demo_rival_faction(api);
    IM_CHECK(player != ENGINE_NO_FACTION);
    api->set_faction_human(e, player, true);
    for (u32 step = 0; step < 300 && api->whose_turn(e) != player; ++step) api->end_turn(e);
    IM_CHECK(api->whose_turn(e) == player);
    engine_officer ruler = api->officer_by_id(e, api->force_ruler(e, player));
    u8 ruler_city = (u8)ruler.current_position;
    u8 corps_city = ENGINE_NO_CITY;
    for (u32 city = 0; city < api->city_count(e) && corps_city == ENGINE_NO_CITY; ++city)
    {
        if (city != ruler_city && api->city_at(e, city).owner_force_id == player) corps_city = (u8)city;
    }
    IM_CHECK(corps_city != ENGINE_NO_CITY);
    for (u32 city = 0; city < api->city_count(e); ++city) if (api->city_at(e, city).owner_force_id == player) api->set_city_gold(e, (u8)city, 30000);
    api->set_actions(e, player, 255);
    v.ui.city.selected = ruler_city;

    /* 港關: the 45 bases in their table and on the mini map. */
    IM_CHECK(api->base_count(e) == 45);
    IM_CHECK(demo_open_panel(ctx, "###Bases", &v.ui.show_bases));
    IM_CHECK(demo_scroll_table(ctx, "###Bases", "bases_table"));
    IM_CHECK(demo_open_panel(ctx, "###MiniMap", &v.ui.show_minimap));
    ctx->SleepStandard();
    v.ui.show_bases = false;

    /* 軍團: a 都督 and a city, then Create. */
    IM_CHECK(demo_open_panel(ctx, "###Ruler", &v.ui.show_ruler));
    ctx->SetRef("###Ruler");
    u16 viceroy = demo_rival_officer(api, player, 0, ENGINE_NO_OFFICER);
    char item[64];
    snprintf(item, sizeof(item), "###viceroy/###o%u", viceroy);
    ctx->ComboClick(item);
    v.ui.ruler.cities = 1ull << corps_city;
    ctx->SleepStandard();
    ctx->ItemClick("###corps_create");
    IM_CHECK(api->corps_at(e, player, 1).alive && api->city_corps(e, corps_city) == 1);
    ctx->SleepStandard();

    /* 軍師: an officer with intellect 70 or more; the advice appears. */
    u16 adviser = demo_rival_officer(api, player, 70, viceroy);
    IM_CHECK(adviser != ENGINE_NO_OFFICER);
    snprintf(item, sizeof(item), "###adviser_pick/###o%u", adviser);
    ctx->ComboClick(item);
    ctx->ItemClick("###appoint_adviser");
    IM_CHECK(api->adviser(e, player) == adviser && api->advice(e, player).count > 0);
    ctx->SleepStandard();

    /* 評定: low public order in the city of the ruler makes 巡察 the first proposal. */
    api->set_city_resource(e, ruler_city, SIM_RESOURCE_PUBLIC_ORDER, 0, 30);
    ctx->Yield(2);
    engine_plan_view council = api->council(e, player, ruler_city);
    IM_CHECK(council.refusal == SIM_REFUSAL_NONE && council.count > 0 && council.items[0].kind == SIM_COMMAND_PATROL);
    u32 mark = api->ledger_count(e);
    ctx->ItemClick("###council0");
    b32 patrolled = false;
    for (u32 index = mark; index < api->ledger_count(e); ++index) patrolled |= api->ledger_at(e, index).kind == SIM_EVENT_PATROLLED;
    IM_CHECK(patrolled);
    ctx->SleepStandard();

    /* 謀反的嫌疑: the editor fires the event; the dialog asks the ruler. */
    engine_officer_detail detail = api->officer_detail(e, viceroy);
    engine_officer_edit edit = {};
    edit.loyalty = 50; edit.ambition = detail.ambition; edit.justice = detail.justice; edit.personality = detail.personality;
    edit.growth = detail.growth; edit.female = detail.female; edit.merit = api->officer_by_id(e, viceroy).merit; edit.captor = ENGINE_NO_FACTION;
    api->set_officer(e, viceroy, &edit);
    IM_CHECK(api->trigger_event(e, 0, player, ENGINE_NO_CITY, ENGINE_NO_OFFICER).refusal == SIM_REFUSAL_NONE);
    ctx->Yield(2);
    IM_CHECK(api->pending_event(e).active);
    ctx->SetRef("###Event");
    ctx->SleepStandard();
    ctx->ItemClick("###choice0");                            /* a slander: the corps stays */
    IM_CHECK(!api->pending_event(e).active && api->corps_at(e, player, 1).alive);
    ctx->SleepStandard();

    /* The next turn: the computer factions plan and act; the corps acts with its own points. */
    settings = api->settings(e);
    settings.ai = true;
    api->set_settings(e, settings);
    mark = api->ledger_count(e);
    demo_next_turn_of(ctx, api, player);
    IM_CHECK(api->whose_turn(e) == player);
    u32 planned = 0;
    for (u32 index = mark; index < api->ledger_count(e); ++index)
    {
        sim_event event = api->ledger_at(e, index);
        planned += event.kind == SIM_EVENT_AI_PLANNED && event.delta > 0;
    }
    IM_CHECK(planned > 0);                                   /* the ledger has the turns of the AI */
    v.ui.show_bases = true;
    ctx->Yield(2);
    ctx->SleepStandard();

    IM_CHECK(demo_end_video(ctx));

    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###Ruler", "ruler"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###Bases", "bases"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###MiniMap", "minimap"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Event ledger", "event_ledger"));
    /* The event dialog: the editor fires the event again for its picture. */
    IM_CHECK(api->trigger_event(e, 0, player, ENGINE_NO_CITY, ENGINE_NO_OFFICER).refusal == SIM_REFUSAL_NONE);
    ctx->Yield(2);
    IM_CHECK(api->pending_event(e).active);
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###Event", "event"));
    ctx->SetRef("###Event");
    ctx->ItemClick("###choice0");
    IM_CHECK(!api->pending_event(e).active);
}

/* M9, playable: the title menu, a new game from the scenario 群雄割據 with
   a chosen ruler and the settings of the start, a new officer, a research
   of the ability chart, turns with 進行, a save and a load, and the
   battle list and the information screen. */
internal void
demo_m9_playable(ImGuiTestContext *ctx)
{
    const char *milestone = "m9_playable";
    demo_vars &v = ctx->GetVars<demo_vars>();
    engine_api *api = &v.api;
    void *e = api->engine;
    IM_CHECK(demo_begin_video(ctx, milestone));

    /* 標題 → 重新開始新遊戲 → 群雄割據. */
    v.ui.show_title = true;
    ctx->Yield(2);
    ctx->SetRef("###Title");
    ctx->SleepStandard();
    ctx->ItemClick("###title_new");
    IM_CHECK(v.ui.show_scenarios);
    ctx->SetRef("###Scenarios");
    ctx->ItemClick("###sc1");
    IM_CHECK(v.ui.show_rulers && api->city_count(e) == 42);

    /* 君主: the faction with the most cities; then the settings of the start. */
    u8 player = demo_rival_faction(api);
    IM_CHECK(player != ENGINE_NO_FACTION);
    ctx->SetRef("###Rulers");
    char item[64];
    snprintf(item, sizeof(item), "###ruler%u", player);
    ctx->ItemClick(item);
    ctx->ItemClick("###rulers_ok");
    IM_CHECK(v.ui.show_start_settings);
    ctx->SetRef("###StartSettings");
    ctx->ItemClick("###duel2");                              /* 單挑・舌戰: automatic */
    ctx->ItemClick("###level1");
    ctx->SleepStandard();
    ctx->ItemClick("###start_ok");
    IM_CHECK(v.ui.show_topbar && api->whose_turn(e) == player && api->settings(e).duel_mode == DUEL_MODE_AUTO);
    engine_officer ruler = api->officer_by_id(e, api->force_ruler(e, player));
    u8 capital = (u8)ruler.current_position;
    v.ui.city.selected = capital;
    for (u32 city = 0; city < api->city_count(e); ++city) if (api->city_at(e, city).owner_force_id == player) api->set_city_gold(e, (u8)city, 30000);
    ctx->SleepStandard();

    /* 項目 → 新武將: a strategist in the capital. */
    ctx->SetRef("###TopBar");
    ctx->ItemClick("###open_system");
    ctx->SetRef("###Options");
    ctx->ItemClick("###open_new_officer");
    v.ui.game.city = capital;
    ctx->SetRef("###NewOfficer");
    ctx->ItemInputValue("###family", "\xE5\xBC\xB5");        /* 張 */
    ctx->ItemInputValue("###given", "\xE4\xB8\x80");         /* 一 */
    ctx->ItemClick("###type1");
    ctx->ItemClick("###create_officer");
    IM_CHECK(api->new_officer_count(e) == 1);
    ctx->SleepStandard();
    v.ui.show_new_officer = v.ui.show_options = false;

    /* 能力表: research 統率+5 in the capital. */
    ctx->SetRef("###TopBar");
    ctx->ItemClick("###open_abilities");
    ctx->SetRef("###Abilities");
    ctx->ItemClick("chart_table/###research0");
    IM_CHECK(api->faction_chart(e, player).job_row == 0);
    ctx->SleepStandard();
    v.ui.show_abilities = false;

    /* 進行: three turns; the computer factions act between them. */
    u32 turn = api->turn(e);
    for (u32 step = 0; step < 3; ++step)
    {
        ctx->SetRef("###TopBar");
        ctx->ItemClick("###proceed");
        for (u32 guard = 0; guard < 20 && api->whose_turn(e) != player; ++guard) { ctx->ItemClick("###proceed"); }
    }
    IM_CHECK(api->turn(e) >= turn + 3 && api->whose_turn(e) == player);
    ctx->SleepStandard();

    /* 儲存 in slot 1 with a comment, then 載入 of the same slot. */
    ctx->SetRef("###TopBar");
    ctx->ItemClick("###open_save");
    ctx->SetRef("###SaveLoad");
    ctx->ItemClick("###slot0");
    ctx->ItemInputValue("###comment", "M9 demo");
    ctx->ItemClick("###save_ok");
    IM_CHECK(api->save_slot(e, 0).used && strcmp(api->save_slot(e, 0).comment, "M9 demo") == 0);
    u32 saved_turn = api->turn(e);
    ctx->SetRef("###TopBar");
    ctx->ItemClick("###proceed");
    ctx->ItemClick("###open_load");
    ctx->SetRef("###SaveLoad");
    ctx->ItemClick("###slot0");
    IM_CHECK(api->turn(e) == saved_turn && !v.ui.show_saveload);
    ctx->SleepStandard();

    /* 情報 and 決戰稱霸. */
    ctx->SetRef("###TopBar");
    ctx->ItemClick("###open_info");
    IM_CHECK(v.ui.show_info);
    v.ui.show_battles = true;
    ctx->Yield(2);
    IM_CHECK(api->battle_count(e) == 5);
    ctx->SleepStandard();

    IM_CHECK(demo_end_video(ctx));

    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###TopBar", "topbar"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###History", "history"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###Info", "info"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###Battles", "battles"));
    v.ui.show_abilities = v.ui.show_new_officer = true;
    ui_open_saveload(&v.ui, api, true);
    v.ui.show_title = v.ui.show_scenarios = v.ui.show_start_settings = true;
    v.ui.game.start_loaded = false;
    ctx->Yield(3);
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###Abilities", "abilities"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###NewOfficer", "new_officer"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###SaveLoad", "save"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###Title", "title"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###Scenarios", "scenarios"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###StartSettings", "start_settings"));
}

/* Scrolls the canvas of the Hex map panel so that the hex is in the middle. */
internal void
demo_scroll_map_to(ImGuiTestContext *ctx, demo_vars &v, hex_coord hex)
{
    ImGuiWindow *canvas = ctx->WindowInfo("//Hex map/map_canvas").Window;
    IM_CHECK(canvas != 0);
    f32 zoom = v.ui.map.zoom;
    hex_point origin;
    origin.x = zoom;
    origin.y = zoom;
    hex_point center = hex_to_pixel(hex, zoom, origin);
    ctx->ScrollToX(canvas->ID, Maximum(0.0f, center.x - canvas->Size.x * 0.5f));
    ctx->ScrollToY(canvas->ID, Maximum(0.0f, center.y - canvas->Size.y * 0.5f));
    ctx->Yield(2);
}

/* M10, the real map: the continent of 200 x 200 hexes in the scene and
   in the Hex map panel; a click on 洛陽 frames its 22 slots; the data of a
   gate hex; the gate sides are cliff; a burning hex. */
internal void
demo_m10_map(ImGuiTestContext *ctx)
{
    const char *milestone = DEMO_M10_NAME;
    demo_vars &v = ctx->GetVars<demo_vars>();
    engine_api *api = &v.api;
    void *e = api->engine;
    engine_map_info info = api->map_info(e);
    IM_CHECK(info.real_map && info.width == 200 && info.height == 200 && info.city_hexes == 294 && info.slots == 591);
    IM_CHECK(demo_begin_video(ctx, milestone));

    /* The continent in the scene. */
    IM_CHECK(demo_open_panel(ctx, "Scene", &v.ui.show_scene));
    ctx->SetRef("Scene");
    ctx->ItemClick("Tilt up");
    ctx->ItemClick("Orbit right");
    ctx->SleepStandard();
    ctx->ItemClick("Reset view");
    ctx->SleepStandard();

    /* The sample save puts the 42 cities on their flowers. */
    IM_CHECK(demo_open_panel(ctx, "Simulation", &v.ui.show_sim));
    ctx->SetRef("Simulation");
    ctx->ItemClick("Load sample save");
    ctx->Yield();
    IM_CHECK_EQ(api->city_count(e), 42u);

    /* The Hex map: 洛陽 and its slots. */
    IM_CHECK(demo_open_panel(ctx, "Hex map", &v.ui.show_map));
    ctx->WindowResize("//Hex map", ImVec2(720, 560));
    v.ui.map.zoom = 10.0f;
    ctx->Yield();
    engine_city_slots luoyang = api->city_slots(e, 15);
    hex_coord center = hex_make(luoyang.q, luoyang.r);
    demo_scroll_map_to(ctx, v, center);
    ctx->SleepStandard();
    demo_click_cell(ctx, v, center);
    IM_CHECK(v.ui.map.has_city && v.ui.map.city == 15);
    IM_CHECK_EQ(luoyang.slot_count, 22);
    ctx->SleepStandard();
    ctx->SleepStandard();

    /* The data of the gate 虎牢關 (43): the gate hex stays, its sides are cliff. */
    engine_base hulao = api->base_at(e, 43 - CONTENT_FIRST_BASE);
    hex_coord gate = hex_make(hulao.q, hulao.r);
    ctx->SetRef("Hex map");
    ctx->ItemClick("Clear selection");
    demo_scroll_map_to(ctx, v, gate);
    demo_click_cell(ctx, v, gate);
    IM_CHECK(v.ui.map.has_from && hex_equal(v.ui.map.from, gate));
    engine_hex gate_hex = api->hex_at(e, gate.q, gate.r);
    IM_CHECK(gate_hex.base_id == 43 && gate_hex.terrain == 18);
    u32 cliffs = 0;
    for (u32 direction = 0; direction < HEX_DIRECTION_COUNT; ++direction)
    {
        hex_coord side = hex_neighbor(gate, direction);
        engine_hex next = api->hex_at(e, side.q, side.r);
        if (next.region == 43 && next.terrain == 15) cliffs++;
    }
    IM_CHECK_GE(cliffs, 2u);
    ctx->SleepStandard();

    /* A fire on grass beside 洛陽: 4 times the cost of the table. */
    for (u32 slot = 0; slot < luoyang.slot_count; ++slot)
    {
        engine_facility facility = api->facility_at(e, 15, (u8)slot);
        if (api->hex_at(e, facility.q, facility.r).terrain != 1) continue;
        u32 before = api->move_cost(e, 1, api->city_at(e, 15).owner_force_id, facility.q, facility.r);
        api->start_fire(e, facility.q, facility.r, 5);
        IM_CHECK_EQ(api->move_cost(e, 1, api->city_at(e, 15).owner_force_id, facility.q, facility.r), before * 4);
        demo_scroll_map_to(ctx, v, hex_make(facility.q, facility.r));
        demo_click_cell(ctx, v, hex_make(facility.q, facility.r));
        break;
    }
    ctx->SleepStandard();
    demo_scroll_map_to(ctx, v, center);
    ctx->SleepStandard();

    IM_CHECK(demo_end_video(ctx));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Hex map", "hex_map"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Scene", "scene"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "Simulation", "simulation"));
}

/* M12: the signed angle from one heading to the next, in one turn. */
internal f32
demo_turn_between(f32 from, f32 to)
{
    f32 turn = to - from;
    while (turn >  RENDER_PI) turn -= 2.0f * RENDER_PI;
    while (turn < -RENDER_PI) turn += 2.0f * RENDER_PI;
    return turn;
}

/* True if the camera looks at the hex (the target is over its centre). */
internal bool
demo_camera_on(demo_vars &v, hex_coord hex)
{
    v3 center = hex_world_center(hex, 0.0f);
    return fabsf(v.ui.scene.cam.target.x - center.x) < 0.01f && fabsf(v.ui.scene.cam.target.z - center.z) < 0.01f;
}

/* M11, the sample save on the real map: a row of the Cities panel frames
   柴桑 and the City panel counts its 12 facilities; a row of the Units
   panel frames a unit of the save; W, Q and Up move, turn and tilt the
   view; the Hex info box tells what the mint hex of 柴桑 is. */
internal void
demo_m11_save_map(ImGuiTestContext *ctx)
{
    const char *milestone = DEMO_M11_NAME;
    demo_vars &v = ctx->GetVars<demo_vars>();
    engine_api *api = &v.api;
    void *e = api->engine;
    IM_CHECK(api->map_info(e).real_map);
    IM_CHECK(demo_begin_video(ctx, milestone));

    IM_CHECK(demo_open_panel(ctx, "Scene", &v.ui.show_scene));
    IM_CHECK(demo_open_panel(ctx, "Simulation", &v.ui.show_sim));
    ctx->SetRef("Simulation");
    ctx->ItemClick("Load sample save");
    ctx->Yield();
    IM_CHECK_EQ(api->city_count(e), 42u);
    ctx->SleepStandard();

    /* 柴桑 from the city list. */
    IM_CHECK(demo_open_panel(ctx, "###Cities", &v.ui.show_cities));
    ctx->SetRef("###Cities");
    ctx->ItemClick("**/26##city_row");
    ctx->Yield(2);
    engine_city_slots chaisang = api->city_slots(e, 26);
    IM_CHECK_EQ(v.ui.city.selected, 26);
    IM_CHECK(demo_camera_on(v, hex_make(chaisang.q, chaisang.r)));
    IM_CHECK(chaisang.used == 12 && chaisang.slot_count == 12);
    IM_CHECK(demo_open_panel(ctx, "###City", &v.ui.show_city));
    ctx->SleepStandard();
    ctx->SleepStandard();

    /* A unit of the save from the Units panel. */
    u32 unit_id = ENGINE_NO_UNIT;
    for (u32 id = 0; id < api->unit_count(e) && unit_id == ENGINE_NO_UNIT; ++id) if (api->unit_at(e, id).id != ENGINE_NO_UNIT) unit_id = id;
    IM_CHECK(unit_id != ENGINE_NO_UNIT);
    IM_CHECK(demo_open_panel(ctx, "###Units", &v.ui.show_units));
    char row[32];
    snprintf(row, sizeof(row), "**/###unit%u", unit_id);
    ctx->SetRef("###Units");
    ctx->ItemClick(row);
    ctx->Yield(2);
    engine_unit unit = api->unit_at(e, unit_id);
    IM_CHECK_EQ(v.ui.units.selected, unit_id);
    IM_CHECK(demo_camera_on(v, hex_make(unit.q, unit.r)));
    ctx->SleepStandard();

    /* The keys: W up the screen, Q clockwise (ui/main-map.md), Up toward top down. */
    ImGui::FocusWindow(0);
    ctx->Yield();
    camera before = v.ui.scene.cam;
    v3 ahead, side;
    camera_ground_axes(&before, &ahead, &side);
    ctx->KeyDown(ImGuiKey_W); ctx->SleepStandard(); ctx->KeyUp(ImGuiKey_W);
    IM_CHECK_GT(v3_dot(v3_sub(v.ui.scene.cam.target, before.target), ahead), 0.0f);
    f32 yaw = v.ui.scene.cam.yaw, pitch = v.ui.scene.cam.pitch;
    ctx->KeyDown(ImGuiKey_Q); ctx->SleepStandard(); ctx->KeyUp(ImGuiKey_Q);
    IM_CHECK_LT(demo_turn_between(yaw, v.ui.scene.cam.yaw), 0.0f);   /* clockwise on the screen */
    ctx->KeyDown(ImGuiKey_UpArrow); ctx->SleepStandard(); ctx->KeyUp(ImGuiKey_UpArrow);
    IM_CHECK_GT(v.ui.scene.cam.pitch, pitch);
    ctx->SleepStandard();

    /* Back to 柴桑, and the Hex info box of its mint. */
    ctx->SetRef("###Cities");
    ctx->ItemClick("**/26##city_row");
    ctx->Yield(2);
    hex_coord mint = {};
    for (u32 slot = 0; slot < chaisang.slot_count; ++slot)
    {
        engine_facility facility = api->facility_at(e, 26, (u8)slot);
        if (strcmp(api->facility_kind_at(e, facility.facility).key, "mint") == 0) mint = hex_make(facility.q, facility.r);
    }
    v.ui.scene.pick     = mint;
    v.ui.scene.has_pick = true;
    ctx->Yield(2);
    IM_CHECK(ctx->WindowInfo("//###HexInfo").Window != 0);
    engine_hex data = api->hex_at(e, mint.q, mint.r);
    IM_CHECK(data.slot_city == 26 && data.facility != ENGINE_NO_FACILITY);
    ctx->SleepStandard();
    ctx->SleepStandard();

    IM_CHECK(demo_end_video(ctx));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###City", "city"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###Units", "units"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###HexInfo", "hex_info"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###Cities", "cities"));
}

/* M12, the main map of the original: the round mini map with the faction
   squares, the roads and the compass ring; a click on it moves the camera;
   the unit plates; Q, E and the arrows turn the view by steps of 60
   degrees, and a held key repeats the step. */
internal void
demo_m12_main_map(ImGuiTestContext *ctx)
{
    const char *milestone = DEMO_M12_NAME;
    demo_vars &v = ctx->GetVars<demo_vars>();
    engine_api *api = &v.api;
    void *e = api->engine;
    IM_CHECK(api->map_info(e).real_map);
    IM_CHECK(demo_begin_video(ctx, milestone));

    IM_CHECK(demo_open_panel(ctx, "Simulation", &v.ui.show_sim));
    ctx->SetRef("Simulation");
    ctx->ItemClick("Load sample save");
    ctx->Yield();
    IM_CHECK_EQ(api->city_count(e), 42u);
    v.ui.show_sim = false;

    /* The gate and port records of the save (0x08428D): 皖口港 shows its 6266
       soldiers on its plate and in the Bases panel, as the game does. */
    engine_base hukou = api->base_at(e, 72 - CONTENT_FIRST_BASE);
    IM_CHECK_EQ(hukou.troops, 6266u);
    IM_CHECK(demo_open_panel(ctx, "###Bases", &v.ui.show_bases));
    ui_frame_hex(&v.ui, api, hex_make(hukou.q, hukou.r));
    ctx->Yield(2);
    IM_CHECK(demo_camera_on(v, hex_make(hukou.q, hukou.r)));
    ctx->SleepStandard();

    /* The round mini map: a click moves the camera there; the distance stays. */
    IM_CHECK(demo_open_panel(ctx, "###MiniMap", &v.ui.show_minimap));
    ctx->WindowResize("//###MiniMap", ImVec2(340, 370));
    ctx->Yield(2);
    ctx->SetRef("###MiniMap");
    ImGuiTestItemInfo disc = ctx->ItemInfo("minimap");
    IM_CHECK(disc.ID != 0);
    ctx->SleepStandard();
    camera before = v.ui.scene.cam;
    ImVec2 centre = disc.RectFull.GetCenter();
    f32 radius = disc.RectFull.GetWidth() * 0.5f - UI_MINIMAP_RING_WIDTH;
    ctx->MouseMoveToPos(ImVec2(centre.x - radius * 0.4f, centre.y + radius * 0.3f));
    ctx->MouseClick(0);
    ctx->Yield();
    IM_CHECK(v3_length(v3_sub(v.ui.scene.cam.target, before.target)) > 10.0f);
    IM_CHECK_EQ(v.ui.scene.cam.distance, before.distance);
    ctx->SleepStandard();

    /* A unit plate: frame a unit of the save from the Units panel. */
    u32 unit_id = ENGINE_NO_UNIT;
    for (u32 id = 0; id < api->unit_count(e) && unit_id == ENGINE_NO_UNIT; ++id) if (api->unit_at(e, id).id != ENGINE_NO_UNIT) unit_id = id;
    IM_CHECK(unit_id != ENGINE_NO_UNIT);
    IM_CHECK(demo_open_panel(ctx, "###Units", &v.ui.show_units));
    char row[32];
    snprintf(row, sizeof(row), "**/###unit%u", unit_id);
    ctx->SetRef("###Units");
    ctx->ItemClick(row);
    ctx->Yield(2);
    engine_unit unit = api->unit_at(e, unit_id);
    IM_CHECK(demo_camera_on(v, hex_make(unit.q, unit.r)));
    IM_CHECK(unit.grain_turns != ENGINE_GRAIN_TURNS_UNKNOWN && unit.grain_turns == unit.grain / (unit.soldiers / 40));   /* the grain of the record */
    IM_CHECK_EQ(ui_unit_mark_of(&unit, api->city_corps(e, unit.home_city)), (u32)UI_UNIT_MARK_READY);   /* 未 */
    v.ui.show_units = false;
    ctx->SleepStandard();

    /* The smooth turn: Q clockwise and E counter-clockwise while the key is down. */
    ImGui::FocusWindow(0);
    ctx->Yield();
    f32 yaw = v.ui.scene.cam.yaw;
    ctx->KeyDown(ImGuiKey_Q); ctx->Sleep(0.8f); ctx->KeyUp(ImGuiKey_Q);
    IM_CHECK_LT(demo_turn_between(yaw, v.ui.scene.cam.yaw), 0.0f);
    yaw = v.ui.scene.cam.yaw;
    ctx->KeyDown(ImGuiKey_RightArrow); ctx->Sleep(0.8f); ctx->KeyUp(ImGuiKey_RightArrow);
    IM_CHECK_GT(demo_turn_between(yaw, v.ui.scene.cam.yaw), 0.0f);
    ctx->SleepStandard();

    IM_CHECK(demo_end_video(ctx));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###MiniMap", "minimap"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###Bases", "bases"));
}

/* M14 (the game screens; the request of the user, main-map.md and
   screen-flow.md): the game mode shows only the HUD and the list at the
   top left and the mini map at the bottom right. A click on 柴桑 in the
   list frames it, and a right click opens its City dialog next to the
   cursor. 巡察 runs and the dialog closes. A right click on the empty map
   opens the map menu, and 進行 ends the turn. Q turns the view smoothly. */
internal void
demo_m14_game_screens(ImGuiTestContext *ctx)
{
    const char *milestone = DEMO_M14_NAME;
    demo_vars &v = ctx->GetVars<demo_vars>();
    engine_api *api = &v.api;
    void *e = api->engine;
    IM_CHECK(api->map_info(e).real_map);
    IM_CHECK(demo_open_panel(ctx, "Simulation", &v.ui.show_sim));
    ctx->SetRef("Simulation");
    ctx->ItemClick("Load sample save");
    ctx->Yield();
    IM_CHECK_EQ(api->city_count(e), 42u);
    ui_enter_game(&v.ui);   /* as a load from the title */
    ctx->Yield(2);
    IM_CHECK(demo_begin_video(ctx, milestone));
    IM_CHECK(ctx->WindowInfo("//###HUD").Window != 0 && v.ui.show_minimap);
    ImGuiWindow *bar = ctx->WindowInfo("//##MainMenuBar", ImGuiTestOpFlags_NoError).Window;
    IM_CHECK(bar == 0 || !bar->Active);   /* no debug panels */
    u8 force = api->whose_turn(e);
    IM_CHECK_EQ(api->city_at(e, 26).owner_force_id, force);
    ctx->SleepStandard();

    /* 柴桑 from the list: a click frames it, a right click opens its dialog. */
    engine_city_slots chaisang = api->city_slots(e, 26);
    ctx->SetRef("//###HUD");
    ctx->ItemClick("**/###hud_row0_26");
    ctx->Yield(2);
    IM_CHECK(demo_camera_on(v, hex_make(chaisang.q, chaisang.r)));
    ctx->SleepStandard();
    ctx->ItemClick("**/###hud_row0_26", ImGuiMouseButton_Right);
    ctx->Yield(2);
    IM_CHECK(v.ui.show_city && v.ui.context.kind == UI_CONTEXT_CITY);
    ctx->SleepStandard();

    /* 巡察: the command runs, and the dialog closes after it. */
    ctx->SetRef("###City");
    ctx->ItemClick("###open_patrol");
    ctx->Yield();
    IM_CHECK(v.ui.show_command[UI_COMMAND_PATROL]);
    ctx->SleepStandard();
    u32 ledger = api->ledger_count(e);
    ctx->SetRef("###Patrol");
    ctx->ItemClick("###decide");
    ctx->Yield(2);
    IM_CHECK(api->ledger_count(e) > ledger && !v.ui.show_city && v.ui.context.kind == UI_CONTEXT_NONE);
    ctx->SleepStandard();

    /* A right click on an empty hex near 柴桑 opens the map menu; 進行 ends the turn. */
    ImVec2 display = ImGui::GetIO().DisplaySize;
    m4 view_projection = camera_view_projection(&v.ui.scene.cam, display.x, display.y);
    hex_offset centre = hex_to_offset(hex_make(chaisang.q, chaisang.r));
    ImVec2 empty(0.0f, 0.0f);
    for (s32 step = 0; step < 13 * 9 && empty.x == 0.0f; ++step)
    {
        hex_offset cell = { centre.col + step % 13 - 6, centre.row + step / 13 - 4 };
        hex_coord hex = hex_from_offset(cell);
        engine_hex data = api->hex_at(e, hex.q, hex.r);
        f32 x = 0.0f, y = 0.0f;
        if (data.on_map && data.city_id == ENGINE_NO_CITY && data.unit_id == ENGINE_NO_UNIT && data.base_id == ENGINE_NO_BASE &&
            camera_project(view_projection, hex_world_center(hex, 0.0f), display.x, display.y, &x, &y) &&
            x > display.x * 0.35f && x < display.x * 0.75f && y > display.y * 0.15f && y < display.y * 0.6f)
        {
            empty = ImVec2(x, y);
        }
    }
    IM_CHECK(empty.x != 0.0f);
    ctx->MouseMoveToPos(empty);
    ctx->MouseClick(ImGuiMouseButton_Right);
    ctx->Yield(2);
    IM_CHECK_EQ(v.ui.context.kind, (u8)UI_CONTEXT_MAP_MENU);
    ctx->SleepStandard();
    u32 turn = api->turn(e);
    ctx->SetRef("//###MapMenu");
    ctx->ItemClick("###proceed");
    ctx->Yield(2);
    IM_CHECK(api->turn(e) != turn || api->whose_turn(e) != force);
    IM_CHECK_EQ(v.ui.context.kind, (u8)UI_CONTEXT_NONE);
    ctx->SleepStandard();

    /* Q turns the view smoothly while it is down. */
    ImGui::FocusWindow(0);
    ctx->Yield();
    f32 yaw = v.ui.scene.cam.yaw;
    ctx->KeyDown(ImGuiKey_Q); ctx->Sleep(0.8f); ctx->KeyUp(ImGuiKey_Q);
    IM_CHECK_LT(demo_turn_between(yaw, v.ui.scene.cam.yaw), 0.0f);
    ctx->SleepStandard();

    IM_CHECK(demo_end_video(ctx));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###HUD", "hud"));
    IM_CHECK(demo_screenshot_panel(ctx, milestone, "###MiniMap", "minimap"));
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

    t = IM_REGISTER_TEST(e, "demo", "m5_march");
    t->SetVarsDataType<demo_vars>();
    t->GuiFunc  = demo_gui;
    t->TestFunc = demo_m5_march;

    t = IM_REGISTER_TEST(e, "demo", "m6_battle");
    t->SetVarsDataType<demo_vars>();
    t->GuiFunc  = demo_gui;
    t->TestFunc = demo_m6_battle;

    t = IM_REGISTER_TEST(e, "demo", "m7_court");
    t->SetVarsDataType<demo_vars>();
    t->GuiFunc  = demo_gui;
    t->TestFunc = demo_m7_court;

    t = IM_REGISTER_TEST(e, "demo", "m8_rivals");
    t->SetVarsDataType<demo_vars>();
    t->GuiFunc  = demo_gui;
    t->TestFunc = demo_m8_rivals;

    t = IM_REGISTER_TEST(e, "demo", "m9_playable");
    t->SetVarsDataType<demo_vars>();
    t->GuiFunc  = demo_gui;
    t->TestFunc = demo_m9_playable;

    t = IM_REGISTER_TEST(e, "demo", DEMO_M10_NAME);
    t->SetVarsDataType<demo_vars>();
    t->GuiFunc  = demo_gui;
    t->TestFunc = demo_m10_map;

    t = IM_REGISTER_TEST(e, "demo", DEMO_M11_NAME);
    t->SetVarsDataType<demo_vars>();
    t->GuiFunc  = demo_gui;
    t->TestFunc = demo_m11_save_map;

    t = IM_REGISTER_TEST(e, "demo", DEMO_M12_NAME);
    t->SetVarsDataType<demo_vars>();
    t->GuiFunc  = demo_gui;
    t->TestFunc = demo_m12_main_map;

    t = IM_REGISTER_TEST(e, "demo", DEMO_M14_NAME);
    t->SetVarsDataType<demo_vars>();
    t->GuiFunc  = demo_gui;
    t->TestFunc = demo_m14_game_screens;
}
