# 34 Information: the information menu, the tables, the political map and the hover panels

Files:
- code/ui/ui_information.cpp: the column of 14 items, 武將一覽, 都市一覽,
  武將情報, 都市情報 and 勢力情報;
- code/ui/ui_political.cpp: the political map 中地圖 and the hover panels;
- code/engine/engine_map.cpp: engine_api.region_view;
- code/ui/ui_context.cpp: 地圖 of the HUD opens 中地圖 in the game mode.

Tests: tests/feature/map_tests.cpp (the region view), the ui tests
"info_menu_items_open_their_screens_and_grey_the_others",
"info_table_header_sorts_and_a_row_opens_the_detail",
"city_table_and_faction_screen_show_and_close",
"political_map_opens_from_the_hud_and_changes_its_background",
"hover_panel_shows_the_city_under_the_mouse", and the demo
"demo/m21_information".

## What it is for
Before M21 the 情報 button of the game mode opened a small debug window.
The 地圖 button opened the 2D hex panel of the debug layout. The player
had no tables and no political map. This tutorial builds milestone M21
(spec ui/information-screens.md, ui/main-map.md "Information menu" and
"Hover panels").

## The information menu
In the game mode, 情報 opens a column of 14 items at the left of the
screen. The spec describes six of the screens:

| Item | Screen |
|---|---|
| 自軍勢力 | 勢力情報 of the player |
| 勢力武將 | 武將一覽 of the officers of the player |
| 勢力都市 | 都市一覽 of the cities of the player |
| 全武將 | 武將一覽 of the active and the free officers |
| 全都市 | 都市一覽 of every city |
| 全勢力 | 勢力情報 with every faction in the list |

The other eight items are grey, and a tooltip says that the spec does
not describe them. The debug layout keeps its own 情報 window.

## The tables
A table window has view tabs, a sortable table, and 返回.
- 武將一覽 has the tabs 所屬, 能力 and 特技 with the columns of the spec.
- 都市一覽 has the tab 狀態: 名稱, 勢力, 太守, 治安 and 特色.
- A click on a header sorts the rows. The view of ui_table.cpp keeps the
  order. An ability sorts from the top value.
- A click on a row opens the detail panel. The list at the left of the
  panel shows the rows in the order of the table. A click on the list
  shows another row.
- 返回 or a right click closes the detail panel first, then the table.

The detail panels show these fields:
- 武將情報: the portrait, the five abilities, 勢力, 所屬, 所在, 身份,
  忠誠, 功績 and 特技;
- 都市情報: the portrait of the 太守 and the fields of the city panel of
  M19;
- 勢力情報: the faction list, the ruler, 技巧P, and the counts and the
  sums of the faction.

## The political map
In the game mode, 地圖 opens 中地圖. engine_api.region_view gives the
region of each hex and the city of each region. The map fills each
region:
- in the colour of the faction of its city (背景 勢力);
- or in a shade from dark to yellow for the share of the largest value
  (背景 士兵, 資金 or 兵糧).

The group 顯示 shows the cities as squares, the gates and the ports as
diamonds, and the units as dots.

## The hover panels
A panel next to the mouse shows the city or the unit under it, with the
rows of the spec. The panel takes no input. The tooltip of a unit stays
in the debug layout.

## What is not built
The spec marks these screens "not observed", or gives no columns:
- 勢力港關, 全港關, 全軍團, 全部隊, 全道具, 爵位, 官職, 特技;
- the tabs 適性, 命令, 個人 and 血緣 of 武將一覽;
- the tabs 戰力, 兵裝, 資金, 兵糧, 設施1, 設施2, 地理 and 災害 of 都市一覽;
- the sub tabs 兵裝 and 內政設施 of 勢力情報 and 都市情報;
- 背景 軍團, 顯示 外交, 戰 and 街道, the legend 外交狀態凡例, and the
  distributions 災害 and 漢帝 of 中地圖;
- the maximum of 氣力 in the hover panel of a unit, and its state word.
