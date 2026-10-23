/* Combat (military-units-and-tactics.md, siege-and-bases.md, skills.md):
   the combat_damage rule hook, 攻擊 (attack) with the counterattack and the
   arrow attack, 齊攻 (joint attack), 戰法 (the tactics of tactics.tsv), the
   critical hits of critical_skills.tsv, morale, the destruction of a unit
   with the capture of its officers, and the attacks on bases, buildings and
   the facilities of a city. Included by sim.cpp after units.cpp.

   The spec documents the classes, the tactics, their costs and ranges, the
   triangle, the skills and the rules of a siege. It gives no damage
   formula: the default combat_damage rule and every percent of rules.tsv
   are assumed (Q-172 to Q-176), and the events say so. */

/* ---- the rule hook ----------------------------------------------------------- */

/* ASSUMED RULE (Q-172): soldiers x attack / (attack + defence) x
   combat.damage_percent, times the multipliers of the attack, and at least
   combat.min_damage. A content pack can set its own rule in sim_ruleset. */
internal u32
sim_rule_combat_damage(sim_state *sim, sim_combat_input *input)
{
    sim_rules *rules = &sim->content.rules;
    if (!input->soldiers) return 0;
    u64 sum = (u64)input->attack + input->defence;
    if (!sum) sum = 1;
    u64 damage = (u64)input->soldiers * input->attack / sum * rules->combat_damage_percent / 100 * input->percent / 100;
    if (damage < rules->combat_min_damage) damage = rules->combat_min_damage;
    return (u32)(damage < 0x7FFFFFFF ? damage : 0x7FFFFFFF);
}

internal u32
sim_combat_damage(sim_state *sim, u32 soldiers, u32 attack, u32 defence, u32 percent)
{
    sim_combat_input input = {};
    input.soldiers = soldiers;
    input.attack   = attack;
    input.defence  = defence;
    input.percent  = percent;
    sim_combat_damage_fn *rule = sim->ruleset.combat_damage ? sim->ruleset.combat_damage : sim_rule_combat_damage;
    return rule(sim, &input);
}

/* ---- the map of a battle ---------------------------------------------------- */

internal b32
sim_hex_is_water(sim_state *sim, hex_coord hex)
{
    terrain_kind *terrain = sim_terrain_at(sim, hex);
    return terrain && !terrain->cost[MOVEMENT_FOOT] && terrain->cost[MOVEMENT_NAVY];
}

/* The class that a unit fights with: a land unit on water is a 走舸
   (manual p.12). */
internal u8
sim_effective_type(sim_state *sim, sim_unit *unit)
{
    unit_type_kind *type = sim_unit_type(sim, unit->unit_type);
    if (type && type->movement != MOVEMENT_NAVY && sim_hex_is_water(sim, unit->hex))
    {
        if (unit->has_ship) return unit->ship_type;   /* the ship class of a unit of a save (record +41) */
        u8 boat = content_find_unit_type(&sim->content, "boat");
        if (boat != CONTENT_NO_UNIT_TYPE) return boat;
    }
    return unit->unit_type;
}

internal u32
sim_building_at(sim_state *sim, hex_coord hex)
{
    for (u32 index = 0; index < sim->building_count; ++index)
    {
        if (sim->buildings[index].alive && hex_equal(sim->buildings[index].hex, hex)) return index;
    }
    return SIM_NO_BUILDING;
}

/* The direction from a hex to an adjacent hex, or HEX_DIRECTION_COUNT. */
internal u32
sim_direction_to(hex_coord from, hex_coord to)
{
    for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d)
    {
        if (hex_equal(hex_neighbor(from, d), to)) return d;
    }
    return HEX_DIRECTION_COUNT;
}

/* The direction from a hex that comes nearest to a far hex. */
internal u32
sim_direction_toward(hex_coord from, hex_coord to)
{
    u32 best = 0;
    s32 best_distance = 0x7FFFFFFF;
    for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d)
    {
        s32 distance = hex_distance(hex_neighbor(from, d), to);
        if (distance < best_distance) { best = d; best_distance = distance; }
    }
    return best;
}

/* What stands at a hex for an attack of a force. */
struct sim_target
{
    u32 unit;          /* an enemy unit, or SIM_NO_UNIT */
    u32 building;      /* an enemy building or one without an owner, or SIM_NO_BUILDING */
    u8  city;          /* an enemy city, or SIM_NO_CITY */
    u8  slot_city;     /* an enemy city with a facility on this hex, or SIM_NO_CITY */
    u8  slot;
    u32 base;          /* M8: a port or a pass of a faction at war, or SIM_NO_BASE */
};

/* A thing of the other owner is a target: no owner, or a faction at war
   (M7: not an ally and not under a ceasefire). */
internal b32
sim_hostile(sim_state *sim, u8 force, u8 other)
{
    return other != force && (other == SIM_NO_FORCE || sim_at_war(sim, force, other));
}

internal sim_target
sim_target_at(sim_state *sim, u8 force, hex_coord hex)
{
    sim_target target = {};
    target.unit = SIM_NO_UNIT; target.building = SIM_NO_BUILDING; target.city = SIM_NO_CITY; target.slot_city = SIM_NO_CITY;
    target.base = sim_base_at(sim, hex);
    if (target.base != SIM_NO_BASE && !sim_hostile(sim, force, sim->bases[target.base].force)) target.base = SIM_NO_BASE;
    u32 unit = sim_unit_at(sim, hex);
    if (unit != SIM_NO_UNIT && sim_hostile(sim, force, sim->units[unit].force_id)) target.unit = unit;
    u32 building = sim_building_at(sim, hex);
    if (building != SIM_NO_BUILDING && sim_hostile(sim, force, sim->buildings[building].force_id)) target.building = building;
    u8 city = sim_city_at(sim, hex);
    if (city != SIM_NO_CITY && sim_hostile(sim, force, sim_find_city(sim, city)->owner_force_id)) target.city = city;
    for (u32 index = 0; index < sim->cities.count && target.slot_city == SIM_NO_CITY; ++index)
    {
        city_component *other = sim->cities.data + index;
        if (!sim_hostile(sim, force, other->owner_force_id)) continue;
        sim_city_site *site = sim->sites + other->city_id;
        for (u32 slot = 0; slot < site->slot_count; ++slot)
        {
            if (site->slots[slot].state != SIM_SLOT_EMPTY && hex_equal(site->slots[slot].hex, hex))
            {
                target.slot_city = other->city_id;
                target.slot      = (u8)slot;
                break;
            }
        }
    }
    return target;
}

internal b32
sim_target_any(sim_target *target)
{
    return target->unit != SIM_NO_UNIT || target->building != SIM_NO_BUILDING || target->city != SIM_NO_CITY || target->slot_city != SIM_NO_CITY ||
           target->base != SIM_NO_BASE;
}

/* ---- the multipliers ---------------------------------------------------------- */

/* The percent of a complete own building of the type in range of the hex:
   the value of the building (drum and maze: more damage; camp: less), or 0. */
internal u32
sim_building_bonus(sim_state *sim, u8 force, hex_coord hex, building_type type)
{
    u32 best = 0;
    for (u32 index = 0; index < sim->building_count; ++index)
    {
        sim_building *building = sim->buildings + index;
        if (!building->alive || !building->complete || building->force_id != force) continue;
        building_kind *kind = sim->content.buildings + building->kind;
        if (kind->type != type) continue;
        if ((u32)hex_distance(building->hex, hex) <= kind->range_max && kind->value > best) best = kind->value;
    }
    return best;
}

/* The class triangle (guide book p.16): pike beats cavalry, cavalry beats
   halberd, halberd beats pike, when the unit uses a tactic. */
internal b32
sim_class_beats(u32 attacker_aptitude, u32 defender_aptitude)
{
    return (attacker_aptitude == OFFICER_APTITUDE_SPEAR    && defender_aptitude == OFFICER_APTITUDE_CAVALRY) ||
           (attacker_aptitude == OFFICER_APTITUDE_CAVALRY  && defender_aptitude == OFFICER_APTITUDE_HALBERD) ||
           (attacker_aptitude == OFFICER_APTITUDE_HALBERD  && defender_aptitude == OFFICER_APTITUDE_SPEAR);
}

internal content_unit_class
sim_class_of_aptitude(u32 aptitude)
{
    switch (aptitude)
    {
        case OFFICER_APTITUDE_SPEAR:    return CONTENT_CLASS_PIKE;
        case OFFICER_APTITUDE_HALBERD:  return CONTENT_CLASS_HALBERD;
        case OFFICER_APTITUDE_CROSSBOW: return CONTENT_CLASS_CROSSBOW;
        case OFFICER_APTITUDE_CAVALRY:  return CONTENT_CLASS_CAVALRY;
        case OFFICER_APTITUDE_SIEGE:    return CONTENT_CLASS_SIEGE;
        default:                        return CONTENT_CLASS_NONE;
    }
}

/* The values of a unit for a battle: the class it fights with, and 精銳
   raises attack and defence. */
internal sim_unit_values
sim_battle_values(sim_state *sim, sim_unit *unit)
{
    u8 type_id = sim_effective_type(sim, unit);
    sim_unit_values values = sim_unit_values_of(sim, unit->force_id, type_id, unit->officers);
    unit_type_kind *type = sim_unit_type(sim, type_id);
    content_unit_class unit_class = type ? sim_class_of_aptitude(type->aptitude) : CONTENT_CLASS_NONE;
    if (unit_class != CONTENT_CLASS_NONE && sim_has_technique_effect(sim, unit->force_id, TECHNIQUE_EFFECT_ELITE, unit_class))
    {
        values.attack  = sim_percent_of(values.attack, sim->content.rules.combat_technique_percent);
        values.defence = sim_percent_of(values.defence, sim->content.rules.combat_technique_percent);
    }
    if (!values.attack)  values.attack = 1;
    if (!values.defence) values.defence = 1;
    return values;
}

/* The force of the commander of a unit, or 0. */
internal u32
sim_commander_force(sim_state *sim, sim_unit *unit)
{
    officer_component *commander = sim_find_officer(sim, unit->officers[0]);
    return commander ? commander->war : 0;
}

/* A critical hit (會心一擊): a skill of critical_skills.tsv forces it, else
   the chance combat.critical_chance. */
internal b32
sim_is_critical(sim_state *sim, sim_unit *attacker, u32 action, sim_target *target, u32 distance)
{
    unit_type_kind *type = sim_unit_type(sim, sim_effective_type(sim, attacker));
    u32 aptitude = type ? type->aptitude : CONTENT_ANY_APTITUDE;
    terrain_kind *terrain = sim_terrain_at(sim, attacker->hex);
    for (u32 index = 0; index < sim->content.critical_count; ++index)
    {
        critical_skill *rule = sim->content.criticals + index;
        if (!(rule->action & action) || !sim_unit_has_skill(sim, attacker->officers, rule->skill)) continue;
        if (rule->aptitude != CONTENT_ANY_APTITUDE && rule->aptitude != aptitude) continue;
        b32 holds = false;
        switch (rule->condition)
        {
            case CRITICAL_ALWAYS:      holds = true; break;
            case CRITICAL_LOWER_FORCE: holds = target->unit != SIM_NO_UNIT &&
                                               sim_commander_force(sim, sim->units + target->unit) < sim_commander_force(sim, attacker); break;
            case CRITICAL_FOREST:      holds = terrain && strcmp(terrain->key, "forest") == 0; break;
            case CRITICAL_BUILDING:    holds = target->unit == SIM_NO_UNIT; break;
            case CRITICAL_MOUNTED:     holds = aptitude == OFFICER_APTITUDE_CAVALRY && distance > 1; break;
            default: break;
        }
        if (holds) return true;
    }
    return sim_random_below(sim, 100) < sim->content.rules.combat_critical_chance;
}

/* ---- damage to a unit, its destruction and the captives ------------------------ */

internal void sim_set_unit_state(sim_state *sim, u32 unit_id, sim_unit_state state, u32 turns, u32 cause, sim_event_sink *events);   /* field.cpp */
internal void sim_trigger_trap(sim_state *sim, u32 building_id, u32 direction, u32 cause, sim_event_sink *events);                  /* field.cpp */
/* sim_start_fire and sim_fire_turns: sim.h (field.cpp) */

internal void
sim_change_morale(sim_state *sim, u32 unit_id, s32 change, u32 cause, sim_event_sink *events)
{
    sim_unit *unit = sim->units + unit_id;
    s32 cap = (s32)(sim->content.rules.morale_max + sim_technique_effect_value(sim, unit->force_id, TECHNIQUE_EFFECT_MORALE_CAP));
    s32 value = (s32)unit->morale + change;
    if (value < 0) value = 0;
    if (value > cap) value = cap;
    s32 delta = value - (s32)unit->morale;
    unit->morale = (u32)value;
    if (delta) sim_emit_unit(sim, SIM_EVENT_MORALE_CHANGED, unit_id, SIM_NO_CITY, delta, false, cause, events);
}

/* An own city of the force for an officer that escapes: its home city if
   the force still owns it, else the first city of the force. */
internal u8
sim_refuge_city(sim_state *sim, u8 force, u8 home)
{
    city_component *city = sim_find_city(sim, home);
    if (city && city->owner_force_id == force) return home;
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        if (sim->cities.data[index].owner_force_id == force) return sim->cities.data[index].city_id;
    }
    return SIM_NO_CITY;
}

/* ASSUMED (Q-176): each officer of a destroyed unit is captured with the
   chance combat.capture_percent (more with a halberd tactic, PK; always
   with 捕縛), else escapes to a city of the faction. 強運 and 血路 never
   become captives. */
internal void
sim_capture_officers(sim_state *sim, u32 unit_id, u32 captor_id, b32 halberd_tactic, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    sim_unit *unit = sim->units + unit_id;
    sim_unit *captor = captor_id < sim->unit_count ? sim->units + captor_id : 0;
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
    {
        officer_component *officer = sim_find_officer(sim, unit->officers[place]);
        if (!officer) continue;
        u32 chance = rules->combat_capture_percent + (halberd_tactic ? rules->combat_halberd_capture : 0);
        if (captor && sim_unit_has_skill(sim, captor->officers, rules->skill_capture)) chance = 100;
        if (sim_officer_has_skill(officer, rules->skill_fortune) || sim_officer_has_skill(officer, rules->skill_escape_capture)) chance = 0;
        b32 captured = captor && chance && sim_random_below(sim, 100) < chance;
        sim_event event = sim_event_make(sim, captured ? SIM_EVENT_OFFICER_CAPTURED : SIM_EVENT_OFFICER_ESCAPED, cause);
        event.subject = sim_officer_entity(sim, officer->officer_id);
        if (captured)
        {
            officer->former_force     = officer->force_id;   /* M7: a release sends it back */
            officer->status           = SIM_STATUS_CAPTIVE;
            officer->force_id         = captor->force_id;
            officer->current_position = captor->home_city;
            officer->base_location    = captor->home_city;
            event.faction = captor->force_id;
        }
        else
        {
            u8 refuge = sim_refuge_city(sim, unit->force_id, unit->home_city);
            officer->current_position = refuge == SIM_NO_CITY ? 0xFFFF : refuge;
            officer->base_location    = officer->current_position;
            event.faction = unit->force_id;
        }
        sim_emit(events, event);
    }
}

internal void
sim_destroy_unit(sim_state *sim, u32 unit_id, u32 captor_id, b32 halberd_tactic, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    sim_capture_officers(sim, unit_id, captor_id, halberd_tactic, cause, events);
    sim->units[unit_id].alive = false;
    sim_emit_unit(sim, SIM_EVENT_UNIT_DESTROYED, unit_id, SIM_NO_CITY, 0, false, cause, events);
    if (captor_id < sim->unit_count && rules->combat_destroy_technique)
    {
        sim_unit *captor = sim->units + captor_id;
        u32 points = rules->combat_destroy_technique * (sim_unit_has_skill(sim, captor->officers, rules->skill_subtle) ? 2 : 1);
        sim->technique_points[captor->force_id] += points;
        sim_event gained = sim_event_make(sim, SIM_EVENT_TECHNIQUE_GAINED, cause);
        gained.faction = captor->force_id;
        gained.delta   = (s32)points;
        gained.assumed = true;
        sim_emit(events, gained);
    }
}

/* Takes soldiers from a unit; at 0 the unit is destroyed. */
internal void
sim_hurt_unit(sim_state *sim, u32 unit_id, u32 amount, u32 attacker_id, b32 halberd_tactic, u32 cause, sim_event_sink *events)
{
    sim_unit *unit = sim->units + unit_id;
    if (!unit->alive) return;
    u32 lost = amount < unit->soldiers ? amount : unit->soldiers;
    unit->soldiers -= lost;
    if (lost) sim_emit_unit(sim, SIM_EVENT_UNIT_DAMAGED, unit_id, SIM_NO_CITY, -(s32)lost, true, cause, events);
    if (!unit->soldiers && sim_unit_type(sim, unit->unit_type)) sim_destroy_unit(sim, unit_id, attacker_id, halberd_tactic, cause, events);
}

/* ---- bases and buildings -------------------------------------------------------- */

/* The maximum durability of a city: the save's maximum (or
   siege.default_durability), more with 強化城牆 (assumed amount, Q-175). */
internal u32
sim_city_durability_max(sim_state *sim, city_component *city)
{
    sim_rules *rules = &sim->content.rules;
    /* G2-009 (Q-222): the original shows the static city constant as the
       maximum (4600 for 襄陽, 4000 for 江陵, 2400 for 新野), not the field
       that the spec named durability_maximum (7000, 6000). Assumed from
       three cities. */
    u32 maximum = city->capacity_constant ? city->capacity_constant :
                  (city->durability_maximum ? city->durability_maximum : rules->siege_default_durability);
    if (sim_has_technique_effect(sim, city->owner_force_id, TECHNIQUE_EFFECT_CITADEL, CONTENT_CLASS_NONE) && rules->siege_wall_percent)
    {
        maximum = sim_percent_of(maximum, rules->siege_wall_percent);
    }
    return maximum;
}

/* The base falls (siege-and-bases.md): the attacker owns it with its stock,
   the officers of the old owner in it become captives, and the unit enters
   it. */
internal void
sim_capture_city(sim_state *sim, city_component *city, u32 attacker_id, u32 cause, sim_event_sink *events)
{
    sim_unit *attacker = sim->units + attacker_id;
    u8 old_owner = city->owner_force_id;
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (!sim_officer_is_active(officer) || officer->force_id != old_owner || officer->current_position != city->city_id) continue;
        officer->former_force = officer->force_id;
        officer->status   = SIM_STATUS_CAPTIVE;
        officer->force_id = attacker->force_id;
        sim_event captured = sim_event_make(sim, SIM_EVENT_OFFICER_CAPTURED, cause);
        captured.subject = sim_officer_entity(sim, officer->officer_id);
        captured.faction = attacker->force_id;
        sim_emit(events, captured);
    }
    city->owner_force_id = attacker->force_id;
    city->durability     = sim_city_durability_max(sim, city) / 2;
    sim_event event = sim_event_make(sim, SIM_EVENT_BASE_CAPTURED, cause);
    event.subject = sim_city_entity(sim, city->city_id);
    event.faction = attacker->force_id;
    event.detail  = old_owner;
    sim_emit(events, event);
    sim_rules *rules = &sim->content.rules;
    sim_add_technique_points(sim, attacker->force_id, (s32)rules->siege_capture_technique, cause, events);
    /* The loss of a city gives technique points to the old owner (tech-tree.md:
       approximately 225, more with many facilities; claimed). */
    u32 built = 0;
    sim_city_site *site = sim->sites + city->city_id;
    for (u32 slot = 0; slot < site->slot_count; ++slot) built += site->slots[slot].state == SIM_SLOT_BUILT;
    sim_add_technique_points(sim, old_owner, (s32)(rules->technique_city_lost + rules->technique_city_lost_facility * built), cause, events);
    /* G2-059: a force that loses its last city falls (張楊軍滅亡). The message of the turn start shows it to every player. */
    if (old_owner != SIM_NO_FORCE && sim_faction_city_count(sim, old_owner) == 0)
    {
        sim_event fell = sim_event_make(sim, SIM_EVENT_FACTION_FELL, cause);
        fell.faction = old_owner;
        fell.detail  = attacker->force_id;
        fell.subject = sim_city_entity(sim, city->city_id);
        sim_emit(events, fell);
    }
    sim_unit_enter_city(sim, attacker_id, city->city_id, SIM_EVENT_NONE, cause, events);
}

/* Soldiers and durability of a base under attack. At 0 of either the base
   falls (manual p.8, p.10). */
internal void
sim_hurt_city(sim_state *sim, city_component *city, u32 soldiers, u32 durability, u32 attacker_id, u32 cause, sim_event_sink *events)
{
    u32 lost = soldiers < city->troops ? soldiers : city->troops;
    city->troops -= lost;
    if (lost) sim_emit_city(sim, SIM_EVENT_BASE_SOLDIERS_LOST, city, -(s32)lost, 0, true, cause, events);
    u32 broken = durability < city->durability ? durability : city->durability;
    city->durability -= broken;
    if (broken) sim_emit_city(sim, SIM_EVENT_BASE_DAMAGED, city, -(s32)broken, 0, true, cause, events);
    if ((city->troops == 0 || city->durability == 0) && attacker_id < sim->unit_count && sim->units[attacker_id].alive)
    {
        sim_capture_city(sim, city, attacker_id, cause, events);
    }
}

internal void
sim_hurt_building(sim_state *sim, u32 building_id, u32 amount, u32 direction, u32 cause, sim_event_sink *events)
{
    sim_building *building = sim->buildings + building_id;
    if (!building->alive) return;
    u32 broken = amount < building->durability ? amount : building->durability;
    building->durability -= broken;
    if (broken) sim_emit_unit(sim, SIM_EVENT_BUILDING_DAMAGED, building_id, SIM_NO_CITY, -(s32)broken, false, cause, events);
    if (building->durability) return;
    building_type type = sim->content.buildings[building->kind].type;
    if (type == BUILDING_SEED || type == BUILDING_BALL || type == BUILDING_SHIP || type == BUILDING_DIKE || type == BUILDING_ROCKFALL)
    {
        sim_trigger_trap(sim, building_id, direction, cause, events);
        return;
    }
    building->alive = false;
    sim_emit_unit(sim, SIM_EVENT_BUILDING_DESTROYED, building_id, SIM_NO_CITY, 0, false, cause, events);
}

/* The facility of a city slot under attack. A destroyed facility gives
   loot to the unit (siege-and-bases.md, "Loot"): gold, grain or soldiers,
   loot.percent of its cost (assumed, Q-175); nothing from a black market. */
internal void
sim_hurt_slot(sim_state *sim, u8 city_id, u32 slot_index, u32 amount, u32 attacker_id, u32 cause, sim_event_sink *events)
{
    city_component *city = sim_find_city(sim, city_id);
    sim_slot *slot = sim->sites[city_id].slots + slot_index;
    if (!city || slot->facility >= sim->content.facility_count) return;
    facility_kind *kind = sim->content.facilities + slot->facility;
    sim_damage_slot(sim, city, slot_index, amount, cause, events);
    if (slot->state != SIM_SLOT_EMPTY || attacker_id >= sim->unit_count || kind->new_year_removal) return;
    sim_unit *attacker = sim->units + attacker_id;
    u32 loot = sim_percent_of(kind->cost, sim->content.rules.loot_percent);
    if (kind->effect == FACILITY_EFFECT_GRAIN || kind->effect == FACILITY_EFFECT_GRANARY || kind->effect == FACILITY_EFFECT_GRAIN_BY_SOLDIERS)
    {
        attacker->grain += loot;
    }
    else if (kind->effect == FACILITY_EFFECT_CONSCRIPTION || kind->effect == FACILITY_EFFECT_DRILL)
    {
        attacker->soldiers += loot;
    }
    else
    {
        attacker->gold += loot;
    }
    if (loot) sim_emit_unit(sim, SIM_EVENT_UNIT_LOOT, attacker_id, SIM_NO_CITY, (s32)loot, true, cause, events);
}

/* ---- 攻擊 and 齊攻 ---------------------------------------------------------------- */

/* The checks every combat order shares: the unit, its turn, its state. */
internal sim_refusal
sim_check_combat_unit(sim_state *sim, sim_unit_order *order, sim_unit **unit_out)
{
    sim_refusal refusal = sim_check_unit_common(sim, order, unit_out);
    if (refusal != SIM_REFUSAL_NONE) return refusal;
    if ((*unit_out)->state != UNIT_STATE_NONE) return SIM_REFUSAL_UNIT_CONFUSED;
    return SIM_REFUSAL_NONE;
}

/* The hexes of a normal attack: 1, or up to the range of the class (2 for
   crossbows, towers and ships; 2 for cavalry with 騎射 or 白馬). */
internal u32
sim_attack_range(sim_state *sim, sim_unit *unit)
{
    unit_type_kind *type = sim_unit_type(sim, sim_effective_type(sim, unit));
    if (!type) return 0;
    u32 range = type->range;
    if (type->aptitude == OFFICER_APTITUDE_CAVALRY &&
        (sim_has_technique_effect(sim, unit->force_id, TECHNIQUE_EFFECT_MOUNTED_ARCHERY, CONTENT_CLASS_CAVALRY) ||
         sim_unit_has_skill(sim, unit->officers, 25)))
    {
        range = 2;
    }
    if (type->aptitude == OFFICER_APTITUDE_CROSSBOW)
    {
        range += sim_technique_effect_value(sim, unit->force_id, TECHNIQUE_EFFECT_RANGE);
    }
    return range;
}

internal sim_refusal
sim_check_attack_target(sim_state *sim, sim_unit *unit, hex_coord hex, sim_target *target)
{
    unit_type_kind *type = sim_unit_type(sim, sim_effective_type(sim, unit));
    if (!type || !type->can_attack) return SIM_REFUSAL_NO_APTITUDE;
    u32 distance = (u32)hex_distance(unit->hex, hex);
    if (distance < 1 || distance > sim_attack_range(sim, unit)) return SIM_REFUSAL_NO_TARGET;
    *target = sim_target_at(sim, unit->force_id, hex);
    if (!sim_target_any(target)) return SIM_REFUSAL_NO_TARGET;
    terrain_kind *terrain = sim_terrain_at(sim, hex);
    /* No arrow attack into a forest, unless 騎射 cavalry or 射手 (manual p.17). */
    if (distance > 1 && terrain && strcmp(terrain->key, "forest") == 0 && !sim_unit_has_skill(sim, unit->officers, sim->content.rules.skill_archer) &&
        type->aptitude != OFFICER_APTITUDE_CAVALRY)
    {
        return SIM_REFUSAL_BAD_TERRAIN;
    }
    return SIM_REFUSAL_NONE;
}

/* One strike of a unit on the target of a hex: the damage, the morale of
   the target, and the counterattack. Returns the soldiers that the target
   lost. percent carries the multipliers of a tactic (100 for an attack). */
internal u32
sim_strike(sim_state *sim, u32 attacker_id, hex_coord hex, sim_target *target, u32 percent, u32 action, b32 counter_allowed,
           b32 halberd_tactic, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    sim_unit *attacker = sim->units + attacker_id;
    sim_unit_values values = sim_battle_values(sim, attacker);
    u32 distance = (u32)hex_distance(attacker->hex, hex);
    u32 bonus = 100 + sim_building_bonus(sim, attacker->force_id, attacker->hex, BUILDING_DRUM) +
                sim_building_bonus(sim, attacker->force_id, attacker->hex, BUILDING_MAZE);
    percent = (u32)((u64)percent * bonus / 100);
    if (sim_is_critical(sim, attacker, action, target, distance))
    {
        percent = sim_percent_of(percent, rules->combat_critical_percent);
        sim_emit_unit(sim, SIM_EVENT_CRITICAL_HIT, attacker_id, SIM_NO_CITY, 0, false, cause, events);
    }
    if (target->unit != SIM_NO_UNIT)
    {
        sim_unit *defender = sim->units + target->unit;
        sim_unit_values defence = sim_battle_values(sim, defender);
        u32 reduce = sim_building_bonus(sim, defender->force_id, defender->hex, BUILDING_CAMP);
        u32 final_percent = (u32)((u64)percent * (100 - (reduce < 100 ? reduce : 99)) / 100);
        if (sim_unit_has_skill(sim, defender->officers, rules->skill_rattan)) final_percent /= 2;
        u32 damage = sim_combat_damage(sim, attacker->soldiers, values.attack, defence.defence, final_percent);
        /* 箭盾 and 大盾: 30 percent of the halberd's chance to block (documented). */
        unit_type_kind *defender_type = sim_unit_type(sim, sim_effective_type(sim, defender));
        if (defender_type && defender_type->aptitude == OFFICER_APTITUDE_HALBERD)
        {
            u32 block = sim_technique_effect_value(sim, defender->force_id, distance > 1 ? TECHNIQUE_EFFECT_ARROW_BLOCK : TECHNIQUE_EFFECT_ATTACK_BLOCK);
            if (block && sim_random_below(sim, 100) < block) damage = 0;
        }
        u32 before = defender->soldiers;
        sim_hurt_unit(sim, target->unit, damage, attacker_id, halberd_tactic, cause, events);
        u32 lost = before - (defender->alive ? defender->soldiers : 0);
        if (defender->alive)
        {
            u32 hit = rules->combat_morale_hit;
            if (sim_unit_has_skill(sim, attacker->officers, rules->skill_sweep)) hit += rules->combat_morale_hit;
            if (sim_unit_has_skill(sim, attacker->officers, rules->skill_awe))   hit += 2 * rules->combat_morale_hit;
            sim_change_morale(sim, target->unit, -(s32)hit, cause, events);
        }
        /* The counterattack: melee only, not by siege weapons; 還射 lets
           crossbows answer arrows; 急襲 avoids it half the time on land. */
        b32 answers = defender->alive && defender->state == UNIT_STATE_NONE && defender_type && defender_type->counterattack;
        if (distance > 1) answers = answers && defender_type->aptitude == OFFICER_APTITUDE_CROSSBOW &&
                                    sim_has_technique_effect(sim, defender->force_id, TECHNIQUE_EFFECT_COUNTER_ARROW, CONTENT_CLASS_CROSSBOW);
        if (answers && counter_allowed && attacker->alive)
        {
            if (sim_unit_has_skill(sim, attacker->officers, rules->skill_surprise) && !sim_hex_is_water(sim, attacker->hex) &&
                sim_random_below(sim, 100) < 50) return lost;
            u32 counter = sim_combat_damage(sim, defender->soldiers, defence.attack, values.defence, rules->combat_counter_percent);
            sim_emit_unit(sim, SIM_EVENT_COUNTERATTACK, target->unit, SIM_NO_CITY, 0, false, cause, events);
            sim_hurt_unit(sim, attacker_id, counter, target->unit, false, cause, events);
        }
        return lost;
    }
    if (target->city != SIM_NO_CITY)
    {
        city_component *city = sim_find_city(sim, target->city);
        if (sim_has_technique_effect(sim, attacker->force_id, TECHNIQUE_EFFECT_BASE_ATTACK, CONTENT_CLASS_NONE))
        {
            percent = sim_percent_of(percent, rules->siege_ladder_percent);
        }
        officer_component *governor = sim_find_officer(sim, sim_city_governor(sim, city->city_id));
        u32 defence = governor ? governor->leadership : 50;
        u32 damage = sim_combat_damage(sim, attacker->soldiers, values.attack, defence, percent);
        if (counter_allowed && distance == 1 && rules->combat_base_counter)
        {
            u32 counter = rules->combat_base_counter;
            if (sim_has_technique_effect(sim, city->owner_force_id, TECHNIQUE_EFFECT_BASE_COUNTER, CONTENT_CLASS_NONE))
            {
                counter = sim_percent_of(counter, rules->siege_defence_percent);
            }
            sim_hurt_unit(sim, attacker_id, counter, SIM_NO_UNIT, false, cause, events);
        }
        if (attacker->alive) sim_hurt_city(sim, city, damage, sim_percent_of(damage, rules->combat_building_percent), attacker_id, cause, events);
        return damage;
    }
    if (target->base != SIM_NO_BASE)
    {
        /* M8: a port or a pass falls like a city (siege-and-bases.md); its walls answer too. */
        u32 damage = sim_combat_damage(sim, attacker->soldiers, values.attack, 50, percent);
        if (counter_allowed && distance == 1 && rules->combat_base_counter) sim_hurt_unit(sim, attacker_id, rules->combat_base_counter, SIM_NO_UNIT, false, cause, events);
        if (attacker->alive) sim_hurt_base(sim, target->base, damage, sim_percent_of(damage, rules->combat_building_percent), attacker_id, cause, events);
        return damage;
    }
    u32 damage = sim_combat_damage(sim, attacker->soldiers, values.attack, 100, percent);
    u32 broken = sim_percent_of(damage, rules->combat_building_percent);
    if (target->building != SIM_NO_BUILDING)
    {
        sim_hurt_building(sim, target->building, broken, sim_direction_toward(attacker->hex, hex), cause, events);
    }
    else if (target->slot_city != SIM_NO_CITY)
    {
        sim_hurt_slot(sim, target->slot_city, target->slot, broken, attacker_id, cause, events);
    }
    return 0;
}

internal sim_check
sim_check_attack(sim_state *sim, sim_unit_order *order)
{
    sim_check check = {};
    check.slot = SIM_ANY_SLOT;
    sim_unit *unit = 0;
    sim_refusal refusal = sim_check_combat_unit(sim, order, &unit);
    if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
    sim_target target;
    refusal = sim_check_attack_target(sim, unit, order->to, &target);
    if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
    check.assumed = true;
    if (target.unit != SIM_NO_UNIT)
    {
        sim_unit_values attack = sim_battle_values(sim, unit), defence = sim_battle_values(sim, sim->units + target.unit);
        check.quantity     = sim_combat_damage(sim, unit->soldiers, attack.attack, defence.defence, 100);
        check.value_before = sim->units[target.unit].soldiers;
    }
    return check;
}

/* The merit and the experience of a battle (fixtures/battle-merit.tsv, run
   of 2026-10-22). Each officer of the attacker gains merit.attack for a
   melee attack, merit.arrow for an arrow attack and merit.tactic for a
   tactic, or merit.destroy (merit.tactic_destroy) when the target unit is
   destroyed. The attackers gain force experience, the destroyers also
   leadership experience. The officers of a target that stands gain
   leadership experience. */
internal void
sim_reward_battle(sim_state *sim, u32 attacker_id, u32 target_id, b32 arrow, b32 tactic, b32 destroyed, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    if (attacker_id >= sim->unit_count || !sim->units[attacker_id].alive) return;
    sim_unit *attacker = sim->units + attacker_id;
    u32 merit = destroyed ? (tactic ? rules->merit_tactic_destroy : rules->merit_destroy)
                          : tactic ? rules->merit_tactic : arrow ? rules->merit_arrow : rules->merit_attack;
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
    {
        officer_component *officer = sim_find_officer(sim, attacker->officers[place]);
        if (!officer) continue;
        if (merit)
        {
            u32 total = Minimum((u32)officer->merit + merit, 0xFFFFu);
            sim_event event = sim_event_make(sim, SIM_EVENT_MERIT_GAINED, cause);
            event.subject = sim_officer_entity(sim, officer->officer_id);
            event.faction = attacker->force_id;
            event.delta   = (s32)(total - officer->merit);
            officer->merit = (u16)total;
            sim_emit(events, event);
        }
        sim_gain_experience(sim, officer, CONTENT_ABILITY_FORCE, tactic ? rules->experience_tactic_war : rules->experience_attack_war, cause, events);
        if (destroyed) sim_gain_experience(sim, officer, CONTENT_ABILITY_LEADERSHIP, rules->experience_destroy_leadership, cause, events);
    }
    sim_unit *target = sim_find_unit(sim, target_id);
    if (!target) return;
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
    {
        officer_component *officer = sim_find_officer(sim, target->officers[place]);
        if (officer) sim_gain_experience(sim, officer, CONTENT_ABILITY_LEADERSHIP, rules->experience_hit_leadership, cause, events);
    }
}

internal void
sim_attack(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_unit_order *order = &command.unit;
    if (sim_check_attack(sim, order).refusal != SIM_REFUSAL_NONE) return;
    sim_unit *unit = sim->units + order->unit;
    sim_target target = sim_target_at(sim, unit->force_id, order->to);
    unit->acted_turn = sim->turn + 1;
    u32 target_unit = target.unit;
    b32 arrow = hex_distance(unit->hex, order->to) > 1;
    sim_strike(sim, order->unit, order->to, &target, 100, CRITICAL_ATTACK, true, false, command.id, events);
    sim_reward_battle(sim, order->unit, target_unit, arrow, false, target_unit != SIM_NO_UNIT && !sim->units[target_unit].alive, command.id, events);
}

/* The units of the force that can join an attack on the hex now: the
   units next to the hex (military-units-and-tactics.md: "a joint attack
   by several adjacent units"). A crossbow unit two hexes away keeps its
   turn. */
internal u32
sim_joint_partners(sim_state *sim, u8 force, hex_coord hex, u32 *ids, u32 capacity)
{
    u32 count = 0;
    for (u32 index = 0; index < sim->unit_count && count < capacity; ++index)
    {
        sim_unit *unit = sim->units + index;
        if (!unit->alive || unit->force_id != force || unit->acted_turn == sim->turn + 1 || unit->state != UNIT_STATE_NONE) continue;
        if (hex_distance(unit->hex, hex) != 1) continue;
        sim_target target;
        if (sim_check_attack_target(sim, unit, hex, &target) == SIM_REFUSAL_NONE) ids[count++] = index;
    }
    return count;
}

/* 齊攻: every unit of the force that can attack the hex attacks it; the
   target answers none of them (assumed, Q-172). The units then wait. */
internal sim_check
sim_check_joint_attack(sim_state *sim, sim_unit_order *order)
{
    sim_check check = sim_check_attack(sim, order);
    if (check.refusal != SIM_REFUSAL_NONE) return check;
    u32 ids[SIM_UNIT_CAPACITY];
    u32 count = sim_joint_partners(sim, sim->units[order->unit].force_id, order->to, ids, SIM_UNIT_CAPACITY);
    if (count < 2) return sim_refuse(check, SIM_REFUSAL_NO_PARTNER);
    check.quantity = count;
    return check;
}

internal void
sim_joint_attack(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_unit_order *order = &command.unit;
    if (sim_check_joint_attack(sim, order).refusal != SIM_REFUSAL_NONE) return;
    u32 ids[SIM_UNIT_CAPACITY];
    u32 count = sim_joint_partners(sim, sim->units[order->unit].force_id, order->to, ids, SIM_UNIT_CAPACITY);
    for (u32 index = 0; index < count; ++index)
    {
        sim_unit *unit = sim->units + ids[index];
        if (!unit->alive) continue;
        sim_target target = sim_target_at(sim, unit->force_id, order->to);
        if (!sim_target_any(&target)) break;   /* the target is gone: the units after it keep their turn */
        unit->acted_turn = sim->turn + 1;
        u32 target_unit = target.unit;
        b32 arrow = hex_distance(unit->hex, order->to) > 1;
        sim_strike(sim, ids[index], order->to, &target, 100, CRITICAL_ATTACK, false, false, command.id, events);
        sim_reward_battle(sim, ids[index], target_unit, arrow, false, target_unit != SIM_NO_UNIT && !sim->units[target_unit].alive, command.id, events);
    }
}

/* ---- 戰法 --------------------------------------------------------------------- */

internal tactic_kind *
sim_tactic(sim_state *sim, u32 tactic)
{
    return tactic < sim->content.tactic_count ? sim->content.tactics + tactic : 0;
}

/* The bit of terrain_kind.tactics for the class of a unit. */
internal u32
sim_tactic_bit(unit_type_kind *type)
{
    switch (type->aptitude)
    {
        case OFFICER_APTITUDE_SPEAR:    return 1u << TACTIC_PIKE;
        case OFFICER_APTITUDE_HALBERD:  return 1u << TACTIC_HALBERD;
        case OFFICER_APTITUDE_CROSSBOW: return 1u << TACTIC_CROSSBOW;
        case OFFICER_APTITUDE_CAVALRY:  return 1u << TACTIC_CAVALRY;
        case OFFICER_APTITUDE_SIEGE:    return 1u << TACTIC_SIEGE;
        case OFFICER_APTITUDE_NAVY:     return 1u << TACTIC_NAVY;
        default:                        return 0;
    }
}

internal sim_check
sim_check_tactic(sim_state *sim, sim_unit_order *order)
{
    sim_check check = {};
    check.slot = SIM_ANY_SLOT;
    sim_unit *unit = 0;
    sim_refusal refusal = sim_check_combat_unit(sim, order, &unit);
    if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
    tactic_kind *tactic = sim_tactic(sim, order->kind);
    u8 type_id = sim_effective_type(sim, unit);
    unit_type_kind *type = sim_unit_type(sim, type_id);
    if (!tactic || !type || !((tactic->unit_types >> type_id) & 1)) return sim_refuse(check, SIM_REFUSAL_NO_APTITUDE);
    sim_unit_values values = sim_unit_values_of(sim, unit->force_id, type_id, unit->officers);
    u32 aptitude = values.aptitude <= 3 ? values.aptitude : 0;
    if (aptitude < tactic->aptitude)           return sim_refuse(check, SIM_REFUSAL_NO_APTITUDE);
    if (unit->morale < tactic->morale)         return sim_refuse(check, SIM_REFUSAL_NO_MORALE);
    terrain_kind *terrain = sim_terrain_at(sim, unit->hex);
    if (terrain && !(terrain->tactics & sim_tactic_bit(type))) return sim_refuse(check, SIM_REFUSAL_BAD_TERRAIN);
    u32 distance = (u32)hex_distance(unit->hex, order->to);
    if (distance < tactic->range_min || distance > tactic->range_max) return sim_refuse(check, SIM_REFUSAL_NO_TARGET);
    sim_target target = sim_target_at(sim, unit->force_id, order->to);
    b32 unit_ok = target.unit != SIM_NO_UNIT && (tactic->targets & (sim_hex_is_water(sim, order->to) ? (TARGET_SHIPS | TARGET_UNITS) : TARGET_UNITS));
    if (target.unit != SIM_NO_UNIT && !sim_hex_is_water(sim, order->to) && !(tactic->targets & TARGET_UNITS)) unit_ok = false;
    b32 building_ok = (target.building != SIM_NO_BUILDING || target.city != SIM_NO_CITY || target.slot_city != SIM_NO_CITY || target.base != SIM_NO_BASE) &&
                      (tactic->targets & TARGET_BUILDINGS);
    if (!unit_ok && !building_ok)              return sim_refuse(check, SIM_REFUSAL_NO_TARGET);
    check.action_points = tactic->morale;      /* the morale of the tactic */
    check.assumed       = true;
    u32 chance = 100;
    if (!tactic->always_success)
    {
        s32 percent = (s32)sim->content.rules.combat_tactic_success;
        if (target.unit != SIM_NO_UNIT) percent += ((s32)values.intellect - (s32)sim_battle_values(sim, sim->units + target.unit).intellect) / 2;
        chance = (u32)(percent < 10 ? 10 : (percent > 95 ? 95 : percent));
    }
    check.quantity = chance;                   /* the chance of success in percent (成功率) */
    return check;
}

/* The hexes that a tactic hits, by its pattern. */
internal u32
sim_tactic_cells(sim_unit *unit, tactic_kind *tactic, hex_coord to, hex_coord *cells)
{
    u32 count = 0;
    switch (tactic->pattern)
    {
        case PATTERN_LINE:
        {
            hex_coord at = unit->hex;
            for (u32 step = 0; step < 8 && !hex_equal(at, to); ++step)
            {
                at = hex_neighbor(at, sim_direction_toward(at, to));
                cells[count++] = at;
            }
            break;
        }
        case PATTERN_FRONT3:
            cells[count++] = to;
            for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d)
            {
                hex_coord side = hex_neighbor(to, d);
                if (hex_distance(side, unit->hex) == 1) cells[count++] = side;
            }
            break;
        case PATTERN_AROUND:
            for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d) cells[count++] = hex_neighbor(unit->hex, d);
            break;
        case PATTERN_SPLASH:
            cells[count++] = to;
            for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d) cells[count++] = hex_neighbor(to, d);
            break;
        default:
            cells[count++] = to;
            break;
    }
    return count;
}

/* True if a unit of the class can stand on the hex: on the map, enterable,
   and free of units, buildings and cities. */
internal b32
sim_hex_free_for(sim_state *sim, sim_unit *unit, hex_coord hex)
{
    unit_type_kind *type = sim_unit_type(sim, unit->unit_type);
    if (!type || !hex_map_contains(&sim->map, hex)) return false;
    terrain_kind *terrain = sim_terrain_at(sim, hex);
    if (!terrain || (!terrain->cost[type->movement] && !(type->movement != MOVEMENT_NAVY && terrain->cost[MOVEMENT_NAVY]))) return false;
    return sim_unit_at(sim, hex) == SIM_NO_UNIT && sim_building_at(sim, hex) == SIM_NO_BUILDING && sim_city_at(sim, hex) == SIM_NO_CITY;
}

/* A tactic moves a unit (manual p.15): it slides hex by hex in a direction.
   A unit or a building in the way stops it, and both lose soldiers; a trap
   in the way starts. */
internal void
sim_push_unit(sim_state *sim, u32 unit_id, u32 direction, u32 hexes, u32 cause, sim_event_sink *events)
{
    sim_unit *unit = sim->units + unit_id;
    u32 moved = 0;
    for (u32 step = 0; step < hexes && unit->alive; ++step)
    {
        hex_coord next = hex_neighbor(unit->hex, direction);
        u32 building = sim_building_at(sim, next);
        if (building != SIM_NO_BUILDING)
        {
            building_type type = sim->content.buildings[sim->buildings[building].kind].type;
            if (type == BUILDING_SEED || type == BUILDING_BALL || type == BUILDING_SHIP) sim_trigger_trap(sim, building, direction, cause, events);
            else sim_hurt_building(sim, building, sim_percent_of(unit->soldiers, sim->content.rules.combat_collision_percent), direction, cause, events);
            sim_hurt_unit(sim, unit_id, sim_percent_of(unit->soldiers, sim->content.rules.combat_collision_percent), SIM_NO_UNIT, false, cause, events);
            break;
        }
        u32 other = sim_unit_at(sim, next);
        if (other != SIM_NO_UNIT)
        {
            u32 loss = sim_percent_of(unit->soldiers, sim->content.rules.combat_collision_percent);
            sim_hurt_unit(sim, other, loss, SIM_NO_UNIT, false, cause, events);
            sim_hurt_unit(sim, unit_id, loss, SIM_NO_UNIT, false, cause, events);
            break;
        }
        if (!sim_hex_free_for(sim, unit, next)) break;
        unit->hex = next;
        moved++;
    }
    if (moved) sim_emit_unit(sim, SIM_EVENT_UNIT_PUSHED, unit_id, SIM_NO_CITY, (s32)moved, false, cause, events);
}

/* The effect of a tactic that succeeds, on its first target. */
internal void
sim_tactic_effect(sim_state *sim, u32 unit_id, tactic_kind *tactic, hex_coord to, u32 target_id, u32 cause, sim_event_sink *events)
{
    sim_unit *unit = sim->units + unit_id;
    u32 direction = sim_direction_toward(unit->hex, to);
    b32 target_alive = target_id != SIM_NO_UNIT && sim->units[target_id].alive;
    switch (tactic->effect)
    {
        case TACTIC_KNOCK:
            if (target_alive) sim_push_unit(sim, target_id, direction, tactic->value, cause, events);
            break;
        case TACTIC_ADVANCE:
        {
            hex_coord before = target_alive ? sim->units[target_id].hex : to;
            if (target_alive) sim_push_unit(sim, target_id, direction, tactic->value, cause, events);
            if (sim_hex_free_for(sim, unit, before) && hex_distance(unit->hex, before) == 1)
            {
                unit->hex = before;
                sim_emit_unit(sim, SIM_EVENT_UNIT_PUSHED, unit_id, SIM_NO_CITY, 1, false, cause, events);
            }
            break;
        }
        case TACTIC_PULL:
        {
            hex_coord behind = hex_neighbor(unit->hex, (direction + 3) % HEX_DIRECTION_COUNT);
            if (target_alive && sim_hex_free_for(sim, unit, behind))
            {
                hex_coord old = unit->hex;
                unit->hex = behind;
                sim->units[target_id].hex = old;
                sim_emit_unit(sim, SIM_EVENT_UNIT_PUSHED, target_id, SIM_NO_CITY, 1, false, cause, events);
            }
            break;
        }
        case TACTIC_PASS:
        {
            hex_coord beyond = hex_neighbor(to, direction);
            if (sim_hex_free_for(sim, unit, beyond))
            {
                unit->hex = beyond;
                sim_emit_unit(sim, SIM_EVENT_UNIT_PUSHED, unit_id, SIM_NO_CITY, 2, false, cause, events);
            }
            break;
        }
        case TACTIC_CONFUSE:
            if (target_alive) sim_set_unit_state(sim, target_id, UNIT_STATE_CONFUSED, tactic->value, cause, events);
            break;
        case TACTIC_FIRE:
            sim_start_fire(sim, to, sim_fire_turns(sim, true), cause, events);   /* a fire from an attack: 1 to 3 turns */
            break;
        default:
            break;
    }
}

internal void
sim_use_tactic(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_unit_order *order = &command.unit;
    sim_check check = sim_check_tactic(sim, order);
    if (check.refusal != SIM_REFUSAL_NONE) return;
    sim_rules *rules = &sim->content.rules;
    sim_unit *unit = sim->units + order->unit;
    tactic_kind *tactic = sim_tactic(sim, order->kind);
    unit->acted_turn = sim->turn + 1;
    sim_change_morale(sim, order->unit, -(s32)tactic->morale, command.id, events);
    b32 success = check.quantity >= 100 || sim_random_below(sim, 100) < check.quantity;
    sim_emit_unit(sim, SIM_EVENT_TACTIC_USED, order->unit, SIM_NO_CITY, success ? 1 : 0, true, command.id, events);
    if (!success) return;
    /* A tactic that succeeds gives technique points (G2-048: 火矢 gave 15; tech-tree.md). */
    sim_add_technique_points(sim, unit->force_id, (s32)rules->technique_tactic, command.id, events);

    unit_type_kind *type = sim_unit_type(sim, sim_effective_type(sim, unit));
    content_unit_class unit_class = sim_class_of_aptitude(type->aptitude);
    u32 percent = tactic->damage_percent;
    if (unit_class != CONTENT_CLASS_NONE && sim_has_technique_effect(sim, unit->force_id, TECHNIQUE_EFFECT_TACTIC_DAMAGE, unit_class))
    {
        percent = sim_percent_of(percent, rules->combat_technique_percent);
    }
    b32 halberd = type->aptitude == OFFICER_APTITUDE_HALBERD;
    sim_target first = sim_target_at(sim, unit->force_id, order->to);
    u32 first_unit = first.unit;
    hex_coord cells[16];
    u32 count = sim_tactic_cells(unit, tactic, order->to, cells);
    b32 hits_own = tactic->pattern == PATTERN_SPLASH || tactic->pattern == PATTERN_LINE;
    for (u32 index = 0; index < count && unit->alive; ++index)
    {
        sim_target target = sim_target_at(sim, unit->force_id, cells[index]);
        if (hits_own && target.unit == SIM_NO_UNIT)
        {
            u32 own = sim_unit_at(sim, cells[index]);
            if (own != SIM_NO_UNIT && own != order->unit) target.unit = own;
        }
        if (target.unit != SIM_NO_UNIT && (tactic->targets & TARGET_UNITS))
        {
            u32 unit_percent = percent;
            unit_type_kind *other = sim_unit_type(sim, sim_effective_type(sim, sim->units + target.unit));
            if (other && sim_class_beats(type->aptitude, other->aptitude)) unit_percent = sim_percent_of(unit_percent, rules->combat_triangle_percent);
            sim_target only_unit = target;
            only_unit.building = SIM_NO_BUILDING; only_unit.city = SIM_NO_CITY; only_unit.slot_city = SIM_NO_CITY; only_unit.base = SIM_NO_BASE;
            sim_strike(sim, order->unit, cells[index], &only_unit, unit_percent, CRITICAL_TACTIC, false, halberd, command.id, events);
        }
        else if (index == 0 && (tactic->targets & TARGET_BUILDINGS) && sim_target_any(&target))
        {
            /* A siege weapon breaks durability by its destroy grade; the
               others strike as an attack (siege-and-bases.md). */
            u32 broken = sim_percent_of(sim_percent_of(rules->combat_siege_percent, rules->grade_percent[type->destroy]), percent);
            if (target.city != SIM_NO_CITY)
            {
                city_component *city = sim_find_city(sim, target.city);
                u32 soldiers = type->destroy == GRADE_NONE || type->range > 1 ?
                               sim_combat_damage(sim, unit->soldiers, sim_battle_values(sim, unit).attack, 50, percent) : 0;
                sim_hurt_city(sim, city, soldiers, type->destroy != GRADE_NONE ? broken : 0, order->unit, command.id, events);
            }
            else if (target.base != SIM_NO_BASE)
            {
                u32 soldiers = type->destroy == GRADE_NONE || type->range > 1 ?
                               sim_combat_damage(sim, unit->soldiers, sim_battle_values(sim, unit).attack, 50, percent) : 0;
                sim_hurt_base(sim, target.base, soldiers, type->destroy != GRADE_NONE ? broken : 0, order->unit, command.id, events);
            }
            else if (target.building != SIM_NO_BUILDING)
            {
                sim_hurt_building(sim, target.building, type->destroy != GRADE_NONE ? broken : sim_percent_of(percent, 100) * 2, sim_direction_toward(unit->hex, order->to),
                                  command.id, events);
            }
            else if (target.slot_city != SIM_NO_CITY)
            {
                sim_hurt_slot(sim, target.slot_city, target.slot, type->destroy != GRADE_NONE ? broken : percent * 2, order->unit, command.id, events);
            }
        }
    }
    if (unit->alive) sim_tactic_effect(sim, order->unit, tactic, order->to, first_unit, command.id, events);
    sim_reward_battle(sim, order->unit, first_unit, false, true, first_unit != SIM_NO_UNIT && !sim->units[first_unit].alive, command.id, events);
}
