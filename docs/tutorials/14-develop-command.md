# 14 The develop command: facilities, slots, periods and refusals

Files: code/sim/tables.h, code/sim/content.h, code/sim/content.cpp,
code/sim/develop.cpp, code/sim/turn.cpp (the leader rule),
code/engine/engine.cpp, code/ui/ui_city.cpp, data/placeholder/facilities.tsv,
data/placeholder/cities.tsv, data/placeholder/rules.tsv.
Tests: tests/unit/content_tests.cpp, tests/unit/develop_tests.cpp,
tests/feature/develop_tests.cpp, the City and Develop tests in
tests/ui/ui_tests.cpp, the demo "demo/m2_turns".

## What it is for
This tutorial covers the first command of a city: 開發 (develop). The
officers of a city build a facility in a free slot. The command costs gold
and action points, and it takes a period of turns. The specs are
mechanics/city-commands.md and mechanics/cities-and-facilities.md. The
features are:
- F-020, the city data;
- F-021, the develop command;
- F-022, the adjacency bonus of the mint and the granary;
- F-057, the rule that picks a governor.

## The tables of the content pack
code/sim/tables.h is the TSV parser. The content pack and the fixture
runner share it. The parser does no I/O: the engine reads the file, and
`tsv_parse` cuts the text in place. A line that starts with "#" is a
comment.

The engine reads three tables at start (`engine_load_content`):
- facilities.tsv: one row for each facility. The columns are the cost, the
  durability, the shortest period, the effect, the value and the limit.
  The 10 base facilities and the 10 PK facilities are in it.
- cities.tsv: the 42 cities with their specialty, slot count and port.
- rules.tsv: the numbers of the turn rules, each with a status of
  "documented" or "assumed".

The manual and the guide book document the costs, the durabilities and
the shortest periods of the 10 base facilities. The engine assumes the
periods of the PK facilities, because the manual gives none. It also
assumes the gold and grain values (Q-021). The manual documents the
ratios of the PK markets. The great market gives 3 times a market. The
fish market gives 2 times, and the black market 0.8 times.

If a table does not load, `content_status` names it. The develop
command then refuses with `SIM_REFUSAL_NO_CONTENT`.

## The slots
Each city has the slot count of cities.tsv (guide book p.25). On the real
map (M10, tutorial 22) the slots are the slot hexes of the map. On the
placeholder map the engine uses a placeholder layout (`sim_city_slot_hex`). Slots 0 to 5 are the ring of
hexes next to the city. Slots 6 to 17 are the second ring. Slots 18 to 23
are the third ring. The city stands on a grid of the placeholder map.

A slot is empty, under construction, or built (`sim_slot_state`). The save
does not give the facilities of a city in a decoded form. Thus a loaded
city starts with empty slots.

## The period
The manual (p.11) gives three points. One officer with the politics of
曹操 (94) needs 30 days. Three good officers need 20 days. Officers with
low politics need 60 days. The guide book (p.9) gives the range: from the
shortest period of the facility to 100 days.

`sim_develop_days` draws a straight line between these points. A politics
sum of 0 needs 100 days (`develop.max_days`). A sum of 100 or more needs
the shortest period (`develop.full_politics`). The result goes up to whole
turns. For a market the line gives 30 days for 94, 20 days for 100 and 60
days for 50. Thus it meets all three points. The line is an assumption
(Q-142).

## The checks and the refusal reasons
`sim_check_develop` checks the order in this sequence. The first reason
that holds is the answer:

| Refusal | When |
|---|---|
| `no_content` | the tables did not load |
| `no_city` | the city is not loaded or has no owner |
| `not_your_turn` | the owner of the city does not act now |
| `no_facility` | no such facility, or the city cannot have it (big city only, port only, an item) |
| `enemy_near` | an enemy unit is within 2 hexes of the city |
| `no_slot` | the city has no free slot |
| `value_at_maximum` | the city has its one facility of a kind with a limit |
| `slot_taken` | the chosen slot is not free or not in the city |
| `no_officer` | the order names no officer |
| `officer_not_in_city` | an officer is not an active officer of the owner in the city |
| `duplicate_officer` | the order names an officer two times |
| `officer_already_acted` | the officer gave a command in this turn |
| `officer_busy` | the officer works on a job |
| `no_action_points` | the pool has fewer points than 20 |
| `no_gold` | the city has less gold than the cost |

The spec does not give the sequence of the checks. It documents only the
reasons. 銅雀台 needs the item 銅雀, and the engine has no items yet
(F-059). Thus 銅雀台 always refuses with `no_facility`. Every other force
is an enemy until diplomacy exists (M7).

## The command and the job
`command_develop` checks first. A refused order gets no command id and
ledgers nothing. A good order does these steps:
1. It pays the gold of the city and the 20 action points of the corps.
   The spec says that a command pays when it starts.
2. It marks the slot as under construction, with the period in turns.
3. It marks each officer: the officer acted in this turn and has a job.
4. It emits `develop_started`, `gold_spent` and `action_points_spent`.

At the start of each turn `sim_progress_jobs` takes one turn from each job.
At 0 the facility is complete. The engine emits `facility_completed` and
`officer_released` for each officer. The officers are then free.

## The mint and the granary
A market next to a built mint gives 1.5 times its gold. A farm next to a
built granary gives 1.5 times its grain. The spec documents both numbers
(`adjacency.percent`). "Next to" means a hex distance of 1 in the slot
layout. The great market, the fish market and the black market get no
bonus. The military farm gets no bonus from a granary. The engine assumes
some details (Q-148):
- A mint under construction gives no bonus.
- Two mints next to one market give the same bonus as one.

`sim_city_gold_income` adds the initial income of the city to the value of
each facility. The gold income event on day 1 uses this sum (tutorial 13).

## The governor
The governor (太守) of a city is the leader of the active officers of the
owner whose base is the city. The rule of factions-corps-and-posts.md is:
1. The ruler or the viceroy comes first.
2. Then the post that commands more soldiers.
3. Then higher leadership, then higher force, then higher merit.

The loader does not decode the post of an officer. Thus `sim_officer_leads_before`
skips step 2 (Q-147). A full tie goes to the smaller officer id (assumed).
The same rule picks the leader of corps 1, and later the commander of a
unit. `city_slots` gives the governor of each city.

## engine_api
- `content_status`, `facility_kind_count` and `facility_kind_at` show the
  tables.
- `city_slots(city)` gives the slot count, the slots in use, the incomes,
  the specialty and the governor.
- `facility_at(city, slot)` gives the state, the facility, the turns left
  and the yield of one slot.
- `can_issue(command)` gives the refusal reason, or the gold, the action
  points and the period.
- `command_develop(city, facility, officers[3], slot)` issues the command.
- `place_unit` puts a placeholder unit on the map. It exists only to test
  `enemy_near` until M5.

## The panels
The City panel shows one city: the owner, the gold, the slots in use, the
incomes, the governor and a table of the slots. Its "Develop..." button
opens the Develop dialog. The dialog has a combo for the facility, the
slot and each of the three officers. It asks `can_issue` each frame and
shows the cost and the period, or the refusal reason. A refused command
disables the "Execute" button. One click of "Execute" sends one
`command_develop`.

## The tests, line by line
- content_tests.cpp: the parser with comments, CRLF and empty cells. The
  facility, city and rule tables, and the bad rows that refuse a table.
- develop_tests.cpp (unit): the three points of the manual. A line that
  never gives a longer period for more politics. The rings of slots and
  the mint bonus on a synthetic city.
- develop_tests.cpp (feature), the tables: each facility has the documented
  cost, durability and period. Each city has its slot count and specialty.
  The governor of each city follows the rule.
- develop_tests.cpp (feature), the refusals: one check for each reason.
  A refused develop changes nothing.
- develop_tests.cpp (feature), the completion: `can_issue` gives the
  period of the politics sum. The farm is under construction for that
  number of turns. Then it is complete and the officer is free.
- develop_tests.cpp (feature), the adjacency: a market next to a mint gives
  1.5 times the gold of a market away from it. The income event of the
  next month has the bonus.
- ui_tests.cpp: the City panel reads each slot and commands nothing. The
  Develop dialog sends one `command_develop` with the chosen facility,
  slot and officers. A refused command disables the button and sends
  nothing.
- demo/m2_turns: load the sample, open a city of the player, develop a
  market, advance turns until it is complete, and scroll the ledger.

## Limits
- The slot layout is a placeholder (Q-145).
- The level of a facility is always 1. The merge (F-024) comes in M4.
- The effects other than gold and grain have no logic yet (conscription,
  production, offices).
- The command raises no politics (F-051). Tutorial 15 adds the technique
  points and the merit of the command (F-005).

## Exercises
- Add the "dismantle" command: it frees a slot and costs 10 action points.
- Show the slot layout as hexes in the City panel, with a colour for each
  state.

The result you can check: `run_tests.bat --demo` prints ALL PASS, and
build\demos\m2_turns.mp4 shows a market that is complete after its period.
