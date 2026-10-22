# Progress of the second clone gap report

This file records the work of the builder on the gaps of
spec-src/spec/ui/clone-gap-report-2.md (G2-001 to G2-065). The observer
task reads it at the start of each run. The order of the work is section
6 of the report. The spec revision is 511531a.

## Fixed gaps

| ID | What changed | Commit | Tests |
|---|---|---|---|
| G2-001 | The load reads the turn counter of the header (0x07C) and sets the date to the scenario start plus one turn for each count. The sample loads at 197/10/1 in winter. | group 1 | unit/calendar, unit/save_format, feature/load_save, feature/turns |
| G2-002 | The load takes the action points (table A, +12) and the technique points (table B, +61) of each force from the save. 劉表 starts with 62 and 23. | group 1 | feature/turns |
| G2-003 | The load runs no AI command, recovers no action points and starts no delegated corps. It gives the turn to the player at once and emits the turn start. The cities keep the soldiers of the file. | group 1 | feature/turns |
| G2-004 | The load takes the acted flag of each officer (+148 bit 0). A unit whose leader acted has acted, so its plate shows 完 and 進行 asks no question. | group 1 | feature/turns, feature/save_map |
| G2-012 | The load reads the morale of the city (+84). The third ship field (+68) is the stock of the 樓船: 江陵 4, 江夏 1. | group 1 | unit/save_format, feature/load_save |
| G2-035 | The load reads the loyalty of each officer from +104 (a hypothesis, Q-005). The game shows at most 100. 英布 97, 劉辟 97, 董承 98, 呂蒙 99, the others 100. | 4a1d459 | unit/save_format, feature/load_save, feature/court |
| G2-042 | After 決定 of 出征 the new unit is selected and its range shows. A left click on the plate of a unit is a click on its hex. A click on an own unit with no selection selects it. | group 2 | ui |
| G2-052 | A left click on an enemy unit while an own unit is selected opens 殲滅 and 驅趕, with the panel 預定抵達時間, 所有兵糧 and 通過據點. 殲滅 marches on the target and attacks it in the same turn when it is in reach, then at each 進行 until it is gone. The plate shows 委. A left click on an own unit asks for the follow order with 是 and 否. | group 2 | feature/orders, ui |
| G2-056 | A land unit enters a river hex as a 走舸 at the cost of the boat row. The far-hex order 移動 takes it onto the river over several turns. | group 2 | feature/orders |
| G2-058 | A left click on a hex out of the range opens 移動 (and 設置, grey) with the days to arrive. 移動 moves the unit to the end of its range in the same turn, marks it 委, and moves it on at each 進行 until it arrives. 委任部隊行動 runs the steps now and is white when a unit has an order. | 3a621c8 | feature/orders, feature/march, ui |
| G2-036 | 人材 > 移動 opens a dialog with the target city and its days, the free officers of the city with check marks, and 行動力 20. The officers travel for the days and arrive at a turn start, with a log line. The days are 10 for each city region that the journey enters (assumed, Q-228): 江陵 to 江夏 gives 10, as the report. | group 3 | feature/people, ui |
| G2-037 | 人材 > 召喚 opens a dialog with the officers of the other cities of the faction, their base and days, and 行動力 20. The officers travel to the city. | group 3 | feature/people, ui |
| G2-060 | 君主 opens a second column: 評定, 軍團, 軍師, 官職, 仲介 (grey), 流放, 研究能力, 培育武將. 官職 opens a window with the posts (官職名, 功績, 指揮, 效果, 武將), the officers without a post, 任命, 免職, 全解除, 回復原狀 and 決定. The clone selects a post and an officer in place of a drag. 流放 opens a list of the officers. 研究能力 and 培育武將 open the abilities window. | group 3 | ui |
| G2-065 | 計略 opens a second column: 二虎競食, 驅虎吞狼, 流言. 驅虎吞狼 and 流言 open a dialog with the bases of the other forces (據點, 太守, 現役, 治安, 日數), one executor from the free officers of the city in the order of 智力, the gold, the days and 行動力. The plot goes on the road for the days, and the result comes at the turn start of the arrival with a log line that names the officer and the base. | group 3 | feature/people, feature/court, ui |

## In progress

- Group 1 has no open work except the parts of G2-061 and G2-063 that
  the spec does not support (see the questions). The corps of a save still act as
  the factions 0 and 12, and the adviser and the relations keep their
  start values.
- Group 2 has no open work. In the original, 殲滅 used a tactic (G2-052).
  The long order of the clone uses the normal attack of the class. The
  rule that picks the tactic is a question below.
- Group 3 has no open work. 仲介 stays grey. The 移動 and 召喚 dialogs
  list cities only, not ports and passes. The log line of an arrival
  names the officer but not the base.
- Next: group 4, the numbers (G2-005, G2-011, G2-013, G2-014, G2-025,
  G2-028, G2-049) and the table of technique points.

## Questions for the observer

- G2-035, the loyalty byte (+104). In the sample, 鮑叔牙 (id 828, in a
  unit) has 99 at +104. The 褒賞 list of the original did not show him.
  李傕 (id 598) has 242. What 忠誠 does the original show for 鮑叔牙 and
  for 李傕 at the load of the sample? If 鮑叔牙 shows 99, the byte is
  the loyalty and the 褒賞 list has a second filter.
- G2-061, the corps. The save marks 長沙 with the owner 0 and 武陵 with
  the owner 12. Table A record 0 has the leader 蘇飛, 255 action points
  and the bytes 1 and 1 at +13 and +14. Record 12 has the leader 吳巨,
  255 action points and the bytes 255 and 0. The spec gives no field
  that links a corps record to its force (Q-202). Does the window
  重編軍團 of the original show a number for each corps? Can the
  analysis side find the link in the save, for example a corps count or
  a corps list in the per-force table B? Until then the clone plays the
  two corps as the factions 0 and 12.
- G2-061, the adviser. The save has no documented adviser field. The
  scenario force record has one at +2 (scenario-file.md), but the save
  keeps another per-force table. The 213 bytes of table B of force 6 do
  not hold the id of 張良 (814). Where does the save keep the adviser?
- G2-063, the relations. The save has no documented relation field. The
  table B record of force 6 holds a 100 at +160. The bytes from +154 to
  +199 are these (then 50 up to +194, then 0):
  `29 34 43 45 0 53 100 0 44 32 0 0 50 50 50 0 50 0 50 12 50 0 0 37 50 50`.
  A vector of 46 relations that
  starts at +154 has its own entry at index 6. But the values do not
  fit the labels of the 親善 dialog at 197/12/21. For example, index 2
  is 43 and index 1 is 34, but 孫策 shows 險惡 and 劉備 不和. Please read
  the 交情 numbers of the 選擇對象勢力 window at the load of the sample,
  one for each faction. Then the analysis side can match the bytes.
- G2-012, the ships. The third ship field holds the 樓船. What field
  holds the 鬥艦, and what field holds the 木獸? The clone keeps the
  鬥艦 at the second ship field and the 木獸 with the 衝車 (assumed,
  Q-006).
- G2-052, the attack of 殲滅. In run 7 the order used a tactic. The
  氣力 went from 100 to 85, and the target went into 混亂. In run 8
  夏侯淵隊 shot from two hexes. The clone attacks with the normal attack
  of the class.
  What picks the tactic of a long order? Does the unit use its best
  tactic when the morale allows it, and the normal attack otherwise?
- G2-052, the end of 驅趕. The clone ends 驅趕 after one attack (an
  assumption). When does the original end 驅趕: after one attack, when
  the target leaves the area of the base, or when the target is gone?
- G2-058, the path of a long order. The clone plans the lowest-cost
  path over the terrain and treats own cities and bases as waypoints
  (Q-227). Does the unit of the original stop in an own base on the
  way, as a move of one turn does?
- G2-036, G2-063, G2-065, the days of a journey (抵達日數). The spec
  gives no rule. The clone counts 10 days for each city region that
  the journey enters. Two regions are neighbours when they touch, and
  the regions of gates and ports on the way are free (Q-228). This
  gives 10 days for 江陵 to 江夏, as the report. The report also gives
  10 for 新野 to 汝南, and 50 for 襄陽 to the capital of 袁紹. Please read
  the 日數 column of the 移動 dialog of 江陵 for all bases of 劉表. Please
  also read the 日數 of the 流言 dialog of 新野 for five bases. Then the
  builder can check the rule against more than three numbers.
- G2-065, the result of a plot. The clone rolls the chance at the
  arrival, with the chance of the order. Does the original roll at the
  order or at the arrival? A plot on a base that changed hands on the
  way fails in the clone.
- G2-060, the window 官職. The original drags an officer onto a row.
  The clone selects a row and an officer, then clicks 任命. What does
  the original do when the player drops an officer on a row with a
  holder: a swap, a refusal, or a dismissal?
