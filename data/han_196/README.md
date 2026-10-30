# Han 196 content pack

This pack is a Three Kingdoms game without the data of the original game. It
is step 1 of "The path to a public game" in docs/MULTIPLAYER.md.

The pack starts in the tenth month of 196. In that year Cao Cao took Emperor
Xian from Luoyang to Xu. Lü Bu held Xiapi, and Liu Bei had moved to Xiaopei.
Sun Ce took the lands south of the Yangtze. The original game has no scenario
of 196.

## The parts of the pack and their sources
| Part | Source |
|---|---|
| The map | misc/make_world_map.py from NOAA ETOPO 2022 heights and Natural Earth vectors. Both are public domain. The rivers follow the widths of the placeholder map (docs/MAPS.md, "The rivers"). |
| The 42 cities, 10 passes and 35 ports | The seats of the Han commanderies and the passes of the history, in map/source/sites.tsv. The pack chose the list. |
| The 20 factions and 161 officers | The history of 196, from the Records of the Three Kingdoms (public domain). |
| The abilities | The tiers of scenarios/y196/source/officers.tsv and the rubric of misc/make_tier_officers.py. |
| The lives | lore/en.tsv. The pack wrote each life in its own words. |
| The troops, gold and food of the cities | A design of the pack, in scenarios/y196/cities.tsv. |

No number of this pack comes from the files of the original game. To change
an officer, edit source/officers.tsv and run the two tools:

    python -X utf8 misc\make_tier_officers.py data\han_196\scenarios\y196
    python -X utf8 misc\make_scenario.py data\han_196\scenarios\y196

## The names of the skills, the tactics, the plots and the techniques
The rule tables are copies of data/medieval_east. The rules of each row stay,
because the law does not protect the mechanics of a game. The names change.

text/zh-TW.tsv and text/en.tsv of this pack give 164 new names: 100 skills,
19 tactics, 9 plots and 36 techniques. The text module looks in these files
first and then in the strings of data/placeholder. Thus a row of this pack
replaces the row of the placeholder pack with the same id (code/text/text.h). The column
name_zh of skills.tsv, tactics.tsv, plots.tsv and techniques.tsv has the
same new names.

These parts keep their names:
- the items, for example 赤兔馬 and 青龍偃月刀. The names come from the
  history and from the novel 三國演義, and both are in the public domain;
- the facilities, the buildings and the terrain. Their names are common
  words, for example 市場 and 農場;
- the official posts and the ranks. They are the titles of the Han court.

## The parts that are not yet original
- The UI text of data/placeholder/text. Most rows are common words. Before a
  public release, read the file for text that the original game wrote.
- The help text and the descriptions of the skills, if a later milestone
  adds them.

## Start the game with this pack

    build\win32_hexdom.exe --pack han_196

The scenario list of the title also has the row 一九六年 天子東遷.
