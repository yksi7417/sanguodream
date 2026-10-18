# 25 Game screens: the HUD, the list and the dialogs on demand

Files: code/ui/ui_hud.cpp, code/ui/ui_context.cpp, code/ui/ui_game.cpp
(ui_enter_game), code/ui/ui.cpp (ui_draw), code/hexdom.cpp. Tests: the
game mode tests in tests/ui/ui_tests.cpp and the demo
"demo/m14_game_screens".

## What it is for
The game mode keeps the map clear. Only the HUD with the list at the top
left and the mini map at the bottom right stay on the screen. Each other
window opens on demand next to the entity that the player clicks. It
closes again after its work. This is a part of milestone M14,
and it follows a request of the user and ui/main-map.md.

## The game mode
- ui_enter_game turns on ui->hud_mode when a game starts from the title
  (a new game or a load). The game exe also starts in this mode.
- The debug menu bar and the debug tools hide (Simulation, Scene,
  Ledger, Memory, the ImGui demo). F12, or 除錯面板 in the system menu,
  shows them again.
- The demos and the ui tests start in the debug layout. A test turns on
  the game mode itself.

## The HUD (ui_hud.cpp)
ui_draw_hud draws one window at the top left (main-map.md, "Status area"
and "List panel"):
- the season, the date and the ruler;
- 行動力, red below UI_HUD_LOW_POINTS, and 技巧;
- three buttons: 地圖 (the map of the whole continent), 情報 (the faction
  information) and 設定 (the system menu);
- the list with three tabs: 據點 (cities, ports and passes), 武將 and 部隊.

The rows that can act come first; the others are grey. A city or a port
can act when an officer there has not acted. A unit can act when it has
not acted; it shows a mark ●. A click on a row moves the camera there and
highlights the row. A right click on a row opens the dialog of its city or
unit.

## The dialogs on demand (ui_context.cpp)
Only one dialog is open at a time. ui_context_open opens it at a point
(the cursor or a button) and remembers its kind.
- Each window of a dialog calls ui_context_place before its Begin. The
  window goes next to the anchor when it appears. Near the right or the
  bottom edge it opens to the left or upward.
- Each window calls ui_context_track inside. ui_context_update runs after
  the panels of a frame. A right click outside every window of the dialog
  closes it, and the scene then ignores that click.
- The close button of the main window also ends the dialog.
- A dialog closes after its work. A City dialog closes when a new
  event reaches the ledger (its command ran). A Unit dialog closes when
  the unit acts.

## The clicks on the map (ui_hud_scene_clicks)
The user chose both mappings: the left click of the original and a right
click that opens the dialog.
- A left click works as in the original (screen-flow.md). It selects or
  moves an own unit and opens its command menu (ui_unit_click). On a city
  it opens the City dialog.
- A right click on a city or a unit opens its dialog at the cursor.
- A right click on the empty map opens the map menu (main-map.md, "Map
  context menu"):
  - 進行 ends the turn;
  - 委任部隊行動 is grey, because units have no long orders yet;
  - 情報 opens the faction information;
  - 此處情報 opens the Hex info box of the hex;
  - 儲存 and 載入 open the save screen.
- A right click counts as a click only if the mouse did not drag. A drag
  still turns the camera.

## The mini map
In the game mode the mini map opens at the bottom right when it appears.
Its close button hides it until the player opens it again.

## Tests to read
- ui: "hud_shows_the_status_and_the_list_and_commands_nothing",
  "hud_row_frames_and_right_click_opens_and_closes_the_city_dialog",
  "map_menu_proceed_issues_one_end_turn" and
  "scene_right_click_opens_the_city_dialog_or_the_map_menu".
- demo/m14_game_screens: 柴桑 from the list, 巡察, the map menu and 進行.
