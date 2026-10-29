# Stage E6: the authorities of the church and the two caliphs

Do E6 of docs/EUROPE.md for the packs medieval_west and medieval_east. The
five sees and the two caliphs take the place of the Han emperor in these
packs.

## The goal
- Each pack has seven authorities: Rome, Constantinople, Alexandria,
  Antioch, Jerusalem, the Abbasid caliph and the Almohad caliph.
- The faction that holds the seat of an authority gains technique points
  each month. Its devout officers gain loyalty.
- A ruler needs the recognition of his authority for the rank of king or
  emperor.
- A ruler can ask his authority for a crown. The pope can put a faction
  under interdict.
- An authority panel shows each authority, its seat, the owner of the
  seat and the factions that it recognizes.
- The rows of history.tsv of E7 get their parts for the sees.
- The Three Kingdoms game, its fixtures, its saves and medieval_europe do
  not change.

## The facts
- Read docs/EUROPE.md, E6 with "The two caliphs", and E7 first. Then read
  code/sim/rivals.cpp, the rules of the Han emperor. The new rules follow
  the rules of the emperor.
- The rules of the emperor:
  - rules.tsv has emperor.technique, emperor.loyalty and emperor.city.
  - officers have han_respect: 0, 1 or 2 (sim.h). In the Europe packs,
    the value 2 means a devout officer.
  - ranks.tsv has the column needs_emperor. officers.cpp reads it in the
    rule of the highest rank.
- forces.tsv of each pack has the column church: latin, greek, armenian
  or muslim (E7).
- The seats on the two maps:

  | Authority | West map | East map |
  |---|---|---|
  | Rome | rome, city 11 | rome, city 11 |
  | Constantinople | no city | constantinople, city 6 |
  | Alexandria | no city | alexandria, city 3 |
  | Antioch | no city | antioch, city 34 |
  | Jerusalem | no city | acre, city 40. The Latin patriarch sat in Acre in 1200 |
  | Abbasid caliph | no city | baghdad, city 38 |
  | Almohad caliph | marrakesh, city 6 | no city |

- The save of hexdom is a table of the members of sim_state
  (code/engine/engine_saves.cpp). A new member goes in the X lists of
  engine_saves.cpp and engine_game_replay.cpp. An old save loads with the
  new member at zero. The old saves must load as they do now.
- Take each draw of the PRNG in its own statement (CLAUDE.md).

## The work
1. The data. Add authorities.tsv to each pack. Each row has a key and the
   key of the seat city, or "-". It also has the church of the holder in
   1200 and the name of the holder for the UI. Add the column authority
   to forces.tsv:
   the key of the authority that recognizes the faction.
   - latin factions: rome. Check Cilicia in two sources. Leo I received
     a crown with the consent of the pope in 1198.
   - greek factions: constantinople.
   - muslim factions: abbasid or almohad, by the table of "The two
     caliphs".
2. The model. Add the rule legitimacy.model to rules.tsv, with the values
   emperor and authorities. The placeholder pack, civil_war and
   medieval_europe keep emperor. The west and east packs use
   authorities.
3. The monthly rule. For each authority with a seat on the map, the
   owner of the seat gains technique points, and its devout officers gain
   loyalty. Use new rules in rules.tsv with the amounts of the emperor.
4. The recognition. A faction has recognition in three cases:
   - it holds the seat of its authority;
   - it is an ally of the owner of the seat;
   - its authority gave it a crown.

   Under the model authorities, needs_emperor
   means "needs recognition". Set needs_emperor to yes for the ranks king
   and emperor of the two packs. A ruler who starts with a rank keeps it.
   The rule applies only to a rise.
5. The commands. Add them to engine_api with a feature test first:
   - ask_crown: a ruler asks his authority for a crown. The owner of the
     seat gives it if its friendliness with the ruler is high. If the seat
     has no owner or no city, use the rules of E6 for a seat without a
     city.
   - interdict: the owner of Rome puts a faction under interdict for some
     months. The officers of that faction lose loyalty each month. A
     friendliness above a limit stops the interdict.
   - Give the AI a simple rule for each command. Write the rule in E6.
6. The effects of history.tsv. Add see_holder and authority_seat, each
   with a unit test first.
   - see_holder:<authority>:<church> gives a see a new holder, or "-".
   - authority_seat:<authority>:<city> moves the seat. Baybars moved the
     Abbasid caliph to Cairo in 1261.
7. The rows of the sees. Add these parts to history.tsv:
   - the west map: the interdict on France from January 1200 to September
     1200, as a row with the clause opening;
   - the east map: a Latin patriarch of Constantinople if a latin faction
     holds the city after the Fourth Crusade;
   - the east map: the fall of Antioch removes the Latin patriarch;
   - the east map: if the Abbasid faction loses Baghdad and Baybars rules
     Cairo, the caliph moves to Cairo.
8. The UI. Add the authority panel and a button for ask_crown. The panel
   reads engine_api only. Add the strings to the text files of the
   placeholder pack.
9. The save. Add the state of the authorities to sim_state and to the X
   lists. Load an old save and check it.
10. Update E6 in docs/EUROPE.md with the final rules and the status in the
    milestone table. Add a tutorial in docs/tutorials.

## The tests
- A unit test for each rule and each effect, first, in the module of the
  rule.
- A feature test for each command through engine_api, and a feature test
  for the monthly rule on each pack.
- A feature test that saves and loads a game with an interdict and a
  moved seat.
- A feature test that loads a save of the Three Kingdoms game with no
  change.
- A ui test for the panel and for the button. The button sends one
  ask_crown command and nothing else.
- run_tests.bat and run_tests.bat --demo-fast pass.
