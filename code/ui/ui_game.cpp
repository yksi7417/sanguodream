/* The game screens of M9 (ui/title-and-load.md, ui/screen-flow.md,
   ui/main-map.md, ui/information-screens.md; F-140 to F-145, F-133 to
   F-135, F-060, F-061, F-009): the title menu, the scenario list, the
   ruler list, the settings of the start, the top bar with 進行 and the
   history window, the save and load screen, the options (new officers,
   editor, settings), the battle mode list, the ability chart and the
   information screen. They sit on top of the debug panels: the city, unit,
   duel and debate panels of the earlier milestones are the command menus
   and screens of the game. Placeholder art: ImGui windows. The UI calls
   engine_api only. */

/* The main map of a game: the game mode. The HUD and the list sit at the
   top left and the mini map at the bottom right. Every other window opens
   on demand (ui_context.cpp). */
/* The city names of the name index (Scenario.s11 of the spec), after a
   game starts from the title or a slot, when no names are loaded yet. The
   player asked on 2026-10-20 for one step: a game start must show the
   names without the debug button. A loaded name index stays. */
internal void
ui_ensure_city_names(ui_state *ui, engine_api *api)
{
    if (api->city_name_at(api->engine, 0).city_id != ENGINE_NO_CITY) return;
    engine_load_result names = api->load_name_index(api->engine, UI_NAME_INDEX_PATH);
    if (names.status == ENGINE_LOAD_OK)
        snprintf(ui->names_message, sizeof(ui->names_message), "names loaded (command %u)", names.command_id);
    else
        snprintf(ui->names_message, sizeof(ui->names_message), "names not loaded: %s", names.detail ? names.detail : "?");
}

/* The main screens of the game sit at the centre of the window in every
   frame (the request of the player, 2026-10-20). A resize of the window
   keeps them there, and the position that imgui.ini kept cannot move them. */
internal void
ui_center_next_window(void)
{
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
}

internal void
ui_enter_game(ui_state *ui)
{
    ui->show_title = ui->show_scenarios = ui->show_rulers = ui->show_start_settings = false;
    ui->show_splash  = false;   /* a shared game can start before the splash closes */
    ui->show_story   = false;
    ui->hud_mode     = true;
    ui->show_topbar  = false;   /* the HUD takes its place */
    ui->show_history = false;
    ui->show_minimap = true;
    ui->show_map = ui->show_city = ui->show_officers = ui->show_cities = ui->show_units = false;
    ui->show_unit = ui->show_unit_menu = false;
    ui->scene.has_pick = false;
    ui->context = {};
    ui_feel_reset_turn(ui);   /* M17: the first turn start shows its messages */
    ui->feel.first_turn  = true;    /* G2-006: the camera goes to the capital, and the adviser says that it is our turn */
    ui->feel.loaded_game = false;   /* G3-023: the load sets it after this call */
}

internal void ui_net_show_notice(ui_state *ui, u32 notice);   /* ui_multiplayer.cpp */

internal void
ui_open_saveload(ui_state *ui, engine_api *api, bool saving)
{
    engine_multiplayer_info shared = api->multiplayer_info(api->engine);
    if (shared.shared)
    {
        /* MP5: a shared game never writes a local slot, and no player loads a save into it.
           儲存 of the host sends a snapshot of the state to the room (ui_multiplayer.cpp). */
        if (!saving)           ui_net_show_notice(ui, PLATFORM_NET_NOTICE_LOAD_OFF);
        else if (!shared.host) ui_net_show_notice(ui, PLATFORM_NET_NOTICE_HOST_SAVES);
        else                   ui->net.snapshot_request = true;
        return;
    }
    ui->show_saveload  = true;
    ui->game.saving    = saving;
    ui->game.chosen    = ENGINE_SAVE_SLOTS;
    ui->game.message[0] = 0;
    api->refresh_save_slots(api->engine);
}

/* 儲存 or 載入 of a menu of the game mode. In a shared game, no dialog opens, and the menu closes. */
internal void
ui_menu_open_saveload(ui_state *ui, engine_api *api, bool saving, f32 x, f32 y)
{
    ui_open_saveload(ui, api, saving);
    if (ui->show_saveload) ui_context_open(ui, api, UI_CONTEXT_SAVELOAD, x, y);
    else                   ui_context_close(ui);
}

/* ---- 標題 (F-145) -------------------------------------------------------------------- */

/* What a title button opens (title-and-load.md, "Title menu"). */
internal void
ui_title_open(ui_state *ui, engine_api *api, u32 choice)
{
    switch (choice)
    {
        case UI_TITLE_LOAD:    ui_open_saveload(ui, api, false); break;
        case UI_TITLE_NEW:     ui->show_scenarios = true;        break;
        case UI_TITLE_OPTIONS: ui->show_options = true;          break;
        case UI_TITLE_QUIT:    ui->quit_requested = true;        break;
        default: break;
    }
}

/* One button of the title. A click opens the screen at once. */
internal void
ui_title_button(ui_state *ui, engine_api *api, const char *text_id, const char *key, u32 choice)
{
    char label[96];
    if (ImGui::Button(ui_label(label, sizeof(label), ui, text_id, key), ImVec2(320, 0))) ui_title_open(ui, api, choice);
}

/* The version and the number of the build (code/version.h), for the title and
   the help window. A disabled Selectable gives the line a stable id for the
   tests, and a click on it does nothing. */
internal void
ui_draw_version(ui_state *ui, const char *stable_id)
{
    char line[160];
    snprintf(line, sizeof(line), "%s %s  %s %s###%s", text_get(ui->text, "about.version"), HEXDOM_VERSION,
             text_get(ui->text, "about.build"), HEXDOM_BUILD_TEXT, stable_id);
    ImGui::BeginDisabled();
    ImGui::Selectable(line, false);
    ImGui::EndDisabled();
}

internal void ui_draw_title_room_code(ui_state *ui);   /* ui_multiplayer.cpp */

internal void
ui_draw_title(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(360, 330), ImGuiCond_FirstUseEver);
    ui_center_next_window();   /* the request of the player of 2026-10-20 */
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "title.title", "Title"), 0, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    ui_draw_title_language(ui, 320.0f);   /* the request of the player of 2026-10-31: the language changes on the first screen */
    ImGui::Separator();
    ui_title_button(ui, api, "title.load", "title_load", UI_TITLE_LOAD);
    ui_title_button(ui, api, "title.new", "title_new", UI_TITLE_NEW);
    ui_title_button(ui, api, "title.options", "title_options", UI_TITLE_OPTIONS);
    ui_title_button(ui, api, "title.quit", "title_quit", UI_TITLE_QUIT);
    ui_draw_title_room_code(ui);   /* MP5: join a shared game with its room code (ui_multiplayer.cpp) */
    ImGui::Separator();
    ui_draw_version(ui, "title_version");
    ImGui::End();
}

/* ---- 劇本 and 君主 (F-133, F-145) --------------------------------------------------------- */

/* Loads a scenario of the scenario list and opens the ruler list. A
   scenario of another pack does not load here: the UI asks the game to start
   the engine again with that pack (game.pack_request), and the new engine
   loads the scenario of the key (ui_start_pack_scenario). */
internal void
ui_choose_scenario(ui_state *ui, engine_api *api, const engine_scenario *scenario)
{
    void *e = api->engine;
    if (scenario->pack[0])
    {
        snprintf(ui->game.pack_request, sizeof(ui->game.pack_request), "%s", scenario->pack);
        snprintf(ui->game.pack_scenario, sizeof(ui->game.pack_scenario), "%s", scenario->key);
        ui->show_scenarios = false;
        return;
    }
    engine_load_result loaded = scenario->kind == ENGINE_SCENARIO_SAVE ? api->load_save(e, scenario->file) : api->load_scenario(e, scenario->file);
    if (loaded.status != ENGINE_LOAD_OK) return;
    ui_ensure_city_names(ui, api);
    ui->game.rulers    = 0;
    ui->show_scenarios = false;
    ui->show_rulers    = true;
}

/* Loads a save slot and enters the game. A save of another pack (its
   slot_NN.meta names that pack) does not load here: the UI asks the game to
   start the engine of that pack, and the new engine loads the slot. */
internal void
ui_load_slot(ui_state *ui, engine_api *api, u32 slot)
{
    void *e = api->engine;
    engine_slot_pack pack = api->slot_pack(e, slot);
    if (pack.pack[0])
    {
        snprintf(ui->game.pack_request, sizeof(ui->game.pack_request), "%s", pack.pack);
        ui->game.pack_slot = slot + 1;
        ui->show_saveload  = false;
        return;
    }
    engine_save_status status = api->load_game(e, slot);
    text_label(ui->text, "save_status", status, ui->game.message, sizeof(ui->game.message));
    if (status != ENGINE_SAVE_OK) return;
    ui_ensure_city_names(ui, api);
    ui->show_saveload = false;
    ui_enter_game(ui);
    ui->feel.loaded_game = true;   /* G3-023: 劉表，可終於輪到你了啊。 and an empty log */
}

/* After the engine started again with another pack: load the scenario or
   the save slot of that pack that the player chose. A scenario then shows
   the ruler list; a slot enters the game. */
internal void
ui_start_pack_scenario(ui_state *ui, engine_api *api)
{
    if (ui->game.pack_request[0]) return;
    if (ui->game.pack_slot)
    {
        u32 slot = ui->game.pack_slot - 1;
        ui->game.pack_slot = 0;
        api->refresh_save_slots(api->engine);
        ui_load_slot(ui, api, slot);
        return;
    }
    if (!ui->game.pack_scenario[0]) return;
    void *e = api->engine;
    for (u32 index = 0; index < api->scenario_count(e); ++index)
    {
        engine_scenario scenario = api->scenario_at(e, index);
        if (scenario.pack[0] || strcmp(scenario.key, ui->game.pack_scenario) != 0) continue;
        ui_choose_scenario(ui, api, &scenario);
        break;
    }
    ui->game.pack_scenario[0] = 0;
    if (!ui->show_rulers) ui->show_scenarios = true;   /* the scenario did not load: the list stays */
}

internal void
ui_draw_scenarios(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(420, 320), ImGuiCond_FirstUseEver);
    ui_center_next_window();   /* the request of the player of 2026-10-20 */
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "scenarios.title", "Scenarios"), &ui->show_scenarios)) { ImGui::End(); return; }
    void *e = api->engine;
    /* The request of the player of 2026-10-30: the list shows only the
       scenarios with a file, in the order of their years. Thus the Three
       Kingdoms come before the game of 1200. In one year, a scenario comes
       before a saved game. The id of each row stays its index in the list. */
    u32 order[UI_SCENARIO_ROWS];
    u32 shown = 0;
    for (u32 index = 0; index < api->scenario_count(e) && shown < UI_SCENARIO_ROWS; ++index)
    {
        engine_scenario scenario = api->scenario_at(e, index);
        if (!scenario.available) continue;
        u32 place = shown++;
        u32 rank  = (u32)scenario.year * 2 + (scenario.kind == ENGINE_SCENARIO_SAVE ? 1 : 0);
        while (place > 0)
        {
            engine_scenario before = api->scenario_at(e, order[place - 1]);
            if ((u32)before.year * 2 + (before.kind == ENGINE_SCENARIO_SAVE ? 1 : 0) <= rank) break;
            order[place] = order[place - 1];
            --place;
        }
        order[place] = index;
    }
    ImGuiStorage *storage = ImGui::GetStateStorage();
    ImGuiID hover_id = ImGui::GetID("scenario_hover");
    for (u32 row_index = 0; row_index < shown; ++row_index)
    {
        u32 index = order[row_index];
        engine_scenario scenario = api->scenario_at(e, index);
        char kind[48], row[256];
        text_label(ui->text, "scenario_kind", scenario.kind, kind, sizeof(kind));
        snprintf(row, sizeof(row), "%u  %s  [%s]###sc%u", scenario.year, ui_text_with_key(ui, "scenario", scenario.key), kind, index);
        if (ImGui::Selectable(row)) ui_choose_scenario(ui, api, &scenario);
        if (ImGui::IsItemHovered()) storage->SetInt(hover_id, (int)index + 1);
    }
    /* The description of the scenario under the pointer, or of the last one. */
    int hovered = storage->GetInt(hover_id, shown ? (int)order[0] + 1 : 0);
    if (hovered > 0 && (u32)hovered <= api->scenario_count(e))
    {
        engine_scenario scenario = api->scenario_at(e, (u32)hovered - 1);
        char id[96];
        snprintf(id, sizeof(id), "scenario_about.%s", scenario.key);
        const char *about = text_find_string(ui->text, id);
        if (about && scenario.available)
        {
            ImGui::Separator();
            ImGui::TextWrapped("%s", about);
        }
    }
    ImGui::End();
}

internal void ui_draw_arms_icon(ui_state *ui, engine_api *api, u8 force_id);   /* ui_locate.cpp (E5) */

/* Up to 8 rulers (manual p.4). The faction that the save played starts checked. */
internal void
ui_draw_rulers(ui_state *ui, engine_api *api)
{
    char title[128], label[96];
    ImGui::SetNextWindowSize(ImVec2(360, 420), ImGuiCond_FirstUseEver);
    ui_center_next_window();   /* the request of the player of 2026-10-20 */
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "rulers.title", "Rulers"), &ui->show_rulers)) { ImGui::End(); return; }
    void *e = api->engine;
    u32 chosen = 0;
    for (u32 force = 0; force < 64; ++force) chosen += (ui->game.rulers >> force) & 1;
    for (u32 index = 0; index < api->faction_count(e); ++index)
    {
        engine_faction faction = api->faction_at(e, index);
        if (api->force_ruler(e, faction.force_id) == ENGINE_NO_OFFICER) continue;
        char name[UI_NAME_SIZE * 2], row[160];
        ui_force_text(ui, api, faction.force_id, name, sizeof(name));
        snprintf(row, sizeof(row), "%s###ruler%u", name, faction.force_id);
        bool on = ((ui->game.rulers >> faction.force_id) & 1) != 0;
        ui_draw_arms_icon(ui, api, faction.force_id);   /* E5 */
        ImGui::SameLine();
        ImGui::BeginDisabled(!on && chosen >= 8);
        if (ImGui::Checkbox(row, &on)) ui->game.rulers ^= 1ull << faction.force_id;
        ImGui::EndDisabled();
    }
    if (ui_story_available(ui))
    {
        if (ImGui::Button(ui_label(label, sizeof(label), ui, "story.button", "story_open"))) { ui->show_story = true; ui->story.page = 0; }
        ImGui::SameLine();
    }
    ImGui::BeginDisabled(chosen == 0);
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "rulers.ok", "rulers_ok")))
    {
        for (u32 index = 0; index < api->faction_count(e); ++index)
        {
            u8 force = api->faction_at(e, index).force_id;
            api->set_faction_human(e, force, ((ui->game.rulers >> force) & 1) != 0);
        }
        ui->show_rulers         = false;
        ui->show_start_settings = true;
        ui->game.start_loaded   = false;
    }
    ImGui::EndDisabled();
    ImGui::End();
}

/* 遊戲設定 of the start (F-132): then the first turn of a player. */
internal void
ui_draw_start_settings(ui_state *ui, engine_api *api)
{
    char title[128], label[96];
    ImGui::SetNextWindowSize(ImVec2(420, 380), ImGuiCond_FirstUseEver);
    ui_center_next_window();   /* the request of the player of 2026-10-20 */
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "start.title", "StartSettings"), &ui->show_start_settings)) { ImGui::End(); return; }
    void *e = api->engine;
    if (!ui->game.start_loaded) { ui->game.start = api->settings(e); ui->game.start_loaded = true; }
    engine_settings settings = ui->game.start;
    int lifespan = !settings.lifespan ? 0 : settings.long_lifespan ? 2 : 1;
    ImGui::TextUnformatted(text_get(ui->text, "start.lifespan"));
    for (int value = 0; value < 3; ++value)
    {
        char text[48], stable[64];
        text_label(ui->text, "lifespan", (u32)value, text, sizeof(text));
        snprintf(stable, sizeof(stable), "%s###lifespan%d", text, value);
        if (value) ImGui::SameLine();
        ImGui::RadioButton(stable, &lifespan, value);
    }
    settings.lifespan      = lifespan != 0;
    settings.long_lifespan = lifespan == 2;
    bool flags[6] = { settings.ability_change != 0, settings.events != 0, settings.female_officers != 0, settings.strong_plots != 0, settings.relations != 0, settings.disasters != 0 };
    static const char *ids[6]     = { "start.ability_change", "start.events", "start.female", "start.strong_plots", "start.relations", "settings.disasters" };
    static const char *stables[6] = { "ability_change", "start_events", "female", "strong_plots", "relations", "start_disasters" };
    for (u32 index = 0; index < 6; ++index) ImGui::Checkbox(ui_label(label, sizeof(label), ui, ids[index], stables[index]), flags + index);
    settings.ability_change  = flags[0];
    settings.events          = flags[1];
    settings.female_officers = flags[2];
    settings.strong_plots    = flags[3];
    settings.relations       = flags[4];
    settings.disasters       = flags[5];
    int duel = settings.duel_mode, level = settings.difficulty;
    ImGui::TextUnformatted(text_get(ui->text, "start.duel"));
    for (int value = 0; value < DUEL_MODE_COUNT; ++value)
    {
        char text[48], stable[64];
        text_label(ui->text, "duel_mode", (u32)value, text, sizeof(text));
        snprintf(stable, sizeof(stable), "%s###duel%d", text, value);
        if (value) ImGui::SameLine();
        ImGui::RadioButton(stable, &duel, value);
    }
    ImGui::TextUnformatted(text_get(ui->text, "start.level"));
    for (int value = 0; value < DIFFICULTY_COUNT; ++value)
    {
        char text[48], stable[64];
        text_label(ui->text, "difficulty", (u32)value, text, sizeof(text));
        snprintf(stable, sizeof(stable), "%s###level%d", text, value);
        if (value) ImGui::SameLine();
        ImGui::RadioButton(stable, &level, value);
    }
    settings.duel_mode  = (u8)duel;
    settings.difficulty = (u8)level;
    ui->game.start      = settings;
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "start.ok", "start_ok")))
    {
        api->set_settings(e, settings);
        /* The turn passes until a chosen ruler acts (after a scenario no faction acts yet). */
        for (u32 step = 0; step < 300; ++step)
        {
            u8 acting = api->whose_turn(e);
            if (acting != ENGINE_NO_FACTION && ((ui->game.rulers >> acting) & 1)) break;
            api->end_turn(e);
        }
        ui_enter_game(ui);
    }
    if (ui->net.available)   /* MP3: the page makes a room with a seat for each ruler (ui_multiplayer.cpp) */
    {
        ImGui::SameLine();
        if (ImGui::Button(ui_label(label, sizeof(label), ui, "net.play_online", "start_online")))
        {
            api->set_settings(e, settings);
            /* The start image holds the first faction that acts. Each faction keeps its
               first turn, because a person can take any seat in the lobby. */
            for (u32 step = 0; step < 300 && api->whose_turn(e) == ENGINE_NO_FACTION; ++step) api->end_turn(e);
            ui->net.host_request = true;
        }
    }
    ImGui::End();
}

/* ---- 主地圖: the top bar and the history window (F-140) ----------------------------------------- */

internal void
ui_draw_topbar(ui_state *ui, engine_api *api)
{
    char title[128], label[96];
    ImGui::SetNextWindowPos(ImVec2(0, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(760, 70), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "topbar.title", "TopBar"), 0, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    void *e = api->engine;
    engine_date date = api->date(e);
    u8 force = api->whose_turn(e);
    char faction[UI_NAME_SIZE * 2] = "-";
    if (force != ENGINE_NO_FACTION) ui_force_text(ui, api, force, faction, sizeof(faction));
    ImGui::Text("%u/%u/%u   %s   %s %u", date.year, date.month, date.day, faction, text_get(ui->text, "topbar.points"),
                force != ENGINE_NO_FACTION ? api->actions_left(e, force) : 0u);
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "topbar.proceed", "proceed"))) ui_round_proceed(ui, api);   /* M20 */
    ImGui::SameLine();
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "topbar.info", "open_info"))) ui->show_info = true;
    ImGui::SameLine();
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "topbar.ruler", "open_ruler"))) ui->show_ruler = true;
    ImGui::SameLine();
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "topbar.abilities", "open_abilities"))) ui->show_abilities = true;
    ImGui::SameLine();
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "topbar.save", "open_save"))) ui_open_saveload(ui, api, true);
    ImGui::SameLine();
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "topbar.load", "open_load"))) ui_open_saveload(ui, api, false);
    ImGui::SameLine();
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "topbar.system", "open_system"))) ui->show_options = true;
    ImGui::End();
}

/* The last events of the ledger, newest first (main-map.md, history window). */
internal void
ui_draw_history(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(420, 220), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "history.title", "History"), &ui->show_history)) { ImGui::End(); return; }
    u32 count = api->ledger_count(api->engine);
    for (u32 shown = 0; shown < 12 && shown < count; ++shown)
    {
        sim_event event = api->ledger_at(api->engine, count - 1 - shown);
        ImGui::Text("%u  %s  %d", event.turn, sim_event_name(event.kind), event.delta);
    }
    ImGui::End();
}

/* ---- 儲存 and 載入 (F-009) ------------------------------------------------------------------ */

internal void
ui_draw_saveload(ui_state *ui, engine_api *api)
{
    char title[128], label[96];
    ImGui::SetNextWindowSize(ImVec2(620, 420), ImGuiCond_FirstUseEver);
    ui_center_next_window();   /* the request of the player of 2026-10-20 */
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "saves.title", "SaveLoad"), &ui->show_saveload)) { ImGui::End(); return; }
    ui_context_track(ui);
    void *e = api->engine;
    ImGui::TextUnformatted(text_get(ui->text, ui->game.saving ? "saves.saving" : "saves.loading"));
    for (u32 page = 0; page < ENGINE_SAVE_SLOTS / 8; ++page)
    {
        char tab[64];
        snprintf(tab, sizeof(tab), "%u-%u###page%u", page * 8 + 1, page * 8 + 8, page);
        if (page) ImGui::SameLine();
        if (ImGui::Button(tab)) ui->game.page = page;
    }
    ImGui::BeginGroup();
    for (u32 slot = ui->game.page * 8; slot < ui->game.page * 8 + 8; ++slot)
    {
        engine_save_slot info = api->save_slot(e, slot);
        char row[160];
        if (info.used) snprintf(row, sizeof(row), "%2u  %u/%u/%u  %s###slot%u", slot + 1, info.year, info.month, info.day, info.comment, slot);
        else           snprintf(row, sizeof(row), "%2u  %s###slot%u", slot + 1, text_get(ui->text, "saves.empty"), slot);
        ImGui::BeginDisabled(!ui->game.saving && !info.used);
        if (ImGui::Button(row, ImVec2(300, 0)))
        {
            if (ui->game.saving) { ui->game.chosen = slot; snprintf(ui->game.comment, sizeof(ui->game.comment), "%s", info.comment); }
            else
            {
                ui_load_slot(ui, api, slot);
            }
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ui->game.hovered = slot;
    }
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::BeginGroup();
    engine_save_slot detail = api->save_slot(e, ui->game.hovered);
    ImGui::Text("%s %u", text_get(ui->text, "saves.slot"), ui->game.hovered + 1);
    if (detail.used)
    {
        char faction[UI_NAME_SIZE * 2] = "-";
        if (detail.player != ENGINE_NO_FACTION) ui_force_text(ui, api, detail.player, faction, sizeof(faction));
        ImGui::Text("%u/%u/%u  %s", detail.year, detail.month, detail.day, faction);
        ImGui::Text("%s: %s", text_get(ui->text, "saves.comment"), detail.comment);
    }
    ImGui::EndGroup();
    if (ui->game.saving && ui->game.chosen < ENGINE_SAVE_SLOTS)
    {
        ImGui::Separator();
        ImGui::InputText(ui_label(label, sizeof(label), ui, "saves.comment", "comment"), ui->game.comment, sizeof(ui->game.comment));
        if (ImGui::Button(ui_label(label, sizeof(label), ui, "saves.ok", "save_ok")))
        {
            engine_save_status status = api->save_game(e, ui->game.chosen, ui->game.comment);
            text_label(ui->text, "save_status", status, ui->game.message, sizeof(ui->game.message));
            ui->game.chosen = ENGINE_SAVE_SLOTS;
        }
        ImGui::SameLine();
        if (ImGui::Button(ui_label(label, sizeof(label), ui, "saves.back", "save_back"))) ui->game.chosen = ENGINE_SAVE_SLOTS;
    }
    if (ui->game.message[0]) ImGui::TextWrapped("%s", ui->game.message);
    ImGui::End();
}

/* ---- 項目: new officers, editor, settings (F-060, F-134) ---------------------------------- */

internal void
ui_draw_options(ui_state *ui, engine_api *api)
{
    (void)api;
    char title[128], label[96];
    ImGui::SetNextWindowSize(ImVec2(300, 160), ImGuiCond_FirstUseEver);
    ui_center_next_window();   /* the request of the player of 2026-10-20 */
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "options.title", "Options"), &ui->show_options)) { ImGui::End(); return; }
    ui_context_track(ui);
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "options.new_officer", "open_new_officer"))) ui->show_new_officer = true;
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "options.editor", "open_editor")))         ui->show_editor = true;
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "options.settings", "open_settings")))     ui->show_settings = true;
    ImGui::End();
}

internal void
ui_draw_new_officer(ui_state *ui, engine_api *api)
{
    char title[128], label[96];
    ImGui::SetNextWindowSize(ImVec2(440, 460), ImGuiCond_FirstUseEver);
    ui_center_next_window();   /* the request of the player of 2026-10-20 */
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "newcomer.title", "NewOfficer"), &ui->show_new_officer)) { ImGui::End(); return; }
    void *e = api->engine;
    ImGui::Text("%s: %u / 8", text_get(ui->text, "newcomer.count"), api->new_officer_count(e));
    ImGui::InputText(ui_label(label, sizeof(label), ui, "newcomer.family", "family"), ui->game.family, sizeof(ui->game.family));
    ImGui::InputText(ui_label(label, sizeof(label), ui, "newcomer.given", "given"), ui->game.given, sizeof(ui->game.given));
    ImGui::Checkbox(ui_label(label, sizeof(label), ui, "newcomer.female", "female_officer"), &ui->game.female);
    for (int type = 0; type < OFFICER_TYPE_COUNT; ++type)
    {
        char text[48], stable[64];
        text_label(ui->text, "officer_type", (u32)type, text, sizeof(text));
        snprintf(stable, sizeof(stable), "%s###type%d", text, type);
        if (type) ImGui::SameLine();
        ImGui::RadioButton(stable, &ui->game.type, type);
    }
    for (int value = 0; value < PERSONALITY_COUNT; ++value)
    {
        char text[48], stable[64];
        text_label(ui->text, "personality", (u32)value, text, sizeof(text));
        snprintf(stable, sizeof(stable), "%s###personality%d", text, value);
        if (value) ImGui::SameLine();
        ImGui::RadioButton(stable, &ui->game.personality, value);
    }
    ImGui::InputInt(ui_label(label, sizeof(label), ui, "newcomer.birth_year", "birth_year"), &ui->game.birth_year);
    char city[UI_NAME_SIZE * 2] = "-";
    if (ui->game.city != ENGINE_NO_CITY) ui_city_label(ui, api, ui->game.city, city, sizeof(city));
    if (ImGui::BeginCombo(ui_label(label, sizeof(label), ui, "newcomer.city", "newcomer_city"), city))
    {
        for (u32 index = 0; index < api->city_count(e); ++index)
        {
            engine_city each = api->city_at(e, index);
            char name[UI_NAME_SIZE * 2], row[128];
            ui_city_label(ui, api, each.city_id, name, sizeof(name));
            snprintf(row, sizeof(row), "%s###c%u", name, each.city_id);
            if (ImGui::Selectable(row, ui->game.city == each.city_id)) ui->game.city = each.city_id;
        }
        ImGui::EndCombo();
    }
    ImGui::Checkbox(ui_label(label, sizeof(label), ui, "newcomer.found_force", "found_force"), &ui->game.found_force);
    char ancient[UI_NAME_SIZE * 2] = "-";
    if (ui->game.ancient != SIM_NO_ANCIENT)
    {
        engine_ancient row = api->ancient_at(e, ui->game.ancient);
        text_person_name(ui->text, row.family_name, sizeof(row.family_name), row.given_name, sizeof(row.given_name), ancient, sizeof(ancient));
    }
    if (ImGui::BeginCombo(ui_label(label, sizeof(label), ui, "newcomer.ancient", "ancient"), ancient))
    {
        if (ImGui::Selectable(text_get(ui->text, "newcomer.none"), ui->game.ancient == SIM_NO_ANCIENT)) ui->game.ancient = SIM_NO_ANCIENT;
        for (u32 index = 0; index < api->ancient_count(e); ++index)
        {
            engine_ancient row = api->ancient_at(e, index);
            char name[UI_NAME_SIZE * 2], item[128];
            text_person_name(ui->text, row.family_name, sizeof(row.family_name), row.given_name, sizeof(row.given_name), name, sizeof(name));
            snprintf(item, sizeof(item), "%s###a%u", name, index);
            if (ImGui::Selectable(item, ui->game.ancient == index)) ui->game.ancient = (u8)index;
        }
        ImGui::EndCombo();
    }
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "newcomer.create", "create_officer")))
    {
        sim_new_officer_order order = {};
        text_encode_big5(ui->game.family, order.family_name, sizeof(order.family_name));
        text_encode_big5(ui->game.given, order.given_name, sizeof(order.given_name));
        order.female      = ui->game.female;
        order.type        = (u8)ui->game.type;
        order.personality = (u8)ui->game.personality;
        order.birth_year  = (u16)Minimum((u32)Maximum(ui->game.birth_year, 0), 400u);
        order.city_id     = ui->game.city;
        order.found_force = ui->game.found_force;
        order.ancient     = ui->game.ancient;
        ui_growth_result(ui, "newcomer.title", api->create_officer(e, &order));
    }
    if (ui->command_message[0]) ImGui::TextWrapped("%s", ui->command_message);
    ImGui::End();
}

/* 編輯 (PK): the officer, faction, city and unit fields that the engine
   editor commands can set (game-setup-and-modes.md). */
internal void
ui_draw_editor(ui_state *ui, engine_api *api)
{
    char title[128], label[96];
    ImGui::SetNextWindowSize(ImVec2(420, 460), ImGuiCond_FirstUseEver);
    if (ui->hud_mode) ui_center_next_window();   /* M22: 編輯 of the system menu is a dialog at a fixed place */
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "editor.title", "Editor"), &ui->show_editor)) { ImGui::End(); return; }
    ui_context_track(ui);
    void *e = api->engine;
    ui_officer_pick(ui, api, "editor.officer", "editor_officer", &ui->game.editor_officer, UI_OFFICERS_OWN, ENGINE_NO_FACTION);
    ImGui::SliderInt(ui_label(label, sizeof(label), ui, "editor.loyalty", "editor_loyalty"), &ui->game.editor_loyalty, 0, 100);
    ImGui::InputInt(ui_label(label, sizeof(label), ui, "editor.merit", "editor_merit"), &ui->game.editor_merit);
    ImGui::BeginDisabled(ui->game.editor_officer == ENGINE_NO_OFFICER);
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "editor.apply_officer", "editor_apply_officer")))
    {
        engine_officer_detail detail = api->officer_detail(e, ui->game.editor_officer);
        engine_officer_edit edit = {};
        edit.loyalty = (u8)Minimum((u32)Maximum(ui->game.editor_loyalty, 0), 100u);
        edit.ambition = detail.ambition; edit.justice = detail.justice; edit.personality = detail.personality; edit.growth = detail.growth;
        edit.female = detail.female;
        edit.merit  = (u16)Minimum((u32)Maximum(ui->game.editor_merit, 0), 65535u);
        edit.captor = ENGINE_NO_FACTION;
        api->set_officer(e, ui->game.editor_officer, &edit);
    }
    ImGui::EndDisabled();
    ImGui::Separator();
    u8 force = api->whose_turn(e);
    ImGui::InputInt(ui_label(label, sizeof(label), ui, "editor.technique", "editor_technique"), &ui->game.editor_technique);
    ImGui::BeginDisabled(force == ENGINE_NO_FACTION);
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "editor.apply_faction", "editor_apply_faction")))
    {
        api->set_technique_points(e, force, (u32)Maximum(ui->game.editor_technique, 0));
    }
    ImGui::EndDisabled();
    ImGui::Separator();
    u8 city = ui->city.selected;
    char name[UI_NAME_SIZE * 2];
    ui_city_label(ui, api, city, name, sizeof(name));
    ImGui::TextUnformatted(name);
    ImGui::InputInt(ui_label(label, sizeof(label), ui, "editor.gold", "editor_gold"), &ui->game.editor_gold);
    ImGui::SliderInt(ui_label(label, sizeof(label), ui, "editor.order", "editor_order"), &ui->game.editor_order, 0, 100);
    ImGui::InputInt(ui_label(label, sizeof(label), ui, "editor.soldiers", "editor_soldiers"), &ui->game.editor_soldiers);
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "editor.apply_city", "editor_apply_city")))
    {
        api->set_city_gold(e, city, (u32)Maximum(ui->game.editor_gold, 0));
        api->set_city_resource(e, city, SIM_RESOURCE_PUBLIC_ORDER, 0, (u32)Maximum(ui->game.editor_order, 0));
        api->set_city_resource(e, city, SIM_RESOURCE_SOLDIERS, 0, (u32)Maximum(ui->game.editor_soldiers, 0));
    }
    ImGui::Separator();
    ImGui::InputInt(ui_label(label, sizeof(label), ui, "editor.unit_soldiers", "editor_unit_soldiers"), &ui->game.editor_unit_soldiers);
    ImGui::SliderInt(ui_label(label, sizeof(label), ui, "editor.unit_morale", "editor_unit_morale"), &ui->game.editor_unit_morale, 0, 120);
    ImGui::BeginDisabled(ui->units.selected == ENGINE_NO_UNIT);
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "editor.apply_unit", "editor_apply_unit")))
    {
        api->set_unit(e, ui->units.selected, (u32)Maximum(ui->game.editor_unit_soldiers, 0), (u32)Maximum(ui->game.editor_unit_morale, 0));
    }
    ImGui::EndDisabled();
    ImGui::End();
}

/* ---- 能力表: 研究能力 and 培育武將 (F-061) ---------------------------------------------------- */

internal void
ui_draw_abilities(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(560, 460), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "abilities.title", "Abilities"), &ui->show_abilities)) { ImGui::End(); return; }
    void *e = api->engine;
    u8 force = api->whose_turn(e);
    if (force == ENGINE_NO_FACTION) { ImGui::TextUnformatted(text_get(ui->text, "court.no_faction")); ImGui::End(); return; }
    engine_faction_chart chart = api->faction_chart(e, force);
    if (chart.job_row != ENGINE_NO_ROW)
    {
        ImGui::Text("%s: %s (%u)", text_get(ui->text, "abilities.running"), ui_text_with_key(ui, "chart", api->chart_row_at(e, chart.job_row).key), chart.turns_left);
    }
    ui_officer_pick(ui, api, "abilities.officer", "trainee", &ui->game.trainee, UI_OFFICERS_OWN, ENGINE_NO_FACTION);
    if (ImGui::BeginTable("chart_table", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn(text_get(ui->text, "abilities.name"));
        ImGui::TableSetupColumn(text_get(ui->text, "abilities.zone"));
        ImGui::TableSetupColumn(text_get(ui->text, "abilities.uses"));
        ImGui::TableSetupColumn(text_get(ui->text, "abilities.research"));
        ImGui::TableSetupColumn(text_get(ui->text, "abilities.train"));
        ImGui::TableHeadersRow();
        for (u32 index = 0; index < api->chart_count(e); ++index)
        {
            engine_chart_row row = api->chart_row_at(e, index);
            char zone[48], button[64];
            text_label(ui->text, "chart_zone", row.zone, zone, sizeof(zone));
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(ui_text_with_key(ui, "chart", row.key));
            ImGui::TableNextColumn(); ImGui::TextUnformatted(zone);
            ImGui::TableNextColumn(); ImGui::Text("%u%s", chart.uses[index], (chart.known >> index) & 1 ? " *" : "");
            ImGui::TableNextColumn();
            snprintf(button, sizeof(button), "%s###research%u", text_get(ui->text, "abilities.research"), index);
            if (ImGui::SmallButton(button)) ui_growth_result(ui, "abilities.research", api->command_research_ability(e, ui->city.selected, (u8)index));
            ImGui::TableNextColumn();
            snprintf(button, sizeof(button), "%s###train%u", text_get(ui->text, "abilities.train"), index);
            ImGui::BeginDisabled(!chart.uses[index] || ui->game.trainee == ENGINE_NO_OFFICER);
            if (ImGui::SmallButton(button)) ui_growth_result(ui, "abilities.train", api->command_train_officer(e, ui->game.trainee, (u8)index));
            ImGui::EndDisabled();
        }
        ImGui::EndTable();
    }
    if (ui->command_message[0]) ImGui::TextWrapped("%s", ui->command_message);
    ImGui::End();
}

/* ---- 情報 (F-143): the faction, its units and corps, and the lists ------------------------- */

internal void
ui_draw_info(ui_state *ui, engine_api *api)
{
    char title[128], label[96];
    ImGui::SetNextWindowSize(ImVec2(460, 420), ImGuiCond_FirstUseEver);
    ui_context_place(ui, 0);   /* the game mode: the dialog opens next to its entity */
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "info.title", "Info"), &ui->show_info)) { ImGui::End(); return; }
    ui_context_track(ui);
    void *e = api->engine;
    u8 force = api->whose_turn(e);
    if (force == ENGINE_NO_FACTION) { ImGui::TextUnformatted(text_get(ui->text, "court.no_faction")); ImGui::End(); return; }
    char name[UI_NAME_SIZE * 2];
    ui_force_text(ui, api, force, name, sizeof(name));
    u32 cities = 0, officers = 0, units = 0, corps = 0;
    for (u32 index = 0; index < api->city_count(e); ++index) cities += api->city_at(e, index).owner_force_id == force;
    for (u32 index = 0; index < api->officer_count(e); ++index) { engine_officer officer = api->officer_at(e, index); officers += officer.force_id == force && officer.status <= 3; }
    for (u32 index = 1; index < SIM_CORPS_CAPACITY; ++index) corps += api->corps_at(e, force, (u8)index).alive;
    engine_rank_kind rank = api->rank_kind_at(e, api->faction_rank(e, force));
    ImGui::Text("%s   %s: %s", name, text_get(ui->text, "info.rank"), ui_text_with_key(ui, "rank", rank.key));
    ImGui::Text("%s %u   %s %u   %s %u   %s %u", text_get(ui->text, "info.cities"), cities, text_get(ui->text, "info.officers"), officers,
                text_get(ui->text, "info.corps"), corps, text_get(ui->text, "info.technique"), api->technique_points(e, force));
    ImGui::SeparatorText(text_get(ui->text, "info.units"));
    for (u32 id = 0; id < api->unit_count(e); ++id)
    {
        engine_unit unit = api->unit_at(e, id);
        if (unit.id == ENGINE_NO_UNIT || unit.force_id != force) continue;
        char text[UI_NAME_SIZE * 4];
        ui_unit_label(ui, api, &unit, text, sizeof(text));
        ImGui::BulletText("%s", text);
        units++;
    }
    if (!units) ImGui::TextDisabled("-");
    ImGui::Separator();
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "info.open_officers", "info_officers"))) ui->show_officers = true;
    ImGui::SameLine();
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "info.open_cities", "info_cities"))) ui->show_cities = true;
    ImGui::SameLine();
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "info.open_bases", "info_bases"))) ui->show_bases = true;
    ImGui::SameLine();
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "info.open_techniques", "info_techniques"))) ui->show_techniques = true;
    ImGui::End();
}

internal void ui_draw_info_menu(ui_state *ui, engine_api *api);   /* ui_information.cpp (M21) */

/* All the game screens; each draws only while it is open. */
internal void
ui_draw_game_screens(ui_state *ui, engine_api *api)
{
    if (ui->show_title)          ui_draw_title(ui, api);
    ui_start_pack_scenario(ui, api);   /* the first frame after the engine started again with another pack */
    if (ui->show_scenarios)      ui_draw_scenarios(ui, api);
    if (ui->show_rulers)         ui_draw_rulers(ui, api);
    if (ui->show_story)          ui_draw_story(ui, api);
    if (ui->show_start_settings) ui_draw_start_settings(ui, api);
    if (ui->show_topbar)         ui_draw_topbar(ui, api);
    if (ui->show_history)        ui_draw_history(ui, api);
    if (ui->show_saveload)       ui_draw_saveload(ui, api);
    if (ui->show_options)        ui_draw_options(ui, api);
    if (ui->show_new_officer)    ui_draw_new_officer(ui, api);
    if (ui->show_editor)         ui_draw_editor(ui, api);
    if (ui->show_abilities)      ui_draw_abilities(ui, api);
    if (ui->show_info && ui->hud_mode) ui_draw_info_menu(ui, api);   /* M21: the column of 14 buttons */
    else if (ui->show_info)            ui_draw_info(ui, api);        /* the debug layout */
}
