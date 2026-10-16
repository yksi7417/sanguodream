/* Unit tests for code/sim/hex.h: coordinates, layout, the map, path search. */

internal void
unit_hex_tests(test_counts *counts)
{
    memory_arena arena = test_arena_create(Megabytes(4));

    hex_coord origin = hex_make(0, 0);
    TEST_CHECK(counts, hex_distance(origin, origin) == 0, "distance to self is 0");
    TEST_CHECK(counts, hex_distance(origin, hex_make(3, 0)) == 3, "distance along q");
    TEST_CHECK(counts, hex_distance(origin, hex_make(0, 3)) == 3, "distance along r");
    TEST_CHECK(counts, hex_distance(origin, hex_make(2, -2)) == 2, "distance along s");
    TEST_CHECK(counts, hex_distance(hex_make(-1, 2), hex_make(2, -1)) == 3, "distance negative coords");

    b32 ring_ok = true;
    hex_coord sum = origin;
    for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d)
    {
        hex_coord n = hex_neighbor(origin, d);
        if (hex_distance(origin, n) != 1) ring_ok = false;
        sum = hex_add(sum, HEX_DIRECTIONS[d]);
    }
    TEST_CHECK(counts, ring_ok, "all six neighbors are at distance 1");
    TEST_CHECK(counts, hex_equal(sum, origin), "directions sum to zero");

    b32 offset_round_trip = true;
    for (s32 row = -4; row <= 4; ++row)
    {
        for (s32 col = -4; col <= 4; ++col)
        {
            hex_offset o; o.col = col; o.row = row;
            hex_offset back = hex_to_offset(hex_from_offset(o));
            if (back.col != col || back.row != row) offset_round_trip = false;
        }
    }
    TEST_CHECK(counts, offset_round_trip, "offset <-> axial round trip, including negatives");

    b32 pixel_round_trip = true;
    hex_point porigin; porigin.x = 100.0f; porigin.y = 50.0f;
    for (s32 q = -5; q <= 5; ++q)
    {
        for (s32 r = -5; r <= 5; ++r)
        {
            hex_coord h = hex_make(q, r);
            hex_point p = hex_to_pixel(h, 9.0f, porigin);
            if (!hex_equal(hex_from_pixel(p, 9.0f, porigin), h)) pixel_round_trip = false;
        }
    }
    TEST_CHECK(counts, pixel_round_trip, "pixel <-> axial round trip at cell centers");

    hex_map map;
    hex_map_init(&map, &arena, 10, 8, 1);
    TEST_CHECK(counts, map.width == 10 && map.height == 8, "map dimensions");
    TEST_CHECK(counts, hex_map_contains(&map, hex_make(0, 0)), "origin is on the map");
    hex_offset last; last.col = 9; last.row = 7;
    TEST_CHECK(counts, hex_map_contains(&map, hex_from_offset(last)), "last cell is on the map");
    hex_offset beyond; beyond.col = 10; beyond.row = 7;
    TEST_CHECK(counts, !hex_map_contains(&map, hex_from_offset(beyond)), "cell past the width is off the map");
    TEST_CHECK(counts, !hex_map_contains(&map, hex_make(-1, 0)), "negative column is off the map");

    b32 index_round_trip = true;
    for (u32 i = 0; i < map.width * map.height; ++i)
    {
        if (hex_map_index(&map, hex_map_coord(&map, i)) != i) index_round_trip = false;
    }
    TEST_CHECK(counts, index_round_trip, "index <-> coord round trip over the whole map");

    TEST_CHECK(counts, hex_map_cost(&map, hex_make(2, 2)) == 1, "default cost");
    hex_map_set_cost(&map, hex_make(2, 2), 3);
    TEST_CHECK(counts, hex_map_cost(&map, hex_make(2, 2)) == 3, "set cost");
    TEST_CHECK(counts, hex_map_cost(&map, hex_make(-5, -5)) == HEX_IMPASSABLE, "off-map cost is impassable");
    hex_map_set_cost(&map, hex_make(-5, -5), 7);
    TEST_CHECK(counts, hex_map_cost(&map, hex_make(-5, -5)) == HEX_IMPASSABLE, "set off-map is ignored");

    /* Paths on an open map of cost 1 are as long as the hex distance. */
    hex_map open;
    hex_map_init(&open, &arena, 12, 12, 1);
    hex_coord a = hex_make(1, 1);
    hex_offset bo; bo.col = 9; bo.row = 8;
    hex_coord b = hex_from_offset(bo);
    memory_arena scratch = arena_sub(&arena, Megabytes(1));
    hex_path p = hex_find_path(&open, a, b, &scratch);
    TEST_CHECK(counts, p.count > 0 && p.total_cost == (u32)hex_distance(a, b), "open-map path cost equals distance");
    TEST_CHECK(counts, p.count == (u32)hex_distance(a, b) + 1, "path cell count is distance + 1");
    TEST_CHECK(counts, p.count > 0 && hex_equal(p.cells[0], a) && hex_equal(p.cells[p.count - 1], b), "path starts at from and ends at to");
    b32 steps_adjacent = true;
    for (u32 i = 1; i < p.count; ++i) if (hex_distance(p.cells[i - 1], p.cells[i]) != 1) steps_adjacent = false;
    TEST_CHECK(counts, steps_adjacent, "every path step moves to a neighbor");

    arena_reset(&scratch);
    hex_path self = hex_find_path(&open, a, a, &scratch);
    TEST_CHECK(counts, self.count == 1 && self.total_cost == 0, "path to self is the single cell at cost 0");

    /* A wall across the map forces a detour; a sealed map has no path. */
    for (u32 row = 0; row < 11; ++row)
    {
        hex_offset w; w.col = 5; w.row = (s32)row;
        hex_map_set_cost(&open, hex_from_offset(w), HEX_IMPASSABLE);
    }
    arena_reset(&scratch);
    hex_path detour = hex_find_path(&open, a, b, &scratch);
    TEST_CHECK(counts, detour.count > 0 && detour.total_cost > (u32)hex_distance(a, b), "wall forces a longer path");

    hex_offset wo; wo.col = 5; wo.row = 11;
    hex_map_set_cost(&open, hex_from_offset(wo), HEX_IMPASSABLE);
    arena_reset(&scratch);
    hex_path blocked = hex_find_path(&open, a, b, &scratch);
    TEST_CHECK(counts, blocked.count == 0 && blocked.total_cost == HEX_NO_PATH, "complete wall means no path");

    arena_reset(&scratch);
    hex_path into_wall = hex_find_path(&open, a, hex_from_offset(wo), &scratch);
    TEST_CHECK(counts, into_wall.count == 0, "impassable destination has no path");

    /* On a single row there is no way around an expensive cell. */
    hex_map costly;
    hex_map_init(&costly, &arena, 5, 1, 1);
    hex_offset mid; mid.col = 2; mid.row = 0;
    hex_map_set_cost(&costly, hex_from_offset(mid), 9);
    hex_offset lo; lo.col = 0; lo.row = 0;
    hex_offset hi; hi.col = 4; hi.row = 0;
    arena_reset(&scratch);
    hex_path straight = hex_find_path(&costly, hex_from_offset(lo), hex_from_offset(hi), &scratch);
    TEST_CHECK(counts, straight.total_cost == 1 + 9 + 1 + 1, "single-row map must pay the expensive cell");

    test_arena_destroy(&arena);
}
