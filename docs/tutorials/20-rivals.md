# 20 Rivals: the AI, corps, council, the Han emperor, events and bases

Files: code/sim/ai.cpp, code/sim/rivals.cpp, code/sim/events.cpp,
code/sim/bases.cpp, code/sim/content.cpp (bases.tsv, events.tsv,
ai_priorities.tsv), code/engine/engine_rivals.cpp, code/ui/ui_rivals.cpp.
The data is in data/placeholder/bases.tsv, events.tsv, ai_priorities.tsv
and rules.tsv.
Tests: tests/unit/rivals_tests.cpp, tests/feature/rivals_tests.cpp, the M8
tests in tests/ui/ui_tests.cpp, the demo "demo/m8_rivals".

## What it is for
This tutorial covers milestone M8. In M8 these things happen:
- the other factions play;
- a ruler delegates cities;
- the council and the adviser give proposals;
- the Han emperor has a protector;
- the generic events fire;
- the ports and passes stand on the map.

The specs are:
- knowledge/ai.md: the facts that players report on the computer players;
- mechanics/factions-corps-and-posts.md: the corps and the delegation;
- mechanics/city-commands.md: 評定, 軍團 and 軍師;
- mechanics/events.md: the 21 generic events;
- mechanics/bases-ports-and-passes.md and mechanics/siege-and-bases.md.

The features are F-032, F-120 to F-123 and F-130 of FEATURES.md.

## The tables
- bases.tsv has the 10 passes and the 35 ports, with the location ids 42
  to 86. Each row has the owner city and the durability of the book. The
  loader refuses a table without all 45 rows.
- events.tsv has one row for each generic event. The columns are:
  - check: quarter, month, search (after 探索), wait (after 待命) or none;
  - min_days: the days since the start before the event can fire;
  - months: the months of a quarter check;
  - chance: the percent at each check;
  - p1 to p6: the numbers of the trigger and the effect;
  - names: the Big5 bytes of the officers that the event names.
  The row transport_arrived is not a generic event. The code asks it when
  a transport of the player reaches its city (G2-027, tutorial 17).
- ai_priorities.tsv has one row for each kind of proposal: the command,
  the priority, the condition and its threshold.

Most chances, amounts and AI numbers are assumptions (Q-183 to Q-186).

## The AI planner
A computer faction gives the same commands as a player. The planner reads
ai_priorities.tsv for each city of the faction:
1. If the condition of a row holds, the planner makes the command of the
   row with the best free officer.
2. The command must pass its own check. Thus no proposal costs more gold
   or points than the faction has.
3. The score is the priority plus an urgency of up to ai.urgency. For
   example, public order 45 under a threshold of 90 gives half the
   urgency.
4. The proposals go in the order of their scores.

The faction itself, the adviser and the council use the flags
CORPS_FLAG_FREE_HAND. These flags allow each command and save nothing.
CORPS_FLAG_ALL also holds 儲存金錢 and 儲存兵糧, and with these flags the
planner never develops or researches.

The row develop builds toward the targets ai.develop_gold,
ai.develop_grain, ai.develop_conscription and ai.develop_weapons:
1. If the grain of the city feeds its soldiers for less than
   ai.grain_turns turns, a grain facility goes first.
2. Then the city builds one facility of each effect that it does not have.
3. Then the city builds the effect with the largest gap to its target.
4. After the targets, the city builds the fewer of gold and grain.

Each city keeps the salaries of ai.salary_months months. The planner
drops a proposal that spends this gold, except the defence of the city.

The row conscript also needs the grain for ai.grain_turns turns of the
soldiers after the conscription. Without it, the new soldiers desert.

A debate of a recruit, a search or an envoy of the computer runs to its
end without a pause. An open debate refuses the commands of each later faction
with SIM_REFUSAL_BUSY. The test command start_debate keeps its debate open.

On its turn a faction runs the best proposal, plans again, and stops after
ai.commands commands. Then each unit attacks an adjacent enemy, or moves
toward the nearest hostile city and attacks after the move. The ledger
gets one AI_PLANNED event with the number of commands.

The setting `ai` turns this off. The test fixtures turn it off so that
the other factions do not change the state of a test.

## Corps and delegation
軍團 makes a delegated corps of some cities. The order names a 都督, the
cities, a policy and the settings:
- the 都督 is an active officer of the faction, not the ruler, and leads
  one corps only;
- the city of the ruler stays in the corps of the ruler;
- a faction has up to seven delegated corps.

At the start of each turn of a human faction, each delegated corps gets
max(統率, 魅力) / corps.ap_divisor action points. It runs the planner for
its own cities with its own points, up to corps.commands commands. The
settings filter the commands: for example, without 攻擊 the corps does
not send units (出征), and with 儲存金錢 it does not develop or research.
Dissolve gives the cities back to the corps of the ruler.

## The council and the adviser
- 評定 shows the proposals of the planner for the city of the ruler. It
  needs the ruler in that city. A click runs the proposal: the officers
  act on it.
- 軍師 needs intellect adviser.intellect. The adviser shows the first
  proposals of the planner for the whole faction, one for each
  advice.per_intellect points of intellect.

## The Han emperor
The start date sets the emperor: 靈帝, 少帝 from 189/4, 獻帝 from 189/9,
and none after 234/4. He lives in the city emperor.city. The owner of
that city is his protector. Each month the protector gains
emperor.technique technique points, and its officers who respect the Han
gain loyalty. The events 少帝即位, 獻帝即位
and 獻帝駕崩 change the emperor.

## The events
Each event has a handler in events.cpp, found by its key. The calendar
checks the month and quarter events on day 1 of each month. The command 探索
checks the travellers of the city. The command 待命 checks the ruins, the
temples and the bonds. The travellers move to a new city each month.

An event that asks a question waits for the choice of a human faction:
- 謀反的嫌疑: "a slander" costs the informer loyalty; "treason" makes the
  都督 found a faction with the cities of the corps;
- 于吉: pay gold for technique points, give soldiers, or arrest him.

While an event waits, the other commands wait (`busy`). A computer
faction takes the first choice. The end of the turn also takes the first
choice. The setting `events` turns the calendar checks off. The editor
command `trigger_event` fires an event now, without its date and chance.

## The ports and passes
The engine places each base near its owner city. It takes the first free
hex of the rings 2 to 8. A port takes a hex next to water. The places are placeholders
(Q-034). In a new game from a scenario, a base starts with the owner of
its city and base.start_troops soldiers (assumed, Q-183). A save gives
its own owner and soldiers in the gate and port record at 0x08428D. The
owner is at +9 (255 = none), and the soldiers are at +10
(sim_load_save_base_records). Its rules are:
- a unit of another faction cannot enter its hex;
- a unit of the player that ends its move on an own port or pass enters
  it (the play report of 2026-10-23, below);
- a city gets base.gold each month for each base with the same owner;
- an attack takes soldiers and durability, and a base without either
  falls to the attacker. The ledger gets BASE_TAKEN. The officers in the
  base become captives of the attacker (sim_capture_officers_at).

A port or a pass is a base of the spec (siege-and-bases.md, "Inputs";
ui/city-commands.md, "Port or pass panel"). It holds soldiers, gold,
grain, weapons, a morale and officers. The soldiers are
sim_base_state.troops. The rest is sim_base_stock, in base_stocks at the
end of sim_state. A save of an older build loads with empty stocks.
sim_depot_at gives one view of a city and of a base for the commands that
both share. The view holds the location id, the force, the pointers to
the stocks, the soldier cap and the hex. The player reported on 2026-10-23 that a unit
stood on a port and did not enter it. Now:
- a unit of the player that ends its move on an own port or pass enters it
  (sim_unit_enter_base). The base takes its soldiers up to the cap of the
  base, its weapons, its cargo, its gold and its grain. The officers stay
  in the base. The ledger gets unit_entered_base, named by the commander.
  A base without a morale takes the morale of the unit (assumed);
- a unit on a long order stands on the base as a waypoint and enters it
  only when the base is its goal (assumed). Thus a pass at the edge of the
  reach does not swallow a 移動 order;
- a unit of the player without a long order that stands on an own base
  at the turn enters it (sim_run_units_turn; the player, 2026-10-24);
- a computer unit never enters a port or a pass (assumed). It has no
  sortie from a base;
- 出征 and 輸送 work from a port or a pass with its own officers and stock
  (sim_check_sortie with sim_depot_at). The home of the unit is the base,
  and 退卻 goes back into it;
- the panel of a port or a pass (ui_draw_base_panel) shows the fields of
  the original. The fields are 資金, 兵糧, 耐久, 士兵, 氣力, the weapon
  stock, 現役 and 俘虜. Then the buttons 出征 and 輸送 follow. The other buttons of the
  port menu wait. A click on an own port or pass opens the panel, as does
  a right click on its row of the list.

The spec gives no rule for the morale of a base, and no rule for a unit
that ends a long order on a waypoint. docs/PROGRESS-GAP3.md lists both
assumptions.

## engine_api
- `base_count`, `base_at`, `base_at_hex`, `corps_at`, `city_corps`,
  `adviser`, `advice`, `council`, `ai_plan`, `emperor`, `relic_count`,
  `relic_at`, `event_kind_count`, `event_kind_at` and `pending_event`;
- `command_corps`, `command_adviser`, `command_council` and
  `command_event_choice`;
- the editor commands `trigger_event`, `set_emperor` and `set_base`.

`ai_plan(force)` gives the proposals of the AI in the order it acts. A
feature test sets a state, then checks the first proposal: "given this
state, the AI issues a command of kind X".

## The panels
- The Ruler panel shows the emperor and his protector. It lists the
  corps and sends 軍團 (create, change, dissolve), 軍師 and 評定. It also
  shows the advice.
- The Bases panel lists the 45 bases: the kind, the owner city, the
  faction, the durability and the soldiers.
- The mini map draws each base as a square in the colour of its owner.
- The event dialog opens while an event waits, and sends the choice.
- The Settings panel turns the AI and the events on and off.

## The tests, line by line
- rivals_tests.cpp (unit) checks the parsers of the three tables, the
  corps settings that filter commands, and the conditions and urgency of
  the planner.
- rivals_tests.cpp (feature) checks these items through engine_api:
  - public order 0 makes the AI issue 巡察 first; morale 0 makes it issue
    訓練 first; no action points give no proposal;
  - with the AI on, each computer faction plans and pays for its
    commands;
  - 軍團 and its refusals, a delegated corps that patrols its city, and
    dissolve;
  - 軍師, its advice, 評定 and its refusals;
  - the emperor, his protector, the monthly technique points and
    獻帝駕崩;
  - 謀反的嫌疑 with both choices;
  - the bases: the table, the places, the income, the block and the
    capture.
- ui_tests.cpp checks that each button of the Ruler panel and the event
  dialog sends one command. The Bases panel and the mini map command
  nothing.
- demo/m8_rivals shows the bases, a new corps, an adviser, a council
  proposal, the treason event and a turn of the AI.

Run `run_tests.bat --e2e` and `run_tests.bat --demo`. Each prints
ALL PASS.
