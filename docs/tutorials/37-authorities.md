# 37 Authorities: the church and the two caliphs

Files:
- code/sim/authority.cpp: the rules, the two commands and the AI;
- code/sim/content.cpp: authorities.tsv, the column authority of forces.tsv and the new history effects;
- code/sim/history.cpp: the effects see_holder, authority_seat and interdict;
- code/sim/turn.cpp: the call of sim_run_authority_month each month;
- code/engine/engine_rivals.cpp: authority_count, authority_at, recognition and the two commands;
- code/engine/engine_saves.cpp: the X list authorities;
- code/ui/ui_rivals.cpp: the authority section of the ruler window;
- data/medieval_west/authorities.tsv, data/medieval_east/authorities.tsv;
- the rows of history.tsv and rules.tsv of the two packs.

Tests: tests/unit/authority_tests.cpp, tests/unit/story_tests.cpp,
tests/feature/authority_tests.cpp, tests/feature/save_compat_tests.cpp and
the ui tests "authority_panel_reads_authorities_and_commands_nothing" and
"ask_crown_button_issues_one_command_ask_crown".

## What it is for
This tutorial builds stage E6 of docs/EUROPE.md. In the Three Kingdoms
game, the Han emperor gives legitimacy. In the packs of 1200, seven
authorities replace him: the five sees of the Pentarchy and two caliphs.

## The data
A pack with the rule legitimacy.model = authorities has authorities.tsv.
Each row has a key, the seat, the church and the name of the holder. The
seat is a city key or a dash. A dash means that the seat is not on the map.

The column authority of forces.tsv gives the authority of each faction.
The loader stores it as an index plus 1. The value 0 means no authority.

## The monthly rule
The function sim_run_authority_month runs each month. The owner of the seat
of an authority gains authority.technique points. Its devout officers gain
authority.loyalty. Then each interdict has its month.

## The recognition
The ranks that have needs_emperor need recognition. A faction has
recognition if it keeps its authority, if the keeper is its ally, or if it
has a crown. An interdict removes the recognition. A ruler that starts with
a rank keeps it, because the check runs only when a rank rises.

## The commands
- ask_crown gives a crown if the keeper has sufficient friendliness. It takes no
  random number.
- interdict is a command of the owner of Rome. It puts a faction under
  interdict for interdict.months.

Both commands call sim_check_ruler_common. They refuse in a duel, a debate
or an event that waits for an answer. The feature tests close a debate before they command.

## The effects of history.tsv
- see_holder:AUTHORITY:CHURCH changes the church of a holder.
- authority_seat:AUTHORITY:CITY moves a seat. Baybars moves the caliph to
  Cairo in 1261.
- interdict:MONTHS puts the faction of the row under interdict.

## The save
The state of the authorities is the struct sim_authorities. It has the
moved seats, the changed holders, the crowns and the interdicts. The X list
authorities in engine_saves.cpp stores it. An old save has no such list.
Its state loads at zero.

The replay checksum hashes the state only in a pack that has authorities.
Thus a Three Kingdoms game keeps the checksum of before E6.

## The panel
The ruler window has a section for the authorities. It reads authority_count,
authority_at and recognition through engine_api. The button "Ask for a
crown" sends one ask_crown command. The ui tests check the calls with the
recorder, and they check that the panel commands nothing.

## How to add an authority
1. Write the test first. The unit test needs a row of authorities.tsv.
2. Add the row, and the column authority of the factions that it serves.
3. Add the strings authority.<key> to the text files.
4. Run `run_tests.bat`. Each layer prints PASS.
