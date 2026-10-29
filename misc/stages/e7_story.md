# Stage E7: the story mode and the events during the game

Do E7 of docs/EUROPE.md for the packs medieval_west and medieval_east. The
story pages tell the background before the first turn. The events of
history.tsv continue the story during the game.

## The goal
- The new game screen of a pack with a story shows a button "Story". It
  opens the pages of E7 for that map. The player can skip the pages.
- history.tsv of each pack has the events of E7, "The events during the
  game", for its map. Each event has a window and preconditions. No event
  has a fixed date.
- The story of the Mamluks plays on the east map, from Cairo to Antioch.
- The Three Kingdoms game, its fixtures and medieval_europe do not change.

## The facts
- Read docs/EUROPE.md, E6 and E7, first. Then read code/sim/history.cpp
  and the M9 tests in tests/unit/playable_tests.cpp.
- The stage e8_factions is on master. Acre, Antioch, Sis, Nicosia,
  Tbilisi and Baghdad have owners on the east map.
- history.cpp checks the rows on day 1 of each month. Each row fires one
  time. sim->history_turn keeps the fired rows, and the save keeps
  history_turn (code/engine/engine_saves.cpp).
- Each row already has a column chance, a percent for each month. Use
  this column. Do not add a clause "chance".
- The sim finds the officer of a row by the Big5 bytes of the column
  names (history.cpp, sim_officer_by_name). The English keys live in the
  engine (code/engine/engine_keys.cpp), not in the sim. The rows of the
  Europe packs name officers by key, for example @baybars. Let the engine
  change each key to the officer before the sim reads the rows. The sim
  stays headless and does not read files.
- The sim has alliances and ceasefires (sim.h, SIM_DIPLOMACY_ALLIANCE and
  SIM_DIPLOMACY_CEASEFIRE). Two factions are at war if they have no
  alliance and no ceasefire.
- E6 waits for the answer to Q-E2. Thus this stage does not add the
  effect see_holder, and it does not add the sees. Leave the see parts of
  the events out. E6 adds them later.
- Q-E4 is open. The story text uses the Latin script only.
- The lectures of Dr. Roy Casagranda give ideas only. Do not copy his
  words. Each fact of a page needs two sources (docs/europe/SOURCES.md).

## The work
1. The new conditions. Add date_until, at_war, allied, owner, church and
   after to history.cpp. Give each one a unit test first.
   - church needs the church of each faction. Add the column church to
     forces.tsv of each pack, with latin, greek, armenian or muslim.
   - after names the key of another row. Add a column key to history.tsv
     if the rows have no key.
2. The new effects. Add seize, rename, target and city_loss. Give each
   one a unit test first.
   - rename changes the English name of a faction. Keep the save format.
     Derive the name from history_turn, or keep it in a block that the
     save already writes. A loaded game must show the new name.
   - target makes the AI of the faction march on a city. Give the target
     a limit of months.
   - city_loss takes a percent of the durability of the walls and of the
     troops of a city. A city of the sim has no population.
3. The keys of the officers. Let the engine read @key in the rows of a
   pack with keys. The rows of the Three Kingdoms packs keep the Big5
   names.
4. The officers that appear later. Add Qutuz, Aybak, Shajar al-Durr and
   Baybars to officers.tsv of the east pack, with the column appear and
   the city Cairo. Add Baldwin IX of Flanders and Henry of Flanders as
   free officers in Venice from 1202. Use the keys of the prototype pack
   where a key exists. Add the lives, the links and the sources as in E4.
   Run misc/make_scenario.py.
5. The rows of history.tsv. Write the events of the table in E7 for each
   map:
   - The west map: the Albigensian Crusade, Las Navas de Tolosa and
     Bouvines.
   - The east map: the Fourth Crusade, the sack of Constantinople, the
     Mamluks take Cairo, Baybars becomes sultan, and the siege of Antioch.
   - France and Flanders are not on the east map. There, the Fourth
     Crusade makes Baldwin IX and Henry of Flanders join Venice, and
     Venice gains gold. Montferrat joins Venice if he is free.
   - If an effect of the table needs more than the effects of this list
     and of M9, use the nearest effect. Write the change in E7.
6. The story pages. Write story/en.tsv and story/zh-TW.tsv of each pack.
   The pages of E7 are:
   - Europe in 1200;
   - the authorities;
   - the sparks of the war;
   - one page for each faction of the map.
   The text module reads the files, as it reads lore/en.tsv. The panel
   calls engine_api only to know the pack. It sends no command.
7. Update E7 in docs/EUROPE.md: the final rows of each map, the new
   clauses and the status in the milestone table. Add a tutorial or a
   part of a tutorial in docs/tutorials for the new clauses.

## The tests
- A unit test for each new condition and for each new effect, in
  tests/unit/playable_tests.cpp. Write each test first and see it fail.
- A unit test for each event row of the two packs. Build the state of the
  preconditions in the test, and check that the row fires one time. Build
  another state where one precondition is false, and check that the row
  does not fire.
- A feature test for each pack. The engine loads its history.tsv and its
  story with no bad row.
- A feature test for rename after a save and a load.
- A ui test for the story panel. The panel sends no command
  (recorder_count_commands).
- The fixtures of the Three Kingdoms events stay green.
- run_tests.bat passes.
