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
    UI_BASE_MENU_PLOT,       /* 計略 (G2-065) */
    UI_BASE_MENU_RULER,      /* 君主 (G2-060) */
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
    UI_BASE_OPEN_NO_API,     /* 仲介: the dialog is not built yet */
    UI_BASE_OPEN_MOVE,       /* 移動 (G2-036, ui_people.cpp) */
    UI_BASE_OPEN_SUMMON,     /* 召喚 (G2-037) */
    UI_BASE_OPEN_DIPLOMACY,  /* 二虎競食: the diplomacy panel */
    UI_BASE_OPEN_PLOT,       /* 驅虎吞狼, 流言: the plot dialog; ui_kind is the sim_faction_plot_kind */
    UI_BASE_OPEN_RULER,      /* 評定, 軍團, 軍師: the ruler panel */
    UI_BASE_OPEN_POSTS,      /* 官職 (G2-060) */
    UI_BASE_OPEN_EXILE,      /* 流放 */
    UI_BASE_OPEN_ABILITIES   /* 研究能力, 培育武將: the abilities window */
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
    { UI_BASE_MENU_PEOPLE,   "base_menu.move",      "base_move",      UI_BASE_OPEN_MOVE,      0,                    ENGINE_COMMAND_KIND_COUNT },
    { UI_BASE_MENU_PEOPLE,   "base_menu.summon",    "base_summon",    UI_BASE_OPEN_SUMMON,    0,                    ENGINE_COMMAND_KIND_COUNT },
    { UI_BASE_MENU_PEOPLE,   "base_menu.search",    "base_search",    UI_BASE_OPEN_COURT,     0,                    ENGINE_COMMAND_KIND_COUNT },
    { UI_BASE_MENU_PEOPLE,   "base_menu.recruit",   "base_recruit",   UI_BASE_OPEN_COURT,     0,                    ENGINE_COMMAND_KIND_COUNT },
    { UI_BASE_MENU_PEOPLE,   "base_menu.reward",    "base_reward",    UI_BASE_OPEN_COURT,     0,                    ENGINE_COMMAND_KIND_COUNT },
    { UI_BASE_MENU_PEOPLE,   "base_menu.give",      "base_give",      UI_BASE_OPEN_COURT,     0,                    ENGINE_COMMAND_KIND_COUNT },
    { UI_BASE_MENU_PLOT,     "plot_cmd.two_tigers", "base_two_tigers", UI_BASE_OPEN_DIPLOMACY, 0,                   ENGINE_COMMAND_KIND_COUNT },
    { UI_BASE_MENU_PLOT,     "plot_cmd.drive_tiger", "base_drive_tiger", UI_BASE_OPEN_PLOT,    SIM_PLOT_DRIVE_TIGER, ENGINE_COMMAND_KIND_COUNT },
    { UI_BASE_MENU_PLOT,     "plot_cmd.rumour",     "base_rumour",    UI_BASE_OPEN_PLOT,      SIM_PLOT_RUMOUR,      ENGINE_COMMAND_KIND_COUNT },
    { UI_BASE_MENU_RULER,    "ruler.council",       "base_council",   UI_BASE_OPEN_RULER,     0,                    ENGINE_COMMAND_KIND_COUNT },
    { UI_BASE_MENU_RULER,    "ruler.corps",         "base_corps",     UI_BASE_OPEN_RULER,     0,                    ENGINE_COMMAND_KIND_COUNT },
    { UI_BASE_MENU_RULER,    "ruler.adviser",       "base_adviser",   UI_BASE_OPEN_RULER,     0,                    ENGINE_COMMAND_KIND_COUNT },
    { UI_BASE_MENU_RULER,    "posts.title",         "base_posts",     UI_BASE_OPEN_POSTS,     0,                    ENGINE_COMMAND_KIND_COUNT },
    { UI_BASE_MENU_RULER,    "base_menu.matchmake", "base_matchmake", UI_BASE_OPEN_NO_API,    0,                    ENGINE_COMMAND_KIND_COUNT },
    { UI_BASE_MENU_RULER,    "exile.title",         "base_exile",     UI_BASE_OPEN_EXILE,     0,                    ENGINE_COMMAND_KIND_COUNT },
    { UI_BASE_MENU_RULER,    "base_menu.research_ability", "base_research_ability", UI_BASE_OPEN_ABILITIES, 0,      ENGINE_COMMAND_KIND_COUNT },
    { UI_BASE_MENU_RULER,    "base_menu.train_officer", "base_train_officer", UI_BASE_OPEN_ABILITIES, 0,            ENGINE_COMMAND_KIND_COUNT },
};

/* Opens the dialog of an item for the selected city. */
internal void
ui_base_open_item(ui_state *ui, const ui_base_item *item, u16 first_officer)
{
    u8 city = ui->city.selected;
    switch (item->open)
    {
        /* M19, ui/city-commands.md "Command dialogs": the game fills the
           officer slots with a proposal when the dialog opens. */
        case UI_BASE_OPEN_DEVELOP:
            ui->show_develop = true;
            ui->develop.city = city;
            ui->develop.prepared_city = ENGINE_NO_CITY;
            ui->develop.message[0] = 0;
            break;
        case UI_BASE_OPEN_MERGE:     ui->show_merge = true;     ui->merge.city = city;     break;
        case UI_BASE_OPEN_RESEARCH:  ui->show_research = true;  ui->research.city = city;  break;
        case UI_BASE_OPEN_SORTIE:    ui->show_sortie = true;    ui->sortie.city = city;    break;
        case UI_BASE_OPEN_TRANSPORT: ui->show_transport = true; ui->transport.city = city; break;
        case UI_BASE_OPEN_MOVE:
            ui->show_move_officers = true;
            ui->move.city   = city;
            ui->move.target = ENGINE_NO_CITY;
            ui->move.count  = 0;
            break;
        case UI_BASE_OPEN_SUMMON:    ui->show_summon = true; ui->summon.city = city; ui->summon.count = 0; break;
        case UI_BASE_OPEN_DIPLOMACY: ui->show_diplomacy = true; break;
        case UI_BASE_OPEN_PLOT:
            ui->show_plot        = true;
            ui->plot.city        = city;
            ui->plot.kind        = item->ui_kind;
            ui->plot.target_city = ENGINE_NO_CITY;
            ui->plot.officer     = ENGINE_NO_OFFICER;
            break;
        case UI_BASE_OPEN_RULER:     ui->show_ruler = true; break;
        case UI_BASE_OPEN_POSTS:
            ui->show_posts         = true;
            ui->posts.post         = ENGINE_NO_ROW;
            ui->posts.officer      = ENGINE_NO_OFFICER;
            ui->posts.staged_count = 0;
            break;
        case UI_BASE_OPEN_EXILE:     ui->show_exile = true; ui->exile.officer = ENGINE_NO_OFFICER; break;
        case UI_BASE_OPEN_ABILITIES: ui->show_abilities = true; break;
        case UI_BASE_OPEN_COMMAND:
            ui->show_command[item->ui_kind] = true;
            ui->commands[item->ui_kind].city = city;
            ui->commands[item->ui_kind].prepared_city = ENGINE_NO_CITY;
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

/* An item is grey when its command cannot run now (G2-015, G2-030,
   G2-041): a cost, no free slot, no facility, a value at its maximum, a
   limit of the turn, no free officer, no item or an enemy near. A
   refusal of the probe values (a placeholder facility or weapon) does
   not grey it. */
/* True if an officer of the force, or a city of the force, holds an item. */
internal b32
ui_force_has_item(engine_api *api, u8 force)
{
    void *e = api->engine;
    if (force == ENGINE_NO_FACTION) return false;
    for (u32 index = 0; index < api->item_count(e); ++index)
    {
        engine_item item = api->item_at(e, index);
        if (item.id == ENGINE_NO_ROW) continue;
        if (item.owner != ENGINE_NO_OFFICER && api->officer_by_id(e, item.owner).force_id == force) return true;
        if (item.city != ENGINE_NO_CITY && api->city_at(e, item.city).owner_force_id == force) return true;
    }
    return false;
}

internal b32
ui_refusal_blocks_item(sim_refusal refusal)
{
    return ui_refusal_is_cost(refusal) || refusal == SIM_REFUSAL_NO_SLOT || refusal == SIM_REFUSAL_NO_FACILITY ||
           refusal == SIM_REFUSAL_VALUE_AT_MAXIMUM || refusal == SIM_REFUSAL_LIMIT_REACHED || refusal == SIM_REFUSAL_NO_OFFICER ||
           refusal == SIM_REFUSAL_NO_ITEM || refusal == SIM_REFUSAL_ENEMY_NEAR;
}

internal void
ui_draw_base_menu(ui_state *ui, engine_api *api, u16 first_officer)
{
    /* The six main items in the order of ui/city-commands.md. 外交 opens its
       panel; 計略 and 君主 open a second column (G2-060, G2-065). */
    static const char *main_text[UI_BASE_MENU_COUNT]   = { 0, "base_menu.city", "base_menu.military", "base_menu.people", "base_menu.plot", "base_menu.ruler" };
    static const char *main_stable[UI_BASE_MENU_COUNT] = { 0, "menu_city", "menu_military", "menu_people", "menu_plot", "menu_ruler" };
    ImGui::BeginGroup();
    for (u32 menu = UI_BASE_MENU_CITY; menu < UI_BASE_MENU_COUNT; ++menu)
    {
        if (menu == UI_BASE_MENU_PLOT && ui_base_main_item(ui, "base_menu.diplomacy", "menu_diplomacy", false))
        {
            ui->show_diplomacy = true;
            ui->city.menu = UI_BASE_MENU_NONE;
        }
        if (ui_base_main_item(ui, main_text[menu], main_stable[menu], ui->city.menu == menu))
        {
            ui->city.menu = (u8)(ui->city.menu == menu ? UI_BASE_MENU_NONE : menu);
        }
    }
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
        b32 grey = no_api || ui_refusal_blocks_item(refusal);
        /* G2-041: 授與沒收 is grey when the force holds no item. */
        if (strcmp(item->stable_id, "base_give") == 0) grey = grey || !ui_force_has_item(api, api->city_at(api->engine, ui->city.selected).owner_force_id);
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
