/* The dialog 俘虜處置 (ui/city-commands.md, "Prisoner dialog";
   screen-flow.md: "an enemy unit falls with an officer captured"): after
   a battle, one dialog for each officer that the player captured, with
   the portrait, the abilities, the age and the skill of the officer and
   the buttons 登庸, 俘虜, 釋放 and 處決. A failed 登庸 opens the dialog
   again with 登庸 grey. The UI calls engine_api only. The play report of
   2026-10-23 asked for the dialog. */

/* True when the faction plays by hand. */
internal b32
ui_captive_human(engine_api *api, u8 force)
{
    for (u32 index = 0; index < SIM_FORCE_CAPACITY; ++index)
    {
        engine_faction faction = api->faction_at(api->engine, index);
        if (faction.force_id == ENGINE_NO_FACTION) break;
        if (faction.force_id == force) return faction.human;
    }
    return false;
}

/* Each frame: the new captures of the faction that acts go into the queue,
   and the dialog opens for the first one. The captures of the computer
   factions open no dialog. */
internal void
ui_captive_update(ui_state *ui, engine_api *api)
{
    void *e = api->engine;
    u32 count = api->ledger_count(e);
    if (ui->captive.ledger_mark > count) ui->captive.ledger_mark = 0;   /* a load replaced the ledger */
    u8 player = api->whose_turn(e);
    b32 human = player != ENGINE_NO_FACTION && ui_captive_human(api, player);
    for (u32 index = ui->captive.ledger_mark; index < count; ++index)
    {
        sim_event event = api->ledger_at(e, index);
        if (!human || event.kind != SIM_EVENT_OFFICER_CAPTURED || event.faction != player) continue;
        engine_subject subject = api->subject_of(e, event.subject);
        if (subject.kind != ENGINE_SUBJECT_OFFICER || ui->captive.count >= UI_CAPTIVE_QUEUE) continue;
        ui->captive.queue[ui->captive.count++] = subject.id;
    }
    ui->captive.ledger_mark = count;
    if (ui->captive.count && !ui->show_captive)
    {
        ui->show_captive           = true;
        ui->captive.recruit_failed = false;
    }
}

/* The next captive of the queue shows, or the dialog closes. */
internal void
ui_captive_next(ui_state *ui)
{
    for (u32 index = 1; index < ui->captive.count; ++index) ui->captive.queue[index - 1] = ui->captive.queue[index];
    if (ui->captive.count) ui->captive.count--;
    ui->captive.recruit_failed = false;
    ui->show_captive = ui->captive.count > 0;
}

/* One button of the dialog. Returns true when the player chose it. */
internal b32
ui_captive_button(ui_state *ui, const char *key, b32 enabled)
{
    char label[96], text_id[48];
    snprintf(text_id, sizeof(text_id), "captive.%s", key);
    ImGui::BeginDisabled(!enabled);
    b32 chosen = ImGui::Button(ui_label(label, sizeof(label), ui, text_id, key), ImVec2(90.0f, 0.0f));
    ImGui::EndDisabled();
    ImGui::SameLine();
    return chosen;
}

internal void
ui_draw_captive_dialog(ui_state *ui, engine_api *api)
{
    void *e = api->engine;
    if (!ui->captive.count) { ui->show_captive = false; return; }
    u16 officer_id = ui->captive.queue[0];
    engine_officer officer = api->officer_by_id(e, officer_id);
    engine_officer_detail detail = api->officer_detail(e, officer_id);
    if (officer.officer_id == ENGINE_NO_OFFICER || !detail.captive)   /* gone: an escape, or a release through the court */
    {
        ui_captive_next(ui);
        return;
    }
    char title[128], name[UI_NAME_SIZE * 2];
    ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    /* No close button: the original asks for one of the four choices. */
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "captive.title", "Captive"), 0, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::TextUnformatted(text_get(ui->text, "captive.target"));
        ui_draw_officer_portrait(ui, api, officer_id);
        ImGui::SameLine();
        ImGui::BeginGroup();
        ui_officer_text(ui, api, officer_id, name, sizeof(name));
        ImGui::TextUnformatted(name);
        ImGui::Text("%s %u  %s %u  %s %u", text_get(ui->text, "picker.leadership"), officer.leadership,
                    text_get(ui->text, "picker.war"), officer.war, text_get(ui->text, "picker.intelligence"), officer.intelligence);
        ImGui::Text("%s %u  %s %u", text_get(ui->text, "picker.politics"), officer.politics, text_get(ui->text, "picker.charm"), officer.charm);
        ImGui::Text(text_get(ui->text, "captive.age"), detail.age);
        if (officer.skill_number != 255 && officer.skill_number < api->skill_kind_count(e))
        {
            ImGui::Text("%s: %s", text_get(ui->text, "captive.skill"), ui_text_with_key(ui, "skill", api->skill_kind_at(e, officer.skill_number).key));
        }
        ImGui::EndGroup();
        ImGui::Separator();
        engine_command probe = {};
        probe.kind         = ENGINE_COMMAND_CAPTIVE;
        probe.court.target = officer_id;
        probe.court.kind   = ENGINE_CAPTIVE_RECRUIT;
        engine_check recruit = api->can_issue(e, &probe);
        if (recruit.refusal == SIM_REFUSAL_NONE) ImGui::Text(text_get(ui->text, "captive.chance"), recruit.quantity);
        u8 choice = 255;
        if (ui_captive_button(ui, "recruit", !ui->captive.recruit_failed && recruit.refusal == SIM_REFUSAL_NONE)) choice = ENGINE_CAPTIVE_RECRUIT;
        if (ui_captive_button(ui, "keep", true))    choice = ENGINE_CAPTIVE_KEEP;
        if (ui_captive_button(ui, "release", true)) choice = ENGINE_CAPTIVE_RELEASE;
        if (ui_captive_button(ui, "execute", true)) choice = ENGINE_CAPTIVE_EXECUTE;
        ImGui::NewLine();
        if (choice != 255)
        {
            static const char *ids[] = { "captive.recruit", "captive.keep", "captive.release", "captive.execute" };
            engine_command_result result = api->command_captive(e, officer_id, choice);
            ui_growth_result(ui, ids[choice], result);
            b32 failed_recruit = choice == ENGINE_CAPTIVE_RECRUIT && result.refusal == SIM_REFUSAL_NONE && api->officer_detail(e, officer_id).captive;
            if (failed_recruit) ui->captive.recruit_failed = true;   /* the dialog opens again with 登庸 grey */
            else                ui_captive_next(ui);
        }
    }
    ImGui::End();
}
