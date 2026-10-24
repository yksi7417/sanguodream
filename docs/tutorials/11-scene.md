# 11 Scene: the 2.5D map, the camera and the pick

Files: code/render/render.h, code/render/terrain_mesh.h, code/render/png.h,
code/d3d11_renderer.h, code/ui/ui_scene.cpp. Tests: tests/unit/render_tests.cpp
(24 checks), terrain_mesh_tests.cpp (9), png_tests.cpp (11), the terrain
checks in tests/feature/engine_tests.cpp, and the Scene steps of the
m0_skeleton demo.

## What it is for
This tutorial covers the map as a 3D scene. The grid, the terrain, the
settlements and the units draw behind the panels. The camera orbits, tilts
and zooms. A click on the ground picks a hex. The game DLL never calls a
graphics API: it fills a command buffer, and the platform draws it.

## The line between the DLL and the platform
- `render_commands` (render.h) is the command buffer. The platform makes one
  in the frame arena each frame and gives it to the DLL in `game_input`.
  The DLL fills it: the camera, the terrain to draw, the overlay colour of
  each cell, model instances and billboards.
- `d3d11_renderer.h` reads the buffer and draws. Only `win32_window.h`
  includes it, so hexdom.dll stays free of D3D11 and hot reload keeps its
  state.

## Read it in this order
1. render.h, the math: `v3`, `m4` (row-major, a point is a row vector),
   look-at, perspective, inverse. The camera is a target, a yaw, a pitch and
   a distance. `camera_orbit`, `camera_zoom` and `camera_pan` clamp the
   pitch to 30 to 75 degrees and the distance to a range.
2. `camera_project` (world to pixel) and `camera_ray` (pixel to a world
   ray). `camera_pick_hex` meets the ray with the ground plane and converts
   the point with the hex layout of size 1. The unit test projects every cell
   of a map and picks it back.
3. terrain_mesh.h: one hex prism for each cell. The top is a flat hexagon at
   the cell's level, a bevel ring sits below it, and the sides go down to a
   floor. Each vertex carries its cell index and its material tile.
4. png.h: a PNG decoder on the engine's own inflate. The platform uses it
   for the material atlas and the sprites.
   The pick of the mouse (camera_pick_hex_levels) meets the top plane of
   each terrain level from the highest down. The first hex whose own level
   is that of the plane is the hex under the mouse. The ground plane alone
   picked the hex under the top of a raised column, one or two hexes off
   (the player, 2026-10-24).
5. d3d11_renderer.h: the HLSL is a string compiled at startup. The terrain
   draw samples the atlas by tile and the overlay texture by cell. Models
   and billboards are instanced: one draw for each mesh id or sprite id.
   When no file exists in data\placeholder, the renderer generates flat
   colour tiles, a house and a figure.
6. ui_scene.cpp: the Scene panel has buttons for orbit, tilt, zoom and reset,
   so the walkthroughs and the ui tests move the camera with clicks. The
   mouse works too: right drag orbits, middle drag pans, the wheel zooms,
   and a left click on the ground picks a cell. The picks share the
   selection with the 2D Hex map panel.

## The terrain data
`hex_map` now holds a kind and a level for each cell, and a version. The
engine fills them with a placeholder landscape from a hash of the cell
position, so the sim's random stream does not change. `engine_api` gives
the renderer a `terrain_view` and the tests a `set_cell_terrain` command.
The spec's map format replaces the placeholder in milestone M5.

## The tests, line by line
- render: "translation moves a point", "inverse undoes the transform",
  the four camera clamps, "target projects to the screen center", "project
  then pick returns the same hex for every cell", "pan right at yaw 90
  degrees moves along +Z", and the command buffer's capacity.
- terrain_mesh: counts per cell, "top center of a cell matches
  hex_world_center", "side normals are horizontal and outward".
- png: a hand-built 2x2 image with each filter, and three refusals.
- feature: "terrain_view exposes the cells and a version that moved", and
  the path tests flatten the map through the api first.
- demo m0: the Scene panel orbits three times, zooms twice, tilts, then
  resets; the checks compare the camera before and after.

## Exercises
- Draw the path between the two picked cells as overlay colours: add a
  query to engine_api that returns the path cells, then mark them.
- Replace the placeholder figure with a billboard that faces the camera's
  up vector too, for a view from above.
