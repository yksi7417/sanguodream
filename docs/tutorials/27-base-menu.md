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

A click on 都市, 軍事, 人材, 計略 or 君主 opens its submenu at the right.
The clicked button stays highlighted. A second click closes the submenu.
外交 opens the Diplomacy panel directly. The second clone gap report
(G2-060, G2-065) gave the submenus of 計略 and 君主:
- 計略: 二虎競食 (the Diplomacy panel), 驅虎吞狼 and 流言 (the plot dialog
  of ui_people.cpp with the target base list);
- 君主: 評定, 軍團 and 軍師 (the Ruler panel), 官職 (the post window), 仲介
  (grey), 流放 (the exile dialog), 研究能力 and 培育武將 (the abilities
  window).
The items 移動 and 召喚 of 人材 open the dialogs of ui_people.cpp (G2-036,
G2-037). The officers travel for the days of the journey, and the
engine gives the days through travel_days (assumed rule, Q-228).

The table ui_base_items gives each submenu item: its main item, its text,
its stable id and the dialog that it opens. A click opens that dialog for
the selected city and closes the submenu. The dialog then proposes its
officers and sends the command, as in the debug layout.

An item is grey in two cases:
- The engine has no command for it. 仲介 is grey, because the clone has
  no dialog for it yet.
- can_issue refuses the trial order for a reason that blocks the command
  now (G2-015, G2-030, G2-041). These reasons are:
  - a cost: the action points, the gold or the technique points;
  - no free slot, no facility or no free officer;
  - a value at its maximum or a limit of the turn;
  - no item, or an enemy near.
  The tooltip names the reason. 授與沒收 is also grey when the force
  holds no item.

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
