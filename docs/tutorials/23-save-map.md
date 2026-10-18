# 23 Save map: the state of a save on the real map, and how to find it

Files: code/sim/save_format.h, save_format.cpp, code/sim/save_map.cpp,
code/render/render.h (the camera, the grid mask), code/d3d11_renderer.h,
code/ui/ui_locate.cpp, ui_scene.cpp, ui_march.cpp (the mini map),
data/placeholder/save_types.tsv, misc/save_dump.cpp. Tests:
tests/unit/save_format_tests.cpp, tests/unit/render_tests.cpp,
tests/feature/save_map_tests.cpp, the session 1 handlers of
tests/feature/fixture_tests.cpp, the M11 tests in tests/ui/ui_tests.cpp,
the demo "demo/m11_save_map".

## What it is for
This tutorial covers milestone M11. After M10 the real map loaded, but a
save left it empty, and the player did not find the cities. M11 puts
the facilities, the bases and the units of a save on the map. It also
lets the player pick a city or a unit from a list, look at any hex and
move the view with the keys.

## The two arrays of the save
formats/save-file.md gives two arrays of the payload:
- The building array at 0x0008A3 has 16384 records of 18 bytes. Byte 1
  is the facility type, byte 2 the owner, bytes 3 and 4 the durability,
  bytes 14 to 17 the hex. Records 0 to 86 are the bases. Type 255 marks
  an empty record (observed; Q-212).
- The troop unit array at 0x086B71 has records of 98 bytes. A record
  gives the leader, two deputies and the troops. It gives the morale (+8),
  the gold (+9), the grain (+13) and the state (+17, 1 = 混亂). It gives
  the hex at +23 (x) and +25 (y). It gives the land armament (+36) and the
  ship class (+41), two rows of movement-costs.tsv. The array
  goes on past the table at 0x08803B: sample.s11 has units in the records
  54 to 56. The engine reads 388 records (assumed, Q-216). An officer in a
  unit has the position 87 + the unit index.

- The per-hex block is the last 160000 bytes of the payload: one u32 for
  each hex, at the index x * 200 + y. The low bits are the turns of fire
  left (1 to 7). The values 16 and 32 mark the two kinds of event site.

save_format reads the arrays and the block. `save_dump --buildings
--units --bases` prints the arrays.

## From a facility type to the pack
save_types.tsv has one row for each facility type of the save. The type
is a base, a facility of facilities.tsv with its level, a building of
buildings.tsv, a natural trap, a temple or a ruin. The types 50 to 59 are
the markets, farms, barracks, smithies and stables of level 2 and 3. A
pack without the table puts nothing on the map.

## The load (save_map.cpp)
sim_load_save_map runs at the end of sim_load_save, on the real map only:
1. Records 0 to 86 give the durability of each base. The owner of a city
   stays that of the city record. The owner and the soldiers of a gate or
   a port come from its own record at 0x08428D (tutorial 20).
2. Each other record puts its facility on the slot of its hex, or its
   building on its hex. A dike or a rockfall record gives its durability
   to the trap of the map.
3. The per-hex block gives the fire count of each hex and the 12 event
   sites. The event sites replace the random pick. The engine takes 16 as
   a temple and 32 as a ruin (assumed, Q-205).
4. Each unit that an officer rides in goes on the hex of its record. The
   unit id is the unit index, so the positions of the officers stay right.
   - The hex can be off the map, on a terrain that a unit cannot enter, or
     under another unit. Then the unit goes on the first free hex near the
     home base of its leader. The ledger counts these fallbacks.
   - The unit gets the morale, the gold, the grain and the 混亂 of its
     record. The save gives no turns of 混亂, so the engine gives 1 turn
     (assumed, Q-218). The unit eats its grain each turn.
   - sim_save_unit_type finds the unit type of the land armament and of the
     ship class. On 河 and 海 the unit moves and fights as its ship
     (sim_unit_cost_type, sim_effective_type), on land as its land class.
     The engine takes water from the terrain of the hex, not from +42,
     because +42 disagrees with the terrain for four units (Q-217).

The ledger gets one SIM_EVENT_SAVE_MAP_LOADED event with the counts: delta
is the facilities, buildings and traps; detail is the units; subject is the
fallbacks. In sample.s11 all 56 units stand on their hexes, 12 on water.

## Find a city or a unit
- ui_frame_city and ui_frame_unit select the city or the unit. The next
  frame, ui_follow_selection moves the camera target to its hex and sets
  the distance. A new selection from any other panel also frames it.
- The Hex map panel scrolls its canvas to the same hex.
- ui_draw_scene_labels projects each city, gate, port and unit to the
  screen with ui_scene_screen. ui_draw_plate draws a plate there on the
  background draw list: the name, the soldiers and a thin bar. The bar of
  a base is its durability; the bar of a unit is its morale.
- When the camera distance is less than UI_PLATE_SLOT_DISTANCE,
  ui_draw_slot_glyphs draws each slot of a city. A built slot shows the
  first character of its facility and its level. A free slot shows a ring.
- The mini map draws a white frame round the main view and a compass ring.
  camera_ground_axes turns the four labels 北, 東, 南 and 西 with the yaw.

## The status bar
ui/main-map.md gives a bar at the bottom centre of the map view. It shows
the hex under the mouse, with no click. ui_draw_status_bar calls hex_at and
terrain_kind_at for the hover hex and shows three fields:
- 地形: the terrain and the base that owns its region;
- 座標: x,y, the column and the row;
- 設置: what the player can place. A buildable hex with no city or base
  takes 軍事設施, 陣 and 陷阱. A water hex takes a fire ship.

## The grid
ui_scene.cpp fills a mask with one byte for each cell: 1 when a unit can
enter the terrain. render_terrain.grid points to it. The renderer draws
the grid outline only on the cells with 1, so cliffs and mountains show no
grid. With no mask, every cell gets the grid.

## The keys of the scene
camera_ground_axes gives the forward and the right directions on the
ground. The eye sits behind the target, so forward is (sin yaw, -cos yaw).
W and S pan along forward, A and D along right, at every yaw. A positive
angle to camera_turn turns the map counter-clockwise on the screen. Q and
Left give a negative angle, so the view turns clockwise; E and Right give a
positive angle. ui/main-map.md gives these keys as the keys of the original.
Up and Down call camera_tilt; the clamp keeps
the pitch between 30 and 75 degrees, and the yaw does not change. The
unit test checks each direction with camera_project at nine headings.

## The Hex info box
A click in the scene sets the pick. The box calls hex_at and shows:
- the terrain, the height and the region by name;
- the city, the base, the slot, the facility and its level;
- the building, the unit, the sites and the fire. The Hex map panel shows the same lines for its picked hex.

## The ui tests and the headless scene
The scene input, the labels and the frames run only with a render target,
so the older ui tests do not see their calls. The M11 ui tests set
ui->scene.headless to run them without a window.

## Tests to read
- feature/save_map: 柴桑 of sample.s11 and the counts of its city
  information screen; 53 units; 30 buildings; the bases.
- unit/render: "a pan forward (W) moves the view up the screen at every
  yaw" and the turn and tilt checks.
- ui: "scene_keys_move_turn_and_tilt_and_command_nothing",
  "status_bar_reads_the_hex_under_the_mouse_and_commands_nothing" and the
  frame tests.
