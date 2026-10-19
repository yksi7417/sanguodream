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
Status: in progress (started 2026-10-19).

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
Status: planned.

## M16 The content of the player's own copy
Status: planned.

## M17 Feel: a first hour that holds
Status: planned.
