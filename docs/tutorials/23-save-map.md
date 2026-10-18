# 23 Save map: the state of a save on the real map, and how to find it

Files: code/sim/save_format.h, save_format.cpp, code/sim/save_map.cpp,
code/render/render.h (the camera), code/ui/ui_locate.cpp, ui_scene.cpp,
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
- The troop unit array at 0x086B71 has records of 98 bytes: the leader,
  two deputies and the troops. The array has 54 records before the next
  table. An officer in a unit has the position 87 + the unit index.

save_format reads both arrays. `save_dump --buildings --units` prints
them.

## From a facility type to the pack
save_types.tsv has one row for each facility type of the save. The type
is a base, a facility of facilities.tsv with its level, a building of
buildings.tsv, a natural trap, a temple or a ruin. The types 50 to 59 are
the markets, farms, barracks, smithies and stables of level 2 and 3. A
pack without the table puts nothing on the map.

## The load (save_map.cpp)
sim_load_save_map runs at the end of sim_load_save, on the real map only:
1. Records 0 to 86 give the durability of each base and the owner of each
   gate and port. The owner of a city stays that of the city record.
2. Each other record puts its facility on the slot of its hex, or its
   building on its hex. A dike or a rockfall record gives its durability
   to the trap of the map. Temples and ruins of the save replace the random pick.
3. Each unit that an officer rides in goes on the map. The unit id is the
   unit index, so the positions of the officers stay right. The save
   gives no hex: the unit goes on the first free hex near the home base
   of its leader (assumed, Q-211). It has no class, and it does not eat.

The ledger gets one SIM_EVENT_SAVE_MAP_LOADED event with the counts.

## Find a city or a unit
- ui_frame_city and ui_frame_unit select the city or the unit. The next
  frame, ui_follow_selection moves the camera target to its hex and sets
  the distance. A new selection from any other panel also frames it.
- The Hex map panel scrolls its canvas to the same hex.
- ui_draw_scene_labels projects the centre of each city to the screen and
  draws a marker and the name on the background draw list.

## The keys of the scene
camera_ground_axes gives the forward and the right directions on the
ground. The eye sits behind the target, so forward is (sin yaw, -cos yaw).
W and S pan along forward, A and D along right, at every yaw. Q and
Left call camera_turn with a positive angle: the map turns counter-
clockwise on the screen. Up and Down call camera_tilt; the clamp keeps
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
- ui: "scene_keys_move_turn_and_tilt_and_command_nothing" and the frame
  tests.
