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
- U-004, a damaged building of a save (Q-233). The load marks a building
  complete only when 耐久 is at the maximum. Thus the 連弩樓 at 98,105
  with 耐久 554 of 700 shows a scaffold on the 3D map and does not shoot.
  The clone keeps this rule until the answer.

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

| U-001 | The 3D map draws each field building of building_at with a placeholder model of its kind, in the colour of its owner. The kinds are a camp, a tower, a stage, a wall, a ring of stones and a trap. A trap without an owner is grey. A build in progress is a scaffold with the smaller, dimmed model inside, as on a slot. The hover panel of a building gives its name, its owner and 耐久 current / maximum. | batch 8 | unit/render, feature/save_map, ui |
| U-002 | A row of the list 設置 is grey when can_issue refuses the building on each hex next to the unit. The engine refuses a building without its technique, without the gold, or without a buildable hex. A building that the engine accepts shows on the 3D map (U-001). | batch 8 | ui |
| U-003 | unit_at gives effective_type, the class that the unit fights with on its hex. On water a land unit fights as its ship. The list 戰法 of the unit menu follows effective_type. A unit on a 樓船 thus lists 火矢 and 猛撞, and a unit on a 鬥艦 also lists 投石. The tab 水上 of 部隊能力 in the sortie dialog lists the tactics of the ship. | batch 8 | feature/combat, ui |
| U-005 | The planner of a computer faction, of the adviser and of the council uses CORPS_FLAG_FREE_HAND. CORPS_FLAG_ALL also holds 儲存金錢, and with it the AI never developed or researched. | batch 14 | feature/ai |
| U-006 | A debate of a recruit, a search or an envoy of the computer runs to its end without a pause. The open debate refused the commands of each later faction with SIM_REFUSAL_BUSY, for up to five turns. | batch 14 | feature/ai |
| U-007 | The row develop builds toward ai.develop_gold 3, ai.develop_grain 3, ai.develop_conscription 1 and ai.develop_weapons 1, the template of a delegated corps in knowledge/ai.md. The AI first builds one facility of each effect. Before, it filled each city with markets and never built a barracks, so it never conscripted. | batch 14 | feature/ai |
| U-008 | The row patrol of ai_priorities.tsv fires below order 90, not 70. Bandits start below order.bandit_threshold 80 and end at 80. They stayed in the cities of the AI and destroyed its facilities. | batch 14 | feature/ai |
| U-009 | The AI conscripts only when the grain of the city feeds the soldiers for ai.grain_turns 18 turns. A city with less grain builds a farm first. Before, the new soldiers deserted for want of grain. | batch 14 | feature/ai |
| U-010 | The pack han_196 gives a market 75 gold and a farm 1125 grain, as knowledge/city-economy.md says. The PK markets keep their documented ratios. With 50 and 250, a city fed approximately 6000 soldiers, and the AI needs 15000 for a sortie. | batch 14 | feature/ai |
| U-011 | The planner of the AI keeps the salaries of ai.salary_months 3 months in each city. It drops a proposal that spends that gold. The defence of a city does not wait. The row reward has priority 52, above develop. In 15 years the unpaid city months fell from 195 to 75, and the officers that left from 78 to 29. | batch 15 | feature/ai |

| a unit passes through own units (2026-10-24) | A unit of the same force blocked the hex like an enemy, so it cut the range. A unit now passes through an own unit and keeps its full movement. It cannot stop on that hex. A unit of another force still blocks its hex, and its zone of control still stops the move. | batch 13 | feature/march |
| a failed tactic still hits (2026-10-24) | A failed tactic did no damage, because the clone stopped after the failure. It is now a normal attack on the target with less damage than the tactic (combat.tactic_fail_percent 100, assumed). It has no effect and gives no technique points, and the target answers it. The log line names the tactic and says 成功 or 失敗，改為普通攻擊. The lines after it name the units that lost soldiers. | batch 12 | feature/combat, ui |
| no arrow or stone at an adjacent hex (2026-10-24) | The player decided that the arrow tactics and the stone tactics need a target at 2 hexes or more. This applies on land and on water. tactics.tsv of both packs gives range_min 2 to 火矢, 貫箭, 亂射, the 火矢 of the tower and of the ships, and the two 投石. The engine refuses an adjacent target, and the row of the tactic is grey. | batch 11 | feature/combat |
| the place of each log line (2026-10-24) | Each log line names the unit, the facility or the field building where the event happens. A click on the line moves the camera to that hex. The engine gives the place in event_report. The events of a field building and of a fire named the commander of a wrong unit. They now name the owner of the building, or no force. The log goes back to the bottom left and the minimap to the bottom right after a change of the screen size. | batch 10 | feature/report, feature/combat, ui |
| the port question again (2026-10-24) | The click of batch 7 gave the location id of hex_at to base_at. base_at needs the index of base_at_hex. The force check thus failed, and the command menu opened. The click on a unit, the name in the question and the scene click on an own port now read base_at(base_at_hex). The recorder gives the location id 42 in hex_at, as the real engine does. | batch 9 | feature/rivals, ui |
| the play notes of 2026-10-24 | A click on an own port or pass in the range asks 是否進入 X？ with 是 and 否, not the command menu. A unit of the player that stands on an own base enters it at the turn. Each log line starts with its place, 【長安】 or 【張遼隊】, when the words do not name it. A land unit that rows a 樓船 or a 鬥艦 shows that ship. After the selected unit acted, a click on another own unit selects it and asks for no follow order. The hits and the strikes of an action wait for the walk. A unit with a long order shows 完 after its step. The pick of the mouse meets the top of a raised hex, not the ground plane under it. | batch 7 | unit/render, feature/march, ui |
| the entry of a port or a pass (2026-10-23) | A unit of the player that ends its move on an own port or pass enters it. The base takes the soldiers, the weapons, the gold and the grain, and the officers stay in the base (unit_entered_base). 出征 and 輸送 work from the base, and 退卻 goes back into it. The panel of a port shows the fields of the original and the two buttons. The officers in a base that falls become captives. | batch 6 | feature/march, ui |

## In progress

- Every row of report 3 has its fix, or waits on the spec (the section
  above). The next play report decides the next batch.
- Batch 8 closes U-001 to U-003 of spec-src/spec/ui/play-reports.md.
  U-004 waits on Q-233.
- Batch 14 closes U-005 to U-010. A headless game of the AI alone on
  han_196 found them (the play run of 2026-11-07). Batch 15 closes U-011.
  U-012 stays open: the war is slow.

## Questions for the observer

- The damage of a failed tactic. The clone makes a failed tactic a normal
  attack at 100 percent (assumed, combat.tactic_fail_percent). Please
  record the soldiers that the target loses after a failed 突刺 and after
  a normal attack of the same unit. Does the target counterattack a
  failed tactic?

- G3-002, the technique points of a won duel. The clone gives 100 to the
  faction of the winner (assumed). The run saw 0 for the loser only.
  Please read 技巧 before and after a won duel.
- G3-003, the capture of a destroyed unit. The runs saw 1 capture in 6
  officers. The clone uses 20 percent. Please count the captures of ten
  destroyed units.
- G3-019, the repair of a facility. The clone repaired facilities of
  許昌 and 長安 at the month start. Please read the durability of two
  facilities before and after a month start in a city at peace.
- The entry of a port or a pass. The clone gives a base without a morale
  the morale of the first unit that enters it (assumed). A unit on a long
  order stands on an own base on its way and enters only its goal
  (assumed). A computer unit never enters a base (assumed). Please move a
  unit into an own port, read 氣力 of the port, and give a unit a far 移動
  order through an own pass. Does the unit stop in the pass?
