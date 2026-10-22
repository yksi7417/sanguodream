# 17 March: terrain, units, the move range and the unit commands

Files: code/sim/units.cpp, code/sim/content.cpp (terrain.tsv and
unit_types.tsv), code/engine/engine_march.cpp (the api and the
placeholder map), code/ui/ui_march.cpp, code/ui/ui_map.cpp and
ui_scene.cpp. The data is in data/placeholder/terrain.tsv,
unit_types.tsv and rules.tsv.
Tests: tests/unit/march_tests.cpp, tests/feature/march_tests.cpp, the M5
tests in tests/ui/ui_tests.cpp, the fixture unit-values-guidebook.tsv, the
demo "demo/m5_march".

## What it is for
This tutorial covers milestone M5: units leave a city, march over the
map and come back. The specs are:
- mechanics/terrain-and-map.md: the terrain kinds, the roads, what each
  kind allows, the map display;
- mechanics/military-units-and-tactics.md: the unit classes, the grades,
  the unit values and the zone of control;
- mechanics/city-commands.md: 出征 (sortie) and 輸送 (transport);
- ui/unit-commands.md: the selection, the move range and the command
  menu.

The features are these rows of FEATURES.md:
- F-072 (movement costs, 難所行軍, plank road, poison spring) and F-073
  (the zone of control);
- F-074 (sortie), F-075 (transport and supply) and F-076 (units on the
  map and the move range);
- F-078 (the mini map and the faction colours);
- F-080 (the unit classes) and F-081 (the unit values).

## The terrain table
terrain.tsv has one row for each kind of hex. A row has the movement cost
for each movement class: foot, cavalry, siege and navy. A cost of 0 means
that the class cannot enter. A row also has the technique that a unit
needs to enter, the soldiers that the hex takes, and what stops that
loss. The last columns are the classes whose tactics work there, the
colour of the placeholder atlas and the height.

The manual documents the kinds and their rules. It gives no cost: every
cost is an assumption of Q-167. These rules come from
the spec:
- A small path and shallows need 難所行軍.
- A plank road takes soldiers unless the faction has 難所行軍 or an officer
  has 踏破 (skills.md #7).
- A poison spring takes soldiers unless an officer has 解毒 (#9).
- Nothing enters a cliff. Only ships enter a river or the sea. The
  water battle comes in M6, so a land unit cannot enter water now.

A rule names a skill by its number in skills.md. The save stores the
number minus 1 (assumed, Q-159). `sim_officer_has_skill` does the
conversion.

## The placeholder map
Until M10 the spec had no map data (Q-034). M10 loads the real map
(tutorial 22); a pack without map\ and the tests of M0 to M9 still get
this placeholder. engine_march.cpp makes the placeholder from terrain.tsv. A hash of each cell picks a kind. The city hexes, their
slots and the hexes next to them are grass. Main roads link the cities
that are neighbours on the grid of the placeholder layout. A road over
water is a ford. Three roads of the west have the special kinds of the
manual:
- a plank road from 漢中 to 梓潼;
- a small path from 成都 to 建寧;
- a poison spring between 建寧 and 雲南.

The map keeps the kind of each cell. `terrain_view` gives the kind and a
tile array: the colour of the atlas from the table. The scene draws the
tile. `set_cell_terrain` also sets the cost of the cell to the foot cost
of the kind.

## The unit table and the unit values
unit_types.tsv has one row for each class. A row has the weapon that the
unit carries, the aptitude that counts and the movement class. It also
has the movement, the grades of attack, defence and destroy, and the
range. The guide book shows the movement: 22 for the foot classes, 28 for
cavalry and 18 for the siege weapons. The fixture
unit-values-guidebook.tsv checks that column against the table. The
movement of the ships and of the transport unit is an assumption of
Q-168.

The spec names the inputs of the five values but gives no formula. The
engine uses an assumed formula (Q-170):
- The commander's ability is the base. If the best deputy is higher, the
  unit gains half of the difference, or all of it for a couple or sworn
  brothers.
- Attack = force × the best aptitude × the attack grade. Defence =
  leadership × aptitude × the defence grade.
- Intellect is the best intellect of the officers. Build is politics by
  the deputy rule.
- Movement is the class movement plus 4 for each technique or skill that
  adds movement (assumed, Q-169).

`sim_unit_values_of` computes them. The engine gives them in each
`engine_unit` and through `unit_values(force, class, officers)` for the
formation dialog. The fixture prints the other columns as open items until
Q-030 gives the officers of each scenario.

## Units
A unit lives in `sim_state.units`. Its id is its place in the array, and
the id stays while the unit lives. An officer in a unit has the position
87 + the id (save-file.md). A unit has its officers, soldiers, gold,
grain and morale. It also has the weapons of its class, the cargo of a
transport unit, its home city and two turn marks: moved and acted. `place_unit` makes a
test unit without a class: it only blocks a hex and has a zone of
control.

## 出征 and 輸送
出征 takes up to three officers (the first is the commander), a class, soldiers,
gold and grain. It costs 10 action points; a military office halves
them. The checks come in this sequence:
1. the content, the end of the game, the city and the turn;
2. the class (`no_unit`), and the destination of a transport unit
   (`no_city`);
3. the officers;
4. the soldiers (`no_soldiers`): more than 0, not above the city, and not
   above the commander's cap;
5. the weapons (`no_weapons`): one weapon for each soldier, or one siege
   weapon or ship;
6. the grain, the gold, a free hex next to the city (`no_room`) and the
   action points.

The cap is an assumption until the posts of M7: 5000 soldiers, plus
改革軍制 (Q-169). The unit stands on the first free hex next to the city,
with the morale of the city. The city ledger has `unit_sortied`,
`unit_weapons_taken`, `unit_supplies` and `unit_grain_taken`. Their sum
is the change of the city.

輸送 is the same command with the transport row of the table, one
officer, a destination city of the faction and a cargo of weapons.
`sortie_proposal` is 自動編制: the officers with the best aptitude, then
force plus leadership, then merit.

## The move range and the zone of control
`sim_unit_reach` is a search of the lowest cost from the hex of the
unit, within its movement. A hex costs the entry cost of its kind for the
movement class of the unit. A unit cannot enter these hexes:
- a hex with another unit;
- a city of another faction;
- a kind that the class cannot enter, or that needs a technique that the
  faction lacks.

Two kinds of hexes end the move there:
- a hex next to an enemy unit, that is the zone of control of manual
  page 14;
- an own city.

飛將 and 遁走 remove the zone of control on land, but not for a siege
weapon. Every other faction
is an enemy until diplomacy (M7). The military facilities come in M6.

## The orders of a unit
- 移動: the hex must be in the range. The unit walks the path. Each hex
  can take soldiers (10 percent of a plank road or a poison spring,
  assumed). A unit moves one time in each turn. If the move ends in an
  own city, the unit enters it. Its soldiers, weapons, cargo, gold and
  grain go into the city, and its officers stay there.
- 待命: the unit ends its turn.
- 退卻: the unit returns to its home city in the same turn (assumed,
  Q-171).
- 補給: a transport unit gives soldiers, gold or grain to an adjacent unit
  of the faction.

Each turn a unit eats one grain for 100 soldiers, as a city does. Without
grain its soldiers leave (`unit_deserted`).

## The long orders (code/sim/orders.cpp)
The second clone gap report (G2-042, G2-052, G2-056, G2-058) added the
long orders of city-commands.md. A unit keeps an order and does one step
at each 進行 of its faction until the order ends. The item 委任部隊行動
of the map menu runs the steps now (manual p.24). The plate of a unit
with an order shows 委.
- 移動 to a hex out of the range: `command_move` to that hex gives the
  order. The unit goes to the end of its range in the same turn and
  moves on at each 進行. The order ends at the hex.
- 殲滅 (`command_destroy`): the unit marches on an enemy unit and attacks
  it with the normal attack of its class when the target is in range. The
  order ends when the target is gone.
- 驅趕 (`command_drive`): as 殲滅, but the order ends after one attack
  (assumed, Q-227).
- The follow order (`command_follow`): the unit stays next to an own
  unit.
The path is a search without a cap over the terrain (`sim_unit_plan`).
Own cities and bases are waypoints, and the units of today do not block
the plan. A step moves the unit to the hex of the path that is farthest
along it and in the reach of this turn. `path_to` gives the plan to a
front end. The plan has the hexes, the cost, the days of 10 for each
turn, the days of grain and the bases on the way. The target menu of the
map shows them as 預定抵達時間, 所有兵糧 and 通過據點.

In the game mode, with an own unit under selection, a left click on an
enemy unit opens 殲滅 and 驅趕. A left click on an own unit asks for the
follow order. A left click on a far hex opens 移動. A land unit enters a
river hex as a 走舸 (the boat row of movement_costs.tsv). A click on the
plate of a unit is a click on its hex. After 決定 of 出征 the dialog
selects the new unit, so that its range shows on the map.

## engine_api
- `terrain_kind_count`, `terrain_kind_at`, `unit_type_count`,
  `unit_type_at`;
- `unit_count`, `unit_at(id)`, `unit_at_hex(q, r)`;
- `reachable(unit)`: the hexes of the range with their costs; a unit that
  moved or acted has only its own hex;
- `unit_values` and `sortie_proposal`;
- `command_sortie`, `command_transport`, `command_move`, `command_wait`,
  `command_retreat` and `command_supply`. `can_issue` checks each of them.
- `path_to`, `command_destroy`, `command_drive`, `command_follow` and
  `command_delegate`: the long orders (see above).

## The panels
- The City panel has the buttons 出征 and 輸送. The sortie dialog has the
  class, 自動編制, the officer slots, the soldiers, the gold, the grain, the
  five values and the action points. The transport dialog has the
  destination and a cargo of four weapons.
- The Hex map panel draws the cities as squares and the units as
  diamonds in the colour of their faction. A click on a unit of the
  faction that acts selects it and draws its range in green. A click in
  the range sends one `command_move` and opens the command menu. A click
  on the hex of the unit opens the menu.
- The command menu has the items of ui/unit-commands.md in their order.
  待命 and 退卻 run now. 補給 opens the Unit panel of a transport unit. The
  combat items are grey until M6.
- The Unit panel shows the selected unit: the officers, the stocks, the
  class with the aptitude letter and the five values. A transport unit
  has the 補給 section.
- The mini map shows the whole map, the cities as large squares and the
  units as diamonds. The scene draws a model on each city and a
  billboard on each unit in the colour of its faction.

## The tests, line by line
- march_tests.cpp (unit) checks the parsers of the two tables, the value
  formula on built officers, and the move range of a small world. It
  also checks the zone of control and the skill 飛將.
- march_tests.cpp (feature) checks these items through engine_api:
  - the two tables and the placeholder map: cities, roads, the plank
    road;
  - 出征: the proposal, each refusal, the unit, the stocks and the ledger;
  - the move range on a corridor of grass, the zone of control, the move,
    待命, the grain of a turn and 退卻;
  - a small path with and without 難所行軍, and the losses of a plank road
    and a poison spring;
  - 輸送, 補給, and a unit that enters an own city.
- order_tests.cpp (feature) checks the long orders. It checks the plan
  and its days, a far 移動 and its steps at 進行, and 殲滅 until the
  target is gone. It also checks the follow order with 委任部隊行動, and
  a river hex in the range.
- ui_tests.cpp checks that the dialogs, the map clicks, the menu and the
  Unit panel each send one command. The target menu sends one command
  for 殲滅, the follow order and a far 移動. The panels that only show
  data send nothing.
- demo/m5_march sends a unit out, moves it twice with 待命, and shows the
  mini map.

Run `run_tests.bat --e2e` and `run_tests.bat --demo`. Each prints
ALL PASS.
