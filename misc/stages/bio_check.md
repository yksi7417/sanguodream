# Stage: a second source for each life

Check the lives of the 103 officers of the 1200 game against a second
source. This stage changes data and documents only, not code.

## The goal
- Each life in data/medieval_europe/lore/en.tsv agrees with two sources.
- The years of officers.tsv agree with the sources, or the doc lists the
  difference.

## The facts
- The first research checked each life against the English Wikipedia
  article only. Some details came from the memory of a model.
- docs/EUROPE.md, "The data to check", lists the known differences.
- Good second sources:
  - Encyclopaedia Britannica;
  - the Catholic Encyclopedia, for the clergy;
  - the Dictionary of National Biography, for the English;
  - Treccani, for the Italians;
  - Deutsche Biographie, for the Germans;
  - the Wikipedia of the language of the officer.

## The work
1. For each officer, find a second source with WebSearch and WebFetch.
2. If a sentence of a life has no support in two sources, change or remove
   the sentence. Write in your own words. Copy no sentence of a source.
   Keep each life at three or four sentences.
3. Change a year of officers.tsv only if two sources agree on the new year.
   Then run `python -X utf8 -I misc/make_scenario.py
   data/medieval_europe/scenarios/y1200`.
4. Update "The data to check" in docs/EUROPE.md: remove the items that you
   solved, and add the new differences. Add a list of the second source of
   each officer to a new file docs/europe/SOURCES.md.
5. Run all tests. The feature test of the world map reads officers.tsv.
