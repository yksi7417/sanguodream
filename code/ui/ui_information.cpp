/* M21 the information menu and its screens (ui/main-map.md "Information
   menu"; ui/information-screens.md). In the game mode the 情報 button of
   the HUD opens a column of 14 buttons. Each button opens its screen:
   - 自軍勢力 and 全勢力: 勢力情報, the faction screen with a faction list;
   - 勢力武將 and 全武將: 武將一覽, the officer table;
   - 勢力都市 and 全都市: 都市一覽, the city table.
   The spec does not describe the other screens ("Not observed"), so their
   buttons are grey. A table sorts on a header click. A row click opens the
   detail panel of the row: 武將情報 or 都市情報. 返回 or a right click
   closes the top window. The 情報 window of the debug layout stays in the
   debug layout (ui_game.cpp). The screens command nothing. Included by
   ui.cpp after ui_hud.cpp. */

enum ui_info_screen
{
    UI_INFO_NONE,
    UI_INFO_FACTION,     /* 勢力情報 */
    UI_INFO_OFFICERS,    /* 武將一覽 */
    UI_INFO_CITIES,      /* 都市一覽 */
};

/* The view tabs of the tables. The spec gives the columns of these tabs. */
enum ui_info_tab
{
    UI_INFO_TAB_BELONG,      /* 武將一覽 所屬 */
    UI_INFO_TAB_ABILITY,     /* 武將一覽 能力 */
    UI_INFO_TAB_SKILL,       /* 武將一覽 特技 */
    UI_INFO_OFFICER_TABS,
    UI_INFO_TAB_STATE = 0,   /* 都市一覽 狀態 */
    UI_INFO_CITY_TABS = 1,
};

struct ui_info_item
{
    const char *text_id;
    const char *stable_id;
    u8          screen;   /* ui_info_screen; UI_INFO_NONE: not observed, grey */
    bool        own;      /* the faction of the player only */
};

/* main-map.md, "Information menu": the items in their order. */
global_variable const ui_info_item ui_info_items[] =
{
    { "info_menu.own_faction",  "im_own_faction",  UI_INFO_FACTION,  true  },
    { "info_menu.own_officers", "im_own_officers", UI_INFO_OFFICERS, true  },
    { "info_menu.own_cities",   "im_own_cities",   UI_INFO_CITIES,   true  },
    { "info_menu.own_ports",    "im_own_ports",    UI_INFO_NONE,     true  },
    { "info_menu.all_officers", "im_all_officers", UI_INFO_OFFICERS, false },
    { "info_menu.all_cities",   "im_all_cities",   UI_INFO_CITIES,   false },
    { "info_menu.all_ports",    "im_all_ports",    UI_INFO_NONE,     false },
    { "info_menu.all_factions", "im_all_factions", UI_INFO_FACTION,  false },
    { "info_menu.all_corps",    "im_all_corps",    UI_INFO_NONE,     false },
    { "info_menu.all_units",    "im_all_units",    UI_INFO_NONE,     false },
    { "info_menu.all_items",    "im_all_items",    UI_INFO_NONE,     false },
    { "info_menu.ranks",        "im_ranks",        UI_INFO_NONE,     false },
    { "info_menu.posts",        "im_posts",        UI_INFO_NONE,     false },
    { "info_menu.skills",       "im_skills",       UI_INFO_NONE,     false },
};

#define UI_INFO_MENU_X        24.0f    /* main-map.md: the column at 24, 112, 110 wide */
#define UI_INFO_MENU_Y        112.0f
#define UI_INFO_MENU_WIDTH    110.0f
#define UI_INFO_TABLE_WIDTH   806.0f   /* information-screens.md: the frame of a table */
#define UI_INFO_TABLE_ROWS    22
#define UI_INFO_LIST_WIDTH    110.0f   /* the list at the left of a detail panel */
#define UI_OFFICER_LIFE_WIDTH_EM 28.0f /* the width of the life of an officer, in the height of the font */

internal void ui_draw_city_game_fields(ui_state *ui, engine_api *api, u8 city_id);   /* ui_city.cpp */

/* Opens a screen of the menu. */
internal void
ui_info_open(ui_state *ui, engine_api *api, u8 screen, bool own)
{
    ui->info.screen  = screen;
    ui->info.own     = own;
    ui->info.tab     = 0;
    ui->info.detail  = false;
    ui->info.faction = api->whose_turn(api->engine);
    ui->info.unit_filter = ENGINE_NO_UNIT;
    ui->info.view    = {};
    ui->info.view.sort_column = UI_SORT_NO_COLUMN;
}

/* The column of 14 buttons, at the left of the screen. */
internal void
ui_draw_info_menu(ui_state *ui, engine_api *api)
{
    ImGui::SetNextWindowPos(ImVec2(UI_INFO_MENU_X, UI_INFO_MENU_Y), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;
    if (ImGui::Begin("###InfoMenu", 0, flags))
    {
        ui_context_track(ui);
        for (u32 index = 0; index < ArrayCount(ui_info_items); ++index)
        {
            const ui_info_item *item = ui_info_items + index;
            char label[96];
            bool grey = item->screen == UI_INFO_NONE;
            ImGui::BeginDisabled(grey);
            bool clicked = ImGui::Button(ui_label(label, sizeof(label), ui, item->text_id, item->stable_id), ImVec2(UI_INFO_MENU_WIDTH, 0.0f));
            ImGui::EndDisabled();
            if (grey && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", text_get(ui->text, "info_menu.not_observed"));
            if (clicked)
            {
                ui_info_open(ui, api, item->screen, item->own);
                ui_context_close(ui);
            }
        }
    }
    ImGui::End();
}

/* M22, screen-flow.md: the popup of a right click on an own unit.
   詳細情報 opens the unit panel. 所屬武將 opens 武將一覽 of the unit. */
internal void
ui_draw_unit_popup(ui_state *ui, engine_api *api)
{
    if (ui_begin_menu_window(ui, "###UnitPopup"))
    {
        u32 unit_id = ui->units.selected;
        f32 x = ui->context.x, y = ui->context.y;
        if (ui_menu_item(ui, "unit_popup.detail", "unit_detail", true))
        {
            ui_context_open(ui, api, UI_CONTEXT_UNIT, x, y);
            ui_select_unit(ui, unit_id);
        }
        if (ui_menu_item(ui, "unit_popup.officers", "unit_officers", true))
        {
            ui_info_open(ui, api, UI_INFO_OFFICERS, false);
            ui->info.unit_filter = unit_id;
            ui_context_close(ui);
            ui_select_unit(ui, ENGINE_NO_UNIT);
        }
    }
    ImGui::End();
}

/* ---- the tables ---------------------------------------------------------- */

/* The rows of the table at this frame: the officers or the cities. */
internal void
ui_info_collect(ui_state *ui, engine_api *api)
{
    void *e = api->engine;
    u8 player = api->whose_turn(e);
    ui->info.id_count = 0;
    if (ui->info.screen == UI_INFO_OFFICERS && ui->info.unit_filter != ENGINE_NO_UNIT)
    {
        /* M22: 所屬武將 of a unit: its commander and its deputies. */
        engine_unit unit = api->unit_at(e, ui->info.unit_filter);
        for (u32 place = 0; place < SIM_COMMAND_OFFICERS && unit.id != ENGINE_NO_UNIT; ++place)
        {
            if (unit.officers[place] != ENGINE_NO_OFFICER) ui->info.ids[ui->info.id_count++] = unit.officers[place];
        }
        return;
    }
    if (ui->info.screen == UI_INFO_OFFICERS)
    {
        u32 count = api->officer_count(e);
        for (u32 index = 0; index < count && ui->info.id_count < ArrayCount(ui->info.ids); ++index)
        {
            engine_officer officer = api->officer_at(e, index);
            bool active = officer.status <= 3, free = officer.status == 6;
            if (ui->info.own ? !(active && officer.force_id == player) : !(active || free)) continue;
            ui->info.ids[ui->info.id_count++] = officer.officer_id;
        }
        return;
    }
    u32 count = api->city_count(e);
    for (u32 index = 0; index < count && ui->info.id_count < ArrayCount(ui->info.ids); ++index)
    {
        engine_city city = api->city_at(e, index);
        if (ui->info.own && city.owner_force_id != player) continue;
        ui->info.ids[ui->info.id_count++] = city.city_id;
    }
}

internal u32
ui_info_column_count(ui_state *ui)
{
    if (ui->info.screen == UI_INFO_CITIES) return 5;
    switch (ui->info.tab)
    {
        case UI_INFO_TAB_BELONG:  return 8;
        case UI_INFO_TAB_ABILITY: return 9;
        default:                  return 2;
    }
}

/* The name of the skill of an officer, or "-". */
internal const char *
ui_info_skill_name(ui_state *ui, engine_api *api, u8 skill)
{
    if (skill == 255 || skill >= api->skill_kind_count(api->engine)) return "-";
    return ui_text_with_key(ui, "skill", api->skill_kind_at(api->engine, skill).key);
}

/* 所在: a base, or the unit of an officer in the field. */
internal void
ui_info_position_text(ui_state *ui, engine_api *api, u16 position, char *out, u32 out_size)
{
    if (position >= UI_HUD_FIRST_UNIT && position != UI_HUD_NO_POSITION)
    {
        engine_unit unit = api->unit_at(api->engine, position - UI_HUD_FIRST_UNIT);
        if (unit.id != ENGINE_NO_UNIT)
        {
            ui_unit_label(ui, api, &unit, out, out_size);
            return;
        }
    }
    ui_code_text(ui, api, UI_CODE_LOCATION, position, out, out_size);
}

/* The cells of an officer row in the columns of the tab. */
internal void
ui_info_officer_cells(ui_state *ui, engine_api *api, u16 officer_id, ui_sort_key *cells)
{
    engine_officer officer = api->officer_by_id(api->engine, officer_id);
    engine_officer_detail detail = api->officer_detail(api->engine, officer_id);
    char name[UI_NAME_SIZE * 2];
    ui_officer_name(ui, &officer, name, sizeof(name));
    ui_key_text(cells + 0, name);
    switch (ui->info.tab)
    {
        case UI_INFO_TAB_BELONG:
            ui_key_code(ui, api, cells + 1, UI_CODE_FORCE, officer.force_id, UI_NO_ID_8);
            ui_key_code(ui, api, cells + 2, UI_CODE_LOCATION, officer.base_location, UI_NO_ID_16);
            ui_info_position_text(ui, api, officer.current_position, name, sizeof(name));
            ui_key_text(cells + 3, name);
            ui_key_code(ui, api, cells + 4, UI_CODE_STATUS, officer.status, U32_MAX);
            ui_key_mixed(cells + 5, officer.status == 2 ? 1 : 0, officer.status == 2 ? "\xE2\x97\x8B" : "\xC3\x97");   /* ○ or × */
            if (officer.status == 0) ui_key_mixed(cells + 6, 101, "---");   /* the ruler has no loyalty */
            else                     ui_key_number(cells + 6, detail.loyalty);
            ui_key_number(cells + 7, officer.merit);
            break;
        case UI_INFO_TAB_ABILITY:
            ui_key_code(ui, api, cells + 1, UI_CODE_STATUS, officer.status, U32_MAX);
            ui_key_number(cells + 2, officer.leadership);
            ui_key_number(cells + 3, officer.war);
            ui_key_number(cells + 4, officer.intelligence);
            ui_key_number(cells + 5, officer.politics);
            ui_key_number(cells + 6, officer.charm);
            ui_key_number(cells + 7, detail.stamina);
            ui_key_number(cells + 8, detail.health);
            break;
        default:
            ui_key_text(cells + 1, ui_info_skill_name(ui, api, officer.skill_number));
            break;
    }
}

internal void
ui_info_city_cells(ui_state *ui, engine_api *api, u8 city_id, ui_sort_key *cells)
{
    engine_city city = api->city_at(api->engine, city_id);
    engine_city_slots slots = api->city_slots(api->engine, city_id);
    char name[UI_NAME_SIZE * 2];
    ui_city_label(ui, api, city_id, name, sizeof(name));
    ui_key_text(cells + 0, name);
    ui_key_code(ui, api, cells + 1, UI_CODE_FORCE, city.owner_force_id, UI_NO_ID_8);
    ui_key_code(ui, api, cells + 2, UI_CODE_OFFICER, city.commanding_officer_id, UI_NO_ID_16);
    ui_key_number(cells + 3, city.public_order);
    ui_key_code(ui, api, cells + 4, UI_CODE_SPECIALTY, slots.specialty, U32_MAX);
}

internal u8
ui_info_row(ui_state *ui, engine_api *api, void *user, u32 source, ui_sort_key *cells)
{
    (void)user;
    if (ui->info.screen == UI_INFO_OFFICERS) ui_info_officer_cells(ui, api, ui->info.ids[source], cells);
    else                                     ui_info_city_cells(ui, api, (u8)ui->info.ids[source], cells);
    return ENGINE_NO_FACTION;
}

/* The headers of the tab: the text id and the stable id of each column. */
internal void
ui_info_columns(ui_state *ui)
{
    static const char *belong[8][2]  = { { "picker.officer", "name" }, { "column.force", "force" }, { "info.col.belong", "belong" },
                                         { "column.location", "location" }, { "picker.status", "status" }, { "info.col.governor", "governor" },
                                         { "info.col.loyalty", "loyalty" }, { "column.merit", "merit" } };
    static const char *ability[9][2] = { { "picker.officer", "name" }, { "picker.status", "status" }, { "picker.leadership", "leadership" },
                                         { "picker.war", "war" }, { "picker.intelligence", "intelligence" }, { "picker.politics", "politics" },
                                         { "picker.charm", "charm" }, { "info.col.stamina", "stamina" }, { "info.col.injury", "injury" } };
    static const char *skill[2][2]   = { { "picker.officer", "name" }, { "info.col.skill", "skill" } };
    static const char *state[5][2]   = { { "column.name", "name" }, { "column.force", "force" }, { "info.col.governor", "governor" },
                                         { "command.order", "order" }, { "city.specialty", "specialty" } };
    const char *(*columns)[2] = ui->info.screen == UI_INFO_CITIES ? state : ui->info.tab == UI_INFO_TAB_BELONG ? belong :
                                ui->info.tab == UI_INFO_TAB_ABILITY ? ability : skill;
    u32 count = ui_info_column_count(ui);
    for (u32 column = 0; column < count; ++column)
    {
        ImGuiTableColumnFlags flags = column >= 2 && ui->info.screen == UI_INFO_OFFICERS && ui->info.tab == UI_INFO_TAB_ABILITY ?
                                      ImGuiTableColumnFlags_PreferSortDescending : 0;
        ui_table_column(ui, columns[column][0], columns[column][1], flags);
    }
}

/* The view tabs. A new tab builds the order again. */
internal void
ui_info_tabs(ui_state *ui)
{
    static const char *officer_tabs[UI_INFO_OFFICER_TABS] = { "info.tab.belong", "info.tab.ability", "info.tab.skill" };
    static const char *officer_keys[UI_INFO_OFFICER_TABS] = { "tab_belong", "tab_ability", "tab_skill" };
    if (!ImGui::BeginTabBar("info_tabs")) return;
    u32 count = ui->info.screen == UI_INFO_CITIES ? UI_INFO_CITY_TABS : UI_INFO_OFFICER_TABS;
    for (u32 tab = 0; tab < count; ++tab)
    {
        char label[96];
        const char *text_id = ui->info.screen == UI_INFO_CITIES ? "info.tab.state" : officer_tabs[tab];
        const char *key     = ui->info.screen == UI_INFO_CITIES ? "tab_state" : officer_keys[tab];
        if (!ImGui::BeginTabItem(ui_label(label, sizeof(label), ui, text_id, key))) continue;
        if (ui->info.tab != (int)tab)
        {
            ui->info.tab = (int)tab;
            ui->info.view = {};
            ui->info.view.sort_column = UI_SORT_NO_COLUMN;
        }
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}

/* 返回: the detail panel closes first, then the table. */
internal void
ui_info_back(ui_state *ui)
{
    if (ui->info.detail) ui->info.detail = false;
    else                 ui->info.screen = UI_INFO_NONE;
}

internal void
ui_info_back_button(ui_state *ui, const char *key)
{
    char label[96];
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.45f, 0.1f, 1.0f));
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "command.back", key))) ui_info_back(ui);
    ImGui::PopStyleColor();
}

internal void
ui_info_center_next(ImVec2 size)
{
    ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
}

internal void
ui_draw_info_table(ui_state *ui, engine_api *api)
{
    ui_info_center_next(ImVec2(UI_INFO_TABLE_WIDTH, 0.0f));
    char title[96];
    const char *title_id = ui->info.screen == UI_INFO_CITIES ? "info.cities_title" : "info.officers_title";
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, title_id, "InfoTable"), 0, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
    {
        ui_info_tabs(ui);
        ui_info_collect(ui, api);
        ui_table_source source = {};
        source.source_count = ui->info.id_count;
        source.column_count = ui_info_column_count(ui);
        source.only_force   = ENGINE_NO_FACTION;
        source.row          = ui_info_row;
        source.rows         = ui->info.rows;
        source.keys         = ui->info.keys;
        source.capacity     = ArrayCount(ui->info.rows);
        char table[32];
        snprintf(table, sizeof(table), "info_table_%u_%d", ui->info.screen, ui->info.tab);
        f32 height = ImGui::GetTextLineHeightWithSpacing() * (UI_INFO_TABLE_ROWS + 2);
        if (ImGui::BeginTable(table, (int)source.column_count, UI_TABLE_FLAGS & ~ImGuiTableFlags_SortTristate, ImVec2(0.0f, height)))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ui_info_columns(ui);
            ImGui::TableHeadersRow();
            ui_table_read_sort(&ui->info.view);
            ui_table_view_update(ui, api, &ui->info.view, &source);
            for (u32 row = 0; row < ui->info.view.count; ++row)
            {
                u32 index = ui->info.rows[row];
                ui_sort_key cells[UI_TABLE_MAX_COLUMNS];
                ui_info_row(ui, api, 0, index, cells);
                char label[UI_SORT_TEXT_SIZE + 24];
                snprintf(label, sizeof(label), "%s###info_row%u", cells[0].text, ui->info.ids[index]);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (ImGui::Selectable(label, ui->info.detail && ui->info.detail_id == ui->info.ids[index], ImGuiSelectableFlags_SpanAllColumns))
                {
                    ui->info.detail    = true;
                    ui->info.detail_id = ui->info.ids[index];
                }
                for (u32 column = 1; column < source.column_count; ++column)
                {
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(cells[column].text);
                }
            }
            ImGui::EndTable();
        }
        ui_info_back_button(ui, "info_back");
    }
    ImGui::End();
}

/* ---- the detail panels --------------------------------------------------- */

/* The list at the left of a detail panel: the rows of the table in its
   order. A click shows that row. */
internal void
ui_info_detail_list(ui_state *ui, engine_api *api)
{
    ImGui::BeginChild("###info_list", ImVec2(UI_INFO_LIST_WIDTH, ImGui::GetTextLineHeightWithSpacing() * UI_INFO_TABLE_ROWS), ImGuiChildFlags_Borders);
    for (u32 row = 0; row < ui->info.view.count; ++row)
    {
        u32 index = ui->info.rows[row];
        if (index >= ui->info.id_count) continue;
        ui_sort_key cells[UI_TABLE_MAX_COLUMNS];
        ui_info_row(ui, api, 0, index, cells);
        char label[UI_SORT_TEXT_SIZE + 24];
        snprintf(label, sizeof(label), "%s###list%u", cells[0].text, ui->info.ids[index]);
        if (ImGui::Selectable(label, ui->info.detail_id == ui->info.ids[index])) ui->info.detail_id = ui->info.ids[index];
    }
    ImGui::EndChild();
}

/* The life of an officer of the pack, under the abilities, and the button of its Wikipedia article.
   An officer without a life shows no text. An officer without an allowed link shows no button.
   The button calls the platform and commands nothing to the engine (docs/EUROPE.md, E4). */
internal void
ui_draw_officer_life(ui_state *ui, const char *officer_key)
{
    const char *life = text_key_lore(ui->text, "officer", officer_key);
    if (life)
    {
        ImGui::Separator();
        f32 width = ImGui::GetFontSize() * UI_OFFICER_LIFE_WIDTH_EM;
        ImGui::BeginChild("###officer_life", ImVec2(width, 0), ImGuiChildFlags_AutoResizeY);
        ImGui::PushTextWrapPos(width);
        ImGui::TextUnformatted(life);
        ImGui::PopTextWrapPos();
        ImGui::EndChild();
    }
    const char *url = text_key_link(ui->text, "officer", officer_key);
    if (url && ui->open_url && text_link_is_allowed(url))
    {
        char label[UI_NAME_SIZE * 2];
        snprintf(label, sizeof(label), "%s###officer_wikipedia", text_get(ui->text, "officer.wikipedia"));
        if (ImGui::Button(label)) ui->open_url(url);
    }
}

internal void
ui_draw_officer_detail(ui_state *ui, engine_api *api)
{
    u16 officer_id = (u16)ui->info.detail_id;
    engine_officer officer = api->officer_by_id(api->engine, officer_id);
    engine_officer_detail detail = api->officer_detail(api->engine, officer_id);
    char name[UI_NAME_SIZE * 2];
    ui_officer_name(ui, &officer, name, sizeof(name));
    ImGui::TextColored(ImVec4(0.62f, 0.82f, 0.96f, 1.0f), "%s", name);
    ui_draw_officer_portrait(ui, api, officer_id);
    ImGui::SameLine();
    ImGui::BeginGroup();
    const char *ids[5] = { "picker.leadership", "picker.war", "picker.intelligence", "picker.politics", "picker.charm" };
    u32 values[5] = { officer.leadership, officer.war, officer.intelligence, officer.politics, officer.charm };
    for (u32 index = 0; index < 5; ++index) ImGui::Text("%s %u", text_get(ui->text, ids[index]), values[index]);
    ImGui::EndGroup();
    ImGui::Separator();
    char force[UI_NAME_SIZE * 2], belong[UI_NAME_SIZE * 2], place[UI_NAME_SIZE * 2], status[UI_NAME_SIZE * 2];
    ui_code_text(ui, api, UI_CODE_FORCE, officer.force_id, force, sizeof(force));
    ui_code_text(ui, api, UI_CODE_LOCATION, officer.base_location, belong, sizeof(belong));
    ui_info_position_text(ui, api, officer.current_position, place, sizeof(place));
    ui_code_text(ui, api, UI_CODE_STATUS, officer.status, status, sizeof(status));
    ImGui::Text("%s %s   %s %s   %s %s   %s %s", text_get(ui->text, "column.force"), force, text_get(ui->text, "info.col.belong"), belong,
                text_get(ui->text, "column.location"), place, text_get(ui->text, "picker.status"), status);
    if (officer.status == 0) ImGui::Text("%s ---   %s %u", text_get(ui->text, "info.col.loyalty"), text_get(ui->text, "column.merit"), officer.merit);
    else ImGui::Text("%s %u   %s %u", text_get(ui->text, "info.col.loyalty"), detail.loyalty, text_get(ui->text, "column.merit"), officer.merit);
    ImGui::Text("%s %s", text_get(ui->text, "info.col.skill"), ui_info_skill_name(ui, api, officer.skill_number));
    ui_draw_officer_life(ui, officer.key);
}

internal void
ui_draw_city_detail(ui_state *ui, engine_api *api)
{
    u8 city_id = (u8)ui->info.detail_id;
    engine_city city = api->city_at(api->engine, city_id);
    if (city.commanding_officer_id != UI_NO_ID_16) ui_draw_officer_portrait(ui, api, city.commanding_officer_id);   /* 太守 */
    ui_draw_city_game_fields(ui, api, city_id);
    char specialty[UI_NAME_SIZE * 2];
    ui_code_text(ui, api, UI_CODE_SPECIALTY, api->city_slots(api->engine, city_id).specialty, specialty, sizeof(specialty));
    ImGui::Text("%s %s", text_get(ui->text, "city.specialty"), specialty);
}

internal void
ui_draw_info_detail(ui_state *ui, engine_api *api)
{
    ui_info_center_next(ImVec2(0.0f, 0.0f));
    char title[96];
    const char *title_id = ui->info.screen == UI_INFO_CITIES ? "info.city_detail" : "info.officer_detail";
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, title_id, "InfoDetail"), 0,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ui_info_detail_list(ui, api);
        ImGui::SameLine();
        ImGui::BeginGroup();
        if (ui->info.screen == UI_INFO_CITIES) ui_draw_city_detail(ui, api);
        else                                   ui_draw_officer_detail(ui, api);
        ImGui::EndGroup();
        ui_info_back_button(ui, "detail_back");
    }
    ImGui::End();
}

/* ---- 勢力情報 --------------------------------------------------------------- */

internal void
ui_draw_faction_info(ui_state *ui, engine_api *api)
{
    void *e = api->engine;
    ui_info_center_next(ImVec2(0.0f, 0.0f));
    char title[96];
    if (!ImGui::Begin(ui_label(title, sizeof(title), ui, "info.faction_title", "FactionInfo"), 0,
                      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        return;
    }
    /* The faction list: the own faction only for 自軍勢力, else every faction. */
    ImGui::BeginChild("###faction_list", ImVec2(UI_INFO_LIST_WIDTH, ImGui::GetTextLineHeightWithSpacing() * UI_INFO_TABLE_ROWS), ImGuiChildFlags_Borders);
    u8 player = api->whose_turn(e);
    for (u32 index = 0; index < api->faction_count(e); ++index)
    {
        engine_faction faction = api->faction_at(e, index);
        if (faction.force_id == ENGINE_NO_FACTION || (ui->info.own && faction.force_id != player)) continue;
        char name[UI_NAME_SIZE * 2], label[UI_NAME_SIZE * 2 + 24];
        ui_force_text(ui, api, faction.force_id, name, sizeof(name));
        snprintf(label, sizeof(label), "%s###faction%u", name, faction.force_id);
        ui_draw_arms_icon(ui, api, faction.force_id);   /* E5 */
        ImGui::SameLine();
        if (ImGui::Selectable(label, ui->info.faction == faction.force_id)) ui->info.faction = faction.force_id;
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginGroup();
    u8 force = ui->info.faction;
    char ruler[UI_NAME_SIZE * 2];
    ui_force_text(ui, api, force, ruler, sizeof(ruler));
    ImGui::ColorButton("###faction_colour", ImGui::ColorConvertU32ToFloat4(ui_faction_color(api, force)), ImGuiColorEditFlags_NoTooltip, ImVec2(14.0f, 14.0f));
    ImGui::SameLine();
    ImGui::Text("%s   %s %u", ruler, text_get(ui->text, "info.technique_points"), force != ENGINE_NO_FACTION ? api->technique_points(e, force) : 0u);
    u16 leader = force != ENGINE_NO_FACTION ? api->force_ruler(e, force) : ENGINE_NO_OFFICER;
    if (leader != ENGINE_NO_OFFICER) ui_draw_officer_portrait(ui, api, leader);   /* 君主 */
    u32 cities = 0, officers = 0, corps = 0;
    u64 gold = 0, grain = 0, soldiers = 0;
    for (u32 index = 0; index < api->city_count(e); ++index)
    {
        engine_city city = api->city_at(e, index);
        if (city.owner_force_id != force) continue;
        cities++; gold += city.gold; grain += city.food; soldiers += city.troops;
    }
    for (u32 index = 0; index < api->officer_count(e); ++index)
    {
        engine_officer officer = api->officer_at(e, index);
        officers += officer.force_id == force && officer.status <= 3;
    }
    for (u32 index = 1; index < SIM_CORPS_CAPACITY && force != ENGINE_NO_FACTION; ++index) corps += api->corps_at(e, force, (u8)index).alive;
    ImGui::Text("%s %u   %s %u   %s %u", text_get(ui->text, "info.corps"), corps, text_get(ui->text, "info.cities"), cities,
                text_get(ui->text, "info.officers"), officers);
    ImGui::Text("%s %llu   %s %llu   %s %llu", text_get(ui->text, "command.gold"), (unsigned long long)gold, text_get(ui->text, "command.grain"),
                (unsigned long long)grain, text_get(ui->text, "command.soldiers"), (unsigned long long)soldiers);
    ImGui::EndGroup();
    ui_info_back_button(ui, "faction_back");
    ImGui::End();
}

/* Each frame: the screen of the menu, and the detail panel over it. A
   right click closes the top window. */
internal void
ui_draw_info_screens(ui_state *ui, engine_api *api)
{
    if (ui->info.screen == UI_INFO_NONE) return;
    ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(0, 0, 0, UI_DIALOG_DIM_ALPHA));
    if (ui->info.screen == UI_INFO_FACTION) ui_draw_faction_info(ui, api);
    else
    {
        ui_draw_info_table(ui, api);
        if (ui->info.detail) ui_draw_info_detail(ui, api);
    }
    if (ui_right_clicked()) ui_info_back(ui);
}
