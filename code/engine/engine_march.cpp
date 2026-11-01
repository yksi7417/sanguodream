/* The engine_api of M5 (march): the placeholder map from terrain.tsv, the
   terrain and unit tables, the units, the move range, and the commands
   出征, 輸送, 移動, 待命, 退卻 and 補給. Included by engine.cpp, before
   engine_make_api. */

/* ---- the placeholder map --------------------------------------------------- */

/* The cells of a straight line between two hexes, both ends included. */
internal u32
engine_hex_line(hex_coord from, hex_coord to, hex_coord *out, u32 capacity)
{
    s32 steps = hex_distance(from, to);
    u32 count = 0;
    for (s32 step = 0; step <= steps && count < capacity; ++step)
    {
        f32 t = steps ? (f32)step / (f32)steps : 0.0f;
        f32 q = (f32)from.q + (f32)(to.q - from.q) * t + 1e-4f;
        f32 r = (f32)from.r + (f32)(to.r - from.r) * t + 1e-4f;
        out[count++] = hex_round(q, r);
    }
    return count;
}

internal void
engine_set_kind(sim_state *sim, hex_coord hex, u8 kind)
{
    if (!hex_map_contains(&sim->map, hex) || kind >= sim->content.terrain_count) return;
    u32 index = hex_map_index(&sim->map, hex);
    terrain_kind *terrain = sim->content.terrains + kind;
    sim->map.kind[index]  = kind;
    sim->map.level[index] = terrain->level;
    sim->map.cost[index]  = terrain->cost[MOVEMENT_FOOT];
}

/* A road between two cities: main road, or a special road of the spec
   between two cities of the west (terrain-and-map.md: plank roads near 漢中
   and 梓潼, a small path near 成都, a poison spring near 建寧 and 雲南).
   A road over water is a ford. */
internal void
engine_road(sim_state *sim, u32 city_a, u32 city_b, u8 road, u8 ford, u8 middle)
{
    hex_coord cells[64];
    u32 count = engine_hex_line(sim->sites[city_a].hex, sim->sites[city_b].hex, cells, ArrayCount(cells));
    for (u32 index = 1; index + 1 < count; ++index)
    {
        if (!hex_map_contains(&sim->map, cells[index])) continue;
        u8 kind = sim->map.kind[hex_map_index(&sim->map, cells[index])];
        terrain_kind *terrain = kind < sim->content.terrain_count ? sim->content.terrains + kind : 0;
        b32 water = terrain && terrain->cost[MOVEMENT_FOOT] == 0 && terrain->cost[MOVEMENT_NAVY] != 0;
        u8 put = water ? ford : road;
        if (middle != CONTENT_NO_TERRAIN && index == count / 2) put = middle;
        engine_set_kind(sim, cells[index], put);
    }
}

/* PLACEHOLDER MAP (Q-034). The spec has no map data. A hash of each cell
   picks a terrain of terrain.tsv; the city hexes and their slots are firm
   ground; roads link the cities that are neighbours on the grid of the
   placeholder layout. It does not touch the PRNG of the sim. */
internal void
engine_content_terrain(engine *e)
{
    sim_state *sim = e->sim;
    sim_content *content = &sim->content;
    u8 grass = content_find_terrain(content, "grass"), forest = content_find_terrain(content, "forest");
    u8 waste = content_find_terrain(content, "waste"), cliff = content_find_terrain(content, "cliff");
    u8 river = content_find_terrain(content, "river"), swamp = content_find_terrain(content, "swamp");
    u8 sand = content_find_terrain(content, "sand"), earth = content_find_terrain(content, "earth");
    u8 road = content_find_terrain(content, "main_road"), ford = content_find_terrain(content, "ford");
    u8 plank = content_find_terrain(content, "plank_road"), path = content_find_terrain(content, "small_path");
    u8 poison = content_find_terrain(content, "poison_spring"), city = content_find_terrain(content, "city");
    if (grass == CONTENT_NO_TERRAIN || road == CONTENT_NO_TERRAIN || city == CONTENT_NO_TERRAIN) return;
    hex_map *map = &sim->map;
    for (u32 index = 0; index < map->width * map->height; ++index)
    {
        u32 col = index % map->width, row = index / map->width;
        u32 h = col * 0x9E3779B1u ^ (row + 1) * 0x85EBCA77u;
        h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12;
        u32 roll = h % 100;
        u8 kind = grass;
        if      (roll < 7)  kind = river;
        else if (roll < 45) kind = grass;
        else if (roll < 62) kind = forest;
        else if (roll < 74) kind = waste;
        else if (roll < 79) kind = swamp;
        else if (roll < 83) kind = sand;
        else if (roll < 93) kind = earth;
        else                kind = cliff;
        if (kind == CONTENT_NO_TERRAIN) kind = grass;
        engine_set_kind(sim, hex_map_coord(map, index), kind);
    }
    for (u32 city_id = 0; city_id < SAVE_CITY_COUNT; ++city_id)
    {
        sim_city_site *site = sim->sites + city_id;
        for (u32 slot = 0; slot < site->slot_count; ++slot) engine_set_kind(sim, site->slots[slot].hex, grass);
        for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d) engine_set_kind(sim, hex_neighbor(site->hex, d), grass);
    }
    for (u32 city_id = 0; city_id < SAVE_CITY_COUNT; ++city_id)
    {
        u32 right = city_id + 1, down = city_id + SIM_SITE_GRID_COLUMNS;
        if (city_id % SIM_SITE_GRID_COLUMNS != SIM_SITE_GRID_COLUMNS - 1 && right < SAVE_CITY_COUNT)
        {
            u8 kind = road, middle = CONTENT_NO_TERRAIN;
            if (city_id == 36 && plank != CONTENT_NO_TERRAIN) kind = plank;    /* 漢中 - 梓潼 */
            if (city_id == 39 && path != CONTENT_NO_TERRAIN)  kind = path;     /* 成都 - 建寧 */
            if (city_id == 40) middle = poison;                                /* 建寧 - 雲南 */
            engine_road(sim, city_id, right, kind, ford, middle);
        }
        if (down < SAVE_CITY_COUNT) engine_road(sim, city_id, down, road, ford, CONTENT_NO_TERRAIN);
    }
    for (u32 city_id = 0; city_id < SAVE_CITY_COUNT; ++city_id) engine_set_kind(sim, sim->sites[city_id].hex, city);
    map->version++;
}

/* ---- the tables ------------------------------------------------------------ */

internal u32
engine_api_terrain_kind_count(void *opaque)
{
    return ((engine *)opaque)->sim->content.terrain_count;
}

internal engine_terrain_kind
engine_api_terrain_kind_at(void *opaque, u32 index)
{
    sim_content *content = &((engine *)opaque)->sim->content;
    engine_terrain_kind result = {};
    result.id = ENGINE_NO_TERRAIN;
    if (index >= content->terrain_count) return result;
    terrain_kind *kind = content->terrains + index;
    result.id = kind->id;
    memcpy(result.key, kind->key, sizeof(result.key));
    for (u32 movement = 0; movement < MOVEMENT_CLASS_COUNT; ++movement) result.cost[movement] = kind->cost[movement];
    result.needs_technique = kind->needs_technique;
    result.loss_percent    = kind->loss_percent;
    result.buildable       = kind->buildable;
    result.tactics         = kind->tactics;
    result.tile            = kind->tile;
    result.level           = kind->level;
    result.enterable       = kind->enterable;   /* M10 */
    result.burnable        = kind->burnable;
    return result;
}

internal u32
engine_api_unit_type_count(void *opaque)
{
    return ((engine *)opaque)->sim->content.unit_type_count;
}

internal engine_unit_type
engine_api_unit_type_at(void *opaque, u32 index)
{
    sim_content *content = &((engine *)opaque)->sim->content;
    engine_unit_type result = {};
    result.id = ENGINE_NO_UNIT_TYPE;
    if (index >= content->unit_type_count) return result;
    unit_type_kind *kind = content->unit_types + index;
    result.id            = kind->id;
    memcpy(result.key, kind->key, sizeof(result.key));
    result.weapon        = kind->weapon;
    result.aptitude      = kind->aptitude;
    result.movement      = (u8)kind->movement;
    result.move          = kind->move;
    result.attack        = (u8)kind->attack;
    result.defence       = (u8)kind->defence;
    result.destroy       = (u8)kind->destroy;
    result.range         = kind->range;
    result.can_attack    = kind->can_attack;
    result.counterattack = kind->counterattack;
    result.transport     = kind->transport;
    result.armament      = kind->armament;   /* M10 */
    result.ship          = kind->ship;
    return result;
}

/* ---- the units --------------------------------------------------------------- */

internal engine_unit_values
engine_values_view(sim_unit_values values)
{
    engine_unit_values result = {};
    result.attack      = values.attack;
    result.defence     = values.defence;
    result.intellect   = values.intellect;
    result.build       = values.build;
    result.move        = values.move;
    result.aptitude    = values.aptitude;
    result.ignores_zoc = values.ignores_zoc;
    result.assumed     = true;   /* Q-170 */
    return result;
}

internal u32
engine_api_unit_count(void *opaque)
{
    return ((engine *)opaque)->sim->unit_count;
}

internal engine_unit
engine_api_unit_at(void *opaque, u32 unit_id)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_unit result = {};
    result.id = ENGINE_NO_UNIT;
    sim_unit *unit = sim_find_unit(sim, unit_id);
    if (!unit) return result;
    result.id          = unit_id;
    result.force_id    = unit->force_id;
    result.q           = unit->hex.q;
    result.r           = unit->hex.r;
    result.unit_type   = unit->unit_type;
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place) result.officers[place] = unit->officers[place];
    result.soldiers    = unit->soldiers;
    result.gold        = unit->gold;
    result.grain       = unit->grain;
    result.morale      = unit->morale;
    result.weapons     = unit->weapons;
    for (u32 stock = 0; stock < CITY_EQUIPMENT_STOCK_COUNT; ++stock) result.cargo[stock] = unit->cargo[stock];
    result.home_city   = unit->home_city;
    result.destination = unit->destination;
    result.moved       = unit->moved_turn == sim->turn + 1;
    result.acted       = unit->acted_turn == sim->turn + 1;
    result.values      = engine_values_view(sim_unit_values_of(sim, unit->force_id, unit->unit_type, unit->officers));
    result.state       = unit->state;
    result.state_turns = unit->state_turns;
    result.fire_immune = sim_unit_has_trait(sim, unit->officers, TRAIT_FIRE_IMMUNE);
    result.ship_type   = unit->has_ship ? unit->ship_type : (u8)ENGINE_NO_UNIT_TYPE;
    result.effective_type = sim_effective_type(sim, unit);
    result.order        = unit->order;
    result.order_q      = unit->order_hex.q;
    result.order_r      = unit->order_hex.r;
    result.order_target = unit->order_target < SIM_UNIT_CAPACITY ? unit->order_target : ENGINE_NO_UNIT;
    /* M12: the supply icon of the plate. A unit eats one grain for each grain.soldiers soldiers each turn (units.cpp). */
    u32 eaten = sim->content.rules.unit_grain_soldiers ? unit->soldiers / sim->content.rules.unit_grain_soldiers : 0;
    if (unit->grain_unknown) result.grain_turns = ENGINE_GRAIN_TURNS_UNKNOWN;
    else if (!eaten)         result.grain_turns = ENGINE_GRAIN_TURNS_ENDLESS;
    else                     result.grain_turns = unit->grain / eaten;
    return result;
}

internal u32
engine_api_unit_at_hex(void *opaque, s32 q, s32 r)
{
    u32 unit_id = sim_unit_at(((engine *)opaque)->sim, hex_make(q, r));
    return unit_id == SIM_NO_UNIT ? ENGINE_NO_UNIT : unit_id;
}

/* A query: the hexes that the unit can reach now. A unit that moved or
   acted has only its own hex. */
internal engine_reach_view
engine_api_reachable(void *opaque, u32 unit_id)
{
    engine *e = (engine *)opaque;
    sim_state *sim = e->sim;
    engine_reach_view view = {};
    view.unit  = unit_id;
    view.cells = e->reach_cells;
    view.costs = e->reach_costs;
    sim_unit *unit = sim_find_unit(sim, unit_id);
    if (!unit) return view;
    if (unit->moved_turn == sim->turn + 1 || unit->acted_turn == sim->turn + 1)
    {
        e->reach_cells[0] = unit->hex;
        e->reach_costs[0] = 0;
        view.count = 1;
        return view;
    }
    arena_reset(&sim->scratch);
    sim_reach reach = sim_unit_reach(sim, unit_id, &sim->scratch);
    u32 cells = sim->map.width * sim->map.height;
    for (u32 index = 0; index < reach.count && index < cells; ++index)
    {
        e->reach_cells[index] = reach.cells[index];
        e->reach_costs[index] = reach.costs[index];
        view.count++;
    }
    return view;
}

internal engine_unit_values
engine_api_unit_values(void *opaque, u8 force_id, u8 unit_type, const u16 officers[SIM_COMMAND_OFFICERS])
{
    return engine_values_view(sim_unit_values_of(((engine *)opaque)->sim, force_id, unit_type, officers));
}

internal engine_sortie_order
engine_api_sortie_proposal(void *opaque, u8 city_id, u8 unit_type)
{
    sim_sortie_order proposal = sim_sortie_proposal(((engine *)opaque)->sim, city_id, unit_type);
    engine_sortie_order result = {};
    result.city_id     = proposal.city_id;
    result.unit_type   = proposal.unit_type;
    result.soldiers    = proposal.soldiers;
    result.gold        = proposal.gold;
    result.grain       = proposal.grain;
    result.destination = ENGINE_NO_CITY;
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place) result.officers[place] = proposal.officers[place];
    return result;
}

/* ---- the commands -------------------------------------------------------------- */

/* Checks first: a refused command is not a command, so it gets no id and
   ledgers nothing. */
internal engine_command_result
engine_run_sortie(engine *e, const engine_sortie_order *in, b32 transport)
{
    engine_command_result result = {};
    sim_sortie_order order = engine_sim_sortie_order(e, in, transport);
    result.refusal = sim_check_sortie(e->sim, &order, transport).refusal;
    if (result.refusal == SIM_REFUSAL_NONE) result.refusal = engine_seat_refusal(e, transport ? SIM_COMMAND_TRANSPORT : SIM_COMMAND_SORTIE);
    if (result.refusal != SIM_REFUSAL_NONE) return result;
    sim_command command = sim_make_command(e->sim, transport ? SIM_COMMAND_TRANSPORT : SIM_COMMAND_SORTIE);
    command.sortie = order;
    engine_run(e, command, 0);
    result.command_id = command.id;
    return result;
}

internal engine_command_result
engine_api_command_sortie(void *opaque, const engine_sortie_order *order)
{
    return engine_run_sortie((engine *)opaque, order, false);
}

internal engine_command_result
engine_api_command_transport(void *opaque, const engine_sortie_order *order)
{
    return engine_run_sortie((engine *)opaque, order, true);
}

internal engine_command_result
engine_run_unit_order(engine *e, sim_command_kind kind, sim_unit_order order)
{
    engine_command_result result = {};
    arena_reset(&e->sim->scratch);
    result.refusal = sim_check_unit_order(e->sim, kind, &order, &e->sim->scratch).refusal;
    if (result.refusal == SIM_REFUSAL_NONE) result.refusal = engine_seat_refusal(e, kind);
    if (result.refusal != SIM_REFUSAL_NONE) return result;
    sim_command command = sim_make_command(e->sim, kind);
    command.unit = order;
    engine_run(e, command, 0);
    result.command_id = command.id;
    return result;
}

internal engine_command_result
engine_api_command_move(void *opaque, u32 unit_id, s32 q, s32 r)
{
    return engine_run_unit_order((engine *)opaque, SIM_COMMAND_MOVE, engine_unit_order(unit_id, q, r, ENGINE_NO_UNIT, 0, 0, 0));
}

internal engine_command_result
engine_api_command_wait(void *opaque, u32 unit_id)
{
    return engine_run_unit_order((engine *)opaque, SIM_COMMAND_WAIT, engine_unit_order(unit_id, 0, 0, ENGINE_NO_UNIT, 0, 0, 0));
}

internal engine_command_result
engine_api_command_retreat(void *opaque, u32 unit_id)
{
    return engine_run_unit_order((engine *)opaque, SIM_COMMAND_RETREAT, engine_unit_order(unit_id, 0, 0, ENGINE_NO_UNIT, 0, 0, 0));
}

internal engine_command_result
engine_api_command_supply(void *opaque, u32 unit_id, u32 target_unit, u32 soldiers, u32 gold, u32 grain)
{
    return engine_run_unit_order((engine *)opaque, SIM_COMMAND_SUPPLY, engine_unit_order(unit_id, 0, 0, target_unit, soldiers, gold, grain));
}

/* ---- the long orders (orders.cpp; G2-052, G2-056, G2-058) ------------------------ */

/* A query: the plan of the unit to the hex. The grain days follow the
   rate of grain.soldiers, as the supply icon does. */
internal engine_path_view
engine_api_path_to(void *opaque, u32 unit_id, s32 q, s32 r)
{
    engine *e = (engine *)opaque;
    sim_state *sim = e->sim;
    engine_path_view view = {};
    view.unit  = unit_id;
    view.cells = e->path_cells;
    sim_unit *unit = sim_find_unit(sim, unit_id);
    if (!unit) return view;
    hex_coord goal = hex_make(q, r);
    arena_reset(&sim->scratch);
    sim_reach reach = sim_unit_reach(sim, unit_id, &sim->scratch);
    u32 place = sim_reach_find(&reach, goal);
    view.in_reach = place != U32_MAX && place != 0;
    arena_reset(&sim->scratch);
    sim_reach plan = sim_unit_plan(sim, unit_id, goal, &sim->scratch);
    view.count = sim_plan_path(&plan, goal, e->path_cells, ArrayCount(e->path_cells));
    if (!view.count) return view;
    view.found = true;
    view.cost  = plan.costs[sim_reach_find(&plan, goal)];
    view.days  = sim_plan_days(sim, unit, view.cost);
    u32 eaten = sim->content.rules.unit_grain_soldiers ? unit->soldiers / sim->content.rules.unit_grain_soldiers : 0;
    view.grain_days = eaten && !unit->grain_unknown ? unit->grain / eaten * CALENDAR_DAYS_PER_TURN : ENGINE_GRAIN_TURNS_ENDLESS;
    for (u32 index = 0; index < view.count && view.base_count < ENGINE_PATH_BASES; ++index)
    {
        u32 base = sim_base_at(sim, e->path_cells[index]);
        if (base != SIM_NO_BASE) view.bases[view.base_count++] = base;
    }
    return view;
}

internal engine_command_result
engine_api_command_destroy(void *opaque, u32 unit_id, u32 target_unit)
{
    return engine_run_unit_order((engine *)opaque, SIM_COMMAND_DESTROY_TARGET, engine_unit_order(unit_id, 0, 0, target_unit, 0, 0, 0));
}

/* G3-020: the long order on the enemy facility at the hex q, r. */
internal engine_command_result
engine_api_command_attack_facility(void *opaque, u32 unit_id, s32 q, s32 r)
{
    return engine_run_unit_order((engine *)opaque, SIM_COMMAND_ATTACK_FACILITY, engine_unit_order(unit_id, q, r, ENGINE_NO_UNIT, 0, 0, 0));
}

internal engine_command_result
engine_api_command_drive(void *opaque, u32 unit_id, u32 target_unit)
{
    return engine_run_unit_order((engine *)opaque, SIM_COMMAND_DRIVE_TARGET, engine_unit_order(unit_id, 0, 0, target_unit, 0, 0, 0));
}

internal engine_command_result
engine_api_command_follow(void *opaque, u32 unit_id, u32 target_unit)
{
    return engine_run_unit_order((engine *)opaque, SIM_COMMAND_FOLLOW, engine_unit_order(unit_id, 0, 0, target_unit, 0, 0, 0));
}

/* 委任部隊行動: the units of the faction that acts do the step of their
   long order now (manual p.24). */
internal engine_command_result
engine_api_command_delegate(void *opaque)
{
    return engine_run_unit_order((engine *)opaque, SIM_COMMAND_DELEGATE, engine_unit_order(ENGINE_NO_UNIT, 0, 0, ENGINE_NO_UNIT, 0, 0, 0));
}

internal void
engine_bind_march_api(engine_api *api)
{
    api->terrain_kind_count = engine_api_terrain_kind_count;
    api->terrain_kind_at    = engine_api_terrain_kind_at;
    api->unit_type_count    = engine_api_unit_type_count;
    api->unit_type_at       = engine_api_unit_type_at;
    api->unit_count         = engine_api_unit_count;
    api->unit_at            = engine_api_unit_at;
    api->unit_at_hex        = engine_api_unit_at_hex;
    api->reachable          = engine_api_reachable;
    api->unit_values        = engine_api_unit_values;
    api->sortie_proposal    = engine_api_sortie_proposal;
    api->command_sortie     = engine_api_command_sortie;
    api->command_transport  = engine_api_command_transport;
    api->command_move       = engine_api_command_move;
    api->command_wait       = engine_api_command_wait;
    api->command_retreat    = engine_api_command_retreat;
    api->command_supply     = engine_api_command_supply;
    api->path_to            = engine_api_path_to;
    api->command_destroy    = engine_api_command_destroy;
    api->command_attack_facility = engine_api_command_attack_facility;
    api->command_drive      = engine_api_command_drive;
    api->command_follow     = engine_api_command_follow;
    api->command_delegate   = engine_api_command_delegate;
}
