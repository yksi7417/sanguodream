# 30 First hour: the title rider, the load transition and the turn messages

Files:
- code/ui/ui_feel.cpp;
- code/ui/ui.h: the "feel" block and the UI_TITLE_, UI_LOAD_ and
  UI_MESSAGE_ constants;
- code/ui/ui_game.cpp: ui_title_open, ui_title_button and the load;
- code/ui/ui_hud.cpp: the list hides;
- code/ui/ui.cpp: ui_draw;
- code/hexdom.cpp: the game exe turns the feel on;
- data/placeholder/text/en.tsv and zh-TW.tsv: the "msg." strings.

Tests: the "M17" ui tests and the e2e test
"the_first_hour_runs_from_the_title_without_a_stall" in
tests/ui/ui_tests.cpp, and the demo "demo/m17_first_hour".

## What it is for
The first hour of a new game must feel like the original. This tutorial
builds milestone M17. The UI waits where the original waits, and it shows
the reports of each turn in the message window. The sim stays instant.
Only the front end waits. The engine api does not change.

## What the spec gives
- ui/title-and-load.md: after a click on a title button, the other five
  buttons disappear. A rider animates for approximately 1 s. Then the
  selected screen opens. A second click in that time has no visible
  effect. A load shows a dark landscape with no controls for
  approximately 4 s.
- ui/main-map.md, "Message window": a frame of 689 x 197 at the bottom
  centre, with the portrait of the speaker and up to 3 lines of text. A
  left click anywhere advances to the next message. The list panel hides
  while the window shows. The window appears at the start of a turn.
- ui/main-map.md, "Message log": the period heading has the form
  《year month 上旬 / 中旬 / 下旬》.
- ui/screen-flow.md: 進行 ends the turn. The other factions act, the
  messages show, and a new turn starts.

The spec has no capture of a first hour. It gives no frame timings, no
order of the messages and no list of the reports. ui/screen-flow.md puts
"重新開始新遊戲 to the first turn" on its list of screens to record next.
The Status block of M17 in docs/ROADMAP.md lists these gaps.

## The two switches
ui_state.feel has two flags:
- original_timing: the rider and the load transition. Off, each screen
  opens without a wait, as in M9 to M16.
- turn_messages: the message window, and the turns of the AI after 進行.

ui_init turns both off. Thus the tests and the demos of M0 to M16 do not
change. code/hexdom.cpp turns both on for the game exe. A test of M17
turns them on itself.

## The rider of the title
ui_title_button draws one button. With original_timing on, a click calls
ui_feel_choose_title. The choice waits in feel.title_choice. While it
waits:
- ui_draw_title draws only the chosen button and a bar that fills in
  UI_TITLE_SELECT_SECONDS. The bar is the placeholder art of the rider.
- A click on the chosen button does nothing.
- ui_feel_update adds the frame time. At UI_TITLE_SELECT_SECONDS it calls
  ui_title_open. That function opens the screen of the button.

## The load transition
A load from the save screen calls ui_enter_game and then
ui_feel_begin_transition. For UI_LOAD_TRANSITION_SECONDS, these changes
apply:
- ui_draw_load_transition draws a dark window over the whole screen. It
  takes the mouse, so the scene gets no click.
- ui_draw skips the HUD and the mini map.
- ui_feel_update closes each dialog that opens.

## The turn start
ui_feel_turn_start runs each frame in the game mode with turn_messages on:
1. If no faction is a player, it stops. A sample save without a player
   stays as it is.
2. If the faction that acts is not a player, it calls end_turn until a
   player acts. The limit is UI_FEEL_PASS_LIMIT calls. Before M17 the
   game mode stopped at the turn of the first AI faction after 進行.
3. If the turn and the faction differ from the last turn start, it
   collects the messages.

ui_feel_collect_messages puts the period heading first. Then it adds the
reports since the last turn start, in ledger order, up to
UI_MESSAGE_CAPACITY. A report is one of these ledger events:
- an event of the player's faction that does not only keep the books.
  ui_feel_bookkeeping lists the kinds that keep the books, for example
  FACTION_TURN_STARTED and ACTION_POINTS_RECOVERED;
- an attack of another faction on a city of the player.
  ui_feel_hostile lists these kinds, for example BASE_DAMAGED.

The clean room chose this rule. The spec has no list of the reports.

## The message window
ui_draw_messages draws "###Messages" at the bottom centre with the size
of main-map.md. The ruler of the player speaks. The spec does not name
the speaker of each report. The portrait frame of tutorial 29 shows the
first character of the name. The text is the heading, or the event name,
the subject and the amount of one report.

The whole window is one invisible button, "###next". A click on it shows
the next message. A left click outside every window also advances
(screen-flow.md, rule 2). While the window shows, these changes apply:
- the HUD draws no list;
- the scene ignores the clicks.

## The tests
- ui "title_click_hides_the_other_buttons_and_opens_after_the_rider":
  after the click only 重新開始新遊戲 stays. A second click does nothing.
  After 1 s the scenario list opens. No command goes to the engine.
- ui "load_transition_hides_the_map_and_takes_no_input": the dark window
  shows, the HUD does not, and a map menu closes. After 4 s the HUD
  returns.
- ui "turn_messages_show_the_heading_then_the_reports_and_a_click_advances":
  three scripted events give two messages, the heading and the income of
  the player. Two clicks close the window, and the list returns.
- ui "turn_messages_open_again_after_proceed": 進行 sends one end_turn,
  and the heading of the new turn shows.
- e2e "the_first_hour_runs_from_the_title_without_a_stall": the real
  engine from the title through 群雄割據 and seven turn starts. Each wait
  has a frame limit.
- demo "demo/m17_first_hour": the first hour on the real map. In the
  fourth turn the player sends a unit from the capital against the
  nearest rival city. The walkthrough ends at the first battle, in the
  eighteenth turn. The AI of M15 does not march in the first ten months
  of this scenario. The demo thus uses the attack of the player.

## Check it
1. Run `build.bat`, then `run_tests.bat`. The ui layer gives 129 of 129.
2. Run `build\win32_hexdom.exe`. Click 重新開始新遊戲. The other buttons
   hide, and the scenario list opens after 1 s.
3. Start 群雄割據 with one ruler. The message window shows the heading of
   the first turn. Click until it closes.
4. Right click the empty map and click 進行. The next turn of the player
   starts without a wait, with its messages.
