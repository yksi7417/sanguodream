# 29 Content source: the install of the player and the source of each asset

Files: code/engine/engine_content.cpp, code/engine/engine.h (the M16
block), code/ui/ui_content.cpp, code/ui/ui_court.cpp (the portrait
frame), data/placeholder/text/en.tsv and zh-TW.tsv (the "content."
strings). Tests: tests/feature/content_source_tests.cpp,
test_make_install_folder in tests/test_files.h, the "content_panel_" and
"officer_view_" tests in tests/ui/ui_tests.cpp, and the demo
"demo/m16_content".

## What it is for
The player owns a copy of the original game. The game can read the content
of that copy at run time. Then the repository needs no file of the
original. This tutorial builds milestone M16. The player names the install
folder. The engine reads the files that the spec documents, and it tells
the source of each asset.

## What the spec gives
formats/scenario-file.md gives the layout of two kinds of file in
media\scenario of the install:
- Scenario.s11, the name index. It gives the names of the 42 cities and
  of the 45 gates and ports.
- SCEN000.s11 to SCEN013.s11, the scenarios. The header of each file gives
  the scenario number, the start year, the start month and the title.

formats/binary-identity.md names the message files media\msg\S11MSG00.s11
to S11MSG03.s11. It also says "Asset formats are out of scope". The spec
has no layout of the message files, of the portraits in media\face, or of
the music. Thus the engine does not decode these files. The Status block
of M16 in docs/ROADMAP.md lists these gaps as open points.

## The asset kinds and the sources
The engine knows five asset kinds (engine_asset_kind) and three sources
(engine_source_kind):

| Asset | Source without an install | Source with an install |
|---|---|---|
| city names | none, or the pack after load_name_index | the install: Scenario.s11 |
| scenarios | the pack: scenarios.tsv | the install: the SCEN files |
| messages | the pack: text/<locale>.tsv | the pack |
| portraits | none | none |
| music | none | none |

content_source(asset) gives an engine_content_source:
- source: the source of the asset now;
- count: the entries that the source gave, for example 42 names;
- install_files: the files of this asset that the engine found in the
  install. For the messages, the engine counts S11MSG00..03.s11 only;
- detail: a short static text that says why;
- path: the file that gave the city names.

## set_install_folder
The player types the folder, for example
C:\Program Files (x86)\Steam\steamapps\common\San11WPK. The engine then
does these steps (engine_api_set_install_folder):
1. It reads SCEN000.s11 to SCEN013.s11 with scenario_load. Each file that
   loads gives one engine_install_scenario row.
2. It loads Scenario.s11 through engine_api_load_name_index. This is one
   command with one ledger entry, as in tutorial 9.
3. If no name index and no scenario file loads, it refuses the folder. A
   refused folder changes nothing.
4. It keeps the folder and the rows. The source of the city names becomes
   the install.
5. It counts the message files that the platform reader can read.

An empty folder clears the install. The scenario list then comes from the
pack again. The city names stay in the game, because a name load is a
command of the game.

install_scenario_count and install_scenario_at give the rows. The file of a
row goes to load_scenario of tutorial 21. That starts a new game from the
scenario of the install.

The platform reader has no other test for a file. Thus a message file that
is larger than the free transient arena counts as absent.

## The Content panel
The debug menu has the item "Content" (code/ui/ui_content.cpp). The panel
has:
- the input "Install folder", with the buttons Set and Clear;
- the table content_sources: one row for each asset kind;
- the table install_scenarios: the number, the title, the start date and a
  Start button for each scenario of the install. The title is Big5 in the
  file. text_decode_big5_into of tutorial 12 decodes it.

The officer view of the Court panel frames the portrait of the officer. No
source gives a picture yet. Thus the frame shows the first character of the
name. A line beside the frame names the source that content_source gives.
The item id of the frame has the source in it, for example
"###portrait_0".

## The tests
- feature/content_source (31 checks):
  - without an install, each asset has the source of the table above;
  - the engine refuses a folder without scenario files, and the refusal
    changes nothing;
  - with the test install, the city names are the names of Scenario.s11 of
    the spec, and the scenario row is 群雄割據 of 194, month 6;
  - load_scenario starts a game from the file of the row;
  - the engine finds one message file, but the messages stay with the pack;
  - Clear gives the scenario list of the pack again and ledgers nothing.
- ui:
  - content_panel_sets_and_clears_the_install_folder: Set and Clear send
    one set_install_folder each, with the typed folder and with "";
  - content_panel_starts_a_scenario_of_the_install: Start sends one
    load_scenario with the file of the row;
  - officer_view_shows_a_portrait_of_each_source: the frame has the id of
    each scripted source, and the view commands nothing.
- demo/m16_content: the walkthrough names the test install in the Content
  panel, starts its scenario and shows the portrait frame in the Court
  panel.

test_make_install_folder makes the test install in the build folder. It
copies Scenario.s11 and SCEN002.s11 from spec-src/spec/fixtures/scenarios.
It writes a short file media\msg\S11MSG00.s11 that the engine only counts.

## The generated portraits
The branch feat/default-portraits has the generated portraits of
data/civil_war/Portraits.zip (docs/ASSETS.md on that branch). This run did
not merge the branch. When the branch merges, the UI can give the source
"pack" to the portraits of the archive. The frame of the officer view then
shows the picture in place of the first character.

## Check it
1. Run build.bat and run_tests.bat. The report shows
   "PASS 31/31 feature/content_source".
2. Start build\win32_hexdom.exe and open Content in the debug menu.
3. Type the folder of your install and press Set. The rows "city names"
   and "scenarios" show the source "Install".
