/* The production system of M3 (cities-and-facilities.md, calendar-and-turn.md,
   skills.md): the gold and the grain of the calendar events, the salaries,
   the grain that the soldiers eat, the caps, the free weapons of a
   specialty city, the durability of the facilities, and the technique
   points and the merit of a command. Included by sim.cpp.

   Each change of the gold or the grain of a city is one ledger event, so
   the sum of the events of a city is the change of its stock. A stock above
   its cap loses the rest with an "over cap" event. The amounts come from
   rules.tsv; most are assumed (DECISIONS.md), and each event says so. */

/* ---- the resource of each event ----------------------------------------- */

internal sim_resource
sim_event_resource(sim_event_kind kind)
{
    switch (kind)
    {
        case SIM_EVENT_GOLD_INCOME:
        case SIM_EVENT_SALARIES_PAID:
        case SIM_EVENT_GOLD_SPENT:
        case SIM_EVENT_GOLD_OVER_CAP:
        case SIM_EVENT_GOLD_EARNED:           return SIM_RESOURCE_GOLD;
        case SIM_EVENT_GRAIN_INCOME:
        case SIM_EVENT_GRAIN_CONSUMED:
        case SIM_EVENT_GRAIN_OVER_CAP:
        case SIM_EVENT_GRAIN_BOUGHT:
        case SIM_EVENT_GRAIN_SOLD:            return SIM_RESOURCE_GRAIN;
        case SIM_EVENT_SOLDIERS_DESERTED:
        case SIM_EVENT_CONSCRIPTED:           return SIM_RESOURCE_SOLDIERS;
        case SIM_EVENT_TRAINED:
        case SIM_EVENT_MORALE_LOST:           return SIM_RESOURCE_MORALE;
        case SIM_EVENT_PATROLLED:
        case SIM_EVENT_ORDER_LOST:            return SIM_RESOURCE_PUBLIC_ORDER;
        case SIM_EVENT_WEAPONS_FREE:
        case SIM_EVENT_WEAPONS_PRODUCED:      return SIM_RESOURCE_WEAPONS;
        case SIM_EVENT_FACILITY_REPAIRED:
        case SIM_EVENT_FACILITY_DAMAGED:      return SIM_RESOURCE_DURABILITY;
        case SIM_EVENT_TECHNIQUE_GAINED:      return SIM_RESOURCE_TECHNIQUE;
        case SIM_EVENT_ACTION_POINTS_RECOVERED:
        case SIM_EVENT_ACTION_POINTS_SPENT:   return SIM_RESOURCE_ACTION_POINTS;
        case SIM_EVENT_MERIT_GAINED:          return SIM_RESOURCE_MERIT;
        default:                              return SIM_RESOURCE_NONE;
    }
}

global_variable const char *sim_resource_names[SIM_RESOURCE_COUNT] = {
    "none", "gold", "grain", "soldiers", "morale", "public_order", "weapons", "durability", "technique",
    "action_points", "merit",
};

internal const char *
sim_resource_name(sim_resource resource)
{
    return resource < SIM_RESOURCE_COUNT ? sim_resource_names[resource] : "?";
}

/* ---- the arithmetic of the production ----------------------------------- */

internal u32
sim_percent_of(u32 amount, u32 percent)
{
    return (u32)(((u64)amount * percent) / 100);
}

/* Merge levels (manual p.28): level 2 gives 1.2 times, level 3 gives 1.5
   times the output of level 1. A level below 1 counts as 1, above 3 as 3. */
internal u32
sim_level_percent(u32 level, sim_rules *rules)
{
    u32 percent = 100;
    if (level == 2)     percent = rules->level2_percent;
    else if (level > 2) percent = rules->level3_percent;
    return percent ? percent : 100;
}

/* The output of one facility: its value, times its level, times 1.5 next
   to a mint or a granary. "A level-3 market next to a mint gives more
   again" (manual p.28): the two multiply. */
internal u32
sim_facility_output(u32 value, u32 level, b32 adjacent_bonus, sim_rules *rules)
{
    u32 adjacency = adjacent_bonus && rules->adjacency_percent ? rules->adjacency_percent : 100;
    return (u32)((u64)value * sim_level_percent(level, rules) * adjacency / 10000);
}

/* ASSUMED RULE (Q-151). "High order raises gold and grain income"
   (cities-and-facilities.md). The line goes from order.income_floor percent
   at public order 0 to 100 percent at order.max. Without the rule, the
   income does not change. */
internal u32
sim_order_percent(u32 public_order, sim_rules *rules)
{
    if (!rules->order_max) return 100;
    u32 order = public_order < rules->order_max ? public_order : rules->order_max;
    u32 floor = rules->order_income_floor < 100 ? rules->order_income_floor : 100;
    return floor + (100 - floor) * order / rules->order_max;
}

/* True if an active officer of the owner with the skill is in the city.
   "Income and disaster skills apply to the city where the officer is"
   (skills.md). The save number of a skill is assumed (Q-159); a rule of 0
   names no skill. */
internal b32
sim_city_has_skill(sim_state *sim, u8 city_id, u32 skill)
{
    city_component *city = sim_find_city(sim, city_id);
    if (!city || city->owner_force_id == SIM_NO_FORCE || skill == 0 || skill >= 255) return false;
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (sim_officer_is_active(officer) && officer->force_id == city->owner_force_id &&
            officer->current_position == city_id && officer->skill_number == skill) return true;
    }
    return false;
}

/* The salaries of a city each month: the active officers of the owner
   whose base is the city, times officer.salary (assumed, Q-027). */
internal u32
sim_city_salaries(sim_state *sim, city_component *city)
{
    u32 officers = 0;
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (sim_officer_is_active(officer) && officer->force_id == city->owner_force_id && officer->base_location == city->city_id) officers++;
    }
    return officers * sim->content.rules.officer_salary;
}

/* 100,000 soldiers; 150,000 for a big city (cities-and-facilities.md). */
internal u32
sim_city_soldier_cap(sim_state *sim, u8 city_id)
{
    sim_rules *rules = &sim->content.rules;
    b32 big = city_id < sim->content.city_count && sim->content.cities[city_id].specialty == CITY_SPECIALTY_BIG_CITY;
    return big ? rules->city_big_soldier_cap : rules->city_soldier_cap;
}

/* The income of a city at its next calendar events:
   - gold each month = (initial income + facilities) x order percent x 富豪
   - grain each season = (initial income + farms) x order percent x 米道
   - 徵稅: the gold comes each turn, each payment halved (skills.md)
   - 徵收: the grain comes each month, each payment halved (skills.md) */
internal sim_income_forecast
sim_city_forecast(sim_state *sim, u8 city_id)
{
    sim_income_forecast forecast = {};
    city_component *city = sim_find_city(sim, city_id);
    if (!city) return forecast;
    sim_rules *rules = &sim->content.rules;
    u32 skill_percent = rules->skill_income_percent ? rules->skill_income_percent : 100;

    forecast.gold_base           = rules->city_gold_income;
    forecast.gold_facilities     = sim_city_income(sim, city_id, FACILITY_EFFECT_GOLD, 0);
    forecast.order_percent       = sim_order_percent(city->public_order, rules);
    forecast.gold_skill_percent  = sim_city_has_skill(sim, city_id, rules->skill_rich) ? skill_percent : 100;
    forecast.gold_every_turn     = sim_city_has_skill(sim, city_id, rules->skill_tax);
    u32 gold = sim_percent_of(sim_percent_of(forecast.gold_base + forecast.gold_facilities, forecast.order_percent),
                              forecast.gold_skill_percent);
    forecast.gold_payment = forecast.gold_every_turn ? gold / 2 : gold;
    forecast.gold_month   = forecast.gold_every_turn ? forecast.gold_payment * CALENDAR_TURNS_PER_MONTH : gold;

    forecast.grain_base          = rules->city_grain_income;
    forecast.grain_facilities    = sim_city_income(sim, city_id, FACILITY_EFFECT_GRAIN, 0);
    forecast.grain_skill_percent = sim_city_has_skill(sim, city_id, rules->skill_rice) ? skill_percent : 100;
    forecast.grain_every_month   = sim_city_has_skill(sim, city_id, rules->skill_levy);
    u32 grain = sim_percent_of(sim_percent_of(forecast.grain_base + forecast.grain_facilities, forecast.order_percent),
                               forecast.grain_skill_percent);
    forecast.grain_payment = forecast.grain_every_month ? grain / 2 : grain;
    forecast.grain_season  = forecast.grain_every_month ? forecast.grain_payment * CALENDAR_MONTHS_PER_SEASON : grain;

    forecast.salaries       = sim_city_salaries(sim, city);
    forecast.grain_per_turn = rules->grain_soldiers ? city->troops / rules->grain_soldiers : 0;
    return forecast;
}

/* ---- the ledger of a city ----------------------------------------------- */

internal void
sim_emit_city(sim_state *sim, sim_event_kind kind, city_component *city, s32 delta, u32 detail, b32 assumed,
              u32 cause, sim_event_sink *events)
{
    sim_event event = sim_event_make(sim, kind, cause);
    event.subject = sim_city_entity(sim, city->city_id);
    event.faction = city->owner_force_id;
    event.delta   = delta;
    event.detail  = detail;
    event.assumed = assumed;
    sim_emit(events, event);
}

/* Adds an amount to a stock of the city, then takes back what is above the
   cap. Both steps are events. */
internal void
sim_city_receive(sim_state *sim, city_component *city, u32 *stock, u32 cap, u32 amount, sim_event_kind kind,
                 sim_event_kind over_kind, b32 assumed, u32 cause, sim_event_sink *events)
{
    u64 total = (u64)*stock + amount;
    sim_emit_city(sim, kind, city, (s32)amount, 0, assumed, cause, events);
    if (cap && total > cap)
    {
        sim_emit_city(sim, over_kind, city, -(s32)(total - cap), 0, false, cause, events);
        total = cap;
    }
    *stock = (u32)total;
}

/* The command gives merit to each officer (save-file.md: +50 for each
   officer that does an order) and, if the command raises them, technique
   points to the faction (calendar-and-turn.md; the amount is assumed,
   Q-158). */
internal void
sim_reward_command(sim_state *sim, u8 force, const u16 *officers, b32 technique, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
    {
        officer_component *officer = sim_find_officer(sim, officers[place]);
        if (!officer || !rules->merit_per_command) continue;
        u32 merit = officer->merit + rules->merit_per_command;
        u32 gained = (merit < 0xFFFF ? merit : 0xFFFF) - officer->merit;
        officer->merit = (u16)(officer->merit + gained);
        sim_event event = sim_event_make(sim, SIM_EVENT_MERIT_GAINED, cause);
        event.subject = sim_officer_entity(sim, officers[place]);
        event.faction = force;
        event.delta   = (s32)gained;
        sim_emit(events, event);
    }
    if (technique && rules->technique_per_command && force != SIM_NO_FORCE)
    {
        sim->technique_points[force] += rules->technique_per_command;
        sim_event event = sim_event_make(sim, SIM_EVENT_TECHNIQUE_GAINED, cause);
        event.faction = force;
        event.delta   = (s32)rules->technique_per_command;
        event.assumed = true;
        sim_emit(events, event);
    }
}

/* ---- the calendar events ------------------------------------------------ */

/* True if a built facility with this effect stands in the city. */
internal b32
sim_city_has_built(sim_state *sim, u8 city_id, facility_effect effect)
{
    sim_city_site *site = sim->sites + city_id;
    for (u32 slot = 0; slot < site->slot_count; ++slot)
    {
        sim_slot *candidate = site->slots + slot;
        if (candidate->state == SIM_SLOT_BUILT && candidate->facility < sim->content.facility_count &&
            sim->content.facilities[candidate->facility].effect == effect) return true;
    }
    return false;
}

/* ASSUMED AMOUNT (Q-022). A specialty city with the matching production
   facility gains some of its weapon every month, but not siege weapons or
   ships (manual p.8, p.33). */
internal void
sim_give_free_weapons(sim_state *sim, city_component *city, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    if (city->city_id >= sim->content.city_count || !rules->produce_free_monthly) return;
    city_specialty specialty = sim->content.cities[city->city_id].specialty;
    for (u32 index = 0; index < sim->content.weapon_count; ++index)
    {
        weapon_kind *weapon = sim->content.weapons + index;
        if (weapon->max_days || weapon->needs_technique || weapon->specialty != specialty) continue;
        if (!sim_city_has_built(sim, city->city_id, weapon->facility)) continue;
        u32 *stock = city->equipment_stocks + weapon->stock;
        u32 room = *stock < rules->city_weapon_cap ? rules->city_weapon_cap - *stock : 0;
        u32 added = rules->produce_free_monthly < room ? rules->produce_free_monthly : room;
        *stock += added;
        sim_emit_city(sim, SIM_EVENT_WEAPONS_FREE, city, (s32)added, weapon->id, true, cause, events);
    }
}

/* Day 1 of each month (guide book p.4): the gold income, the salaries, the
   grain of 徵收, and the free weapons. */
internal void
sim_run_production_month(sim_state *sim, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        city_component *city = sim->cities.data + index;
        if (city->owner_force_id == SIM_NO_FORCE) continue;
        sim_income_forecast forecast = sim_city_forecast(sim, city->city_id);
        if (!forecast.gold_every_turn)
        {
            sim_city_receive(sim, city, &city->gold, rules->city_gold_cap, forecast.gold_payment, SIM_EVENT_GOLD_INCOME,
                             SIM_EVENT_GOLD_OVER_CAP, true, cause, events);
        }
        if (forecast.salaries)
        {
            u32 paid = forecast.salaries < city->gold ? forecast.salaries : city->gold;
            city->gold -= paid;
            sim_emit_city(sim, SIM_EVENT_SALARIES_PAID, city, -(s32)paid, 0, true, cause, events);
            if (paid < forecast.salaries)
            {
                sim_emit_city(sim, SIM_EVENT_SALARIES_UNPAID, city, 0, forecast.salaries - paid, true, cause, events);
            }
        }
        if (forecast.grain_every_month)
        {
            sim_city_receive(sim, city, &city->food, rules->city_grain_cap, forecast.grain_payment, SIM_EVENT_GRAIN_INCOME,
                             SIM_EVENT_GRAIN_OVER_CAP, true, cause, events);
        }
        sim_give_free_weapons(sim, city, cause, events);
    }
}

/* Day 1 of months 1, 4, 7 and 10 (guide book p.4): the grain income. */
internal void
sim_run_production_season(sim_state *sim, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        city_component *city = sim->cities.data + index;
        if (city->owner_force_id == SIM_NO_FORCE) continue;
        sim_income_forecast forecast = sim_city_forecast(sim, city->city_id);
        if (forecast.grain_every_month) continue;
        sim_city_receive(sim, city, &city->food, rules->city_grain_cap, forecast.grain_payment, SIM_EVENT_GRAIN_INCOME,
                         SIM_EVENT_GRAIN_OVER_CAP, true, cause, events);
    }
}

/* Each turn: the gold of 徵稅, then the grain that the soldiers eat (guide
   book p.4: in proportion to the soldiers; one grain for each
   grain.soldiers soldiers, assumed, Q-119). If the grain is not
   sufficient, the soldiers that the missing grain feeds leave (manual p.14;
   the number is assumed, Q-160). The units of M5 eat too. */
internal void
sim_run_production_turn(sim_state *sim, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        city_component *city = sim->cities.data + index;
        if (city->owner_force_id == SIM_NO_FORCE) continue;
        sim_income_forecast forecast = sim_city_forecast(sim, city->city_id);
        if (forecast.gold_every_turn)
        {
            sim_city_receive(sim, city, &city->gold, rules->city_gold_cap, forecast.gold_payment, SIM_EVENT_GOLD_INCOME,
                             SIM_EVENT_GOLD_OVER_CAP, true, cause, events);
        }
        u32 needed = forecast.grain_per_turn;
        u32 eaten  = needed < city->food ? needed : city->food;
        city->food -= eaten;
        if (eaten) sim_emit_city(sim, SIM_EVENT_GRAIN_CONSUMED, city, -(s32)eaten, 0, true, cause, events);
        if (eaten < needed)
        {
            u64 hungry = (u64)(needed - eaten) * rules->grain_soldiers;
            u32 lost = hungry < city->troops ? (u32)hungry : city->troops;
            city->troops -= lost;
            if (lost) sim_emit_city(sim, SIM_EVENT_SOLDIERS_DESERTED, city, -(s32)lost, 0, true, cause, events);
        }
    }
}

/* ---- durability (cities-and-facilities.md, "Durability and damage") ---- */

internal u32
sim_slot_durability_max(sim_state *sim, sim_slot *slot)
{
    return slot->facility < sim->content.facility_count ? sim->content.facilities[slot->facility].durability : 0;
}

/* Each turn a facility recovers durability when the city has a governor
   and no enemy is near (manual p.11). The amount is assumed (Q-157). */
internal void
sim_run_durability(sim_state *sim, u32 cause, sim_event_sink *events)
{
    u32 recovery = sim->content.rules.facility_recovery;
    if (!recovery) return;
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        city_component *city = sim->cities.data + index;
        if (city->owner_force_id == SIM_NO_FORCE || sim_city_governor(sim, city->city_id) == SIM_NO_OFFICER ||
            sim_enemy_near(sim, city)) continue;
        sim_city_site *site = sim->sites + city->city_id;
        for (u32 slot_index = 0; slot_index < site->slot_count; ++slot_index)
        {
            sim_slot *slot = site->slots + slot_index;
            u32 maximum = sim_slot_durability_max(sim, slot);
            if (slot->state == SIM_SLOT_EMPTY || slot->durability >= maximum) continue;
            u32 added = maximum - slot->durability < recovery ? maximum - slot->durability : recovery;
            slot->durability += added;
            sim_emit_city(sim, SIM_EVENT_FACILITY_REPAIRED, city, (s32)added, slot_index, true, cause, events);
        }
    }
}

/* Why the test command cannot damage the slot, or SIM_REFUSAL_NONE. */
internal sim_refusal
sim_check_damage(sim_state *sim, sim_damage_order *order)
{
    if (!sim->has_content) return SIM_REFUSAL_NO_CONTENT;
    if (!sim_find_city(sim, order->city_id)) return SIM_REFUSAL_NO_CITY;
    sim_city_site *site = sim->sites + order->city_id;
    if (order->slot >= site->slot_count || site->slots[order->slot].state == SIM_SLOT_EMPTY) return SIM_REFUSAL_NO_FACILITY;
    return SIM_REFUSAL_NONE;
}

/* The test command until an attacker exists (M6): it takes durability from
   a facility. At 0 the facility is destroyed and must be built again
   (manual p.11); its officers are free. The loot of the attacker and the
   soldiers of a barracks come with the attacker (F-031, M6). */
internal void
sim_damage_facility(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_damage_order *order = &command.damage;
    if (sim_check_damage(sim, order) != SIM_REFUSAL_NONE) return;
    city_component *city = sim_find_city(sim, order->city_id);
    sim_slot *slot = sim->sites[order->city_id].slots + order->slot;
    u32 damage = order->amount < slot->durability ? order->amount : slot->durability;
    slot->durability -= damage;
    sim_emit_city(sim, SIM_EVENT_FACILITY_DAMAGED, city, -(s32)damage, order->slot, false, command.id, events);
    if (slot->durability > 0) return;

    u8 facility = slot->facility;
    sim_release_slot_officers(sim, slot, command.id, events);
    slot->state      = SIM_SLOT_EMPTY;
    slot->facility   = CONTENT_NO_FACILITY;
    slot->level      = 0;
    slot->turns_left = 0;
    slot->job_weapon = CONTENT_NO_WEAPON;
    slot->used_turn  = 0;
    sim_emit_city(sim, SIM_EVENT_FACILITY_DESTROYED, city, facility, order->slot, false, command.id, events);
}
