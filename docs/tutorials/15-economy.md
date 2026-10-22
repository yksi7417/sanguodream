# 15 Economy: production, durability and the five city commands

Files: code/sim/economy.cpp, code/sim/city_commands.cpp, code/sim/develop.cpp
(levels and jobs), code/sim/turn.cpp (the calendar), code/sim/content.cpp
(weapons.tsv), code/engine/engine.cpp, code/ui/ui_city_commands.cpp,
code/ui/ui_economy.cpp, code/ui/ui.cpp (the ledger filter),
data/placeholder/rules.tsv, weapons.tsv, facilities.tsv.
Tests: tests/unit/economy_tests.cpp, tests/feature/economy_tests.cpp,
tests/feature/city_command_tests.cpp, the M3 tests in tests/ui/ui_tests.cpp,
the demo "demo/m3_economy".

## What it is for
This tutorial covers milestone M3: the money and the grain of a city, and
the commands that change them. The specs are:
- mechanics/cities-and-facilities.md: facility income, the mint and the
  granary, the PK facilities, the merge levels, durability;
- mechanics/calendar-and-turn.md: the calendar events, the caps, the
  technique points;
- mechanics/city-commands.md: the costs, the limits and the inputs of
  each command;
- mechanics/weapons-and-production.md: the weapons, their costs and their
  facilities;
- mechanics/skills.md: the income skills 富豪, 米道, 徵稅 and 徵收.

The features are F-005 (technique points), F-006 (gold and grain), F-025
(patrol), F-026 (conscription), F-027 (weapon production), F-029 (the
merchant), F-030 (train) and F-031 (durability).

## The rule of the ledger
Each change of the gold or the grain of a city is one ledger event. Thus
the sum of the events of a city is the change of its stock. A test checks
this rule for each turn of a 12-month trace. The rule has two parts:
- An income event has the full amount. If the stock passes its cap, a
  second event (`gold_over_cap` or `grain_over_cap`) takes back the rest.
- An unpaid salary is the event `salaries_unpaid`. Its delta is 0, and
  `detail` holds the gold that the city did not pay.

Each event now has a `detail` field: a slot, a weapon or an amount. The
function `sim_event_resource` gives the resource that an event changes:
gold, grain, soldiers, morale, public order, weapons, durability, technique
points, action points or merit. The ledger filter uses it.

## The production system
economy.cpp runs at the calendar events. `sim_run_calendar` calls it in
this order:
1. Day 1 of each month (`sim_run_production_month`): the gold income, the
   salaries, the grain of 徵收, and the free weapons of a specialty city.
2. Day 1 of months 1, 4, 7 and 10 (`sim_run_production_season`): the
   grain income.
3. Each turn (`sim_run_production_turn`): the gold of 徵稅, then the grain
   that the soldiers eat.
4. Each turn (`sim_run_durability`): the facilities recover durability.

`sim_city_forecast` gives the income of a city at its next events. The
engine gives it to the front end as `city_income_forecast`. The rules are:
- gold each month = (initial income + facilities) x order percent x 富豪
- grain each season = (initial income + farms) x order percent x 米道
- 徵稅: the gold comes at each turn, and each payment is half.
- 徵收: the grain comes on day 1 of each month, and each payment is half.

The output of one facility is its value, times its level, times 1.5 next
to a mint or a granary (`sim_facility_output`). The manual documents the
level multipliers: level 2 gives 1.2 times and level 3 gives 1.5 times. A
level-3 market next to a mint gives 1.5 x 1.5 times. The PK markets have
their documented ratios in facilities.tsv. The great market gives 3 times a
market, the fish market 2 times, and the black market 0.8 times. The merge
command comes in M4 (F-024). Until then, the editor command
`set_facility_level` sets a level for the tests.

The manual documents the caps: 100,000 gold and 1,000,000 grain for a
city. It also documents 100,000 soldiers (150,000 for a big city) and
100,000 of each weapon.

## The assumed numbers
The spec names the inputs of each amount but gives few numbers. Each
number is in rules.tsv with the status "assumed" and a question:

| Rule | Value | Question |
|---|---|---|
| Income at public order 0, as a percent of the income at order 100 | 50 | Q-151 |
| Public order of one patrol | 10 percent of the leadership sum | Q-152 |
| Soldiers of one conscription | floor(F x (1000 + 6 x S x P / 100)) for the charm sum S and the public order P; F is 1.0 at level 1, 1.2 at level 2 (assumed) and 1.5 at level 3 | observed at the levels 1 and 3 (G2-011, the run of 2026-10-22) |
| Public order and morale that a conscription takes | 10 and 9 | observed (pairs/conscript/001); one officer took 12 and 6 in the run of 2026-10-22 |
| Morale of a loaded city | 50 | Q-154 |
| Morale of one train | floor(4 + 0.115 x S) with a drill ground, floor(3 + S / 15) without one; S is the force of the first officer plus half of the force of each other officer | observed (G2-022, the run of 2026-10-22) |
| Price of grain | the price R of the city (cities.tsv, 3 to 7 grain for 1 gold) and the trade effect 400 / (450 - politics); a sale loses 20 percent | observed (G2-025); the monthly drift of R is assumed (Q-155) |
| Weapons of one production | F x (1000 + 10 x I); I is the intellect of the first officer plus half of the intellect of each other officer; F is 1.5 at smithy level 3 | observed at level 3 (G2-026, the run of 2026-10-22); the factors of the levels 1 and 2 are assumed |
| Period of a siege weapon or a ship | 90 days at sum 0 to 30 days at sum 100 | Q-156 |
| Cost in a specialty city | 80 percent | Q-022, Q-156 |
| Free weapons of a specialty city | 100 each month | Q-022 |
| Durability that a facility recovers | 100 each turn | Q-157 |
| Technique points of one command | 10 | Q-158 |
| Save number of a skill | the number of skills.md minus 1 | Q-159 |
| Soldiers that leave a city without grain | 100 for each missing grain | Q-160 |

The values of M2 stay assumed. They are the initial income, the value of a
market and a farm (Q-021), the salary (Q-027) and the grain of the
soldiers (Q-119). save-file.md documents the merit of a command: +50 for
each officer that does an order.

## Durability
A develop gives the facility its full durability when it starts. Each turn,
a facility recovers durability if its city has a governor and no enemy is
within 2 hexes (manual p.11). At durability 0 the engine destroys the
facility: the slot is empty, and the officers of a job are free. No attacker exists until
M6. Thus the test command `damage_facility` takes durability from a
facility. It ledgers `facility_damaged`, and at 0 `facility_destroyed`. The
loot of the attacker comes with the attacker.

## The five city commands
city_commands.cpp has the commands. Each command costs 20 action points.
Each command has the refusal reasons of the develop command, in the same
sequence. The content, the city and the turn come first. Then come the
checks of the command, the officers, the action points and the gold. M3
adds four reasons:

| Refusal | When |
|---|---|
| `limit_reached` | patrol or train ran in this turn; each barracks or production facility ran in this turn; a workshop or a shipyard has a job |
| `too_many_officers` | the merchant takes one officer |
| `no_amount` | the merchant order has no amount or an unknown mode |
| `no_grain` | the city has less grain than the sale |

The commands are:
- 巡察 patrol: 100 gold. The leadership sum raises public order, up to
  100. An enemy within 2 hexes gives half the rise. At 80 or more, the
  city has no bandits. This is a flag (`bandits_blocked`) until F-007.
- 徵兵 conscript: 300 gold. It needs a barracks, and it runs one time for
  each barracks in each turn. The soldiers are
  floor(F x (1000 + 6 x S x P / 100)). S is the charm sum, P is the
  public order and F is the factor of the barracks level. It takes 10
  public order and 9 morale.
- 訓練 train: no gold. The force of the officers raises morale, up to
  100. S is the force of the first officer plus half of the force of each
  other officer. The rise is floor(4 + 0.115 x S) with a drill ground and
  floor(3 + S / 15) without one. It runs one time in each turn.
- 商人 merchant: one officer. The order buys grain with gold, or sells
  grain for gold, at the price R of the city and the trade effect of the
  officer. A sale loses 20 percent. An enemy within 2 hexes blocks it.
  It gives no technique points.
- 生產兵裝 produce: the cost of the weapon (weapons.tsv). Pikes,
  halberds, crossbows and war horses arrive in the same turn. The
  quantity is F x (1000 + 10 x I). I is the intellect of the first
  officer plus half of the intellect of each other officer. F is the
  factor of the smithy level. A siege weapon or a ship arrives after a period,
  one item for each command, and its officers are busy until then. The
  weapons that need a technique (木獸, 投石, 鬥艦) refuse with
  `no_facility` until M4.

Each command marks its officers as acted. Each officer gains 50 merit.
Each command gives the technique points of its action. The technique rows
of rules.tsv hold them, from the table of knowledge/tech-tree.md. The
values are these:
- 巡察 3 and 訓練 3;
- 徵兵 2 to 5 by the soldiers;
- 商人 0;
- 生產兵裝 5 for a weapon and 10 for a siege weapon or a ship;
- 開發 0 at the order and 15 when the facility is complete.

## engine_api
- `city_income_forecast(city)` gives each part of the income.
- `city_economy(city)` gives the stocks, the caps, public order, morale,
  the soldiers, the weapon stocks, the price, the barracks and the
  technique points of the owner.
- `technique_points(force)`, `weapon_kind_count` and `weapon_kind_at`.
- `ledger_filter(city, resource)` gives the indices of the ledger events
  of a city that change a resource. `ENGINE_ANY_CITY` and
  `ENGINE_ANY_RESOURCE` turn a filter off. It is a query.
- `command_patrol`, `command_conscript`, `command_train`,
  `command_merchant` and `command_produce`.
- `can_issue` checks the five commands. Its answer has the effect:
  `value_before`, `value_after`, `quantity` and `gold_gain`.
- `damage_facility` is the test command of durability.
  `set_facility_level` and `set_city_resource` are editor commands; they
  ledger nothing.

## The panels
The City panel shows public order, morale and the soldiers. It has one
button for each of the five commands. Each button opens a dialog with the
layout of ui/city-commands.md:
- the 執行武將 button and the officer slots, with the first free officer
  as the proposal;
- the effect field "current → result";
- the 資金 row (cost / stock); train has no 資金 row;
- the 行動力 row (cost / available);
- 決定 (green) and 返回 (orange).

決定 is grey when `can_issue` refuses. One click of 決定 sends one command
and closes the dialog. The City panel then shows the result. 返回 closes the
dialog and sends nothing. The produce dialog has a button for each weapon
with its cost in the city. It shows the stock of each weapon, the period
and the 效果 line of the weapon. An item is grey without its facility or
its technique (G2-029). The merchant dialog has one slider from the full
sale to the full purchase, the city price and the trade effect of the
officer (G2-031). The centre of the slider trades nothing.

The Economy panel has one row for each city of the faction that acts. Each
row has the gold and the grain with their caps. It also has the income of
the next month and season, the salaries and the grain of each turn. The
last columns are public order, morale and the soldiers. The Event ledger
has a filter for a city and a resource.

## The tests, line by line
- economy_tests.cpp (unit) checks these items:
  - the percent arithmetic and the level and adjacency multipliers;
  - the order percent and the rules of each command;
  - the price of the merchant, and the quantity and the period of a
    production;
  - the forecast with each income skill on a synthetic city;
  - the resource of each event, and the weapon table.
- economy_tests.cpp (feature), the trace: 36 turns of one AI city that
  nobody touches. Each turn has the gold income, the salaries, the grain
  income and the consumption in the calendar order, with the amounts of
  the forecast. The ledger explains each change. The stocks stay under the
  caps.
- economy_tests.cpp (feature), the other checks:
  - the gold cap and the grain cap;
  - the soldiers that leave a city without grain, and the unpaid salaries;
  - the level and adjacency multipliers;
  - durability recovery and destruction;
  - the technique points and the merit of a command.
- city_command_tests.cpp (feature): the effect, the costs and the ledger
  of each command, and each refusal. A specialty city pays less and gets
  free weapons. The ledger filter is a query.
- ui_tests.cpp: the City panel opens each dialog and commands nothing.
  決定 sends exactly one command with the choices of the dialog. 返回 sends
  nothing. A refused command has a grey 決定. The Economy panel and the
  ledger filter call only queries.
- demo/m3_economy: load the sample, open a city of the player, build a
  barracks and a smithy, then patrol, conscript, train and produce pikes.
  Advance three months. Show the Economy panel and the ledger of the city,
  filtered to its gold.

## Limits
- Every effect of a command stays an assumption until Q-151 to Q-160 close.
- The merge command (F-024), the disasters (F-007) and the techniques
  (F-040) come later. The bandit threshold is a flag only.
- No attacker exists, so a destroyed facility gives no loot.
- The drill ground does not shorten the period of ships yet.
- The weapon stocks use the order of save-file.md. The engine assumes the
  order of the siege and ship stocks (Q-006, Q-156).

## Exercises
- Add the dismantle command (撤除): it frees a slot, costs 10 action
  points and takes technique points.
- Show the slots in the Economy panel with their durability as a bar.

The result you can check: `run_tests.bat --demo` prints ALL PASS, and
build\demos\m3_economy.mp4 shows the four commands and the filtered ledger.
