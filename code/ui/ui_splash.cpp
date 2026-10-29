/* The splash art before the title menu (docs/ASSETS.md, "The splash art").
   The game opens the splash one time, at the start (hexdom.cpp). The splash
   closes after UI_SPLASH_SECONDS, on a click or on a key. Then the title
   menu opens. The platform gives the picture (splash_picture). Without
   the picture, the splash shows a dark screen with the hint. The splash
   calls no function of engine_api. */

internal void
ui_splash_open(ui_state *ui)
{
    ui->show_splash    = true;
    ui->splash_seconds = 0.0f;
    ui->show_title     = false;
}

internal void
ui_splash_close(ui_state *ui)
{
    ui->show_splash = false;
    ui->show_title  = true;
}

/* The largest rect with the aspect of the picture in the display, at the
   centre. The full picture shows, with dark bars at two sides. */
internal void
ui_splash_fit(ImVec2 display, f32 aspect, ImVec2 *top_left, ImVec2 *bottom_right)
{
    f32 width  = display.x;
    f32 height = width * aspect;
    if (height > display.y)
    {
        height = display.y;
        width  = height / aspect;
    }
    *top_left     = ImVec2((display.x - width) * 0.5f, (display.y - height) * 0.5f);
    *bottom_right = ImVec2(top_left->x + width, top_left->y + height);
}

internal bool
ui_splash_key_pressed(void)
{
    return ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
           ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false) || ImGui::IsKeyPressed(ImGuiKey_Space, false);
}

/* One window over the full display, on top of each other window. One
   invisible button covers it, so that a click anywhere closes the splash. */
internal void
ui_draw_splash(ui_state *ui, f32 dt_seconds)
{
    if (!ui->show_splash) return;
    /* The engine starts in the first frame, and a hidden tab stops the
       frames. Thus one frame can be several seconds long. */
    ui->splash_seconds += Minimum(dt_seconds, UI_SPLASH_LONGEST_STEP);
    if (ui->splash_seconds >= UI_SPLASH_SECONDS)
    {
        ui_splash_close(ui);
        return;
    }
    bool key = ui_splash_key_pressed();
    ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::SetNextWindowFocus();
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.05f, 0.07f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("###Splash", 0, flags);
    bool clicked = ImGui::InvisibleButton("###splash", ImVec2(Maximum(display.x, 1.0f), Maximum(display.y, 1.0f)));
    ImDrawList *draw = ImGui::GetWindowDrawList();
    platform_picture picture = {};
    if (ui->splash_picture) picture = ui->splash_picture();
    if (picture.texture && picture.aspect > 0.0f)
    {
        ImVec2 top_left, bottom_right;
        ui_splash_fit(display, picture.aspect, &top_left, &bottom_right);
        draw->AddImage((ImTextureID)picture.texture, top_left, bottom_right);
    }
    const char *hint = text_get(ui->text, "splash.hint");
    ImVec2 hint_size = ImGui::CalcTextSize(hint);
    ImVec2 hint_at((display.x - hint_size.x) * 0.5f, display.y - hint_size.y * 2.5f);
    draw->AddText(ImVec2(hint_at.x + 1.0f, hint_at.y + 1.0f), IM_COL32(0, 0, 0, 200), hint);
    draw->AddText(hint_at, IM_COL32(255, 255, 255, 230), hint);
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    if (clicked || key) ui_splash_close(ui);
}
