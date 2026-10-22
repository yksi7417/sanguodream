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
| G2-065 | 計略 opens a second column: 二虎競食, 驅虎吞狼, 流言. 驅虎吞狼 and 流言 open a dialog with the bases of the other forces (據點, 太守, 現役, 治安, 日數), one executor from the free officers of the city in the order of 智力, the gold, the days and 行動力. The plot goes on the road for the days, and the result comes at the turn start of the arrival with a log line that names the officer and the base. | 8cc350d | feature/people, feature/court, ui |
| G2-014 | The technique points come from one rules.tsv row for each action (technique.*), with the status of each row: observed for the gap reports and Q-158, claimed for the video of tech-tree.md. 巡察 3, 訓練 3, 徵兵 2 to 5 by the soldiers, 商人 0, 生產兵裝 5 (10 for a siege weapon or a ship), 開發 0 at the order and 15 at the completion, 吸收合併 10, a tactic 15, a destroyed unit 25, 破城 125, the loss of a city 225 plus 25 for each facility, 探索 10 when it finds an officer, 登庸 20, 流放 of a captive 3, 流放 of an own officer 5 plus 5 scaled by the best ability, 親善 0, 同盟 25, 停戰 15, 解除同盟 +50, 贖人 25 and 15, 請求援軍 15, 勸降 500, 二虎競食 15, 流言 15, 驅虎吞狼 50. | group 4 | unit/economy, feature/economy, feature/court, fixture/conscript |
| G2-011 | 徵兵 gives floor(F x (1000 + 5.6 x S)) soldiers (city-commands.md) and takes 10 治安 and 9 氣力. The seven dialogs of the sample fit within 7 soldiers. The fixture handler sets the charm of the officers and the level of the barracks through the editor. | group 4 | unit/economy, feature/city_commands, fixture/conscript |
| G2-025 | 商人 uses E = 400 / (450 - P), a purchase gives floor(G x R x E) grain and a sale floor(N x E x 0.8 / R) gold. The dialog shows 交易效果 as a whole percent. All 16 rows of fixtures/merchant.tsv fit (the purchases of 長沙 within 3 grain). | group 4 | unit/economy, feature/city_commands, fixture/merchant |
| G2-013 | The price R is a column of cities.tsv: 柴桑 3, 新野 5, 江陵 7 and 江夏 3 as observed, 5 for the other cities (assumed). The panel shows it. At each month start it drifts by -1, 0 or +1 within 3 to 7 (assumed, Q-155). The income stays open: see the questions. | group 4 | unit/content, feature/city_commands |
| G2-049 | A unit eats one grain for 10 soldiers each turn (2043 soldiers eat 204). The preset of 出征 proposes two grain for each soldier (G2-044: 2043 soldiers, 4086 grain). The grain of a unit of the save stays open (Q-226). | group 4 | feature/march, feature/save_map, feature/ai |
| G2-028 | The clone of the report rolled a plague (10 治安, 5 percent of the soldiers) because its date was wrong before G2-001. A plague starts only in the months 1, 4, 7 and 10. A regression test loads the sample, reaches 197/11/1 and checks that 江夏 keeps its soldiers and its public order. The gold income and the free weapons stay open: see the questions. | 0ac20d2 | feature/turns |
| G2-027 | A transport of the save with the task 移動 keeps a long order to its city and moves at 進行. At the city the cargo, the soldiers, the gold and the grain go into the city. Then the question 輸送隊到達 waits for the player with the leader, the city and two choices: 將其留在該據點, or 令其返回原據點 (the officers go back on the road, 10 days for each city region). The dialog of an event now names its base. A step of a long order resets the scratch arena between the plan and the reach: the real map overflowed it. | group 5 | feature/people, feature/rivals, feature/playable |
| G2-055 | An officer past its death year rolls its death at each month start, not at the year start (朱儁, death year 195, died at the change to 197/12/1). The chance for each month is assumed (age.death_month_chance 10, Q-029). The deputy leads the unit after the death. The death is a message of the turn start with the portrait of the officer, and a line of the log. | group 5 | feature/turns |
| G2-062 | The log line of a complete research names the technique (開發投石的技巧研究完畢), and the turn start shows it as a message. The research of 劉表 in the sample does not load: the spec marks the research fields of the save as open (Q-202, see the questions). | group 5 | ui/report (the line), feature/growth |

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
- Group 4. G2-005 stays open: the recovery of 行動力 and the gain of 技巧
  at the turn start have no rule in the spec (see the questions). The
  income of G2-013 and G2-028 stays open for the same reason. The rows
  technique.facility_lost, technique.facility_lost_leveled and
  technique.tactic_extra_unit are in rules.tsv. No code gives them yet.
  The clone has no facility that an enemy destroys. A tactic that hits
  more units gives 15 one time.
- Group 5. The research of the save (G2-062) stays open until the
  analysis side finds its fields. The clone shows the question of a
  transport after 進行 and before the date changes, as the original. The
  death of an officer is a message of the turn start, not a picture of a
  death bed: the clone has no such picture.
- Next: the other P1 rows of the report, then P2, then P3.

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
- G2-005, the recovery of 行動力. The spec names the inputs (the cities,
  the leader, the adviser, 符節台) but no formula (Q-019). 劉表 with 9
  cities went 62 to 236 (+174). The clone adds the larger of the
  leadership and the charm of the ruler (83, assumed). Please read these
  numbers at one turn start of the original:
  - the 行動力 of 劉表 before and after the turn start;
  - the 統率 and the 魅力 of 劉表, and the 智力 of the adviser;
  - the number of 符節台 in the nine cities.
  Please read the same numbers for one other force with few cities, for
  example 劉璋. Then the builder can fit a line.
- G2-005, the gain of 技巧 at the turn start. The original gave +37, +75,
  +24, +20, +7, +7, +29, +5 and +12 at the turn starts of the runs. Some
  of these turns had no command. The HUD showed 161 at the change of the
  date and 178 at the start of the turn of 劉表. The spec gives no rule
  for a gain at a turn start. Does a facility give 技巧 each turn, for
  example 錬兵所? How many such facilities do the nine cities of 劉表
  hold? Does the log or the 報告 window name the source of the points at
  the turn start?
- G2-013 and G2-028, the income. The spec gives no formula beyond the
  multipliers and names the initial income value of a city as unknown
  (Q-021). The original shows 資金收入 2047 and 兵糧收入 23575 for 江陵,
  1818 and 9405 for 新野, 1772 and 7990 for 江夏. Please read, for all
  nine cities of 劉表: 資金收入, 兵糧收入, 治安, and the count and the
  level of 市場, 農場 and the other income facilities. With nine rows the
  builder can fit the initial value and the share of each facility.
- G2-028, the free weapons of a specialty city. The original gave 213 戟
  to 江夏 at a month start. The clone gives 100 (assumed, Q-156). Does the
  number change with the 鍛冶廠 count or level, or with the month? Please
  read the gain of 江夏 at two month starts, with its 鍛冶廠 levels.
- G2-011, the barracks factor of level 1. The rule
  floor(F x (1000 + 5.6 x S)) fits the level-3 dialogs of the sample
  within 7 soldiers. The 長沙 rows of 2026-10-20 have a level-1
  barracks, with F 1.0 assumed. They give 1000 + 5.9 x S, for example
  1536 for S 91 and 1383 for S 65. The level factor of the spec is a
  decision of the player, not an observation. Please open 徵兵 in a city
  of the sample with a level-1 or level-2 barracks. Please read the
  prediction for one officer and the charm on the screen. Then the
  builder can fit F for each level.
- G2-011, the loss of 治安 and 氣力. One conscription with three officers
  took 10 and 9. Does a conscription with one officer take the same?
  Please do 徵兵 with one officer in 新野 and read 治安 and 氣力 before
  and after.
- G2-049, the grain of a unit of the save. 霍去病隊 shows 兵糧 10500 in
  the original and 12600 in the clone (Q-226). The clone reads the field
  +13 of the unit record. Can the analysis side check the field against
  the 兵糧 of three units of the sample?
- G2-062, the research of the save. formats/save-file.md says that +67
  of table B is 255 for 劉表 while 技巧表 shows 開發投石 with 80 days
  (Q-202). In the sample, +69 holds 8 and +70 holds 6 for 劉表. Eighty
  days are 8 turns, so +69 can be the turns that remain. The technique
  開發投石 is the level 3 of the line 發明, the sixth line of 技巧表. Does +70 hold the
  line, as 1 to 9? The other forces do not fit this hypothesis. Force 1
  holds 7 and 2 there with no research on 技巧表. The forces 2, 7 and 9
  hold 128 to 130 at +70. Please read 技巧表 of three forces of the sample with a
  research in progress, with the technique and the days left. Then the
  analysis side can match the bytes, and the clone can load the research.
- G2-027, the unit of 吳巨. The report says that the clone of run 5
  showed no unit near 江陵 for the transport of 吳巨 (3790 soldiers). The
  save holds it as unit 53 at the map hex 69,148 with the task 移動 to
  the base 29. Please compare the units near 江陵 again with the new
  build, and name the hex of the unit of 吳巨 on the status bar.
