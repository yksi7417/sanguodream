# Progress on clone gap report 3

This document tracks the work of the clone on
spec-src/spec/ui/clone-gap-report-3.md, the 23 rows G3-001 to G3-023 of
the side-by-side run of 2026-10-23. It also tracks the answers in
spec-src/spec/experiments/gap3-answers-2026-10-23.md. The builder closes
the gaps from P1 down, tests first, and publishes each batch.
docs/PROGRESS-GAP2.md holds the work on report 2.

## Fixed gaps

| ID | What changed | Commit | Tests |
|---|---|---|---|
| save format 3 | A save writes a member table after the header: the id, the offset and the size of each top-level member of sim_state. The load copies each member by its id, and the records of the officers and the cities record by record. The files of version 1 and 2 load through measured tables. A change of the state no longer refuses the saves of the player. | batch 1 | feature/save_compat |
| G3-002 | The unit of the loser of a duel loses 30 percent of its soldiers. The loser becomes a captive of the force of the winner, and his items go to that force (item_taken). | batch 1 | feature/court |
| G3-003 | The attacker takes half the gold and a quarter of the grain of a destroyed unit (unit_loot, unit_plunder_grain). The capture chance of a destroyed unit is 20 percent (assumed from 1 capture in 6 officers). | batch 1 | feature/combat |
| G3-011 | The maximum of the wall is the u16 at +82 of the city record of the save (4600, 4000, 2400, 2800), not the capacity constant. | batch 1 | feature/combat |
| G3-019, the loyalty | The own officers keep their loyalty at a month start. A captive loses 2 or 3. The repair of a wall at its maximum ends with G3-011. | batch 1 | feature/court |
| G3-013 | The rise of a patrol is 3 plus the best leadership of the officers over 40 (assumed: it fits the four observations 97 to 5, 59 to 4, 30 to 3, and 28 with 21 to 3; the sum does not). The fixture patrol.tsv now checks the gain. | batch 2 | unit/economy, fixtures/patrol, feature/city_commands |
| G3-014 | One of the executors of a city command speaks the result, picked by the command id (the original picks at random: slot 1 five times, slot 3 two times). | batch 2 | feature/report |

## Blocked on the spec

These rows need a formula that the spec does not have. The clone keeps
its assumed rule and does not guess a new one. The data is in
spec-src/spec/experiments/gap3-answers-2026-10-23.md and in the fixtures.
- G3-001, the recovery of the action points (Q-019). The original gives
  a constant number for each corps. The force of 劉表, with 9 cities and
  an adviser of 智力 100, gets 174. The force of 劉璋, with 5 cities,
  gets 103. The force of 孫策, with 1 city, gets 57. The six forces of
  fixtures/action-points.tsv get 52 to 74.
  The fixture handler counts the turn rows as open.
- G3-004, the income of a city (Q-021). fixtures/city-income.tsv gives
  seven cities with their facilities, the public order and the income.
  The fixture handler counts the rows as open.
- G3-018, the gain of a conscription (Q-219). The original computes the
  gain from the ability on the screen, and the screen shows some
  abilities a few points below the record. The clone uses the record.
  The spec does not give the modifier of the screen value.

| G3-020 | A click on an enemy facility out of the range asks 即將對 X 進行攻擊。確定嗎？ with 是 and 否. The unit takes a long order (attack_facility), marches at each turn and attacks the facility when it is in range, until the facility falls. The hover panel of a facility shows its name, its owner and 耐久 as current / maximum. | batch 3 | feature/orders, ui |

## In progress

- The P2 and P3 rows wait for the next batches.

## Questions for the observer

- G3-002, the technique points of a won duel. The clone gives 100 to the
  faction of the winner (assumed). The run saw 0 for the loser only.
  Please read 技巧 before and after a won duel.
- G3-003, the capture of a destroyed unit. The runs saw 1 capture in 6
  officers. The clone uses 20 percent. Please count the captures of ten
  destroyed units.
- G3-019, the repair of a facility. The clone repaired facilities of
  許昌 and 長安 at the month start. Please read the durability of two
  facilities before and after a month start in a city at peace.
