# 33 AI round: the turn of the others with no click

Files:
- code/ui/ui_round.cpp: 進行, the show of the AI round, the hex of a unit
  during the show;
- code/ui/ui_feel.cpp: the turn start opens the message window only with
  the show on;
- code/ui/ui_hud.cpp: the HUD names the AI faction of the show;
- code/ui/ui_scene.cpp and ui_locate.cpp: the units and their plates
  stand on the hex of the show;
- code/ui/ui_settings.cpp: the setting of the show.

Tests: the ui tests
"proceed_with_the_default_setting_opens_no_message_and_the_log_keeps_the_round",
"ai_show_names_each_faction_and_floats_the_events_in_order",
"ai_show_moves_a_unit_hex_by_hex_and_the_camera_stays", and the demo
"demo/m20_ai_round".

## What it is for
Before M20 the player saw nothing of the other factions. After 進行 a
window showed the reports of the new turn, and each report needed a
click. This tutorial builds milestone M20 (spec ui/clone-gap-report.md
section 7).

## The decision of the player
On 2026-10-20 the player decided (spec DECISIONS.md):
- the show of the AI round is off by default;
- no part of the show needs a click;
- after 進行 the next turn starts with no message to click;
- the log keeps every event, and its filter shows all events or only
  the events of the player.

## The setting off: the default
進行 calls ui_round_proceed. It calls end_turn one time. The engine runs
the turns of all AI factions inside end_turn. The turn start of the
player opens no message window. The log of M18 reads the new events in
the same frame. It shows the date of the period and one line for
each event of each faction.

## The setting on: the show
The settings screen has the check box "顯示其他勢力的行動". With the show
on, ui_round_proceed keeps two facts before end_turn:
- the ledger count, the start of the round;
- the hex of each unit.

After end_turn, ui_round_update plays the events of the round, one step
at a time:
1. An event of a new AI faction first gives the HUD to that faction for
   UI_ROUND_FACTION_SECONDS. The HUD shows its ruler and its 技巧, and
   no 行動力. The list panel hides.
2. An event of a city with a sign rises as a value over the city. A city
   off the screen shows no value, because the camera does not move.
3. A unit that moved walks from its hex before 進行 to its hex now, one
   hex in each UI_ROUND_HEX_SECONDS. ui_round_unit_hex gives the hex for
   the scene and the plates.
4. The events of the player and the books of the turn play no step.

The camera stays where the player left it. The scene takes no click
while the show plays. After the last event, the turn start of the
player shows its reports in the message window.

## What is assumed
- The spec gives the frame times of one round, but no time for each
  step. The constants of ui_round.cpp are an assumption.
- The ledger gives no path of a move. The unit walks on the line of
  hexes from its old hex to its new hex.
- The effects of an attack and the cut-in 突擊!! of the original are not
  in this milestone. They need the art of track G.
- The filter of the log shows all events or the events of the player.
  A filter for one faction or for the events near the player is open.
