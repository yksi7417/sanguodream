# 21 Playable: save and load, the start, new officers, PK training, history and the game screens

Files: code/engine/engine_saves.cpp, code/engine/engine_playable.cpp,
code/sim/pk.cpp, code/sim/history.cpp, code/ui/ui_game.cpp, code/text/text.cpp
(text_encode_big5). The data is in data/placeholder/ability_chart.tsv,
ancient_officers.tsv, history.tsv, portraits.tsv, scenarios.tsv,
battles.tsv and rules.tsv, and in the second pack data/civil_war/.
Tests: tests/unit/playable_tests.cpp, tests/unit/text_tests.cpp,
tests/feature/playable_tests.cpp, the M9 tests in tests/ui/ui_tests.cpp,
the demo "demo/m9_playable".

## What it is for
This tutorial covers milestone M9, the last milestone of the roadmap. M9
makes the engine a game that a player can start, play, save and load. The
specs are:
- mechanics/game-setup-and-modes.md: the settings, the scenarios, the new
  officers, the editor and the battle mode;
- mechanics/pk-officer-training.md: 研究能力 and 培育武將;
- mechanics/events.md: the historical events, the endings and the
  portraits;
- ui/title-and-load.md, ui/screen-flow.md, ui/main-map.md and
  ui/information-screens.md.

The features are F-009, F-060, F-061, F-131 to F-135 and F-140 to F-145 of
FEATURES.md.

## Save and load
The engine writes its own format, not the format of the original game. A
slot file holds four parts:
1. a header: the magic, the version, the struct sizes and the map size;
   then the pack and the slot info (comment, date, faction, turn);
2. the bytes of sim_state;
3. the arrays that sim_state points to: the entities, the officer and city
   tables, the map and the fire;
4. the ledger.

A load keeps the pointers of the live sim and copies the arrays into them.
The seed of the random numbers is in sim_state, so a loaded game plays on
as the saved game did. A file of another build or another pack does not
load, and a refused load changes nothing. The platform layer has a file
writer for this (`platform_write_entire_file`). The 32 slots are files
slot_01.hxs to slot_32.hxs in the save folder of engine_config.

The header of version 2 carries the layout of sim_state:
`engine_save_layout` hashes the offset and the size of each member. The
struct sizes alone did not protect the player. On 2026-10-23 a new rule
in sim_rules moved every member after content by four bytes, with the
same sizeof(sim_state). The old slot loaded, and the game showed the year
12 and the wrong ruler. Now a file with another layout gets
ENGINE_SAVE_OTHER_VERSION. The tables and the rules of the pack
(sim_state.content) come from the pack of this build, never from the
file, so a new rule does not break a save. A version 1 file has the
layout of 2026-10-22, and the load inserts the four bytes.
tests/feature/save_compat_tests.cpp loads such a file,
tests/fixtures/saves/slot_01.hxs, and checks its date and its player.
When a change of sim_state breaks that test, add a migration or make the
load refuse the old files. Do not remove the test.

## The settings of the start
engine_settings has the settings of game-setup-and-modes.md:
- 壽命: none, historical, or long (lifespan.long_years more);
- 能力變動, 事件 and the disasters;
- 女武將: off, a free female officer never appears;
- 落雷・妖術: off, nobody can use the two plots;
- 血緣・關係: off, the engine clears the liked and disliked officers, the
  spouses and the sworn brothers (the father stays);
- 單挑・舌戰: manual, semi-automatic (the computer plays the rounds) or
  automatic (the computer also takes the first result);
- 遊戲等級: the computer factions give half, the same, 1.5 times or 2 times
  ai.commands commands.

The female and the relation settings change the officers one time: when
they go from on to off.

## New officers and the ancient officers
create_officer adds a free officer in a city. The type sets the abilities:
萬能 high, 軍師 normal leadership and force, 將軍 normal intellect and
politics, 凡才 low. An ancient officer of ancient_officers.tsv brings its
own name, abilities, skill and personality. A new officer can found a
faction in an empty city. At most new_officer.max (8) new officers join one
game. The text module encodes a typed name to Big5 for the order.

## The ability chart of the PK
ability_chart.tsv has one row for each ability. A row has a zone, a kind,
a count of uses, the months of research, and the rows before it on the
arrows:
- 研究能力 costs 20 AP and 300 gold, one research at a time, and gives the
  uses of the row when its months end;
- 培育武將 costs 20 AP and one use. The officer is away 30 days (3 turns)
  and takes no command. Only one officer of a faction trains a base
  ability at a time.

The kinds are:
- plus: the ability rises by 5;
- floor: an ability below 70, 80 or 95 rises to that value;
- aptitude: one grade up, from the grade before only;
- skill: the new skill replaces the old one.

## The historical events
history.tsv holds the 40 historical events of events.md as data. Each row
has a chance, conditions, effects and the Big5 names of its officers. The
conditions and effects are clauses, separated by ";":
- a condition, for example `free:@1`, `rules:22|23` or `death_year:@0`;
- an effect, for example `join:@1:100:4000`, `marry:@0:@1` or `dies:@0`.

`@i` is the officer i of the names column. history.cpp is the one
interpreter. On day 1 of each month, the calendar checks each row that has
not fired. It tries each faction with a ruler. The first faction that meets
the conditions gets the event. The loader refuses an unknown clause or an `@i`
outside the names. The editor command `trigger_history` runs the effects of
a row now.

## The endings and the portraits
A unification ends in one of three endings (events.md):
- A 漢朝再興: the emperor abdicates to his protector. The protector respects
  the Han, or is normal with more than 15 cities at public order 100;
- B 外征繁榮: the military facilities are more than 30 % of the built
  facilities, or more than 500000 soldiers and more warriors than scholars;
- C 內政繁榮: the others.

portraits.tsv gives the age of the second portrait of 25 officers.
officer_detail.portrait_stage is 1 from that age.

## The scenarios and the second pack
scenarios.tsv lists the save, the scenario 群雄割據 and the six PK
scenarios. The clean room has the files of the first two only, so the six
PK rows are not available. load_scenario starts a new game from a scenario
file: the engine makes a save from its records. No faction is human until
the front end chooses the rulers.

### The start of a new game
On 2026-10-22 the player started 群雄割據 as 曹操 in the original and in
the clone. The two 武將一覽 lists did not agree. These fixes make the start
agree with the scenario file:
- Gold and troops. scenario-file.md names +13 the gold and +17 the troops.
  But a scenario field +n is the save field +n+3. save-file.md swapped the
  save fields +16 and +20 on 2026-10-17. Thus the loader reads +13 as the
  troops and +17 as the gold. 陳留 starts with 28,000 troops and 5,600
  gold. The plate of a city shows the troops.
- Loyalty. The loyalty byte +104 is in the part that the scenario record
  shares with the save record. The shared decoder reads it. The rule
  loyalty.start applies only to a record without the byte.
- Skills. The skill byte +124 is a file id. The order of the file ids is
  not the order of skills.md. The column file_id of skills.tsv gives the
  file id of each number. The clone matched the holders of skills.md
  against the bytes of SCEN002. 97 numbers agree, and no byte has two
  numbers. The numbers 35, 41 and 61 have no holder. Their file ids follow
  from the order of their neighbours. The loader changes the byte into the
  number - 1 that the sim keeps.
- Merit. Before the fix, load_scenario also started the first turn. No
  faction was human, so the AI played every faction, also 曹操. Each
  officer got the merit of a command. Now load_scenario only makes the
  order of the factions, and whose_turn gives no faction. The first
  end_turn after the choice of the rulers starts the first turn.
- Officers. SCEN002 puts 27 officers in force 0, with 夏侯恩 and 曹純. The
  list of the original shows 24 rows and a scroll bar. The clone keeps the
  27 officers. docs/PROGRESS-GAP2.md asks the observer for the rows below.

The names of two skills differ in the original: 連戰 is 連擊 and 急襲 is
突襲. The text of the skill is the same.

data/civil_war/ is the second content pack (see its README). It has other
units, weapons, rules and events, and no historical events. The same
engine code runs it.

## The game screens
ui_game.cpp puts the screens of the game on top of the debug panels:
1. the title menu: load, new game, battle mode, options, quit;
2. the scenario list, the ruler list (up to 8 rulers) and the settings of
   the start;
3. the main map: the top bar (date, faction, action points, 進行), the
   history window, the mini map and the hex map;
4. the save and load screen: four pages of 8 slots, the detail of a slot,
   and the comment of a save;
5. the options: the new officer dialog, the editor and the settings;
6. the battle list, the ability chart and the information screen.

The city, unit, ruler, duel and debate panels of the earlier milestones
are the command menus and screens of the game. The game executable opens
the title menu at the start.

## The stubs
- The editor sets the officer, faction, city and unit fields that the
  engine editor commands allow. The spec lists more fields.
- The battle mode lists the five battles that the Steam guide names. The
  spec has no data for them, so no battle starts (Q-190).
- F-077, the map of the real continent, waited for Q-034. M10 builds it
  (tutorial 22).

## The tests, line by line
- playable_tests.cpp (unit) checks the loaders of the chart, the ancient
  officers and the history rows, and the clause check.
- text_tests.cpp checks that a typed name goes to Big5 and back.
- playable_tests.cpp (feature) checks these items through engine_api:
  - a save and a load in a second engine, and the same turns after it;
  - each setting of the start;
  - new officers, an ancient officer, a new faction and the limit of 8;
  - research of the chart and the training of an officer;
  - the 40 history rows, a trigger, and 郭嘉之死 on the calendar;
  - the endings A and B or C, and the portrait of 曹操;
  - the scenario list and a new game from 群雄割據;
  - the troops and the gold of 陳留 after a new game as 曹操;
  - the loyalty, the merit and the skill of 25 officers of 曹操;
  - the 27 officers of 曹操 at the start;
  - the Civil War pack.
- ui_tests.cpp checks the API calls of each new screen:
  - the title flow and 進行;
  - save and load;
  - the new officer dialog, the ability chart and the editor;
  - the read-only screens.
- demo/m9_playable plays a new game from the title menu to a save and a
  load.

Run `run_tests.bat --e2e` and `run_tests.bat --demo`. Each prints
ALL PASS.
