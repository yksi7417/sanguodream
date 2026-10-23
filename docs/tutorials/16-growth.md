# 16 Growth: research, merge, disasters and the end of the game

Files: code/sim/research.cpp, code/sim/merge.cpp, code/sim/disasters.cpp,
code/sim/outcome.cpp, code/sim/content.cpp (techniques.tsv and
disasters.tsv). The effects are in code/sim/turn.cpp, economy.cpp and
city_commands.cpp. The api and the panels are in
code/engine/engine_growth.cpp and code/ui/ui_growth.cpp. The data is in
data/placeholder/techniques.tsv, disasters.tsv, weapons.tsv and rules.tsv.
Tests: tests/unit/growth_tests.cpp, tests/feature/growth_tests.cpp, the M4
tests in tests/ui/ui_tests.cpp, the demo "demo/m4_growth".

## What it is for
This tutorial covers milestone M4: the things that make a faction grow
after the economy of M3. The specs are:
- mechanics/research-techniques.md: the eight lines of four techniques,
  the four techniques of the Power Up Kit, the costs and the periods;
- mechanics/cities-and-facilities.md: the PK facilities and the merge;
- mechanics/weapons-and-production.md: the strong weapons that replace
  the weak ones;
- mechanics/calendar-and-turn.md: the disasters, and victory and defeat;
- mechanics/skills.md: the disaster skills 風水, 祈願 and 威壓, and 指導.

The features are these rows of FEATURES.md:
- F-007 (disasters) and F-008 (victory and defeat);
- F-023 (the PK facilities) and F-024 (the merge);
- F-028 (siege weapons and ships with their techniques);
- F-040 to F-043 (the techniques).

## The research window (G2-064)
The window of 研究技巧 shows the state of each technique. A technique
that the force has is green and grey. The technique of the research in
progress is blue with the turns left, and grey. A technique whose line
needs the level before it is grey: the probe of the command answers
NEEDS_TECHNIQUE. An open technique is white. A hover names the state
with the points and the gold. In the game mode the window keeps itself
on the screen (ui_context_place_sized), because a city at the left edge
opened it partly outside the screen.

## The technique table
techniques.tsv has one row for each technique. A row has the line, the
level, the technique points and the gold. It also has the ability that
shortens the period, the period range, an effect and a unit class. The parser checks
that level n of a line follows level n - 1 in an earlier row. The manual
documents the costs:

| Level | Technique points | Gold |
|---|---|---|
| 1 | 1000 | 1000 |
| 2 | 2000 | 2000 |
| 3 | 3000 | 5000 |
| 4 | 5000 | 10000 |

A faction keeps its techniques as a set of bits, one for each row
(`sim_state.techniques`). `sim_has_technique` reads one bit.
`sim_has_technique_effect` asks if the faction has any technique with an
effect, for a unit class or for all classes. The rules of M5 and M6
(movement, combat, fire) read the flags this way. The scenario can give a
faction its techniques at the start: the editor call `grant_technique`
does this.

## The research command
研究技巧 takes up to three officers, 50 action points, the technique
points and the gold of the row. The checks come in this sequence:
1. the content, the end of the game, the city, the turn;
2. an enemy within 2 hexes (`enemy_near`);
3. no such technique (`no_technique`);
4. the faction has it (`value_at_maximum`);
5. the faction lacks the level below (`needs_technique`);
6. a research of the faction runs (`limit_reached`);
7. the officers, the action points, the technique points
   (`no_technique_points`) and the gold.

The command pays when it starts. The ledger has `gold_spent`,
`action_points_spent`, `technique_spent` and `research_started`. Each
officer gains merit; the research gives no technique points. The officers
are busy for the whole period. At the start of each turn
`sim_progress_research` counts down. At 0 the faction has the technique
(`technique_researched`) and the officers are free. The log line names
the technique, and the turn start shows it as a message (G2-062).

The period is an assumed line (Q-161). It goes from the longest period of
the row at an ability sum of 0 to the shortest at a sum of 200. A
personnel office in the city gives 75 percent of it. The skill 指導 halves
the gold (documented). One research at a time is an assumption of Q-024.

## The effects of the techniques
M4 applies the effects that have a rule now:

| Technique | Effect in M4 | Status |
|---|---|---|
| 熟練兵 | morale of a city up to 120 (`sim_city_morale_max`) | documented |
| 培訓工兵 | facilities recover 2 times the durability | assumed amount |
| 開發木獸 | the wood beast; the ram cannot be produced | documented |
| 開發投石 | the catapult and the battle ship replace the tower and the tower ship | documented |
| 整備政令 | public order losses are half | assumed amount |

weapons.tsv names the technique of a weapon and the weapon that it
replaces. `sim_check_weapon_technique` gives `needs_technique` or
`weapon_replaced`. The other effects are flags for M5 to M7. They change damage, movement,
range, the morale of units and the soldier cap. Others change military
facilities, fire, transport, ports and loyalty.

## The merge
吸收合併 takes one officer, 10 action points and 100 gold. The absorber and
the absorbed side must be two built facilities of one kind that can merge.
The kinds are market, farm, barracks, smithy and stable. The two
facilities must be neighbours. The
absorbed side must be level 1, and the sum of the levels must be 3 or
less. Thus a level-2 facility is always the absorber. Else the answer is
`not_mergeable`.

During the merge, the absorbed slot has the state `SIM_SLOT_MERGING`. It
gives nothing, and the absorber gives its old output. The period is an
assumed line from 60 days at politics 0 to 20 days at politics 100
(Q-163). When the merge is complete, the absorber gains the level and the
absorbed slot is free (`facility_merged`). If an attack destroys the
absorbed side first, the merge ends without a level.

## The PK facilities
The ten PK facilities are in facilities.tsv since M1. M4 adds the effects
that have a command now:
- The tally office adds 10 action points at the start of each turn for
  each office of the faction (assumed amount, Q-164).
- The personnel office makes a research take 75 percent of the period
  (assumed amount, Q-164).
- The military office halves the action points of a military command.
  Train is the military command that exists now.
- The drill ground gives ships 75 percent of the period (assumed amount,
  Q-164), and train gives more (M3).
- The great market, the fish market, the black market and the military
  farm have their income since M3.

The plot office, the diplomacy office and the half cost of search and
recruit come with their commands in M6 and M7.

## The disasters
disasters.tsv has one row for each disaster. The months come from the
manual. Every number is an assumption of Q-162. On day 1 of a month,
`sim_roll_disasters` rolls the disasters of the month. It rolls after the
income of the day, for each city that has an owner:
- A disaster that runs does not start again.
- A row with `low_order_only` starts only below public order 80.
- An officer with the `skill_block` skill in the city stops it (風水).
- An officer with the `skill_chance` skill changes the chance (祈願
  doubles the harvest, 威壓 halves the bandits).

The effects are data:

| Disaster | Start | Each turn | End |
|---|---|---|---|
| Locusts | public order -10 | 200 durability from a farm; half of each grain payment | after 9 turns |
| Plague | public order -10 | 5 percent of the soldiers | after 6 turns |
| Bandits | — | public order -2; 300 durability from any facility | at public order 80 |
| Good harvest | half of the grain of a season, at once | — | — |

Each event of a disaster has the assumed mark. The loss of public order is
`order_lost` with `detail` = 1 + the disaster. The bandit base of the
manual needs combat, so the bandits stop at public order 80 until M6. The
game setting `disasters` turns the rolls off. The feature tests turn it off
in the fixture, so that the other tests stay exact. The editor call
`start_disaster` starts a disaster with its start events.

## Victory and defeat
outcome.cpp checks the human factions at the start of each turn:
- Defeat: the year is 351, the faction has no city, or the faction has no
  active officer. A surrender comes with diplomacy in M7.
- Victory: the faction owns every city that has an owner. An empty city
  does not count.

The ledger has `game_lost` or `game_won` with the reason in `detail`. The
game is over when a human faction wins, or when every human faction lost.
Then "proceed" does nothing and each command refuses with `game_over`.

## engine_api
- `technique_kind_count`, `technique_kind_at` and
  `faction_techniques(force)`: the table, and the set and the research of a
  faction.
- `disaster_kind_count`, `disaster_kind_at` and `city_disasters(city)`.
- `outcome()` and `settings()`.
- `command_research(city, technique, officers)` and
  `command_merge(city, absorber, absorbed, officer)`. `can_issue` checks
  both with `ENGINE_COMMAND_RESEARCH` and `ENGINE_COMMAND_MERGE`.
- `start_disaster` is a test and editor command; it ledgers.
- `set_settings`, `grant_technique`, `set_technique_points` and
  `set_date` are setup and editor calls; they ledger nothing.

## The panels
The City panel has the buttons 吸收合併 and 研究技巧, and a line with the
disasters that run in the city. The research dialog shows the tree: one row for each
line, one button for each technique. A technique of the faction is green
and the chosen one is blue. The dialog has the officer slots, the
technique points, the gold, the period, the action points, 決定 and 返回.
The merge dialog has two combos (the absorber and the absorbed side), one
officer slot, the levels "now → after", the period and the costs. The
technique table (技巧表) shows each line and level of the faction that
acts. The Settings panel has the disasters check box. The Simulation panel
shows the year of the end, or the outcome.

## The tests, line by line
- growth_tests.cpp (unit) checks the parsers of techniques.tsv and
  disasters.tsv and the month list. It also checks the periods of research
  and merge, the chance with a skill, and victory and defeat on a small
  world.
- growth_tests.cpp (feature) checks these items through engine_api:
  - the table, each research refusal, the costs, the ledger, the busy
    officers, the completion and the next level;
  - the morale cap, the strong weapons, the recovery and the order loss of
    the techniques;
  - the merge to level 2 and 3, each refusal, and the outputs;
  - each disaster with `start_disaster`, and two years of random rolls
    that start each disaster on day 1 of one of its months;
  - the tally office, the military office, the drill ground and the
    personnel office;
  - victory, the defeat without a city, and the defeat in the year 351.
- ui_tests.cpp checks that each dialog sends one command for one click of
  決定, and nothing for 返回 or a grey 決定. The technique table and the
  outcome only read. The disasters check box sends one `set_settings`.
- demo/m4_growth merges two markets, researches 熟練兵, starts locusts,
  and records build\demos\m4_growth.mp4.

Run `run_tests.bat --e2e` and `run_tests.bat --demo`. Each prints
ALL PASS.
