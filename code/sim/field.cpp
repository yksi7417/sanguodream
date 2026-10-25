/* The field of battle (plots-and-states.md, military-facilities-and-traps.md,
   terrain-and-map.md, siege-and-bases.md): 計略 (the plots of plots.tsv), the
   states of a unit, fire on the map, 設置 / 修補 / 撤除 of the buildings of
   buildings.tsv, the traps and the natural traps, and the effects of each
   turn: states, fire, towers, music, mazes, and the blockade and recovery
   of the bases. Included by sim.cpp after combat.cpp. The amounts are
   assumed (Q-173 to Q-175). */

/* ---- states ----------------------------------------------------------------- */

internal void
sim_set_unit_state(sim_state *sim, u32 unit_id, sim_unit_state state, u32 turns, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    sim_unit *unit = sim->units + unit_id;
    if (!unit->alive) return;
    if (state == UNIT_STATE_CONFUSED && (sim_unit_has_skill(sim, unit->officers, rules->skill_calm) ||
                                         sim_unit_has_skill(sim, unit->officers, rules->skill_mirror))) return;
    if (state == UNIT_STATE_FALSE_REPORT && (sim_unit_has_skill(sim, unit->officers, rules->skill_discipline) ||
                                             sim_unit_has_skill(sim, unit->officers, rules->skill_mirror))) return;
    unit->state       = (u8)state;
    unit->state_turns = (u8)(turns < 255 ? turns : 255);
    sim_emit_unit(sim, SIM_EVENT_UNIT_STATE, unit_id, SIM_NO_CITY, (s32)state, false, cause, events);
}

/* ---- fire ----------------------------------------------------------------------- */

/* The turns of a new fire (terrain-and-map.md, F-149): 1 to 3 at random
   for a fire from an attack, 1 to 7 for another source (Q-198 asks which
   action is which). A pack without these rules uses fire.turns. */
internal u32
sim_fire_turns(sim_state *sim, b32 from_attack)
{
    sim_rules *rules = &sim->content.rules;
    u32 low  = from_attack ? rules->fire_attack_turns_min : rules->fire_other_turns_min;
    u32 high = from_attack ? rules->fire_attack_turns_max : rules->fire_other_turns_max;
    if (!high || high < low) return rules->fire_turns;
    return low + sim_random_below(sim, high - low + 1);
}

/* A hex keeps a count of fire turns, 0 to 7 (F-149). A fire stays only on
   a burnable terrain (the 12 terrains of terrain-and-map.md). On any other
   terrain (河 and 海 too) a fire starts only on a hex with a unit. */
internal void
sim_start_fire(sim_state *sim, hex_coord hex, u32 turns, u32 cause, sim_event_sink *events)
{
    terrain_kind *terrain = sim_terrain_at(sim, hex);
    if (!terrain || !sim->fire || !turns) return;
    if (!terrain->burnable && sim_unit_at(sim, hex) == SIM_NO_UNIT) return;
    if (turns > SIM_FIRE_MAX_TURNS) turns = SIM_FIRE_MAX_TURNS;
    u32 cell = hex_map_index(&sim->map, hex);
    if (sim->fire[cell] >= turns) return;
    sim->fire[cell] = (u8)turns;
    sim_emit_cell(sim, SIM_EVENT_FIRE_STARTED, cell, (s32)turns, true, cause, events);
}

internal b32
sim_hex_burns(sim_state *sim, hex_coord hex)
{
    return sim->fire && hex_map_contains(&sim->map, hex) && sim->fire[hex_map_index(&sim->map, hex)] > 0;
}

/* The soldiers that fire takes from a unit: 藤甲 doubles it, 火神 stops it. */
internal void
sim_burn_unit(sim_state *sim, u32 unit_id, u32 percent, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    sim_unit *unit = sim->units + unit_id;
    if (!unit->alive || sim_unit_has_skill(sim, unit->officers, rules->skill_fire_god)) return;
    u32 loss = sim_percent_of(unit->soldiers, percent);
    if (sim_unit_has_skill(sim, unit->officers, rules->skill_rattan)) loss *= 2;
    if (!loss && unit->soldiers) loss = 1;
    sim_hurt_unit(sim, unit_id, loss, SIM_NO_UNIT, false, cause, events);
}

/* ---- buildings ------------------------------------------------------------------ */

internal u32
sim_add_building(sim_state *sim, u8 kind, u8 force, hex_coord hex, u32 durability, u8 city_id)
{
    u32 id = SIM_NO_BUILDING;
    for (u32 index = 0; index < sim->building_count && id == SIM_NO_BUILDING; ++index)
    {
        if (!sim->buildings[index].alive) id = index;
    }
    if (id == SIM_NO_BUILDING)
    {
        if (sim->building_count >= SIM_BUILDING_CAPACITY) return SIM_NO_BUILDING;
        id = sim->building_count++;
    }
    sim_building *building = sim->buildings + id;
    *building = {};
    building->alive      = true;
    building->kind       = kind;
    building->force_id   = force;
    building->hex        = hex;
    building->durability = durability;
    building->complete   = durability >= sim->content.buildings[kind].durability;
    building->city_id    = city_id;
    return id;
}

internal b32
sim_building_is_military(building_kind *kind)
{
    return kind->type == BUILDING_CAMP || kind->type == BUILDING_TOWER || kind->type == BUILDING_DRUM ||
           kind->type == BUILDING_MUSIC || kind->type == BUILDING_MAZE;
}

internal b32
sim_building_is_trap(building_kind *kind)
{
    return kind->type == BUILDING_SEED || kind->type == BUILDING_BALL || kind->type == BUILDING_SHIP;
}

/* A building that the faction can build: its technique, and no stronger
   version that replaces it (manual p.16). */
internal b32
sim_building_available(sim_state *sim, u8 force, u32 kind_id)
{
    building_kind *kind = sim->content.buildings + kind_id;
    if (kind->natural) return false;
    if (kind->technique != CONTENT_NO_TECHNIQUE && !sim_has_technique(sim, force, kind->technique)) return false;
    for (u32 index = 0; index < sim->content.building_count; ++index)
    {
        building_kind *strong = sim->content.buildings + index;
        if (strong->replaces != kind_id) continue;
        if (strong->technique == CONTENT_NO_TECHNIQUE || sim_has_technique(sim, force, strong->technique)) return false;
    }
    return true;
}

/* The placement rules of manual p.16: no base and no building on the hex;
   no military facility within 2 hexes of a base or of another military
   facility; no obstacle or trap within 2 hexes of a base. F-147 (M10): a
   fire ship stands only on 河 and 海; a military facility, an obstacle or
   a fire trap stands only on a terrain with "buildable" (草地, 土, 砂地
   and 主徑 in facility-types.tsv). */
internal b32
sim_can_place(sim_state *sim, building_kind *kind, hex_coord hex)
{
    terrain_kind *terrain = sim_terrain_at(sim, hex);
    if (!terrain || sim_building_at(sim, hex) != SIM_NO_BUILDING || sim_city_at(sim, hex) != SIM_NO_CITY || sim_unit_at(sim, hex) != SIM_NO_UNIT) return false;
    if (sim_base_at(sim, hex) != SIM_NO_BASE) return false;
    if (kind->water ? !sim_hex_is_water(sim, hex) : !terrain->buildable) return false;
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        if (hex_distance(sim->sites[sim->cities.data[index].city_id].hex, hex) <= 2) return false;
    }
    if (sim_building_is_military(kind))
    {
        for (u32 index = 0; index < sim->building_count; ++index)
        {
            sim_building *other = sim->buildings + index;
            if (other->alive && sim_building_is_military(sim->content.buildings + other->kind) && hex_distance(other->hex, hex) <= 2) return false;
        }
    }
    return true;
}

internal sim_check
sim_check_build(sim_state *sim, sim_command_kind kind, sim_unit_order *order)
{
    sim_check check = {};
    check.slot = SIM_ANY_SLOT;
    sim_unit *unit = 0;
    sim_refusal refusal = sim_check_combat_unit(sim, order, &unit);
    if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
    unit_type_kind *type = sim_unit_type(sim, unit->unit_type);
    if (type->movement == MOVEMENT_SIEGE) return sim_refuse(check, SIM_REFUSAL_NO_APTITUDE);   /* manual p.14 */
    if (hex_distance(unit->hex, order->to) != 1) return sim_refuse(check, SIM_REFUSAL_NO_TARGET);
    sim_unit_values values = sim_unit_values_of(sim, unit->force_id, unit->unit_type, unit->officers);
    u32 work = values.build * sim->content.rules.build_durability_per_build;
    if (kind == SIM_COMMAND_BUILD)
    {
        if (order->kind >= sim->content.building_count || !sim_building_available(sim, unit->force_id, order->kind))
        {
            return sim_refuse(check, SIM_REFUSAL_NEEDS_TECHNIQUE);
        }
        building_kind *building = sim->content.buildings + order->kind;
        if (!sim_can_place(sim, building, order->to)) return sim_refuse(check, SIM_REFUSAL_NOT_BUILDABLE);
        if (unit->gold < building->gold) return sim_refuse(check, SIM_REFUSAL_NO_GOLD);
        check.gold     = building->gold;
        check.quantity = work < building->durability ? work : building->durability;
        check.assumed  = true;
        return check;
    }
    u32 target = sim_building_at(sim, order->to);
    if (target == SIM_NO_BUILDING || sim->buildings[target].force_id != unit->force_id) return sim_refuse(check, SIM_REFUSAL_NO_TARGET);
    sim_building *building = sim->buildings + target;
    if (kind == SIM_COMMAND_REPAIR)
    {
        u32 maximum = sim->content.buildings[building->kind].durability;
        if (building->durability >= maximum) return sim_refuse(check, SIM_REFUSAL_VALUE_AT_MAXIMUM);
        check.quantity = work < maximum - building->durability ? work : maximum - building->durability;
        check.assumed  = true;
        return check;
    }
    /* 撤除 (manual p.24): 10 AP; the technique points decrease. */
    if (sim->turns.action_points[unit->force_id] < sim->content.rules.ap_dismantle) return sim_refuse(check, SIM_REFUSAL_NO_ACTION_POINTS);
    check.action_points = sim->content.rules.ap_dismantle;
    return check;
}

/* 設置, 修補 and 撤除. A building works when its durability is full. */
internal void
sim_build(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_unit_order *order = &command.unit;
    sim_check check = sim_check_build(sim, command.kind, order);
    if (check.refusal != SIM_REFUSAL_NONE) return;
    sim_unit *unit = sim->units + order->unit;
    unit->acted_turn = sim->turn + 1;
    if (command.kind == SIM_COMMAND_BUILD)
    {
        building_kind *kind = sim->content.buildings + order->kind;
        unit->gold -= check.gold;
        u8 owner = sim_building_is_trap(kind) ? (u8)SIM_NO_FORCE : unit->force_id;   /* a trap has no owner (Steam guide) */
        u32 id = sim_add_building(sim, order->kind, owner, order->to, check.quantity, SIM_NO_CITY);
        if (id == SIM_NO_BUILDING) return;
        sim_emit_building(sim, SIM_EVENT_BUILDING_STARTED, id, order->kind, true, command.id, events);
        if (sim->buildings[id].complete) sim_emit_building(sim, SIM_EVENT_BUILDING_COMPLETED, id, 0, false, command.id, events);
        return;
    }
    u32 id = sim_building_at(sim, order->to);
    sim_building *building = sim->buildings + id;
    if (command.kind == SIM_COMMAND_REPAIR)
    {
        building->durability += check.quantity;
        if (!building->complete && building->durability >= sim->content.buildings[building->kind].durability)
        {
            building->complete = true;
            sim_emit_building(sim, SIM_EVENT_BUILDING_COMPLETED, id, 0, false, command.id, events);
        }
        return;
    }
    building->alive = false;
    sim_emit_building(sim, SIM_EVENT_BUILDING_DESTROYED, id, 0, false, command.id, events);
    sim->turns.action_points[unit->force_id] -= check.action_points;
    u32 taken = sim->content.rules.dismantle_technique < sim->technique_points[unit->force_id] ? sim->content.rules.dismantle_technique
                                                                                               : sim->technique_points[unit->force_id];
    sim->technique_points[unit->force_id] -= taken;
    if (taken) sim_emit_unit(sim, SIM_EVENT_TECHNIQUE_SPENT, order->unit, SIM_NO_CITY, -(s32)taken, true, command.id, events);
}

/* A trap or a natural trap starts (military-facilities-and-traps.md,
   terrain-and-map.md): a seed burns the hexes around it, a ball rolls 3
   hexes, a fire ship burns on water, a dike floods its city, a rockfall
   falls on the hexes around it. A natural trap stays and recovers; the
   other traps are gone. */
internal void
sim_trigger_trap(sim_state *sim, u32 building_id, u32 direction, u32 cause, sim_event_sink *events)
{
    sim_building *building = sim->buildings + building_id;
    building_kind *kind = sim->content.buildings + building->kind;
    sim_emit_building(sim, SIM_EVENT_TRAP_TRIGGERED, building_id, 0, true, cause, events);
    hex_coord cells[16];
    u32 count = 0;
    if (kind->type == BUILDING_BALL)
    {
        hex_coord at = building->hex;
        for (u32 step = 0; step < kind->range_max && count < 16; ++step) { at = hex_neighbor(at, direction % HEX_DIRECTION_COUNT); cells[count++] = at; }
    }
    else
    {
        /* F-150 (M10): a dike of the real map floods its fixed zone, the
           flood hexes of its region (map byte 8). Else a trap reaches its range. */
        b32 flood_zone = kind->type == BUILDING_DIKE && sim->map_layers.loaded;
        for (u32 index = 0; index < sim->unit_count && count < 16; ++index)
        {
            sim_unit *unit = sim->units + index;
            if (!unit->alive) continue;
            b32 hit = flood_zone ? sim_map_in_flood(sim, building->hex, unit->hex) : (u32)hex_distance(unit->hex, building->hex) <= kind->range_max;
            if (hit) cells[count++] = unit->hex;
        }
        cells[count < 16 ? count++ : 15] = building->hex;
    }
    for (u32 index = 0; index < count; ++index)
    {
        u32 unit_id = sim_unit_at(sim, cells[index]);
        if (unit_id != SIM_NO_UNIT)
        {
            u32 loss = kind->type == BUILDING_DIKE ? sim->units[unit_id].soldiers : kind->value;
            sim_hurt_unit(sim, unit_id, loss, SIM_NO_UNIT, false, cause, events);
            /* 業火種 can confuse the units in its range (manual p.34). */
            if (kind->type == BUILDING_SEED && kind->range_max > 1) sim_set_unit_state(sim, unit_id, UNIT_STATE_CONFUSED, 2, cause, events);
        }
        if (kind->type != BUILDING_DIKE && kind->type != BUILDING_ROCKFALL) sim_start_fire(sim, cells[index], sim_fire_turns(sim, false), cause, events);
    }
    if (kind->type == BUILDING_DIKE && building->city_id != SIM_NO_CITY)
    {
        city_component *city = sim_find_city(sim, building->city_id);
        if (city) sim_hurt_city(sim, city, sim_percent_of(city->troops, 10), city->durability / 2, SIM_NO_UNIT, cause, events);
    }
    if (kind->natural)
    {
        building->durability = 0;
        building->complete   = false;
        return;
    }
    building->alive = false;
    sim_emit_building(sim, SIM_EVENT_BUILDING_DESTROYED, building_id, 0, false, cause, events);
}

/* ---- 計略 -------------------------------------------------------------------- */

internal plot_kind *
sim_plot(sim_state *sim, u32 plot)
{
    return plot < sim->content.plot_count ? sim->content.plots + plot : 0;
}

/* The chance of a plot (Q-173): plot.base_success plus the difference of
   intellect, 5 to 95. 虛實 and 神算 always succeed against a lower
   intellect; 洞察 always sees through. */
internal u32
sim_plot_chance(sim_state *sim, sim_unit *unit, u32 target_id)
{
    sim_rules *rules = &sim->content.rules;
    if (target_id == SIM_NO_UNIT) return 100;
    sim_unit *target = sim->units + target_id;
    if (target->force_id == unit->force_id) return 100;
    if (sim_unit_has_skill(sim, target->officers, rules->skill_insight)) return 0;
    u32 mine = sim_battle_values(sim, unit).intellect, theirs = sim_battle_values(sim, target).intellect;
    if (mine > theirs && (sim_unit_has_skill(sim, unit->officers, rules->skill_feint) ||
                          sim_unit_has_skill(sim, unit->officers, rules->skill_strategist))) return 100;
    s32 chance = (s32)rules->plot_base_success + (s32)mine - (s32)theirs;
    return (u32)(chance < 5 ? 5 : (chance > 95 ? 95 : chance));
}

internal sim_check
sim_check_plot(sim_state *sim, sim_unit_order *order)
{
    sim_check check = {};
    check.slot = SIM_ANY_SLOT;
    sim_unit *unit = 0;
    sim_refusal refusal = sim_check_combat_unit(sim, order, &unit);
    if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
    plot_kind *plot = sim_plot(sim, order->kind);
    if (!plot || ((plot->not_unit_types >> unit->unit_type) & 1)) return sim_refuse(check, SIM_REFUSAL_NO_APTITUDE);
    if (!sim->settings.strong_plots && (plot->effect == PLOT_SORCERY || plot->effect == PLOT_LIGHTNING)) return sim_refuse(check, SIM_REFUSAL_NO_APTITUDE);   /* M9: 落雷・妖術 off */
    if (plot->needs_skill && !sim_unit_has_skill(sim, unit->officers, plot->needs_skill) &&
        !(plot->needs_skill_2 && sim_unit_has_skill(sim, unit->officers, plot->needs_skill_2))) return sim_refuse(check, SIM_REFUSAL_NO_APTITUDE);
    if (unit->morale < plot->morale) return sim_refuse(check, SIM_REFUSAL_NO_MORALE);
    if (plot->from_terrain != CONTENT_NO_TERRAIN && sim->map.kind[hex_map_index(&sim->map, unit->hex)] != plot->from_terrain)
    {
        return sim_refuse(check, SIM_REFUSAL_BAD_TERRAIN);
    }
    u32 range = plot->range + (sim_unit_has_skill(sim, unit->officers, sim->content.rules.skill_long_plot) ? 1 : 0);
    if (plot->effect == PLOT_FIRE) range += sim_technique_effect_value(sim, unit->force_id, TECHNIQUE_EFFECT_FIRE_RANGE) ? 2 : 0;
    u32 distance = (u32)hex_distance(unit->hex, order->to);
    if (distance > range || (plot->target != PLOT_TARGET_HEX && distance == 0)) return sim_refuse(check, SIM_REFUSAL_NO_TARGET);
    u32 other = sim_unit_at(sim, order->to);
    if (plot->target == PLOT_TARGET_ENEMY && (other == SIM_NO_UNIT || !sim_at_war(sim, unit->force_id, sim->units[other].force_id)))
    {
        return sim_refuse(check, SIM_REFUSAL_NO_TARGET);
    }
    if (plot->target == PLOT_TARGET_OWN && (other == SIM_NO_UNIT || sim->units[other].force_id != unit->force_id)) return sim_refuse(check, SIM_REFUSAL_NO_TARGET);
    if (plot->target == PLOT_TARGET_HEX && !hex_map_contains(&sim->map, order->to)) return sim_refuse(check, SIM_REFUSAL_NO_TARGET);
    check.action_points = plot->morale;
    check.quantity      = plot->effect == PLOT_EXTINGUISH || plot->effect == PLOT_CALM ? 100 : sim_plot_chance(sim, unit, other);
    check.assumed       = true;
    return check;
}

/* The enemy units on the target hex and the six hexes around it. */
internal u32
sim_splash_units(sim_state *sim, u8 force, hex_coord hex, b32 own_too, u32 *ids)
{
    u32 count = 0;
    for (u32 index = 0; index < sim->unit_count; ++index)
    {
        sim_unit *unit = sim->units + index;
        if (!unit->alive || hex_distance(unit->hex, hex) > 1 || (!own_too && unit->force_id == force)) continue;
        ids[count++] = index;
    }
    return count;
}

internal void
sim_use_plot(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_unit_order *order = &command.unit;
    sim_check check = sim_check_plot(sim, order);
    if (check.refusal != SIM_REFUSAL_NONE) return;
    sim_rules *rules = &sim->content.rules;
    sim_unit *unit = sim->units + order->unit;
    plot_kind *plot = sim_plot(sim, order->kind);
    unit->acted_turn = sim->turn + 1;
    sim_change_morale(sim, order->unit, -(s32)plot->morale, command.id, events);
    b32 success = check.quantity >= 100 || (check.quantity && sim_random_below(sim, 100) < check.quantity);
    sim_emit_unit(sim, SIM_EVENT_PLOT_USED, order->unit, SIM_NO_CITY, success ? 1 : 0, true, command.id, events);
    if (!success) return;
    u32 target = sim_unit_at(sim, order->to);
    u32 ids[SIM_UNIT_CAPACITY];
    switch (plot->effect)
    {
        case PLOT_FIRE:
            sim_start_fire(sim, order->to, sim_fire_turns(sim, false), command.id, events);
            if (sim_building_at(sim, order->to) != SIM_NO_BUILDING)
            {
                u32 building = sim_building_at(sim, order->to);
                if (sim_building_is_trap(sim->content.buildings + sim->buildings[building].kind))
                {
                    sim_trigger_trap(sim, building, sim_direction_toward(unit->hex, order->to), command.id, events);
                }
            }
            break;
        case PLOT_EXTINGUISH:
            for (u32 d = 0; d <= HEX_DIRECTION_COUNT; ++d)
            {
                hex_coord hex = d < HEX_DIRECTION_COUNT ? hex_neighbor(order->to, d) : order->to;
                if (!sim_hex_burns(sim, hex)) continue;
                u32 cell = hex_map_index(&sim->map, hex);
                sim->fire[cell] = 0;
                sim_emit_cell(sim, SIM_EVENT_FIRE_ENDED, cell, 0, false, command.id, events);
            }
            break;
        case PLOT_FALSE_REPORT:
            sim_set_unit_state(sim, target, UNIT_STATE_FALSE_REPORT, plot->turns, command.id, events);
            break;
        case PLOT_CONFUSE:
            sim_set_unit_state(sim, target, UNIT_STATE_CONFUSED, plot->turns, command.id, events);
            break;
        case PLOT_CALM:
            if (sim->units[target].state != UNIT_STATE_NONE)
            {
                sim->units[target].state = UNIT_STATE_NONE;
                sim->units[target].state_turns = 0;
                sim_emit_unit(sim, SIM_EVENT_UNIT_STATE, target, SIM_NO_CITY, 0, false, command.id, events);
            }
            break;
        case PLOT_AMBUSH:
        {
            /* The intellect is the attack; no counterattack (manual p.17). */
            sim_unit_values mine = sim_battle_values(sim, unit), theirs = sim_battle_values(sim, sim->units + target);
            u32 percent = rules->combat_ambush_percent;
            if (sim_unit_has_skill(sim, unit->officers, rules->skill_ambush_critical)) percent = sim_percent_of(percent, rules->combat_critical_percent);
            sim_hurt_unit(sim, target, sim_combat_damage(sim, unit->soldiers, mine.intellect, theirs.defence, percent), order->unit, false, command.id, events);
            break;
        }
        case PLOT_INFIGHTING:
        {
            /* The target attacks the strongest unit of its own force next to it. */
            sim_unit *victim = sim->units + target;
            u32 best = SIM_NO_UNIT;
            for (u32 index = 0; index < sim->unit_count; ++index)
            {
                sim_unit *other = sim->units + index;
                if (!other->alive || index == target || other->force_id != victim->force_id || hex_distance(other->hex, victim->hex) != 1) continue;
                if (best == SIM_NO_UNIT || other->soldiers > sim->units[best].soldiers) best = index;
            }
            if (best != SIM_NO_UNIT)
            {
                sim_unit_values a = sim_battle_values(sim, victim), b = sim_battle_values(sim, sim->units + best);
                sim_hurt_unit(sim, best, sim_combat_damage(sim, victim->soldiers, a.attack, b.defence, 100), target, false, command.id, events);
            }
            break;
        }
        case PLOT_SORCERY:
        {
            u32 count = sim_splash_units(sim, unit->force_id, order->to, false, ids);
            for (u32 index = 0; index < count; ++index) sim_set_unit_state(sim, ids[index], UNIT_STATE_CONFUSED, plot->turns, command.id, events);
            break;
        }
        case PLOT_LIGHTNING:
        {
            u32 count = sim_splash_units(sim, unit->force_id, order->to, true, ids);
            sim_unit_values mine = sim_battle_values(sim, unit);
            for (u32 index = 0; index < count; ++index)
            {
                sim_unit_values theirs = sim_battle_values(sim, sim->units + ids[index]);
                sim_hurt_unit(sim, ids[index], sim_combat_damage(sim, unit->soldiers, mine.intellect, theirs.defence, rules->combat_lightning_percent),
                              order->unit, false, command.id, events);
            }
            sim_start_fire(sim, order->to, sim_fire_turns(sim, false), command.id, events);
            for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d) sim_start_fire(sim, hex_neighbor(order->to, d), sim_fire_turns(sim, false), command.id, events);
            break;
        }
        default:
            break;
    }
}

/* ---- each turn ------------------------------------------------------------------ */

/* A unit under 偽報 marches toward its home city by itself (manual p.17). */
internal void
sim_false_report_march(sim_state *sim, u32 unit_id, u32 cause, sim_event_sink *events)
{
    sim_unit *unit = sim->units + unit_id;
    city_component *home = sim_find_city(sim, unit->home_city);
    if (!home) return;
    hex_coord goal = sim->sites[unit->home_city].hex;
    arena_reset(&sim->scratch);
    sim_reach reach = sim_unit_reach(sim, unit_id, &sim->scratch);
    u32 best = 0;
    for (u32 index = 1; index < reach.count; ++index)
    {
        if (hex_distance(reach.cells[index], goal) < hex_distance(reach.cells[best], goal)) best = index;
    }
    if (best == 0) return;
    unit->hex = reach.cells[best];
    sim_emit_unit(sim, SIM_EVENT_UNIT_MOVED, unit_id, SIM_NO_CITY, (s32)reach.costs[best], true, cause, events);
    if (sim_city_at(sim, unit->hex) == unit->home_city && home->owner_force_id == unit->force_id)
    {
        sim_unit_enter_city(sim, unit_id, unit->home_city, SIM_EVENT_NONE, cause, events);
    }
}

/* The chance in percent that a unit in 混亂 recovers at the start of a
   turn: the highest 智力 of its officers, at least confusion.recover_floor
   and at most confusion.recover_cap (assumed, Q-218; knowledge/combat.md,
   "Confusion": high 智力 recovers in 1 to 2 turns, low in 3 or more). */
internal u32
sim_confusion_recover_chance(sim_state *sim, sim_unit *unit)
{
    sim_rules *rules = &sim->content.rules;
    u32 chance = sim_unit_ability(sim, unit->officers, SIM_ABILITY_INTELLECT);
    if (chance < rules->confusion_recover_floor) chance = rules->confusion_recover_floor;
    if (rules->confusion_recover_cap && chance > rules->confusion_recover_cap) chance = rules->confusion_recover_cap;
    return chance;
}

/* The panic (assumed, Q-218): a unit with panic.soldiers soldiers or fewer
   next to an enemy unit with panic.ratio times its soldiers can confuse
   itself at the start of a turn, with panic.chance. */
internal b32
sim_unit_panics(sim_state *sim, u32 unit_id)
{
    sim_rules *rules = &sim->content.rules;
    sim_unit *unit = sim->units + unit_id;
    if (!rules->panic_chance || !unit->soldiers || unit->soldiers > rules->panic_soldiers || unit->state != UNIT_STATE_NONE) return false;
    b32 threat = false;
    for (u32 index = 0; index < sim->unit_count && !threat; ++index)
    {
        sim_unit *other = sim->units + index;
        if (!other->alive || index == unit_id || hex_distance(other->hex, unit->hex) != 1 || !sim_at_war(sim, unit->force_id, other->force_id)) continue;
        threat = (u64)other->soldiers >= (u64)unit->soldiers * rules->panic_ratio;
    }
    return threat && sim_random_below(sim, 100) < rules->panic_chance;
}

/* At the start of each turn: 混亂 ends by a chance from the 智力 of the
   unit (Q-218); 偽報 counts down its turns. A unit in the range of an own
   camp can recover sooner (PK, manual p.34; the chance is assumed). 鎮靜
   still cures at once (sim_use_plot). Then a small unit next to a large
   enemy can panic. */
internal void
sim_run_states(sim_state *sim, u32 cause, sim_event_sink *events)
{
    for (u32 index = 0; index < sim->unit_count; ++index)
    {
        sim_unit *unit = sim->units + index;
        if (!unit->alive || unit->state == UNIT_STATE_NONE) continue;
        if (unit->state == UNIT_STATE_FALSE_REPORT) sim_false_report_march(sim, index, cause, events);
        if (!unit->alive) continue;
        b32 camp = sim_building_bonus(sim, unit->force_id, unit->hex, BUILDING_CAMP) > 0 &&
                   sim_random_below(sim, 100) < sim->content.rules.camp_recover_chance;
        b32 recovered = camp;
        if (unit->state == UNIT_STATE_CONFUSED)
        {
            if (!recovered) recovered = sim_random_below(sim, 100) < sim_confusion_recover_chance(sim, unit);
        }
        else
        {
            if (unit->state_turns > 0) unit->state_turns--;
            recovered = recovered || unit->state_turns == 0;
        }
        if (recovered)
        {
            unit->state = UNIT_STATE_NONE;
            unit->state_turns = 0;
            sim_emit_unit(sim, SIM_EVENT_UNIT_STATE, index, SIM_NO_CITY, 0, false, cause, events);
        }
    }
    for (u32 index = 0; index < sim->unit_count; ++index)
    {
        if (sim->units[index].alive && sim_unit_panics(sim, index)) sim_set_unit_state(sim, index, UNIT_STATE_CONFUSED, 0, cause, events);
    }
}

internal void
sim_run_fire(sim_state *sim, u32 cause, sim_event_sink *events)
{
    if (!sim->fire) return;
    sim_rules *rules = &sim->content.rules;
    for (u32 index = 0; index < sim->unit_count; ++index)
    {
        if (sim->units[index].alive && sim_hex_burns(sim, sim->units[index].hex)) sim_burn_unit(sim, index, rules->fire_damage_percent, cause, events);
    }
    for (u32 index = 0; index < sim->building_count; ++index)
    {
        sim_building *building = sim->buildings + index;
        if (building->alive && sim_hex_burns(sim, building->hex) && !sim->content.buildings[building->kind].natural)
        {
            sim_hurt_building(sim, index, rules->fire_building_damage, 0, cause, events);
        }
    }
    /* Each turn the count goes down by 1; at 0 the fire stops. On an empty
       hex of a terrain that does not burn, the fire stops at once (F-149). */
    for (u32 cell = 0; cell < sim->map.width * sim->map.height; ++cell)
    {
        if (!sim->fire[cell]) continue;
        hex_coord hex = hex_map_coord(&sim->map, cell);
        terrain_kind *terrain = sim_terrain_at(sim, hex);
        if (terrain && !terrain->burnable && sim_unit_at(sim, hex) == SIM_NO_UNIT) sim->fire[cell] = 0;
        else                                                                       sim->fire[cell]--;
        if (!sim->fire[cell]) sim_emit_cell(sim, SIM_EVENT_FIRE_ENDED, cell, 0, false, cause, events);
    }
}

/* Towers shoot the nearest enemy unit in range; music towers give morale;
   mazes can confuse; natural traps recover. */
internal void
sim_run_buildings(sim_state *sim, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    for (u32 index = 0; index < sim->building_count; ++index)
    {
        sim_building *building = sim->buildings + index;
        if (!building->alive) continue;
        building_kind *kind = sim->content.buildings + building->kind;
        if (kind->natural)
        {
            if (building->complete) continue;
            /* F-150 (M10): a used rockfall leaves its site. If the site is
               empty and does not burn, a new rockfall comes there with a
               chance of rockfall.return_percent at each check (each turn;
               Q-196 asks for the interval). A dike recovers slowly. */
            if (kind->type == BUILDING_ROCKFALL && rules->rockfall_return_percent)
            {
                if (sim_unit_at(sim, building->hex) != SIM_NO_UNIT || sim_hex_burns(sim, building->hex)) continue;
                if (sim_random_below(sim, 100) >= rules->rockfall_return_percent) continue;
                building->durability = kind->durability;
                building->complete   = true;
                sim_emit_building(sim, SIM_EVENT_ROCKFALL_RETURNED, index, 0, false, cause, events);
                continue;
            }
            building->durability = Minimum(kind->durability, building->durability + rules->natural_recovery);
            building->complete = building->durability >= kind->durability;
            continue;
        }
        if (!building->complete) continue;
        for (u32 unit_id = 0; unit_id < sim->unit_count; ++unit_id)
        {
            sim_unit *unit = sim->units + unit_id;
            if (!unit->alive) continue;
            u32 distance = (u32)hex_distance(unit->hex, building->hex);
            if (distance < kind->range_min || distance > kind->range_max) continue;
            b32 enemy = building->force_id != SIM_NO_FORCE && unit->force_id != building->force_id;
            if (kind->type == BUILDING_TOWER && enemy)
            {
                sim_hurt_unit(sim, unit_id, kind->value, SIM_NO_UNIT, false, cause, events);
                break;   /* one shot each turn */
            }
            if (kind->type == BUILDING_MUSIC && !enemy) sim_change_morale(sim, unit_id, (s32)kind->value, cause, events);
            if (kind->type == BUILDING_MAZE && enemy && sim_random_below(sim, 100) < rules->maze_confuse_chance)
            {
                sim_set_unit_state(sim, unit_id, UNIT_STATE_CONFUSED, 1, cause, events);
            }
        }
    }
    for (u32 unit_id = 0; unit_id < sim->unit_count; ++unit_id)
    {
        sim_unit *unit = sim->units + unit_id;
        if (unit->alive && sim_unit_has_skill(sim, unit->officers, rules->skill_music)) sim_change_morale(sim, unit_id, 5, cause, events);
    }
}

/* A base recovers durability each turn; with an enemy unit within 2 hexes
   it does not, and it loses soldiers (manual p.8, p.10; the amounts are
   assumed, Q-175). */
internal void
sim_run_bases(sim_state *sim, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        city_component *city = sim->cities.data + index;
        if (city->owner_force_id == SIM_NO_FORCE) continue;
        if (sim_enemy_near(sim, city))
        {
            u32 lost = rules->siege_blockade_loss < city->troops ? rules->siege_blockade_loss : city->troops;
            city->troops -= lost;
            if (lost) sim_emit_city(sim, SIM_EVENT_BASE_SOLDIERS_LOST, city, -(s32)lost, 0, true, cause, events);
            continue;
        }
        u32 maximum = sim_city_durability_max(sim, city);
        if (city->durability > maximum) city->durability = maximum;   /* G2-009: a save of an older rule can hold more */
        if (city->durability >= maximum) continue;
        u32 recovery = rules->siege_recovery;
        u32 engineers = sim_technique_effect_value(sim, city->owner_force_id, TECHNIQUE_EFFECT_FACILITY_RECOVERY);
        if (engineers) recovery = sim_percent_of(recovery, engineers);
        u32 added = maximum - city->durability < recovery ? maximum - city->durability : recovery;
        city->durability += added;
        sim_emit_city(sim, SIM_EVENT_BASE_RECOVERED, city, (s32)added, 0, true, cause, events);
    }
}

internal void
sim_run_field_turn(sim_state *sim, u32 cause, sim_event_sink *events)
{
    sim_run_states(sim, cause, events);
    sim_run_fire(sim, cause, events);
    sim_run_buildings(sim, cause, events);
    sim_run_bases(sim, cause, events);
}
