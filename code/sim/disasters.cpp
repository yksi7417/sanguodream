/* The disasters of the calendar (calendar-and-turn.md, "Disasters";
   skills.md, "Disasters"): locusts, plague, bandits and the good harvest.
   Included by sim.cpp.

   disasters.tsv gives each disaster as data: the months when it can start,
   the chance, the length and the effects. The spec gives the months and the
   kind of each effect; every number is assumed (Q-162), and each event of a
   disaster has the assumed mark. On day 1 of a month the calendar rolls the
   disasters of that month for each city that has an owner. Each turn a
   running disaster acts and counts down. The game setting "disasters"
   turns the rolls off; the editor command starts a disaster in any case. */

internal b32
sim_disaster_month(disaster_kind *kind, u32 month)
{
    return month >= 1 && month <= 12 && ((kind->month_mask >> month) & 1);
}

/* The chance in percent: skill_chance_percent of it when an officer with
   the skill of the row is in the city (祈願 doubles the harvest, 威壓
   halves the bandits). */
internal u32
sim_disaster_chance(disaster_kind *kind, b32 has_chance_skill)
{
    if (!has_chance_skill || !kind->skill_chance) return kind->chance_percent;
    u32 chance = sim_percent_of(kind->chance_percent, kind->skill_chance_percent);
    return chance < 100 ? chance : 100;
}

internal b32
sim_disaster_active(city_component *city, u32 disaster)
{
    return disaster < CONTENT_DISASTER_CAPACITY && city->disaster_turns[disaster] != 0;
}

/* Takes public order from the city for a disaster: the event detail is 1 +
   the disaster (0 is a command). 整備政令 lowers the loss. */
internal void
sim_disaster_order_loss(sim_state *sim, city_component *city, u32 disaster, u32 loss, u32 cause, sim_event_sink *events)
{
    u32 wanted = sim_order_loss(sim, city->owner_force_id, loss);
    u32 lost = wanted < city->public_order ? wanted : city->public_order;
    city->public_order = (u8)(city->public_order - lost);
    if (lost) sim_emit_city(sim, SIM_EVENT_ORDER_LOST, city, -(s32)lost, 1 + disaster, true, cause, events);
}

/* The good harvest: grain_percent of the grain of the season, within the cap. */
internal void
sim_disaster_harvest(sim_state *sim, city_component *city, disaster_kind *kind, u32 cause, sim_event_sink *events)
{
    sim_income_forecast forecast = sim_city_forecast(sim, city->city_id);
    u32 bonus = sim_percent_of(forecast.grain_season, kind->grain_percent);
    if (!bonus) return;
    u32 cap = sim->content.rules.city_grain_cap;
    u64 total = (u64)city->food + bonus;
    sim_emit_city(sim, SIM_EVENT_GRAIN_HARVEST, city, (s32)bonus, kind->id, true, cause, events);
    if (cap && total > cap)
    {
        sim_emit_city(sim, SIM_EVENT_GRAIN_OVER_CAP, city, -(s32)(total - cap), 0, false, cause, events);
        total = cap;
    }
    city->food = (u32)total;
}

/* Why the editor command cannot start the disaster, or SIM_REFUSAL_NONE. */
internal sim_refusal
sim_check_start_disaster(sim_state *sim, sim_disaster_order *order)
{
    if (!sim->has_content) return SIM_REFUSAL_NO_CONTENT;
    city_component *city = sim_find_city(sim, order->city_id);
    if (!city || city->owner_force_id == SIM_NO_FORCE) return SIM_REFUSAL_NO_CITY;
    if (order->disaster >= sim->content.disaster_count || sim_disaster_active(city, order->disaster)) return SIM_REFUSAL_NO_DISASTER;
    return SIM_REFUSAL_NONE;
}

/* Starts a disaster in the city: the start event, the loss of public order,
   and the length. A disaster without a length that does not end by public
   order (the harvest) acts at once and does not run. */
internal void
sim_start_disaster(sim_state *sim, city_component *city, u32 disaster, u32 cause, sim_event_sink *events)
{
    disaster_kind *kind = sim->content.disasters + disaster;
    b32 runs = kind->turns > 0 || kind->ends_at_order;
    sim_emit_city(sim, SIM_EVENT_DISASTER_STARTED, city, (s32)kind->turns, disaster, true, cause, events);
    if (kind->start_order_loss) sim_disaster_order_loss(sim, city, disaster, kind->start_order_loss, cause, events);
    if (!runs)
    {
        if (kind->grain_percent) sim_disaster_harvest(sim, city, kind, cause, events);
        return;
    }
    city->disaster_turns[disaster] = kind->turns ? (u16)kind->turns : (u16)SIM_DISASTER_OPEN;
}

internal void
sim_start_disaster_command(sim_state *sim, sim_command command, sim_event_sink *events)
{
    if (sim_check_start_disaster(sim, &command.disaster) != SIM_REFUSAL_NONE) return;
    sim_start_disaster(sim, sim_find_city(sim, command.disaster.city_id), command.disaster.disaster, command.id, events);
}

/* Day 1 of a month: for each city that has an owner and each disaster of
   the month, one roll. A running disaster does not start again. Bandits
   start only below the bandit threshold of public order (80). A skill in
   the city can stop the disaster or change its chance. */
internal void
sim_roll_disasters(sim_state *sim, u32 month, u32 cause, sim_event_sink *events)
{
    if (!sim->settings.disasters) return;
    sim_rules *rules = &sim->content.rules;
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        city_component *city = sim->cities.data + index;
        if (city->owner_force_id == SIM_NO_FORCE) continue;
        for (u32 disaster = 0; disaster < sim->content.disaster_count; ++disaster)
        {
            disaster_kind *kind = sim->content.disasters + disaster;
            if (!sim_disaster_month(kind, month) || sim_disaster_active(city, disaster)) continue;
            if (kind->low_order_only && city->public_order >= rules->order_bandit_threshold) continue;
            if (kind->skill_block && sim_city_has_skill(sim, city->city_id, kind->skill_block)) continue;
            u32 chance = sim_disaster_chance(kind, kind->skill_chance && sim_city_has_skill(sim, city->city_id, kind->skill_chance));
            if (chance && sim_random_below(sim, 100) < chance) sim_start_disaster(sim, city, disaster, cause, events);
        }
    }
}

/* At a grain payment: each running disaster with grain_percent (locusts)
   takes that percent of the payment. */
internal void
sim_disaster_grain(sim_state *sim, city_component *city, u32 payment, u32 cause, sim_event_sink *events)
{
    for (u32 disaster = 0; disaster < sim->content.disaster_count; ++disaster)
    {
        disaster_kind *kind = sim->content.disasters + disaster;
        if (!sim_disaster_active(city, disaster) || !kind->grain_percent) continue;
        u32 wanted = sim_percent_of(payment, kind->grain_percent);
        u32 lost = wanted < city->food ? wanted : city->food;
        city->food -= lost;
        if (lost) sim_emit_city(sim, SIM_EVENT_GRAIN_LOST, city, -(s32)lost, disaster, true, cause, events);
    }
}

/* A random built facility of the target effect in the city, or SIM_ANY_SLOT. */
internal u32
sim_disaster_target_slot(sim_state *sim, city_component *city, u32 target)
{
    sim_city_site *site = sim->sites + city->city_id;
    u32 candidates[SIM_SLOT_CAPACITY];
    u32 count = 0;
    for (u32 slot_index = 0; slot_index < site->slot_count; ++slot_index)
    {
        sim_slot *slot = site->slots + slot_index;
        if (slot->state != SIM_SLOT_BUILT || slot->facility >= sim->content.facility_count) continue;
        if (target != CONTENT_TARGET_ANY && (u32)sim->content.facilities[slot->facility].effect != target) continue;
        candidates[count++] = slot_index;
    }
    return count ? candidates[sim_random_below(sim, count)] : SIM_ANY_SLOT;
}

/* One turn of the disasters that run in one city. */
internal void
sim_run_city_disasters(sim_state *sim, city_component *city, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    for (u32 disaster = 0; disaster < sim->content.disaster_count; ++disaster)
    {
        if (!sim_disaster_active(city, disaster)) continue;
        disaster_kind *kind = sim->content.disasters + disaster;
        b32 ended = kind->ends_at_order && city->public_order >= rules->order_bandit_threshold;
        if (!ended)
        {
            if (kind->turn_order_loss) sim_disaster_order_loss(sim, city, disaster, kind->turn_order_loss, cause, events);
            if (kind->soldier_percent && city->troops)
            {
                u32 lost = sim_percent_of(city->troops, kind->soldier_percent);
                if (!lost) lost = 1;
                city->troops -= lost;
                sim_emit_city(sim, SIM_EVENT_SOLDIERS_LOST, city, -(s32)lost, disaster, true, cause, events);
            }
            if (kind->facility_damage && kind->facility_target != FACILITY_EFFECT_NONE)
            {
                u32 slot = sim_disaster_target_slot(sim, city, kind->facility_target);
                if (slot != SIM_ANY_SLOT) sim_damage_slot(sim, city, slot, kind->facility_damage, cause, events);
            }
            u16 *turns = city->disaster_turns + disaster;
            if (*turns != SIM_DISASTER_OPEN) (*turns)--;
            ended = *turns == 0;
        }
        if (ended)
        {
            city->disaster_turns[disaster] = 0;
            sim_emit_city(sim, SIM_EVENT_DISASTER_ENDED, city, 0, disaster, true, cause, events);
        }
    }
}

/* Each turn, after the production and the durability: the running
   disasters act and count down. */
internal void
sim_run_disasters_turn(sim_state *sim, u32 cause, sim_event_sink *events)
{
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        city_component *city = sim->cities.data + index;
        if (city->owner_force_id == SIM_NO_FORCE) continue;
        sim_run_city_disasters(sim, city, cause, events);
    }
}
