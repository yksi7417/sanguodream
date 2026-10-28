# Medieval Europe content pack

This pack is the prototype of a map from real geography (docs/MAPS.md).
It shows Europe and North Africa in the year 1200 with the rules of the
placeholder pack.

misc/make_world_map.py writes these files from map/source/region.tsv and
map/source/sites.tsv:
- map/hexes.tsv and map/bases.tsv: the map, 372 x 213 hexes of 13.6 km;
- cities.tsv: the 42 cities, with their specialty, slots and Big5 names;
- bases.tsv: the 10 passes and the 35 ports, with their Big5 names.

The pack has its own scenario in scenarios/y1200: 24 factions and 103
historical officers in 1200. scenarios.tsv lists it. misc/make_scenario.py
checks the files of the scenario and writes the Big5 names of the
officers (docs/MAPS.md, "The scenario of a pack").

The other tables are copies. These come from data/placeholder: the rules,
the terrain, the units, the weapons and the movement costs. rules.tsv
changes one value: game.end_year is 1360, not 351. These come from
data/civil_war, because they contain no Three Kingdoms people:
ancient_officers.tsv, events.tsv, history.tsv and portraits.tsv.

The heights come from NOAA ETOPO 2022. The coasts, the lakes, the rivers
and the deserts come from Natural Earth. Both are public domain
(docs/ASSETS.md, "Map data"). The names, the years and the factions of
the officers are historical facts. Their abilities are a design of the
pack.

To start the game with this pack:

    build\win32_hexdom.exe --pack medieval_europe
