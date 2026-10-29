# Stage E8 step 4: the new factions of the east map

Add the new factions of the east map in docs/EUROPE.md, E8, "The parts in
the two maps". Each new faction gets its cities, its officers, their lives
and their Wikipedia links.

## The goal
- The east game of 1200 starts with six new factions:
  1. the kingdom of Jerusalem, in Acre;
  2. the principality of Antioch, in Antioch;
  3. the Armenian kingdom of Cilicia, in Sis;
  4. the kingdom of Cyprus, in Nicosia;
  5. the kingdom of Georgia, in Tbilisi;
  6. the Abbasid caliphate, in Baghdad.
- Each new faction has a ruler and two officers or more. Each officer has
  a life in lore/en.tsv, a native name and a Wikipedia URL, or "-".
- The game starts and the AI plays two turns.
- data/medieval_europe, data/medieval_west and the Three Kingdoms game do
  not change.

## The facts
- Read docs/EUROPE.md, E3, E4 and E8, and docs/europe/SOURCES.md first.
- Each fact of a life needs two sources (docs/EUROPE.md, "The sources").
  If the sources do not agree, keep the data and add a line to "The data
  to check".
- The east pack has 11 factions with the force ids 0 to 10. The new
  factions get the force ids 11 to 16, in the order of the goal.
- These cities of the east map have no owner now: Sis (33), Antioch (34),
  Baghdad (38), Acre (40) and Nicosia (41). Tbilisi is not a city of the
  map.
- Aimery of Lusignan was the king of Jerusalem and the king of Cyprus in
  1200. Check this fact in two sources. If it is true, give Acre to
  Aimery and give Nicosia to another officer of the Lusignan family. The
  two factions are then allies at the start, if the scenario files can
  give a relation. If they cannot, add a line to "The data to check".
- Bohemond III held Antioch in 1200 and died in 1201. Bohemond IV and
  Raymond-Roupen then fought for the city. Leo I of Cilicia helped
  Raymond-Roupen.
- The key al_nasir is the Almohad caliph Muhammad al-Nasir. The Abbasid
  caliph al-Nasir li-Din Allah needs another key, for example
  al_nasir_abbasid. A key never changes after it is in a pack.
- The keys of a person stay the same in all packs. If an officer of the
  prototype pack serves a new faction, use the key of the prototype.
- officers.tsv of the east pack has columns for the native name and for
  the Wikipedia URL. misc/make_scenario.py checks the rows.
- Q-E2 asks if the caliph is an authority of the church. This stage makes
  the caliphate a faction only. It does not answer Q-E2.

## The work
1. Replace the city Bologna (id 19) with Tbilisi at 41.69 N, 44.80 E. Keep
   the id 19. Give Tbilisi a new key, an English name and a Chinese name.
   Run misc/make_world_map.py for medieval_east. Look at the PNG. Make
   sure that Tbilisi has a road to Trebizond or to Sivas.
2. Add the six factions to scenarios/y1200/forces.tsv and give each a
   colour in faction_colours.tsv.
3. Give each faction its city in scenarios/y1200/cities.tsv, with troops,
   gold and food on the scale of the other cities. A small faction gets
   6000 to 12000 soldiers.
4. Research the officers. Give each faction its ruler of 1200 and two to
   four officers who served it between 1195 and 1215. Add each officer to
   officers.tsv with the abilities of a design, as the other rows.
5. Add the life of each new officer to lore/en.tsv and its URL to
   lore/links.tsv. Add the sources to docs/europe/SOURCES.md.
6. Run misc/make_scenario.py for the scenario of the east pack.
7. Add a row for each new faction to docs/europe/arms.tsv and to the
   table of docs/europe/ARMS.md. Find a file on Wikimedia Commons and
   read its licence through the Commons API. Prefer public domain or CC0.
   If the only file is CC BY-SA, put it in the table with its licence and
   do not use it in the game (Q-E3).
8. Update E8 in docs/EUROPE.md: the factions, the officers and the cities
   without an owner. Update the README.md of the pack.

## The tests
- Change the test of the east pack in tests/feature/world_map_tests.cpp
  to 17 factions. The test now checks that Antioch has no owner. Change
  this check to the principality, and add checks for the owners of Acre
  and Baghdad. Write the change first and see it fail.
- The tests of medieval_europe and medieval_west stay green with no
  change.
- run_tests.bat passes.
- Put the new picture of the east map in docs/maps/medieval_east.png.
