/* The ports and passes of M8 (bases-ports-and-passes.md, siege-and-bases.md):
   45 bases with an owner, durability and soldiers on the placeholder map. A
   base gives gold to its city when both have the same owner; an attack
   takes its soldiers and durability, and a base without soldiers or
   durability falls to the attacker. Included by sim.cpp after
   rivals.cpp.

   The owner city and the durability are documented. The base records of
   the save are not decoded: the owner at the start is the owner of the
   city, and the soldiers, the gold and the place on the map are assumed
   (Q-183, Q-034). */

internal sim_base_state *
sim_find_base(sim_state *sim, u32 index)
{
    return index < sim->content.base_count && sim->bases[index].exists ? sim->bases + index : 0;
}

/* The base on the hex: its index in bases.tsv, or SIM_NO_BASE. */
internal u32
sim_base_at(sim_state *sim, hex_coord hex)
{
    for (u32 index = 0; index < sim->content.base_count; ++index)
    {
        if (sim->bases[index].exists && hex_equal(sim->bases[index].hex, hex)) return index;
    }
    return SIM_NO_BASE;
}

/* True if the hex is free for a base: on the map, firm ground, no city, no
   slot of a city, no base. */
internal b32
sim_base_hex_free(sim_state *sim, hex_coord hex)
{
    terrain_kind *terrain = sim_terrain_at(sim, hex);
    if (!terrain || !terrain->cost[MOVEMENT_FOOT]) return false;
    if (sim_city_at(sim, hex) != SIM_NO_CITY || sim_base_at(sim, hex) != SIM_NO_BASE) return false;
    for (u32 city = 0; city < SAVE_CITY_COUNT; ++city)
    {
        sim_city_site *site = sim->sites + city;
        if (hex_equal(site->hex, hex)) return false;
        for (u32 slot = 0; slot < site->slot_count; ++slot) if (hex_equal(site->slots[slot].hex, hex)) return false;
    }
    return true;
}

/* A port wants a hex next to water. */
internal b32
sim_hex_near_water(sim_state *sim, hex_coord hex)
{
    for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d) if (sim_hex_is_water(sim, hex_neighbor(hex, d))) return true;
    return false;
}

/* PLACEHOLDER (Q-034): the first free hex on the rings 2 to 8 around the
   owner city. Each ring starts on a side that the base number turns, so
   that the bases of one city spread; a port takes a hex next to water when
   one exists. */
internal hex_coord
sim_place_base(sim_state *sim, base_kind *kind)
{
    hex_coord center = sim->sites[kind->owner_city].hex;
    for (u32 pass = 0; pass < 2; ++pass)
    {
        b32 want_water = kind->type == BASE_PORT && pass == 0;
        for (s32 distance = 2; distance <= 8; ++distance)
        {
            u32 start = kind->id % HEX_DIRECTION_COUNT;
            hex_coord hex = center;
            for (s32 step = 0; step < distance; ++step) hex = hex_neighbor(hex, (start + 4) % HEX_DIRECTION_COUNT);
            for (u32 side = 0; side < HEX_DIRECTION_COUNT; ++side)
            {
                for (s32 step = 0; step < distance; ++step)
                {
                    if (sim_base_hex_free(sim, hex) && (!want_water || sim_hex_near_water(sim, hex))) return hex;
                    hex = hex_neighbor(hex, (start + side) % HEX_DIRECTION_COUNT);
                }
            }
        }
    }
    return center;
}
/* The bases after a save loads. */
internal void
sim_init_bases(sim_state *sim)
{
    for (u32 index = 0; index < CONTENT_BASE_COUNT; ++index) sim->bases[index] = {};
    for (u32 index = 0; index < sim->content.base_count; ++index)
    {
        base_kind *kind = sim->content.bases + index;
        sim_base_state *base = sim->bases + index;
        city_component *city = sim_find_city(sim, kind->owner_city);
        base->exists     = true;
        base->force      = city ? city->owner_force_id : (u8)SIM_NO_FORCE;
        base->durability = kind->durability;
        base->troops     = base->force != SIM_NO_FORCE ? sim->content.rules.base_start_troops : 0;
        base->hex        = sim_place_base(sim, kind);
    }
}

/* The soldier cap of a base: base.troop_cap, more with 擴展港關. */
internal u32
sim_base_troop_cap(sim_state *sim, u8 force)
{
    u32 cap = sim->content.rules.base_troop_cap;
    if (sim_has_technique_effect(sim, force, TECHNIQUE_EFFECT_PORT_CAPS, CONTENT_CLASS_NONE)) cap = sim_percent_of(cap, sim->content.rules.base_caps_percent);
    return cap;
}

/* The gold that the bases give to a city each month: base.gold for each
   base of the city with the same owner (manual p.25: a base has income
   only with its city). The monthly income of the city includes it. */
internal u32
sim_city_base_gold(sim_state *sim, u8 city_id)
{
    city_component *city = sim_find_city(sim, city_id);
    if (!city) return 0;
    u32 gold = 0;
    for (u32 index = 0; index < sim->content.base_count; ++index)
    {
        sim_base_state *base = sim->bases + index;
        if (base->exists && sim->content.bases[index].owner_city == city_id && base->force != SIM_NO_FORCE && base->force == city->owner_force_id)
        {
            gold += sim->content.rules.base_gold;
        }
    }
    return gold;
}

/* An attack on a base: soldiers and durability. At 0 of either it falls to
   the attacker (siege-and-bases.md). */
internal void
sim_hurt_base(sim_state *sim, u32 index, u32 soldiers, u32 durability, u32 attacker_id, u32 cause, sim_event_sink *events)
{
    sim_base_state *base = sim_find_base(sim, index);
    if (!base) return;
    base->troops     -= Minimum(soldiers, base->troops);
    base->durability -= Minimum(durability, base->durability);
    sim_unit *attacker = attacker_id < sim->unit_count && sim->units[attacker_id].alive ? sim->units + attacker_id : 0;
    if ((base->troops && base->durability) || !attacker) return;
    base->force      = attacker->force_id;
    base->durability = sim->content.bases[index].durability / 2;
    base->troops     = 0;
    sim_event event = sim_event_make(sim, SIM_EVENT_BASE_TAKEN, cause);
    event.faction = attacker->force_id;
    event.detail  = sim->content.bases[index].id;
    sim_emit(events, event);
    sim_add_technique_points(sim, attacker->force_id, (s32)sim->content.rules.siege_capture_technique, cause, events);
}
