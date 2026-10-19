/* M14 the base command menu of the game mode (ui/city-commands.md, "Base
   command menu"; tutorial 27). The City dialog shows six main items in a
   column. A click on 都市, 軍事 or 人材 opens its submenu at the right. A
   submenu item opens the dialog of its command for the city. 外交, 計略
   and 君主 have no observed submenu: they open their panels at once. An
   item is grey when its command has no api, or when the action points,
   the gold or the technique points refuse it. The debug layout keeps the
   flat buttons of the City panel. Included by ui.cpp. */

enum ui_base_menu
{
    UI_BASE_MENU_NONE,
    UI_BASE_MENU_CITY,       /* 都市 */
    UI_BASE_MENU_MILITARY,   /* 軍事 */
    UI_BASE_MENU_PEOPLE,     /* 人材 */
    UI_BASE_MENU_COUNT
};

enum ui_base_open
{
    UI_BASE_OPEN_DEVELOP,
    UI_BASE_OPEN_MERGE,
    UI_BASE_OPEN_COMMAND,    /* a dialog of ui_command_infos */
    UI_BASE_OPEN_RESEARCH,
    UI_BASE_OPEN_SORTIE,
    UI_BASE_OPEN_TRANSPORT,
    UI_BASE_OPEN_COURT,
    UI_BASE_OPEN_NO_API      /* 移動 and 召喚: the engine has no command yet */
};

struct ui_base_item
{
    u8                  menu;        /* ui_base_menu */
    const char         *text_id;
    const char         *stable_id;
    u8                  open;        /* ui_base_open */
    u8                  ui_kind;     /* UI_BASE_OPEN_COMMAND: the ui_command_kind */
    engine_command_kind probe;       /* the command that can_issue checks; ENGINE_COMMAND_KIND_COUNT for none */
};

/* The submenu items in the order of city-commands.md. */
global_variable const ui_base_item ui_base_items[] =
{
    { UI_BASE_MENU_CITY,     "city.develop",        "base_develop",   UI_BASE_OPEN_DEVELOP,   0,                    ENGINE_COMMAND_DEVELOP },
    { UI_BASE_MENU_CITY,     "city.merge",          "base_merge",     UI_BASE_OPEN_MERGE,     0,                    ENGINE_COMMAND_KIND_COUNT },
    { UI_BASE_MENU_CITY,     "city.conscript",      "base_conscript", UI_BASE_OPEN_COMMAND,   UI_COMMAND_CONSCRIPT, ENGINE_COMMAND_CONSCRIPT },
    { UI_BASE_MENU_CITY,     "city.produce",        "base_produce",   UI_BASE_OPEN_COMMAND,   UI_COMMAND_PRODUCE,   ENGINE_COMMAND_PRODUCE },
    { UI_BASE_MENU_CITY,     "city.patrol",         "base_patrol",    UI_BASE_OPEN_COMMAND,   UI_COMMAND_PATROL,    ENGINE_COMMAND_PATROL },
    { UI_BASE_MENU_CITY,     "city.merchant",       "base_merchant",  UI_BASE_OPEN_COMMAND,   UI_COMMAND_MERCHANT,  ENGINE_COMMAND_MERCHANT },
    { UI_BASE_MENU_CITY,     "city.research",       "base_research",  UI_BASE_OPEN_RESEARCH,  0,                    ENGINE_COMMAND_RESEARCH },
    { UI_BASE_MENU_MILITARY, "city.train",          "base_train",     UI_BASE_OPEN_COMMAND,   UI_COMMAND_TRAIN,     ENGINE_COMMAND_TRAIN },
    { UI_BASE_MENU_MILITARY, "city.sortie",         "base_sortie",    UI_BASE_OPEN_SORTIE,    0,                    ENGINE_COMMAND_SORTIE },
    { UI_BASE_MENU_MILITARY, "city.transport",      "base_transport", UI_BASE_OPEN_TRANSPORT, 0,                    ENGINE_COMMAND_TRANSPORT },
    { UI_BASE_MENU_PEOPLE,   "base_menu.move",      "base_move",      UI_BASE_OPEN_NO_API,    0,                    ENGINE_COMMAND_KIND_COUNT },
    { UI_BASE_MENU_PEOPLE,   "base_menu.summon",    "base_summon",    UI_BASE_OPEN_NO_API,    0,                    ENGINE_COMMAND_KIND_COUNT },
    { UI_BASE_MENU_PEOPLE,   "base_menu.search",    "base_search",    UI_BASE_OPEN_COURT,     0,                    ENGINE_COMMAND_KIND_COUNT },
    { UI_BASE_MENU_PEOPLE,   "base_menu.recruit",   "base_recruit",   UI_BASE_OPEN_COURT,     0,                    ENGINE_COMMAND_KIND_COUNT },
    { UI_BASE_MENU_PEOPLE,   "base_menu.reward",    "base_reward",    UI_BASE_OPEN_COURT,     0,                    ENGINE_COMMAND_KIND_COUNT },
    { UI_BASE_MENU_PEOPLE,   "base_menu.give",      "base_give",      UI_BASE_OPEN_COURT,     0,                    ENGINE_COMMAND_KIND_COUNT },
};

/* Opens the dialog of an item for the selected city. */
internal void
ui_base_open_item(ui_state *ui, const ui_base_item *item, u16 first_officer)
{
    u8 city = ui->city.selected;
    switch (item->open)
    {
        case UI_BASE_OPEN_DEVELOP:
            ui->show_develop = true;
            ui->develop.city = city;
            ui->develop.message[0] = 0;
            break;
        case UI_BASE_OPEN_MERGE:     ui->show_merge = true;     ui->merge.city = city;     break;
        case UI_BASE_OPEN_RESEARCH:  ui->show_research = true;  ui->research.city = city;  break;
        case UI_BASE_OPEN_SORTIE:    ui->show_sortie = true;    ui->sortie.city = city;    break;
        case UI_BASE_OPEN_TRANSPORT: ui->show_transport = true; ui->transport.city = city; break;
        case UI_BASE_OPEN_COMMAND:
            ui->show_command[item->ui_kind] = true;
            ui->commands[item->ui_kind].city = city;
            break;
        case UI_BASE_OPEN_COURT:
            ui->show_court = true;
            if (first_officer != ENGINE_NO_OFFICER) ui->court.officer = first_officer;
            break;
        default: break;
    }
}

/* One main item. Returns true on a click. */
internal b32
ui_base_main_item(ui_state *ui, const char *text_id, const char *stable_id, b32 open)
{
    char label[96];
    if (open) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));   /* the clicked button stays highlighted */
    b32 clicked = ImGui::Button(ui_label(label, sizeof(label), ui, text_id, stable_id), ImVec2(90.0f, 0.0f));
    if (open) ImGui::PopStyleColor();
    return clicked;
}

internal void
ui_draw_base_menu(ui_state *ui, engine_api *api, u16 first_officer)
{
    static const char *main_text[UI_BASE_MENU_COUNT]   = { 0, "base_menu.city", "base_menu.military", "base_menu.people" };
    static const char *main_stable[UI_BASE_MENU_COUNT] = { 0, "menu_city", "menu_military", "menu_people" };
    ImGui::BeginGroup();
    for (u32 menu = UI_BASE_MENU_CITY; menu < UI_BASE_MENU_COUNT; ++menu)
    {
        if (ui_base_main_item(ui, main_text[menu], main_stable[menu], ui->city.menu == menu))
        {
            ui->city.menu = (u8)(ui->city.menu == menu ? UI_BASE_MENU_NONE : menu);
        }
    }
    if (ui_base_main_item(ui, "base_menu.diplomacy", "menu_diplomacy", false)) { ui->show_diplomacy = true; ui->city.menu = UI_BASE_MENU_NONE; }
    if (ui_base_main_item(ui, "base_menu.plot", "menu_plot", false))           { ui->show_diplomacy = true; ui->city.menu = UI_BASE_MENU_NONE; }
    if (ui_base_main_item(ui, "base_menu.ruler", "menu_ruler", false))         { ui->show_ruler = true;     ui->city.menu = UI_BASE_MENU_NONE; }
    ImGui::EndGroup();
    if (ui->city.menu == UI_BASE_MENU_NONE || ui->city.menu >= UI_BASE_MENU_COUNT) return;

    ImGui::SameLine();
    ImGui::BeginGroup();
    for (u32 index = 0; index < ArrayCount(ui_base_items); ++index)
    {
        const ui_base_item *item = ui_base_items + index;
        if (item->menu != ui->city.menu) continue;
        char label[96];
        b32 no_api = item->open == UI_BASE_OPEN_NO_API;
        sim_refusal refusal = item->probe == ENGINE_COMMAND_KIND_COUNT ? SIM_REFUSAL_NONE
                                                                  : ui_probe_city_command(api, ui->city.selected, item->probe, first_officer);
        b32 grey = no_api || ui_refusal_is_cost(refusal);
        ImGui::BeginDisabled(grey);
        b32 clicked = ImGui::Button(ui_label(label, sizeof(label), ui, item->text_id, item->stable_id), ImVec2(110.0f, 0.0f));
        ImGui::EndDisabled();
        if (grey && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip("%s", no_api ? text_get(ui->text, "base_menu.no_api") : ui_refusal_text(ui, refusal));
        }
        if (clicked)
        {
            ui_base_open_item(ui, item, first_officer);
            ui->city.menu = UI_BASE_MENU_NONE;
        }
    }
    ImGui::EndGroup();
}
