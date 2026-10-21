# 32 City scene: the officer selection, the 開發 grid and the placement

Files:
- code/ui/ui_officer_pick.cpp: the officer selection window 選擇執行武將;
- code/ui/ui_placement.cpp: the 開發 dialog of the game mode and the
  placement mode;
- code/ui/ui_city.cpp: the city panel of the game mode and
  ui_propose_officers;
- code/ui/ui_city_commands.cpp: the 執行武將 button of each command
  dialog;
- code/ui/ui_base_menu.cpp: each open proposes the officers again;
- code/ui/ui_hud.cpp: a click on the map during the placement.

Tests: the ui tests
"officer_pick_checks_rows_and_fills_the_command_dialog",
"officer_pick_header_sorts_and_check_all_takes_the_top_rows",
"develop_proposal_with_one_free_officer_leaves_the_other_slots_empty",
"develop_grid_starts_the_placement_and_a_slot_click_sends_one_develop",
"placement_ignores_other_hexes_and_back_stops_it", the base menu tests,
and the demo "demo/m19_city_scene".

## What it is for
Before M19 the city of the game mode was a debug panel. A command
dialog chose each officer from a combo. 開發 chose the slot from a
combo. This tutorial builds milestone M19 (spec ui/city-commands.md,
ui/clone-gap-report.md sections 3, 4 and 5).

## The proposal
Each command dialog fills its officer slots when it opens. The base
command menu clears the proposal of the dialog at each open, so a new
open never shows an officer that acted. ui_propose_officers ranks the
free officers of the city by the ability of the command:
- 政治 for 開發 and 商人;
- 統率 for 巡察;
- 武力 for 訓練;
- 魅力 for 徵兵;
- 智力 for 生產兵裝.

A city with fewer free officers than slots leaves the other slots
empty.

## The officer selection
The 執行武將 button of a dialog calls ui_picker_open. The window lists
the free officers of the city. The window checks the proposal of the
dialog.
The columns are the columns of the first tab of the spec: 武將, 軍團,
所在, 日數, 身份 and the five abilities.
- A click on a row checks or clears it. A checked row is blue.
- A click on a header sorts the rows. An ability sorts from the top
  value. The table view of ui_table.cpp keeps the order.
- 一併 checks the rows from the top, as many as the command takes.
- 一併解除 clears the rows.
- 決定 and 一併解除 are grey while the list has no check.
- 決定 writes the checked officers into the slots of the dialog.
- 返回 closes the window and keeps the slots.

The window commands nothing. The engine sees only the command of the
dialog.

## The 開發 grid and the placement
In the game mode, 開發 shows a grid of the facilities. Each cell shows
the name, the built count in parentheses and the cost. The fields show
資金, 期間, 空地 and 行動力. 決定 is grey when can_issue refuses the
order with any slot.

決定 sends no command. It starts the placement mode:
1. The dialog closes, and a panel opens at the top right.
2. The panel shows 設施, 期間, 費用, the officers and the row 場所.
3. The arrows of 場所 step through the empty slots and frame each one.
4. The panel is green while the mouse is over an empty slot of the
   city, and brown otherwise.
5. A left click on an empty slot sends one command_develop for it.
6. A click on another hex does nothing. 返回 or a right click stops
   the placement.

The slot of a hex comes from engine_api.hex_at (slot_city and
slot_index). The state of a slot comes from facility_at.

## What is assumed
- The spec does not say what happens after a check when the command
  takes no more officers. The window ignores the click.
- 一併 checks only as many rows as the command takes.
- The colours of the placement panel are "assumed" in the spec.
- The fields 效果 and 限制 of 開發 have no source of text. The dialog
  does not show them.
- The other tabs of the officer selection show the columns of 武將一覽.
  They come with the information screens of M21.
