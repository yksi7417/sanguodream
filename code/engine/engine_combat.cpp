/* The engine_api of M6 (combat): the tactic, plot and building tables, the
   buildings and the fire on the map, the siege state of a city, and the
   commands 攻擊, 共同攻擊, 戰法, 計略, 設置, 修補 and 撤除. Included by
   engine.cpp, before engine_make_api. */

/* ---- the natural traps of the placeholder map ------------------------------ */

internal u8
engine_find_building_kind(sim_content *content, const char *key)
{
    for (u32 index = 0; index < content->building_count; ++index)
    {
        if (strcmp(content->buildings[index].key, key) == 0) return (u8)index;
    }
    return CONTENT_NO_BUILDING;
}

/* The first hex at the distance from the city where a land unit can walk
   and no building stands, in the order of the hex directions. */
internal b32
engine_trap_hex(sim_state *sim, u32 city_id, s32 distance, hex_coord *out)
{
    hex_coord center = sim->sites[city_id].hex;
    for (u32 direction = 0; direction < HEX_DIRECTION_COUNT; ++direction)
    {
        hex_coord hex = center;
        for (s32 step = 0; step < distance; ++step) hex = hex_neighbor(hex, direction);
        terrain_kind *terrain = sim_terrain_at(sim, hex);
        if (!terrain || !terrain->cost[MOVEMENT_FOOT]) continue;
        if (sim_building_at(sim, hex) != SIM_NO_BUILDING || sim_city_at(sim, hex) != SIM_NO_CITY) continue;
        *out = hex;
        return true;
    }
    return false;
}

/* PLACEHOLDER (Q-034): the spec names natural traps but has no map. A dike
   stands 3 hexes from four cities (it floods that city when it breaks) and
   a rockfall stands near 漢中 and 梓潼 on the mountain road. */
internal void
engine_place_natural_traps(engine *e)
{
    sim_state *sim = e->sim;
    u8 dike = engine_find_building_kind(&sim->content, "dike");
    u8 rockfall = engine_find_building_kind(&sim->content, "rockfall");
    if (sim->map_layers.loaded)
    {
        engine_place_map_traps(e, dike, rockfall);   /* M10: the 4 dikes and the 18 rockfall sites of the map */
        return;
    }
    local_persist const u8 dike_cities[] = {0, 6, 8, 10};
    local_persist const u8 rockfall_cities[] = {35, 36};
    hex_coord hex = {};
    for (u32 index = 0; dike != CONTENT_NO_BUILDING && index < ArrayCount(dike_cities); ++index)
    {
        if (engine_trap_hex(sim, dike_cities[index], 3, &hex))
        {
            sim_add_building(sim, dike, (u8)SIM_NO_FORCE, hex, sim->content.buildings[dike].durability, dike_cities[index]);
        }
    }
    for (u32 index = 0; rockfall != CONTENT_NO_BUILDING && index < ArrayCount(rockfall_cities); ++index)
    {
        if (engine_trap_hex(sim, rockfall_cities[index], 3, &hex))
        {
            sim_add_building(sim, rockfall, (u8)SIM_NO_FORCE, hex, sim->content.buildings[rockfall].durability, SIM_NO_CITY);
        }
    }
}

/* ---- the tables -------------------------------------------------------------- */

internal u32
engine_api_tactic_kind_count(void *opaque)
{
    return ((engine *)opaque)->sim->content.tactic_count;
}

internal engine_tactic_kind
engine_api_tactic_kind_at(void *opaque, u32 index)
{
    sim_content *content = &((engine *)opaque)->sim->content;
    engine_tactic_kind result = {};
    result.id = ENGINE_NO_ROW;
    if (index >= content->tactic_count) return result;
    tactic_kind *kind = content->tactics + index;
    result.id             = kind->id;
    memcpy(result.key, kind->key, sizeof(result.key));
    result.unit_types     = kind->unit_types;
    result.aptitude       = (u8)kind->aptitude;
    result.morale         = kind->morale;
    result.range_min      = kind->range_min;
    result.range_max      = kind->range_max;
    result.pattern        = (u8)kind->pattern;
    result.effect         = (u8)kind->effect;
    result.damage_percent = kind->damage_percent;
    result.always_success = kind->always_success;
    return result;
}

internal u32
engine_api_plot_kind_count(void *opaque)
{
    return ((engine *)opaque)->sim->content.plot_count;
}

internal engine_plot_kind
engine_api_plot_kind_at(void *opaque, u32 index)
{
    sim_content *content = &((engine *)opaque)->sim->content;
    engine_plot_kind result = {};
    result.id = ENGINE_NO_ROW;
    if (index >= content->plot_count) return result;
    plot_kind *kind = content->plots + index;
    result.id     = kind->id;
    memcpy(result.key, kind->key, sizeof(result.key));
    result.morale = kind->morale;
    result.range  = kind->range;
    result.target = (u8)kind->target;
    result.effect = (u8)kind->effect;
    result.turns  = kind->turns;
    return result;
}

internal u32
engine_api_building_kind_count(void *opaque)
{
    return ((engine *)opaque)->sim->content.building_count;
}

internal engine_building_kind
engine_api_building_kind_at(void *opaque, u32 index)
{
    sim_content *content = &((engine *)opaque)->sim->content;
    engine_building_kind result = {};
    result.id = ENGINE_NO_ROW;
    if (index >= content->building_count) return result;
    building_kind *kind = content->buildings + index;
    result.id         = kind->id;
    memcpy(result.key, kind->key, sizeof(result.key));
    result.type       = (u8)kind->type;
    result.durability = kind->durability;
    result.gold       = kind->gold;
    result.technique  = kind->technique;
    result.range_min  = kind->range_min;
    result.range_max  = kind->range_max;
    result.natural    = kind->natural;
    return result;
}

/* ---- the map: buildings, fire, siege ------------------------------------------ */

internal u32
engine_api_building_count(void *opaque)
{
    return ((engine *)opaque)->sim->building_count;
}

internal engine_building
engine_api_building_at(void *opaque, u32 building_id)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_building result = {};
    result.id = ENGINE_NO_BUILDING;
    if (building_id >= sim->building_count || !sim->buildings[building_id].alive) return result;
    sim_building *building = sim->buildings + building_id;
    result.id             = building_id;
    result.kind           = building->kind;
    result.force_id       = building->force_id;
    result.q              = building->hex.q;
    result.r              = building->hex.r;
    result.durability     = building->durability;
    result.durability_max = sim->content.buildings[building->kind].durability;
    result.complete       = building->complete;
    result.city_id        = building->city_id;
    return result;
}

internal u32
engine_api_building_at_hex(void *opaque, s32 q, s32 r)
{
    u32 id = sim_building_at(((engine *)opaque)->sim, hex_make(q, r));
    return id == SIM_NO_BUILDING ? ENGINE_NO_BUILDING : id;
}

internal engine_fire_view
engine_api_fire_view(void *opaque)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_fire_view view = {};
    view.width  = sim->map.width;
    view.height = sim->map.height;
    view.turns  = sim->fire;
    return view;
}

internal engine_city_siege
engine_api_city_siege(void *opaque, u8 city_id)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_city_siege result = {};
    result.city_id = ENGINE_NO_CITY;
    city_component *city = sim_find_city(sim, city_id);
    if (!city) return result;
    result.city_id        = city_id;
    result.durability     = city->durability;
    result.durability_max = sim_city_durability_max(sim, city);
    result.soldiers       = city->troops;
    u32 enemies = 0, free_cells = 0;
    for (u32 direction = 0; direction < HEX_DIRECTION_COUNT; ++direction)
    {
        hex_coord hex = hex_neighbor(sim->sites[city_id].hex, direction);
        u32 unit_id = sim_unit_at(sim, hex);
        if (unit_id != SIM_NO_UNIT && sim->units[unit_id].force_id != city->owner_force_id) ++enemies;
        else if (sim_entry_cost(sim, city->owner_force_id, MOVEMENT_FOOT, hex, SIM_NO_UNIT)) ++free_cells;
    }
    result.surrounded = enemies > 0 && free_cells == 0;
    return result;
}

/* ---- the commands ---------------------------------------------------------------- */

internal engine_command_result
engine_api_command_attack(void *opaque, u32 unit_id, s32 q, s32 r)
{
    return engine_run_unit_order((engine *)opaque, SIM_COMMAND_ATTACK, engine_unit_order(unit_id, q, r, ENGINE_NO_UNIT, 0, 0, 0));
}

internal engine_command_result
engine_api_command_joint_attack(void *opaque, u32 unit_id, s32 q, s32 r)
{
    return engine_run_unit_order((engine *)opaque, SIM_COMMAND_JOINT_ATTACK, engine_unit_order(unit_id, q, r, ENGINE_NO_UNIT, 0, 0, 0));
}

internal engine_command_result
engine_run_row_order(engine *e, sim_command_kind kind, u32 unit_id, u8 row, s32 q, s32 r)
{
    sim_unit_order order = engine_unit_order(unit_id, q, r, ENGINE_NO_UNIT, 0, 0, 0);
    order.kind = row;
    return engine_run_unit_order(e, kind, order);
}

internal engine_command_result
engine_api_command_tactic(void *opaque, u32 unit_id, u8 row, s32 q, s32 r)
{
    return engine_run_row_order((engine *)opaque, SIM_COMMAND_TACTIC, unit_id, row, q, r);
}

internal engine_command_result
engine_api_command_plot(void *opaque, u32 unit_id, u8 row, s32 q, s32 r)
{
    return engine_run_row_order((engine *)opaque, SIM_COMMAND_PLOT, unit_id, row, q, r);
}

internal engine_command_result
engine_api_command_build(void *opaque, u32 unit_id, u8 row, s32 q, s32 r)
{
    return engine_run_row_order((engine *)opaque, SIM_COMMAND_BUILD, unit_id, row, q, r);
}

internal engine_command_result
engine_api_command_repair(void *opaque, u32 unit_id, s32 q, s32 r)
{
    return engine_run_unit_order((engine *)opaque, SIM_COMMAND_REPAIR, engine_unit_order(unit_id, q, r, ENGINE_NO_UNIT, 0, 0, 0));
}

internal engine_command_result
engine_api_command_dismantle(void *opaque, u32 unit_id, s32 q, s32 r)
{
    return engine_run_unit_order((engine *)opaque, SIM_COMMAND_DISMANTLE, engine_unit_order(unit_id, q, r, ENGINE_NO_UNIT, 0, 0, 0));
}

/* ---- test and editor commands ------------------------------------------------------ */

internal u32
engine_api_place_building(void *opaque, u8 kind, u8 force_id, s32 q, s32 r)
{
    sim_state *sim = ((engine *)opaque)->sim;
    hex_coord hex = hex_make(q, r);
    if (kind >= sim->content.building_count || !hex_map_contains(&sim->map, hex)) return ENGINE_NO_BUILDING;
    if (sim_building_at(sim, hex) != SIM_NO_BUILDING) return ENGINE_NO_BUILDING;
    u32 id = sim_add_building(sim, kind, force_id, hex, sim->content.buildings[kind].durability, SIM_NO_CITY);
    return id == SIM_NO_BUILDING ? ENGINE_NO_BUILDING : id;
}

internal void
engine_api_set_unit(void *opaque, u32 unit_id, u32 soldiers, u32 morale)
{
    sim_unit *unit = sim_find_unit(((engine *)opaque)->sim, unit_id);
    if (!unit) return;
    unit->soldiers = soldiers;
    unit->morale   = morale;
}

/* M15: a test command; the grain of the unit becomes known. */
internal void
engine_api_set_unit_grain(void *opaque, u32 unit_id, u32 grain)
{
    sim_unit *unit = sim_find_unit(((engine *)opaque)->sim, unit_id);
    if (!unit) return;
    unit->grain         = grain;
    unit->grain_unknown = false;
}

/* An editor command: puts the unit on a free hex, for the faction. */
internal b32
engine_api_set_unit_place(void *opaque, u32 unit_id, u8 force_id, s32 q, s32 r)
{
    sim_state *sim = ((engine *)opaque)->sim;
    sim_unit *unit = sim_find_unit(sim, unit_id);
    hex_coord hex = hex_make(q, r);
    if (!unit || force_id >= SIM_FORCE_CAPACITY || !hex_map_contains(&sim->map, hex)) return false;
    u32 other = sim_unit_at(sim, hex);
    if ((other != SIM_NO_UNIT && other != unit_id) || sim_city_at(sim, hex) != SIM_NO_CITY || sim_building_at(sim, hex) != SIM_NO_BUILDING) return false;
    unit->hex      = hex;
    unit->force_id = force_id;
    return true;
}

internal void
engine_api_set_city_durability(void *opaque, u8 city_id, u32 durability)
{
    sim_state *sim = ((engine *)opaque)->sim;
    city_component *city = sim_find_city(sim, city_id);
    if (!city) return;
    u32 maximum = sim_city_durability_max(sim, city);
    city->durability = durability < maximum ? durability : maximum;
}

internal void
engine_bind_combat_api(engine_api *api)
{
    api->tactic_kind_count    = engine_api_tactic_kind_count;
    api->tactic_kind_at       = engine_api_tactic_kind_at;
    api->plot_kind_count      = engine_api_plot_kind_count;
    api->plot_kind_at         = engine_api_plot_kind_at;
    api->building_kind_count  = engine_api_building_kind_count;
    api->building_kind_at     = engine_api_building_kind_at;
    api->building_count       = engine_api_building_count;
    api->building_at          = engine_api_building_at;
    api->building_at_hex      = engine_api_building_at_hex;
    api->fire_view            = engine_api_fire_view;
    api->city_siege           = engine_api_city_siege;
    api->command_attack       = engine_api_command_attack;
    api->command_joint_attack = engine_api_command_joint_attack;
    api->command_tactic       = engine_api_command_tactic;
    api->command_plot         = engine_api_command_plot;
    api->command_build        = engine_api_command_build;
    api->command_repair       = engine_api_command_repair;
    api->command_dismantle    = engine_api_command_dismantle;
    api->place_building       = engine_api_place_building;
    api->set_unit             = engine_api_set_unit;
    api->set_unit_place       = engine_api_set_unit_place;
    api->set_unit_grain       = engine_api_set_unit_grain;
    api->set_city_durability  = engine_api_set_city_durability;
}
