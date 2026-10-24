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

## In progress

- G3-001, G3-004, G3-013 and G3-018 are the numbers of the action points,
  the income, the patrol and the conscription against the fixtures.
- G3-020 is the attack order on a facility from a distance.
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
