# 24 Main map: the round mini map, the unit plates and the step turn

Files: code/ui/ui_minimap.cpp, code/ui/ui_locate.cpp (the unit plates),
code/render/render.h (the step turn and the mini map axes),
code/engine/engine_march.cpp (grain_turns). Tests:
tests/unit/render_tests.cpp, tests/feature/march_tests.cpp,
tests/feature/save_map_tests.cpp, the M12 tests in tests/ui/ui_tests.cpp,
the demo "demo/m12_main_map".

## What it is for
This tutorial covers milestone M12. It closes three items of
ui/main-map.md that M11 left open:
- the round mini map;
- the full unit plates;
- the step turn of Q and E.

## The step turn
The camera keeps a camera_turn_step in ui->scene.turn. Each frame,
ui_scene_handle_keys gives camera_turn_update the key that is down:
- -1 for Q or Left (clockwise on the screen), +1 for E or Right, 0 for
  none.
- A new press starts a step. camera_step_target gives the next yaw on
  the grid of 60 degrees in that direction. A yaw off the grid (after a
  mouse orbit) goes to the next grid line.
- The animation turns one step in CAMERA_TURN_STEP_SECONDS (0.25 s). At
  the end, the yaw goes exactly onto the grid, so the hex grid lines up.
- A held key starts the next step after CAMERA_TURN_REPEAT_SECONDS
  (0.5 s). A press during the animation adds one more step.

The spec estimates 30 to 45 degrees for each press. The clean room
assumes the three values (Q-214).

## The round mini map
ui_minimap.cpp draws the panel. ui_minimap_fit puts the centre of the map
at the centre of the disc. It scales the map so that its half diagonal
fills the disc, so the whole continent shows at every yaw.

camera_ground_to_minimap turns a ground offset into a disc offset. The
forward direction of the camera is up, so the disc turns with the main
view. camera_minimap_to_ground is the inverse. The disc shows:
- the terrain colour of each hex inside the circle;
- the roads: a line between two neighbour 主徑 hexes, and from a road into
  a city, a gate or a port;
- each city as a square in the colour of its owner, with the diplomacy
  border of M7. The owner comes from the city record of the save;
- each gate and port as a hollow square, each unit as a diamond;
- a white frame: the ground under the four corners of the screen;
- the compass ring with 北, 東, 南 and 西 at their ground directions.

A click or a drag on the disc calls ui_minimap_move_camera. The camera
goes to the hex under the mouse. The distance, the yaw and the tilt stay.

## The unit plates
ui_draw_unit_plate draws a plate over each unit:
- The portrait. The clean room has no officer pictures. A square in the
  faction colour with the first character of the commander's name stands
  in for the portrait.
- The unit name and the status mark. ui_unit_mark_of gives 完 (grey) after
  the unit acted in this turn. It gives 委 (blue) when the home city of the
  unit is in a corps other than corps 0. Else it gives 未 (green).
- The supply icon 糧 and the soldiers. unit_at gives grain_turns: the
  turns that the grain lasts at one grain for each grain.soldiers
  soldiers. ui_grain_colour makes the icon red below 3 turns, yellow below
  10 and green from 10. The icon is grey when the save gives no grain
  (Q-211).

The clean room assumes what 委 and the colours of the supply icon show
(Q-215).

## Tests to read
- unit/render: "a press turns the view one step of 60 degrees clockwise",
  "six presses make a full turn", "a key held for 1.2 s turns three
  steps", and the mini map axes at nine headings.
- feature/march: unit_at gives the turns that the grain lasts.
- feature/save_map: a unit of the save has no known grain.
- ui: "minimap_click_moves_the_camera_and_commands_nothing" and
  "unit_plates_read_the_commander_and_the_corps_and_command_nothing".
