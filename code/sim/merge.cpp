/* The merge of facilities (吸收合併, Power Up Kit; cities-and-facilities.md,
   "Merge"; city-commands.md). Included by sim.cpp.

   Two adjacent facilities of the same kind merge into one. The absorber
   keeps the facility and gains the level of the absorbed side; the absorbed
   side must be level 1, and the maximum level is 3. During the merge the
   absorber still produces at its old level and the absorbed side gives
   nothing (state SIM_SLOT_MERGING). When the merge completes, the absorbed
   slot is free. The command takes one officer, 10 action points and 100
   gold; the politics of the officer shortens the period (an assumed line,
   Q-163). */

#define SIM_MAX_FACILITY_LEVEL 3

/* ASSUMED LINE (Q-163): from merge.max_days at politics 0 to merge.min_days
   at merge.full_politics, up to whole turns. */
internal u32
sim_merge_days(u32 politics, sim_rules *rules)
{
    u32 days = rules->merge_max_days;
    if (rules->merge_full_politics && rules->merge_max_days > rules->merge_min_days)
    {
        u32 sum = politics < rules->merge_full_politics ? politics : rules->merge_full_politics;
        days = rules->merge_max_days - (rules->merge_max_days - rules->merge_min_days) * sum / rules->merge_full_politics;
    }
    return sim_round_to_turns(days);
}

/* True if the two slots are two adjacent built facilities of one kind that
   can merge: the absorbed side is level 1 and the sum of the levels is 3 or
   less. */
internal b32
sim_slots_can_merge(sim_state *sim, sim_city_site *site, u32 absorber_index, u32 absorbed_index)
{
    if (absorber_index >= site->slot_count || absorbed_index >= site->slot_count || absorber_index == absorbed_index) return false;
    sim_slot *absorber = site->slots + absorber_index;
    sim_slot *absorbed = site->slots + absorbed_index;
    if (absorber->state != SIM_SLOT_BUILT || absorbed->state != SIM_SLOT_BUILT) return false;
    if (absorber->facility != absorbed->facility || absorber->facility >= sim->content.facility_count) return false;
    if (!sim->content.facilities[absorber->facility].mergeable) return false;
    if (absorber->merge_slot != SIM_ANY_SLOT || absorbed->merge_slot != SIM_ANY_SLOT) return false;
    if (hex_distance(absorber->hex, absorbed->hex) != 1) return false;
    return absorbed->level == 1 && absorber->level + absorbed->level <= SIM_MAX_FACILITY_LEVEL;
}

/* The checks of the merge command, in order. The first reason that holds
   is the answer. */
internal sim_check
sim_check_merge(sim_state *sim, sim_merge_order *order)
{
    sim_check check = {};
    check.slot    = SIM_ANY_SLOT;
    check.assumed = true;
    if (!sim->has_content)                              return sim_refuse(check, SIM_REFUSAL_NO_CONTENT);
    if (sim_game_over(sim))                             return sim_refuse(check, SIM_REFUSAL_GAME_OVER);
    city_component *city = sim_find_city(sim, order->city_id);
    if (!city || city->owner_force_id == SIM_NO_FORCE) return sim_refuse(check, SIM_REFUSAL_NO_CITY);
    u8 force = city->owner_force_id;
    if (sim_whose_turn(sim) != force)                   return sim_refuse(check, SIM_REFUSAL_NOT_YOUR_TURN);
    if (sim_enemy_near(sim, city))                      return sim_refuse(check, SIM_REFUSAL_ENEMY_NEAR);
    sim_city_site *site = sim->sites + city->city_id;
    if (!sim_slots_can_merge(sim, site, order->absorber, order->absorbed)) return sim_refuse(check, SIM_REFUSAL_NOT_MERGEABLE);

    u16 officers[SIM_COMMAND_OFFICERS] = { order->officer, SIM_NO_OFFICER, SIM_NO_OFFICER };
    u32 politics = 0;
    sim_refusal refusal = sim_check_city_officers(sim, city, officers, 1, SIM_ABILITY_POLITICS, &politics);
    if (refusal != SIM_REFUSAL_NONE)                    return sim_refuse(check, refusal);
    sim_rules *rules = &sim->content.rules;
    if (sim->turns.action_points[force] < rules->ap_merge) return sim_refuse(check, SIM_REFUSAL_NO_ACTION_POINTS);
    if (city->gold < rules->merge_gold)                 return sim_refuse(check, SIM_REFUSAL_NO_GOLD);

    sim_slot *absorber = site->slots + order->absorber;
    check.gold          = rules->merge_gold;
    check.action_points = rules->ap_merge;
    check.days          = sim_merge_days(politics, rules);
    check.turns         = check.days / CALENDAR_DAYS_PER_TURN;
    check.slot          = order->absorber;
    check.value_before  = absorber->level;
    check.value_after   = absorber->level + site->slots[order->absorbed].level;
    return check;
}

/* Starts the merge: pays, keeps the officer busy, and marks the absorbed
   side. */
internal void
sim_merge(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_merge_order *order = &command.merge;
    sim_check check = sim_check_merge(sim, order);
    if (check.refusal != SIM_REFUSAL_NONE) return;
    city_component *city = sim_find_city(sim, order->city_id);
    u8 force = city->owner_force_id;
    sim_city_site *site = sim->sites + city->city_id;
    sim_slot *absorber = site->slots + order->absorber;
    sim_slot *absorbed = site->slots + order->absorbed;

    city->gold -= check.gold;
    sim_emit_city(sim, SIM_EVENT_GOLD_SPENT, city, -(s32)check.gold, 0, false, command.id, events);
    sim->turns.action_points[force] -= check.action_points;
    sim_event points = sim_event_make(sim, SIM_EVENT_ACTION_POINTS_SPENT, command.id);
    points.faction = force;
    points.delta   = -(s32)check.action_points;
    sim_emit(events, points);

    absorber->merge_slot = order->absorbed;
    absorber->turns_left = (u16)check.turns;
    absorber->officers[0] = order->officer;
    absorber->officers[1] = SIM_NO_OFFICER;
    absorber->officers[2] = SIM_NO_OFFICER;
    absorbed->state = SIM_SLOT_MERGING;
    officer_component *officer = sim_find_officer(sim, order->officer);
    if (officer)
    {
        officer->acted_turn = sim->turn + 1;
        officer->job_city   = order->city_id;
        officer->job_slot   = order->absorber;
    }
    sim_emit_city(sim, SIM_EVENT_MERGE_STARTED, city, (s32)check.turns, order->absorber, true, command.id, events);
    u16 officers[SIM_COMMAND_OFFICERS] = { order->officer, SIM_NO_OFFICER, SIM_NO_OFFICER };
    /* The merge gives merit, and technique points as the develop command does (assumed, Q-163). */
    sim_reward_command(sim, force, officers, true, command.id, events);
}

/* The merge of the absorber is complete: it gains the level and the
   absorbed slot is free. If an attack destroyed the absorbed side in the
   meantime, the merge ends without a level. */
internal void
sim_complete_merge(sim_state *sim, city_component *city, sim_slot *slot, u32 slot_index, u32 cause, sim_event_sink *events)
{
    sim_city_site *site = sim->sites + (city ? city->city_id : 0);
    u8 absorbed_index = slot->merge_slot;
    slot->merge_slot = SIM_ANY_SLOT;
    if (city && absorbed_index < site->slot_count && site->slots[absorbed_index].state == SIM_SLOT_MERGING)
    {
        sim_slot *absorbed = site->slots + absorbed_index;
        u32 level = slot->level + absorbed->level;
        slot->level = (u8)(level < SIM_MAX_FACILITY_LEVEL ? level : SIM_MAX_FACILITY_LEVEL);
        *absorbed = {};
        absorbed->hex        = sim_site_slot_hex(sim, city->city_id, absorbed_index);
        absorbed->state      = SIM_SLOT_EMPTY;
        absorbed->facility   = CONTENT_NO_FACILITY;
        absorbed->job_weapon = CONTENT_NO_WEAPON;
        absorbed->merge_slot = SIM_ANY_SLOT;
        for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place) absorbed->officers[place] = SIM_NO_OFFICER;
        sim_emit_city(sim, SIM_EVENT_FACILITY_MERGED, city, slot->level, slot_index, false, cause, events);
    }
    sim_release_slot_officers(sim, slot, cause, events);
}
