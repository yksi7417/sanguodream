# The replay of a game

This document gives the format and the rules of the replay of a game. A
viewer opens a finished game or a game that runs. The viewer then steps
through it, one player action at a time.

## The record
The engine keeps one record for each game. The record has two parts:
1. The start state. It is an image in the format of a save slot. The
   PRNG state of the sim is in it, so the record also holds the seed.
2. The steps after the start, in order. Each step names the faction that
   acted and the turn.

These actions are steps:
- a command that goes through engine_run, for example end_turn, patrol
  or a sortie;
- a change of the settings (set_settings);
- a change of the human factions (set_faction_human);
- a clear of the ledger (ledger_clear).

A refused command is no step. The AI factions act inside the end_turn
step of a player. The events of that step show what they did.

## The start of a record
A load starts a new, empty record. load_save, load_scenario, load_game
and the pair replay of M13 are loads. The engine takes the start image at
the first command after the load. The setters and the settings before
that command are thus a part of the start state.

A setter after the start changes the state outside the steps. Two
examples are set_city_gold and the editor. The engine keeps a checksum
of the state after each step. Before the next step, the engine compares
the checksum. If the state changed, the step gets the mark "edited". The
replay can differ from the live game from that step. The field
first_edit of engine_game_replay_info gives the first edited step.

The checksum leaves out the pointers, the content of the pack, the names,
the scratch memory and the command counter. The api takes the id of a
command before the command runs. The replay gives each command its
recorded id again.

## The view of a step
game_replay_view(N) shows the state after the first N steps. View 0 is
the start state. View step_count is the live game.
1. At the first view, the engine writes an image of the live game into
   memory.
2. The engine loads the nearest image before step N: a checkpoint, or
   the start image.
3. The engine applies the recorded steps from that image to step N.
4. A step forward applies one step to the view.
5. game_replay_close loads the image of the live game again.

The answer of a view tells if the state has the checksum that the live
game had after that step. A command while a view is open closes the view
first. The command then runs on the live game. A save while a view is open
saves the live game.

## The checkpoints
A step back in a long game applies many end_turn steps. In the debug
build, one step back in a game of 30 turns took 5 seconds. Thus the
engine keeps up to 16 checkpoints in memory. A checkpoint is an image
without the ledger. The ledger of the live image holds the events of the
checkpoint.

The engine takes a checkpoint when a turn passes and the last checkpoint
is N turns back. N starts at 1. When the memory is full, the engine
removes every second checkpoint and doubles N. With the checkpoints, a
jump to the middle of the same game and one step forward take 0.3
seconds. The save file does not hold the checkpoints. After a load, the
first views make them again.

## The record in the save file
save_game writes the record after the ledger of the slot file. The slot
format stays version 3. An older build reads the image and does not read
the rest of the file.

| Part | Contents |
|---|---|
| Section header | the magic "HXRP", the version 1, the layout of sim_state, the size of a step, the step count, the first edited step, the size of the start image and the checksum of the start state |
| Start image | a complete slot image: the header, the member table, sim_state, the arrays and the ledger at the start |
| Steps | the step records, engine_game_replay_step_record |

A step record holds these fields:
- the sim_command of a command step;
- the settings of a settings step;
- the date and the turn before the step;
- the faction that acted and the first officer of the command;
- the place of the first event of the step that has a place;
- the range of the events of the step in the ledger;
- the checksum after the step and the mark "edited".

The load uses the record only if the version, the layout of sim_state and
the size of a step agree with the build. If not, the game loads without
the record. A new record then starts at the next command. A record of an
old build can give other states, because the rules changed.

## The Replay panel
The panel is in code/ui/ui_game_replay.cpp. The Windows menu and the
system menu of the game mode open it. The panel calls engine_api only:
- the buttons |<, <, > and >| show the start, one step back, one step
  forward and the last step;
- a turn number and Go show the first step of that turn;
- a click on a row shows the state after that step;
- Live closes the view.

The map and all panels show the state of the view. The camera goes to
the place of the step. The panel lists the log lines of the step. While a
view is open, the log, the messages of a turn start and the clicks on
the map wait. A close of the panel returns to the live game.

## Multiplayer
A game instance of the multiplayer server has players and spectators.
They can watch the same record, because the save file holds it. A server
sends the save to a viewer, and the viewer steps through the game with
the same panel. A later version can also send the new steps one at a
time. The record needs no change for that.

## Tests
- tests/feature/game_replay_tests.cpp plays a short game with the sample
  save. The view of each step N shows the state that the live game had
  after step N. The tests also check the step list, the turn jump, an
  edit, a clear of the ledger and a second engine that loads the save.
- tests/ui/ui_tests.cpp, "replay_panel_steps_through_the_game", checks
  the calls of the buttons, the rows, the turn jump and Live.
