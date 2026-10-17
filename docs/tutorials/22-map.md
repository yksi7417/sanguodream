# 22 Map: the real continent, its terrain and its sites

Files: code/sim/map.cpp, code/sim/hex.h, code/engine/engine_map.cpp,
code/ui/ui_map.cpp, code/ui/ui_scene.cpp, misc/make_map_pack.py. The data is
in data/placeholder/map/ (hexes.tsv and bases.tsv of the spec),
data/placeholder/terrain.tsv, movement_costs.tsv, unit_types.tsv and
rules.tsv. Tests: tests/unit/map_tests.cpp, tests/unit/hex_tests.cpp,
tests/feature/map_tests.cpp, the map handlers of
tests/feature/fixture_tests.cpp, the M10 tests in tests/ui/ui_tests.cpp, the
demo "demo/m10_map".

## What it is for
This tutorial covers milestone M10. M10 replaces the placeholder map with
the map of the real game. The specs are:
- formats/map-data.md: the grid, the 11 bytes of each hex, the regions,
  the bases and the slots;
- mechanics/terrain-and-map.md: the terrain rules, the movement costs,
  fire, the natural traps and the sites;
- the fixtures in spec/fixtures/map/.

The features are F-077 and F-147 to F-151 of FEATURES.md.

## The grid
The map has 200 columns and 200 rows. Column x runs west to east, row y
runs north to south, and an odd column sits half a hex south. hex.h stores
this grid in odd-q offset coordinates (tutorial 03). The cell of hex (x, y)
is y * 200 + x. The renderer and the Hex map panel draw flat-top hexes.

## The load
The engine reads the map in four steps (engine_create, engine_map.cpp):
1. It reads map\hexes.tsv of the pack into the transient arena.
   sim_map_measure_hexes gives the size of the grid, and sim_init_map makes
   a map of that size.
2. After the content loads, sim_map_load_hexes reads each row: the terrain
   id (the row of terrain.tsv), the region, the trap, the five flags and
   the height. The level of the renderer is the height / 16.
3. sim_map_load_bases reads map\bases.tsv: the hex, the kind and the city
   of each of the 87 bases.
4. sim_map_finish applies the rules of the load (F-151):
   - each side hex of a gate becomes cliff, so a unit passes a gate only
     through the gate hex;
   - each slot goes to the city of its region. A slot in the region of a
     port or a gate goes to the city of that base.

The loader reads each line of hexes.tsv where it is. A tsv_table of 40,000
rows needs approximately 5 MB of cell pointers, and the loader does not
need them. If a step fails, map_info says why, and the engine makes the
placeholder map. A pack without map\ (data/civil_war) also gets the
placeholder map.

The layers of the map are in sim_state.map_layers. They do not change
during a game, so a save does not hold them; a load keeps the layers of the
live sim.

## Cities, bases and slots
- sim_init_sites puts each city on its centre hex and gives it the slots of
  the map. sim_city_at returns the city for each of the 7 hexes of its
  flower.
- sim_init_bases puts each port and gate on its hex of the map.
- The mint and the granary give their bonus to the adjacent slots. On the
  real map these are the real neighbours: facility_at gives slot_neighbours,
  the column of slots.tsv.

## Movement (F-148)
movement_costs.tsv has one row for each of the 12 armaments and one column
for each terrain key. A unit type of unit_types.tsv names its armament on
land and its ship on 河 and 海. sim_terrain_cost picks the row. A pack
without the table uses the class costs of terrain.tsv. To enter a hex with a
fire costs fire.cost_multiplier (4) times the cost. A unit can enter a port
or a gate of its own faction and stops there, as in an own city (assumed).

## Fire (F-149)
A hex keeps a count of fire turns, 0 to 7. A fire from an attack lasts 1 to
3 turns, a fire from a plot or a trap 1 to 7 turns. A fire stays only on a
burnable terrain. On any other terrain it starts only under a unit, and it
stops when the hex is empty.

## Build permission (F-147)
sim_can_place reads "buildable" of terrain.tsv. A military facility, an
obstacle or a fire trap stands only on 草地, 土, 砂地 and 主徑. A fire ship
stands only on 河 and 海.

## Sites (F-150)
- A dike stands on each of the 4 dike hexes. When it breaks, it floods the
  flood hexes of its region (sim_map_in_flood).
- A rockfall stands on each of the 18 rockfall sites. A used rockfall
  leaves its site. Each turn, if the site is empty and does not burn, a new
  rockfall comes with a chance of rockfall.return_percent (Q-196 asks for
  the interval).
- When bandits start in a city, the engine picks one of up to 10 empty
  bandit sites of the city and ledgers SIM_EVENT_BANDIT_SITE.
- At the start of a game, the engine picks 6 event sites for the temples
  and 6 for the ruins. Q-193 asks for the kind of each group.

## Through the api
map_info gives the counts of the loaded map. hex_at gives the data of one
hex. armament_count, armament_at and move_cost give the cost table and the
cost for a unit type. bandit_sites gives the candidates of a city.
start_fire is a test command; it ledgers nothing.

## The UI
The Hex map panel draws the tile colour of each visible hex from
terrain_view, so it does not ask the cost of 40,000 hexes. A click shows the
data of the hex. A click on a city hex selects the city, and the panel and
the scene frame its slots in yellow. The mesh of the scene has 40,000
prisms; the frame arena of the platform is 160 MB for it.

## The tests of M0 to M9
The feature, ui and demo tests of M0 to M9 keep the placeholder map of 40 x
30 hexes as their board. Their fixtures set engine_config.use_pack_map to
false. The tests of M10 and the game window use the real map.

## Tests to read
- unit/map:
  - the counts of the spec: 40,000 hexes, 42 cities, 10 gates, 35 ports,
    294 city hexes, 591 slots;
  - the flowers, the gate cliffs, and the slots of slots.tsv with their
    neighbours;
  - the sites, the flood zones and the errors of the loader.
- feature/map:
  - the counts through the api, and the movement cost of each armament on
    real terrain;
  - fire, the build permission, the rockfall, the bandit sites and the
    event sites;
  - three AI turns and a save.
- fixture/map/*: each row of each map fixture against the loaded engine.

## Exercises
- Use the zone byte (byte 9) when Q-192 has an answer.
- Draw the water direction (byte 4) as arrows in the scene, for Q-197.
