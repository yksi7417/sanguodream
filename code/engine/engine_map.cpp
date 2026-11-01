/* The engine_api of M10 (the real map): the load of map\hexes.tsv and
   map\bases.tsv of the content pack, the natural traps on their sites,
   and the queries of the map data and of the movement cost table
   (formats/map-data.md, terrain-and-map.md; F-077, F-147 to F-151).
   Included by engine.cpp, before engine_make_api. */

/* Reads map\hexes.tsv of the pack into the transient arena and measures it.
   Returns 0 if the pack has no map or the text is bad. */
internal char *
engine_read_map_text(engine *e, const char *folder, u32 *width, u32 *height)
{
    if (!e->read_entire_file || !folder || !folder[0]) return 0;
    char path[260];
    snprintf(path, sizeof(path), "%s/map/hexes.tsv", folder);
    read_file_result file = e->read_entire_file(path, e->transient);
    if (!file.contents) return 0;
    char *text = (char *)arena_push_size(e->transient, file.size + 1, 1);
    memcpy(text, file.contents, (size_t)file.size);
    text[file.size] = 0;
    return sim_map_measure_hexes(text, width, height) ? text : 0;
}

/* Fills the counts of engine_map_info from the loaded layers. */
internal void
engine_count_map(engine *e)
{
    sim_state *sim = e->sim;
    sim_map_layers *layers = &sim->map_layers;
    engine_map_info *info = &e->map_info;
    info->width  = sim->map.width;
    info->height = sim->map.height;
    if (!layers->loaded) return;
    info->gate_side_cliffs = layers->gate_side_cliffs;
    for (u32 region = 0; region < SIM_MAP_REGION_CAPACITY; ++region)
    {
        if (layers->base_type[region] == MAP_BASE_CITY) info->cities++;
        if (layers->base_type[region] == MAP_BASE_GATE) info->gates++;
        if (layers->base_type[region] == MAP_BASE_PORT) info->ports++;
    }
    for (u32 city = 0; city < SAVE_CITY_COUNT; ++city) info->slots += layers->slot_count[city];
    for (u32 cell = 0; cell < sim->map.width * sim->map.height; ++cell)
    {
        u8 kind = sim->map.kind[cell], flags = layers->flags[cell], region = layers->region[cell];
        if (kind == layers->city_kind) info->city_hexes++;
        if (kind == layers->port_kind) info->port_hexes++;
        if (kind == layers->gate_kind) info->gate_hexes++;
        if (layers->trap[cell] == SIM_MAP_TRAP_DIKE)     info->dikes++;
        if (layers->trap[cell] == SIM_MAP_TRAP_ROCKFALL) info->rockfalls++;
        if (flags & SIM_MAP_FLAG_FLOOD)   info->flood_hexes++;
        if (flags & SIM_MAP_FLAG_BANDIT)  info->bandit_sites++;
        if (flags & SIM_MAP_FLAG_EVENT)   info->event_sites++;
        if (flags & SIM_MAP_FLAG_UNKNOWN) info->unknown_flags++;
        if ((flags & SIM_MAP_FLAG_SLOT) && region < SIM_MAP_REGION_CAPACITY && layers->base_type[region] == MAP_BASE_PORT) info->port_region_slots++;
    }
}

/* Loads the map of the pack after the content: the terrain table gives the
   cost of each kind. If a step fails, the engine keeps the placeholder map
   and map_info says why. */
internal b32
engine_load_pack_map(engine *e, const char *folder, const char *hexes_text)
{
    sim_state *sim = e->sim;
    sim_map_load_result result = sim_map_load_hexes(sim, hexes_text);
    if (result.ok)
    {
        arena_reset(&e->scratch);
        tsv_table bases = {};
        if (!engine_read_table(e, folder, "map/bases.tsv", &bases)) result = sim_map_result(false, "map/bases.tsv not found", 0);
        else                                                       result = sim_map_load_bases(sim, &bases);
        arena_reset(&e->scratch);
    }
    if (result.ok) result = sim_map_finish(sim);
    e->map_info.detail = result.detail;
    if (!result.ok)
    {
        sim->map_layers.loaded = false;
        return false;
    }
    e->map_info.real_map = true;
    sim_init_sites(sim);   /* the cities stand on the centres and the slots on the 開發地 of the map */
    engine_count_map(e);
    return true;
}

/* F-150: at the start a dike stands on each dike hex and a rockfall on
   each rockfall site of the map. The dike belongs to the city of its region. */
internal void
engine_place_map_traps(engine *e, u8 dike, u8 rockfall)
{
    sim_state *sim = e->sim;
    sim_map_layers *layers = &sim->map_layers;
    for (u32 cell = 0; cell < sim->map.width * sim->map.height; ++cell)
    {
        u8 trap = layers->trap[cell];
        u8 kind = trap == SIM_MAP_TRAP_DIKE ? dike : trap == SIM_MAP_TRAP_ROCKFALL ? rockfall : (u8)CONTENT_NO_BUILDING;
        if (kind == CONTENT_NO_BUILDING) continue;
        u8 city = trap == SIM_MAP_TRAP_DIKE ? sim_map_region_city(sim, layers->region[cell]) : (u8)SIM_NO_CITY;
        sim_add_building(sim, kind, (u8)SIM_NO_FORCE, hex_map_coord(&sim->map, cell), sim->content.buildings[kind].durability, city);
    }
}

/* ---- engine_api ------------------------------------------------------------------ */

internal engine_map_info
engine_api_map_info(void *opaque)
{
    engine *e = (engine *)opaque;
    engine_map_info info = e->map_info;
    info.width  = e->sim->map.width;
    info.height = e->sim->map.height;
    return info;
}

internal engine_hex
engine_api_hex_at(void *opaque, s32 q, s32 r)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_hex result = {};
    result.region      = ENGINE_NO_REGION;
    result.region_city = ENGINE_NO_CITY;
    result.city_id     = ENGINE_NO_CITY;
    result.base_id     = ENGINE_NO_BASE;
    result.slot_city   = ENGINE_NO_CITY;
    result.facility    = ENGINE_NO_FACILITY;
    result.building_id = ENGINE_NO_BUILDING;
    result.building_kind = ENGINE_NO_ROW;
    result.unit_id     = ENGINE_NO_UNIT;
    hex_coord hex = hex_make(q, r);
    if (!hex_map_contains(&sim->map, hex)) return result;
    u32 cell = hex_map_index(&sim->map, hex);
    result.on_map     = true;
    result.terrain    = sim->map.kind[cell];
    result.fire_turns = sim->fire ? sim->fire[cell] & SIM_FIRE_TURNS_MASK : 0;
    result.city_id    = sim_city_at(sim, hex);
    u32 base = sim_base_at(sim, hex);
    if (base != SIM_NO_BASE) result.base_id = sim->content.bases[base].id;
    /* M11: the facility, the building and the unit on the hex. */
    u32 building = sim_building_at(sim, hex);
    if (building != SIM_NO_BUILDING) { result.building_id = building; result.building_kind = sim->buildings[building].kind; }
    u32 unit = sim_unit_at(sim, hex);
    if (unit != SIM_NO_UNIT) result.unit_id = unit;
    u32 slot_index = 0;
    u8 slot_city = sim_slot_at(sim, hex, &slot_index);
    if (slot_city != SIM_NO_CITY)
    {
        sim_slot *slot = sim->sites[slot_city].slots + slot_index;
        result.slot_city  = slot_city;
        result.slot_index = (u8)slot_index;
        if (slot->state != SIM_SLOT_EMPTY && slot->facility < sim->content.facility_count)
        {
            result.facility       = slot->facility;
            result.facility_level = slot->level;
        }
    }
    sim_map_layers *layers = &sim->map_layers;
    if (!layers->loaded) return result;
    u8 flags = layers->flags[cell];
    result.region       = layers->region[cell];
    result.region_city  = sim_map_region_city(sim, layers->region[cell]);
    result.trap         = layers->trap[cell];
    result.height       = layers->height[cell];
    result.slot         = (flags & SIM_MAP_FLAG_SLOT) != 0;
    result.bandit_site  = (flags & SIM_MAP_FLAG_BANDIT) != 0;
    result.flood        = (flags & SIM_MAP_FLAG_FLOOD) != 0;
    result.event_site   = (flags & SIM_MAP_FLAG_EVENT) != 0;
    result.unknown_flag = (flags & SIM_MAP_FLAG_UNKNOWN) != 0;
    return result;
}

internal u32
engine_api_armament_count(void *opaque)
{
    return ((engine *)opaque)->sim->content.armament_count;
}

internal engine_armament
engine_api_armament_at(void *opaque, u32 index)
{
    sim_content *content = &((engine *)opaque)->sim->content;
    engine_armament result = {};
    result.id = ENGINE_NO_ARMAMENT;
    if (index >= content->armament_count) return result;
    armament_kind *kind = content->armaments + index;
    result.id   = kind->id;
    result.move = kind->move;
    memcpy(result.key, kind->key, sizeof(result.key));
    for (u32 terrain = 0; terrain < CONTENT_TERRAIN_CAPACITY; ++terrain) result.cost[terrain] = kind->cost[terrain];
    return result;
}

/* The cost for a unit of the type and the faction to enter the hex now:
   the row of its armament, fire times 4, blocked by units, enemy cities
   and enemy bases (F-148). */
internal u32
engine_api_move_cost(void *opaque, u8 unit_type, u8 force_id, s32 q, s32 r)
{
    sim_state *sim = ((engine *)opaque)->sim;
    unit_type_kind *type = unit_type < sim->content.unit_type_count ? sim->content.unit_types + unit_type : 0;
    movement_class movement = type ? type->movement : MOVEMENT_FOOT;
    hex_coord hex = hex_make(q, r);
    if (!hex_map_contains(&sim->map, hex)) return 0;
    return sim_entry_cost_of(sim, force_id, movement, unit_type, hex, SIM_NO_UNIT, false);
}

internal engine_bandit_sites
engine_api_bandit_sites(void *opaque, u8 city_id)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_bandit_sites result = {};
    hex_coord sites[ENGINE_BANDIT_SITE_CAPACITY];
    result.count = sim_map_bandit_sites(sim, city_id, sites, ArrayCount(sites));
    for (u32 index = 0; index < result.count; ++index)
    {
        result.q[index] = sites[index].q;
        result.r[index] = sites[index].r;
    }
    return result;
}

internal void
engine_api_start_fire(void *opaque, s32 q, s32 r, u32 turns)
{
    engine *e = (engine *)opaque;
    hex_coord hex = hex_make(q, r);
    if (!hex_map_contains(&e->sim->map, hex)) return;
    arena_reset(&e->scratch);
    sim_event_sink sink = sim_sink_from_arena(&e->scratch, ENGINE_SINK_CAPACITY);   /* the events go nowhere: a test command */
    sim_start_fire(e->sim, hex, turns, false, 0, &sink);
}

/* M21: the region of each hex and the city of each region. Without the
   map layers (the placeholder board) the view has no regions. */
internal engine_region_view
engine_api_region_view(void *opaque)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_region_view view = {};
    view.width  = sim->map.width;
    view.height = sim->map.height;
    for (u32 region = 0; region < ENGINE_REGION_CAPACITY; ++region) view.region_city[region] = ENGINE_NO_CITY;
    if (!sim->map_layers.loaded) return view;
    view.region = sim->map_layers.region;
    for (u32 region = 0; region < ENGINE_REGION_CAPACITY; ++region)
    {
        u8 city = sim_map_region_city(sim, (u8)region);
        view.region_city[region] = city == SIM_NO_CITY ? ENGINE_NO_CITY : city;
    }
    return view;
}

internal void
engine_bind_map_api(engine_api *api)
{
    api->region_view    = engine_api_region_view;
    api->map_info       = engine_api_map_info;
    api->hex_at         = engine_api_hex_at;
    api->armament_count = engine_api_armament_count;
    api->armament_at    = engine_api_armament_at;
    api->move_cost      = engine_api_move_cost;
    api->bandit_sites   = engine_api_bandit_sites;
    api->start_fire     = engine_api_start_fire;
}
