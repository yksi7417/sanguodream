# 28 AI view: the goals of the AI units and the record of an AI turn

Files: code/sim/ai.cpp, code/sim/sim.h (sim_ai_goal, sim_ai_record),
code/engine/engine_rivals.cpp (ai_explain), code/engine/engine_combat.cpp
(set_unit_grain), code/ui/ui_ai.cpp, data/placeholder/ai_priorities.tsv,
rules.tsv. Tests: tests/feature/ai_tests.cpp, the "ai_panel_" tests in
tests/ui/ui_tests.cpp, and the demo "demo/m15_ai".

## What it is for
The computer factions act on the map with a purpose that a player can
see. A unit defends its city, takes a free city, attacks a city of an
enemy, or goes home before its grain ends. The AI panel tells why the AI
did what it did. This tutorial builds milestone M15 on top of the M8
planner of tutorial 20.

The spec gives the behaviour of the original AI only as unverified
claims in knowledge/ai.md. Thus the clean room assumes each goal, score
and rule of this tutorial (Q-185). The spec has no year log of the original
yet. The statistics test of the roadmap waits for it.

## The two new rows of the planner
ai_priorities.tsv has two new rows. Both send a unit out of a city
(出征):

| Row | Priority | Condition |
|---|---|---|
| defend | 70 | an enemy unit within enemy.block_hexes of the city |
| take_free | 65 | a city without an owner within ai.target_hexes |

The condition enemy_near gives the full urgency of 20. Thus "defend"
scores 90 and goes before 巡察 at public order 0 (60 + 20). The old row
"sortie" stays at 25.

On the real map the nearest city of each city is 13 to 29 hexes away.
The M8 reach of 12 hexes gave the AI no target there. The rule
ai.target_hexes is now 30.

The rule ai.attack_after_months stops each sortie but "defend" in the
first 5 months after the start of the scenario. knowledge/ai.md says
that the AI armies arrive 5 to 6 months after the start. With this rule
the replay of economy-turn/001 (tutorial 26) keeps 334 confirmed
differences.

## The goals of a unit (sim_ai_unit_goals)
Each AI unit scores four goals. Each goal has the best target of its
kind:

| Goal | Target | Score |
|---|---|---|
| defend | the enemy unit nearest to an own city within ai.defend_hexes | 100 + 200 - hexes |
| take a free city | a city without an owner | 50 + 200 - hexes |
| attack a city | a city of a faction at war | 200 - hexes |
| resupply | the nearest own city | 1000 + 200 - hexes |

The hexes count from the unit to the target, at most 200. The resupply
goal holds only when the grain does not last the march home and
ai.grain_margin_turns more turns. The march home counts ai.march_hexes
(4) hexes for each turn. A unit of a save without grain data
(grain_unknown) never takes this goal.

The unit takes the goal with the best score. Then sim_ai_units does
these steps:
1. A defender attacks its enemy if the enemy stands next to it.
2. A unit that is not on the way home attacks a target next to it and
   stays. A unit in contact does not march away.
3. Else the unit marches to the reachable hex nearest to the target. An
   own city at the end takes the unit in.
4. After the move it attacks its enemy, or a target next to it.

## The record of an AI turn (sim_ai_record)
sim_run_ai_faction starts a record for the faction. The sim keeps 64
records, one for each faction that acted last. A new turn of the same
faction reuses its record. A record holds up to 32 options in this
order:
1. each city command that the AI gave, in the order of the turn;
2. the chosen goal of each unit;
3. the other goals of the units;
4. the proposals of the first plan that the AI did not give, by score.

The engine saves the records with the game, because it writes sim_state
as one block.

## The api
- `ai_explain(faction)` gives the record as engine_ai_explain. Each
  option has the goal, the command, the city, the unit and the officer.
  It also has the score, the hex of the goal and the key of its row. `turn` is 0 for a
  faction without an AI turn, for example the human faction.
- `set_unit_grain(unit, grain)` is a test command. It sets the grain of a
  unit, and the grain becomes known.

## The AI panel (ui_ai.cpp)
The debug menu has the item AI. The panel has a combo of the computer
factions and a table "ai_options" with one row for each option. A mark *
shows a chosen option. A click on a row frames its unit, else its city.
The panel calls ai_explain only and commands nothing.

While the panel is open, the scene draws a line from each unit of the
faction to the hex of its chosen goal. A ring marks the goal.

## Tests to read
- tests/feature/ai_tests.cpp:
  - ai_explain after an AI turn, with the chosen 巡察 of a city;
  - a free city falls to the AI within 6 turns, but not in the first 5
    months of the scenario;
  - a city under attack sends a unit out, and the unit hits the enemy;
  - a unit with grain for one turn goes home, and no soldier deserts.
- tests/feature/rivals_tests.cpp: the planner tests of M8 expect 12 rows,
  and the city checks skip the two new sortie rows.
- tests/ui/ui_tests.cpp, "ai_panel_reads_the_record_of_the_faction" and
  "ai_panel_row_frames_the_unit_or_the_city".
- tests/demo/demo_tests.cpp, "demo/m15_ai": on the real map an AI faction
  sends a unit to a free city, and the city falls.
