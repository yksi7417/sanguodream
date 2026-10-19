# Progress of M13 to M17

This file records the state of the milestones M13 to M17 of
docs/ROADMAP.md. The scheduled task san11-m13-m17 reads it at the start
of each run and writes it at the end. The next milestone to build is the
first section with a status other than "done".

## M13 Fidelity: replay the original's saves
Status: done (2026-10-19, tutorial 26, demo "demo/m13_replay"). The
commit of the status is the commit that writes this entry. The commits
of the milestone are:
- 2bbd0d9 fix: read the troops of a city at +16 and the gold at +20
- d431336 feat: decode the two per-force tables of a save
- 84579ec feat: replay the save pairs of the original through engine_api
- a41d2dd feat: add the Fidelity panel for the replay of the save pairs
- 78fc816 docs: add tutorial 26 on the replay and the M13 status

Tests after a clean rebuild: unit 669 of 669, feature 42254 of 42254
(41423 fixture rows, 13 replay checks), ui 99 of 99. `run_tests.bat
--demo` records all 16 demos, "demo/m13_replay" too.

Replay counts (5884 fields for each pair):

| Pair | Differ | Confirmed |
|---|---|---|
| conscript/001 | 3 | 2 |
| develop/001 | 2 | 1 |
| economy-turn/001 | 375 | 334 |

Open points for the analysis side. The run did not append them to
spec-src/spec/QUESTIONS.md. An unattended run does not commit to the spec
repository, and a changed submodule stops the next run. Copy them to
QUESTIONS.md by hand:
1. Q-153: one 徵兵 in 江陵 by the officers 636, 808 and 811 gave 3439
   soldiers. The clean room gives 3570. What is the formula?
2. Q-152 and Q-154: the same 徵兵 took 10 public order, and city +84 fell
   by 9. The clean room takes 5 public order and 5 morale.
3. Q-158: the 徵兵 gave 5 technique points. The 開發 gave none at the
   order. The clean room gives 10 for each command.
4. The merit of 開發: the original gives no merit at the order, but 徵兵
   gives 50 at the order. When does the officer of 開發 get the merit?
5. pair.json does not name the officers and the slot of a command. The
   driver can write them. The index now takes the officers from
   diff.json, and the engine takes the first free slot.
6. economy-turn/001 runs the AI of 13 factions. Its differences are
   mostly AI choices (M15). A 進行 on the first day of a month can show
   the income (Q-151) with fewer AI effects.
7. No pair for movement yet (Q-167 to Q-171). The roadmap measure of M13
   names movement.
8. Q-202: the record of force 46 of table A ends inside the city array.
   The loader does not read the action points of force 46.

Other notes:
- run_tests.bat and build.bat had LF line ends in the checkout. With LF,
  cmd finds the wrong label: a failed layer ran into the demo part. The
  run changed the two files to CRLF in the checkout only. Git shows no
  change.

## M14 A turn without the debug panels
Status: done (2026-10-19, tutorials 25 and 27, demo
"demo/m14_game_screens"). The commits of this task are:
- c7ea72e docs: record the survey and the plan of M14
- f5d09a0 feat: show the base command menu of the original in the game mode
- 3cc440a test: play a turn of the save from the game screens only
- the commit that writes this entry (tutorial 27 and the status)

Tests after a clean rebuild: unit 669 of 669, feature 42254 of 42254, ui
119 of 119. The ui count has 19 tests of the base command menu and 4 e2e
tests. The window smoke run passes. `run_tests.bat --demo` records all 16 demos.

Open points for the analysis side:
1. 移動 and 召喚: mechanics/city-commands.md gives 20 action points and
   "turns: yes", but no travel time. The two items stay grey.
2. The spec did not observe the submenus of 外交, 計略 and 君主. The items
   open the Diplomacy panel and the Ruler panel.
3. The port menu has no 都市. The clean room has no port dialog yet.

The text below is the survey and the plan of the start of M14.

Tutorial 25 and the demo "demo/m14_game_screens" did a part before this
task. They give the game mode with the HUD, the list and the mini map.
They also give the dialogs on demand, the map menu and the system menu. The M11 fix loads the per-hex
block (the fire and the 12 event sites), and hex_at gives fire_turns and
event_site. Thus `hex_state_at` of the roadmap is hex_at.

Survey of 2026-10-19 against ui/city-commands.md and ui/unit-commands.md:
- The City dialog is a flat panel of buttons. The original has the base
  command menu: 都市, 軍事, 人材, 外交, 計略 and 君主, each with a
  submenu. A port menu has no 都市.
- 召喚 (summon) and 移動 (an officer to another base) have no api.
- The unit command menu matches unit-commands.md (9 items for a combat
  unit, 7 for a transport).

Plan, in order:
1. code/ui/ui_base_menu.cpp: the base command menu with the submenus of
   city-commands.md. Each item opens the dialog of its command. An item
   without an api is grey. ui tests: one for each item.
2. The e2e test: a scripted turn of 柴桑 from sample.s11 with the debug
   panels hidden.
3. 召喚 and 移動 only if the spec gives their rules. Otherwise a question.
4. Extend the demo m14_game_screens (one demo for each milestone), the
   tutorial, the Status block and this entry.

## M15 An opponent worth playing
Status: done (2026-10-19, tutorial 28, demo "demo/m15_ai"). The commits
of this task are:
- fda9931 docs: record the survey and the plan of M15
- 201e25c feat: give the AI unit goals and a record of each AI turn
- 04fb3ca feat: add the AI panel and the goal lines of the AI view
- 6653f77 feat: let the AI march on the real map after the first months
- 8797636 test: add the demo m15_ai and keep the older demos green
- 859752b docs: add tutorial 28 on the AI view and the M15 status
- the commit that writes this entry

Tests after a rebuild: unit 669 of 669, feature 42275 of 42275 (41423
fixture rows, 21 AI checks), ui 121 of 121. The window smoke run passes.
`run_tests.bat --demo` records all 16 demos, "demo/m15_ai" too. The
replay of economy-turn/001 keeps 334 confirmed differences.

Changes to the behaviour of M8 and to older tests, for M15:
- ai_priorities.tsv has 12 rows. The planner tests of M8 expect 12 rows,
  and their city checks skip the two new sortie rows.
- The rule ai.target_hexes (30) replaces the reach of 12 hexes of M8.
- An AI unit next to a target attacks it and stays. The enemy unit of the
  demo m6_battle stays in its place again.
- The demo m12_main_map turns the AI off before the load.

Open points for the analysis side. The run did not append them to
spec-src/spec/QUESTIONS.md, for the reason of M13. Copy them by hand:
1. A year log of the original: the saves of 12 months from one start.
   The roadmap test compares the soldiers, the gold and the cities of
   each faction with it.
2. The month of the first AI attack when the player does nothing. The
   claim of knowledge/ai.md gives 5 to 6 months. The clean room uses 5.
3. Does the original AI take a free city, and how fast?
4. The distance of an enemy unit that makes an AI city send a unit out.
5. Does an AI unit go home for grain? The naval claim says that AI ships
   starve.
6. The order of the AI actions in a turn and the choice of the targets
   (Q-113, Q-140, Q-185).

Survey of 2026-10-19:
- The spec has no "year log" of the original. The statistics test of the
  roadmap has no input. knowledge/ai.md gives only unverified claims.
- The M8 planner (code/sim/ai.cpp) scores city proposals from
  ai_priorities.tsv. Units march to the nearest city of a faction at war.
- A city without an owner is no target of the march. sim_hostile lets an
  attack take it, but no AI unit goes there.
- No AI unit reacts to an enemy unit next to its city. No AI unit looks at
  its grain.
- The engine keeps no record of the options of the last AI turn.

Plan, in order:
1. Feature tests first, in tests/feature/ai_tests.cpp:
   - ai_explain gives the scored options of the last AI turn;
   - a free city falls to the AI within 6 turns;
   - a city under attack sorties a unit against the enemy;
   - a unit with low grain goes home and keeps its soldiers.
2. sim: the goal of each AI unit (defend, take a free city, attack a city,
   go home for grain), with a score. The row "defend" with the condition
   enemy_near in ai_priorities.tsv. The record of the last AI turn for each
   faction. The rules ai.free_city_bonus, ai.defend_bonus and
   ai.grain_margin_turns (assumed, Q-185).
3. engine_api: `ai_explain(faction)` and the test command `set_unit_grain`.
4. UI: the AI panel of the debug menu, and the goal lines of the AI view
   on the map. ui tests through the recorder.
5. The demo "demo/m15_ai", tutorial 28, the Status block and this entry.

## M16 The content of the player's own copy
Status: planned.

## M17 Feel: a first hour that holds
Status: planned.
