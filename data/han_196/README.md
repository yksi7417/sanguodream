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
| The map | misc/make_world_map.py from NOAA ETOPO 2022 heights and Natural Earth vectors. Both are public domain. The rivers follow the widths of the placeholder map (docs/MAPS.md, "The rivers"). 30% of the land is relief cliff, and armies cross the ranges in valleys and at gates (docs/MAPS.md, "The mountains and the bases"). |
| The 42 cities, 10 passes and 35 ports | The places of the novel Romance of the Three Kingdoms (public domain), in map/source/sites.tsv. See "The names of the places". The pack chose the list. |
| The 20 factions and 161 officers | The history of 196, from the Records of the Three Kingdoms (public domain). |
| The abilities | The tiers of scenarios/y196/source/officers.tsv and the rubric of misc/make_tier_officers.py. |
| The lives | lore/en.tsv. The pack wrote each life in its own words. |
| The troops, gold and food of the cities | A design of the pack, in scenarios/y196/cities.tsv. |

No number of this pack comes from the files of the original game. To change
an officer, edit source/officers.tsv and run the two tools:

    python -X utf8 misc\make_tier_officers.py data\han_196\scenarios\y196
    python -X utf8 misc\make_scenario.py data\han_196\scenarios\y196

## The names of the places
The pack uses the names of the novel, so that a player knows the places.
The places of the major battles of the novel come first.
- Cities: the novel names replace the names of the Han records. Examples
  are 許昌 for 許, 天水 for 漢陽 and 白帝 for 魚復.
- Three cities of famous battles replace three quiet seats. 柳城 (the
  battle of 白狼山) replaces 代. 合肥 replaces 廬江, and 建業 replaces 丹陽.
- Passes: 盧龍塞, 壺關, 祁山, 虎牢關, 潼關, 武關, 陳倉, 陽平關, 劍閣 and
  街亭. Each gate guards its owner city against the city across it. The
  across column of sites.tsv gives that city.
- Ports: the river ports of the great battles, for example 赤壁, 烏林,
  夏口, 濡須口, 樊城, 夷陵 and 柴桑. The fords of the Yellow River are
  白馬津, 延津, 黎陽 and 倉亭.
- These coast ports keep a real Han name that the novel does not use:
  句章, 東冶, 揭陽 and 鹽瀆. The novel names no port on those coasts.

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
