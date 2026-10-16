# 03 Hex grid: coordinates, map, paths

File: code/sim/hex.h. Tests: tests/unit/hex_tests.cpp (28 checks), the map
checks in tests/feature/engine_tests.cpp, and the "map_panel" ui test.

## What it is for
The world is a hex grid. This module is the geometry (where cells are and
how far apart), the storage (a rectangle of per-cell costs) and the search
(shortest path by cost). It knows nothing about terrain names, units or
rules; those are tables and rules in later layers.

## Two coordinate systems
- Axial `(q, r)` is what all logic uses. Distance, neighbours and rounding
  are simple in axial coordinates.
- Odd-r offset `(col, row)` is how the map is stored and how a scenario file
  will describe it: a rectangle where odd rows are shifted right by half a
  cell. `hex_to_offset` and `hex_from_offset` convert; the tests prove the
  round trip including negative coordinates (the `& 1` and `/ 2` tricks are
  where off-by-one bugs would hide).

## Layout
`hex_to_pixel` places a pointy-top hex of circumradius `size`. `hex_from_pixel`
inverts it with cube rounding (`hex_round`). The UI panel uses both: one to
draw, one to turn a click into a cell. The round-trip test over an 11x11
neighbourhood is what lets the UI trust clicks.

## The map
`hex_map` is width, height and one byte of entry cost per cell. Cost 0 is
`HEX_IMPASSABLE`. Reading off the map returns impassable; writing off the
map is ignored. Index and coordinate conversions are tested over every cell.

## Shortest path
`hex_find_path` is A* with hex distance as the heuristic, which is
admissible because every entry costs at least 1. It uses a binary heap and
three arrays, all pushed into a scratch arena the caller provides; the
returned path lives in that arena too. No allocation, no recursion.

Tests to read:
- "open-map path cost equals distance" and "path cell count is distance + 1":
  on uniform cost the path is a straight line.
- "wall forces a longer path", "complete wall means no path",
  "impassable destination has no path": the three ways terrain changes the
  answer.
- "single-row map must pay the expensive cell": when there is no way around,
  the search still finds the only path.

## Through the api (feature tests)
`map_size`, `cell_cost`, `path_cost` and the editor command `set_cell_cost`
are how everything above the engine sees the map. The feature test builds a
wall through the api, checks there is no path, opens a gate, and checks the
path is longer than the straight line. "map edits do not touch turn or
ledger" is there to keep editor commands out of the game's history.

## Exercises
- Add `hex_ring(center, radius)` that fills an array with the cells at
  exactly that distance, and test that the count is 6 * radius.
- Add `hex_reachable(map, from, budget, scratch)` (Dijkstra flood within a
  movement budget) and a test with a wall. Movement range overlays in M5
  will use it.
