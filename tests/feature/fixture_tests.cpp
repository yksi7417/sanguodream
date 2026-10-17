/* Acceptance tests from the spec: every spec-src\spec\fixtures\*.tsv is a
   feature test for one mechanic, run through engine_api against the real
   engine. A fixture whose mechanic has no handler here FAILS: that is the RED
   step of the TDD loop and the signal to implement it. */

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>   /* directory listing only */

/* The table parser is code/sim/tables.h, the same one that reads the
   content pack. This file only reads the file. */

internal char *
read_text_file(const char *path, memory_arena *arena, u64 *size_out)
{
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = (char *)arena_push_size(arena, (u64)size + 1);
    size_t read = fread(text, 1, (size_t)size, file);
    fclose(file);
    text[read] = 0;
    if (size_out) *size_out = (u64)read;
    return text;
}

internal b32
tsv_load(const char *path, memory_arena *arena, tsv_table *table)
{
    char *text = read_text_file(path, arena, 0);
    return text && tsv_parse(text, arena, table);
}

/* A handler builds the state each row describes through engine_api, runs the
   row's command, compares with the row's expected outcome, and records one
   pass or fail per row. */
typedef void fixture_handler_fn(tsv_table *table, engine_api *api, test_counts *counts);

struct fixture_handler
{
    const char         *mechanic;   /* fixture file name without .tsv */
    fixture_handler_fn *run;
};

/* One entry per mechanic in spec-src/spec/mechanics/. Add the handler in the
   RED step, before implementing the mechanic in code/sim/ and code/engine/. */
/* unit-values-guidebook.tsv: the unit values of 18 screenshots of the
   guide book. The handler checks the movement column against the class
   row of unit_types.tsv in the engine (before techniques and skills: the
   screenshots show none that change the movement). The attack, defence,
   intellect and build columns need the officer data of each scenario
   (Q-030), and the engine's formula is assumed (Q-170). Until Q-030 the
   handler prints those columns as pending. They are not failures. */
internal void
fixture_unit_values_guidebook(tsv_table *table, engine_api *api, test_counts *counts)
{
    for (u32 row = 0; row < table->row_count; ++row)
    {
        const char *unit_class = tsv_cell(table, row, "class");
        u32 movement = 0, expected = 0;
        for (u32 index = 0; index < api->unit_type_count(api->engine); ++index)
        {
            engine_unit_type type = api->unit_type_at(api->engine, index);
            if (strcmp(type.key, unit_class) == 0) expected = type.move;
        }
        b32 parsed = tsv_cell_u32(table, row, "move", &movement);
        char name[128];
        snprintf(name, sizeof(name), "unit-values-guidebook row %u (%s): movement of the class in unit_types.tsv", row, unit_class);
        TEST_CHECK(counts, parsed && expected != 0 && movement == expected, name);
    }
    printf("  PENDING %u rows x attack, defence, intellect, build: Q-030 (the officers of each scenario) and Q-170 (the formula)\n",
           table->row_count);
}

/* ---- M10: spec-src\spec\fixtures\map\*.tsv (formats/map-data.md) ----------
   These handlers run against an engine with the real map of the pack and
   the sample save. Each one compares the map that the engine loaded with
   the fixture, so they also prove that data\placeholder\map is the spec. */

#define FIXTURE_MAP_GATE  18   /* the terrain ids of terrain-types.tsv */
#define FIXTURE_MAP_CLIFF 15

internal hex_coord
fixture_map_hex(tsv_table *table, u32 row)
{
    u32 x = 0, y = 0;
    tsv_cell_u32(table, row, "x", &x);
    tsv_cell_u32(table, row, "y", &y);
    hex_offset offset;
    offset.col = (s32)x;
    offset.row = (s32)y;
    return hex_from_offset(offset);
}

internal b32
fixture_flag(tsv_table *table, u32 row, const char *column)
{
    return strcmp(tsv_cell(table, row, column), "1") == 0;
}

/* hexes.tsv: each hex as the engine has it. A side hex of a gate is cliff
   after the load (F-151); the gate hex is a base. */
internal void
fixture_map_hexes(tsv_table *table, engine_api *api, test_counts *counts)
{
    u32 wrong = 0;
    for (u32 row = 0; row < table->row_count; ++row)
    {
        u32 terrain = 0, region = 0, trap = 0, height = 0;
        tsv_cell_u32(table, row, "terrain", &terrain);
        tsv_cell_u32(table, row, "region", &region);
        tsv_cell_u32(table, row, "trap", &trap);
        tsv_cell_u32(table, row, "height", &height);
        hex_coord hex = fixture_map_hex(table, row);
        engine_hex data = api->hex_at(api->engine, hex.q, hex.r);
        u32 expected = terrain == FIXTURE_MAP_GATE && data.base_id == ENGINE_NO_BASE ? FIXTURE_MAP_CLIFF : terrain;
        b32 same = data.on_map && data.terrain == expected && data.region == region && data.trap == trap && data.height == height &&
                   data.slot == fixture_flag(table, row, "slot") && data.unknown_flag == fixture_flag(table, row, "flag6") &&
                   data.bandit_site == fixture_flag(table, row, "flag7") && data.flood == fixture_flag(table, row, "flood") &&
                   data.event_site == fixture_flag(table, row, "flag10");
        if (same) { counts->passed++; continue; }
        counts->failed++;
        if (wrong++ < 5) printf("  FAIL map/hexes row %u: hex (%s, %s) differs from the engine\n", row, tsv_cell(table, row, "x"), tsv_cell(table, row, "y"));
    }
    printf("  PENDING water_dir and zone: the engine does not read them (Q-192, Q-197)\n");
}

/* bases.tsv: the hex of each city, gate and port. */
internal void
fixture_map_bases(tsv_table *table, engine_api *api, test_counts *counts)
{
    void *e = api->engine;
    for (u32 row = 0; row < table->row_count; ++row)
    {
        u32 id = 0;
        tsv_cell_u32(table, row, "id", &id);
        hex_coord hex = fixture_map_hex(table, row);
        engine_hex data = api->hex_at(e, hex.q, hex.r);
        b32 same = false;
        if (strcmp(tsv_cell(table, row, "kind"), "city") == 0)
        {
            engine_city_slots site = api->city_slots(e, (u8)id);
            same = site.q == hex.q && site.r == hex.r && data.city_id == id;
        }
        else
        {
            engine_base base = api->base_at(e, id - CONTENT_FIRST_BASE);
            same = base.id == id && base.q == hex.q && base.r == hex.r && data.base_id == id;
        }
        char name[128];
        snprintf(name, sizeof(name), "map/bases row %u: base %u stands on (%s, %s)", row, id, tsv_cell(table, row, "x"), tsv_cell(table, row, "y"));
        TEST_CHECK(counts, same, name);
    }
}

/* slots.tsv: each slot belongs to its city, with its slot neighbours. */
internal void
fixture_map_slots(tsv_table *table, engine_api *api, test_counts *counts)
{
    void *e = api->engine;
    for (u32 row = 0; row < table->row_count; ++row)
    {
        u32 city = 0, neighbours = 0;
        tsv_cell_u32(table, row, "city", &city);
        tsv_cell_u32(table, row, "slot_neighbours", &neighbours);
        hex_coord hex = fixture_map_hex(table, row);
        engine_city_slots site = api->city_slots(e, (u8)city);
        b32 same = false;
        for (u32 slot = 0; slot < site.slot_count && !same; ++slot)
        {
            engine_facility facility = api->facility_at(e, (u8)city, (u8)slot);
            same = facility.q == hex.q && facility.r == hex.r && facility.slot_neighbours == neighbours;
        }
        char name[128];
        snprintf(name, sizeof(name), "map/slots row %u: city %u has the slot (%s, %s) with %u slot neighbours", row, city,
                 tsv_cell(table, row, "x"), tsv_cell(table, row, "y"), neighbours);
        TEST_CHECK(counts, same, name);
    }
}

/* sites.tsv: the dikes, the rockfall sites and the flag sites. */
internal void
fixture_map_sites(tsv_table *table, engine_api *api, test_counts *counts)
{
    void *e = api->engine;
    for (u32 row = 0; row < table->row_count; ++row)
    {
        const char *kind = tsv_cell(table, row, "kind");
        hex_coord hex = fixture_map_hex(table, row);
        engine_hex data = api->hex_at(e, hex.q, hex.r);
        u32 building = api->building_at_hex(e, hex.q, hex.r);
        b32 same = false;
        if      (strcmp(kind, "dike") == 0)     same = data.trap == 1 && building != ENGINE_NO_BUILDING;
        else if (strcmp(kind, "rockfall") == 0) same = data.trap == 2 && building != ENGINE_NO_BUILDING;
        else if (strcmp(kind, "flag6") == 0)    same = data.unknown_flag;
        else if (strcmp(kind, "flag7") == 0)    same = data.bandit_site;
        else if (strcmp(kind, "flag10") == 0)   same = data.event_site;
        char name[128];
        snprintf(name, sizeof(name), "map/sites row %u: a %s at (%s, %s)", row, kind, tsv_cell(table, row, "x"), tsv_cell(table, row, "y"));
        TEST_CHECK(counts, same, name);
    }
}

internal u8
fixture_cost(tsv_table *table, u32 row, const char *column)
{
    u32 cost = 0;
    tsv_cell_u32(table, row, column, &cost);
    return cost == 255 ? (u8)0 : (u8)cost;
}

/* terrain-types.tsv: enterable, burnable, military_build and the costs. */
internal void
fixture_map_terrain_types(tsv_table *table, engine_api *api, test_counts *counts)
{
    for (u32 row = 0; row < table->row_count; ++row)
    {
        u32 id = 0;
        tsv_cell_u32(table, row, "id", &id);
        engine_terrain_kind kind = api->terrain_kind_at(api->engine, id);
        b32 same = kind.id == id && kind.enterable == fixture_flag(table, row, "enterable") && kind.burnable == fixture_flag(table, row, "burnable") &&
                   kind.buildable == fixture_flag(table, row, "military_build") && kind.cost[MOVEMENT_FOOT] == fixture_cost(table, row, "cost_land") &&
                   kind.cost[MOVEMENT_CAVALRY] == fixture_cost(table, row, "cost_cavalry") && kind.cost[MOVEMENT_SIEGE] == fixture_cost(table, row, "cost_siege") &&
                   kind.cost[MOVEMENT_NAVY] == fixture_cost(table, row, "cost_ship");
        char name[128];
        snprintf(name, sizeof(name), "map/terrain-types row %u: terrain %u (%s)", row, id, kind.key);
        TEST_CHECK(counts, same, name);
    }
}

/* movement-costs.tsv: the 12 armaments x 20 terrains. The terrain columns
   follow stat_c in the order of the terrain ids. */
internal void
fixture_map_movement_costs(tsv_table *table, engine_api *api, test_counts *counts)
{
    s32 first = tsv_column(table, "stat_c") + 1;
    for (u32 row = 0; row < table->row_count; ++row)
    {
        u32 armament = 0, move = 0;
        tsv_cell_u32(table, row, "armament", &armament);
        tsv_cell_u32(table, row, "stat_c", &move);
        engine_armament kind = api->armament_at(api->engine, armament);
        b32 same = first > 0 && kind.id == armament && kind.move == move;
        for (u32 terrain = 0; same && terrain < api->terrain_kind_count(api->engine); ++terrain)
        {
            u8 expected = fixture_cost(table, row, table->columns[first + (s32)terrain]);
            same = kind.cost[terrain] == expected;
        }
        char name[128];
        snprintf(name, sizeof(name), "map/movement-costs row %u: armament %u (%s) on the 20 terrains", row, armament, kind.key);
        TEST_CHECK(counts, same, name);
    }
}

/* facility-types.tsv: the terrain mask of the military facilities, the
   obstacles and the traps is the build permission of the engine (F-147). */
internal void
fixture_map_facility_types(tsv_table *table, engine_api *api, test_counts *counts)
{
    u32 pending = 0;
    for (u32 row = 0; row < table->row_count; ++row)
    {
        u32 category = 0, mask = 0;
        tsv_cell_u32(table, row, "category", &category);
        const char *mask_text = tsv_cell(table, row, "terrain_mask");
        for (const char *at = mask_text + 2; *at; ++at) mask = mask * 16 + (u32)(*at <= '9' ? *at - '0' : (*at | 0x20) - 'a' + 10);
        b32 military = mask == 0x407, fire_ship = mask == 0x180;
        if ((category < 1 || category > 3) || (!military && !fire_ship)) { pending++; continue; }
        b32 same = true;
        for (u32 terrain = 0; terrain < api->terrain_kind_count(api->engine); ++terrain)
        {
            engine_terrain_kind kind = api->terrain_kind_at(api->engine, terrain);
            b32 water = kind.cost[MOVEMENT_NAVY] && !kind.cost[MOVEMENT_FOOT];
            b32 allowed = military ? kind.buildable : water;
            if (allowed != (b32)((mask >> terrain) & 1u)) same = false;
        }
        char name[128];
        snprintf(name, sizeof(name), "map/facility-types row %u: the terrain mask %s of %s", row, mask_text, military ? "a military facility" : "the fire ship");
        TEST_CHECK(counts, same, name);
    }
    printf("  PENDING %u rows: bases, domestic facilities (on slots only), natural traps and the 淺灘 trap have no build rule of their own\n", pending);
}

global_variable fixture_handler g_fixture_handlers[] = {
    { "unit-values-guidebook", fixture_unit_values_guidebook },
    { "map/hexes",             fixture_map_hexes },
    { "map/bases",             fixture_map_bases },
    { "map/slots",             fixture_map_slots },
    { "map/sites",             fixture_map_sites },
    { "map/terrain-types",     fixture_map_terrain_types },
    { "map/movement-costs",    fixture_map_movement_costs },
    { "map/facility-types",    fixture_map_facility_types },
    { 0, 0 }   /* sentinel */
};

internal fixture_handler *
find_fixture_handler(const char *mechanic)
{
    for (fixture_handler *h = g_fixture_handlers; h->mechanic; ++h)
    {
        if (strcmp(h->mechanic, mechanic) == 0) return h;
    }
    return 0;
}

/* Runs every fixture file in dir. The mechanic of a file is its name
   without .tsv, after prefix ("" or "map/"). With real_map, each file gets
   an engine with the map of the pack and the sample save (M10). Returns the
   number of files seen. */
internal u32
feature_fixture_tests(const char *dir, const char *prefix, b32 real_map, memory_arena *arena, test_counts *totals)
{
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*.tsv", dir);

    WIN32_FIND_DATAA found;
    HANDLE search = FindFirstFileA(pattern, &found);
    if (search == INVALID_HANDLE_VALUE)
    {
        printf("fixtures: none found in %s\n", dir);
        return 0;
    }

    u32 file_count = 0;
    do
    {
        file_count++;
        char mechanic[MAX_PATH];
        snprintf(mechanic, sizeof(mechanic), "%s%s", prefix, found.cFileName);
        char *dot = strrchr(mechanic, '.');
        if (dot) *dot = 0;

        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s\\%s", dir, found.cFileName);

        tsv_table table = {};
        if (!tsv_load(path, arena, &table))
        {
            printf("FAIL 0/0 fixture/%s (could not parse)\n", mechanic);
            totals->failed++;
            continue;
        }

        fixture_handler *handler = find_fixture_handler(mechanic);
        if (!handler)
        {
            printf("FAIL 0/%u fixture/%s (no handler in tests/feature/fixture_tests.cpp)\n", table.row_count, mechanic);
            totals->failed += table.row_count ? table.row_count : 1;
            continue;
        }

        /* Each mechanic gets a fresh engine so rows cannot leak into each other. */
        engine_fixture f;
        if (real_map)
        {
            engine_fixture_init_real(&f, 1, 4096);
            f.api.load_save(f.api.engine, SAMPLE_SAVE_RELATIVE_PATH);
        }
        else
        {
            engine_fixture_init(&f, 1, 4096);
        }
        test_counts counts = {};
        handler->run(&table, &f.api, &counts);
        printf("%s %u/%u fixture/%s\n", counts.failed ? "FAIL" : "PASS",
               counts.passed, counts.passed + counts.failed, mechanic);
        totals->passed += counts.passed;
        totals->failed += counts.failed;
        engine_fixture_destroy(&f);
    } while (FindNextFileA(search, &found));
    FindClose(search);
    return file_count;
}
