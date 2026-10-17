# 03 Hex grid: coordinates, map, paths

File: code/sim/hex.h. Tests: tests/unit/hex_tests.cpp (28 checks), the map
checks in tests/feature/engine_tests.cpp, and the "map_panel" ui test.

## What it is for
This tutorial covers the hex grid. The world is a hex grid. This module is
three things:
- the geometry: where cells are and how far apart they are
- the storage: a rectangle of per-cell costs
- the search: the shortest path by cost

It knows nothing of terrain names, units or rules. Those are tables and
rules in later layers.

## Two coordinate systems
- Axial `(q, r)` is what all logic uses. Distance, neighbours and the round
  to a cell are simple in axial coordinates.
- Odd-q offset `(col, row)` is how the map stores cells. It is the grid of
  spec/formats/map-data.md (M10): column x runs west to east, row y runs
  north to south, and an odd column sits half a hex south of its
  neighbours. Thus `col = x`, `row = y`, `q = x` and
  `r = y - (x - (x mod 2)) / 2`. `hex_to_offset` and `hex_from_offset`
  convert between the two systems. The tests prove the round trip, with
  negative coordinates included. The `& 1` and `/ 2` tricks are the place
  where off-by-one bugs hide.
- The test "the neighbours of a hex are those of the table of map-data.md"
  compares the six neighbours with the table of the spec, for an even and
  an odd column. The spec numbers the directions north-west, north,
  north-east, south-west, south, south-east. `HEX_DIRECTIONS` keeps the
  order around the hex, because the ring walks need it.

## Layout
`hex_to_pixel` places a flat-top hex of circumradius `size`: the next column
is 1.5 cells to the east, and a row is sqrt(3) cells to the south.
`hex_from_pixel` inverts it and rounds in cube coordinates (`hex_round`). The
UI panel uses both: one to draw, one to turn a click into a cell. The
round-trip test over an 11x11 neighbourhood is what lets the UI trust clicks.

## The map
`hex_map` is a width, a height and one byte of entry cost per cell. Cost 0
is `HEX_IMPASSABLE`. A read off the map returns impassable. The map ignores
a write off the map. The tests cover the index and coordinate conversions
over every cell.

## Shortest path
`hex_find_path` is A* with the hex distance as the heuristic. The heuristic
is admissible because every entry costs at least 1. The search uses a
binary heap and three arrays. The caller provides a scratch arena, and the
search pushes all of them into it; the path it returns lives in that arena
too. No allocation, no recursion.

Tests to read:
- "open-map path cost equals distance" and "path cell count is distance + 1":
  on uniform cost the path is a straight line.
- "wall forces a longer path", "complete wall means no path",
  "impassable destination has no path": the three ways terrain changes the
  answer.
- "single-row map must pay the expensive cell": when there is no way past
  the cell, the search still finds the only path.

## Through the api (feature tests)
`map_size`, `cell_cost`, `path_cost` and the editor command `set_cell_cost`
are how everything above the engine sees the map. The feature test builds a
wall through the api and checks that there is no path. Then it opens a gate
and checks that the path is longer than the straight line. "map edits do not
touch turn or ledger" is there to keep editor commands out of the game's
history.

## Exercises
- Add `hex_ring(center, radius)` that fills an array with the cells at
  exactly that distance, and test that the count is 6 * radius.
- Add `hex_reachable(map, from, budget, scratch)` (Dijkstra flood within a
  movement budget) and a test with a wall. Movement range overlays in M5
  will use it.
