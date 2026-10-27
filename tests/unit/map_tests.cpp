/* Unit tests of code/sim/map.cpp (M10): the load of the real map from the
   spec fixtures (fixtures/map/hexes.tsv, bases.tsv, slots.tsv, sites.tsv,
   terrain-types.tsv), the rules of the load (F-151), and the movement cost
   table of content.cpp (F-148). The loader does no I/O; this file reads the
   fixtures and gives the text to it. */

/* The keys of the 20 terrains, in the order of the spec ids. */
global_variable const char *unit_map_terrain_keys[] = {
    "grass", "earth", "sand", "swamp", "poison_spring", "forest", "stream", "river", "sea", "waste",
    "main_road", "plank_road", "ford", "shallows", "shore", "cliff", "city", "port", "gate", "small_path"
};

internal char *
unit_map_read(const char *relative, memory_arena *arena)
{
    char path[MAX_PATH];
    test_fixture_path(path, MAX_PATH, relative);
    return (char *)test_read_file(path, arena).contents;   /* the arena gives a zero after the text */
}

internal b32
unit_map_table(const char *relative, memory_arena *arena, tsv_table *table)
{
    char *text = unit_map_read(relative, arena);
    return text && tsv_parse(text, arena, table);
}

internal u8
unit_map_cost(tsv_table *table, u32 row, const char *column)
{
    u32 cost = 0;
    tsv_cell_u32(table, row, column, &cost);
    return cost == 255 ? (u8)0 : (u8)cost;
}

/* The terrain table of the content from terrain-types.tsv. */
internal b32
unit_map_terrain(sim_content *content, tsv_table *types)
{
    if (types->row_count != ArrayCount(unit_map_terrain_keys)) return false;
    content->terrain_count = types->row_count;
    for (u32 row = 0; row < types->row_count; ++row)
    {
        terrain_kind *kind = content->terrains + row;
        *kind = {};
        kind->id = (u8)row;
        memcpy(kind->key, unit_map_terrain_keys[row], strlen(unit_map_terrain_keys[row]) + 1);
        kind->cost[MOVEMENT_FOOT]    = unit_map_cost(types, row, "cost_land");
        kind->cost[MOVEMENT_CAVALRY] = unit_map_cost(types, row, "cost_cavalry");
        kind->cost[MOVEMENT_SIEGE]   = unit_map_cost(types, row, "cost_siege");
        kind->cost[MOVEMENT_NAVY]    = unit_map_cost(types, row, "cost_ship");
        kind->enterable = strcmp(tsv_cell(types, row, "enterable"), "1") == 0;
        kind->burnable  = strcmp(tsv_cell(types, row, "burnable"), "1") == 0;
        kind->buildable = strcmp(tsv_cell(types, row, "military_build"), "1") == 0;
        kind->needs_technique = kind->loss_waiver_technique = CONTENT_NO_TECHNIQUE;
    }
    return true;
}

internal hex_coord
unit_map_hex(tsv_table *table, u32 row)
{
    u32 x = 0, y = 0;
    tsv_cell_u32(table, row, "x", &x);
    tsv_cell_u32(table, row, "y", &y);
    hex_offset offset;
    offset.col = (s32)x;
    offset.row = (s32)y;
    return hex_from_offset(offset);
}

/* The errors of the loader, on small texts. */
internal void
unit_map_error_tests(test_counts *counts, memory_arena *arena, sim_content *content)
{
    sim_state *sim = PushStruct(arena, sim_state);
    sim_init(sim, arena, 1);
    sim->content = *content;
    sim_init_map(sim, arena, 2, 1, 1);
    const char *header = "x\ty\tterrain\tregion\ttrap\tslot\tflag6\tflag7\tflood\tflag10\theight\n";
    char text[512];
    snprintf(text, sizeof(text), "%s0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t30\n1\t0\t1\t0\t0\t1\t0\t0\t0\t0\t40\n", header);
    u32 width = 0, height = 0;
    TEST_CHECK(counts, sim_map_measure_hexes(text, &width, &height) && width == 2 && height == 1, "the size of a grid is the largest x and y plus one");
    sim_map_load_result good = sim_map_load_hexes(sim, text);
    TEST_CHECK(counts, good.ok && sim->map.kind[1] == 1 && sim->map_layers.height[1] == 40 && (sim->map_layers.flags[1] & SIM_MAP_FLAG_SLOT) &&
                       sim->map.level[1] == 40 / SIM_MAP_HEIGHT_STEP && sim->map.cost[0] == content->terrains[0].cost[MOVEMENT_FOOT],
               "a small grid loads: kind, cost, level, height and the slot flag");
    snprintf(text, sizeof(text), "%s0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t30\n0\t0\t1\t0\t0\t1\t0\t0\t0\t0\t40\n", header);
    sim_map_load_result twice = sim_map_load_hexes(sim, text);
    TEST_CHECK(counts, !twice.ok && twice.line == 3, "refused: a hex with two rows (the line of the second row)");
    snprintf(text, sizeof(text), "%s0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t30\n", header);
    TEST_CHECK(counts, !sim_map_load_hexes(sim, text).ok, "refused: a hex without a row");
    snprintf(text, sizeof(text), "%s0\t0\t25\t0\t0\t0\t0\t0\t0\t0\t30\n1\t0\t1\t0\t0\t0\t0\t0\t0\t0\t40\n", header);
    TEST_CHECK(counts, !sim_map_load_hexes(sim, text).ok, "refused: a terrain id that terrain.tsv does not have");
    snprintf(text, sizeof(text), "x\ty\tterrain\n0\t0\t0\n1\t0\t0\n");
    TEST_CHECK(counts, !sim_map_load_hexes(sim, text).ok && !sim->map_layers.loaded, "refused: hexes.tsv without the flag columns");
}

/* movement_costs.tsv of the pack and the cost of a unit type (F-148). */
internal void
unit_map_cost_tests(test_counts *counts, memory_arena *arena, sim_content *base)
{
    sim_content *content = PushStruct(arena, sim_content);
    *content = *base;
    char text[4096];
    u32 used = (u32)snprintf(text, sizeof(text), "id\tkey\tmove");
    for (u32 terrain = 0; terrain < content->terrain_count; ++terrain) used += (u32)snprintf(text + used, sizeof(text) - used, "\t%s", content->terrains[terrain].key);
    used += (u32)snprintf(text + used, sizeof(text) - used, "\n0\tsword\t20");
    for (u32 terrain = 0; terrain < content->terrain_count; ++terrain) used += (u32)snprintf(text + used, sizeof(text) - used, "\t%u", terrain == 15 ? 255u : 5u + terrain % 3);
    used += (u32)snprintf(text + used, sizeof(text) - used, "\n1\tboat\t16");
    for (u32 terrain = 0; terrain < content->terrain_count; ++terrain) used += (u32)snprintf(text + used, sizeof(text) - used, "\t%u", terrain == 7 || terrain == 8 ? 4u : 255u);
    snprintf(text + used, sizeof(text) - used, "\n");
    tsv_table table = {};
    u32 bad_row = 0;
    b32 loaded = tsv_parse(text, arena, &table) && content_load_armaments(content, &table, &bad_row);
    TEST_CHECK(counts, loaded && content->armament_count == 2 && content->armaments[0].move == 20 && content->armaments[0].cost[0] == 5 &&
                       content->armaments[0].cost[15] == 0 && content->armaments[1].cost[7] == 4 && content->armaments[1].cost[0] == 0,
               "movement_costs.tsv loads; 255 means that the armament cannot enter");

    content->unit_type_count = 2;
    content->unit_types[0] = {};
    content->unit_types[0].movement = MOVEMENT_FOOT;
    content->unit_types[0].armament = 0;
    content->unit_types[0].ship     = 1;
    content->unit_types[1] = {};
    content->unit_types[1].movement = MOVEMENT_FOOT;
    content->unit_types[1].armament = CONTENT_NO_ARMAMENT;
    content->unit_types[1].ship     = CONTENT_NO_ARMAMENT;
    sim_state *sim = PushStruct(arena, sim_state);
    sim_init(sim, arena, 1);
    sim->content = *content;
    TEST_CHECK(counts, sim_terrain_cost(sim, MOVEMENT_FOOT, 0, 1) == 6 && sim_terrain_cost(sim, MOVEMENT_FOOT, 0, 7) == 4 &&
                       sim_terrain_cost(sim, MOVEMENT_FOOT, 0, 15) == 0,
               "a unit type pays the row of its armament on land and the row of its ship on 河 (F-148)");
    TEST_CHECK(counts, sim_terrain_cost(sim, MOVEMENT_FOOT, 1, 1) == content->terrains[1].cost[MOVEMENT_FOOT] &&
                       sim_terrain_cost(sim, MOVEMENT_FOOT, 1, 7) == content->terrains[7].cost[MOVEMENT_NAVY],
               "a unit type without armaments pays the class costs; on water a foot unit is a 走舸");

    snprintf(text, sizeof(text), "id\tkey\tmove\tgrass\n0\tsword\t20\t5\n");
    TEST_CHECK(counts, tsv_parse(text, arena, &table) && !content_load_armaments(content, &table, &bad_row) && content->armament_count == 0,
               "refused: movement_costs.tsv without a column for each terrain");
}

internal void
unit_map_tests(test_counts *counts)
{
    memory_arena arena = test_arena_create(Megabytes(64));
    tsv_table types = {}, bases = {}, slots = {}, sites = {};
    char *hexes = unit_map_read("map/hexes.tsv", &arena);
    b32 read = hexes && unit_map_table("map/terrain-types.tsv", &arena, &types) && unit_map_table("map/bases.tsv", &arena, &bases) &&
               unit_map_table("map/slots.tsv", &arena, &slots) && unit_map_table("map/sites.tsv", &arena, &sites);
    TEST_CHECK(counts, read, "the map fixtures of the spec are readable");
    sim_content *content = PushStruct(&arena, sim_content);
    TEST_CHECK(counts, read && unit_map_terrain(content, &types), "terrain-types.tsv has the 20 terrains");
    if (!read || content->terrain_count != ArrayCount(unit_map_terrain_keys)) { test_arena_destroy(&arena); return; }

    sim_state *sim = PushStruct(&arena, sim_state);
    sim_init(sim, &arena, 1);
    sim->content = *content;
    u32 width = 0, height = 0;
    TEST_CHECK(counts, sim_map_measure_hexes(hexes, &width, &height) && width == 200 && height == 200, "the grid is 200 x 200 (map-data.md)");
    sim_init_map(sim, &arena, width, height, 1);
    sim_map_load_result loaded = sim_map_load_hexes(sim, hexes);
    TEST_CHECK(counts, loaded.ok, "hexes.tsv loads: one row for each of the 40,000 hexes");
    if (!loaded.ok) { test_arena_destroy(&arena); return; }

    /* Before the rules of the load, the count of each terrain is the column "hexes". */
    u32 terrain_hexes[CONTENT_TERRAIN_CAPACITY] = {};
    u32 cells = width * height;
    for (u32 cell = 0; cell < cells; ++cell) terrain_hexes[sim->map.kind[cell]]++;
    b32 terrain_counts = true;
    for (u32 row = 0; row < types.row_count; ++row)
    {
        u32 expected = 0;
        tsv_cell_u32(&types, row, "hexes", &expected);
        if (terrain_hexes[row] != expected) terrain_counts = false;
    }
    TEST_CHECK(counts, terrain_counts && terrain_hexes[16] == 294 && terrain_hexes[18] == 38, "the count of each terrain equals terrain-types.tsv (294 city hexes, 38 gate hexes)");

    sim_map_load_result based = sim_map_load_bases(sim, &bases);
    u32 cities = 0, gates = 0, ports = 0;
    for (u32 region = 0; region < SIM_MAP_REGION_CAPACITY; ++region)
    {
        cities += sim->map_layers.base_type[region] == MAP_BASE_CITY;
        gates  += sim->map_layers.base_type[region] == MAP_BASE_GATE;
        ports  += sim->map_layers.base_type[region] == MAP_BASE_PORT;
    }
    TEST_CHECK(counts, based.ok && cities == 42 && gates == 10 && ports == 35, "bases.tsv has 42 cities, 10 gates and 35 ports");

    sim_map_load_result finished = sim_map_finish(sim);
    TEST_CHECK(counts, finished.ok && sim->map_layers.loaded, "the rules of the load run");
    if (!finished.ok) { test_arena_destroy(&arena); return; }
    sim_map_layers *layers = &sim->map_layers;

    /* A city is a flower of 7 hexes around its centre. */
    b32 flowers = true;
    for (u32 city = 0; city < SAVE_CITY_COUNT; ++city)
    {
        hex_coord center = layers->base_hex[city];
        u32 cell = sim_map_cell(sim, center);
        if (cell == U32_MAX || sim->map.kind[cell] != layers->city_kind || layers->region[cell] != city) flowers = false;
        for (u32 direction = 0; direction < HEX_DIRECTION_COUNT; ++direction)
        {
            u32 petal = sim_map_cell(sim, hex_neighbor(center, direction));
            if (petal == U32_MAX || sim->map.kind[petal] != layers->city_kind || layers->region[petal] != city) flowers = false;
        }
    }
    TEST_CHECK(counts, flowers, "each of the 42 cities is a flower of 7 city hexes around the base hex");

    /* F-151: the side hexes of each gate are cliff; the gate hex stays. */
    u32 gate_hexes = 0, port_hexes = 0;
    for (u32 cell = 0; cell < cells; ++cell)
    {
        gate_hexes += sim->map.kind[cell] == layers->gate_kind;
        port_hexes += sim->map.kind[cell] == layers->port_kind;
    }
    b32 gates_stand = true;
    for (u32 region = 0; region < SIM_MAP_REGION_CAPACITY; ++region)
    {
        if (layers->base_type[region] != MAP_BASE_GATE) continue;
        u32 cell = sim_map_cell(sim, layers->base_hex[region]);
        if (cell == U32_MAX || sim->map.kind[cell] != layers->gate_kind || layers->region[cell] != region) gates_stand = false;
    }
    TEST_CHECK(counts, layers->gate_side_cliffs == 28 && gate_hexes == 10 && gates_stand && port_hexes == 35,
               "the 28 side hexes of the gates become cliff; each of the 10 gates keeps its gate hex (F-151)");
    TEST_CHECK(counts, sim->map.cost[sim_map_cell(sim, layers->base_hex[42])] == content->terrains[18].cost[MOVEMENT_FOOT] &&
                       content->terrains[15].cost[MOVEMENT_FOOT] == 0,
               "a unit can enter the gate hex and not its sides");

    /* The slots: each city gets the slots of its region and of its ports. */
    u32 slot_total = 0, port_slots = 0;
    b32 slot_counts = true;
    for (u32 city = 0; city < SAVE_CITY_COUNT; ++city)
    {
        slot_total += layers->slot_count[city];
        u32 expected = 0;
        tsv_cell_u32(&bases, city, "slots", &expected);
        if (layers->slot_count[city] != expected) slot_counts = false;
        for (u32 slot = 0; slot < layers->slot_count[city]; ++slot)
        {
            u8 region = layers->region[sim_map_cell(sim, layers->slot_hex[city][slot])];
            if (layers->base_type[region] == MAP_BASE_PORT) port_slots++;
        }
    }
    TEST_CHECK(counts, slot_total == 591 && slot_counts, "591 slots; the count of each city equals bases.tsv and the guide book");
    TEST_CHECK(counts, port_slots == 50, "50 slots stand in port regions and count for the city of the port (F-151)");

    b32 slot_rows = slots.row_count == 591;
    u32 next[SAVE_CITY_COUNT] = {};
    for (u32 row = 0; row < slots.row_count && slot_rows; ++row)
    {
        u32 city = 0, neighbours = 0;
        tsv_cell_u32(&slots, row, "city", &city);
        tsv_cell_u32(&slots, row, "slot_neighbours", &neighbours);
        hex_coord hex = unit_map_hex(&slots, row);
        if (city >= SAVE_CITY_COUNT || next[city] >= layers->slot_count[city] || !hex_equal(layers->slot_hex[city][next[city]], hex)) slot_rows = false;
        else if (sim_map_slot_neighbours(sim, (u8)city, hex) != neighbours) slot_rows = false;
        if (city < SAVE_CITY_COUNT) next[city]++;
    }
    TEST_CHECK(counts, slot_rows, "the slot list of each city is slots.tsv in its order, with its slot neighbours");

    /* The sites of sites.tsv are the trap and the flags of the map. */
    u32 dikes = 0, rockfalls = 0, unknown = 0, bandits = 0, events = 0, wrong = 0;
    for (u32 row = 0; row < sites.row_count; ++row)
    {
        const char *kind = tsv_cell(&sites, row, "kind");
        u32 cell = sim_map_cell(sim, unit_map_hex(&sites, row));
        if (cell == U32_MAX) { wrong++; continue; }
        u8 flags = layers->flags[cell], trap = layers->trap[cell];
        if      (strcmp(kind, "dike") == 0)     { dikes++;     if (trap != SIM_MAP_TRAP_DIKE) wrong++; }
        else if (strcmp(kind, "rockfall") == 0) { rockfalls++; if (trap != SIM_MAP_TRAP_ROCKFALL) wrong++; }
        else if (strcmp(kind, "flag6") == 0)    { unknown++;   if (!(flags & SIM_MAP_FLAG_UNKNOWN)) wrong++; }
        else if (strcmp(kind, "flag7") == 0)    { bandits++;   if (!(flags & SIM_MAP_FLAG_BANDIT)) wrong++; }
        else if (strcmp(kind, "flag10") == 0)   { events++;    if (!(flags & SIM_MAP_FLAG_EVENT)) wrong++; }
        else wrong++;
    }
    u32 floods = 0;
    for (u32 cell = 0; cell < cells; ++cell) floods += (layers->flags[cell] & SIM_MAP_FLAG_FLOOD) != 0;
    TEST_CHECK(counts, wrong == 0 && dikes == 4 && rockfalls == 18 && unknown == 396 && bandits == 190 && events == 39,
               "sites.tsv: 4 dikes, 18 rockfall sites, 190 bandit sites, 39 event sites and 396 hexes of byte 6 (F-150)");
    TEST_CHECK(counts, floods == 195, "195 hexes are in the flood zone of a dike");

    /* The flood zone of a dike is the flood hexes of its own region. */
    hex_coord dike_hexes[4];
    u32 dike_count = 0;
    for (u32 cell = 0; cell < cells && dike_count < 4; ++cell)
    {
        if (layers->trap[cell] == SIM_MAP_TRAP_DIKE) dike_hexes[dike_count++] = hex_map_coord(&sim->map, cell);
    }
    u32 own = 0, other = 0;
    for (u32 cell = 0; cell < cells; ++cell)
    {
        if (!(layers->flags[cell] & SIM_MAP_FLAG_FLOOD)) continue;
        hex_coord hex = hex_map_coord(&sim->map, cell);
        for (u32 dike = 0; dike < dike_count; ++dike)
        {
            b32 same_region = layers->region[sim_map_cell(sim, dike_hexes[dike])] == layers->region[cell];
            if (sim_map_in_flood(sim, dike_hexes[dike], hex)) { if (same_region) own++; else other++; }
        }
    }
    TEST_CHECK(counts, dike_count == 4 && own == 195 && other == 0, "a dike floods the 195 flood hexes of the 4 dike regions, each only its own (F-150)");

    unit_map_error_tests(counts, &arena, content);
    unit_map_cost_tests(counts, &arena, content);
    test_arena_destroy(&arena);
}
