# 35 Fit and finish: the global rules of the screen flow and the HUD

Files:
- code/ui/ui_context.cpp: ui_context_back, the rule of the right click;
- code/ui/ui_hud.cpp: ui_hud_panels_hidden, the map menu, the system
  menu, the help window, the question before 進行 and the unit row;
- code/ui/ui_information.cpp: the popup of a unit and 所屬武將;
- code/ui/ui_locate.cpp: ui_status_place, the 設置 field of the status
  bar;
- code/ui/ui_minimap.cpp, code/ui/ui_city_commands.cpp, code/ui/ui.cpp.

Tests: the ui tests "flow_rule1_right_click_goes_back_one_level" to
"flow_rule7_lists_menus_and_tabs_have_the_pitches_of_the_original",
"map_menu_has_the_five_items_of_the_original",
"system_menu_has_the_six_items_of_the_original",
"minimap_has_no_close_button_in_the_game_mode",
"hud_unit_row_moves_the_camera_and_opens_no_window",
"status_bar_shows_red_not_possible_on_a_city_hex",
"help_button_opens_the_help_window",
"proceed_with_units_that_have_not_acted_asks_first",
"unit_popup_opens_the_detail_or_the_officers_of_the_unit", and the demo
"demo/m22_fit_and_finish".

## What it is for
The clone gap report lists small gaps of severity B and C. Each gap is
a place where hexdom does a different thing from the original. This
tutorial builds milestone M22. It closes these gaps and tests the seven
global rules of ui/screen-flow.md, one test for each rule. The player
confirmed the removals on 2026-10-20.

## The global rules
| Rule | What hexdom does | Where |
|---|---|---|
| 1 Right click | It goes back one level. See the list below. | ui_context_back |
| 2 Left click | A click anywhere shows the next message. | ui_feel_update (M17) |
| 3 Hidden panels | A selection or a menu hides the list, the log and the mini map. | ui_hud_panels_hidden |
| 4 Camera keys | W A S D, Q and E move the camera. They close no panel. | ui_scene_handle_keys |
| 5 決定 and 返回 | 決定 is at the left of 返回. 決定 is grey until the command is valid. | ui_draw_command_buttons |
| 6 Places | A menu opens at the clicked object. A command dialog stays at the centre. | ui_context_place, ui_draw_city_command |
| 7 Pitches | List rows 20 px, menu items 34 px, tabs 76 px. | UI_LIST_PITCH, UI_MENU_PITCH, UI_TAB_PITCH |

The levels of rule 1 are these:
- the officer selection goes back to its command dialog;
- a command dialog goes back to the city panel and its menu;
- the city panel closes;
- a target pick or the action list goes back to the unit menu;
- the unit menu goes back to the selection;
- the selection ends, and the map is idle;
- any other menu or dialog closes.

On the idle map, the right click opens the map menu. On an own unit, it
opens the popup 詳細情報 and 所屬武將.

## The hidden panels
ui_hud_panels_hidden is true while one of these is open:
- a dialog or a menu of ui_context.cpp;
- the selection of a unit;
- the question before 進行;
- a screen of the information menu, or 中地圖;
- the placement mode of 開發.

The status area of the HUD stays. The flags of the player for the log
and the mini map stay. The panels come back when the selection ends.
A unit dialog that closes also ends the selection.

## The removals and the additions
- The map menu has five items: 進行, 委任部隊行動, 情報, 儲存 and 載入.
  此處情報 is gone.
- The system menu has six items: 儲存, 載入, 環境設定, 編輯, 返回開頭畫面 and
  結束遊戲. The item 編輯 opens the editor of M9 as a dialog. The item
  項目 stays on the title screen. The debug panels open with F12 only.
- The mini map has no close button in the game mode.
- A left click on a unit row moves the camera to the unit. It selects
  nothing and opens no window. A right click on the row still opens the
  unit panel.
- The HUD has a fourth button "?". It opens a help window with the
  controls of hexdom.
- 進行 asks first when a unit of the player has not acted. The buttons
  是 and 否 are above the question, at the bottom of the screen. 否 and
  a right click keep the turn.
- The 設置 field of the status bar shows 不可 in red on a city hex.

## What is not built
The spec has no data for these items. They stay open:
- the text of the help screen of the original. hexdom shows its own
  list of controls;
- the 設置 field on a gate or a port hex;
- the yellow fields of the city panel when the cursor is on a menu
  item. The spec gives two examples, not the full list;
- the skill tags and the grades of the unit panel of the original;
- the cyan area of a city under the mouse;
- the art of the HUD, the unit plates and the facility models (track G).
