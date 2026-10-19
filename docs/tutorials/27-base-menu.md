# 27 Base menu: a turn from the game screens

Files: code/ui/ui_base_menu.cpp, code/ui/ui_city.cpp
(ui_draw_city_buttons). Tests: the "base_menu_" tests and the e2e test
"a_turn_of_chaisang_runs_from_the_game_screens" in tests/ui/ui_tests.cpp,
and the demo "demo/m14_game_screens".

## What it is for
The player plays a whole turn with the game screens, as in the original.
Tutorial 25 gives the HUD, the list, the mini map and the map menu. This
tutorial adds the base command menu of ui/city-commands.md to the City
dialog. Together they finish milestone M14.

## The base command menu (ui_base_menu.cpp)
In the game mode the City dialog shows six main items in a column:
- 都市: 開發, 吸收合併, 徵兵, 生產兵裝, 巡察, 商人, 研究技巧;
- 軍事: 訓練, 出征, 輸送;
- 人材: 移動, 召喚, 探索人材, 登庸武將, 褒賞, 授與沒收;
- 外交, 計略 and 君主.

A click on 都市, 軍事 or 人材 opens its submenu at the right. The clicked
button stays highlighted. A second click closes the submenu. The spec
did not observe the submenus of 外交, 計略 and 君主. Thus these items open
the Diplomacy panel and the Ruler panel directly.

The table ui_base_items gives each submenu item: its main item, its text,
its stable id and the dialog that it opens. A click opens that dialog for
the selected city and closes the submenu. The dialog then proposes its
officers and sends the command, as in the debug layout.

An item is grey in two cases:
- The engine has no command for it. 移動 and 召喚 are grey, because the
  spec gives no travel time for them.
- can_issue refuses the trial order for a cost: the action points, the
  gold or the technique points. The tooltip names the reason.

The debug layout keeps the flat buttons of the City panel. The older ui
tests use them.

## The scripted turn
The e2e test "a_turn_of_chaisang_runs_from_the_game_screens" uses the
real engine:
1. It loads sample.s11 and enters the game mode. The debug panels hide.
2. It opens the City dialog of 柴桑 (city 26) and clicks 都市, then 巡察.
3. 決定 of the Patrol dialog runs the patrol. The City dialog closes.
4. 進行 of the map menu ends the turn.
5. The debug menu bar did not show in the whole turn.

The demo m14 plays the same turn on the real map.

## Tests to read
- tests/ui/ui_tests.cpp, ui_base_menu_cases: one test for each item. The
  test asserts the dialog that the item opens, or the grey state. A
  dialog that opens commands nothing.
- tests/ui/ui_tests.cpp, the e2e test of the scripted turn.
- tests/demo/demo_tests.cpp, "demo/m14_game_screens".
