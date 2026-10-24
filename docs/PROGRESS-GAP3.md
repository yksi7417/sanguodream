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

| G3-005, G3-007 | The items 攻擊, 齊攻, 單挑, 修補 and 撤除 of the unit menu are grey when no hex within 3 of the menu hex takes the command: the probe of the engine knows the range of the class, the targets and the own buildings. | batch 4 | ui |
| G3-006, G3-021 | The map goes dark during a target pick, with the targets in red. No hover panel shows during a pick, a strike or a duel. | batch 4 | ui |
| G3-010, G3-017, G3-023 | The log leaves out the events of other forces that change a value and their bandits. A line without a name drops its bare colon. The log is empty after a load, and the first line of a loaded game is 劉表，可終於輪到你了啊 with no portrait. | batch 4 | ui |
| G3-016 | No speech bubble opens during the show of the AI round. | batch 4 | ui |
| G3-022, G2-020 | The sex of an officer comes from the byte +55 of the record. The prisoner dialog shows 性別 and the cost line of the original, and no chance. The captive answers over its city after 登庸 (refusal) and 處決 (last words). | batch 4 | ui |

| G3-008 | After the target of 單挑, the window 武將 lists the officers of the unit with 成功率, 武力 and 體力. A row sends the duel with that officer, who fights first. The challenger and the defender speak before the first round. | batch 5 | feature/court, ui |
| G3-009 | An enemy unit that marches on an own city gives a line of the turn start: 根據急報，發現了以X為進攻目標的敵軍蹤影. The goal comes from the last AI turn of its force (unit_goal). The governor of the target city speaks, and the camera goes to the unit. | batch 5 | ui |
| G3-012 | While the window of a city is open, the row of another city moves the window there. Esc goes back one level as a right click. The X of the window closes its dialogs too. | batch 5 | ui |
| G3-015 | The ghost text was the hover panel of the city under the mouse, drawn over the window that opened. The hover panel of a city stays closed while a city window is open. | batch 5 | ui |

## In progress

- Every row of report 3 has its fix, or waits on the spec (the section
  above). The next play report decides the next batch.

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
