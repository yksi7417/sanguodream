# Stage E4: the life of an officer in the officer panel

Do the UI part of E4 in docs/EUROPE.md.

## The goal
The officer panel shows the life of an officer of the 1200 game under the
abilities. A button "Wikipedia" opens the English article of the officer.
An officer without a life or without an article shows no text and no
button. The Three Kingdoms game does not change.

## The facts
- The work of E2 is complete. engine_officer has the English key of an officer.
- data/medieval_europe/lore/en.tsv has the life of each officer, keyed by
  officer.<key>. The column wikipedia of officers.tsv has the URL, or "-".
- The UI must not read officers.tsv. Let misc/make_scenario.py write the
  URLs to a table of the pack, for example lore/links.tsv, keyed by
  officer.<key>. Let the text module load lore/<locale>.tsv and
  lore/links.tsv of the game pack, as it loads names/<locale>.tsv.
- To open a link, add a function to the platform contract in
  code/hexdom.h. win32_hexdom.cpp opens the URL in the default browser.
  wasm_hexdom.cpp opens it in a new tab. Accept only URLs that start with
  "https://en.wikipedia.org/wiki/".
- Find the officer panel through the officer view of the game mode
  (code/ui/ui_court.cpp and code/ui/ui_content.cpp). Read the code first.

## The tests
- A unit test of the text module for the lore tables.
- A ui test: the panel shows the life, and the button calls the platform
  function with the URL. It commands nothing to the engine.
- A check in misc/make_scenario.py that each URL starts with the prefix.
- Update E4 in docs/EUROPE.md and the milestone table.
