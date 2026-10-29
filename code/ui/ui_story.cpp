/* The story mode (docs/EUROPE.md, E7). The pages tell the background of the
   map before the first turn. The first three pages are the world: Europe in
   1200, the authorities and the sparks of the war. Then comes one page for
   each faction that has a row "force.<key>" in story/<locale>.tsv of the
   pack. The text module gives the pages. The panel sends no command to the
   engine. It reads the list of factions from engine_api. The player can skip
   the pages at any time. Included by ui.cpp, before ui_game.cpp. */

#define UI_STORY_WORLD_PAGES 3

internal void ui_center_next_window(void);   /* ui_game.cpp */

global_variable const char *ui_story_world_keys[UI_STORY_WORLD_PAGES] = { "europe", "authorities", "sparks" };

/* True if the pack of the game has story pages. */
internal b32
ui_story_available(ui_state *ui)
{
    return text_key_story(ui->text, "page", ui_story_world_keys[0]) != 0;
}

/* The key of the faction of a page after the world pages, or 0 if the page does not exist. */
internal b32
ui_story_force_page(ui_state *ui, engine_api *api, u32 page, engine_faction *out)
{
    u32 number = UI_STORY_WORLD_PAGES;
    for (u32 index = 0; index < api->faction_count(api->engine); ++index)
    {
        engine_faction faction = api->faction_at(api->engine, index);
        if (!faction.key[0] || !text_key_story(ui->text, "force", faction.key)) continue;
        if (number++ == page) { *out = faction; return true; }
    }
    return false;
}

internal u32
ui_story_page_count(ui_state *ui, engine_api *api)
{
    u32 count = UI_STORY_WORLD_PAGES;
    engine_faction faction;
    while (ui_story_force_page(ui, api, count, &faction)) ++count;
    return count;
}

/* The title and the text of a page. False if the page has no text. */
internal b32
ui_story_page(ui_state *ui, engine_api *api, u32 page, char *title, u32 title_size, const char **body)
{
    title[0] = 0;
    *body    = 0;
    if (page < UI_STORY_WORLD_PAGES)
    {
        const char *name = text_key_story(ui->text, "title", ui_story_world_keys[page]);
        text_copy_utf8(name ? name : ui_story_world_keys[page], title, title_size);
        *body = text_key_story(ui->text, "page", ui_story_world_keys[page]);
        return *body != 0;
    }
    engine_faction faction;
    if (!ui_story_force_page(ui, api, page, &faction)) return false;
    ui_force_text(ui, api, faction.force_id, title, title_size);
    *body = text_key_story(ui->text, "force", faction.key);
    return *body != 0;
}

internal void
ui_draw_story(ui_state *ui, engine_api *api)
{
    char label[96], title[UI_NAME_SIZE * 2];
    ImGui::SetNextWindowSize(ImVec2(560, 420), ImGuiCond_FirstUseEver);
    ui_center_next_window();
    if (!ImGui::Begin(ui_label(label, sizeof(label), ui, "story.title", "Story"), &ui->show_story)) { ImGui::End(); return; }
    u32 count = ui_story_page_count(ui, api);
    if (ui->story.page >= count) ui->story.page = count - 1;
    const char *body;
    if (ui_story_page(ui, api, ui->story.page, title, sizeof(title), &body))
    {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "%s", title);
        ImGui::Separator();
        ImGui::BeginChild("###story_text", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 1.5f));
        ImGui::TextWrapped("%s", body);
        ImGui::EndChild();
    }
    ImGui::Text("%u / %u", ui->story.page + 1, count);
    ImGui::SameLine();
    ImGui::BeginDisabled(ui->story.page == 0);
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "story.previous", "story_previous"))) --ui->story.page;
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(ui->story.page + 1 >= count);
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "story.next", "story_next"))) ++ui->story.page;
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(ui_label(label, sizeof(label), ui, "story.skip", "story_skip"))) ui->show_story = false;
    ImGui::End();
}
