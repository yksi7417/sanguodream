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
| The 20 factions and 193 officers | The history of 196, from the Records of the Three Kingdoms (public domain). 32 of the officers come of age after 196 and appear as free officers. |
| The abilities | The tiers of scenarios/y196/source/officers.tsv and the rubric of misc/make_tier_officers.py. |
| The lives | lore/en.tsv. The pack wrote each life in its own words. |
| The troops, gold and food of the cities | A design of the pack, in scenarios/y196/cities.tsv. |
| The ground tiles and the effects | materials.jpg and fx.png. The owner made them with Gemini (docs/ASSETS.md, "Terrain tiles" and "Effects and icons"). |

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
- Passes: 盧龍塞, 壺關, 井陘, 虎牢關, 潼關, 武關, 陳倉, 陽平關, 劍閣 and
  街亭. Each gate guards its owner city against the city across it. The
  across column of sites.tsv gives that city. See "The gates and their
  purpose".
- Ports: the river ports of the great battles, for example 赤壁, 烏林,
  夏口, 濡須口, 樊城, 夷陵 and 柴桑. The fords of the Yellow River are
  白馬津, 延津, 黎陽 and 倉亭.
- These coast ports keep a real Han name that the novel does not use:
  句章, 東冶, 揭陽 and 鹽瀆. The novel names no port on those coasts.

## The gates and their purpose
Each gate closes a road that history knew as the only way through the
mountains. misc/make_world_map.py checks each gate on the finished map. It
closes the gate hex and finds the land route between the two cities again.
That route must be 1.5 times longer, or 20 hexes longer, or closed
(docs/MAPS.md, "The purpose of a gate"). The last column gives the result of
the map of this pack.

| Gate | Guards, against | The road that it closes in history | Route through the gate, and without it |
|---|---|---|---|
| 盧龍塞 Lulong Fort | 北平, against 柳城 | The pass through the Yan mountains from the Hebei plain to Liaoxi. In 207 Cao Cao took his army through it and defeated the Wuhuan at 白狼山. | 55 hexes, no route |
| 壺關 Hu Pass | 上黨, against 鄴 | The pass of the Taihang mountains from the Hebei plain up to the Shangdang plateau. Shangdang holds the high ground above Ye. | 21 hexes, 67 hexes |
| 井陘 Jingxing | 晉陽, against 南皮 | One of the eight passes of the Taihang mountains, on the road from Bing province to the Hebei plain. Han Xin won the battle of Jingxing there in 204 BC. | 54 hexes, 82 hexes |
| 虎牢關 Hulao Pass | 洛陽, against 陳留 | The pass between the Song mountains and the Yellow River, on the road from the eastern plain to Luoyang. In the novel, the allies fight Lü Bu there in 190. | 30 hexes, 114 hexes |
| 潼關 Tong Pass | 長安, against 洛陽 | The gap between the Qinling mountains and the bend of the Yellow River. It closes the east road into Guanzhong. Cao Cao fought Ma Chao there in 211. | 56 hexes, 211 hexes |
| 武關 Wu Pass | 長安, against 宛城 | The southeast gate of Guanzhong, on the road from Nanyang through the Qinling mountains. Liu Bang went into Guanzhong through it in 207 BC. | 62 hexes, 89 hexes |
| 陳倉 Chencang | 長安, against 漢中 | The north end of the road from Hanzhong through the Qinling mountains to the Wei valley. Hao Zhao held it against Zhuge Liang in 228. | 42 hexes, 163 hexes |
| 陽平關 Yangping Pass | 漢中, against 梓潼 | The pass west of Hanzhong, on the road from Hanzhong to Shu. Cao Cao took Hanzhong from Zhang Lu there in 215. | 39 hexes, 180 hexes |
| 劍閣 Jiange | 梓潼, against 漢中 | The narrow road through the cliffs of 劍門 on the way from Hanzhong to Chengdu. Jiang Wei held it against Zhong Hui in 263. | 39 hexes, 180 hexes |
| 街亭 Jieting | 天水, against 長安 | The road from Guanzhong over the Long mountains to Tianshui. Ma Su lost it to Zhang He in 228, and the first northern campaign of Zhuge Liang failed. | 51 hexes, 150 hexes |

The pack left out 祁山 Qishan. In history, Qishan is a fort on the road from
Hanzhong to Tianshui, and Zhuge Liang went north through it. On the map of
this pack, the route from Tianshui to Hanzhong does not go through the gate.
Without the gate, that route has the same length. Thus the gate guarded
nothing, and 井陘 took its id.

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
