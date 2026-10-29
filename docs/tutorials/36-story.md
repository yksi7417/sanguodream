# 36 Story: the story pages and the events during the game

Files:
- code/sim/history.cpp: the new clauses and effects of the rows;
- code/sim/ai.cpp: the AI target of a faction;
- code/sim/content.cpp: the history rows and the church of a faction;
- code/engine/engine_history_keys.cpp: the change of @key into an officer;
- code/text/text.cpp: text_key_story;
- code/ui/ui_story.cpp: the story panel;
- data/medieval_west/history.tsv, data/medieval_east/history.tsv;
- data/medieval_west/story/en.tsv, data/medieval_east/story/en.tsv.

Tests: tests/unit/playable_tests.cpp, tests/unit/story_tests.cpp,
tests/unit/text_tests.cpp (unit_text_story_tests),
tests/feature/world_map_tests.cpp (feature_pack_story_tests) and the ui
test "story_panel_walks_the_pages_of_a_pack_and_commands_nothing".

## What it is for
This tutorial builds stage E7 of docs/EUROPE.md. The story mode tells the
reader why the war starts. The events during the game then follow the
state of the game and not a fixed date.

## The rows of history.tsv
Each row has a window and preconditions. A row starts one time. The
interpreter checks the rows on day 1 of each month.

A semicolon separates the clauses. These clauses are new:
- date_until, at_war, allied, owner, church and after.

These effects are new:
- seize: an officer takes the rule of his faction;
- rename: the faction gets a new English name;
- target: the AI of the faction marches on a city for some months;
- city_loss: a city loses a percent of its walls and its troops.

The unit tests of each clause and of each effect are in
tests/unit/playable_tests.cpp. The tests of each row are in
tests/unit/story_tests.cpp.

## The keys of officers
A row of the Europe packs names an officer by the English key. The engine
changes each key into the form @N and the Big5 name before the sim reads
the row. The sim never sees a key. Thus the sim stays the same for the
Three Kingdoms game.

## The rename
The rename effect does not change the save format. The sim reads the
rows that have started and finds the last rename. The engine gives the key
of that rename to the front end as name_key. The text module then gives
the name. The feature test fires the row of the Mamluks, saves, loads and
checks that name_key is still "mamluk".

## The church
A faction has a church: latin, greek, armenian or muslim. The column
church of forces.tsv gives it. The engine fills it at the start and at each
load of a scenario, because the save does not hold content.

## The story pages
The panel has three pages of the world and one page for each faction. The
page of a faction exists only if story/en.tsv has the row force.<key>.
The panel asks engine_api for the faction keys and sends no command. The
ui test checks that recorder_count_commands is 0.

A locale that has no story file uses the English file. Thus the packs
have no zh-TW file. The text is in the Latin script (Q-E4).

## How to add a page or a row
1. Write the test first. A row needs a test in tests/unit/story_tests.cpp.
2. Add the row to history.tsv, or the key to story/en.tsv.
3. Run `run_tests.bat`. Each layer prints PASS.
