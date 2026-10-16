/* The development area of a city and the develop command (開發;
   city-commands.md, cities-and-facilities.md). Included by sim.cpp.

   PLACEHOLDER LAYOUT. The spec gives the slot count of each city but not
   the shape of the slots (Q-145). Until it does, slot 0 to 5 are the ring
   of hexes next to the city, slot 6 to 17 the second ring, and so on. The
   city stands on a grid of the placeholder map. */

#define SIM_SITE_GRID_COLUMNS 7
#define SIM_SITE_GRID_STEP_COL 5
#define SIM_SITE_GRID_STEP_ROW 4
#define SIM_SITE_GRID_MARGIN   3

/* The hex of a slot relative to the city: a spiral of rings. */
internal hex_coord
sim_city_slot_hex(u32 slot)
{
    u32 ring = 1, first_of_ring = 0;
    while (slot >= first_of_ring + HEX_DIRECTION_COUNT * ring)
    {
        first_of_ring += HEX_DIRECTION_COUNT * ring;
        ring++;
    }
    u32 step = slot - first_of_ring;
    hex_coord hex = hex_make(HEX_DIRECTIONS[4].q * (s32)ring, HEX_DIRECTIONS[4].r * (s32)ring);
    for (u32 side = 0; side < HEX_DIRECTION_COUNT; ++side)
    {
        for (u32 along = 0; along < ring; ++along)
        {
            if (step == 0) return hex;
            hex = hex_neighbor(hex, side);
            step--;
        }
    }
    return hex;
}

/* Lays out the slots of every city from the city table, all empty. */
internal void
sim_init_sites(sim_state *sim)
{
    for (u32 city_id = 0; city_id < SAVE_CITY_COUNT; ++city_id)
    {
        sim_city_site *site = sim->sites + city_id;
        hex_offset offset;
        offset.col = (s32)(SIM_SITE_GRID_MARGIN + (city_id % SIM_SITE_GRID_COLUMNS) * SIM_SITE_GRID_STEP_COL);
        offset.row = (s32)(SIM_SITE_GRID_MARGIN + (city_id / SIM_SITE_GRID_COLUMNS) * SIM_SITE_GRID_STEP_ROW);
        site->hex = hex_from_offset(offset);
        u32 slots = city_id < sim->content.city_count ? sim->content.cities[city_id].slot_count : 0;
        site->slot_count = (u8)(slots < SIM_SLOT_CAPACITY ? slots : SIM_SLOT_CAPACITY);
        for (u32 slot_index = 0; slot_index < SIM_SLOT_CAPACITY; ++slot_index)
        {
            sim_slot *slot = site->slots + slot_index;
            *slot = {};
            slot->hex      = hex_add(site->hex, sim_city_slot_hex(slot_index));
            slot->state    = SIM_SLOT_EMPTY;
            slot->facility = CONTENT_NO_FACILITY;
            for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place) slot->officers[place] = SIM_NO_OFFICER;
        }
    }
}

/* ASSUMED LINE (Q-142). The manual (p.11) gives three points: one officer
   with the politics of 曹操 (94) needs 30 days, three good officers need 20
   days, and officers with low politics need 60 days. The guide book (p.9)
   gives the range: from the shortest period of the facility to 100 days.
   This rule draws a straight line from develop.max_days at a politics sum
   of 0 to the shortest period at develop.full_politics, then rounds up to
   whole turns. It meets all three points of the manual for a market. */
internal u32
sim_develop_days(u32 politics_sum, u32 min_days, sim_rules *rules)
{
    u32 days = min_days;
    if (rules->develop_full_politics && rules->develop_max_days > min_days)
    {
        u32 sum = politics_sum < rules->develop_full_politics ? politics_sum : rules->develop_full_politics;
        days = rules->develop_max_days - (rules->develop_max_days - min_days) * sum / rules->develop_full_politics;
    }
    u32 turns = calendar_turns_for_days(days);
    return (turns ? turns : 1) * CALENDAR_DAYS_PER_TURN;
}

internal b32
sim_enemy_near(sim_state *sim, city_component *city)
{
    hex_coord center = sim->sites[city->city_id].hex;
    for (u32 index = 0; index < sim->unit_count; ++index)
    {
        sim_unit *unit = sim->units + index;
        /* Every other force is an enemy until diplomacy exists (M7; assumed). */
        if (unit->force_id != city->owner_force_id &&
            hex_distance(unit->hex, center) <= (s32)sim->content.rules.enemy_block_hexes) return true;
    }
    return false;
}

/* The number of facilities of a kind, built or under construction: in the
   city, or in every city of the owner for a facility of the territory. */
internal u32
sim_facility_count(sim_state *sim, city_component *city, u8 facility, b32 whole_territory)
{
    u32 count = 0;
    for (u32 city_id = 0; city_id < SAVE_CITY_COUNT; ++city_id)
    {
        city_component *other = sim_find_city(sim, (u8)city_id);
        if (!other) continue;
        if (whole_territory ? other->owner_force_id != city->owner_force_id : other != city) continue;
        sim_city_site *site = sim->sites + city_id;
        for (u32 slot = 0; slot < site->slot_count; ++slot)
        {
            if (site->slots[slot].state != SIM_SLOT_EMPTY && site->slots[slot].facility == facility) count++;
        }
    }
    return count;
}

/* Why the city cannot have this facility at all, or SIM_REFUSAL_NONE. */
internal sim_refusal
sim_check_facility_kind(sim_state *sim, city_component *city, u8 facility)
{
    if (facility >= sim->content.facility_count) return SIM_REFUSAL_NO_FACILITY;
    facility_kind *kind = sim->content.facilities + facility;
    city_kind *place = city->city_id < sim->content.city_count ? sim->content.cities + city->city_id : 0;
    /* The items are not modelled yet: no faction has 銅雀 (F-059). */
    if (kind->needs_item) return SIM_REFUSAL_NO_FACILITY;
    if (kind->limit == FACILITY_LIMIT_BIG_CITY && (!place || place->specialty != CITY_SPECIALTY_BIG_CITY)) return SIM_REFUSAL_NO_FACILITY;
    if (kind->limit == FACILITY_LIMIT_PORT_CITY && (!place || !place->has_port)) return SIM_REFUSAL_NO_FACILITY;
    return SIM_REFUSAL_NONE;
}

/* The checks of one officer of the order. */
internal sim_refusal
sim_check_develop_officer(sim_state *sim, city_component *city, officer_component *officer)
{
    if (!officer || !sim_officer_is_active(officer) || officer->force_id != city->owner_force_id ||
        officer->current_position != city->city_id)
    {
        return SIM_REFUSAL_OFFICER_NOT_IN_CITY;
    }
    if (officer->acted_turn == sim->turn + 1) return SIM_REFUSAL_OFFICER_ALREADY_ACTED;
    if (officer->job_city != SIM_NO_CITY)     return SIM_REFUSAL_OFFICER_BUSY;
    return SIM_REFUSAL_NONE;
}

internal sim_check
sim_refuse(sim_check check, sim_refusal refusal)
{
    check.refusal = refusal;
    return check;
}

/* The checks of the develop command, in order. The first reason that
   holds is the answer. */
internal sim_check
sim_check_develop(sim_state *sim, sim_develop_order *order)
{
    sim_check check = {};
    check.slot = SIM_ANY_SLOT;
    if (!sim->has_content) return sim_refuse(check, SIM_REFUSAL_NO_CONTENT);
    city_component *city = sim_find_city(sim, order->city_id);
    if (!city || city->owner_force_id == SIM_NO_FORCE) return sim_refuse(check, SIM_REFUSAL_NO_CITY);
    if (sim_whose_turn(sim) != city->owner_force_id)    return sim_refuse(check, SIM_REFUSAL_NOT_YOUR_TURN);
    sim_refusal kind_refusal = sim_check_facility_kind(sim, city, order->facility);
    if (kind_refusal != SIM_REFUSAL_NONE)               return sim_refuse(check, kind_refusal);
    if (sim_enemy_near(sim, city))                      return sim_refuse(check, SIM_REFUSAL_ENEMY_NEAR);

    facility_kind *kind = sim->content.facilities + order->facility;
    sim_city_site *site = sim->sites + city->city_id;
    u32 free_slot = SIM_ANY_SLOT;
    for (u32 slot = 0; slot < site->slot_count && free_slot == SIM_ANY_SLOT; ++slot)
    {
        if (site->slots[slot].state == SIM_SLOT_EMPTY) free_slot = slot;
    }
    if (free_slot == SIM_ANY_SLOT) return sim_refuse(check, SIM_REFUSAL_NO_SLOT);
    if (kind->limit != FACILITY_LIMIT_NONE &&
        sim_facility_count(sim, city, order->facility, kind->limit == FACILITY_LIMIT_TERRITORY) >= 1)
    {
        return sim_refuse(check, SIM_REFUSAL_VALUE_AT_MAXIMUM);
    }
    u32 slot = order->slot == SIM_ANY_SLOT ? free_slot : order->slot;
    if (slot >= site->slot_count || site->slots[slot].state != SIM_SLOT_EMPTY) return sim_refuse(check, SIM_REFUSAL_SLOT_TAKEN);

    u32 named = 0, politics = 0;
    for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place)
    {
        u16 officer_id = order->officers[place];
        if (officer_id == SIM_NO_OFFICER) continue;
        named++;
        officer_component *officer = sim_find_officer(sim, officer_id);
        sim_refusal officer_refusal = sim_check_develop_officer(sim, city, officer);
        if (officer_refusal != SIM_REFUSAL_NONE) return sim_refuse(check, officer_refusal);
        for (u32 earlier = 0; earlier < place; ++earlier)
        {
            if (order->officers[earlier] == officer_id) return sim_refuse(check, SIM_REFUSAL_DUPLICATE_OFFICER);
        }
        politics += officer->politics;
    }
    if (named == 0) return sim_refuse(check, SIM_REFUSAL_NO_OFFICER);

    sim_rules *rules = &sim->content.rules;
    if (sim->turns.action_points[city->owner_force_id] < rules->ap_develop) return sim_refuse(check, SIM_REFUSAL_NO_ACTION_POINTS);
    if (city->gold < kind->cost) return sim_refuse(check, SIM_REFUSAL_NO_GOLD);

    check.refusal       = SIM_REFUSAL_NONE;
    check.gold          = kind->cost;
    check.action_points = rules->ap_develop;
    check.days          = sim_develop_days(politics, kind->min_days, rules);
    check.turns         = check.days / CALENDAR_DAYS_PER_TURN;
    check.slot          = (u8)slot;
    return check;
}

/* Starts the job: pays the gold and the action points when the command
   starts (city-commands.md, Edge cases), and keeps the officers busy for
   the whole period. A refused order changes nothing. */
internal void
sim_develop(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_develop_order *order = &command.develop;
    sim_check check = sim_check_develop(sim, order);
    if (check.refusal != SIM_REFUSAL_NONE) return;

    city_component *city = sim_find_city(sim, order->city_id);
    entity_id city_entity = sim_city_entity(sim, order->city_id);
    u8 force = city->owner_force_id;
    city->gold -= check.gold;
    sim->turns.action_points[force] -= check.action_points;

    sim_slot *slot = sim->sites[order->city_id].slots + check.slot;
    slot->state      = SIM_SLOT_BUILDING;
    slot->facility   = order->facility;
    slot->level      = 1;
    slot->turns_left = (u16)check.turns;
    for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place)
    {
        slot->officers[place] = order->officers[place];
        officer_component *officer = sim_find_officer(sim, order->officers[place]);
        if (!officer) continue;
        officer->acted_turn = sim->turn + 1;
        officer->job_city   = order->city_id;
        officer->job_slot   = check.slot;
    }

    sim_event started = sim_event_make(sim, SIM_EVENT_DEVELOP_STARTED, command.id);
    started.subject = city_entity;
    started.faction = force;
    started.delta   = order->facility;
    sim_emit(events, started);
    sim_event gold = sim_event_make(sim, SIM_EVENT_GOLD_SPENT, command.id);
    gold.subject = city_entity;
    gold.faction = force;
    gold.delta   = -(s32)check.gold;
    sim_emit(events, gold);
    sim_event points = sim_event_make(sim, SIM_EVENT_ACTION_POINTS_SPENT, command.id);
    points.faction = force;
    points.delta   = -(s32)check.action_points;
    sim_emit(events, points);
}

/* At the start of each turn each job advances one turn. A job at 0 turns is
   complete: the facility works and its officers are free. */
internal void
sim_progress_jobs(sim_state *sim, u32 cause, sim_event_sink *events)
{
    for (u32 city_id = 0; city_id < SAVE_CITY_COUNT; ++city_id)
    {
        city_component *city = sim_find_city(sim, (u8)city_id);
        sim_city_site *site = sim->sites + city_id;
        for (u32 slot_index = 0; slot_index < site->slot_count; ++slot_index)
        {
            sim_slot *slot = site->slots + slot_index;
            if (slot->state != SIM_SLOT_BUILDING) continue;
            if (slot->turns_left > 0) slot->turns_left--;
            if (slot->turns_left > 0) continue;
            slot->state = SIM_SLOT_BUILT;
            sim_event completed = sim_event_make(sim, SIM_EVENT_FACILITY_COMPLETED, cause);
            completed.subject = city ? sim_city_entity(sim, (u8)city_id) : 0;
            completed.faction = city ? city->owner_force_id : SIM_NO_FORCE;
            completed.delta   = slot->facility;
            sim_emit(events, completed);
            for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place)
            {
                officer_component *officer = sim_find_officer(sim, slot->officers[place]);
                if (officer)
                {
                    officer->job_city = SIM_NO_CITY;
                    sim_event released = sim_event_make(sim, SIM_EVENT_OFFICER_RELEASED, cause);
                    released.subject = sim_officer_entity(sim, slot->officers[place]);
                    released.faction = officer->force_id;
                    sim_emit(events, released);
                }
                slot->officers[place] = SIM_NO_OFFICER;
            }
        }
    }
}

/* True if a built facility of the effect stands next to the slot. */
internal b32
sim_slot_has_neighbour(sim_state *sim, sim_city_site *site, u32 slot_index, facility_effect effect)
{
    for (u32 other = 0; other < site->slot_count; ++other)
    {
        sim_slot *neighbour = site->slots + other;
        if (other == slot_index || neighbour->state != SIM_SLOT_BUILT || neighbour->facility >= sim->content.facility_count) continue;
        if (sim->content.facilities[neighbour->facility].effect == effect &&
            hex_distance(neighbour->hex, site->slots[slot_index].hex) == 1) return true;
    }
    return false;
}

/* The gold each month or the grain each season of one built facility. A
   market next to a built mint, or a farm next to a built granary, gives
   adjacency.percent of its value (1.5 times; documented). One mint or
   several give the same bonus (assumed, Q-148). */
internal u32
sim_slot_yield(sim_state *sim, u8 city_id, u32 slot_index, facility_effect *effect)
{
    *effect = FACILITY_EFFECT_NONE;
    if (city_id >= SAVE_CITY_COUNT) return 0;
    sim_city_site *site = sim->sites + city_id;
    if (slot_index >= site->slot_count) return 0;
    sim_slot *slot = site->slots + slot_index;
    if (slot->state != SIM_SLOT_BUILT || slot->facility >= sim->content.facility_count) return 0;
    facility_kind *kind = sim->content.facilities + slot->facility;
    u32 value = 0;
    switch (kind->effect)
    {
        case FACILITY_EFFECT_GOLD:
            *effect = FACILITY_EFFECT_GOLD;
            value = kind->value;
            if (kind->adjacency_bonus && sim_slot_has_neighbour(sim, site, slot_index, FACILITY_EFFECT_MINT))
            {
                value = value * sim->content.rules.adjacency_percent / 100;
            }
            break;
        case FACILITY_EFFECT_GRAIN:
            *effect = FACILITY_EFFECT_GRAIN;
            value = kind->value;
            if (kind->adjacency_bonus && sim_slot_has_neighbour(sim, site, slot_index, FACILITY_EFFECT_GRANARY))
            {
                value = value * sim->content.rules.adjacency_percent / 100;
            }
            break;
        case FACILITY_EFFECT_GRAIN_BY_SOLDIERS:
        {
            *effect = FACILITY_EFFECT_GRAIN;
            city_component *city = sim_find_city(sim, city_id);
            value = city && kind->value ? city->troops / kind->value : 0;
            break;
        }
        default:
            break;
    }
    return value;
}

internal u32
sim_city_income(sim_state *sim, u8 city_id, facility_effect wanted, u32 base)
{
    if (city_id >= SAVE_CITY_COUNT) return 0;
    u32 total = base;
    for (u32 slot = 0; slot < sim->sites[city_id].slot_count; ++slot)
    {
        facility_effect effect = FACILITY_EFFECT_NONE;
        u32 value = sim_slot_yield(sim, city_id, slot, &effect);
        if (effect == wanted) total += value;
    }
    return total;
}

/* Gold income = the initial income of the city plus the facility values
   (guide book p.24). The initial income is assumed (Q-021). */
internal u32
sim_city_gold_income(sim_state *sim, u8 city_id)
{
    return sim_city_income(sim, city_id, FACILITY_EFFECT_GOLD, sim->content.rules.city_gold_income);
}

internal u32
sim_city_grain_income(sim_state *sim, u8 city_id)
{
    return sim_city_income(sim, city_id, FACILITY_EFFECT_GRAIN, sim->content.rules.city_grain_income);
}

internal u32
sim_city_used_slots(sim_state *sim, u8 city_id)
{
    if (city_id >= SAVE_CITY_COUNT) return 0;
    u32 used = 0;
    for (u32 slot = 0; slot < sim->sites[city_id].slot_count; ++slot)
    {
        if (sim->sites[city_id].slots[slot].state != SIM_SLOT_EMPTY) used++;
    }
    return used;
}
