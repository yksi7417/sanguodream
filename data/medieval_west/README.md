# Medieval west content pack

This pack is the west map of the game of 1200 (docs/EUROPE.md, E8). It shows
the west of Europe and the Maghreb. The war of England and France is at its
centre.

The box goes from 10.5 W to 15 E and from 30.5 N to 56 N. The map has 268 x 282
hexes of 10.1 km. The 42 cities, the 4 passes and the 41 ports are in
map/source/sites.tsv.

misc/make_world_map.py writes these files from map/source/region.tsv and
map/source/sites.tsv:
- map/hexes.tsv and map/bases.tsv: the map;
- cities.tsv: the 42 cities, with their specialty, slots and Big5 names;
- bases.tsv: the 4 passes and the 41 ports, with their Big5 names.

The pack has its own scenario in scenarios/y1200: 17 factions and 77
historical officers in 1200. scenarios.tsv lists it. misc/make_scenario.py
checks the files of the scenario and writes the Big5 names of the
officers (docs/MAPS.md, "The scenario of a pack").

The English keys of the officers, the forces and the cities are the same as
in data/medieval_europe. Thus lore/en.tsv, names/native.tsv and lore/links.tsv
carry over by key. The pack starts in English: pack.tsv sets default_locale
to en.

The other tables are copies of data/medieval_europe. The heights come from
NOAA ETOPO 2022. The coasts, the lakes, the rivers and the deserts come from
Natural Earth. Both are public domain (docs/ASSETS.md, "Map data"). The
names, the years and the factions of the officers are historical facts. Their
abilities are a design of the pack.

To start the game with this pack:

    build\win32_hexdom.exe --pack medieval_west

authorities.tsv lists the seven authorities of the game of 1200 (docs/EUROPE.md, E6). The seats of this map are Rome and Marrakesh. The other five authorities have no seat here.
