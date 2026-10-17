/* The panels of M4 (growth): the 研究技巧 dialog (research), the 吸收合併
   dialog (merge), the technique table (技巧表) and the disasters of a city.
   The dialogs have the frame of ui/city-commands.md: the officer slots, the
   cost rows, the period, 決定 and 返回. They ask can_issue each frame; 決定
   is grey when the command cannot run, and one click sends one command.
   Everything goes through engine_api. Included by ui.cpp. */

internal u32
ui_ability_of(engine_officer *officer, u32 ability)
{
    switch (ability)
    {
        case CONTENT_ABILITY_LEADERSHIP: return officer->leadership;
        case CONTENT_ABILITY_FORCE:      return officer->war;
        case CONTENT_ABILITY_INTELLECT:  return officer->intelligence;
        case CONTENT_ABILITY_POLITICS:   return officer->politics;
        case CONTENT_ABILITY_CHARM:      return officer->charm;
        default:                         return 0;
    }
}

internal void
ui_growth_officer_label(ui_state *ui, engine_api *api, u16 officer_id, u32 ability, char *out, u32 out_size)
{
    engine_officer officer = officer_id == ENGINE_NO_OFFICER ? engine_officer{} : api->officer_by_id(api->engine, officer_id);
    if (officer_id == ENGINE_NO_OFFICER || officer.officer_id == ENGINE_NO_OFFICER)
    {
        if (officer_id == ENGINE_NO_OFFICER) snprintf(out, out_size, "%s", text_get(ui->text, "develop.no_officer"));
        else                                 snprintf(out, out_size, "%u", officer_id);
        return;
    }
    char name[UI_NAME_SIZE * 2];
    ui_officer_name(ui, &officer, name, sizeof(name));
    snprintf(out, out_size, "%s (%u)", name, ui_ability_of(&officer, ability));
}

/* One officer slot: a combo of the free officers of the city, with the
   ability that counts for the command. */
internal void
ui_draw_growth_officer(ui_state *ui, engine_api *api, u16 *chosen, u32 place, u32 ability, const u16 *free_ids, u32 free_count)
{
    char label[128], preview[96], combo[128];
    ui_growth_officer_label(ui, api, *chosen, ability, preview, sizeof(preview));
    snprintf(combo, sizeof(combo), "%s %u###officer%u", text_get(ui->text, "develop.officer"), place + 1, place + 1);
    if (ImGui::BeginCombo(combo, preview))
    {
        snprintf(label, sizeof(label), "%s###o_none", text_get(ui->text, "develop.no_officer"));
        if (ImGui::Selectable(label, *chosen == ENGINE_NO_OFFICER)) *chosen = ENGINE_NO_OFFICER;
        for (u32 index = 0; index < free_count; ++index)
        {
            char name[96];
            ui_growth_officer_label(ui, api, free_ids[index], ability, name, sizeof(name));
            snprintf(label, sizeof(label), "%s###o%u", name, free_ids[index]);
            if (ImGui::Selectable(label, *chosen == free_ids[index])) *chosen = free_ids[index];
        }
        ImGui::EndCombo();
    }
}

/* 決定 (green) and 返回 (orange). Returns true for a click of 決定 on a
   command that can run. */
internal b32
ui_draw_decide_back(ui_state *ui, b32 runs, bool *show)
{
    char label[96];
    b32 decided = false;
    ImGui::BeginDisabled(!runs);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.25f, 1.0f));
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "command.decide", "decide"))) decided = true;
    ImGui::PopStyleColor();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.45f, 0.1f, 1.0f));
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "command.back", "back"))) *show = false;
    ImGui::PopStyleColor();
    return decided;
}

internal void
ui_growth_result(ui_state *ui, const char *title_id, engine_command_result result)
{
    if (result.refusal == SIM_REFUSAL_NONE)
    {
        snprintf(ui->command_message, sizeof(ui->command_message), "%s: %s %u", text_get(ui->text, title_id),
                 text_get(ui->text, "command.done"), result.command_id);
    }
    else
    {
        snprintf(ui->command_message, sizeof(ui->command_message), "%s: %s", text_get(ui->text, "develop.refused"),
                 ui_refusal_text(ui, result.refusal));
    }
}

/* ---- 研究技巧 ------------------------------------------------------------- */

internal const char *
ui_technique_name(ui_state *ui, engine_api *api, u8 technique)
{
    engine_technique_kind kind = api->technique_kind_at(api->engine, technique);
    return kind.id == ENGINE_NO_TECHNIQUE ? "-" : ui_text_with_key(ui, "technique", kind.key);
}

/* The tree: one row for each line, one button for each level. A technique
   that the faction has is green; the chosen one is blue. */
internal void
ui_draw_technique_grid(ui_state *ui, engine_api *api, engine_faction_techniques *owned)
{
    u32 count = api->technique_kind_count(api->engine);
    for (u32 line = 0; line < TECHNIQUE_LINE_COUNT; ++line)
    {
        ImGui::Text("%-6s", ui_text_with_key(ui, "line", content_line_name((technique_line)line)));
        for (u32 index = 0; index < count; ++index)
        {
            engine_technique_kind kind = api->technique_kind_at(api->engine, index);
            if (kind.line != line) continue;
            ImGui::SameLine();
            b32 has = (owned->owned >> kind.id) & 1;
            b32 chosen = ui->research.technique == kind.id;
            char label[96];
            snprintf(label, sizeof(label), "%s###t%u", ui_text_with_key(ui, "technique", kind.key), kind.id);
            if (has)         ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.25f, 1.0f));
            else if (chosen) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
            if (ImGui::Button(label)) ui->research.technique = kind.id;
            if (has || chosen) ImGui::PopStyleColor();
        }
    }
}

internal void
ui_research_reset_for_city(ui_state *ui, engine_api *api)
{
    u16 first = ENGINE_NO_OFFICER;
    ui_free_officers(api, ui->research.city, &first, 1);
    ui->research.officers[0]   = first;
    ui->research.officers[1]   = ENGINE_NO_OFFICER;
    ui->research.officers[2]   = ENGINE_NO_OFFICER;
    ui->research.prepared_city = ui->research.city;
}

internal void
ui_draw_research_fields(ui_state *ui, engine_api *api, engine_check *check, engine_faction_techniques *owned, u32 city_gold)
{
    b32 runs = check->refusal == SIM_REFUSAL_NONE;
    ImGui::Text("%s: %u / %u", text_get(ui->text, "research.points"), runs ? check->quantity : 0, owned->technique_points);
    ImGui::Text("%s: %u / %u", text_get(ui->text, "command.gold"), runs ? check->gold : 0, city_gold);
    ImGui::Text("%s: %u%s", text_get(ui->text, "command.period"), runs ? check->days : 0, text_get(ui->text, "command.days"));
    ImGui::Text("%s: %u / %u", text_get(ui->text, "command.action_points"), runs ? check->action_points : 0,
                api->actions_left(api->engine, owned->force_id));
    if (!runs)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "%s: %s", text_get(ui->text, "develop.refused"), ui_refusal_text(ui, check->refusal));
    }
}

internal void
ui_draw_research(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(760, 460), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "research.title", "Research"), &ui->show_research))
    {
        if (ui->research.prepared_city != ui->research.city) ui_research_reset_for_city(ui, api);
        char city_name[96];
        ui_city_label(ui, api, ui->research.city, city_name, sizeof(city_name));
        ImGui::Text("%s: %s", text_get(ui->text, "city.select"), city_name);
        engine_city city = api->city_at(api->engine, ui->research.city);
        engine_faction_techniques owned = api->faction_techniques(api->engine, city.owner_force_id);
        if (owned.researching != ENGINE_NO_TECHNIQUE)
        {
            ImGui::Text("%s: %s (%u %s)", text_get(ui->text, "research.running"), ui_technique_name(ui, api, owned.researching),
                        owned.turns_left, text_get(ui->text, "research.turns"));
        }
        ui_draw_technique_grid(ui, api, &owned);
        engine_technique_kind chosen = api->technique_kind_at(api->engine, ui->research.technique);
        ImGui::Separator();
        ImGui::Text("%s: %s", text_get(ui->text, "research.technique"), ui_technique_name(ui, api, ui->research.technique));

        char label[96];
        ImGui::Button(ui_label(label, sizeof(label), ui, "command.executor", "executor"));
        u16 free_ids[UI_DEVELOP_OFFICER_LIST];
        u32 free_count = ui_free_officers(api, ui->research.city, free_ids, ArrayCount(free_ids));
        u32 ability = chosen.id != ENGINE_NO_TECHNIQUE ? chosen.ability : CONTENT_ABILITY_LEADERSHIP;
        for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place)
        {
            ui_draw_growth_officer(ui, api, ui->research.officers + place, place, ability, free_ids, free_count);
        }

        engine_command command = {};
        command.kind      = ENGINE_COMMAND_RESEARCH;
        command.city_id   = ui->research.city;
        command.technique = ui->research.technique;
        for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place) command.officers[place] = ui->research.officers[place];
        engine_check check = api->can_issue(api->engine, &command);
        ImGui::Separator();
        ui_draw_research_fields(ui, api, &check, &owned, city.gold);
        if (ui_draw_decide_back(ui, check.refusal == SIM_REFUSAL_NONE, &ui->show_research))
        {
            engine_command_result result = api->command_research(api->engine, command.city_id, command.technique, command.officers);
            ui_growth_result(ui, "research.title", result);
            if (result.refusal == SIM_REFUSAL_NONE)
            {
                ui->research.prepared_city = ENGINE_NO_CITY;
                ui->show_research = false;
            }
        }
    }
    ImGui::End();
}

/* ---- 吸收合併 ------------------------------------------------------------- */

internal void
ui_merge_slot_label(ui_state *ui, engine_api *api, u8 city, u8 slot, char *out, u32 out_size)
{
    if (slot == ENGINE_ANY_SLOT)
    {
        snprintf(out, out_size, "-");
        return;
    }
    engine_facility facility = api->facility_at(api->engine, city, slot);
    snprintf(out, out_size, "%u %s Lv%u", slot, ui_facility_name(ui, api, facility.facility), facility.level);
}

/* A combo of the built facilities of the city: 場所 of the merge. */
internal void
ui_draw_merge_slot_combo(ui_state *ui, engine_api *api, const char *text_id, const char *stable_id, char prefix, u8 *chosen)
{
    char label[128], preview[96];
    ui_merge_slot_label(ui, api, ui->merge.city, *chosen, preview, sizeof(preview));
    if (ImGui::BeginCombo(ui_label(label, sizeof(label), ui, text_id, stable_id), preview))
    {
        engine_city_slots slots = api->city_slots(api->engine, ui->merge.city);
        for (u32 index = 0; index < slots.slot_count; ++index)
        {
            engine_facility facility = api->facility_at(api->engine, ui->merge.city, (u8)index);
            if (facility.state != SIM_SLOT_BUILT) continue;
            char name[96];
            ui_merge_slot_label(ui, api, ui->merge.city, (u8)index, name, sizeof(name));
            snprintf(label, sizeof(label), "%s###%c%u", name, prefix, index);
            if (ImGui::Selectable(label, *chosen == index)) *chosen = (u8)index;
        }
        ImGui::EndCombo();
    }
}

internal void
ui_draw_merge(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(460, 320), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "merge.title", "Merge"), &ui->show_merge))
    {
        if (ui->merge.prepared_city != ui->merge.city)
        {
            u16 first = ENGINE_NO_OFFICER;
            ui_free_officers(api, ui->merge.city, &first, 1);
            ui->merge.officer       = first;
            ui->merge.absorber      = ENGINE_ANY_SLOT;
            ui->merge.absorbed      = ENGINE_ANY_SLOT;
            ui->merge.prepared_city = ui->merge.city;
        }
        char city_name[96];
        ui_city_label(ui, api, ui->merge.city, city_name, sizeof(city_name));
        ImGui::Text("%s: %s", text_get(ui->text, "city.select"), city_name);
        ui_draw_merge_slot_combo(ui, api, "merge.absorber", "absorber", 'a', &ui->merge.absorber);
        ui_draw_merge_slot_combo(ui, api, "merge.absorbed", "absorbed", 'b', &ui->merge.absorbed);
        char label[96];
        ImGui::Button(ui_label(label, sizeof(label), ui, "command.executor", "executor"));
        u16 free_ids[UI_DEVELOP_OFFICER_LIST];
        u32 free_count = ui_free_officers(api, ui->merge.city, free_ids, ArrayCount(free_ids));
        ui_draw_growth_officer(ui, api, &ui->merge.officer, 0, CONTENT_ABILITY_POLITICS, free_ids, free_count);

        engine_command command = {};
        command.kind        = ENGINE_COMMAND_MERGE;
        command.city_id     = ui->merge.city;
        command.slot        = ui->merge.absorber;
        command.absorbed    = ui->merge.absorbed;
        command.officers[0] = ui->merge.officer;
        command.officers[1] = ENGINE_NO_OFFICER;
        command.officers[2] = ENGINE_NO_OFFICER;
        engine_check check = api->can_issue(api->engine, &command);
        b32 runs = check.refusal == SIM_REFUSAL_NONE;
        engine_city city = api->city_at(api->engine, ui->merge.city);
        const char *arrow = "\xE2\x86\x92";   /* U+2192 */
        ImGui::Separator();
        if (runs) ImGui::Text("%s: Lv%u %s Lv%u", text_get(ui->text, "merge.level"), check.value_before, arrow, check.value_after);
        else      ImGui::Text("%s: - %s -", text_get(ui->text, "merge.level"), arrow);
        ImGui::Text("%s: %u%s", text_get(ui->text, "command.period"), runs ? check.days : 0, text_get(ui->text, "command.days"));
        ImGui::Text("%s: %u / %u", text_get(ui->text, "command.gold"), runs ? check.gold : 0, city.gold);
        ImGui::Text("%s: %u / %u", text_get(ui->text, "command.action_points"), runs ? check.action_points : 0,
                    city.owner_force_id != ENGINE_NO_FACTION ? api->actions_left(api->engine, city.owner_force_id) : 0);
        if (!runs) ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "%s: %s", text_get(ui->text, "develop.refused"), ui_refusal_text(ui, check.refusal));
        if (ui_draw_decide_back(ui, runs, &ui->show_merge))
        {
            engine_command_result result = api->command_merge(api->engine, command.city_id, command.slot, command.absorbed, command.officers[0]);
            ui_growth_result(ui, "merge.title", result);
            if (result.refusal == SIM_REFUSAL_NONE)
            {
                ui->merge.prepared_city = ENGINE_NO_CITY;
                ui->show_merge = false;
            }
        }
    }
    ImGui::End();
}

/* ---- 技巧表 and the disasters ---------------------------------------------- */

/* The technique table of the faction that acts (or of the owner of the
   city of the City panel): the lines and levels, the techniques that the
   faction has, and the research that runs. */
internal void
ui_draw_techniques(ui_state *ui, engine_api *api)
{
    char title[128];
    ImGui::SetNextWindowSize(ImVec2(720, 330), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "techniques.title", "Techniques"), &ui->show_techniques))
    {
        u8 force = api->whose_turn(api->engine);
        if (force == ENGINE_NO_FACTION && api->city_count(api->engine)) force = api->city_at(api->engine, ui->city.selected).owner_force_id;
        engine_faction_techniques owned = api->faction_techniques(api->engine, force);
        ImGui::Text("%s:", text_get(ui->text, "column.owner"));
        ImGui::SameLine();
        ui_code_cell(ui, api, UI_CODE_FORCE, force, ENGINE_NO_FACTION);
        ImGui::SameLine();
        ImGui::Text("  %s: %u", text_get(ui->text, "research.points"), owned.technique_points);
        if (owned.researching != ENGINE_NO_TECHNIQUE)
        {
            ImGui::Text("%s: %s (%u %s)", text_get(ui->text, "research.running"), ui_technique_name(ui, api, owned.researching),
                        owned.turns_left, text_get(ui->text, "research.turns"));
        }
        ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
        if (ImGui::BeginTable("techniques", 5, flags))
        {
            ui_table_column(ui, "research.line", "line");
            for (u32 level = 1; level <= 4; ++level)
            {
                char id[16];
                snprintf(id, sizeof(id), "lv%u", level);
                ImGui::TableSetupColumn(id);
            }
            ImGui::TableHeadersRow();
            u32 count = api->technique_kind_count(api->engine);
            for (u32 line = 0; line < TECHNIQUE_LINE_COUNT; ++line)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(ui_text_with_key(ui, "line", content_line_name((technique_line)line)));
                for (u32 level = 1; level <= 4; ++level)
                {
                    ImGui::TableNextColumn();
                    for (u32 index = 0; index < count; ++index)
                    {
                        engine_technique_kind kind = api->technique_kind_at(api->engine, index);
                        if (kind.line != line || kind.level != level) continue;
                        b32 has = (owned.owned >> kind.id) & 1;
                        if (has) ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "%s", ui_text_with_key(ui, "technique", kind.key));
                        else     ImGui::TextDisabled("%s", ui_text_with_key(ui, "technique", kind.key));
                    }
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

/* The running disasters of a city, for the City panel. */
internal void
ui_draw_city_disasters(ui_state *ui, engine_api *api, u8 city_id)
{
    engine_city_disasters disasters = api->city_disasters(api->engine, city_id);
    ImGui::Text("%s:", text_get(ui->text, "city.disasters"));
    if (disasters.city_id == ENGINE_NO_CITY || disasters.running == 0)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", text_get(ui->text, "city.no_disaster"));
        return;
    }
    u32 count = api->disaster_kind_count(api->engine);
    for (u32 index = 0; index < count && index < CONTENT_DISASTER_CAPACITY; ++index)
    {
        if (!disasters.turns[index]) continue;
        engine_disaster_kind kind = api->disaster_kind_at(api->engine, index);
        ImGui::SameLine();
        if (disasters.turns[index] == ENGINE_DISASTER_OPEN)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "%s", ui_text_with_key(ui, "disaster", kind.key));
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "%s (%u)", ui_text_with_key(ui, "disaster", kind.key), disasters.turns[index]);
        }
    }
}

/* The buttons of the City panel that open 研究技巧 and 吸收合併 for its city. */
internal void
ui_draw_growth_buttons(ui_state *ui)
{
    char label[96];
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "city.merge", "open_merge")))
    {
        ui->show_merge = true;
        ui->merge.city = ui->city.selected;
    }
    ImGui::SameLine();
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "city.research", "open_research")))
    {
        ui->show_research = true;
        ui->research.city = ui->city.selected;
    }
}

/* The end of the game, for the Simulation panel. */
internal void
ui_draw_outcome(ui_state *ui, engine_api *api)
{
    engine_outcome outcome = api->outcome(api->engine);
    if (outcome.kind == SIM_OUTCOME_NONE)
    {
        ImGui::Text("%s: %u", text_get(ui->text, "outcome.end_year"), outcome.end_year);
        return;
    }
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s:", ui_text_with_key(ui, "outcome", sim_outcome_name((sim_outcome_kind)outcome.kind)));
    ImGui::SameLine();
    ui_code_cell(ui, api, UI_CODE_FORCE, outcome.force_id, ENGINE_NO_FACTION);
    if (outcome.kind == SIM_OUTCOME_VICTORY)
    {
        char ending[64];
        text_label(ui->text, "ending", outcome.ending, ending, sizeof(ending));
        ImGui::TextUnformatted(ending);   /* M9: events.md, endings */
    }
}
