/* Units on the map (military-units-and-tactics.md, terrain-and-map.md,
   city-commands.md): the unit values from the officers, the move range with
   the terrain costs and the zone of control, and the commands 出征 (sortie),
   輸送 (transport), 移動 (move), 待命 (wait), 退卻 (retreat) and 補給
   (supply). Included by sim.cpp.

   The spec documents the classes, their grades and movement, the terrain
   kinds, the zone of control and the command costs. It gives no formula
   for the unit values and no terrain costs: those are assumed (Q-167 to
   Q-170), and the events of an assumed amount say so. */

#define SIM_UNIT_PATH_CAPACITY 64   /* the longest move: the highest movement over the lowest cost, with room */

/* ---- units --------------------------------------------------------------- */

internal sim_unit *
sim_find_unit(sim_state *sim, u32 unit_id)
{
    if (unit_id >= sim->unit_count || !sim->units[unit_id].alive) return 0;
    return sim->units + unit_id;
}

/* A free place in the unit array: the first dead unit, or a new place. */
internal u32
sim_new_unit_id(sim_state *sim)
{
    for (u32 index = 0; index < sim->unit_count; ++index)
    {
        if (!sim->units[index].alive) return index;
    }
    return sim->unit_count < SIM_UNIT_CAPACITY ? sim->unit_count++ : SIM_NO_UNIT;
}

/* The alive unit on a hex, or SIM_NO_UNIT. */
internal u32
sim_unit_at(sim_state *sim, hex_coord hex)
{
    for (u32 index = 0; index < sim->unit_count; ++index)
    {
        if (sim->units[index].alive && hex_equal(sim->units[index].hex, hex)) return index;
    }
    return SIM_NO_UNIT;
}

/* The city whose hex this is, or SIM_NO_CITY. */
internal u8
sim_city_at(sim_state *sim, hex_coord hex)
{
    /* M10: on the real map a city covers its flower of 7 hexes of 都市. */
    sim_map_layers *layers = &sim->map_layers;
    if (layers->loaded)
    {
        u32 cell = sim_map_cell(sim, hex);
        if (cell == U32_MAX || sim->map.kind[cell] != layers->city_kind) return SIM_NO_CITY;
        u8 region = layers->region[cell];
        return region < SAVE_CITY_COUNT && sim->city_index[region] != SIM_NO_CITY ? region : (u8)SIM_NO_CITY;
    }
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        u8 city_id = sim->cities.data[index].city_id;
        if (hex_equal(sim->sites[city_id].hex, hex)) return city_id;
    }
    return SIM_NO_CITY;
}

internal unit_type_kind *
sim_unit_type(sim_state *sim, u8 unit_type)
{
    return unit_type < sim->content.unit_type_count ? sim->content.unit_types + unit_type : 0;
}

/* ---- the unit values (assumed formula, Q-170) ----------------------------- */

/* Two officers in a sworn group or a couple give the full value of the
   better one (Steam guide section 4). The other relations come with F-054. */
internal b32
sim_officers_close(officer_component *a, officer_component *b)
{
    if (a->spouse == b->officer_id || b->spouse == a->officer_id) return true;
    return a->sworn_brother_group_head != 0xFFFF && a->sworn_brother_group_head == b->sworn_brother_group_head;
}

/* ASSUMED RULE (Q-170). The value of the commander is the base. If the best
   deputy is higher, the unit gains unit.deputy_percent of the difference,
   or all of it when the two officers are close. */
internal u32
sim_unit_ability(sim_state *sim, const u16 *officers, sim_ability ability)
{
    officer_component *commander = sim_find_officer(sim, officers[0]);
    if (!commander) return 0;
    u32 value = sim_officer_ability(commander, ability);
    u32 best = value;
    officer_component *best_deputy = 0;
    for (u32 place = 1; place < SIM_COMMAND_OFFICERS; ++place)
    {
        officer_component *deputy = sim_find_officer(sim, officers[place]);
        if (deputy && sim_officer_ability(deputy, ability) > best)
        {
            best = sim_officer_ability(deputy, ability);
            best_deputy = deputy;
        }
    }
    if (!best_deputy) return value;
    if (sim_officers_close(commander, best_deputy)) return best;
    /* M7 (officers-and-relations.md): a disliked deputy gives nothing; a
       liked one gives more, less than a sworn brother (assumed, Q-170). */
    if (sim_officer_dislikes(commander, best_deputy->officer_id) || sim_officer_dislikes(best_deputy, commander->officer_id)) return value;
    u32 percent = sim->content.rules.unit_deputy_percent;
    if (sim_officer_likes(commander, best_deputy->officer_id) || sim_officer_likes(best_deputy, commander->officer_id))
    {
        percent = Minimum(percent + sim->content.rules.relation_liked_percent, 100u);
    }
    return value + (best - value) * percent / 100;
}

/* True if two officers of the unit dislike each other: the unit intellect
   falls (officers-and-relations.md). */
internal b32
sim_unit_has_dislike(sim_state *sim, const u16 *officers)
{
    for (u32 a = 0; a < SIM_COMMAND_OFFICERS; ++a)
    {
        officer_component *one = sim_find_officer(sim, officers[a]);
        for (u32 b = 0; one && b < SIM_COMMAND_OFFICERS; ++b)
        {
            if (a != b && officers[b] != SIM_NO_OFFICER && sim_officer_dislikes(one, officers[b])) return true;
        }
    }
    return false;
}

/* The best aptitude of the officers for the class: 0 C .. 3 S. */
internal u32
sim_unit_aptitude(sim_state *sim, const u16 *officers, unit_type_kind *type)
{
    if (!type || type->aptitude == CONTENT_APTITUDE_NONE) return 255;
    u32 best = 0;
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
    {
        officer_component *officer = sim_find_officer(sim, officers[place]);
        if (officer && type->aptitude < OFFICER_UNIT_APTITUDE_COUNT && officer->unit_aptitudes[type->aptitude] > best)
        {
            best = officer->unit_aptitudes[type->aptitude];
        }
    }
    return best > 3 ? 3 : best;
}

internal b32
sim_unit_has_skill(sim_state *sim, const u16 *officers, u32 skill)
{
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
    {
        if (sim_officer_has_skill(sim_find_officer(sim, officers[place]), skill)) return true;
    }
    return false;
}

/* True if an officer of the unit has a skill with the trait. */
internal b32
sim_unit_has_trait(sim_state *sim, const u16 *officers, skill_trait trait)
{
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
    {
        if (sim_officer_has_trait(sim, sim_find_officer(sim, officers[place]), trait)) return true;
    }
    return false;
}

/* ASSUMED (Q-169): each technique or skill that raises the movement adds
   move.bonus. The class movement is observed (22, 28, 18). */
internal u32
sim_unit_move(sim_state *sim, u8 force, const u16 *officers, unit_type_kind *type)
{
    sim_rules *rules = &sim->content.rules;
    u32 move = type->move, bonus = rules->move_bonus;
    content_unit_class tech_class = CONTENT_CLASS_NONE;
    switch (type->aptitude)
    {
        case OFFICER_APTITUDE_SPEAR:    tech_class = CONTENT_CLASS_PIKE;     break;
        case OFFICER_APTITUDE_HALBERD:  tech_class = CONTENT_CLASS_HALBERD;  break;
        case OFFICER_APTITUDE_CROSSBOW: tech_class = CONTENT_CLASS_CROSSBOW; break;
        case OFFICER_APTITUDE_CAVALRY:  tech_class = CONTENT_CLASS_CAVALRY;  break;
        case OFFICER_APTITUDE_SIEGE:    tech_class = CONTENT_CLASS_SIEGE;    break;
        default: break;
    }
    if (tech_class != CONTENT_CLASS_NONE && sim_has_technique_effect(sim, force, TECHNIQUE_EFFECT_ELITE, tech_class)) move += bonus;
    if (tech_class == CONTENT_CLASS_CAVALRY && sim_has_technique_effect(sim, force, TECHNIQUE_EFFECT_MOVEMENT, CONTENT_CLASS_CAVALRY)) move += bonus;
    if (tech_class == CONTENT_CLASS_SIEGE && sim_has_technique_effect(sim, force, TECHNIQUE_EFFECT_SIEGE_SPEED, CONTENT_CLASS_SIEGE)) move += bonus;
    if (type->transport && sim_has_technique_effect(sim, force, TECHNIQUE_EFFECT_TRANSPORT_SPEED, CONTENT_CLASS_NONE)) move += bonus;
    if (type->movement != MOVEMENT_NAVY && sim_unit_has_trait(sim, officers, TRAIT_MOVE_LAND)) move += bonus;
    if (type->movement == MOVEMENT_CAVALRY && sim_unit_has_trait(sim, officers, TRAIT_MOVE_CAVALRY)) move += bonus;
    if (type->transport && sim_unit_has_trait(sim, officers, TRAIT_MOVE_TRANSPORT)) move += bonus;
    if (type->movement == MOVEMENT_NAVY && sim_unit_has_trait(sim, officers, TRAIT_MOVE_WATER)) move += bonus;
    return move;
}

/* The five values of a unit of this type with these officers. */
internal sim_unit_values
sim_unit_values_of(sim_state *sim, u8 force, u8 unit_type, const u16 *officers)
{
    sim_unit_values values = {};
    values.aptitude = 255;
    unit_type_kind *type = sim_unit_type(sim, unit_type);
    if (!type) return values;
    sim_rules *rules = &sim->content.rules;
    values.aptitude = sim_unit_aptitude(sim, officers, type);
    u32 aptitude_percent = values.aptitude <= 3 && rules->aptitude_percent[values.aptitude] ? rules->aptitude_percent[values.aptitude] : 100;
    u32 attack_percent = rules->grade_percent[type->attack], defence_percent = rules->grade_percent[type->defence];
    /* The run of 2026-10-22 (military-units-and-tactics.md): attack = force x aptitude x class and
       defence = leadership x aptitude x class, within one point. The class factors are rules of
       the pack (class.attack_*, class.defence_*); without them the grade of the class holds. A
       unit without a weapon (劍兵) has 0.36 of the force and the leadership. */
    switch (sim_class_of_aptitude(type->aptitude))
    {
        case CONTENT_CLASS_PIKE:     if (rules->class_attack_pike)     { attack_percent = rules->class_attack_pike;     defence_percent = rules->class_defence_pike; }     break;
        case CONTENT_CLASS_HALBERD:  if (rules->class_attack_halberd)  { attack_percent = rules->class_attack_halberd;  defence_percent = rules->class_defence_halberd; }  break;
        case CONTENT_CLASS_CROSSBOW: if (rules->class_attack_crossbow) { attack_percent = rules->class_attack_crossbow; defence_percent = rules->class_defence_crossbow; } break;
        case CONTENT_CLASS_CAVALRY:  if (rules->class_attack_cavalry)  { attack_percent = rules->class_attack_cavalry;  defence_percent = rules->class_defence_cavalry; }  break;
        default:
            if (rules->sword_percent && type->weapon == CONTENT_NO_WEAPON && !type->transport && type->movement == MOVEMENT_FOOT)
            {
                attack_percent = defence_percent = rules->sword_percent;
                aptitude_percent = 100;
            }
            break;
    }
    values.attack  = (u32)((u64)sim_unit_ability(sim, officers, SIM_ABILITY_FORCE) * aptitude_percent * attack_percent / 10000);
    values.defence = (u32)((u64)sim_unit_ability(sim, officers, SIM_ABILITY_LEADERSHIP) * aptitude_percent * defence_percent / 10000);
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
    {
        officer_component *officer = sim_find_officer(sim, officers[place]);
        if (officer && officer->intelligence > values.intellect) values.intellect = officer->intelligence;
    }
    if (sim_unit_has_dislike(sim, officers)) values.intellect = sim_percent_of(values.intellect, 100 - Minimum(rules->relation_disliked_intellect, 100u));
    /* 建設 is 50 plus two thirds of the politics (run of 2026-10-22; unit.build_base, unit.build_percent). */
    u32 politics = sim_unit_ability(sim, officers, SIM_ABILITY_POLITICS);
    values.build = rules->build_percent ? rules->build_base + politics * rules->build_percent / 100 : politics;
    values.move  = sim_unit_move(sim, force, officers, type);
    values.ignores_zoc = type->movement != MOVEMENT_SIEGE && type->movement != MOVEMENT_NAVY &&
                         sim_unit_has_trait(sim, officers, TRAIT_ZOC_LAND);
    return values;
}

/* The soldiers that a commander leads (Q-169 until the posts of M7):
   unit.soldier_cap, plus 改革軍制. */
internal u32
sim_unit_soldier_cap(sim_state *sim, u8 force)
{
    return sim->content.rules.unit_soldier_cap + sim_technique_effect_value(sim, force, TECHNIQUE_EFFECT_SOLDIER_CAP);
}

/* ---- the move range ------------------------------------------------------- */

internal terrain_kind *
sim_terrain_at(sim_state *sim, hex_coord hex)
{
    if (!hex_map_contains(&sim->map, hex)) return 0;
    u8 kind = sim->map.kind[hex_map_index(&sim->map, hex)];
    return kind < sim->content.terrain_count ? sim->content.terrains + kind : 0;
}

/* The cost to enter a terrain, without fire and units; 0 if the unit
   cannot enter. With movement_costs.tsv (M10, F-148) a unit type pays the
   row of its armament on land and the row of its ship on 河 and 海
   (terrain-and-map.md). Without the table, or for no unit type, the class
   costs of terrain.tsv apply. */
internal u32
sim_terrain_cost(sim_state *sim, movement_class movement, u8 unit_type, u8 terrain_id)
{
    if (terrain_id >= sim->content.terrain_count) return 0;
    terrain_kind *terrain = sim->content.terrains + terrain_id;
    b32 water = terrain->cost[MOVEMENT_NAVY] && !terrain->cost[MOVEMENT_FOOT];
    unit_type_kind *type = sim_unit_type(sim, unit_type);
    if (type && sim->content.armament_count && (type->armament != CONTENT_NO_ARMAMENT || type->ship != CONTENT_NO_ARMAMENT))
    {
        u8 row = water ? type->ship : type->armament;
        return row < sim->content.armament_count ? sim->content.armaments[row].cost[terrain_id] : 0;
    }
    /* A land unit on water is a 走舸 and pays the navy cost (manual p.12);
       a siege weapon cannot go on water (M6, assumed: Q-176). */
    if (!terrain->cost[movement] && (movement == MOVEMENT_FOOT || movement == MOVEMENT_CAVALRY) && terrain->cost[MOVEMENT_NAVY])
    {
        movement = MOVEMENT_NAVY;
    }
    return terrain->cost[movement];
}

/* The unit type that pays to enter a hex. On 河 and 海 a unit of a save
   with its own ship class (record +41) pays the row of that ship. Else the
   unit type pays: its armament row on land and its ship row on water. */
internal u8
sim_unit_cost_type(sim_state *sim, sim_unit *unit, hex_coord hex)
{
    if (unit->has_ship && sim_hex_is_water(sim, hex)) return unit->ship_type;
    return unit->unit_type;
}

/* The cost for a unit of the force to enter the hex, or 0 if it cannot.
   self is the unit that moves; unit_type is CONTENT_NO_UNIT_TYPE for the
   class costs only. ignore_units: a plan over several turns (orders.cpp)
   does not stop at the units of today. */
internal u32
sim_entry_cost_of(sim_state *sim, u8 force, movement_class movement, u8 unit_type, hex_coord hex, u32 self, b32 ignore_units)
{
    terrain_kind *terrain = sim_terrain_at(sim, hex);
    if (!terrain) return 0;
    u32 cost = sim_terrain_cost(sim, movement, unit_type, sim->map.kind[hex_map_index(&sim->map, hex)]);
    if (!cost) return 0;
    if (terrain->needs_technique != CONTENT_NO_TECHNIQUE && !sim_has_technique(sim, force, terrain->needs_technique)) return 0;
    u32 other = ignore_units ? SIM_NO_UNIT : sim_unit_at(sim, hex);
    if (other != SIM_NO_UNIT && other != self) return 0;
    u8 city_id = sim_city_at(sim, hex);
    if (city_id != SIM_NO_CITY && sim_find_city(sim, city_id)->owner_force_id != force) return 0;
    /* M8: a port or a pass blocks the hex. M10: many routes pass through a
       port or a pass (siege-and-bases.md), so a unit enters a base of its
       own faction and stops there, as in an own city (assumed). */
    u32 base = sim_base_at(sim, hex);
    if (base != SIM_NO_BASE && sim->bases[base].force != force) return 0;
    /* A building blocks the hex, but a trap hides and does not (M6). */
    u32 building_id = sim_building_at(sim, hex);
    if (building_id != SIM_NO_BUILDING)
    {
        sim_building *building = sim->buildings + building_id;
        building_kind *kind    = sim->content.buildings + building->kind;
        b32 hidden = kind->type == BUILDING_SEED || kind->type == BUILDING_BALL || kind->natural;
        if (!hidden) return 0;
    }
    /* F-148: to enter a burning hex costs fire.cost_multiplier (4) times the
       cost of the table; a pack without the rule adds fire.extra_cost. */
    b32 fire_free = self < sim->unit_count && sim_unit_has_trait(sim, sim->units[self].officers, TRAIT_FIRE_IMMUNE);
    if (sim_hex_burns(sim, hex) && !fire_free)
    {
        u32 multiplier = sim->content.rules.fire_cost_multiplier;
        cost = cost * (multiplier ? multiplier : 1) + sim->content.rules.fire_extra_cost;
    }
    return cost;
}

internal u32
sim_entry_cost(sim_state *sim, u8 force, movement_class movement, hex_coord hex, u32 self)
{
    return sim_entry_cost_of(sim, force, movement, CONTENT_NO_UNIT_TYPE, hex, self, false);
}

/* True if an enemy unit or a complete enemy military building stands next
   to the hex (manual p.14: a unit stops next to an enemy unit). An ally and
   a faction under a ceasefire are no enemy (M7, diplomacy.md). */
internal b32
sim_in_enemy_zone(sim_state *sim, u8 force, hex_coord hex, u32 self)
{
    for (u32 index = 0; index < sim->unit_count; ++index)
    {
        sim_unit *other = sim->units + index;
        if (!other->alive || index == self || !sim_at_war(sim, force, other->force_id)) continue;
        if (hex_distance(other->hex, hex) == 1) return true;
    }
    for (u32 index = 0; index < sim->building_count; ++index)
    {
        sim_building *building = sim->buildings + index;
        if (!building->alive || !building->complete || !sim_at_war(sim, force, building->force_id)) continue;
        building_type type = sim->content.buildings[building->kind].type;
        b32 military = type == BUILDING_CAMP || type == BUILDING_TOWER || type == BUILDING_DRUM || type == BUILDING_MUSIC;
        if (military && hex_distance(building->hex, hex) == 1) return true;
    }
    return false;
}

/* True if another unit of the same force stands on the hex. A unit can
   pass through the hex of an own unit, but it cannot stop there (the
   player, 2026-10-24). Only the units of another force block the way. */
internal b32
sim_own_unit_on(sim_state *sim, u32 unit_id, hex_coord hex)
{
    u32 other = sim_unit_at(sim, hex);
    return other != SIM_NO_UNIT && other != unit_id && sim->units[other].force_id == sim->units[unit_id].force_id;
}

/* The reach with the hexes of own units after count. The paths through
   from[] still reach these hexes, but no caller picks one as the end of a
   move. The new arrays have the size of the reach, not of the map. */
internal sim_reach
sim_reach_without_own_units(sim_state *sim, u32 unit_id, sim_reach reach, memory_arena *scratch)
{
    u32 passed = 0;
    for (u32 index = 1; index < reach.count; ++index) passed += sim_own_unit_on(sim, unit_id, reach.cells[index]) ? 1 : 0;
    if (!passed) return reach;
    u32 total = reach.count;
    u32 *place = PushArray(scratch, total, u32);
    u32 kept = 0, after = total - passed;
    for (u32 index = 0; index < total; ++index)
    {
        place[index] = index && sim_own_unit_on(sim, unit_id, reach.cells[index]) ? after++ : kept++;
    }
    sim_reach result = {};
    result.cells = PushArray(scratch, total, hex_coord);
    result.costs = PushArray(scratch, total, u32);
    result.from  = PushArray(scratch, total, u32);
    result.count = kept;
    for (u32 index = 0; index < total; ++index)
    {
        u32 to = place[index];
        result.cells[to] = reach.cells[index];
        result.costs[to] = reach.costs[index];
        result.from[to]  = reach.from[index] == U32_MAX ? U32_MAX : place[reach.from[index]];
    }
    return result;
}

/* The search of the lowest cost from the hex of the unit. The reach of
   this turn (plan = false) stops at its movement, in an enemy zone of
   control, in an own city and at an own base. A plan (plan = true,
   orders.cpp) has no cap, treats a city and a base as waypoints, does
   not stop at the units of today, and ends at the goal. Arrays come from
   the scratch arena. */
internal sim_reach
sim_unit_search(sim_state *sim, u32 unit_id, memory_arena *scratch, b32 plan, hex_coord goal)
{
    sim_reach reach = {};
    sim_unit *unit = sim_find_unit(sim, unit_id);
    if (!unit) return reach;
    unit_type_kind *type = sim_unit_type(sim, unit->unit_type);
    if (!type) return reach;
    sim_unit_values values = sim_unit_values_of(sim, unit->force_id, unit->unit_type, unit->officers);
    u32 cells = sim->map.width * sim->map.height;
    reach.cells = PushArray(scratch, cells, hex_coord);
    reach.costs = PushArray(scratch, cells, u32);
    reach.from  = PushArray(scratch, cells, u32);
    u32 *best   = PushArray(scratch, cells, u32);
    u32 *slot   = PushArray(scratch, cells, u32);
    u32 *parent = PushArray(scratch, cells, u32);
    for (u32 index = 0; index < cells; ++index) { best[index] = U32_MAX; slot[index] = U32_MAX; parent[index] = U32_MAX; }
    hex_heap open;
    open.entries = PushArray(scratch, cells * HEX_DIRECTION_COUNT + 1, hex_heap_entry);
    open.count   = 0;
    if (!hex_map_contains(&sim->map, unit->hex)) return reach;

    /* 操舵 #6 and 推進 #5: a land unit that starts on water sails. */
    b32 water_free = sim_unit_has_trait(sim, unit->officers, TRAIT_ZOC_WATER);
    u32 move = values.move;
    if (type->movement != MOVEMENT_NAVY && sim_hex_is_water(sim, unit->hex) && sim_unit_has_trait(sim, unit->officers, TRAIT_MOVE_WATER)) move += sim->content.rules.move_bonus;
    u32 start = hex_map_index(&sim->map, unit->hex);
    best[start] = 0;
    hex_heap_push(&open, 0, start);
    while (open.count)
    {
        hex_heap_entry current = hex_heap_pop(&open);
        if (current.cost != best[current.index] || slot[current.index] != U32_MAX) continue;
        hex_coord here = hex_map_coord(&sim->map, current.index);
        /* The parent is final before the hex: the search is by cost. */
        u32 previous = parent[current.index] != U32_MAX ? slot[parent[current.index]] : U32_MAX;
        slot[current.index] = reach.count;
        reach.cells[reach.count] = here;
        reach.costs[reach.count] = current.cost;
        reach.from[reach.count]  = previous;
        reach.count++;
        if (plan && hex_equal(here, goal)) break;
        /* A unit stops next to an enemy and in an own city. */
        if (!plan && current.index != start)
        {
            b32 free_of_zone = sim_hex_is_water(sim, here) ? water_free : values.ignores_zoc;
            if (!free_of_zone && sim_in_enemy_zone(sim, unit->force_id, here, unit_id)) continue;
            if (sim_city_at(sim, here) != SIM_NO_CITY) continue;
            if (sim_base_at(sim, here) != SIM_NO_BASE) continue;   /* M10: an own port or pass (assumed) */
        }
        for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d)
        {
            hex_coord next = hex_neighbor(here, d);
            if (!hex_map_contains(&sim->map, next)) continue;
            b32 own = sim_own_unit_on(sim, unit_id, next);   /* the unit passes through an own unit */
            u32 cost = sim_entry_cost_of(sim, unit->force_id, type->movement, sim_unit_cost_type(sim, unit, next), next, unit_id, plan || own);
            if (!cost) continue;
            u32 next_index = hex_map_index(&sim->map, next);
            u32 total = current.cost + cost;
            if ((!plan && total > move) || total >= best[next_index] || slot[next_index] != U32_MAX) continue;
            best[next_index]   = total;
            parent[next_index] = current.index;
            hex_heap_push(&open, total, next_index);
        }
    }
    if (!plan) reach = sim_reach_without_own_units(sim, unit_id, reach, scratch);
    return reach;
}

/* The hexes that the unit can reach in this turn. */
internal sim_reach
sim_unit_reach(sim_state *sim, u32 unit_id, memory_arena *scratch)
{
    return sim_unit_search(sim, unit_id, scratch, false, hex_make(0, 0));
}

/* The path of a long order to the goal, over several turns (orders.cpp). */
internal sim_reach
sim_unit_plan(sim_state *sim, u32 unit_id, hex_coord goal, memory_arena *scratch)
{
    return sim_unit_search(sim, unit_id, scratch, true, goal);
}

/* The place of a hex in the reach, or U32_MAX. */
internal u32
sim_reach_find(sim_reach *reach, hex_coord hex)
{
    for (u32 index = 0; index < reach->count; ++index)
    {
        if (hex_equal(reach->cells[index], hex)) return index;
    }
    return U32_MAX;
}

/* ---- the ledger of a unit -------------------------------------------------- */

internal void
sim_emit_unit(sim_state *sim, sim_event_kind kind, u32 unit_id, u8 city_id, s32 delta, b32 assumed, u32 cause, sim_event_sink *events)
{
    sim_event event = sim_event_make(sim, kind, cause);
    event.subject = city_id != SIM_NO_CITY ? sim_city_entity(sim, city_id) : 0;
    if (city_id == SIM_NO_CITY && unit_id < sim->unit_count && sim->units[unit_id].officers[0] != SIM_NO_OFFICER)
    {
        /* The commander names the unit in the log (周泰隊：士兵 -232), also after the unit is gone. */
        event.subject = sim_officer_entity(sim, sim->units[unit_id].officers[0]);
    }
    event.faction = unit_id < sim->unit_count ? sim->units[unit_id].force_id : SIM_NO_FORCE;
    event.detail  = unit_id;
    event.delta   = delta;
    event.assumed = assumed;
    sim_emit(events, event);
}

/* An event of a field building: the owner of the building is the faction,
   and the event has no subject. A building id is no unit index, so
   sim_emit_unit gave these events the commander of another unit (the
   player, 2026-10-24). */
internal void
sim_emit_building(sim_state *sim, sim_event_kind kind, u32 building_id, s32 delta, b32 assumed, u32 cause, sim_event_sink *events)
{
    sim_event event = sim_event_make(sim, kind, cause);
    event.faction = building_id < sim->building_count ? sim->buildings[building_id].force_id : SIM_NO_FORCE;
    event.detail  = building_id;
    event.delta   = delta;
    event.assumed = assumed;
    sim_emit(events, event);
}

/* An event of a map cell (the fire): no subject and no faction. */
internal void
sim_emit_cell(sim_state *sim, sim_event_kind kind, u32 cell, s32 delta, b32 assumed, u32 cause, sim_event_sink *events)
{
    sim_event event = sim_event_make(sim, kind, cause);
    event.faction = SIM_NO_FORCE;
    event.detail  = cell;
    event.delta   = delta;
    event.assumed = assumed;
    sim_emit(events, event);
}

/* ---- 出征 and 輸送 ----------------------------------------------------------- */

/* The first free hex next to the city that the class can enter. On the
   real map the six neighbours of the centre are the city flower (M10), so
   the search goes on to the ring of 12 hexes around the flower. */
internal hex_coord
sim_sortie_hex(sim_state *sim, u8 city_id, u8 force, movement_class movement, b32 *found)
{
    hex_coord center = sim_travel_base_hex(sim, city_id);   /* a city, or a port or a pass (the play report of 2026-10-23) */
    *found = false;
    for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d)
    {
        hex_coord hex = hex_neighbor(center, d);
        if (sim_entry_cost(sim, force, movement, hex, SIM_NO_UNIT) && sim_city_at(sim, hex) == SIM_NO_CITY && sim_base_at(sim, hex) == SIM_NO_BASE)
        {
            *found = true;
            return hex;
        }
    }
    /* The ring at distance 2: start two steps in direction 4, then walk the
       six sides (the directions of hex.h are in order around the hex). */
    hex_coord hex = hex_neighbor(hex_neighbor(center, 4), 4);
    for (u32 side = 0; side < HEX_DIRECTION_COUNT; ++side)
    {
        for (u32 step = 0; step < 2; ++step)
        {
            if (sim_entry_cost(sim, force, movement, hex, SIM_NO_UNIT) && sim_city_at(sim, hex) == SIM_NO_CITY)
            {
                *found = true;
                return hex;
            }
            hex = hex_neighbor(hex, side);
        }
    }
    return center;
}

/* The weapons that a unit of the type needs from the city: one for each
   soldier for a weapon of the smithy or the stable, one item for a siege
   weapon or a ship. */
internal u32
sim_sortie_weapons(sim_state *sim, unit_type_kind *type, u32 soldiers)
{
    if (type->weapon >= sim->content.weapon_count) return 0;
    return sim->content.weapons[type->weapon].max_days ? 1 : soldiers;
}

internal sim_refusal sim_check_depot_officers(sim_state *sim, sim_depot *depot, const u16 *officers, u32 max_officers, sim_ability ability, u32 *sum);

/* The checks of 出征 (and of 輸送 with transport set), in order. */
internal sim_check
sim_check_sortie(sim_state *sim, sim_sortie_order *order, b32 transport)
{
    sim_check check = {};
    check.slot = SIM_ANY_SLOT;
    if (!sim->has_content)                              return sim_refuse(check, SIM_REFUSAL_NO_CONTENT);
    if (sim_game_over(sim))                             return sim_refuse(check, SIM_REFUSAL_GAME_OVER);
    /* The base is a city, or a port or a pass with a garrison (the play report of 2026-10-23). */
    sim_depot depot = sim_depot_at(sim, order->city_id);
    if (!depot.ok)                                      return sim_refuse(check, SIM_REFUSAL_NO_CITY);
    u8 force = depot.force;
    if (sim_whose_turn(sim) != force)                   return sim_refuse(check, SIM_REFUSAL_NOT_YOUR_TURN);
    unit_type_kind *type = sim_unit_type(sim, order->unit_type);
    if (!type || (transport != 0) != (type->transport != 0)) return sim_refuse(check, SIM_REFUSAL_NO_UNIT);
    if (transport)
    {
        city_component *destination = sim_find_city(sim, order->destination);
        if (!destination || destination->owner_force_id != force || order->destination == order->city_id) return sim_refuse(check, SIM_REFUSAL_NO_CITY);
    }
    u32 sum = 0;
    sim_refusal refusal = sim_check_depot_officers(sim, &depot, order->officers, SIM_COMMAND_OFFICERS, SIM_ABILITY_LEADERSHIP, &sum);
    if (refusal != SIM_REFUSAL_NONE)                    return sim_refuse(check, refusal);
    if (order->officers[0] == SIM_NO_OFFICER)           return sim_refuse(check, SIM_REFUSAL_NO_OFFICER);
    if (sim_busy(sim))                                  return sim_refuse(check, SIM_REFUSAL_BUSY);
    u32 cap = sim_officer_soldier_cap(sim, sim_find_officer(sim, order->officers[0]));   /* M7: the post or rank of the commander */
    if ((!transport && order->soldiers == 0) || order->soldiers > *depot.troops || order->soldiers > cap)
    {
        return sim_refuse(check, SIM_REFUSAL_NO_SOLDIERS);
    }
    u32 weapons = sim_sortie_weapons(sim, type, order->soldiers);
    if (weapons && depot.equipment[sim->content.weapons[type->weapon].stock] < weapons) return sim_refuse(check, SIM_REFUSAL_NO_WEAPONS);
    if (!transport && order->ship)
    {
        /* G2-057: the ship for the water is a navy class with a weapon row, one from the stock of the city. */
        unit_type_kind *ship = sim_unit_type(sim, order->ship);
        if (!ship || ship->movement != MOVEMENT_NAVY || ship->weapon >= sim->content.weapon_count) return sim_refuse(check, SIM_REFUSAL_NO_UNIT);
        if (depot.equipment[sim->content.weapons[ship->weapon].stock] < 1) return sim_refuse(check, SIM_REFUSAL_NO_WEAPONS);
    }
    for (u32 stock = 0; transport && stock < CITY_EQUIPMENT_STOCK_COUNT; ++stock)
    {
        if (order->cargo[stock] > depot.equipment[stock]) return sim_refuse(check, SIM_REFUSAL_NO_WEAPONS);
    }
    if (order->grain > *depot.grain)                    return sim_refuse(check, SIM_REFUSAL_NO_GRAIN);
    if (order->gold > *depot.gold)                      return sim_refuse(check, SIM_REFUSAL_NO_GOLD);
    b32 room = false;
    sim_sortie_hex(sim, order->city_id, force, type->movement, &room);
    if (!room)                                          return sim_refuse(check, SIM_REFUSAL_NO_ROOM);
    u32 free_unit = SIM_NO_UNIT;
    for (u32 index = 0; index < SIM_UNIT_CAPACITY && free_unit == SIM_NO_UNIT; ++index)
    {
        if (index >= sim->unit_count || !sim->units[index].alive) free_unit = index;
    }
    if (free_unit == SIM_NO_UNIT)                       return sim_refuse(check, SIM_REFUSAL_NO_ROOM);
    u32 action_points = transport ? sim->content.rules.ap_transport : sim->content.rules.ap_sortie;
    if (depot.city) action_points = sim_military_ap(sim, depot.city, action_points);   /* a port or a pass has no military office */
    if (sim->turns.action_points[force] < action_points) return sim_refuse(check, SIM_REFUSAL_NO_ACTION_POINTS);

    check.action_points = action_points;
    check.gold          = order->gold;
    check.quantity      = order->soldiers;
    check.value_before  = *depot.troops;
    check.value_after   = *depot.troops - order->soldiers;
    return check;
}

/* Takes an amount from a stock of the base, with one event. The event
   names the city. A port or a pass has no city entity: the commander of
   the unit names the event (sim_emit_unit). */
internal void
sim_depot_give(sim_state *sim, sim_depot *depot, u32 *stock, u32 amount, sim_event_kind kind, u32 unit_id, u32 cause,
               sim_event_sink *events)
{
    if (!amount) return;
    *stock -= amount;
    sim_emit_unit(sim, kind, unit_id, depot->city ? depot->city->city_id : (u8)SIM_NO_CITY, -(s32)amount, false, cause, events);
}

/* 出征 and 輸送: the officers leave the city with the soldiers, the weapons,
   the gold and the grain. The unit stands on a free hex next to the city
   with the morale of the city. */
internal void
sim_sortie(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_sortie_order *order = &command.sortie;
    b32 transport = command.kind == SIM_COMMAND_TRANSPORT;
    sim_check check = sim_check_sortie(sim, order, transport);
    if (check.refusal != SIM_REFUSAL_NONE) return;
    sim_depot depot = sim_depot_at(sim, order->city_id);
    u8 force = depot.force;
    u8 city_id = depot.city ? depot.city->city_id : (u8)SIM_NO_CITY;   /* the subject of the events; a port or a pass has none */
    unit_type_kind *type = sim_unit_type(sim, order->unit_type);
    u32 unit_id = sim_new_unit_id(sim);
    sim_unit *unit = sim->units + unit_id;
    b32 room = false;
    *unit = {};
    unit->alive       = true;
    unit->force_id    = force;
    unit->hex         = sim_sortie_hex(sim, order->city_id, force, type->movement, &room);
    unit->unit_type   = order->unit_type;
    unit->soldiers    = order->soldiers;
    unit->gold        = order->gold;
    unit->grain       = order->grain;
    unit->morale      = *depot.morale;
    unit->home_city   = order->city_id;
    unit->destination = transport ? order->destination : SIM_NO_CITY;
    unit->weapons     = sim_sortie_weapons(sim, type, order->soldiers);
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
    {
        unit->officers[place] = order->officers[place];
        officer_component *officer = sim_find_officer(sim, order->officers[place]);
        if (!officer) continue;
        officer->acted_turn       = sim->turn + 1;
        officer->current_position = (u16)(SIM_UNIT_POSITION_BASE + unit_id);
    }

    *depot.troops -= order->soldiers;
    sim_emit_unit(sim, SIM_EVENT_UNIT_SORTIED, unit_id, city_id, -(s32)order->soldiers, false, command.id, events);
    if (unit->weapons)
    {
        sim_depot_give(sim, &depot, depot.equipment + sim->content.weapons[type->weapon].stock, unit->weapons,
                       SIM_EVENT_UNIT_WEAPONS_TAKEN, unit_id, command.id, events);
    }
    if (!transport && order->ship)
    {
        /* G2-057: the unit takes one ship of the stock, and rows as it on the water. */
        unit_type_kind *ship = sim_unit_type(sim, order->ship);
        unit->has_ship  = true;
        unit->ship_type = order->ship;
        sim_depot_give(sim, &depot, depot.equipment + sim->content.weapons[ship->weapon].stock, 1,
                       SIM_EVENT_UNIT_WEAPONS_TAKEN, unit_id, command.id, events);
    }
    for (u32 stock = 0; transport && stock < CITY_EQUIPMENT_STOCK_COUNT; ++stock)
    {
        unit->cargo[stock] = order->cargo[stock];
        sim_depot_give(sim, &depot, depot.equipment + stock, order->cargo[stock], SIM_EVENT_UNIT_WEAPONS_TAKEN, unit_id, command.id, events);
    }
    sim_depot_give(sim, &depot, depot.gold, order->gold, SIM_EVENT_UNIT_SUPPLIES, unit_id, command.id, events);
    sim_depot_give(sim, &depot, depot.grain, order->grain, SIM_EVENT_UNIT_GRAIN_TAKEN, unit_id, command.id, events);
    sim->turns.action_points[force] -= check.action_points;
    sim_event points = sim_event_make(sim, SIM_EVENT_ACTION_POINTS_SPENT, command.id);
    points.faction = force;
    points.delta   = -(s32)check.action_points;
    sim_emit(events, points);
}

/* ---- a unit enters a city -------------------------------------------------- */

internal void
sim_city_take(sim_state *sim, city_component *city, u32 *stock, u32 cap, u32 amount, sim_event_kind kind, sim_event_kind over_kind,
              u32 unit_id, u32 cause, sim_event_sink *events)
{
    if (!amount) return;
    u64 total = (u64)*stock + amount;
    sim_emit_unit(sim, kind, unit_id, city->city_id, (s32)amount, false, cause, events);
    if (cap && total > cap)
    {
        if (over_kind != SIM_EVENT_NONE) sim_emit_city(sim, over_kind, city, -(s32)(total - cap), 0, false, cause, events);
        total = cap;
    }
    *stock = (u32)total;
}

/* The unit leaves the map into an own city: its soldiers, weapons, cargo,
   gold and grain go into the city, and its officers stay there (assumed
   for a transport unit and a city that is not the home city: city-commands.md
   lets the player choose, Q-171). */
internal void
sim_unit_enter_city(sim_state *sim, u32 unit_id, u8 city_id, sim_event_kind kind, u32 cause, sim_event_sink *events)
{
    sim_unit *unit = sim->units + unit_id;
    city_component *city = sim_find_city(sim, city_id);
    sim_rules *rules = &sim->content.rules;
    if (kind == SIM_EVENT_UNIT_RETREATED) sim_emit_unit(sim, kind, unit_id, city_id, 0, false, cause, events);
    unit_type_kind *type = sim_unit_type(sim, unit->unit_type);
    if (city)
    {
        sim_city_take(sim, city, &city->troops, sim_city_soldier_cap(sim, city_id), unit->soldiers, SIM_EVENT_UNIT_ENTERED_CITY,
                      SIM_EVENT_NONE, unit_id, cause, events);
        if (unit->has_ship)
        {
            /* G2-057: the ship of the unit goes back to the stock of the city. */
            unit_type_kind *ship = sim_unit_type(sim, unit->ship_type);
            if (ship && ship->weapon < sim->content.weapon_count)
            {
                u32 *ship_stock = city->equipment_stocks + sim->content.weapons[ship->weapon].stock;
                *ship_stock = (u32)Minimum((u64)*ship_stock + 1, (u64)rules->city_weapon_cap);
            }
        }
        if (type && type->weapon < sim->content.weapon_count && unit->weapons)
        {
            u32 *stock = city->equipment_stocks + sim->content.weapons[type->weapon].stock;
            *stock = (u32)Minimum((u64)*stock + unit->weapons, (u64)rules->city_weapon_cap);
        }
        for (u32 stock = 0; stock < CITY_EQUIPMENT_STOCK_COUNT; ++stock)
        {
            city->equipment_stocks[stock] = (u32)Minimum((u64)city->equipment_stocks[stock] + unit->cargo[stock], (u64)rules->city_weapon_cap);
        }
        sim_city_take(sim, city, &city->gold, rules->city_gold_cap, unit->gold, SIM_EVENT_UNIT_GOLD_RETURNED, SIM_EVENT_GOLD_OVER_CAP,
                      unit_id, cause, events);
        sim_city_take(sim, city, &city->food, rules->city_grain_cap, unit->grain, SIM_EVENT_UNIT_GRAIN_RETURNED, SIM_EVENT_GRAIN_OVER_CAP,
                      unit_id, cause, events);
    }
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
    {
        officer_component *officer = sim_find_officer(sim, unit->officers[place]);
        if (!officer) continue;
        officer->current_position = city_id;
        officer->base_location    = city_id;
    }
    unit->alive = false;
}

/* Puts an amount into a stock of a port or a pass, up to the cap. */
internal void
sim_base_take(u32 *stock, u32 amount, u32 cap)
{
    u64 total = (u64)*stock + amount;
    *stock = (u32)(cap && total > cap ? cap : total);
}

/* The unit leaves the map into an own port or pass (the play report of
   2026-10-23). The base is a base of the spec (siege-and-bases.md,
   ui/city-commands.md): its soldiers, weapons, cargo, gold and grain go
   into the base and its stock, and its officers stay there. The soldiers
   stop at the cap of the base, as in a city. A base without a morale takes
   the morale of the unit (assumed). The ledger gets unit_entered_base,
   named by the commander. */
internal void
sim_unit_enter_base(sim_state *sim, u32 unit_id, u32 base_index, sim_event_kind kind, u32 cause, sim_event_sink *events)
{
    sim_unit *unit = sim->units + unit_id;
    sim_base_state *base = sim_find_base(sim, base_index);
    if (!base) return;
    sim_base_stock *stock = sim->base_stocks + base_index;
    sim_rules *rules = &sim->content.rules;
    u16 location = (u16)sim->content.bases[base_index].id;
    if (kind == SIM_EVENT_UNIT_RETREATED) sim_emit_unit(sim, kind, unit_id, SIM_NO_CITY, 0, false, cause, events);
    unit_type_kind *type = sim_unit_type(sim, unit->unit_type);
    sim_base_take(&base->troops, unit->soldiers, sim_base_troop_cap(sim, base->force));
    if (unit->has_ship)
    {
        unit_type_kind *ship = sim_unit_type(sim, unit->ship_type);
        if (ship && ship->weapon < sim->content.weapon_count) sim_base_take(stock->equipment_stocks + sim->content.weapons[ship->weapon].stock, 1, rules->city_weapon_cap);
    }
    if (type && type->weapon < sim->content.weapon_count && unit->weapons)
    {
        sim_base_take(stock->equipment_stocks + sim->content.weapons[type->weapon].stock, unit->weapons, rules->city_weapon_cap);
    }
    for (u32 index = 0; index < CITY_EQUIPMENT_STOCK_COUNT; ++index) sim_base_take(stock->equipment_stocks + index, unit->cargo[index], rules->city_weapon_cap);
    sim_base_take(&stock->gold, unit->gold, rules->city_gold_cap);
    sim_base_take(&stock->grain, unit->grain, rules->city_grain_cap);
    if (!stock->morale) stock->morale = (u8)Minimum(unit->morale, 255u);
    sim_event event = sim_event_make(sim, SIM_EVENT_UNIT_ENTERED_BASE, cause);
    event.subject = unit->officers[0] != SIM_NO_OFFICER ? sim_officer_entity(sim, unit->officers[0]) : 0;
    event.faction = unit->force_id;
    event.detail  = location;
    event.delta   = (s32)unit->soldiers;
    sim_emit(events, event);
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
    {
        officer_component *officer = sim_find_officer(sim, unit->officers[place]);
        if (!officer) continue;
        officer->current_position = location;
        officer->base_location    = location;
    }
    unit->alive = false;
}

/* The unit enters the base of the location: a city, or a port or a pass. */
internal void
sim_unit_enter_depot(sim_state *sim, u32 unit_id, u8 location, sim_event_kind kind, u32 cause, sim_event_sink *events)
{
    if (location < SAVE_CITY_COUNT) sim_unit_enter_city(sim, unit_id, location, kind, cause, events);
    else if (location >= CONTENT_FIRST_BASE) sim_unit_enter_base(sim, unit_id, (u32)location - CONTENT_FIRST_BASE, kind, cause, events);
}

/* The home city of a unit for a captive or an item: the home when it is
   a city of the force, else a refuge city. A unit from a port or a pass
   has no home city. */
internal u8
sim_unit_home_city(sim_state *sim, sim_unit *unit)
{
    return sim_refuge_city(sim, unit->force_id, unit->home_city);
}

/* The officer of a 出征 or 輸送 from a base: in the base, of its force,
   free and not yet acted. A city uses the check of the city commands. */
internal sim_refusal
sim_check_depot_officer(sim_state *sim, sim_depot *depot, officer_component *officer)
{
    if (depot->city) return sim_check_develop_officer(sim, depot->city, officer);
    if (!officer || !sim_officer_is_active(officer) || officer->force_id != depot->force || officer->current_position != depot->location)
    {
        return SIM_REFUSAL_OFFICER_NOT_IN_CITY;
    }
    if (officer->acted_turn == sim->turn + 1) return SIM_REFUSAL_OFFICER_ALREADY_ACTED;
    if (officer->job_city != SIM_NO_CITY)     return SIM_REFUSAL_OFFICER_BUSY;
    if (officer->travel_turns)                return SIM_REFUSAL_OFFICER_BUSY;
    return SIM_REFUSAL_NONE;
}

/* The officers of a 出征 or 輸送, as sim_check_city_officers for a base. */
internal sim_refusal
sim_check_depot_officers(sim_state *sim, sim_depot *depot, const u16 *officers, u32 max_officers, sim_ability ability, u32 *sum)
{
    u32 named = 0;
    *sum = 0;
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
    {
        u16 officer_id = officers[place];
        if (officer_id == SIM_NO_OFFICER) continue;
        named++;
        officer_component *officer = sim_find_officer(sim, officer_id);
        sim_refusal refusal = sim_check_depot_officer(sim, depot, officer);
        if (refusal != SIM_REFUSAL_NONE) return refusal;
        for (u32 earlier = 0; earlier < place; ++earlier)
        {
            if (officers[earlier] == officer_id) return SIM_REFUSAL_DUPLICATE_OFFICER;
        }
        *sum += sim_officer_ability(officer, ability);
    }
    if (named == 0)           return SIM_REFUSAL_NO_OFFICER;
    if (named > max_officers) return SIM_REFUSAL_TOO_MANY_OFFICERS;
    return SIM_REFUSAL_NONE;
}

/* ---- the orders of a unit --------------------------------------------------- */

/* The checks that every unit order shares: the unit, its owner, its turn. */
internal sim_refusal
sim_check_unit_common(sim_state *sim, sim_unit_order *order, sim_unit **unit_out)
{
    if (!sim->has_content) return SIM_REFUSAL_NO_CONTENT;
    if (sim_game_over(sim)) return SIM_REFUSAL_GAME_OVER;
    if (sim_busy(sim)) return SIM_REFUSAL_BUSY;   /* M7: a duel or a debate runs */
    sim_unit *unit = sim_find_unit(sim, order->unit);
    if (!unit || !sim_unit_type(sim, unit->unit_type)) return SIM_REFUSAL_NO_UNIT;
    if (sim_whose_turn(sim) != unit->force_id) return SIM_REFUSAL_NOT_YOUR_TURN;
    if (unit->aid_turns) return SIM_REFUSAL_NO_UNIT;   /* M7: an aid unit of an ally follows its own faction */
    if (unit->acted_turn == sim->turn + 1) return SIM_REFUSAL_UNIT_ACTED;
    *unit_out = unit;
    return SIM_REFUSAL_NONE;
}

internal sim_check
sim_check_unit_order_at(sim_state *sim, sim_command_kind kind, sim_unit_order *order, hex_coord hex, memory_arena *scratch)
{
    sim_unit *unit = order->unit < sim->unit_count ? sim->units + order->unit : 0;
    if (!unit) return sim_check_unit_order(sim, kind, order, scratch);
    hex_coord home = unit->hex;
    unit->hex = hex;
    sim_check check = sim_check_unit_order(sim, kind, order, scratch);
    unit->hex = home;
    return check;
}

internal sim_check
sim_check_unit_order(sim_state *sim, sim_command_kind kind, sim_unit_order *order, memory_arena *scratch)
{
    switch (kind)   /* the orders of M6 have their own checks (combat.cpp, field.cpp) */
    {
        case SIM_COMMAND_DESTROY_TARGET:
        case SIM_COMMAND_DRIVE_TARGET:
        case SIM_COMMAND_FOLLOW:
        case SIM_COMMAND_DELEGATE:
        case SIM_COMMAND_ATTACK_FACILITY: return sim_check_long_order(sim, kind, order, scratch);   /* orders.cpp */
        case SIM_COMMAND_ATTACK:       return sim_check_attack(sim, order);
        case SIM_COMMAND_JOINT_ATTACK: return sim_check_joint_attack(sim, order);
        case SIM_COMMAND_TACTIC:       return sim_check_tactic(sim, order);
        case SIM_COMMAND_PLOT:         return sim_check_plot(sim, order);
        case SIM_COMMAND_BUILD:
        case SIM_COMMAND_REPAIR:
        case SIM_COMMAND_DISMANTLE:    return sim_check_build(sim, kind, order);
        default:                       break;
    }
    sim_check check = {};
    check.slot = SIM_ANY_SLOT;
    sim_unit *unit = 0;
    sim_refusal refusal = sim_check_unit_common(sim, order, &unit);
    if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
    /* A unit in 混亂 or 偽報 cannot act (plots-and-states.md); it can only wait. */
    if (kind != SIM_COMMAND_WAIT && unit->state != UNIT_STATE_NONE) return sim_refuse(check, SIM_REFUSAL_UNIT_CONFUSED);
    switch (kind)
    {
        case SIM_COMMAND_MOVE:
        {
            if (unit->moved_turn == sim->turn + 1) return sim_refuse(check, SIM_REFUSAL_UNIT_ACTED);
            u64 mark = scratch->used;
            sim_reach reach = sim_unit_reach(sim, order->unit, scratch);
            u32 place = sim_reach_find(&reach, order->to);
            if (place == 0) return sim_refuse(check, SIM_REFUSAL_UNREACHABLE);
            check.assumed = true;
            if (place != U32_MAX)
            {
                check.days = reach.costs[place];
                return check;
            }
            /* G2-058: a hex out of the range is a long order 移動. The days
               are the turns of the plan (orders.cpp). A hex with a unit is
               no goal. */
            if (sim_unit_at(sim, order->to) != SIM_NO_UNIT) return sim_refuse(check, SIM_REFUSAL_UNREACHABLE);
            /* The reach is done. A plan needs the whole scratch on the real
               map: two searches at once overflow it (the transport that
               could not move, 2026-10-22). */
            scratch->used = mark;
            sim_reach plan = sim_unit_plan(sim, order->unit, order->to, scratch);
            place = sim_reach_find(&plan, order->to);
            if (place == U32_MAX) return sim_refuse(check, SIM_REFUSAL_UNREACHABLE);
            check.days  = sim_plan_days(sim, unit, plan.costs[place]);
            check.turns = check.days / CALENDAR_DAYS_PER_TURN;
            return check;
        }
        case SIM_COMMAND_WAIT:
            return check;
        case SIM_COMMAND_RETREAT:
        {
            sim_depot home = sim_depot_at(sim, unit->home_city);   /* a city, or a port or a pass */
            if (!home.ok || home.force != unit->force_id) return sim_refuse(check, SIM_REFUSAL_NO_CITY);
            return check;
        }
        case SIM_COMMAND_SUPPLY:
        {
            sim_unit *target = sim_find_unit(sim, order->target);
            unit_type_kind *type = sim_unit_type(sim, unit->unit_type);
            if (!type->transport) return sim_refuse(check, SIM_REFUSAL_NO_UNIT);
            if (!target || order->target == order->unit || target->force_id != unit->force_id || !sim_unit_type(sim, target->unit_type) ||
                hex_distance(target->hex, unit->hex) != 1)
            {
                return sim_refuse(check, SIM_REFUSAL_NOT_ADJACENT);
            }
            if (order->soldiers + order->gold + order->grain == 0) return sim_refuse(check, SIM_REFUSAL_NO_AMOUNT);
            if (order->soldiers > unit->soldiers || target->soldiers + order->soldiers > sim_unit_soldier_cap(sim, unit->force_id))
            {
                return sim_refuse(check, SIM_REFUSAL_NO_SOLDIERS);
            }
            if (order->gold > unit->gold)   return sim_refuse(check, SIM_REFUSAL_NO_GOLD);
            if (order->grain > unit->grain) return sim_refuse(check, SIM_REFUSAL_NO_GRAIN);
            check.quantity = order->soldiers;
            return check;
        }
        default:
            return sim_refuse(check, SIM_REFUSAL_NO_UNIT);
    }
}

/* The soldiers that a hex takes on entry (plank road, poison spring), unless
   the faction has the technique or an officer has the skill. */
internal void
sim_terrain_loss(sim_state *sim, u32 unit_id, hex_coord hex, u32 cause, sim_event_sink *events)
{
    sim_unit *unit = sim->units + unit_id;
    terrain_kind *terrain = sim_terrain_at(sim, hex);
    if (!terrain || !terrain->loss_percent || !unit->soldiers) return;
    if (terrain->loss_waiver_technique != CONTENT_NO_TECHNIQUE && sim_has_technique(sim, unit->force_id, terrain->loss_waiver_technique)) return;
    if (sim_unit_has_skill(sim, unit->officers, terrain->loss_waiver_skill)) return;
    u32 lost = sim_percent_of(unit->soldiers, terrain->loss_percent);
    if (!lost) lost = 1;
    unit->soldiers -= lost;
    sim_emit_unit(sim, SIM_EVENT_UNIT_SOLDIERS_LOST, unit_id, SIM_NO_CITY, -(s32)lost, true, cause, events);
}

/* 移動: the unit walks its path. Each hex can take soldiers. An own city at
   the end takes the unit in; for a transport unit at its destination this
   delivers the cargo. */
internal void
sim_move_unit(sim_state *sim, sim_command command, memory_arena *scratch, sim_event_sink *events)
{
    sim_unit_order *order = &command.unit;
    u64 mark = scratch->used;
    sim_check check = sim_check_unit_order(sim, SIM_COMMAND_MOVE, order, scratch);
    if (check.refusal != SIM_REFUSAL_NONE) return;
    /* The check built a reach of its own. On the real map one reach takes
       most of the scratch arena, so give its memory back before the move
       builds the reach of the path. */
    scratch->used = mark;
    sim_reach reach = sim_unit_reach(sim, order->unit, scratch);
    u32 place = sim_reach_find(&reach, order->to);
    if (place == U32_MAX)
    {
        /* G2-058: the hex is out of the range. The unit takes the long
           order 移動 and does its first step now (orders.cpp). */
        sim_give_unit_order(sim, order->unit, UNIT_ORDER_MOVE, order->to, SIM_NO_UNIT, command.id, events);
        sim_unit_step(sim, order->unit, order->to, false, command.id, events);
        sim_unit *walker = sim_find_unit(sim, order->unit);
        if (walker && hex_equal(walker->hex, order->to)) sim_end_unit_order(sim, order->unit, command.id, events);
        return;
    }
    u32 path[SIM_UNIT_PATH_CAPACITY];
    u32 length = 0;
    for (u32 at = place; at != U32_MAX && at != 0 && length < SIM_UNIT_PATH_CAPACITY; at = reach.from[at]) path[length++] = at;
    sim_unit *unit = sim->units + order->unit;
    unit->moved_turn = sim->turn + 1;
    sim_emit_unit(sim, SIM_EVENT_UNIT_MOVED, order->unit, SIM_NO_CITY, (s32)reach.costs[place], true, command.id, events);
    while (length)
    {
        hex_coord step = reach.cells[path[--length]];
        unit->hex = step;
        sim_terrain_loss(sim, order->unit, step, command.id, events);
    }
    u8 city_id = sim_city_at(sim, unit->hex);
    if (city_id != SIM_NO_CITY)
    {
        unit_type_kind *type = sim_unit_type(sim, unit->unit_type);
        b32 delivered = type && type->transport && unit->destination == city_id;
        u16 officers[SIM_COMMAND_OFFICERS] = { unit->officers[0], unit->officers[1], unit->officers[2] };
        u8 home = unit->home_city, force = unit->force_id;
        sim_unit_enter_city(sim, order->unit, city_id, SIM_EVENT_NONE, command.id, events);
        /* G2-027: a transport at its city asks where its officers go (events.cpp). */
        if (delivered) sim_transport_arrived(sim, officers, city_id, home, force, command.id, events);
        return;
    }
    /* A unit of the player that ends its move on an own port or pass enters
       it (the play report of 2026-10-23). The reach lets a unit stop only on
       a base of its own faction (sim_entry_cost_of). ASSUMED: a unit on a
       long order passes the base as a waypoint and stands on its hex, so
       that a gate at the edge of its reach does not swallow the order. It
       enters when the base is the goal. A computer unit never enters: it has
       no sortie from a base. */
    u32 base = sim_base_at(sim, unit->hex);
    b32 human = unit->force_id < SIM_FORCE_CAPACITY && sim->turns.human[unit->force_id];
    b32 free_or_goal = unit->order == UNIT_ORDER_NONE || hex_equal(unit->order_hex, unit->hex);
    if (base != SIM_NO_BASE && sim->bases[base].force == unit->force_id && human && free_or_goal)
    {
        sim_unit_enter_base(sim, order->unit, base, SIM_EVENT_NONE, command.id, events);
    }
}

/* 待命, 退卻 and 補給. */
internal void
sim_unit_action(sim_state *sim, sim_command command, memory_arena *scratch, sim_event_sink *events)
{
    sim_unit_order *order = &command.unit;
    sim_check check = sim_check_unit_order(sim, command.kind, order, scratch);
    if (check.refusal != SIM_REFUSAL_NONE) return;
    sim_unit *unit = sim->units + order->unit;
    unit->acted_turn = sim->turn + 1;
    switch (command.kind)
    {
        case SIM_COMMAND_WAIT:
            sim_emit_unit(sim, SIM_EVENT_UNIT_WAITED, order->unit, SIM_NO_CITY, 0, false, command.id, events);
            sim_events_after_wait(sim, order->unit, command.id, events);   /* M8: ruins, temples, spouses, sworn brothers */
            break;
        case SIM_COMMAND_RETREAT:
            /* ASSUMED (Q-171): the unit returns to its home city at once. */
            sim_unit_enter_depot(sim, order->unit, unit->home_city, SIM_EVENT_UNIT_RETREATED, command.id, events);
            break;
        case SIM_COMMAND_SUPPLY:
        {
            sim_unit *target = sim->units + order->target;
            unit->soldiers -= order->soldiers;  target->soldiers += order->soldiers;
            unit->gold     -= order->gold;      target->gold     += order->gold;
            unit->grain    -= order->grain;     target->grain    += order->grain;
            sim_emit_unit(sim, SIM_EVENT_UNIT_SUPPLIED, order->target, SIM_NO_CITY, (s32)order->soldiers, false, command.id, events);
            break;
        }
        default:
            break;
    }
}

/* ---- each turn --------------------------------------------------------------- */

/* Each turn each unit eats one grain for each unit.grain_soldiers soldiers
   (G2-049: 2043 soldiers ate 204, 6730 ate 673). Without grain the soldiers
   that the missing grain feeds leave (assumed, Q-160). */
internal void
sim_run_units_turn(sim_state *sim, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    /* A unit of the player that stands on an own port or pass without a
       long order enters it at the turn (the player, 2026-10-24: a unit on
       a base is in the base, as in a city). */
    for (u32 unit_id = 0; unit_id < sim->unit_count; ++unit_id)
    {
        sim_unit *unit = sim->units + unit_id;
        if (!unit->alive || unit->order != UNIT_ORDER_NONE || unit->force_id >= SIM_FORCE_CAPACITY || !sim->turns.human[unit->force_id]) continue;
        u32 base = sim_base_at(sim, unit->hex);
        if (base != SIM_NO_BASE && sim->bases[base].force == unit->force_id) sim_unit_enter_base(sim, unit_id, base, SIM_EVENT_NONE, cause, events);
    }
    if (!rules->unit_grain_soldiers) return;
    for (u32 unit_id = 0; unit_id < sim->unit_count; ++unit_id)
    {
        sim_unit *unit = sim->units + unit_id;
        if (!unit->alive || !unit->soldiers || unit->grain_unknown) continue;
        /* 屯田 #79: next to an own port or pass, the soldiers use no grain (the distance is assumed, Q-234). */
        if (sim_unit_has_trait(sim, unit->officers, TRAIT_BASE_NO_GRAIN))
        {
            b32 at_base = false;
            for (u32 base = 0; base < sim->content.base_count && !at_base; ++base)
            {
                at_base = sim->bases[base].exists && sim->bases[base].force == unit->force_id && hex_distance(sim->bases[base].hex, unit->hex) <= 1;
            }
            if (at_base) continue;
        }
        u32 needed = unit->soldiers / rules->unit_grain_soldiers;
        u32 eaten  = needed < unit->grain ? needed : unit->grain;
        unit->grain -= eaten;
        if (eaten) sim_emit_unit(sim, SIM_EVENT_UNIT_GRAIN_EATEN, unit_id, SIM_NO_CITY, -(s32)eaten, true, cause, events);
        if (eaten < needed)
        {
            u64 hungry = (u64)(needed - eaten) * rules->unit_grain_soldiers;
            u32 lost = hungry < unit->soldiers ? (u32)hungry : unit->soldiers;
            unit->soldiers -= lost;
            if (lost) sim_emit_unit(sim, SIM_EVENT_UNIT_DESERTED, unit_id, SIM_NO_CITY, -(s32)lost, true, cause, events);
        }
    }
}

/* ---- 自動編制 (the proposal of the formation dialog) ------------------------ */

/* The proposal of the guide book (p.35): the officers with the best
   aptitude for the class, then the highest sum of force and leadership,
   then merit. The soldiers: the most that the city, the weapons and the
   commander allow. The grain: 30 turns of food (a proposal, not a rule). */
internal sim_sortie_order
sim_sortie_proposal(sim_state *sim, u8 city_id, u8 unit_type)
{
    sim_sortie_order order = {};
    order.city_id     = city_id;
    order.unit_type   = unit_type;
    order.destination = SIM_NO_CITY;
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place) order.officers[place] = SIM_NO_OFFICER;
    sim_depot depot = sim_depot_at(sim, city_id);   /* a city, or a port or a pass */
    unit_type_kind *type = sim_unit_type(sim, unit_type);
    if (!depot.ok || !type) return order;
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
    {
        officer_component *best = 0;
        for (u32 index = 0; index < sim->officers.count; ++index)
        {
            officer_component *officer = sim->officers.data + index;
            if (sim_check_depot_officer(sim, &depot, officer) != SIM_REFUSAL_NONE) continue;
            b32 taken = false;
            for (u32 earlier = 0; earlier < place; ++earlier) taken |= order.officers[earlier] == officer->officer_id;
            if (taken) continue;
            if (!best) { best = officer; continue; }
            u32 apt_a = type->aptitude < OFFICER_UNIT_APTITUDE_COUNT ? officer->unit_aptitudes[type->aptitude] : 0;
            u32 apt_b = type->aptitude < OFFICER_UNIT_APTITUDE_COUNT ? best->unit_aptitudes[type->aptitude] : 0;
            u32 sum_a = (u32)officer->war + officer->leadership, sum_b = (u32)best->war + best->leadership;
            if (apt_a != apt_b ? apt_a > apt_b : (sum_a != sum_b ? sum_a > sum_b : officer->merit > best->merit)) best = officer;
        }
        if (best) order.officers[place] = best->officer_id;
    }
    u32 soldiers = Minimum(*depot.troops, sim_officer_soldier_cap(sim, sim_find_officer(sim, order.officers[0])));
    if (type->weapon < sim->content.weapon_count && !sim->content.weapons[type->weapon].max_days)
    {
        soldiers = Minimum(soldiers, depot.equipment[sim->content.weapons[type->weapon].stock]);
    }
    if (type->transport) soldiers = 0;
    order.soldiers = soldiers;
    /* G2-044: the preset proposes sortie.grain_per_soldier grain for each soldier (2043 soldiers, 4086 grain). */
    u32 grain = soldiers * sim->content.rules.sortie_grain_per_soldier;
    order.grain = Minimum(grain, *depot.grain);
    return order;
}
