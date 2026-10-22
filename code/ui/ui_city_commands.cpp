/* The five city command dialogs of M3 (city-commands.md; ui/city-commands.md):
   巡察 patrol, 徵兵 conscript, 訓練 train, 商人 merchant and 生產兵裝
   produce. Each dialog has the frame of the spec: the 執行武將 officer
   slots, the effect field "current -> result", the 資金 cost/stock row, the
   行動力 cost/available row, and the buttons 決定 and 返回. The dialog asks
   can_issue each frame. 決定 is grey when the command cannot run; one click
   sends one command and closes the dialog. 返回 closes it and sends
   nothing. The dialogs read the game only through engine_api. Included by
   ui.cpp, after ui_city.cpp. */

struct ui_command_info
{
    const char         *window;       /* the stable id of the window */
    const char         *title_id;     /* the text id of the title */
    const char         *open_id;      /* the stable id of the button in the City panel */
    const char         *button_id;    /* the text id of that button */
    const char         *value_id;     /* the text id of the effect field */
    engine_command_kind kind;
    u32                 max_officers;
};

global_variable ui_command_info ui_command_infos[UI_COMMAND_COUNT] = {
    { "Patrol",    "patrol.title",    "open_patrol",    "city.patrol",    "command.order",    ENGINE_COMMAND_PATROL,    3 },
    { "Conscript", "conscript.title", "open_conscript", "city.conscript", "command.soldiers", ENGINE_COMMAND_CONSCRIPT, 3 },
    { "Train",     "train.title",     "open_train",     "city.train",     "command.morale",   ENGINE_COMMAND_TRAIN,     3 },
    { "Merchant",  "merchant.title",  "open_merchant",  "city.merchant",  "command.grain",    ENGINE_COMMAND_MERCHANT,  1 },
    { "Produce",   "produce.title",   "open_produce",   "city.produce",   "command.stock",    ENGINE_COMMAND_PRODUCE,   3 },
};

#define UI_MERCHANT_DEFAULT_AMOUNT 1000

internal void ui_picker_open(ui_state *ui, engine_api *api, u8 target, u8 city, u32 places);   /* ui_officer_pick.cpp (M19) */

/* The ability that decides the effect of the command (city-commands.md). */
internal u32
ui_command_ability(engine_officer *officer, u32 kind)
{
    switch (kind)
    {
        case UI_COMMAND_PATROL:    return officer->leadership;
        case UI_COMMAND_CONSCRIPT: return officer->charm;
        case UI_COMMAND_TRAIN:     return officer->war;
        case UI_COMMAND_MERCHANT:  return officer->politics;
        case UI_COMMAND_PRODUCE:   return officer->intelligence;
        default:                   return 0;
    }
}

internal void
ui_command_officer_label(ui_state *ui, engine_api *api, u16 officer_id, u32 kind, char *out, u32 out_size)
{
    if (officer_id == ENGINE_NO_OFFICER)
    {
        snprintf(out, out_size, "%s", text_get(ui->text, "develop.no_officer"));
        return;
    }
    engine_officer officer = api->officer_by_id(api->engine, officer_id);
    if (officer.officer_id == ENGINE_NO_OFFICER)
    {
        snprintf(out, out_size, "%u", officer_id);
        return;
    }
    char name[UI_NAME_SIZE * 2];
    ui_officer_name(ui, &officer, name, sizeof(name));
    snprintf(out, out_size, "%s (%u)", name, ui_command_ability(&officer, kind));
}

/* One of the 執行武將 slots: a combo of the free officers of the city. */
internal void
ui_draw_command_officer(ui_state *ui, engine_api *api, u32 kind, u32 place, const u16 *free_ids, u32 free_count)
{
    ui_command_dialog *dialog = ui->commands + kind;
    char label[128], preview[96], combo[128];
    ui_command_officer_label(ui, api, dialog->officers[place], kind, preview, sizeof(preview));
    snprintf(combo, sizeof(combo), "%s %u###officer%u", text_get(ui->text, "develop.officer"), place + 1, place + 1);
    ui_draw_officer_face(ui, api, dialog->officers[place], ImGui::GetFrameHeight());   /* M19 */
    if (ImGui::BeginCombo(combo, preview))
    {
        snprintf(label, sizeof(label), "%s###o_none", text_get(ui->text, "develop.no_officer"));
        if (ImGui::Selectable(label, dialog->officers[place] == ENGINE_NO_OFFICER)) dialog->officers[place] = ENGINE_NO_OFFICER;
        for (u32 index = 0; index < free_count; ++index)
        {
            char name[96];
            ui_command_officer_label(ui, api, free_ids[index], kind, name, sizeof(name));
            snprintf(label, sizeof(label), "%s###o%u", name, free_ids[index]);
            ui_draw_officer_face(ui, api, free_ids[index], ImGui::GetTextLineHeight() * 1.5f);
            if (ImGui::Selectable(label, dialog->officers[place] == free_ids[index])) dialog->officers[place] = free_ids[index];
        }
        ImGui::EndCombo();
    }
}

/* The ability that ranks the proposal of a command (ui_propose_officers):
   the ability of ui_command_ability. The skill 名聲 of 徵兵 does not count
   yet. */
internal u32
ui_command_ability_kind(u32 kind)
{
    switch (kind)
    {
        case UI_COMMAND_PATROL:    return UI_ABILITY_LEADERSHIP;
        case UI_COMMAND_CONSCRIPT: return UI_ABILITY_CHARM;
        case UI_COMMAND_TRAIN:     return UI_ABILITY_WAR;
        case UI_COMMAND_MERCHANT:  return UI_ABILITY_POLITICS;
        default:                   return UI_ABILITY_INTELLIGENCE;
    }
}

/* The dialog keeps its choices while it shows one city. For a new city,
   or after a command, it proposes the most suitable officers (M19). */
internal void
ui_command_reset_for_city(ui_state *ui, engine_api *api, u32 kind)
{
    ui_command_dialog *dialog = ui->commands + kind;
    dialog->officers[0] = dialog->officers[1] = dialog->officers[2] = ENGINE_NO_OFFICER;
    u32 places = ui_command_infos[kind].max_officers < 3 ? ui_command_infos[kind].max_officers : 3;
    ui_propose_officers(api, dialog->city, ui_command_ability_kind(kind), dialog->officers, places);
    dialog->prepared_city = dialog->city;
    dialog->trade  = 0;   /* G2-031: the slider starts at the centre with no trade */
    dialog->amount = 0;
}

/* The current value that the command changes, from the city queries. */
internal u32
ui_command_current_value(engine_api *api, u32 kind, engine_city_economy *economy, u8 weapon)
{
    switch (kind)
    {
        case UI_COMMAND_PATROL:    return economy->public_order;
        case UI_COMMAND_CONSCRIPT: return economy->soldiers;
        case UI_COMMAND_TRAIN:     return economy->morale;
        case UI_COMMAND_MERCHANT:  return economy->grain;
        case UI_COMMAND_PRODUCE:
        {
            engine_weapon_kind row = api->weapon_kind_at(api->engine, weapon);
            return row.id != ENGINE_NO_WEAPON && row.stock < CITY_EQUIPMENT_STOCK_COUNT ? economy->stocks[row.stock] : 0;
        }
        default:                   return 0;
    }
}

/* The 兵裝 rows of 生產兵裝: one button for each weapon with its cost, and
   the 庫存 (stock) of each weapon. */
internal void
ui_draw_weapon_rows(ui_state *ui, engine_api *api, engine_city_economy *economy)
{
    ui_command_dialog *dialog = ui->commands + UI_COMMAND_PRODUCE;
    ImGui::TextUnformatted(text_get(ui->text, "command.weapons"));
    u32 count = api->weapon_kind_count(api->engine);
    const char *selected_key = 0;
    for (u32 index = 0; index < count; ++index)
    {
        engine_weapon_kind kind = api->weapon_kind_at(api->engine, index);
        /* G2-029: the true cost of the city (a specialty city pays less), and a grey item
           when the city has no facility for it or the force lacks its technique. */
        engine_command probe = {};
        probe.kind    = ENGINE_COMMAND_PRODUCE;
        probe.city_id = dialog->city;
        probe.weapon  = kind.id;
        for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place) probe.officers[place] = dialog->officers[place];
        engine_check check = api->can_issue(api->engine, &probe);
        b32 blocked = check.refusal == SIM_REFUSAL_NO_FACILITY || check.refusal == SIM_REFUSAL_NEEDS_TECHNIQUE;
        u32 cost = check.refusal == SIM_REFUSAL_NONE ? check.gold : kind.cost;
        char label[96];
        snprintf(label, sizeof(label), "%s %u###w%u", ui_text_with_key(ui, "weapon", kind.key), cost, kind.id);
        if (index % 4 != 0) ImGui::SameLine();
        bool selected = dialog->weapon == kind.id;
        if (selected) selected_key = kind.key;
        if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
        ImGui::BeginDisabled(blocked);
        if (ImGui::Button(label)) dialog->weapon = kind.id;
        ImGui::EndDisabled();
        if (selected) ImGui::PopStyleColor();
    }
    ImGui::Text("%s:", text_get(ui->text, "command.stock"));
    for (u32 index = 0; index < count; ++index)
    {
        engine_weapon_kind kind = api->weapon_kind_at(api->engine, index);
        if (index % 5 != 0) ImGui::SameLine();
        ImGui::Text("%s %u", ui_text_with_key(ui, "weapon", kind.key), kind.stock < CITY_EQUIPMENT_STOCK_COUNT ? economy->stocks[kind.stock] : 0);
    }
    if (selected_key)
    {
        /* G2-029: the 效果 line of the weapon, when the text table has one. */
        char effect_id[64];
        snprintf(effect_id, sizeof(effect_id), "weapon_effect.%s", selected_key);
        const char *effect = text_table_find(&ui->text->strings, effect_id);
        if (!effect) effect = text_table_find(&ui->text->fallback_strings, effect_id);
        if (effect) ImGui::Text("%s: %s", text_get(ui->text, "command.weapon_effect"), effect);
    }
}

/* The merchant: buy or sell, the amount and the city price. */
internal void
ui_draw_merchant_order(ui_state *ui, engine_city_economy *economy)
{
    ui_command_dialog *dialog = ui->commands + UI_COMMAND_MERCHANT;
    char label[96];
    /* G2-031: one slider from the full sale (left) to the full purchase (right). The centre
       trades nothing, and 決定 is grey there (the check refuses no amount). */
    s32 limit = (s32)Maximum(Minimum(economy->grain, 100000u), Minimum(economy->gold, 100000u));
    if (limit < 1) limit = 1;
    if (dialog->trade < -limit) dialog->trade = -limit;
    if (dialog->trade > limit)  dialog->trade = limit;
    snprintf(label, sizeof(label), "%s###trade", text_get(ui->text, "command.amount"));
    ImGui::SetNextItemWidth(260.0f);
    ImGui::SliderInt(label, &dialog->trade, -limit, limit);
    dialog->merchant_mode = dialog->trade < 0 ? SIM_MERCHANT_SELL : SIM_MERCHANT_BUY;
    dialog->amount = dialog->trade < 0 ? (int)Minimum((u32)(-dialog->trade), economy->grain) : (int)Minimum((u32)dialog->trade, economy->gold);
    ImGui::Text("%s: %d", text_get(ui->text, dialog->trade < 0 ? "command.sell" : "command.buy"), dialog->amount);
    ImGui::Text("%s: %s%u=%s1", text_get(ui->text, "command.price"), text_get(ui->text, "command.grain"), economy->grain_per_gold,
                text_get(ui->text, "command.gold"));
}

internal engine_command_result
ui_issue_city_command(engine_api *api, u32 kind, engine_command *command)
{
    switch (kind)
    {
        case UI_COMMAND_PATROL:    return api->command_patrol(api->engine, command->city_id, command->officers);
        case UI_COMMAND_CONSCRIPT: return api->command_conscript(api->engine, command->city_id, command->officers);
        case UI_COMMAND_TRAIN:     return api->command_train(api->engine, command->city_id, command->officers);
        case UI_COMMAND_MERCHANT:  return api->command_merchant(api->engine, command->city_id, command->officers[0],
                                                                command->merchant_mode, command->amount);
        case UI_COMMAND_PRODUCE:   return api->command_produce(api->engine, command->city_id, command->weapon, command->officers);
        default:
        {
            engine_command_result none = {};
            none.refusal = SIM_REFUSAL_NO_CITY;
            return none;
        }
    }
}

/* The fields of the frame: effect, 資金, 期間, 行動力, or the refusal. */
internal void
ui_draw_command_fields(ui_state *ui, engine_api *api, u32 kind, engine_city_economy *economy, engine_check *check, u32 current)
{
    ui_command_info *info = ui_command_infos + kind;
    b32 runs = check->refusal == SIM_REFUSAL_NONE;
    const char *arrow = "\xE2\x86\x92";   /* U+2192 */
    if (runs) ImGui::Text("%s: %u %s %u", text_get(ui->text, info->value_id), current, arrow, check->value_after);
    else      ImGui::Text("%s: %u %s -", text_get(ui->text, info->value_id), current, arrow);
    if (kind == UI_COMMAND_MERCHANT && runs)
    {
        /* G2-025: the trade effect of the officer, as the original shows it (交易效果 93%). */
        ImGui::Text("%s: %u%%", text_get(ui->text, "command.effect"), check->effect_percent);
    }
    if (kind == UI_COMMAND_MERCHANT && runs && check->gold_gain)
    {
        ImGui::Text("%s: +%u / %u", text_get(ui->text, "command.gold"), check->gold_gain, economy->gold);
    }
    else if (kind != UI_COMMAND_TRAIN)
    {
        ImGui::Text("%s: %u / %u", text_get(ui->text, "command.gold"), runs ? check->gold : 0, economy->gold);
    }
    if (kind == UI_COMMAND_PRODUCE)
    {
        ImGui::Text("%s: %u%s", text_get(ui->text, "command.period"), runs ? check->days : 0, text_get(ui->text, "command.days"));
    }
    engine_city city = api->city_at(api->engine, ui->commands[kind].city);
    u32 available = city.owner_force_id != ENGINE_NO_FACTION ? api->actions_left(api->engine, city.owner_force_id) : 0;
    ImGui::Text("%s: %u / %u", text_get(ui->text, "command.action_points"), runs ? check->action_points : 0, available);
    if (!runs)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "%s: %s", text_get(ui->text, "develop.refused"), ui_refusal_text(ui, check->refusal));
    }
}

/* 決定 (green) and 返回 (orange). */
internal void
ui_draw_command_buttons(ui_state *ui, engine_api *api, u32 kind, engine_command *command, engine_check *check)
{
    char label[96];
    ImGui::BeginDisabled(check->refusal != SIM_REFUSAL_NONE);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.25f, 1.0f));
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "command.decide", "decide")))
    {
        engine_command_result result = ui_issue_city_command(api, kind, command);
        if (result.refusal == SIM_REFUSAL_NONE)
        {
            snprintf(ui->command_message, sizeof(ui->command_message), "%s: %s %u", text_get(ui->text, ui_command_infos[kind].title_id),
                     text_get(ui->text, "command.done"), result.command_id);
            ui->commands[kind].prepared_city = ENGINE_NO_CITY;
            ui->show_command[kind] = false;
        }
        else
        {
            snprintf(ui->command_message, sizeof(ui->command_message), "%s: %s", text_get(ui->text, "develop.refused"),
                     ui_refusal_text(ui, result.refusal));
        }
    }
    ImGui::PopStyleColor();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.45f, 0.1f, 1.0f));
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "command.back", "back"))) ui->show_command[kind] = false;
    ImGui::PopStyleColor();
}

internal void
ui_draw_city_command(ui_state *ui, engine_api *api, u32 kind)
{
    ui_command_info *info = ui_command_infos + kind;
    ui_command_dialog *dialog = ui->commands + kind;
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(440, kind == UI_COMMAND_PRODUCE ? 420.0f : 300.0f), ImGuiCond_FirstUseEver);
    ImGuiIO &io = ImGui::GetIO();
    if (ui->hud_mode)
    {
        /* M19, ui/city-commands.md "Command dialogs": the dialog sits at a
           fixed place in the centre, and the map behind it is dark. */
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), io.DisplaySize, IM_COL32(0, 0, 0, UI_DIALOG_DIM_ALPHA));
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.45f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));   /* M22: rule 6 */
    }
    ImGuiWindowFlags flags = ui->hud_mode ? ImGuiWindowFlags_AlwaysAutoResize : 0;
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, info->title_id, info->window), &ui->show_command[kind], flags))
    {
        ui_context_track(ui);
        if (dialog->prepared_city != dialog->city) ui_command_reset_for_city(ui, api, kind);
        char city_name[96];
        ui_city_label(ui, api, dialog->city, city_name, sizeof(city_name));
        ImGui::Text("%s: %s", text_get(ui->text, "city.select"), city_name);
        engine_city_economy economy = api->city_economy(api->engine, dialog->city);
        if (kind == UI_COMMAND_PRODUCE)  ui_draw_weapon_rows(ui, api, &economy);
        if (kind == UI_COMMAND_MERCHANT) ui_draw_merchant_order(ui, &economy);

        /* 執行武將: the button opens the officer selection (M19); the combos
           also choose the officers. */
        char label[96];
        if (ImGui::Button(ui_label(label, sizeof(label), ui, "command.executor", "executor")))
        {
            ui_picker_open(ui, api, (u8)kind, dialog->city, info->max_officers);
        }
        u16 free_ids[UI_DEVELOP_OFFICER_LIST];
        u32 free_count = ui_free_officers(api, dialog->city, free_ids, ArrayCount(free_ids));
        /* M19: the portrait slots of the chosen officers, as in the original. */
        for (u32 place = 0; place < info->max_officers; ++place)
            ui_draw_officer_face(ui, api, dialog->officers[place], ImGui::GetTextLineHeightWithSpacing() * UI_SLOT_PORTRAIT_LINES);
        ImGui::NewLine();
        for (u32 place = 0; place < info->max_officers; ++place) ui_draw_command_officer(ui, api, kind, place, free_ids, free_count);

        engine_command command = {};
        command.kind          = info->kind;
        command.city_id       = dialog->city;
        command.weapon        = dialog->weapon;
        command.merchant_mode = (u8)dialog->merchant_mode;
        command.amount        = (u32)(dialog->amount > 0 ? dialog->amount : 0);
        for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place)
        {
            command.officers[place] = place < info->max_officers ? dialog->officers[place] : ENGINE_NO_OFFICER;
        }
        engine_check check = api->can_issue(api->engine, &command);
        ImGui::Separator();
        ui_draw_command_fields(ui, api, kind, &economy, &check, ui_command_current_value(api, kind, &economy, dialog->weapon));
        ui_draw_command_buttons(ui, api, kind, &command, &check);
    }
    ImGui::End();
}

/* A trial order for a button of the City panel: the first free officer
   and the first row of each table. The answer of can_issue says whether
   the action points or the gold allow the command at all; the dialog then
   checks the real order. */
internal sim_refusal
ui_probe_city_command(engine_api *api, u8 city, engine_command_kind kind, u16 first_officer)
{
    engine_command command = {};
    command.kind    = kind;
    command.city_id = city;
    command.slot    = ENGINE_ANY_SLOT;
    for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place) command.officers[place] = ENGINE_NO_OFFICER;
    command.officers[0] = first_officer;
    switch (kind)
    {
        case ENGINE_COMMAND_DEVELOP:  command.facility = api->facility_kind_at(api->engine, 0).id; break;
        case ENGINE_COMMAND_PRODUCE:  command.weapon = api->weapon_kind_at(api->engine, 0).id; break;
        case ENGINE_COMMAND_RESEARCH: command.technique = api->technique_kind_at(api->engine, 0).id; break;
        case ENGINE_COMMAND_MERCHANT:
        {
            command.merchant_mode = SIM_MERCHANT_BUY;
            command.amount        = UI_MERCHANT_DEFAULT_AMOUNT;
            break;
        }
        case ENGINE_COMMAND_SORTIE:
        case ENGINE_COMMAND_TRANSPORT:
        {
            command.sortie.city_id = city;
            for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place) command.sortie.officers[place] = ENGINE_NO_OFFICER;
            command.sortie.officers[0] = first_officer;
            command.sortie.soldiers    = 1;
            command.sortie.destination = city;
            break;
        }
        case ENGINE_COMMAND_MERGE: return SIM_REFUSAL_NONE;   /* the two slots are the choice of the dialog */
        default: break;
    }
    return api->can_issue(api->engine, &command).refusal;
}

/* The buttons of the City panel that open the five dialogs for its city. */
internal void
ui_draw_city_command_buttons(ui_state *ui, engine_api *api, u16 first_officer)
{
    for (u32 kind = 0; kind < UI_COMMAND_COUNT; ++kind)
    {
        ui_command_info *info = ui_command_infos + kind;
        if (kind > 0) ImGui::SameLine();
        if (ui_city_button(ui, info->button_id, info->open_id,
                           ui_probe_city_command(api, ui->city.selected, info->kind, first_officer)))
        {
            ui->show_command[kind] = true;
            ui->commands[kind].city = ui->city.selected;
        }
    }
    if (ui->command_message[0]) ImGui::TextUnformatted(ui->command_message);
}
