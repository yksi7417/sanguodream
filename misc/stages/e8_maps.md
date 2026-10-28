# Stage E8: the west map and the east map

Make the two packs of E8 in docs/EUROPE.md: data/medieval_west and
data/medieval_east. Each pack has its own map, its own 42 cities and a
scenario of 1200.

## The goal
- The title lists a game of 1200 for each pack. Each game starts and the
  AI plays two turns.
- The west map goes from 10.5 W to 15 E and from 30.5 N to 56 N.
- The east map goes from 8 E to 46 E and from 28.5 N to 49 N.
- data/medieval_europe does not change. Its feature test and its saves
  stay as they are.
- The Three Kingdoms game does not change.

## The facts
- docs/MAPS.md, "Make a map" and "The scenario of a pack", give the steps
  and the rules of the files. Read them first.
- A map has 42 cities, 10 passes and 35 ports. The format of a save fixes
  these counts. Do not change SAVE_CITY_COUNT or the counts of the bases.
- docs/EUROPE.md, E8, gives the factions of each map and the reasons for
  the line. Use only the factions of the prototype in this stage. Do not
  add a new faction. A later stage adds the new factions of the east.
- The English keys of the officers, the forces and the cities stay the
  same in all packs. Thus lore/en.tsv, names/native.tsv and the Wikipedia
  URLs carry over by key. A city of a new map that the prototype does not
  have gets a new key.
- misc/make_world_map.py downloads the heights and the vectors. The main
  checkout C:\src\hexdom\build\geo_cache has a cache of the Natural Earth
  files and of ETOPO for the box of the prototype. Copy these files into
  build\geo_cache of your worktree before the first run. The east box
  needs new ETOPO data, and the tool downloads it.
- The limit of a map is 80000 hexes (max_hexes in region.tsv). The tool
  makes the hexes larger if the box needs more.

## The work
1. Copy data/medieval_europe to data/medieval_west and to
   data/medieval_east. Give each pack its own pack.tsv, README.md and
   map/source/region.tsv with the box above.
2. Choose the 42 cities of each map in map/source/sites.tsv. Keep the
   cities of the prototype that are in the box. Then add cities by these
   rules, in this order:
   - the seat of each faction on the map;
   - the cities of the wars of E8 and E7, for example Rouen, Chinon,
     Acre, Antioch and Aleppo, and Lille near the field of Bouvines;
   - the largest towns of 1200 in Bosker, Buringh and van Zanden.
   Antioch must be a city of the east map. On the east map, Aleppo and
   Damascus belong to the Ayyubids. A city without a faction has the
   owner -.
3. Choose 10 passes and 35 ports of each map by the same method. Get each
   latitude and longitude from Wikidata or from the English Wikipedia.
4. Run misc/make_world_map.py for each pack. Look at build/maps/<pack>.png.
   Keep the distance between cities near 22 hexes.
5. Make scenarios/y1200 of each pack from the scenario of the prototype:
   - Keep the factions of the map. Give them the force ids 0 up, in the
     order of docs/EUROPE.md, E8.
   - Keep the officers of these factions and the free officers in the
     box. Change the force and the city of each officer to the new ids.
   - Give each city of a faction its troops, gold and food on the scale of
     the prototype.
   - Run misc/make_scenario.py for the scenario. It checks the rules and
     writes names/en.tsv and names/native.tsv of the pack.
6. Copy the rows of lore/en.tsv and lore/links.tsv for the officers of the
   pack. Copy the rows of names for the new cities and bases in en.tsv,
   with an English name and a Chinese name.
7. Add one row for each pack to data/placeholder/scenarios.tsv, as the row
   of medieval_europe. Give the west game and the east game a Chinese and
   an English title.
8. Add each pack to the preload of build_web.bat. Read the memory budget
   in docs/WEB.md first. If the two packs do not fit the budget, add the
   question to docs/EUROPE.md and do not change build_web.bat.

## The tests
- A feature test for each pack in tests/feature/world_map_tests.cpp, as
  the test of the prototype. It checks the counts of the map, the keys of
  three cities, loads the scenario and plays two turns of the AI.
- The test of the prototype stays green with no change.
- Update E8 in docs/EUROPE.md: the cities of each map, the hexes, the size
  of a hex and the status in the milestone table. Add the two packs to the
  table "The regions of the planned games" of docs/MAPS.md with the real
  numbers.
- Put the pictures of the two maps in docs/maps, as the prototype has.
