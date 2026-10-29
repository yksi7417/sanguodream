# Medieval east content pack

This pack is the east map of the game of 1200 (docs/EUROPE.md, E8). It shows
the east of Europe, Anatolia, the Levant, Egypt and Mesopotamia as far as
Baghdad. The crusades are at its centre.

The box goes from 8 E to 46 E and from 28.5 N to 49 N. The map has 366 x 217
hexes of 10.7 km. The 42 cities, the 10 passes and the 35 ports are in
map/source/sites.tsv.

Six factions of the east have their own cities: Jerusalem (Acre), Antioch,
Cilicia (Sis), Cyprus (Nicosia), Georgia (Tbilisi) and the Abbasid caliphate
(Baghdad). Aleppo and Damascus belong to the Ayyubids. Six cities have no
owner: Barqa, Milan, Genoa, Florence, Belgrade and Mosul.

misc/make_world_map.py writes these files from map/source/region.tsv and
map/source/sites.tsv:
- map/hexes.tsv and map/bases.tsv: the map;
- cities.tsv: the 42 cities, with their specialty, slots and Big5 names;
- bases.tsv: the 10 passes and the 35 ports, with their Big5 names.

The pack has its own scenario in scenarios/y1200: 17 factions and 65
historical officers in 1200. scenarios.tsv lists it. misc/make_scenario.py
checks the files of the scenario and writes the Big5 names of the
officers (docs/MAPS.md, "The scenario of a pack").

The English keys of the officers, the forces and the cities are the same as
in data/medieval_europe. Thus lore/en.tsv, names/native.tsv and lore/links.tsv
carry over by key. The pack starts in English: pack.tsv sets default_locale
to en.

The Almohad ruler and his officers stand in Tunis, because Marrakesh is not
on this map. The other tables are copies of data/medieval_europe. The heights
come from NOAA ETOPO 2022. The coasts, the lakes, the rivers and the deserts
come from Natural Earth. Both are public domain (docs/ASSETS.md, "Map data").
The names, the years and the factions of the officers are historical facts.
Their abilities are a design of the pack.

To start the game with this pack:

    build\win32_hexdom.exe --pack medieval_east

authorities.tsv lists the seven authorities of the game of 1200 (docs/EUROPE.md, E6). The seats of this map are Rome, Constantinople, Alexandria, Antioch, Acre and Baghdad. The Almohad caliph has no seat here.
