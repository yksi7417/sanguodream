/* The Settings panel: the settings of the front end. The locale is a front
   end setting, so this panel changes it in the text catalog and commands
   nothing in the engine. Included by ui.cpp. */

/* Loads the locale that the Language combo chose in an earlier frame. The
   catalog resets its arena when it loads, so the change happens before the
   panels ask for strings, not between two panels. */
internal void
ui_apply_pending_locale(ui_state *ui)
{
    if (!ui->pending_locale[0]) return;
    if (ui->text) text_catalog_set_locale(ui->text, ui->pending_locale);
    ui->pending_locale[0] = 0;
}

/* The name of a locale in its own language, from locales.tsv, or its code. */
internal const char *
ui_locale_name(text_catalog *text, const char *code)
{
    for (u32 index = 0; index < text->locale_count; ++index)
    {
        if (strcmp(text->locales[index].code, code) == 0) return text->locales[index].name;
    }
    return code;
}

internal void
ui_draw_language_combo(ui_state *ui)
{
    text_catalog *text = ui->text;
    char label[128];
    if (ImGui::BeginCombo(ui_label(label, sizeof(label), ui, "settings.language", "Language"),
                          ui_locale_name(text, text->locale)))
    {
        for (u32 index = 0; index < text->locale_count; ++index)
        {
            text_locale_info *locale = text->locales + index;
            bool selected = strcmp(locale->code, text->locale) == 0;
            snprintf(label, sizeof(label), "%s###%s", locale->name, locale->code);
            if (ImGui::Selectable(label, selected) && !selected)
            {
                snprintf(ui->pending_locale, sizeof(ui->pending_locale), "%s", locale->code);
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

/* docs/EUROPE.md, E3: the Names choice. "English" shows the name by English key. "Own language"
   shows the name in the Latin script of its own language. The choice is one flag of the text
   catalog, so it loads no file and commands nothing in the engine. */
internal void
ui_draw_names_combo(ui_state *ui)
{
    text_catalog *text = ui->text;
    char label[128];
    const bool native = text->native_names != 0;
    const char *shown = native ? text_get(text, "settings.names_native") : text_get(text, "settings.names_english");
    if (ImGui::BeginCombo(ui_label(label, sizeof(label), ui, "settings.names", "Names"), shown))
    {
        snprintf(label, sizeof(label), "%s###names_english", text_get(text, "settings.names_english"));
        if (ImGui::Selectable(label, !native)) text->native_names = false;
        snprintf(label, sizeof(label), "%s###names_native", text_get(text, "settings.names_native"));
        if (ImGui::Selectable(label, native)) text->native_names = true;
        ImGui::EndCombo();
    }
}

/* The game settings of the engine (game-setup-and-modes.md). A change is
   one set_settings call. */
internal void
ui_draw_game_settings(ui_state *ui, engine_api *api)
{
    engine_settings settings = api->settings(api->engine);
    bool disasters = settings.disasters != 0;
    char label[128];
    if (ImGui::Checkbox(ui_label(label, sizeof(label), ui, "settings.disasters", "disasters"), &disasters))
    {
        settings.disasters = disasters;
        api->set_settings(api->engine, settings);
    }
    bool ai = settings.ai != 0, events = settings.events != 0;
    if (ImGui::Checkbox(ui_label(label, sizeof(label), ui, "settings.ai", "ai"), &ai))
    {
        settings.ai = ai;
        api->set_settings(api->engine, settings);
    }
    if (ImGui::Checkbox(ui_label(label, sizeof(label), ui, "settings.events", "events"), &events))
    {
        settings.events = events;
        api->set_settings(api->engine, settings);
    }
}

/* The font size in pixels. The platform layer builds the atlas again when
   it changes (win32_hexdom.cpp); the headless tests only keep the number. */
internal void
ui_draw_font_size(ui_state *ui)
{
    char label[128];
    ImGui::SetNextItemWidth(200.0f);
    ImGui::SliderInt(ui_label(label, sizeof(label), ui, "settings.font_size", "font_size"), &ui->font_size,
                     HEXDOM_FONT_MIN_PIXELS, HEXDOM_FONT_MAX_PIXELS, "%d px");
    if (ui->font_size < HEXDOM_FONT_MIN_PIXELS) ui->font_size = HEXDOM_FONT_MIN_PIXELS;
    if (ui->font_size > HEXDOM_FONT_MAX_PIXELS) ui->font_size = HEXDOM_FONT_MAX_PIXELS;
}

internal void
ui_draw_settings(ui_state *ui, engine_api *api)
{
    char title[128];
    ui_context_place(ui, 0);   /* the game mode: the dialog opens next to its entity */
    /* The window fits its contents, so the Credits button at the bottom shows without a scroll. */
    if (ImGui::Begin(ui_label(title, sizeof(title), ui, "settings.title", "Settings"), &ui->show_settings,
                     ImGuiWindowFlags_AlwaysAutoResize))
    {
        ui_context_track(ui);
        ui_draw_game_settings(ui, api);
        char label[128];   /* M20: the show of the AI round, a setting of the front end */
        ImGui::Checkbox(ui_label(label, sizeof(label), ui, "settings.ai_show", "ai_show"), &ui->round.show);
        /* The duel and debate screens, front end settings. Off: only the result line shows. */
        ImGui::Checkbox(ui_label(label, sizeof(label), ui, "settings.show_duels", "show_duels"), &ui->feel.show_duels);
        ImGui::Checkbox(ui_label(label, sizeof(label), ui, "settings.show_debates", "show_debates"), &ui->feel.show_debates);
        if (ui->text)
        {
            ui_draw_language_combo(ui);
            ui_draw_names_combo(ui);
            if (!text_catalog_has_strings(ui->text))
            {
                ImGui::TextDisabled("%s", text_get(ui->text, "settings.no_strings"));
            }
        }
        ui_draw_font_size(ui);
        ImFont *font = ImGui::GetFont();
        ImGui::Text("%s: %s", text_get(ui->text, "settings.font"), font ? font->GetDebugName() : "-");
        if (ui->text)
        {
            ImGui::Text("%s: %llu", text_get(ui->text, "settings.missing"),
                        (unsigned long long)ui->text->stats.missing_strings);
        }
        ImGui::Separator();
        if (ImGui::Button(ui_label(label, sizeof(label), ui, "settings.credits", "open_credits"))) ui->show_credits = true;
    }
    ImGui::End();
}
