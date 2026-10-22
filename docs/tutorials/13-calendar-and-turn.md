# 13 Calendar and turn: dates, the turn order and action points

Files: code/sim/calendar.h, code/sim/turn.cpp, code/sim/sim.h,
code/sim/sim.cpp, code/engine/engine.h, code/engine/engine.cpp,
code/ui/ui.cpp (the Simulation panel), data/placeholder/rules.tsv.
Tests: tests/unit/calendar_tests.cpp, tests/unit/turn_tests.cpp,
tests/feature/turn_tests.cpp, the "Advance turn" tests in
tests/ui/ui_tests.cpp, the demo "demo/m2_turns".

## What it is for
This tutorial covers the time of the game. The spec is
spec-src/spec/mechanics/calendar-and-turn.md. The features are F-002 (the
calendar), F-003 (the turn order and the "proceed" button) and F-004 (the
action points). The module answers three questions: what is the date,
which faction acts now, and how many action points it has.

## The calendar
calendar.h has pure functions over `calendar_date`. A date is a year, a
month and the turn of the month (0, 1 or 2). The three turns of a month
start on day 1, 11 and 21. The manual documents these rules (p.7, p.9):
- One turn is 10 days. A month has 3 turns. A year has 12 months.
- Spring is months 1 to 3, summer 4 to 6, autumn 7 to 9, winter 10 to 12.
- `calendar_next_turn` moves one turn. Day 21 of month 12 goes to
  1 January of the next year.
- `calendar_days_between` gives the days between two dates.
- `calendar_turns_for_days` gives the turns of a job. A 20-day job takes
  2 turns. A part of a turn counts as a whole turn.

The header of a save holds the scenario start and a turn counter (Q-004,
save-file.md 0x07C). A load sets the calendar to the start plus one turn
for each count (`calendar_add_turns`). The sample holds 194/6/1 and 120.
Thus the game starts at 197/10/1 (G2-001). A new game without a
save starts on 1 January 190 (`SIM_DEFAULT_START_YEAR`). The header field
of the ten-day period is 1 in every save. The loader reads 1 as day 1.
This is an assumption (Q-144).

## The calendar events
When the date moves, `sim_run_calendar` emits named events into the
ledger. The order is:
1. The jobs of the develop command advance (tutorial 14).
2. On 1 January: `year_started`, the removal of each black market, and
   the hooks `hook_officers_age` and `hook_new_officers`.
3. On day 1 of each month: `month_started`, then `gold_income` and
   `salaries_paid` for each city with an owner, then `hook_bandits`.
4. On day 1 of months 1, 4, 7 and 10: `season_started`, then
   `grain_income` for each city, then the hooks of the disasters.
5. On each turn: `grain_consumed` for each city.

The hooks have a name in the ledger and no logic. F-007 adds the disasters later.
The table of hooks follows the spec:

| Month | Hooks |
|---|---|
| each month | bandits |
| 1 | officers age, new officers, locusts, plague |
| 4 | locusts, plague |
| 7 | good harvest, locusts, plague |
| 10 | plague |

Tutorial 15 adds public order, the income skills, the caps and the free
weapons to the rules below.
The engine assumes the amounts of gold and grain. Each amount comes from
rules.tsv, and each event has `assumed` set. The Event ledger panel shows
"assumed" in its last column. The rules are:
- Gold income on day 1 is `city.gold_income` plus the gold of the
  facilities (Q-021).
- Salaries are `officer.salary` for each active officer whose base is the
  city (Q-027).
- Grain income at a season start is `city.grain_income` plus the grain of
  the facilities (Q-021).
- Each turn a city eats one grain for each `grain.soldiers` soldiers
  (Q-119).
- A city holds at most `city.gold_cap` gold and `city.grain_cap` grain.
  The manual documents these caps.

## The turn order
A faction is a force that owns a city. `sim_compute_turn_order` builds
the order at the start of each turn. `sim_faction_before` compares two
factions:
1. Fewer cities act first (calendar-and-turn.md, unverified).
2. With equal cities, fewer active officers act first
   (knowledge/turn-structure.md, unverified).
3. Then fewer troops act first (the same source).
4. Then the smaller force id acts first. The engine assumes this last
   rule (Q-143).

A player controls a human faction. The AI controls the other factions.
After a load, the faction of the ruler that the header names is human.
The engine compares the Big5 name of the header with the family and the
given name of each ruler. The sample save names 劉表.

## The flow of one turn
`sim_start_turn` computes the order, recovers the action points, and
calls `sim_run_factions_until_human`. Each AI faction passes with the
event `ai_passed`, because the AI phase is a placeholder until M8. The
first human faction then gets `faction_turn_started`, and `whose_turn`
gives its force id.

The "Advance turn" button of the Simulation panel calls `end_turn` one
time. In the game this is the "proceed" button. `sim_end_faction_turn`
emits `faction_turn_ended` for the faction that acts. The next factions
then act. After the last faction, the date moves and the next turn
starts. If no faction is human, one `end_turn` moves the date by one
turn.

## Action points
Each faction has one pool, for corps 1 of the ruler. The loader does not
decode the corps of a save, so a faction has no other corps yet. The cap is
`ap.cap` (255, documented). The pool recovers at the start of each turn,
with the event `action_points_recovered`.

No source gives the recovery formula (Q-019). The engine uses an
assumed rule: the higher of the leadership and the charm of the leader
(Q-141). The rule comes from an unverified claim in
knowledge/turn-structure.md. The run of 2026-10-22 read the recovery of
12 corps (fixtures/action-points.tsv): a fixed number for each corps
that no single input explains. The cities, the adviser and the tally
office do not change the recovery yet. A load gives each faction one
recovery, because the loader does not decode the pool of the save.

At the same turn start each built 鍊兵所 of the faction gives 15
technique points (G2-005, the run of 2026-10-22, rules.tsv
technique.drill_ground_turn).

## engine_api
- `date` gives the year, month, day, season and the days since the start.
- `whose_turn` gives the force that acts, or `ENGINE_NO_FACTION`.
- `actions_left(force)` gives the pool of the force.
- `faction_count` and `faction_at(index)` give the order of this turn.
- `end_turn` is the "proceed" button.
- `set_faction_human`, `set_city_owner`, `set_city_gold` and
  `set_actions` are editor commands (F-134). The feature tests use them to
  set up a state through the api. They ledger nothing.

## The tests, line by line
- calendar_tests.cpp: the turn of 10 days, the month of 3 turns and the
  year end. The seasons. The starts of months, seasons and years. The
  turns of a job.
- turn_tests.cpp (unit): the cap of the pool and each step of the order
  rule. The order of a small synthetic world. The recovery rule and the
  leader rule (tutorial 14).
- turn_tests.cpp (feature), the calendar: a new game, one turn, one month,
  one year, and the count of each hook in one year.
- turn_tests.cpp (feature), the sample: the start date of the header, the
  player's faction and the AI passes before it. The first recovery, the
  recovery at each turn and the cap. The gold and grain of one city at a
  season start.
- turn_tests.cpp (feature), three factions: the editor gives the 42 cities
  to three forces (20, 12 and 10 cities). The order is 10, 12, 20. Each
  "proceed" passes the turn to the next faction. After the last one the
  date moves. A tie in cities goes to fewer officers. An AI faction
  passes.
- ui_tests.cpp: "Advance turn" calls `end_turn` exactly one time.

## Limits
- The AI gives no command. It only passes (M8).
- The disasters, the officer age and the new officers are hooks only.
- Each faction has one corps.
- Every amount of gold and grain stays an assumption until Q-021, Q-027
  and Q-119 close.

## Exercises
- Add the tally office to the recovery rule (+5 each month is a claim in
  knowledge/turn-structure.md).
- Add a "Turn order" column with the date of the last action of each
  faction.

The result you can check: `run_tests.bat --e2e` prints ALL PASS. Load the
sample save: the Simulation panel shows the date, the faction that acts
and its action points.
