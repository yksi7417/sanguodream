/* The real map of a content pack (M10; formats/map-data.md,
   terrain-and-map.md): the hex grid of map\hexes.tsv and the bases of
   map\bases.tsv go into the hex_map of the sim and the map layers beside
   it. This file does no I/O: the engine reads the files and gives the text.
   Included by sim.cpp after content.cpp.

   The order of a load:
   1. sim_map_measure_hexes gives the size of the grid, for sim_init_map.
   2. sim_map_load_hexes fills the terrain, the costs and the layers.
   3. sim_map_load_bases gives the position and the city of each base.
   4. sim_map_finish turns the side hexes of each gate to cliff and makes
      the slot list of each city (F-151). */

/* ---- reading the text of hexes.tsv in place ------------------------------- */

/* hexes.tsv has 40,000 rows. A tsv_table of it needs approximately 5 MB of
   cell pointers, so this file reads each line where it is and does not
   change the text. */

internal b32
sim_map_end_of_field(char c)
{
    return c == 0 || c == '\t' || c == '\n' || c == '\r';
}

/* The start of the line after this one, or 0 at the end of the text. */
internal const char *
sim_map_next_line(const char *line)
{
    while (*line && *line != '\n') ++line;
    return *line ? line + 1 : 0;
}

internal b32
sim_map_line_is_data(const char *line)
{
    return line[0] != 0 && line[0] != '\n' && line[0] != '\r' && line[0] != '#';
}

/* The first line that is not empty and not a comment: the header. */
internal const char *
sim_map_header_line(const char *text)
{
    for (const char *line = text; line; line = sim_map_next_line(line))
    {
        if (sim_map_line_is_data(line)) return line;
    }
    return 0;
}

/* The column number of a header name, or -1. */
internal s32
sim_map_column(const char *header, const char *name)
{
    u64 length = strlen(name);
    const char *field = header;
    for (s32 column = 0;; ++column)
    {
        u64 field_length = 0;
        while (!sim_map_end_of_field(field[field_length])) ++field_length;
        if (field_length == length && memcmp(field, name, length) == 0) return column;
        if (field[field_length] != '\t') return -1;
        field += field_length + 1;
    }
}

/* A decimal cell of a line, by its column number. */
internal b32
sim_map_field_u32(const char *line, s32 column, u32 *value)
{
    if (column < 0) return false;
    const char *field = line;
    for (s32 skip = 0; skip < column; ++skip)
    {
        while (!sim_map_end_of_field(*field)) ++field;
        if (*field != '\t') return false;
        ++field;
    }
    u64 result = 0;
    u32 digits = 0;
    for (; *field >= '0' && *field <= '9'; ++field, ++digits)
    {
        result = result * 10 + (u64)(*field - '0');
        if (result > U32_MAX) return false;
    }
    if (!digits || !sim_map_end_of_field(*field)) return false;
    *value = (u32)result;
    return true;
}

internal sim_map_load_result
sim_map_result(b32 ok, const char *detail, u32 line)
{
    sim_map_load_result result;
    result.ok     = ok;
    result.detail = detail;
    result.line   = line;
    return result;
}

/* The size of the grid: the largest x and the largest y, plus one. */
internal b32
sim_map_measure_hexes(const char *text, u32 *width, u32 *height)
{
    *width = *height = 0;
    const char *header = text ? sim_map_header_line(text) : 0;
    if (!header) return false;
    s32 column_x = sim_map_column(header, "x"), column_y = sim_map_column(header, "y");
    if (column_x < 0 || column_y < 0) return false;
    for (const char *line = sim_map_next_line(header); line; line = sim_map_next_line(line))
    {
        if (!sim_map_line_is_data(line)) continue;
        u32 x = 0, y = 0;
        if (!sim_map_field_u32(line, column_x, &x) || !sim_map_field_u32(line, column_y, &y)) return false;
        if (x + 1 > *width)  *width  = x + 1;
        if (y + 1 > *height) *height = y + 1;
    }
    return *width > 0 && *height > 0;
}

/* ---- the hex grid ------------------------------------------------------------ */

enum sim_map_column_id
{
    MAP_COLUMN_X, MAP_COLUMN_Y, MAP_COLUMN_TERRAIN, MAP_COLUMN_REGION, MAP_COLUMN_TRAP, MAP_COLUMN_SLOT,
    MAP_COLUMN_FLAG6, MAP_COLUMN_FLAG7, MAP_COLUMN_FLOOD, MAP_COLUMN_FLAG10, MAP_COLUMN_HEIGHT, MAP_COLUMN_COUNT
};

global_variable const char *sim_map_column_names[MAP_COLUMN_COUNT] = {
    "x", "y", "terrain", "region", "trap", "slot", "flag6", "flag7", "flood", "flag10", "height"
};

#define SIM_MAP_SEEN 0x80u   /* a flag of the load only: the row of the cell was read */

/* Fills the map from the text of hexes.tsv. The map must have the size of
   sim_map_measure_hexes, and the content must have its terrain table. Each
   cell must have exactly one row. */
internal sim_map_load_result
sim_map_load_hexes(sim_state *sim, const char *text)
{
    sim_map_layers *layers = &sim->map_layers;
    hex_map *map = &sim->map;
    layers->loaded = false;
    const char *header = text ? sim_map_header_line(text) : 0;
    if (!header) return sim_map_result(false, "hexes.tsv has no header", 1);
    if (!sim->content.terrain_count) return sim_map_result(false, "the pack has no terrain table", 0);
    s32 columns[MAP_COLUMN_COUNT];
    for (u32 column = 0; column < MAP_COLUMN_COUNT; ++column)
    {
        columns[column] = sim_map_column(header, sim_map_column_names[column]);
        if (columns[column] < 0) return sim_map_result(false, "hexes.tsv lacks a column", 1);
    }

    u32 cells = map->width * map->height;
    for (u32 cell = 0; cell < cells; ++cell)
    {
        layers->region[cell] = SIM_MAP_NO_REGION;
        layers->trap[cell]   = SIM_MAP_TRAP_NONE;
        layers->flags[cell]  = 0;
        layers->height[cell] = 0;
    }

    u32 rows = 0, line_number = 1;
    for (const char *line = header; line; line = sim_map_next_line(line), ++line_number)
    {
        if (line == header || !sim_map_line_is_data(line)) continue;
        u32 value[MAP_COLUMN_COUNT];
        for (u32 column = 0; column < MAP_COLUMN_COUNT; ++column)
        {
            if (!sim_map_field_u32(line, columns[column], value + column)) return sim_map_result(false, "hexes.tsv has a bad cell", line_number);
        }
        if (value[MAP_COLUMN_X] >= map->width || value[MAP_COLUMN_Y] >= map->height) return sim_map_result(false, "a hex is off the map", line_number);
        if (value[MAP_COLUMN_TERRAIN] >= sim->content.terrain_count)              return sim_map_result(false, "a terrain id is not in terrain.tsv", line_number);
        if (value[MAP_COLUMN_REGION] >= SIM_MAP_REGION_CAPACITY || value[MAP_COLUMN_TRAP] > SIM_MAP_TRAP_ROCKFALL || value[MAP_COLUMN_HEIGHT] > 255)
        {
            return sim_map_result(false, "a region, a trap or a height is out of range", line_number);
        }
        u32 flags = 0;
        u32 flag_columns[] = { MAP_COLUMN_SLOT, MAP_COLUMN_FLAG6, MAP_COLUMN_FLAG7, MAP_COLUMN_FLOOD, MAP_COLUMN_FLAG10 };
        u32 flag_bits[]    = { SIM_MAP_FLAG_SLOT, SIM_MAP_FLAG_UNKNOWN, SIM_MAP_FLAG_BANDIT, SIM_MAP_FLAG_FLOOD, SIM_MAP_FLAG_EVENT };
        for (u32 index = 0; index < ArrayCount(flag_columns); ++index)
        {
            u32 flag = value[flag_columns[index]];
            if (flag > 1) return sim_map_result(false, "a flag is not 0 or 1", line_number);
            if (flag) flags |= flag_bits[index];
        }
        /* The offset of hex (x, y) is (col x, row y): the cell is y * width + x. */
        u32 cell = value[MAP_COLUMN_Y] * map->width + value[MAP_COLUMN_X];
        if (layers->flags[cell] & SIM_MAP_SEEN) return sim_map_result(false, "a hex has two rows", line_number);
        terrain_kind *terrain = sim->content.terrains + value[MAP_COLUMN_TERRAIN];
        map->kind[cell]      = (u8)value[MAP_COLUMN_TERRAIN];
        map->cost[cell]      = terrain->cost[MOVEMENT_FOOT];
        map->level[cell]     = (u8)(value[MAP_COLUMN_HEIGHT] / SIM_MAP_HEIGHT_STEP);
        layers->region[cell] = (u8)value[MAP_COLUMN_REGION];
        layers->trap[cell]   = (u8)value[MAP_COLUMN_TRAP];
        layers->flags[cell]  = (u8)(flags | SIM_MAP_SEEN);
        layers->height[cell] = (u8)value[MAP_COLUMN_HEIGHT];
        rows++;
    }
    for (u32 cell = 0; cell < cells; ++cell) layers->flags[cell] = (u8)(layers->flags[cell] & ~SIM_MAP_SEEN & 0xFFu);
    if (rows != cells) return sim_map_result(false, "hexes.tsv does not have one row for each hex", line_number);
    map->version++;
    return sim_map_result(true, "ok", 0);
}

/* ---- the bases ----------------------------------------------------------------- */

internal sim_map_load_result
sim_map_load_bases(sim_state *sim, tsv_table *table)
{
    sim_map_layers *layers = &sim->map_layers;
    for (u32 region = 0; region < SIM_MAP_REGION_CAPACITY; ++region)
    {
        layers->base_type[region] = MAP_BASE_NONE;
        layers->base_city[region] = SIM_NO_CITY;
        layers->base_hex[region]  = hex_make(0, 0);
    }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        u32 id = 0, x = 0, y = 0, city = 0;
        if (!tsv_cell_u32(table, row, "id", &id) || !tsv_cell_u32(table, row, "x", &x) || !tsv_cell_u32(table, row, "y", &y) ||
            !tsv_cell_u32(table, row, "city", &city))
        {
            return sim_map_result(false, "bases.tsv has a bad cell", row + 2);
        }
        const char *kind = tsv_cell(table, row, "kind");
        u8 type = strcmp(kind, "city") == 0 ? (u8)MAP_BASE_CITY : strcmp(kind, "gate") == 0 ? (u8)MAP_BASE_GATE :
                  strcmp(kind, "port") == 0 ? (u8)MAP_BASE_PORT : (u8)MAP_BASE_NONE;
        if (id >= SIM_MAP_REGION_CAPACITY || city >= SAVE_CITY_COUNT || type == MAP_BASE_NONE) return sim_map_result(false, "bases.tsv has a bad base", row + 2);
        if ((type == MAP_BASE_CITY) != (id < SAVE_CITY_COUNT) || (type == MAP_BASE_CITY && city != id))
        {
            return sim_map_result(false, "a city of bases.tsv is not its own city", row + 2);
        }
        if (layers->base_type[id] != MAP_BASE_NONE) return sim_map_result(false, "bases.tsv has a base two times", row + 2);
        hex_offset offset;
        offset.col = (s32)x;
        offset.row = (s32)y;
        hex_coord hex = hex_from_offset(offset);
        if (!hex_map_contains(&sim->map, hex)) return sim_map_result(false, "a base is off the map", row + 2);
        layers->base_type[id] = type;
        layers->base_city[id] = (u8)city;
        layers->base_hex[id]  = hex;
    }
    return sim_map_result(true, "ok", 0);
}

/* ---- the rules of the load (F-151) ------------------------------------------------ */

internal u32
sim_map_cell(sim_state *sim, hex_coord hex)
{
    return hex_map_contains(&sim->map, hex) ? hex_map_index(&sim->map, hex) : U32_MAX;
}

/* The city of a region: the city itself, or the city of its gate or port. */
internal u8
sim_map_region_city(sim_state *sim, u8 region)
{
    sim_map_layers *layers = &sim->map_layers;
    if (region >= SIM_MAP_REGION_CAPACITY || layers->base_type[region] == MAP_BASE_NONE) return SIM_NO_CITY;
    return layers->base_city[region];
}

/* Two steps:
   - At the start of a game, each side hex of a gate (terrain 關所 that is
     not the hex of the gate) becomes 崖 cliff. Only the gate hex stays
     passable (map-data.md).
   - A slot belongs to the city of its region. A slot in the region of a gate
     or a port belongs to the city of that base (terrain-and-map.md). */
internal sim_map_load_result
sim_map_finish(sim_state *sim)
{
    sim_map_layers *layers = &sim->map_layers;
    hex_map *map = &sim->map;
    layers->city_kind  = content_find_terrain(&sim->content, "city");
    layers->port_kind  = content_find_terrain(&sim->content, "port");
    layers->gate_kind  = content_find_terrain(&sim->content, "gate");
    layers->cliff_kind = content_find_terrain(&sim->content, "cliff");
    if (layers->city_kind == CONTENT_NO_TERRAIN || layers->port_kind == CONTENT_NO_TERRAIN ||
        layers->gate_kind == CONTENT_NO_TERRAIN || layers->cliff_kind == CONTENT_NO_TERRAIN)
    {
        return sim_map_result(false, "terrain.tsv lacks city, port, gate or cliff", 0);
    }

    layers->gate_side_cliffs = 0;
    u32 cells = map->width * map->height;
    for (u32 cell = 0; cell < cells; ++cell)
    {
        u8 region = layers->region[cell];
        if (map->kind[cell] != layers->gate_kind || region >= SIM_MAP_REGION_CAPACITY || layers->base_type[region] != MAP_BASE_GATE) continue;
        if (cell == hex_map_index(map, layers->base_hex[region])) continue;
        map->kind[cell] = layers->cliff_kind;
        map->cost[cell] = sim->content.terrains[layers->cliff_kind].cost[MOVEMENT_FOOT];
        layers->gate_side_cliffs++;
    }

    for (u32 city = 0; city < SAVE_CITY_COUNT; ++city) layers->slot_count[city] = 0;
    for (u32 x = 0; x < map->width; ++x)
    {
        for (u32 y = 0; y < map->height; ++y)
        {
            u32 cell = y * map->width + x;
            if (!(layers->flags[cell] & SIM_MAP_FLAG_SLOT)) continue;
            u8 city = sim_map_region_city(sim, layers->region[cell]);
            if (city == SIM_NO_CITY) return sim_map_result(false, "a slot is in a region without a city", 0);
            if (layers->slot_count[city] >= SIM_SLOT_CAPACITY) return sim_map_result(false, "a city has too many slots", 0);
            layers->slot_hex[city][layers->slot_count[city]++] = hex_map_coord(map, cell);
        }
    }
    layers->loaded = true;
    map->version++;
    return sim_map_result(true, "ok", 0);
}

/* ---- queries for the rules ------------------------------------------------------- */

internal b32
sim_map_has_flag(sim_state *sim, hex_coord hex, u32 flag)
{
    u32 cell = sim->map_layers.loaded ? sim_map_cell(sim, hex) : U32_MAX;
    return cell != U32_MAX && (sim->map_layers.flags[cell] & flag) != 0;
}

/* The six neighbours of the hex that are slots of the same city (the
   column "slot_neighbours" of slots.tsv). The mint and the granary give
   their bonus to these neighbours. */
internal u32
sim_map_slot_neighbours(sim_state *sim, u8 city_id, hex_coord hex)
{
    u32 count = 0;
    for (u32 direction = 0; direction < HEX_DIRECTION_COUNT; ++direction)
    {
        hex_coord next = hex_neighbor(hex, direction);
        if (!sim_map_has_flag(sim, next, SIM_MAP_FLAG_SLOT)) continue;
        if (sim_map_region_city(sim, sim->map_layers.region[sim_map_cell(sim, next)]) == city_id) count++;
    }
    return count;
}

/* The bandit sites that bandits or a tribe near the city can use: empty,
   no fire, in a region of the city (the city, its gates and its ports), up
   to SIM_MAP_BANDIT_CANDIDATES in the order of the scan (terrain-and-map.md). */
internal u32
sim_map_bandit_sites(sim_state *sim, u8 city_id, hex_coord *out, u32 capacity)
{
    sim_map_layers *layers = &sim->map_layers;
    if (!layers->loaded) return 0;
    u32 limit = capacity < SIM_MAP_BANDIT_CANDIDATES ? capacity : SIM_MAP_BANDIT_CANDIDATES;
    u32 count = 0;
    hex_map *map = &sim->map;
    for (u32 x = 0; x < map->width && count < limit; ++x)
    {
        for (u32 y = 0; y < map->height && count < limit; ++y)
        {
            u32 cell = y * map->width + x;
            if (!(layers->flags[cell] & SIM_MAP_FLAG_BANDIT) || sim_map_region_city(sim, layers->region[cell]) != city_id) continue;
            hex_coord hex = hex_map_coord(map, cell);
            if (sim_unit_at(sim, hex) != SIM_NO_UNIT || sim_building_at(sim, hex) != SIM_NO_BUILDING || sim_hex_burns(sim, hex)) continue;
            out[count++] = hex;
        }
    }
    return count;
}

/* True if the hex is in the flood zone of the dike: a flood hex of the
   region of the dike (map byte 8). */
internal b32
sim_map_in_flood(sim_state *sim, hex_coord dike, hex_coord hex)
{
    u32 dike_cell = sim->map_layers.loaded ? sim_map_cell(sim, dike) : U32_MAX;
    u32 cell      = sim->map_layers.loaded ? sim_map_cell(sim, hex) : U32_MAX;
    if (dike_cell == U32_MAX || cell == U32_MAX) return false;
    return (sim->map_layers.flags[cell] & SIM_MAP_FLAG_FLOOD) && sim->map_layers.region[cell] == sim->map_layers.region[dike_cell];
}
