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
| The map | misc/make_world_map.py from NOAA ETOPO 2022 heights and Natural Earth vectors. Both are public domain. |
| The 42 cities, 10 passes and 35 ports | The seats of the Han commanderies and the passes of the history, in map/source/sites.tsv. The pack chose the list. |
| The 20 factions and 161 officers | The history of 196, from the Records of the Three Kingdoms (public domain). |
| The abilities | The tiers of scenarios/y196/source/officers.tsv and the rubric of misc/make_tier_officers.py. |
| The lives | lore/en.tsv. The pack wrote each life in its own words. |
| The troops, gold and food of the cities | A design of the pack, in scenarios/y196/cities.tsv. |

No number of this pack comes from the files of the original game. To change
an officer, edit source/officers.tsv and run the two tools:

    python -X utf8 misc\make_tier_officers.py data\han_196\scenarios\y196
    python -X utf8 misc\make_scenario.py data\han_196\scenarios\y196

## The parts that are not yet original
The rule tables are copies of data/medieval_east, and those tables come from
the placeholder pack. Thus these parts still follow the original game:
- the names of the skills in skills.tsv, for example 飛將 and 神算;
- the names and the values of the tactics, the facilities and the techniques;
- the UI text of data/placeholder/text.

Before a public release, give these tables new names and new values. The
mechanics can stay. The names and the text are the parts that copyright
protects.

## Start the game with this pack

    build\win32_hexdom.exe --pack han_196

The scenario list of the title also has the row 一九六年 天子東遷.
