# Medieval Europe content pack

This pack is the prototype of a map from real geography (docs/MAPS.md).
It shows Europe and North Africa in the year 1200 with the rules of the
placeholder pack.

misc/make_world_map.py writes these files from map/source/region.tsv and
map/source/sites.tsv:
- map/hexes.tsv and map/bases.tsv: the map, 372 x 213 hexes of 13.6 km;
- cities.tsv: the 42 cities, with their specialty and slots;
- bases.tsv: the 10 passes and the 35 ports.

The other tables are copies. These come from data/placeholder: the rules,
the terrain, the units, the weapons and the movement costs. These come
from data/civil_war, because they contain no Three Kingdoms people:
ancient_officers.tsv, events.tsv, history.tsv, portraits.tsv and
scenarios.tsv.

The pack has no scenario of its own yet. The scenario list has the sample
save only. Thus the cities show the names and the officers of the sample
save (docs/MAPS.md, step W2).

The heights come from NOAA ETOPO 2022. The coasts, the lakes, the rivers
and the deserts come from Natural Earth. Both are public domain
(docs/ASSETS.md, "Map data").

To start the game with this pack:

    build\win32_hexdom.exe --pack medieval_europe
