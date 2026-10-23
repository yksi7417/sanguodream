# Progress of the second clone gap report

This file records the work of the builder on the gaps of
spec-src/spec/ui/clone-gap-report-2.md (G2-001 to G2-065). The observer
task reads it at the start of each run. The order of the work is section
6 of the report. The spec revision is 20da6c0. The play run of
2026-10-22 (spec/experiments/gap2-answers-2026-10-22.md) answered most
of the questions of the groups 1 to 5. The rows below name it as "the
run of 2026-10-22".

## Fixed gaps

| ID | What changed | Commit | Tests |
|---|---|---|---|
| G2-001 | The load reads the turn counter of the header (0x07C) and sets the date to the scenario start plus one turn for each count. The sample loads at 197/10/1 in winter. | group 1 | unit/calendar, unit/save_format, feature/load_save, feature/turns |
| G2-002 | The load takes the action points (table A, +12) and the technique points (table B, +61) of each force from the save. 劉表 starts with 62 and 23. | group 1 | feature/turns |
| G2-003 | The load runs no AI command, recovers no action points and starts no delegated corps. It gives the turn to the player at once and emits the turn start. The cities keep the soldiers of the file. | group 1 | feature/turns |
| G2-004 | The load takes the acted flag of each officer (+148 bit 0). A unit whose leader acted has acted, so its plate shows 完 and 進行 asks no question. | group 1 | feature/turns, feature/save_map |
| G2-012 | The load reads the morale of the city (+84). The third ship field (+68) is the stock of the 樓船: 江陵 4, 江夏 1. | group 1 | unit/save_format, feature/load_save |
| G2-035 | The load reads the loyalty of each officer from +104. The run of 2026-10-22 confirmed the byte: the screen shows 100 for every value of 100 or more, and the game keeps the surplus (韓嵩 107, 張良 113, 李傕 242). The clone keeps the byte of the save, shows at most 100, and a loss takes from the surplus first. A gain stops at 100. | 4a1d459, settled rules | unit/save_format, feature/load_save, feature/court |
| G2-042 | After 決定 of 出征 the new unit is selected and its range shows. A left click on the plate of a unit is a click on its hex. A click on an own unit with no selection selects it. | group 2 | ui |
| G2-052 | A left click on an enemy unit while an own unit is selected opens 殲滅 and 驅趕, with the panel 預定抵達時間, 所有兵糧 and 通過據點. 殲滅 marches on the target and attacks it in the same turn when it is in reach, then at each 進行 until it is gone. The plate shows 委. A left click on an own unit asks for the follow order with 是 and 否. The run of 2026-10-22 showed that the unit uses a tactic when the order allows it, and the normal attack otherwise: the long order of the clone now tries the tactics of the class first. | group 2, settled rules | feature/orders, ui |
| G2-056 | A land unit enters a river hex as a 走舸 at the cost of the boat row. The far-hex order 移動 takes it onto the river over several turns. | group 2 | feature/orders |
| G2-058 | A left click on a hex out of the range opens 移動 (and 設置, grey) with the days to arrive. 移動 moves the unit to the end of its range in the same turn, marks it 委, and moves it on at each 進行 until it arrives. 委任部隊行動 runs the steps now and is white when a unit has an order. | 3a621c8 | feature/orders, feature/march, ui |
| G2-036 | 人材 > 移動 opens a dialog with the target city and its days, the free officers of the city with check marks, and 行動力 20. The officers travel for the days and arrive at a turn start, with a log line. The days are 10 for each city region that the journey enters (assumed, Q-228): 江陵 to 江夏 gives 10, as the report. The run of 2026-10-22 read 59 rows (fixtures/journey-days.tsv). The rule fits the 18 rows of 10 days, and 1 of the 41 farther rows. The handler checks the rows of 10 days. The rule of the farther bases is a question below. | group 3 | feature/people, ui, fixture/journey-days |
| G2-037 | 人材 > 召喚 opens a dialog with the officers of the other cities of the faction, their base and days, and 行動力 20. The officers travel to the city. | group 3 | feature/people, ui |
| G2-060 | 君主 opens a second column: 評定, 軍團, 軍師, 官職, 仲介 (grey), 流放, 研究能力, 培育武將. 官職 opens a window with the posts (官職名, 功績, 指揮, 效果, 武將), the officers without a post, 任命, 免職, 全解除, 回復原狀 and 決定. The clone selects a post and an officer in place of a drag. 流放 opens a list of the officers. 研究能力 and 培育武將 open the abilities window. The run of 2026-10-22 showed that a drop on a row with a holder does nothing: the clone refuses 任命 on a taken post, as before. | group 3 | ui, feature/court |
| G2-065 | 計略 opens a second column: 二虎競食, 驅虎吞狼, 流言. 驅虎吞狼 and 流言 open a dialog with the bases of the other forces (據點, 太守, 現役, 治安, 日數), one executor from the free officers of the city in the order of 智力, the gold, the days and 行動力. The plot goes on the road for the days, and the result comes at the turn start of the arrival with a log line that names the officer and the base. | 8cc350d | feature/people, feature/court, ui |
| G2-014 | The technique points come from one rules.tsv row for each action (technique.*), with the status of each row: observed for the gap reports and Q-158, claimed for the video of tech-tree.md. 巡察 3, 訓練 3, 徵兵 2 to 5 by the soldiers, 商人 0, 生產兵裝 5 (10 for a siege weapon or a ship), 開發 0 at the order and 15 at the completion, 吸收合併 10, a tactic 15, a destroyed unit 25, 破城 125, the loss of a city 225 plus 25 for each facility, 探索 10 when it finds an officer, 登庸 20, 流放 of a captive 3, 流放 of an own officer 5 plus 5 scaled by the best ability, 親善 0, 同盟 25, 停戰 15, 解除同盟 +50, 贖人 25 and 15, 請求援軍 15, 勸降 500, 二虎競食 15, 流言 15, 驅虎吞狼 50. | group 4 | unit/economy, feature/economy, feature/court, fixture/conscript |
| G2-011 | 徵兵 gives floor(F x (1000 + 6 x S x 治安 / 100)) soldiers, with F 1.0 for a level-1 barracks and 1.5 for level 3 (the run of 2026-10-22, 13 rows within 10 soldiers or 1 percent). It takes 10 治安 and 9 氣力. The fixture handler sets the charm of the officers, the level of the barracks and the public order of the city through the editor. Two rows with 程普 (skill 名聲, 1.5 times the soldiers) stay pending: the clone has no skills. | group 4, settled rules | unit/economy, feature/city_commands, fixture/conscript |
| G2-025 | 商人 uses E = 400 / (450 - P), a purchase gives floor(G x R x E) grain and a sale floor(N x E x 0.8 / R) gold. The dialog shows 交易效果 as a whole percent. All 16 rows of fixtures/merchant.tsv fit (the purchases of 長沙 within 3 grain). | group 4 | unit/economy, feature/city_commands, fixture/merchant |
| G2-013 | The price R is a column of cities.tsv: 柴桑 3, 新野 5, 江陵 7 and 江夏 3 as observed, 5 for the other cities (assumed). The panel shows it. At each month start it drifts by -1, 0 or +1 within 3 to 7 (assumed, Q-155). The income stays open: see the questions. | group 4 | unit/content, feature/city_commands |
| G2-049 | A unit eats one grain for 10 soldiers each turn (2043 soldiers eat 204). The preset of 出征 proposes two grain for each soldier (G2-044: 2043 soldiers, 4086 grain). The grain of a unit of the save stays open (Q-226). | group 4 | feature/march, feature/save_map, feature/ai |
| G2-028 | The clone of the report rolled a plague (10 治安, 5 percent of the soldiers) because its date was wrong before G2-001. A plague starts only in the months 1, 4, 7 and 10. A regression test loads the sample, reaches 197/11/1 and checks that 江夏 keeps its soldiers and its public order. The gold income and the free weapons stay open: see the questions. | 0ac20d2 | feature/turns |
| G2-027 | A transport of the save with the task 移動 keeps a long order to its city and moves at 進行. At the city the cargo, the soldiers, the gold and the grain go into the city. Then the question 輸送隊到達 waits for the player with the leader, the city and two choices: 將其留在該據點, or 令其返回原據點 (the officers go back on the road, 10 days for each city region). The dialog of an event now names its base. A step of a long order resets the scratch arena between the plan and the reach: the real map overflowed it. | group 5 | feature/people, feature/rivals, feature/playable |
| G2-055 | An officer past its death year rolls its death at each month start, not at the year start (朱儁, death year 195, died at the change to 197/12/1). The chance for each month is assumed (age.death_month_chance 10, Q-029). The deputy leads the unit after the death. The death is a message of the turn start with the portrait of the officer, and a line of the log. | group 5 | feature/turns |
| G2-062 | The log line of a complete research names the technique (開發投石的技巧研究完畢), and the turn start shows it as a message. The research of 劉表 in the sample does not load: the spec marks the research fields of the save as open (Q-202, see the questions). | 196a186 | ui/report (the line), feature/growth |
| G2-063 | The envoy of a diplomacy command goes on the road for the days of 抵達日數 to the capital of the target (travel.cpp, as the plots). The result comes at the turn start of the arrival, with the command as its cause. The dialog lists only the free officers of the city that did not act, and shows 抵達日數 for the chosen faction. The relations of the save stay open (see the questions of group 1). | P1 rest | feature/court, feature/replay |
| G2-048 | The 技巧 of a battle came with group 4: a destroyed unit gives 25, a tactic 15 (0ac20d2). The run of 2026-10-22 settled the merit and the experience: an attack gives 20 merit to each officer of the attacker, an arrow attack 10, a tactic 50, a destroyed unit 320 (350 with a tactic). An attack gives 1 force experience, a tactic 3. A destroyed unit gives 3 leadership experience, and the officers of the unit that was hit gain 1. The rows are rules.tsv (merit.*, experience.*). The fixture handler checks a melee attack and a destroy. The arrow and tactic rows stay pending in the handler. | 0ac20d2, settled rules | feature/combat, fixture/battle-merit |
| G2-008 | The header of the list panel is the selected city when the player owns it, else the first own city. The rows with a base that can act come first, then the grey rows. | P2 batch 1 | ui (the list tests) |
| G2-015 | An item of the city menu is grey when its command cannot run: no free slot (開發), no facility, a value at its maximum, a limit of the turn, no free officer, no item or an enemy near. 吸收合併 stays white: its probe needs the two slots of the dialog. The window still scrolls. | P2 batch 1 | ui |
| G2-016 | 現役 counts all officers of the city, also those in its units. 在野 counts the free officers of the city that appeared (assumed, Q-223). | P2 batch 1 | ui |
| G2-019 | A new line scrolls the log to its end. A corps of a save that plays as a faction shows the name of its leader, not 勢力 N. | P2 batch 1 | ui |
| G2-023 | The factions of the Diplomacy panel show the name of the ruler or the leader. The panel shows no line of the last command. The panel stays one window with the plots. | P2 batch 1 | ui |
| G2-030 | 巡察 is grey when 治安 is at its maximum (the probe refuses with the value at the maximum). | P2 batch 1 | ui |
| G2-033 | 委任部隊行動 is white when a unit of the player has not acted, or has a long order. | P2 batch 1 | ui |
| G2-034 | The develop dialog shows 空地 as the free slots of the total. The period, the lock of a facility, the lines 耐久, 效果 and 限制, and the state 建設中 stay open. | P2 batch 1 | ui |
| G2-041 | 授與沒收 is grey when no officer and no city of the force holds an item. | f4e7fbc | ui |
| G2-029 | An item of 生產兵裝 is grey when the city has no facility for it or the force lacks its technique. The item shows the cost of the city (560 for 槍 in a pike city). The dialog shows the 效果 line of 槍, 戟, 弩 and 軍馬. The log names the item and the new stock (新野之槍庫存數變為29644（+3315）). The items with an unknown technique still show, grey. The skill row and the speech stay open. | P2 batch 2 | ui |
| G2-031 | 商人 has one slider from the full sale (left) to the full purchase (right). The centre trades nothing and 決定 is grey there. The dialog shows 交易效果 (G2-025). | P2 batch 2 | ui |
| G2-045 | 攻擊, 齊攻, 戰法 and 單挑 are grey without an enemy unit within 2 hexes. The unit panel shows no line after a command that ran; it shows the reason of a refusal. The move before the menu and the cancel of a target stay as they are. | P2 batch 2 | ui |
| G2-047 | Esc, or a left click outside a context menu, closes it. | P2 batch 2 | ui |
| G2-050 | 攻擊 is grey without an enemy next to the unit, and the unit panel shows no line after the attack (see G2-045). The speech of the leader, the battle show and the target hexes on the map stay open. | P2 batch 2 | ui |
| G2-051 | The tab 部隊 lists the units that can act first, then the units that acted in grey (the list of M22). | M22 | ui |
| G2-053 | The unit panel opens at the left edge of the screen under the status area, not at the centre of the map. | a5ffabb | ui |
| G2-039 | A search with no find gives the log line 蒯良在江陵的探索失敗. A find gives 發現人才. The executor list with 軍師推薦, 政治 and 眼力 stays open. | P2 batch 3 | feature/court |
| G2-038 | The log line of a failed 登庸 names both officers (高長恭登庸卞氏失敗). A failure gives 0 技巧 (group 4). The dialog with a target list that has columns, and the speech of the executor, stay open. | P2 batch 3 | feature/court |
| G2-059 | A force that loses its last city falls: the ledger has 張楊軍滅亡, the log shows it, and the turn start shows it as a message to every player. The picture of a burning castle stays open: the clone has no such picture. | P2 batch 3 | feature/combat |
| G2-040 | 人材 > 褒賞 opens a dialog with the officers of the whole force that can get a reward (忠誠 below 100, not the ruler, not in a unit, not away, no reward this turn), each with a check box, the columns 武將, 忠誠, the rise, 所在, 身份 and 功績, the buttons 一併 and 一併解除, the cost of the set and 決定. The dialog checks every row when it opens, as the original does. One order rewards the set for 5 行動力 and 100 gold for each officer, and the city of the order pays. The rise of 忠誠 is a formula of rules.tsv (reward.loyalty 10, less one for each reward.compatibility_divisor 15 of the distance of compatibility to the ruler, at least reward.loyalty_min 1). The spec names the compatibility as the one factor and gives no numbers (Q-028): the rows are assumed, and the fit is a question below. | feat/court-feedback | unit/court, feature/court, ui |
| G2-054 | 戰法 opens the tactics as a column next to the unit menu, and the menu stays. A row shows the cost in 氣力 in brackets. A tactic with no target in its pattern is grey (the probe of the command over the hexes within 3). A choice closes the menu and starts the pick. During the pick the scene shows the hexes that the tactic can hit in red, and the status bar says 以戰法進行攻擊. The speech of the leader, the dark map with the cross icon and the show of the shot stay open (G2-050). | P2 batch 4 | ui |
| G2-064 | The research window stays on the screen: a dialog of a known size clamps its place to the display (ui_context_place_sized). Each technique shows its state: green and grey when the force has it, blue with the turns left while the research runs on it, grey when its line needs the level before it, white when it is open. A hover names the state with the points and the gold. The menu item 研究技巧 probes every technique, not the first one, so a force that owns the first technique still sees the item white. | P2 batch 4 | ui |
| G2-024 | The city window opens at its top each time, so the menu buttons stand at the same place at each open. | P2 batch 4 | ui |
| G2-044 | 出征 opens on the screen with empty slots and a grey 決定. The presets 編制槍兵, 編制戟兵, 編制弩兵 and 編制騎兵 fill the slots with the proposal of the engine, two grain for each soldier and 1000 gold (the preset of the original; the gold is what the city can give). A preset or a class without its weapon in the city is grey. The row 武器 has one button for each land class with the 效果 line of the chosen weapon. Three sliders give the soldiers, the gold and the grain with the days it lasts. 部隊能力 has the tabs 陸上 and 水上 with the five values, the grade of the class, 氣力 and the tactics of the class (grey when the grade of the unit is below the grade of the tactic). 據點庫存 shows the weapon, the soldiers, the gold and the grain of the base before and after, red when a line goes to zero. The pentagon chart, the icons and 編制建設 stay open: the spec gives no rule for the officers of 編制建設. | P2 batch 5 | feature/march, ui |
| G2-057 | The row 艦船 offers the 走舸 always, and 樓船 or 鬥艦 when the city holds one. The order carries the ship: the sortie takes one ship of the stock, and the unit rows and fights as that ship on the water (the effective class of the sim). A unit that enters a city gives the ship back. The tab 水上 shows the values on the water, and the unit panel shows the ship with its grade. | P2 batch 5 | feature/march, ui |
| G2-046 | The unit panel shows 狀態 (普通 when nothing is on the unit), the skill of each officer in a red box, 艦船 with the ship of the unit (the 走舸 without one) and its grade, and 俘虜. The clone keeps no captive on a unit (a captured officer goes to a city of the captor, Q-176), so 俘虜 is 0. | P2 batch 5 | ui |
| G2-038, G2-039 (the dialogs) | The Court panel shows the clue of the adviser next to 探索 and next to 登庸 (ui/city-commands.md: the estimate after the choice of a target). The clue has three bands of the chance in percent (advice.low_below 34, advice.high_from 67, assumed) in three colours, with the name of the 軍師. A hover shows the chance. The words and the thresholds of the original are a question below. | feat/court-feedback | unit/court, feature/court, ui |
| G2-022 | 訓練 raises the morale by floor(4 + 0.115 x S) in a city with a 鍊兵所 and by floor(3 + S / 15) without one. S is the force of the first officer plus half of the force of each other officer (the run of 2026-10-22, 17 rows). The fixture handler checks the rows without a drill ground, builds one, then checks the rows with it. | settled rules | unit/economy, feature/city_commands, fixture/train |
| G2-026 | 生產兵裝 gives F x (1000 + 10 x I) weapons, with F 1.5 for a level-3 smithy. I is the intellect of the first officer plus half of the intellect of each other officer (the run of 2026-10-22, 6 rows). | settled rules | unit/economy, feature/city_commands, fixture/produce |
| G2-043 | The attack of a unit is 武力 x A x C, and the defence is 統率 x A x D. A is the aptitude factor (S 0.95, A 0.85, B 0.75, C 0.65). C and D are the class factors (槍兵 1.0 and 1.0, 戟兵 0.95 and 1.1, 弩兵 0.9 and 0.95, 騎兵 1.1 and 0.95). A sword unit uses 0.36 with no aptitude. 建設 is 50 plus two thirds of 政治. 智力 is the intellect of the officer. The rows are rules.tsv (class.*, unit.*, aptitude.*). The fixture handler sets the abilities and the aptitudes of one officer through the editor: the pike, the halberd and the sword rows fit within one point, the crossbow and the cavalry rows within two. The ship rows stay pending. | settled rules | unit/units, feature/march, fixture/unit-values-sortie |
| start of 194 | The player compared 武將一覽 of a new game of 群雄割據 as 曹操 in the original and in the clone (2026-10-22). The scenario loader reads +13 of a city as the troops and +17 as the gold: 陳留 has 28,000 troops and 5,600 gold. It reads the loyalty +104 of a scenario officer. The skill byte is a file id, and skills.tsv gives its number (column file_id). load_scenario starts no turn: the first end_turn after the choice of the rulers starts it, so no officer gets the merit of an AI command before the start. Tutorial 21 gives the details. | fix/scenario-start | unit/scenario_format, unit/court, feature/scenario_fixture, feature/playable |
| G2-005 | The gain of 技巧 at the turn start: each built 鍊兵所 of the force gives 15 (the run of 2026-10-22, rules.tsv technique.drill_ground_turn). The recovery of 行動力 stays open: see the questions. | settled rules | feature/turns |
| G2-017 | The command dialogs of the city show the skill of each chosen officer under its portrait. | P2 batch 6 | ui (build) |
| G2-018 | The last officer of a city command speaks its result (assumed from two observations of the run). The float of a value shows an arrow before the number, down for a loss and up for a gain. 徵兵 speaks the warning of the public order, and 巡察 the line of the third officer. | P2 batch 6 | feature/report |
| G2-006 | After the load, the camera stands at the capital. The adviser says 可終於輪到我們了啊 after the heading of the first turn. | P2 batch 6 | ui |
| G2-007 | The turn start ends with the lines of the adviser: the threat, when an enemy city 10 days from an own city has more soldiers (assumed), and 請下達下一個指示. The adviser speaks them with the name of the ruler first. The camera moves to the city of each message. The messages of a turn that the player did not read close when the turn ends. | P2 batch 6 | ui |
| G2-032 | At a month start each captive escapes with the chance captive.escape_percent (assumed 10). The event captive_escaped has a report, the log line 逃亡 and the speech 我已摸清了警備的編制. | P2 batch 6 | feature/court |
| 齊攻 (play report of 2026-10-23) | 齊攻 takes only the units next to the target. A crossbow unit two hexes away keeps its turn. A unit after a destroyed target keeps its turn. | P2 batch 6 | feature/combat |
| saves of older builds (play report of 2026-10-23) | The header of a save carries the layout of sim_state. A file with another layout does not load. The content comes from the pack, not from the file. The saves of 2026-10-22 load through a migration. tests/fixtures/saves/slot_01.hxs guards the load. | save compat | feature/save_compat |
| unit log lines (play report of 2026-10-23) | The log names a unit by its commander (周泰隊：士兵 -232) in place of 一支部隊. The commander is the subject of a unit event, so the name stays after the unit is gone. | P2 batch 6 | feature/combat |

## In progress

- Group 1 has no open work except the parts of G2-061 and G2-063 that
  the spec does not support (see the questions). The corps of a save still act as
  the factions 0 and 12, and the adviser and the relations keep their
  start values.
- Group 2 has no open work. The long order of the clone uses the first
  tactic that the unit can pay, else the normal attack (G2-052, the run
  of 2026-10-22). The end of 驅趕 stays a question below.
- Group 3 has no open work. 仲介 stays grey. The 移動 and 召喚 dialogs
  list cities only, not ports and passes. The log line of an arrival
  names the officer but not the base.
- Group 4. G2-005 stays half open. The gain of 技巧 at the turn start
  comes from the drill grounds. The recovery of 行動力 has no rule
  that fits the 12 corps of the run (see the questions). The income of
  G2-013 and G2-028 stays open for the same reason. The rows
  technique.facility_lost, technique.facility_lost_leveled and
  technique.tactic_extra_unit are in rules.tsv. No code gives them yet.
  The clone has no facility that an enemy destroys. A tactic that hits
  more units gives 15 one time.
- Group 5. The research of the save (G2-062) stays open until the
  analysis side finds its fields. The clone shows the question of a
  transport after 進行 and before the date changes, as the original. The
  death of an officer is a message of the turn start, not a picture of a
  death bed: the clone has no such picture.
- The other P1 rows. G2-022 (訓練), G2-026 (生產兵裝) and G2-043 (the
  unit values) follow the rules of the run of 2026-10-22 (the table
  above). G2-061 waits for the corps fields of the save (the questions
  of group 1).
- P2 batch 1 (f4e7fbc): the greys of the city menu, the counts of the
  city panel, the list header, the log and the Diplomacy panel.
- P2 batch 2 (a5ffabb): the produce and merchant dialogs, the unit menu,
  the context menus and the place of the unit panel.
- P2 batch 3: the results of 探索 and 登庸 in the log, and the end of a
  force.
- P2 batch 4: the tactic column, the research window and the city
  window at its top.
- P2 batch 5: the 出征 dialog of the original with the ships, and the
  unit panel.
- Next: the other P2 rows, then P3. The P2 rows are:
  - the skill row and the speech of the command dialogs (G2-017,
    G2-018);
  - the messages of the load, the turn start and the month start
    (G2-006, G2-007, G2-032);
  - the durability maximum (G2-009, Q-222).
  The P3 rows are the glyph of 儁 (G2-010), the portraits of 蔡和
  (G2-020) and the ghost text of the city panel (G2-021). The default
  portraits by name make G2-020 moot.

## Questions for the observer

- G2-035, the 褒賞 list. The byte +104 is the loyalty (the run of
  2026-10-22). 鮑叔牙 (99, in a unit) was not in the 褒賞 list of the
  original. What is the second filter of the list: the officers in a
  unit, the officers away, or another rule?
- G2-040, the rise of 忠誠 of one 褒賞 (Q-028). The clone uses 10, less
  one for each 15 of the distance of compatibility to the ruler, at
  least 1. The spec names the compatibility only. To fit the formula,
  please read these items for each rewarded officer:
  - 忠誠 before the reward and after it (武將一覽). Read it again after
    the next 進行. The videos claim that the reward settles at the end
    of the season.
  - The 相性 of the officer and of the ruler (武將情報).
  - The 性格 of the officer and the 魅力 of the ruler.
  - Whether the force has a 符節台.
  Reward at least four officers. Take one near the 相性 of the ruler
  and one far from it (a distance near 75). Take one with 忠誠 below 50
  and one with 忠誠 above 90. Then the builder can fit the base and the
  slope of the compatibility. The data also shows a part of the current
  loyalty, the personality or the charm of the ruler.
- G2-038, G2-039, the estimate of the adviser. The clone shows three
  bands of the chance (below 34 low, 67 and more high) with assumed
  words. Please record the exact words of the 軍師 after the choice of a
  target in 登庸 and after the open of 探索. Take a sure case, a
  doubtful case and a hopeless case. For each case, note the 魅力 of the
  recruiter, the 忠誠 and the 相性 of the target, and the 政治 of the
  searcher. Does the message change with the 智力 of the 軍師, or
  without a 軍師?
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
- G2-052, the end of 驅趕. The run of 2026-10-22 saw 驅趕 continue over
  two attacks while the target stayed near 長安. The clone ends 驅趕
  after one attack (an assumption). When does the original end 驅趕:
  when the target leaves the area of the base, or when the target is
  gone?
- G2-058, the path of a long order. The clone plans the lowest-cost
  path over the terrain and treats own cities and bases as waypoints
  (Q-227). Does the unit of the original stop in an own base on the
  way, as a move of one turn does?
- G2-036, G2-063, G2-065, the days of a journey (抵達日數). The run of
  2026-10-22 read 59 rows (fixtures/journey-days.tsv). The clone counts
  10 days for each city region that the journey enters, with the
  regions of gates and ports free (Q-228). That rule fits the 18 rows
  of 10 days and 1 of the 41 farther rows. The builder tried three
  other rules against the 59 rows. Ten days for each region, ports and
  gates included: 31 rows. Ten days for each 22 hexes of the straight
  distance: 34 rows. Ten days for each 147 points of the lowest
  foot-path cost over the terrain of the map: 23 rows. No rule of the
  map alone fits. A question for the analysis side: does the game keep
  a table of the days between the bases, or a route graph of the
  bases? The 10-day bases of 江陵 are not all neighbours of its region
  (九江港 and 柴桑 are far down the river).
- G2-065, the result of a plot. The clone rolls the chance at the
  arrival, with the chance of the order. Does the original roll at the
  order or at the arrival? A plot on a base that changed hands on the
  way fails in the clone.
- G2-005, the recovery of 行動力. The run of 2026-10-22 read the
  recovery of 12 corps (fixtures/action-points.tsv). It is a fixed
  number for each corps:
  - 劉表, 9 cities, 統率 48, 魅力 83, adviser 100: 174;
  - 劉璋, 5 cities, 16 and 65, adviser 88: 103;
  - 孫策, 1 city, 92 and 92, adviser 98: 57;
  - 袁術, 1 city, 44 and 39, adviser 76: 40.
  No line of the city count and one ability fits all rows.
  The clone keeps the assumed rule (Q-019). A question for the analysis
  side: what inputs make the recovery? The 符節台 count of the AI forces
  is unknown.
- G2-013 and G2-028, the income. The run of 2026-10-22 read the income
  of 11 cities with their facilities and their public order
  (fixtures/city-income.tsv). The spec still names the initial income
  value of a city as unknown (Q-021). The fixture handler prints the
  rows as open. A question for the analysis side: what is the base
  income of a city, and what share does each facility level add?
- G2-028, the free weapons of a specialty city. The run of 2026-10-22
  read 13 rows (fixtures/free-weapons.tsv). The clone gives 100
  (assumed, Q-156), and the handler prints the rows as open. The
  rule by the governor or the 鍛冶廠 stays open (Q-022).
- G2-011, the loss of 治安 and 氣力. In the run of 2026-10-22 one
  officer in 新野 took 12 治安 and 6 氣力. Three officers in 江陵 took 10
  and 9. The clone takes 10 and 9 for any count. What rule gives the
  loss: the count of the officers, the charm, or the public order?
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
- G2-048, the merit of an attack that took many soldiers. The run of
  2026-10-22 settled 20 for an attack, 10 for an arrow attack and 50 for
  a tactic. The earlier run 7 saw +100 for an attack that took 1888
  soldiers with no destroy. Does the merit grow with the soldiers that
  the attack took, or did that attack use a tactic?
- G2-043, the values of a unit. The run of 2026-10-22 settled the
  factors of the pike, the halberd, the crossbow and the cavalry. The
  cut of the product to a whole number stays open: a round fits some
  rows, and a floor fits others. The factors of the ships and of a
  second officer stay open (Q-170). Please read the five values of one
  officer with 走舸 and with 樓船, and of two officers together with
  槍兵.
- G2-010, the glyph of 儁. The clone shows 朱?隊 because the name decoder
  gives a private-use code point for the byte pair of 儁. The standard
  Big5 table (data/unicode/BIG5.TXT) has no 儁, and neither has cp950.
  Big5-HKSCS has it at 0xFA67. Two questions for the analysis side. What
  two bytes does the officer record of 朱儁 hold for 儁? Do other names of
  the scenario files use bytes outside the standard Big5 table? With the
  list, the clone adds a supplement table of the pack.
- G2-021, the ghost text of the city panel. The window clears the back
  buffer in each frame, so the copy of the text is not an old frame. The
  builder did not reproduce it headless. Does it still show after the
  build of 2026-10-23? If yes, a screenshot of the first second helps.
- G2-027, the unit of 吳巨. The report says that the clone of run 5
  showed no unit near 江陵 for the transport of 吳巨 (3790 soldiers). The
  save holds it as unit 53 at the map hex 69,148 with the task 移動 to
  the base 29. Please compare the units near 江陵 again with the new
  build, and name the hex of the unit of 吳巨 on the status bar.
- Start of 194, the officers of 曹操. SCEN002 puts 27 officers in force
  0, with status 3 for 夏侯恩 (appears 194) and 曹純 (appears 189). The
  武將一覽 of the original showed 24 rows and a scroll bar. Please scroll
  the list down and name the rows below 司馬朗. Are 曹操, 夏侯恩 and 曹純
  there? The clone keeps the 27 officers until then.
- Start of 194, the gold and the troops. scenario-file.md still names
  the city field +13 the gold and +17 the troops. The swap of the save
  fields +16 and +20 (2026-10-17) applies to the scenario too: 陳留 has
  28,000 at +13 and 5,600 at +17. Please correct the table of the city
  record in scenario-file.md.
- Start of 194, the skill ids. The skill byte +124 is not the number of
  skills.md - 1. The clone matched the holders of skills.md against
  SCEN002. It wrote a file id for each number in skills.tsv. The guide
  book names a holder for each of 97 numbers. The skills 槍神 (35), 霸王
  (41) and 看破 (61) have no holder. The clone gave them the free ids 42,
  48 and 60 by the order of their neighbours. Please add the table of the skill ids to skills.md, and
  confirm the three ids with an officer that has one of these skills.
- Start of 194, the ports. The plate of 官渡港 shows 3000 soldiers, the
  assumed rule base.start_troops. The scenario file has no mapped record
  of a gate or a port (scenario-file.md, 0x2764E "to map"). Please read
  the soldiers of 官渡港 and of two other ports at the start of 194.
- G2-017, the officers of a command dialog. The clone shows the skill of
  each chosen officer. The rule that picks the three officers of the
  original is open: the ability of the command, then the skill? Please
  open 徵兵 in two cities and write down the three officers and their
  abilities and skills.
- G2-018, the speaker. The clone makes the last officer of a city command
  speak, from two observations. Please run 徵兵 and 巡察 with one, two and
  three officers, and write down who speaks each time.
- G2-007, the threat. The clone names the enemy city 10 days from an own
  city with more soldiers. Please note the threat line of three turn
  starts with the soldiers of the two cities.
- G2-032, the escape. The clone rolls 10 percent for each captive at each
  month start. Please count the captives and the escapes over 12 months
  of one game.
