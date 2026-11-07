# Stage E9: the next generation of officers

Add the accepted rows of docs/europe/E9-CANDIDATES.tsv to the three packs
of 1200. The plan is docs/EUROPE.md, E9.

## The goal
- Each pack of 1200 keeps 42 active officers or more after 30 years of a
  game of the AI alone. The target is 60.
- Each new officer has a life in lore/en.tsv, a native name, a Wikipedia
  URL and two sources in docs/europe/SOURCES.md.
- An heir joins the faction of the family through a row of history.tsv,
  if the player accepted it in Q-E11.
- The officers of 1200 and the Three Kingdoms packs do not change.

## The facts
- Read docs/EUROPE.md, E3, E4, E8 and E9, and docs/europe/SOURCES.md
  first.
- Use only the rows of E9-CANDIDATES.tsv with the column review "accept".
  A row without that word waits for the player.
- A new officer has the status free and the force "-". The loader refuses
  a force for an officer who appears after 1200. The column appear gives
  the year, and the column of the city of each map gives the city.
- A faction can be on two maps and on the prototype map. Add the officer
  to each pack of the column packs, with the city of that map.
- The keys of a person stay the same in all packs. A key never changes
  after it is in a pack.
- The ids of officers.tsv go on from the last id of each pack.
- Each fact of a life needs two sources. If the sources do not agree,
  keep the data and add a line to "The data to check".

## The work
1. Write the feature test first: each pack of 1200 plays 30 years with
   the AI alone, and keeps 42 active officers or more. See it fail. The
   numbers before the work are in E9, "The problem".
2. For each accepted row, add the officer to officers.tsv of each pack
   in the column packs. Give the five abilities from the tiers of E9,
   "The abilities". Give the aptitudes and the skill from the role.
3. Set the column devout from the life (Q-E10).
4. Add the life to lore/en.tsv and the sources to docs/europe/SOURCES.md.
5. For each heir, add a row of history.tsv with the clauses
   date_from:YYYY/1, free:@key and ruler:@parent, and the effect
   join:@key:100:MERIT. Give MERIT from the tier of the best ability.
6. Run misc/make_scenario.py for the scenario of each pack. Look at the
   report of the tool for each refused row.
7. Update E9 in docs/EUROPE.md with the new counts, and the README.md of
   each pack.

## The tests
- The new feature test of step 1 passes.
- The tests of the world maps in tests/feature/world_map_tests.cpp stay
  green. Change a count of officers only if the test counts them.
- run_tests.bat passes, and run_tests.bat --demo-fast passes.
