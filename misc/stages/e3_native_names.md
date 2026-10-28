# Stage E3: names in their own language

Do the UI part of E3 in docs/EUROPE.md.

## The goal
The Settings panel has a choice "Names" with two values: "English" and
"Own language". "Own language" shows the column name_latin of
data/medieval_europe/scenarios/y1200/officers.tsv for each officer. The
default is "English". The Three Kingdoms game does not change.

## The facts
- The work of E2 is complete. The UI asks text_key_name(catalog, "officer", key) for the
  name of an officer (code/ui/ui_world.cpp, ui_officer_name).
- misc/make_scenario.py writes data/medieval_europe/names/en.tsv. Let it
  also write a second table, for example names/native.tsv, with the
  name_latin of each officer by the same keys.
- The text module (code/text/) loads the tables of the game pack in
  text_catalog_set_locale. A setting of the front end chooses the table.
- The fonts load only Latin, Latin-1 and Vietnamese in code/ui/ui_fonts.h.
  name_latin has letters of Latin Extended-A and Latin Extended Additional,
  for example "ł", "ř", "ā" and "ṣ". Add those ranges. Check that the CJK
  font has the glyphs. If it does not, add a font with an open licence and
  list it in docs/ASSETS.md.
- Do not show the Arabic, Greek, Hebrew or Cyrillic script. That is
  question Q-E4.

## The tests
- A unit test of the text module for the native table.
- A ui test: the Names choice of the Settings panel changes the name of an
  officer with a key, and commands nothing.
- Update E3 in docs/EUROPE.md and the milestone table.
