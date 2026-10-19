# 26 Replay: the save pairs of the original

Files: code/engine/engine_replay.cpp, code/sim/sim.cpp (sim_resume_save),
code/sim/save_format.cpp (the per-force tables), code/ui/ui_fidelity.cpp,
misc/make_pair_index.py, data/placeholder/pairs.tsv. Tests:
tests/feature/replay_tests.cpp, the force tables in
tests/unit/save_format_tests.cpp, the Fidelity test in
tests/ui/ui_tests.cpp and the demo "demo/m13_replay".

## What it is for
The replay compares the engine with the original game, one command at a
time. This is level A of spec/experiments/ab-testing-plan.md and the work
of milestone M13. A save pair is a save of the original before one
command and a save after it. The analysis side makes the pairs with its
driver and puts them in spec/fixtures/saves/pairs/<rule>/<n>/. Each folder
has before.s11, after.s11, pair.json and diff.json.

## The pair index (misc/make_pair_index.py)
The engine does not read JSON. The script turns each pair folder into one
row of data/placeholder/pairs.tsv:
- rule and number: the folder;
- command: end_turn, develop, conscript, patrol or train, or "-" when the
  engine has no handler;
- city and facility: the ids of map/bases.tsv and facilities.tsv for the
  names of pair.json;
- officers: the officers whose acted flag goes from 0 to 1 in diff.json;
- q_ids and folder.

pair.json does not name the officers of a command or the slot of a
develop command. The script takes the officers from diff.json, and the
engine takes the first free slot. Both choices are assumptions. Run the
script after each update of the spec-src submodule:

    python -X utf8 misc/make_pair_index.py

## The load in the middle of a turn (sim_resume_save)
The original saves while the player acts. The factions before the player
already acted in that turn. sim_load_save starts a new turn: the AI
factions before the player act, and all factions recover their action
points. That is correct for a new game from a save, but not for a replay.
sim_resume_save loads the same world and then:
- gives the turn to the first human faction of the order, without an AI
  turn;
- takes the action points (table A, +12) and the technique points (table
  B, +0) of each force from the save;
- takes the acted flag of each officer (+148 bit 0) from the save.

## The replay (engine_api replay_pair)
replay_pair(before, command, after) does these steps:
1. It loads the second save with sim_resume_save and takes a snapshot. This
   is the original.
2. It loads the first save the same way and takes a snapshot. This is the
   before.
3. It runs the command through the engine with the AI on. The original
   always plays its AI factions.
4. It takes a snapshot of the result and compares it with the original.

The snapshot holds 16 fields of each city, 6 fields of each officer and 2
fields of each force. Both saves go through the same loader. Thus a field
differs only when the engine does the command in another way than the
original. Each difference has the path of the save dump of the spec, for
example cities[30].troops, and the rule and the Q-id that own it.

A field is "confirmed" when formats/save-file.md marks it confirmed. The
spec does not confirm the equipment stocks (Q-006) and the two per-force
tables (Q-202). report.differ_verified counts the confirmed fields that
differ. The replay replaces the game.

## The Fidelity panel (ui_fidelity.cpp)
The debug menu has the item Fidelity. The panel lists the rows of
pairs.tsv with their command and their Q-ids. The Replay button of a row
calls replay_pair with the two saves of its folder. The panel then shows
the counts and one row for each field that differs. A row has the before
value, the replay value, the original value, the rule and the Q-id. A mark * shows a
confirmed field.

## The measure
feature_replay_tests runs each row of pairs.tsv. A pair folder without a
row, or with the command "-", fails. Each replay prints its counts and its
differences by rule. The table feature_replay_ceilings keeps the measured
count of confirmed differences of each pair. A change that makes a replay
worse fails the test. A change that makes it better must lower the
ceiling. These are the counts of 2026-10-19:

| Pair | Fields | Differ | Confirmed | The differences |
|---|---|---|---|---|
| conscript/001 | 5884 | 3 | 2 | troops +3570, not +3439 (Q-153); public order -5, not -10 (Q-152); technique points +10, not +5 (Q-158) |
| develop/001 | 5884 | 2 | 1 | the officer gets merit +50 at once, but the original gives none in this turn; technique points +10, not 0 (Q-158) |
| economy-turn/001 | 5884 | 375 | 334 | the AI factions pick other commands than the original: conscript where the original patrols, no sorties (M15) |

The gold of the develop command (200) and of the conscript command (300)
is the same as in the original. The merit, the acted flags and the
officers of the two city commands are the same too.

## Tests to read
- tests/unit/save_format_tests.cpp: the two per-force tables, and the
  record of force 46 that ends inside the city array (Q-202).
- tests/feature/replay_tests.cpp: the folder check, one replay for each
  pair, and the errors.
- tests/ui/ui_tests.cpp, "fidelity_panel_replays_the_pair_of_the_row":
  one click sends one replay_pair with the paths of the row.
- tests/demo/demo_tests.cpp, "demo/m13_replay": the panel replays the
  develop pair.
