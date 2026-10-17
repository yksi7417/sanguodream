#ifndef HEX_H
#define HEX_H
/* Hex grid mathematics and the map that stores it.

   Coordinates are axial (q, r) on a flat-top grid; storage is a rectangle
   in odd-q offset coordinates (col, row). This is the grid of
   formats/map-data.md: column x runs west to east, row y runs north to
   south, and an odd column sits half a hex south of the even columns
   beside it. Thus col = x, row = y, q = x and r = y - (x - (x mod 2)) / 2.
   Everything here is generic hex geometry and graph search, not game
   rules: terrain kinds, movement rules and zones of control come from the
   spec and live in sim.cpp. */

#include "base.h"

struct hex_coord
{
    s32 q;
    s32 r;
};

struct hex_offset
{
    s32 col;
    s32 row;
};

struct hex_point
{
    f32 x;
    f32 y;
};

#define HEX_DIRECTION_COUNT 6
#define HEX_IMPASSABLE      0      /* a cell cost of zero cannot be entered */
#define HEX_NO_PATH         U32_MAX

global_variable const hex_coord HEX_DIRECTIONS[HEX_DIRECTION_COUNT] = {
    { 1, 0 }, { 1, -1 }, { 0, -1 }, { -1, 0 }, { -1, 1 }, { 0, 1 }
};

internal hex_coord hex_make(s32 q, s32 r) { hex_coord h; h.q = q; h.r = r; return h; }
internal b32       hex_equal(hex_coord a, hex_coord b) { return a.q == b.q && a.r == b.r; }
internal hex_coord hex_add(hex_coord a, hex_coord b) { return hex_make(a.q + b.q, a.r + b.r); }
internal hex_coord hex_neighbor(hex_coord h, u32 direction)
{
    Assert(direction < HEX_DIRECTION_COUNT);
    return hex_add(h, HEX_DIRECTIONS[direction]);
}

internal s32
hex_abs(s32 v)
{
    return v < 0 ? -v : v;
}

internal s32
hex_distance(hex_coord a, hex_coord b)
{
    s32 dq = a.q - b.q;
    s32 dr = a.r - b.r;
    return (hex_abs(dq) + hex_abs(dr) + hex_abs(dq + dr)) / 2;
}

/* odd-q offset: odd columns are shoved down (south) by half a cell. */
internal hex_offset
hex_to_offset(hex_coord h)
{
    hex_offset o;
    o.col = h.q;
    o.row = h.r + (h.q - (h.q & 1)) / 2;
    return o;
}

internal hex_coord
hex_from_offset(hex_offset o)
{
    return hex_make(o.col, o.row - (o.col - (o.col & 1)) / 2);
}

/* Flat-top layout: size is the circumradius, origin the pixel of hex (0,0).
   x grows to the east and y to the south. */
internal hex_point
hex_to_pixel(hex_coord h, f32 size, hex_point origin)
{
    hex_point p;
    p.x = origin.x + size * (1.5f * (f32)h.q);
    p.y = origin.y + size * (0.8660254f * (f32)h.q + 1.7320508f * (f32)h.r);
    return p;
}

internal hex_coord
hex_round(f32 fq, f32 fr)
{
    f32 fs = -fq - fr;
    s32 q = (s32)(fq + (fq < 0 ? -0.5f : 0.5f));
    s32 r = (s32)(fr + (fr < 0 ? -0.5f : 0.5f));
    s32 s = (s32)(fs + (fs < 0 ? -0.5f : 0.5f));
    f32 dq = (f32)q - fq, dr = (f32)r - fr, ds = (f32)s - fs;
    if (dq < 0) dq = -dq;
    if (dr < 0) dr = -dr;
    if (ds < 0) ds = -ds;
    if (dq > dr && dq > ds) q = -r - s;
    else if (dr > ds)       r = -q - s;
    return hex_make(q, r);
}

internal hex_coord
hex_from_pixel(hex_point p, f32 size, hex_point origin)
{
    f32 x = (p.x - origin.x) / size;
    f32 y = (p.y - origin.y) / size;
    f32 fq = 0.6666667f * x;
    f32 fr = (-0.3333333f * x) + (0.5773503f * y);
    return hex_round(fq, fr);
}

/* ---- the map ------------------------------------------------------------ */

struct hex_map
{
    u32 width;      /* columns */
    u32 height;     /* rows */
    u8 *cost;       /* move cost to enter each cell, row-major by offset (row * width + col); 0 = impassable */
    u8 *kind;       /* terrain kind per cell: an index into the pack's terrain table */
    u8 *level;      /* height per cell in steps; the renderer scales it */
    u32 version;    /* bumped by every change to kind or level; the renderer rebuilds its mesh when it changes */
};

internal void
hex_map_init(hex_map *map, memory_arena *arena, u32 width, u32 height, u8 default_cost)
{
    map->width   = width;
    map->height  = height;
    map->cost    = PushArray(arena, (u64)width * height, u8);
    map->kind    = PushArray(arena, (u64)width * height, u8);
    map->level   = PushArray(arena, (u64)width * height, u8);
    map->version = 1;
    for (u32 i = 0; i < width * height; ++i) map->cost[i] = default_cost;
}

internal b32
hex_map_contains(hex_map *map, hex_coord h)
{
    hex_offset o = hex_to_offset(h);
    return o.col >= 0 && o.row >= 0 && (u32)o.col < map->width && (u32)o.row < map->height;
}

internal u32
hex_map_index(hex_map *map, hex_coord h)
{
    hex_offset o = hex_to_offset(h);
    Assert(hex_map_contains(map, h));
    return (u32)o.row * map->width + (u32)o.col;
}

internal hex_coord
hex_map_coord(hex_map *map, u32 index)
{
    hex_offset o;
    o.col = (s32)(index % map->width);
    o.row = (s32)(index / map->width);
    return hex_from_offset(o);
}

internal u8
hex_map_cost(hex_map *map, hex_coord h)
{
    return hex_map_contains(map, h) ? map->cost[hex_map_index(map, h)] : (u8)HEX_IMPASSABLE;
}

internal void
hex_map_set_cost(hex_map *map, hex_coord h, u8 cost)
{
    if (hex_map_contains(map, h)) map->cost[hex_map_index(map, h)] = cost;
}

internal void
hex_map_set_terrain(hex_map *map, hex_coord h, u8 kind, u8 level)
{
    if (!hex_map_contains(map, h)) return;
    u32 index = hex_map_index(map, h);
    map->kind[index]  = kind;
    map->level[index] = level;
    map->version++;
}

/* ---- shortest path ------------------------------------------------------ */

struct hex_path
{
    hex_coord *cells;        /* from (inclusive) to to (inclusive), in order */
    u32        count;        /* 0 when no path exists */
    u32        total_cost;   /* sum of entry costs of every cell after from; HEX_NO_PATH if none */
};

struct hex_heap_entry
{
    u32 cost;
    u32 index;
};

struct hex_heap
{
    hex_heap_entry *entries;
    u32             count;
};

internal void
hex_heap_push(hex_heap *heap, u32 cost, u32 index)
{
    u32 i = heap->count++;
    heap->entries[i].cost  = cost;
    heap->entries[i].index = index;
    while (i > 0)
    {
        u32 parent = (i - 1) / 2;
        if (heap->entries[parent].cost <= heap->entries[i].cost) break;
        hex_heap_entry t = heap->entries[parent];
        heap->entries[parent] = heap->entries[i];
        heap->entries[i] = t;
        i = parent;
    }
}

internal hex_heap_entry
hex_heap_pop(hex_heap *heap)
{
    hex_heap_entry top = heap->entries[0];
    heap->entries[0] = heap->entries[--heap->count];
    u32 i = 0;
    for (;;)
    {
        u32 left = 2 * i + 1, right = left + 1, smallest = i;
        if (left < heap->count && heap->entries[left].cost < heap->entries[smallest].cost) smallest = left;
        if (right < heap->count && heap->entries[right].cost < heap->entries[smallest].cost) smallest = right;
        if (smallest == i) break;
        hex_heap_entry t = heap->entries[smallest];
        heap->entries[smallest] = heap->entries[i];
        heap->entries[i] = t;
        i = smallest;
    }
    return top;
}

/* A* with the hex distance as heuristic (admissible because every entry
   costs at least 1). All working memory comes from scratch; the returned
   path's cells live there too, so copy them out before the arena is reset. */
internal hex_path
hex_find_path(hex_map *map, hex_coord from, hex_coord to, memory_arena *scratch)
{
    hex_path result = {};
    result.total_cost = HEX_NO_PATH;
    if (!hex_map_contains(map, from) || !hex_map_contains(map, to)) return result;
    if (hex_map_cost(map, to) == HEX_IMPASSABLE) return result;

    u32 cell_count = map->width * map->height;
    u32 *best      = PushArray(scratch, cell_count, u32);
    u32 *came_from = PushArray(scratch, cell_count, u32);
    u8  *closed    = PushArray(scratch, cell_count, u8);
    for (u32 i = 0; i < cell_count; ++i) { best[i] = U32_MAX; came_from[i] = U32_MAX; }

    hex_heap open;
    open.entries = PushArray(scratch, cell_count * HEX_DIRECTION_COUNT + 1, hex_heap_entry);
    open.count   = 0;

    u32 start = hex_map_index(map, from);
    u32 goal  = hex_map_index(map, to);
    best[start] = 0;
    hex_heap_push(&open, (u32)hex_distance(from, to), start);

    while (open.count)
    {
        hex_heap_entry current = hex_heap_pop(&open);
        if (closed[current.index]) continue;
        closed[current.index] = 1;
        if (current.index == goal) break;

        hex_coord here = hex_map_coord(map, current.index);
        for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d)
        {
            hex_coord next = hex_neighbor(here, d);
            if (!hex_map_contains(map, next)) continue;
            u8 cost = hex_map_cost(map, next);
            if (cost == HEX_IMPASSABLE) continue;
            u32 next_index = hex_map_index(map, next);
            u32 candidate  = best[current.index] + cost;
            if (candidate < best[next_index])
            {
                best[next_index]      = candidate;
                came_from[next_index] = current.index;
                hex_heap_push(&open, candidate + (u32)hex_distance(next, to), next_index);
            }
        }
    }

    if (best[goal] == U32_MAX) return result;

    u32 length = 1;
    for (u32 i = goal; i != start; i = came_from[i]) length++;
    result.cells = PushArray(scratch, length, hex_coord);
    result.count = length;
    result.total_cost = best[goal];
    u32 write = length;
    for (u32 i = goal;; i = came_from[i])
    {
        result.cells[--write] = hex_map_coord(map, i);
        if (i == start) break;
    }
    return result;
}

#endif /* HEX_H */
