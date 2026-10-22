/* The journeys of officers and agents (city-commands.md: 移動, 召喚 and
   the plots 計略; clone gap report 2: G2-036, G2-037, G2-065). An officer
   that moves or that a base summons travels for the days of 抵達日數 and
   arrives at a turn start. A plot sends its officer for the same days, and
   the result comes at the turn start of the arrival (G2-065: the original
   gave the result of 流言 at the next turn start). Included by sim.cpp
   after orders.cpp.

   The spec gives no rule for 抵達日數. ASSUMED (Q-228): on the real map the
   days are 10 for each city region that the journey enters, where two
   regions are neighbours when they touch, and the regions of gates and
   ports on the way are free. Observed: 江陵 to 江夏 10 days, 新野 to 汝南
   10 days, 襄陽 to the capital of 袁紹 50 days. On the placeholder map the
   days are 10 for each 10 hexes between the centres. */

/* ---- the days of a journey --------------------------------------------------- */

/* Builds the table of the regions that touch, one time for each version of
   the map. */
internal void
sim_build_region_links(sim_state *sim)
{
    sim_map_layers *layers = &sim->map_layers;
    for (u32 a = 0; a < SIM_MAP_REGION_CAPACITY; ++a)
    {
        for (u32 b = 0; b < SIM_MAP_REGION_CAPACITY; ++b) sim->region_links[a][b] = 0;
    }
    if (!layers->loaded) return;
    u32 cells = sim->map.width * sim->map.height;
    for (u32 cell = 0; cell < cells; ++cell)
    {
        u8 region = layers->region[cell];
        if (region >= SIM_MAP_REGION_CAPACITY) continue;
        hex_coord hex = hex_map_coord(&sim->map, cell);
        for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d)
        {
            u32 next = sim_map_cell(sim, hex_neighbor(hex, d));
            if (next == U32_MAX) continue;
            u8 other = layers->region[next];
            if (other >= SIM_MAP_REGION_CAPACITY || other == region) continue;
            sim->region_links[region][other] = 1;
            sim->region_links[other][region] = 1;
        }
    }
    sim->region_links_version = sim->map.version;
    sim->region_links_built   = true;
}

/* The hex of a city, a gate or a port (0 to 86). */
internal hex_coord
sim_travel_base_hex(sim_state *sim, u8 base)
{
    if (base < SAVE_CITY_COUNT) return sim->sites[base].hex;
    u32 index = (u32)base - CONTENT_FIRST_BASE;
    return index < CONTENT_BASE_COUNT ? sim->bases[index].hex : sim->sites[0].hex;
}

/* The days from one base to another (抵達日數). The bases are cities,
   gates or ports (0 to 86). */
internal u32
sim_travel_days(sim_state *sim, u8 from_base, u8 to_base)
{
    if (from_base == to_base) return 0;
    sim_map_layers *layers = &sim->map_layers;
    if (layers->loaded && from_base < SIM_MAP_REGION_CAPACITY && to_base < SIM_MAP_REGION_CAPACITY)
    {
        if (!sim->region_links_built || sim->region_links_version != sim->map.version) sim_build_region_links(sim);
        /* The fewest steps over the regions. A step into the region of a city
           counts 10 days; the region of a gate or a port on the way is free
           (江陵 to 江夏 shows 10 days with the port 江津港 between them). */
        u32 nodes = CONTENT_FIRST_BASE + CONTENT_BASE_COUNT;
        u8 steps[SIM_MAP_REGION_CAPACITY];
        for (u32 index = 0; index < SIM_MAP_REGION_CAPACITY; ++index) steps[index] = 0xFF;
        steps[from_base] = 0;
        b32 changed = true;
        while (changed)
        {
            changed = false;
            for (u32 here = 0; here < nodes; ++here)
            {
                if (steps[here] == 0xFF) continue;
                for (u32 other = 0; other < nodes; ++other)
                {
                    if (!sim->region_links[here][other]) continue;
                    u8 candidate = (u8)(steps[here] + (other < SAVE_CITY_COUNT ? 1 : 0));
                    if (candidate < steps[other]) { steps[other] = candidate; changed = true; }
                }
            }
        }
        if (steps[to_base] != 0xFF) return Maximum((u32)steps[to_base], 1u) * CALENDAR_DAYS_PER_TURN;
    }
    u32 distance = (u32)hex_distance(sim_travel_base_hex(sim, from_base), sim_travel_base_hex(sim, to_base));
    u32 turns = (distance + 9) / 10;
    return Maximum(turns, 1u) * CALENDAR_DAYS_PER_TURN;
}

/* ---- 移動 and 召喚 ------------------------------------------------------------ */

/* True if an enemy unit stands within enemy.block_hexes of the city
   (city-commands.md: 移動 cannot run then). */
internal b32
sim_enemy_near_city(sim_state *sim, city_component *city)
{
    hex_coord center = sim->sites[city->city_id].hex;
    for (u32 index = 0; index < sim->unit_count; ++index)
    {
        sim_unit *unit = sim->units + index;
        if (!unit->alive || !sim_at_war(sim, city->owner_force_id, unit->force_id)) continue;
        if (hex_distance(unit->hex, center) <= (s32)sim->content.rules.enemy_block_hexes) return true;
    }
    return false;
}

/* The checks of 移動 (the officers of the city go to another own city) and
   of 召喚 (officers of other own cities come to the city). */
internal sim_check
sim_check_move_officers(sim_state *sim, sim_command_kind kind, sim_move_order *order)
{
    sim_check check = {};
    check.slot    = SIM_ANY_SLOT;
    check.assumed = true;
    sim_rules *rules = &sim->content.rules;
    city_component *city = 0;
    sim_refusal refusal = sim_check_court_common(sim, order->city_id, &city);
    if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
    u8 force = city->owner_force_id;
    city_component *target = 0;
    if (kind == SIM_COMMAND_MOVE_OFFICERS)
    {
        target = sim_find_city(sim, order->target_city);
        if (!target || target == city || target->owner_force_id != force) return sim_refuse(check, SIM_REFUSAL_NO_CITY);
        if (sim_enemy_near_city(sim, city)) return sim_refuse(check, SIM_REFUSAL_ENEMY_NEAR);
    }
    u32 count = 0, days = 0;
    for (u32 place = 0; place < SIM_MOVE_OFFICERS; ++place)
    {
        u16 id = order->officers[place];
        if (id == SIM_NO_OFFICER) continue;
        for (u32 earlier = 0; earlier < place; ++earlier)
        {
            if (order->officers[earlier] == id) return sim_refuse(check, SIM_REFUSAL_DUPLICATE_OFFICER);
        }
        officer_component *officer = sim_find_officer(sim, id);
        if (kind == SIM_COMMAND_MOVE_OFFICERS)
        {
            refusal = sim_check_develop_officer(sim, city, officer);
            if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
            if (officer->status == OFFICER_STATUS_RULER) return sim_refuse(check, SIM_REFUSAL_NO_OFFICER);
            days = sim_travel_days(sim, city->city_id, target->city_id);
        }
        else
        {
            if (!officer || !sim_officer_is_active(officer) || officer->force_id != force || officer->status == OFFICER_STATUS_RULER ||
                officer->current_position >= SAVE_CITY_COUNT || officer->current_position != officer->base_location ||
                officer->current_position == city->city_id)
            {
                return sim_refuse(check, SIM_REFUSAL_OFFICER_NOT_IN_CITY);
            }
            if (officer->travel_turns || officer->job_city != SIM_NO_CITY) return sim_refuse(check, SIM_REFUSAL_OFFICER_BUSY);
            days = Maximum(days, sim_travel_days(sim, (u8)officer->current_position, city->city_id));
        }
        count++;
    }
    if (!count) return sim_refuse(check, SIM_REFUSAL_NO_OFFICER);
    check.action_points = kind == SIM_COMMAND_MOVE_OFFICERS ? rules->ap_move_officers : rules->ap_summon;
    check.quantity      = count;
    check.days          = days;
    check.turns         = days / CALENDAR_DAYS_PER_TURN;
    return sim_check_points(sim, force, check);
}

/* The officers leave for the days of the journey. They are busy until the
   arrival (sim_run_travel). */
internal void
sim_move_officers_command(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_move_order *order = &command.move;
    sim_check check = sim_check_move_officers(sim, command.kind, order);
    if (check.refusal != SIM_REFUSAL_NONE) return;
    city_component *city = sim_find_city(sim, order->city_id);
    u8 force = city->owner_force_id;
    sim->turns.action_points[force] -= check.action_points;
    sim_event points = sim_event_make(sim, SIM_EVENT_ACTION_POINTS_SPENT, command.id);
    points.faction = force;
    points.delta   = -(s32)check.action_points;
    sim_emit(events, points);
    for (u32 place = 0; place < SIM_MOVE_OFFICERS; ++place)
    {
        officer_component *officer = sim_find_officer(sim, order->officers[place]);
        if (!officer) continue;
        u8 destination = command.kind == SIM_COMMAND_MOVE_OFFICERS ? order->target_city : city->city_id;
        u32 days = sim_travel_days(sim, (u8)officer->current_position, destination);
        officer->travel_city  = destination;
        officer->travel_turns = (u8)Maximum(days / CALENDAR_DAYS_PER_TURN, 1u);
        officer->acted_turn   = sim->turn + 1;
        sim_emit_officer(sim, SIM_EVENT_OFFICER_MOVED, officer, (s32)days, destination, true, command.id, events);
    }
}

/* At each turn start: the officers on the road come one turn nearer, and
   the ones that arrive settle in their new base. */
internal void
sim_run_travel(sim_state *sim, u32 cause, sim_event_sink *events)
{
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (!officer->travel_turns) continue;
        officer->travel_turns--;
        if (officer->travel_turns) continue;
        officer->base_location    = officer->travel_city;
        officer->current_position = officer->travel_city;
        officer->job_city         = SIM_NO_CITY;
        sim_emit_officer(sim, SIM_EVENT_OFFICER_ARRIVED, officer, 0, officer->travel_city, true, cause, events);
    }
}

/* ---- the missions of the plots -------------------------------------------------- */

/* The base that a plot aims at: the target city, or the capital of the
   first target faction of 二虎競食. */
internal u8
sim_mission_target(sim_state *sim, sim_court_order *order)
{
    if (order->kind != SIM_PLOT_TWO_TIGERS) return order->target_city;
    officer_component *ruler = sim_find_officer(sim, sim_force_ruler(sim, order->force));
    return ruler && ruler->base_location < SAVE_CITY_COUNT ? (u8)ruler->base_location : order->city_id;
}

/* Starts the mission of a plot: the officer goes for the days of the
   journey, and the result comes at the arrival. */
internal b32
sim_start_mission(sim_state *sim, sim_court_order *order, u8 force, u32 chance, u32 cause, sim_event_sink *events)
{
    sim_mission *mission = 0;
    for (u32 index = 0; index < SIM_MISSION_CAPACITY && !mission; ++index)
    {
        if (!sim->missions[index].active) mission = sim->missions + index;
    }
    if (!mission) return false;
    u32 days = sim_travel_days(sim, order->city_id, sim_mission_target(sim, order));
    mission->active     = true;
    mission->order      = *order;
    mission->force      = force;
    mission->chance     = chance;
    mission->command_id = cause;
    mission->turns_left = (u8)Maximum(days / CALENDAR_DAYS_PER_TURN, 1u);
    officer_component *actor = sim_find_officer(sim, order->officer);
    if (actor) sim_emit_officer(sim, SIM_EVENT_MISSION_STARTED, actor, (s32)days, sim_mission_target(sim, order), true, cause, events);
    return true;
}

/* At each turn start: the missions come one turn nearer, and the ones
   that arrive give their result (sim_resolve_plot, diplomacy.cpp). */
internal void
sim_run_missions(sim_state *sim, u32 cause, sim_event_sink *events)
{
    for (u32 index = 0; index < SIM_MISSION_CAPACITY; ++index)
    {
        sim_mission *mission = sim->missions + index;
        if (!mission->active) continue;
        if (mission->turns_left) mission->turns_left--;
        if (mission->turns_left) continue;
        mission->active = false;
        sim_resolve_plot(sim, mission, events);
    }
    (void)cause;
}
