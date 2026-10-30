/* The Credits window: the works of other people that hexdom uses, with
   their authors and licences (docs/ASSETS.md). The Settings dialog opens
   it as its second window. It commands nothing in the engine. Included by
   ui.cpp. */

enum ui_credit_section : u8
{
    UI_CREDIT_ART,
    UI_CREDIT_FONTS,
    UI_CREDIT_CODE,
    UI_CREDIT_DATA,
    UI_CREDIT_TESTS,
    UI_CREDIT_SECTION_COUNT,
};

global_variable const char *ui_credit_section_ids[UI_CREDIT_SECTION_COUNT] = {
    "credits.section.art", "credits.section.fonts", "credits.section.code", "credits.section.data",
    "credits.section.tests",
};

/* One work. A proper name stays the same in each locale. A field that is
   0 comes from the text id "credits.<field>.<id>". The text id
   "credits.use.<id>" says what the game uses the work for. */
struct ui_credit
{
    u8          section;
    const char *id;            /* the stable id of the row; the ui tests find the row by it */
    const char *work;
    const char *author;
    const char *licence;
    const char *licence_url;   /* 0: no link */
    const char *url;           /* the home of the work; 0: no link */
};

global_variable const ui_credit ui_credits[] = {
    {UI_CREDIT_ART, "meshy_city", "Walled Imperial City", "Meshy", "CC BY 4.0",
     "https://creativecommons.org/licenses/by/4.0/", "https://www.meshy.ai"},
    {UI_CREDIT_ART, "own_art", 0, 0, 0, 0, 0},
    {UI_CREDIT_ART, "arms", 0, 0, 0, "https://policies.google.com/terms", 0},   /* docs/EUROPE.md, E5 */
    {UI_CREDIT_ART, "portraits", 0, 0, 0, 0, 0},
    {UI_CREDIT_FONTS, "noto_cjk", "Noto Sans CJK TC", "Google, Adobe", "SIL Open Font License 1.1",
     "https://openfontlicense.org", "https://github.com/notofonts/noto-cjk"},
    {UI_CREDIT_FONTS, "noto_thai", "Noto Sans Thai", "Google", "SIL Open Font License 1.1",
     "https://openfontlicense.org", "https://github.com/notofonts/notofonts.github.io"},
    {UI_CREDIT_CODE, "imgui", "Dear ImGui " IMGUI_VERSION, "Omar Cornut", "MIT License",
     "https://github.com/ocornut/imgui/blob/master/LICENSE.txt", "https://github.com/ocornut/imgui"},
    {UI_CREDIT_CODE, "stb", "stb_truetype, stb_rect_pack, stb_textedit", "Sean Barrett", "MIT License / Public Domain",
     "https://github.com/nothings/stb/blob/master/LICENSE", "https://github.com/nothings/stb"},
    {UI_CREDIT_DATA, "unicode_big5", "BIG5.TXT", "Unicode, Inc.", "Unicode License v3",
     "https://www.unicode.org/license.txt", "https://www.unicode.org/Public/MAPPINGS/OBSOLETE/EASTASIA/OTHER/"},
    {UI_CREDIT_DATA, "natural_earth", "Natural Earth 1:50m", "Natural Earth", "Public Domain",
     "https://www.naturalearthdata.com/about/terms-of-use/", "https://www.naturalearthdata.com"},
    {UI_CREDIT_DATA, "noaa_etopo", "ETOPO 2022 Global Relief Model", "NOAA National Centers for Environmental Information",
     "Public Domain (U.S. Government work)", 0, "https://doi.org/10.25921/fd45-gt74"},
    {UI_CREDIT_TESTS, "imgui_test_engine", "Dear ImGui Test Engine", "Omar Cornut", "Dear ImGui Test Engine License",
     "https://github.com/ocornut/imgui_test_engine/blob/main/imgui_test_engine/LICENSE.txt",
     "https://github.com/ocornut/imgui_test_engine"},
};

/* The field itself, or the text of "credits.<field>.<id>" when it is 0. */
internal const char *
ui_credit_text(ui_state *ui, const char *field_value, const char *field, const char *id)
{
    if (field_value) return field_value;
    char text_id[96];
    snprintf(text_id, sizeof(text_id), "credits.%s.%s", field, id);
    return text_get(ui->text, text_id);
}

internal void
ui_draw_credit(ui_state *ui, const ui_credit *credit)
{
    char label[256];
    snprintf(label, sizeof(label), "%s###credit_%s", ui_credit_text(ui, credit->work, "work", credit->id), credit->id);
    ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_Bullet);
    ImGui::Indent();
    ImGui::TextWrapped("%s", ui_credit_text(ui, 0, "use", credit->id));
    ImGui::TextDisabled("%s: %s", text_get(ui->text, "credits.author"), ui_credit_text(ui, credit->author, "author", credit->id));
    const char *licence = ui_credit_text(ui, credit->licence, "licence", credit->id);
    ImGui::TextDisabled("%s:", text_get(ui->text, "credits.licence"));
    ImGui::SameLine();
    if (credit->licence_url)
    {
        snprintf(label, sizeof(label), "%s###licence_%s", licence, credit->id);
        ImGui::TextLinkOpenURL(label, credit->licence_url);
    }
    else
    {
        ImGui::TextDisabled("%s", licence);
    }
    if (credit->url)
    {
        snprintf(label, sizeof(label), "%s###link_%s", credit->url, credit->id);
        ImGui::TextLinkOpenURL(label, credit->url);
    }
    ImGui::Unindent();
    ImGui::Spacing();
}

internal void
ui_draw_credits(ui_state *ui)
{
    char title[128];
    ImVec2 size(560, 560);
    ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
    ui_context_place_sized(ui, 1, size);   /* the game mode: beside the Settings dialog */
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "credits.title", "Credits"), &ui->show_credits))
    {
        ui_context_track(ui);
        ImGui::TextWrapped("%s", text_get(ui->text, "credits.intro"));
        for (u8 section = 0; section < UI_CREDIT_SECTION_COUNT; ++section)
        {
            ImGui::SeparatorText(text_get(ui->text, ui_credit_section_ids[section]));
            for (u32 index = 0; index < ArrayCount(ui_credits); ++index)
            {
                if (ui_credits[index].section == section) ui_draw_credit(ui, ui_credits + index);
            }
        }
        ImGui::Separator();
        ImGui::TextWrapped("%s", text_get(ui->text, "credits.original"));
    }
    ImGui::End();
}
