# 18 Battle: attacks, tactics, plots, fire, buildings and sieges

Files: code/sim/combat.cpp, code/sim/field.cpp, code/sim/content.cpp
(tactics.tsv, plots.tsv, buildings.tsv and critical_skills.tsv),
code/engine/engine_combat.cpp, code/ui/ui_combat.cpp and ui_march.cpp.
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

齊攻 (joint attack) strikes with every unit of the faction that can attack
the hex. It needs two units (`no_partner`). The target answers none of
them (assumed).

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
A base has a durability: the maximum of the save, or
siege.default_durability. An attack on a base takes soldiers and
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
of the faction. The victor gains technique points.

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
- demo/m6_battle sends a unit out, attacks an enemy unit, sets a hex on
  fire and destroys the enemy in a later turn.

Run `run_tests.bat --e2e` and `run_tests.bat --demo`. Each prints
ALL PASS.
