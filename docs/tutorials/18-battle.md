# 18 Battle: attacks, tactics, plots, fire, buildings and sieges

Files: code/sim/combat.cpp, code/sim/field.cpp, code/sim/content.cpp
(tactics.tsv, plots.tsv, buildings.tsv and critical_skills.tsv),
code/engine/engine_combat.cpp, code/ui/ui_combat.cpp, ui_march.cpp and
ui_war.cpp (the feedback of war).
The data is in data/placeholder/tactics.tsv, plots.tsv, buildings.tsv,
critical_skills.tsv and rules.tsv.
Tests: tests/unit/combat_tests.cpp, tests/feature/combat_tests.cpp, the
M6 tests in tests/ui/ui_tests.cpp, the demo "demo/m6_battle".

## What it is for
This tutorial covers milestone M6: units fight units, buildings and
bases. The specs are:
- mechanics/military-units-and-tactics.md: the attack, the
  counterattack, the 19 tactics, the critical hits, morale and the
  capture of officers;
- mechanics/plots-and-states.md: the battle plots, the states and fire;
- mechanics/military-facilities-and-traps.md: the buildings that a unit
  places, the obstacles and the traps;
- mechanics/siege-and-bases.md: the durability of a base, the blockade,
  the fall of a base and the loot;
- mechanics/terrain-and-map.md: the natural traps and the water.

The features are F-082 to F-093 of FEATURES.md.

## The rule hook and the assumed numbers
The spec documents the classes, the tactics, their costs and ranges, the
class triangle, the skills and the rules of a siege. It gives no damage
formula. The engine sends every damage through one hook:
`sim->ruleset.combat_damage`. The default rule is
`sim_rule_combat_damage`:

    damage = soldiers x attack / (attack + defence) x combat.damage_percent
             x the percent of the attack, and at least combat.min_damage

A content pack can set its own function. Every percent in rules.tsv for
M6 is an assumption of Q-172 to Q-176. The events of a damage have the
assumed flag.

## The tables
- tactics.tsv has one row for each tactic. A row has the classes that use
  it and the lowest aptitude (B, A, S or any). It also has the morale
  cost, the range, the pattern of the hexes, the effect, the damage
  percent and the targets.
- plots.tsv has one row for each battle plot. A row has the morale cost,
  the range and the target (an enemy, an own unit or a hex). It also has
  the effect, the turns of the state, the skills and the terrain.
- buildings.tsv has one row for each building. A row has the kind, the
  durability, the gold and the technique. It also has the weak building
  that it replaces, the range and the value. The dike and the rockfall are natural traps.
- critical_skills.tsv names the skills that force a critical hit, with
  the action (attack, tactic or both), the class and the condition.

A rule names a skill by its number in skills.md.

## 攻擊 and the counterattack
`sim_check_attack` checks these items in this sequence:
1. the unit, its turn and its state (`unit_acted`, `unit_confused`);
2. the class (a siege weapon cannot attack: `no_aptitude`);
3. the range: 1, or 2 for crossbows, ships and 騎射 cavalry (`no_target`);
4. an enemy unit, building, base or facility on the hex (`no_target`);
5. an arrow attack into a forest (`bad_terrain`), unless 射手.

`can_issue` gives the expected damage in `quantity` and the soldiers of
the target in `value_before`. The strike takes soldiers and 5 morale.
A unit that can counterattack answers a melee attack. Crossbows answer
an arrow only with 還射. A critical hit (`critical_hit`) multiplies the
damage. A skill of critical_skills.tsv forces it, else the chance is
combat.critical_chance.

齊攻 (joint attack) strikes with every unit of the faction next to the
hex that can attack it (military-units-and-tactics.md: "several adjacent
units"). A crossbow unit two hexes away keeps its turn. It needs two
units (`no_partner`). The target answers none of them (assumed). When
the target is gone before a unit strikes, that unit keeps its turn.

## 戰法
`sim_check_tactic` needs the class of the tactic, the aptitude grade,
the morale and a terrain that allows the tactics of the class. It gives
the morale cost in `action_points` and the chance of success in
`quantity`. A tactic of the tower always works. The pattern gives the
hexes:
- single: the target;
- line: the hexes from the unit to the target (貫箭);
- front3: the target and its two neighbours next to the unit (橫掃);
- ring: the six hexes next to the unit (旋風);
- splash: the target and the six hexes next to it (亂射).

The class triangle adds combat.triangle_percent: pike beats cavalry,
cavalry beats halberd, halberd beats pike. The effect then moves a unit
(knock, pull, advance, pass), confuses it or starts a fire. A unit that
slides into a unit or a building stops, and both lose soldiers. No unit
answers a tactic.

## 計略 and the states
`sim_check_plot` needs the morale, the range (more with 鬼謀), the target
and the terrain of 伏兵 (a forest). The chance is plot.base_success plus
the difference of intellect, from 5 to 95 percent.

These skills change the chance:
- 虛實 and 神算 always work against a lower intellect;
- 洞察 sees through every plot.

A plot on an empty hex, 滅火 and 鎮靜 always work.

A unit in a state cannot move, retreat, supply, attack or use a plot; it
can only wait. A unit under 偽報 marches to its home city by itself, and
the state ends after its turns. 混亂 has no count of turns:
- At the start of each turn the unit recovers by a chance. The chance in
  percent is the highest 智力 of the unit.
- The chance is at least confusion.recover_floor (20) and at most
  confusion.recover_cap (90).
- A small unit can panic. It has panic.soldiers (500) soldiers or fewer,
  and an enemy unit next to it has panic.ratio (5) times its soldiers.
  Then it confuses itself with panic.chance (30 percent).
- The clean room assumes these numbers (Q-218).

沈著 and 明鏡 stop confusion; 規律 and 明鏡 stop 偽報. A camp of the faction
can end a state sooner, and 鎮靜 ends it in the same turn.

## Fire
The array `sim->fire` keeps the turns of fire of each cell. Each turn a
unit on a hex with fire loses fire.damage_percent of its soldiers (twice
with 藤甲), and a building loses fire.building_damage. A unit with 火神
takes no fire damage. A hex with fire costs more to enter. `fire_view`
gives the array to a front end.

## Buildings and traps
設置 needs a unit next to the hex, the technique, the gold that the unit
carries and a free hex. No building stands within 2 hexes of a base, and
no military building within 2 hexes of another. The work of one turn is
the build value x build.durability_per_build. A building works when its
durability is full. 修補 adds work. 撤除 takes 10 action points and some
technique points.

- A complete enemy military building has a zone of control.
- A tower shoots the nearest enemy unit in its range each turn.
- A drum tower and a stone maze add damage. A camp takes damage away.
- A music tower gives morale.
- A seed, a ball and a fire ship are traps: they have no owner and do not
  block a hex. A unit that touches one, or a fire, starts it.
- The natural traps (a dike near four cities, a rockfall near 漢中 and
  梓潼) stay after they start and recover. They are placeholders of
  Q-034.

## Sieges
A base has a durability. Its maximum is the static city constant of the
save, else siege.default_durability (G2-009, Q-222). The original shows
4600/4600 for 襄陽, where the field that the spec named
durability_maximum holds 7000. The wall never goes above the maximum. An
attack on a base takes soldiers and
durability. The walls answer a melee attack with combat.base_counter
soldiers. When the soldiers or the durability of a base end, the
attacker takes the base, its officers become captives and the unit
enters the base. A base with an enemy within 2 hexes loses soldiers each
turn and does not recover. A destroyed facility of a city gives loot to
the unit.

## The fate of a unit
A unit with no soldiers goes off the map. The victor captures each officer
with the chance combat.capture_percent, more with a halberd tactic and always with
捕縛. 強運 and 血路 always escape. An officer that escapes goes to a city
of the faction. The victor gains technique points. The victor also takes
half the gold and a quarter of the grain of the destroyed unit (G3-003,
observed). The log lines are `unit_loot` and `unit_plunder_grain`. The
capture chance is combat.capture_percent, 20 after the runs of report 3
(1 capture in 6 officers). When the player captures an officer, the
dialog 俘虜處置 opens for it (tutorial 19).

## The tactic column (G2-054)
戰法 of the unit menu opens the tactics of the class as a column next to
the menu, and the menu stays. A row shows the name and the cost in 氣力
in brackets. A tactic with no target in its pattern is grey:
ui_tactic_has_target probes the command on each hex within 3 of the unit
(ui_pick_allows). A choice closes the menu and the column, and starts
the pick. During a pick, the scene marks the hexes that the command can
hit in red, and the status bar names the pick. The ui test
"tactic_column_greys_a_tactic_without_a_target_and_marks_the_targets"
covers the grey row and the marks. In the recorder, only the hex of the
rival unit allows a target command.

## The merit and the experience of a battle
The run of 2026-10-22 settled the rewards of a battle (G2-048,
fixtures/battle-merit.tsv). `sim_reward_battle` gives them after an
attack, a joint attack and a tactic:
- Each officer of the attacker gains merit (rules.tsv merit.*). An
  attack gives 20, an arrow attack 10 and a tactic 50. A destroyed unit
  gives 320, or 350 when a tactic destroyed it.
- Each officer of the attacker gains force experience: 1 for an attack,
  3 for a tactic. A destroyed unit gives 3 leadership experience.
- Each officer of the target unit gains 1 leadership experience.

## Water
A foot or cavalry unit on water is a 走舸: it pays the navy cost and
fights with the values of the boat. A siege weapon stays on land
(assumed, Q-176). The ship classes have their own tactics. The ports of
the spec come with the map data (Q-034).

## engine_api
- `tactic_kind_count`, `tactic_kind_at`, `plot_kind_count`,
  `plot_kind_at`, `building_kind_count`, `building_kind_at`;
- `building_count`, `building_at`, `building_at_hex`, `fire_view` and
  `city_siege`;
- `command_attack`, `command_joint_attack`, `command_tactic`,
  `command_plot`, `command_build`, `command_repair` and
  `command_dismantle`. `can_issue` checks each of them;
- the editor commands `place_building`, `set_unit`, `set_unit_place` and
  `set_city_durability`. They ledger nothing.

## The panels
- The command menu enables 攻擊, 齊攻, 戰法, 計略, 設置, 修補 and 撤除 for a unit
  that did not act. 單挑 stays grey until M7.
- 攻擊, 齊攻, 修補 and 撤除 start a pick. 戰法, 計略 and 設置 open the
  Actions list first. A row of the list starts the pick of that row.
- During a pick, the Hex map outlines in red the hexes where
  `can_issue` passes. The next click on the map sends one command.
- The Hex map draws the buildings as triangles in the colour of the
  faction (grey for traps) and the fire as orange dots.
- The Unit panel shows the state of the unit and a button that cancels
  the pick.

## The feedback of war
code/ui/ui_war.cpp gives the player four signs of a battle. The user asked
for them on 2026-10-22. The sim stays instant. Only the front end plays
them, and it reads the facts through engine_api.
- The walk. A click in the range of a unit calls path_to before its
  command_move. ui_walk_plan keeps the hex of the unit and the cells of
  the plan. ui_walk_begin trims the plan to the hex that the unit reached.
  The unit then stands on each hex of the plan for UI_WALK_HEX_SECONDS.
  ui_round_unit_hex gives the hex of the walk to the scene, the plates and
  the Hex map. The command menu opens before the move, at the destination
  (G2-045, tutorial 17). The strike of an action after a move waits for
  the end of the walk. The far 移動 of the target menu walks the part of
  the way of this turn.
- The strike. After a 攻擊, 齊攻, 單挑, 戰法 or 計略 that the engine
  accepts, ui_combat_click pushes a strike on the target hex. The strike
  shows for UI_STRIKE_SECONDS: a ring that grows, and three slashes for
  an attack, rays for a tactic, or two rings for a plot. A
  SIM_EVENT_UNIT_DAMAGED of another faction pushes a strike on the hit
  unit, so the player sees the attacks of the AI too.
- The hit. ui_war_update reads the new events of the ledger. Each
  SIM_EVENT_UNIT_DAMAGED, UNIT_SOLDIERS_LOST, UNIT_DESERTED and
  UNIT_SUPPLIED is a hit of the soldiers. Each SIM_EVENT_MORALE_CHANGED is
  a hit of the morale. The value rises over the unit as "-495 兵" or
  "-10 氣力" for UI_HIT_FLOAT_SECONDS. Then ui_hit_mark gives the change to
  the plate and to the Unit panel. They show the value in red for a loss
  or in green for a gain, for UI_HIT_MARK_SECONDS. During the show of the AI
  round, ui_round_play_event raises the hits of the AI factions in the
  time of the show.
- The sprite. ui_unit_sprite maps the key of the unit class to a
  render_sprite. The scene draws that picture on the hex of the unit
  (docs/ASSETS.md, "Unit sprites"). Each class has its own sprite. The
  renderer takes the art of the class when the pack has it, else the
  placeholder picture (2026-10-24). A land unit on a river or on the
  sea is a 走舸, so it shows the boat and not its weapon. The water
  rule is the one of the sim: the terrain has a navy cost and no foot
  cost (ui_hex_is_water).

## The tests, line by line
- combat_tests.cpp (unit) checks the parsers of the four tables, the
  assumed damage rule and its hook, the class triangle and the hexes of
  each pattern.
- combat_tests.cpp (feature) checks these items through engine_api:
  - the four tables and the natural traps of the placeholder map;
  - 攻擊: the expected damage, the counterattack, the morale and each
    refusal;
  - the tower arrow: always a hit, the morale cost and the fire;
  - the fire of a turn, 齊攻 and the plot refusals, then 火計;
  - 設置 of a camp, the shot of an enemy tower, 修補 and 撤除;
  - the destruction of a unit and the fate of its officers;
  - the siege: the damage, the walls, the blockade and the fall;
  - a land unit that enters a river.
- ui_tests.cpp checks that each pick and each list sends one command,
  and that the map sends nothing when it draws.
- ui_tests.cpp, the feedback of war:
  "map_click_selects_a_unit_and_moves_it_with_one_command" checks the
  walk and the menu after it.
  "an_attack_shows_a_strike_on_the_target_hex_for_a_few_seconds" checks
  the strike and its end.
  "unit_hits_rise_from_the_ledger_and_then_mark_the_plate" checks the
  values and the mark.
  "unit_sprite_follows_the_class_of_the_unit" checks the sprite of each
  class.
- demo/m6_battle sends a unit out, attacks an enemy unit, sets a hex on
  fire and destroys the enemy in a later turn.

Run `run_tests.bat --e2e` and `run_tests.bat --demo`. Each prints
ALL PASS.
