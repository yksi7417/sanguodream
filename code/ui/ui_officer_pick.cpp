/* M19 the officer selection window 選擇執行武將 (ui/city-commands.md,
   "Officer selection lists"). The 執行武將 button of a command dialog opens
   it. The window lists the free officers of the city of the dialog in a
   sortable table. A click on a row checks or clears the row. 一併 checks the
   rows from the top, and 一併解除 clears them. 決定 writes the checked
   officers into the slots of the dialog. 返回 keeps the slots. The window
   commands nothing. Only the first tab exists: the other tabs show the
   columns of 武將一覽 (M21). Included by ui.cpp after ui_city_commands.cpp. */

enum ui_picker_column
{
    UI_PICKER_COLUMN_CHECK,
    UI_PICKER_COLUMN_NAME,
    UI_PICKER_COLUMN_CORPS,      /* 軍團: the ruler of the force */
    UI_PICKER_COLUMN_BASE,       /* 所在 */
    UI_PICKER_COLUMN_DAYS,       /* 日數: 0, the free officers stand in the city */
    UI_PICKER_COLUMN_STATUS,     /* 身份 */
    UI_PICKER_COLUMN_LEADERSHIP,
    UI_PICKER_COLUMN_WAR,
    UI_PICKER_COLUMN_INTELLIGENCE,
    UI_PICKER_COLUMN_POLITICS,
    UI_PICKER_COLUMN_CHARM,
    UI_PICKER_COLUMN_COUNT
};

#define UI_PICKER_VISIBLE_ROWS 22   /* the rows of the window of the original */

/* The officer slots of the dialog that opened the window. */
internal u16 *
ui_picker_slots(ui_state *ui, u8 target)
{
    if (target < UI_COMMAND_COUNT) return ui->commands[target].officers;
    return ui->develop.officers;
}

internal b32
ui_picker_is_checked(ui_state *ui, u16 officer_id)
{
    for (u32 index = 0; index < ui->picker.checked_count; ++index)
    {
        if (ui->picker.checked[index] == officer_id) return true;
    }
    return false;
}

/* A click on a row: a checked row clears. An unchecked row checks while the
   command takes more officers. */
internal void
ui_picker_toggle(ui_state *ui, u16 officer_id)
{
    for (u32 index = 0; index < ui->picker.checked_count; ++index)
    {
        if (ui->picker.checked[index] != officer_id) continue;
        for (u32 next = index + 1; next < ui->picker.checked_count; ++next) ui->picker.checked[next - 1] = ui->picker.checked[next];
        ui->picker.checked_count--;
        return;
    }
    if (ui->picker.checked_count < ui->picker.places) ui->picker.checked[ui->picker.checked_count++] = officer_id;
}

/* Opens the window for a dialog. The checked rows are the officers of the
   slots of the dialog. */
internal void
ui_picker_open(ui_state *ui, engine_api *api, u8 target, u8 city, u32 places)
{
    ui->show_picker    = true;
    ui->picker.target  = target;
    ui->picker.city    = city;
    ui->picker.places  = (u8)(places < SIM_DEVELOP_OFFICERS ? places : SIM_DEVELOP_OFFICERS);
    ui->picker.hovered = ENGINE_NO_OFFICER;
    ui->picker.id_count = ui_free_officers(api, city, ui->picker.ids, ArrayCount(ui->picker.ids));
    ui->picker.checked_count = 0;
    u16 *slots = ui_picker_slots(ui, target);
    for (u32 place = 0; place < ui->picker.places; ++place)
    {
        if (slots[place] != ENGINE_NO_OFFICER) ui->picker.checked[ui->picker.checked_count++] = slots[place];
    }
    ui->picker.view.built = false;   /* a new list: build the order again */
}

/* The cells of one row: the text of each column and the number of its sort. */
internal u8
ui_picker_row(ui_state *ui, engine_api *api, void *user, u32 source, ui_sort_key *cells)
{
    (void)user;
    engine_officer officer = api->officer_by_id(api->engine, ui->picker.ids[source]);
    char text[UI_NAME_SIZE * 2];
    ui_key_mixed(cells + UI_PICKER_COLUMN_CHECK, ui_picker_is_checked(ui, officer.officer_id) ? 1 : 0, "");
    ui_officer_name(ui, &officer, text, sizeof(text));
    ui_key_text(cells + UI_PICKER_COLUMN_NAME, text);
    ui_key_code(ui, api, cells + UI_PICKER_COLUMN_CORPS, UI_CODE_FORCE, officer.force_id, UI_NO_ID_8);
    ui_city_label(ui, api, (u8)officer.current_position, text, sizeof(text));
    ui_key_text(cells + UI_PICKER_COLUMN_BASE, text);
    ui_key_number(cells + UI_PICKER_COLUMN_DAYS, 0);
    ui_key_code(ui, api, cells + UI_PICKER_COLUMN_STATUS, UI_CODE_STATUS, officer.status, U32_MAX);
    ui_key_number(cells + UI_PICKER_COLUMN_LEADERSHIP, officer.leadership);
    ui_key_number(cells + UI_PICKER_COLUMN_WAR, officer.war);
    ui_key_number(cells + UI_PICKER_COLUMN_INTELLIGENCE, officer.intelligence);
    ui_key_number(cells + UI_PICKER_COLUMN_POLITICS, officer.politics);
    ui_key_number(cells + UI_PICKER_COLUMN_CHARM, officer.charm);
    return ENGINE_NO_FACTION;
}

internal ui_table_source
ui_picker_source(ui_state *ui)
{
    ui_table_source source = {};
    source.source_count = ui->picker.id_count;
    source.column_count = UI_PICKER_COLUMN_COUNT;
    source.only_force   = ENGINE_NO_FACTION;
    source.row          = ui_picker_row;
    source.rows         = ui->picker.rows;
    source.keys         = ui->picker.keys;
    source.capacity     = ArrayCount(ui->picker.rows);
    return source;
}

/* The columns of the first tab. A click on an ability sorts from the top
   value. 日數 sorts from the lowest (Q-046 keeps the rule open). */
internal void
ui_picker_columns(ui_state *ui)
{
    ImGuiTableColumnFlags down = ImGuiTableColumnFlags_PreferSortDescending;
    ImGui::TableSetupColumn("###pick_check", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed, 24.0f);
    ui_table_column(ui, "picker.officer", "officer");
    ui_table_column(ui, "picker.corps", "corps");
    ui_table_column(ui, "column.location", "base");
    ui_table_column(ui, "picker.days", "days");
    ui_table_column(ui, "picker.status", "status");
    ui_table_column(ui, "picker.leadership", "leadership", down);
    ui_table_column(ui, "picker.war", "war", down);
    ui_table_column(ui, "picker.intelligence", "intelligence", down);
    ui_table_column(ui, "picker.politics", "politics", down);
    ui_table_column(ui, "picker.charm", "charm", down);
}

/* One row. The check mark and the row are one selectable. A checked row is blue. */
internal void
ui_picker_draw_row(ui_state *ui, engine_api *api, u32 source)
{
    u16 officer_id = ui->picker.ids[source];
    ui_sort_key cells[UI_TABLE_MAX_COLUMNS];
    ui_picker_row(ui, api, 0, source, cells);
    b32 checked = ui_picker_is_checked(ui, officer_id);
    char label[64];
    snprintf(label, sizeof(label), "%s###pick_row%u", checked ? "\xE2\x96\xA0" : "\xE2\x96\xA1", officer_id);   /* U+25A0, U+25A1 */
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if (ImGui::Selectable(label, checked != 0, ImGuiSelectableFlags_SpanAllColumns)) ui_picker_toggle(ui, officer_id);
    if (ImGui::IsItemHovered()) ui->picker.hovered = officer_id;
    for (u32 column = UI_PICKER_COLUMN_NAME; column < UI_PICKER_COLUMN_COUNT; ++column)
    {
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(cells[column].text);
    }
}

/* 一併 checks the rows from the top of the order, as many as the command takes. */
internal void
ui_picker_check_all(ui_state *ui)
{
    ui->picker.checked_count = 0;
    for (u32 row = 0; row < ui->picker.view.count && ui->picker.checked_count < ui->picker.places; ++row)
    {
        ui->picker.checked[ui->picker.checked_count++] = ui->picker.ids[ui->picker.rows[row]];
    }
}

/* 決定: the checked officers go into the slots, in the order of the checks. */
internal void
ui_picker_apply(ui_state *ui)
{
    u16 *slots = ui_picker_slots(ui, ui->picker.target);
    for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place)
    {
        slots[place] = place < ui->picker.checked_count ? ui->picker.checked[place] : ENGINE_NO_OFFICER;
    }
    if (ui->picker.target < UI_COMMAND_COUNT) ui->commands[ui->picker.target].prepared_city = ui->picker.city;
    else                                      ui->develop.prepared_city = ui->picker.city;
    ui->show_picker = false;
}

internal void
ui_picker_buttons(ui_state *ui)
{
    char label[96];
    b32 none = ui->picker.checked_count == 0;
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "picker.check_all", "check_all"))) ui_picker_check_all(ui);
    ImGui::SameLine();
    ImGui::BeginDisabled(none);
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "picker.clear_all", "clear_all"))) ui->picker.checked_count = 0;
    ImGui::EndDisabled();
    ImGui::BeginDisabled(none);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.25f, 1.0f));
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "command.decide", "pick_decide"))) ui_picker_apply(ui);
    ImGui::PopStyleColor();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.45f, 0.1f, 1.0f));
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "command.back", "pick_back"))) ui->show_picker = false;
    ImGui::PopStyleColor();
}

/* The title of the first tab: the name of the command. */
internal const char *
ui_picker_tab_text(ui_state *ui)
{
    if (ui->picker.target < UI_COMMAND_COUNT) return text_get(ui->text, ui_command_infos[ui->picker.target].title_id);
    return text_get(ui->text, "develop.title");
}

internal void
ui_draw_picker(ui_state *ui, engine_api *api)
{
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(820.0f, 0.0f), ImGuiCond_Appearing);
    char title[96];
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "picker.title", "OfficerPick"), &ui->show_picker,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
    {
        ui_context_track(ui);
        char tab[96];
        snprintf(tab, sizeof(tab), "%s###pick_tab", ui_picker_tab_text(ui));
        if (ImGui::BeginTabBar("pick_tabs"))
        {
            if (ImGui::BeginTabItem(tab)) ImGui::EndTabItem();
            ImGui::EndTabBar();
        }
        ImGui::BeginGroup();
        ui_draw_officer_face(ui, api, ui->picker.hovered, ImGui::GetTextLineHeightWithSpacing() * UI_SLOT_PORTRAIT_LINES);
        ImGui::NewLine();
        ImGui::EndGroup();
        ImGui::SameLine();
        ui_table_source source = ui_picker_source(ui);
        f32 height = ImGui::GetTextLineHeightWithSpacing() * (UI_PICKER_VISIBLE_ROWS + 2);
        if (ImGui::BeginTable("pick_table", UI_PICKER_COLUMN_COUNT, UI_TABLE_FLAGS & ~ImGuiTableFlags_SortTristate, ImVec2(0.0f, height)))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ui_picker_columns(ui);
            ImGui::TableHeadersRow();
            ui_table_read_sort(&ui->picker.view);
            ui_table_view_update(ui, api, &ui->picker.view, &source);
            for (u32 row = 0; row < ui->picker.view.count; ++row) ui_picker_draw_row(ui, api, ui->picker.rows[row]);
            ImGui::EndTable();
        }
        ui_picker_buttons(ui);
    }
    ImGui::End();
}
