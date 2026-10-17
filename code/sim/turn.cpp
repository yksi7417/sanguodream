/* The turn rules (calendar-and-turn.md): the calendar events, the order in
   which the factions act, the action points of each corps, and the rule
   that picks a leader (factions-corps-and-posts.md). Included by sim.cpp.

   The flow of one turn:
   1. sim_advance_turn moves the date and calls sim_run_calendar: the jobs
      progress, then the month, season and year events, then the grain
      that the cities eat.
   2. sim_start_turn computes the order, recovers the action points, and
      lets each AI faction pass until a human faction acts.
   3. A human faction gives its commands, then "proceed" (end_turn) calls
      sim_end_faction_turn. The next factions act. After the last one the
      date advances (step 1). */

internal officer_component *
sim_find_officer(sim_state *sim, u16 officer_id)
{
    if (officer_id >= ArrayCount(sim->officer_index) || sim->officer_index[officer_id] == SIM_NO_OFFICER) return 0;
    return sim->officers.data + sim->officer_index[officer_id];
}

internal city_component *
sim_find_city(sim_state *sim, u8 city_id)
{
    if (city_id >= ArrayCount(sim->city_index) || sim->city_index[city_id] == SIM_NO_CITY) return 0;
    return sim->cities.data + sim->city_index[city_id];
}

internal entity_id
sim_city_entity(sim_state *sim, u8 city_id)
{
    if (!sim_find_city(sim, city_id)) return 0;
    return sim->cities.dense_owner[sim->city_index[city_id]];
}

internal entity_id
sim_officer_entity(sim_state *sim, u16 officer_id)
{
    if (!sim_find_officer(sim, officer_id)) return 0;
    return sim->officers.dense_owner[sim->officer_index[officer_id]];
}

/* Status 0 ruler, 1 viceroy, 2 governor, 3 regular (save-file.md). */
internal b32
sim_officer_is_active(officer_component *officer)
{
    return officer->status <= 3;
}

/* ---- the leader rule ---------------------------------------------------- */

/* factions-corps-and-posts.md: ruler or viceroy first; then the post that
   commands more soldiers; then higher leadership; then higher force; then
   higher merit. The post is not decoded, so the rule skips it (Q-147). A
   full tie goes to the smaller officer id (assumed). */
internal u32
sim_leader_rank(officer_component *officer)
{
    return officer->status <= 1 ? officer->status : 2;
}

internal b32
sim_officer_leads_before(officer_component *a, officer_component *b)
{
    u32 rank_a = sim_leader_rank(a), rank_b = sim_leader_rank(b);
    if (rank_a != rank_b)         return rank_a < rank_b;
    if (a->leadership != b->leadership) return a->leadership > b->leadership;
    if (a->war != b->war)         return a->war > b->war;
    if (a->merit != b->merit)     return a->merit > b->merit;
    return a->officer_id < b->officer_id;
}

internal u16
sim_select_leader(sim_state *sim, const u16 *officer_ids, u32 count)
{
    officer_component *best = 0;
    for (u32 index = 0; index < count; ++index)
    {
        officer_component *officer = sim_find_officer(sim, officer_ids[index]);
        if (officer && (!best || sim_officer_leads_before(officer, best))) best = officer;
    }
    return best ? best->officer_id : SIM_NO_OFFICER;
}

/* The governor (太守) of a city: the leader of the active officers of the
   owner whose base is the city. */
internal u16
sim_city_governor(sim_state *sim, u8 city_id)
{
    city_component *city = sim_find_city(sim, city_id);
    if (!city) return SIM_NO_OFFICER;
    officer_component *best = 0;
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (!sim_officer_is_active(officer) || officer->force_id != city->owner_force_id || officer->base_location != city_id) continue;
        if (!best || sim_officer_leads_before(officer, best)) best = officer;
    }
    return best ? best->officer_id : SIM_NO_OFFICER;
}

/* The ruler (君主) of a force: the officer with status 0 and that force id
   (save-file.md, officer +95 and +100). The per-force table of the save
   also has the ruler id, but its base is not decoded (Q-149). If two
   officers match, the smaller officer id wins (assumed). */
internal u16
sim_force_ruler(sim_state *sim, u8 force_id)
{
    u16 ruler = SIM_NO_OFFICER;
    if (force_id == SIM_NO_FORCE) return ruler;
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (officer->status != OFFICER_STATUS_RULER || officer->force_id != force_id) continue;
        if (officer->officer_id < ruler) ruler = officer->officer_id;
    }
    return ruler;
}

/* ---- the turn order ----------------------------------------------------- */

/* Fewer cities act first (calendar-and-turn.md, unverified). A tie goes to
   fewer officers, then fewer troops (knowledge/turn-structure.md,
   unverified), then the smaller force id (assumed; Q-143). */
internal b32
sim_faction_before(sim_faction *a, sim_faction *b)
{
    if (a->city_count != b->city_count)       return a->city_count < b->city_count;
    if (a->officer_count != b->officer_count) return a->officer_count < b->officer_count;
    if (a->troops != b->troops)               return a->troops < b->troops;
    return a->force_id < b->force_id;
}

/* Builds the order of this turn: each force that owns a city, with its
   size now. The game computes the order again every turn
   (knowledge/turn-structure.md). */
internal void
sim_compute_turn_order(sim_state *sim)
{
    sim_turn_state *turns = &sim->turns;
    sim_faction by_force[SIM_FORCE_CAPACITY] = {};
    b32 owns_city[SIM_FORCE_CAPACITY] = {};
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        city_component *city = sim->cities.data + index;
        if (city->owner_force_id == SIM_NO_FORCE) continue;
        sim_faction *faction = by_force + city->owner_force_id;
        owns_city[city->owner_force_id] = true;
        faction->city_count++;
        faction->troops += city->troops;
    }
    officer_component *leaders[SIM_FORCE_CAPACITY] = {};
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (!sim_officer_is_active(officer) || officer->force_id == SIM_NO_FORCE) continue;
        by_force[officer->force_id].officer_count++;
        officer_component **leader = leaders + officer->force_id;
        if (!*leader || sim_officer_leads_before(officer, *leader)) *leader = officer;
    }

    turns->order_count = 0;
    for (u32 force = 0; force < SIM_FORCE_CAPACITY; ++force)
    {
        if (!owns_city[force]) continue;
        sim_faction faction = by_force[force];
        faction.force_id          = (u8)force;
        faction.leader_officer_id = leaders[force] ? leaders[force]->officer_id : SIM_NO_OFFICER;
        /* insertion sort: the order has at most one entry for each force */
        u32 place = turns->order_count++;
        while (place > 0 && sim_faction_before(&faction, turns->order + place - 1))
        {
            turns->order[place] = turns->order[place - 1];
            place--;
        }
        turns->order[place] = faction;
    }
    turns->acting = 0;
}

internal u8
sim_whose_turn(sim_state *sim)
{
    sim_turn_state *turns = &sim->turns;
    return turns->acting < turns->order_count ? turns->order[turns->acting].force_id : SIM_NO_FORCE;
}

/* ---- action points ------------------------------------------------------ */

internal u32
sim_recover_action_points(u32 pool, u32 recovery, u32 cap)
{
    u32 total = pool + recovery;
    return total < cap ? total : cap;
}

/* ASSUMED RULE (Q-019, Q-141). The spec names the inputs of the recovery
   (the cities of the corps, the leadership and the charm of the leader,
   the intellect of the adviser, the tally office) but gives no formula.
   This rule takes the higher of the leadership and the charm of the leader,
   from the unverified claim of knowledge/turn-structure.md. The cities,
   the adviser and the tally office do not change it yet. Each faction has
   one corps (corps 1, the ruler's), because the save's corps are not
   decoded. */
internal u32
sim_action_point_recovery(sim_state *sim, sim_faction *faction)
{
    officer_component *leader = sim_find_officer(sim, faction->leader_officer_id);
    if (!leader) return 0;
    u32 recovery = leader->leadership > leader->charm ? leader->leadership : leader->charm;
    /* The tally office (符節台, PK) raises the recovery of the corps of its
       city (manual p.29): tally.action_points for each built office in the
       cities of the faction (assumed, Q-164). */
    u32 offices = 0;
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        city_component *city = sim->cities.data + index;
        if (city->owner_force_id == faction->force_id) offices += sim_count_built(sim, city->city_id, FACILITY_EFFECT_ACTION_POINTS);
    }
    return recovery + offices * sim->content.rules.tally_action_points;
}

internal void
sim_recover_all_action_points(sim_state *sim, u32 cause, sim_event_sink *events)
{
    sim_turn_state *turns = &sim->turns;
    for (u32 index = 0; index < turns->order_count; ++index)
    {
        sim_faction *faction = turns->order + index;
        u32 *pool  = turns->action_points + faction->force_id;
        u32 before = *pool;
        *pool = sim_recover_action_points(before, sim_action_point_recovery(sim, faction), sim->content.rules.ap_cap);
        sim_event event = sim_event_make(sim, SIM_EVENT_ACTION_POINTS_RECOVERED, cause);
        event.faction = faction->force_id;
        event.delta   = (s32)(*pool - before);
        event.assumed = true;
        sim_emit(events, event);
    }
}

/* ---- the factions of a turn --------------------------------------------- */

/* Lets each AI faction pass, from the faction that acts now, until a human
   faction acts or every faction acted. */
internal void
sim_run_factions_until_human(sim_state *sim, u32 cause, sim_event_sink *events)
{
    sim_turn_state *turns = &sim->turns;
    while (turns->acting < turns->order_count)
    {
        sim_faction *faction = turns->order + turns->acting;
        if (turns->human[faction->force_id])
        {
            sim_event started = sim_event_make(sim, SIM_EVENT_FACTION_TURN_STARTED, cause);
            started.faction = faction->force_id;
            started.delta   = (s32)turns->acting;
            sim_emit(events, started);
            return;
        }
        /* The AI phase placeholder: the faction gives no command (M8 adds the planner). */
        sim_event passed = sim_event_make(sim, SIM_EVENT_AI_PASSED, cause);
        passed.faction = faction->force_id;
        passed.delta   = (s32)turns->acting;
        sim_emit(events, passed);
        turns->acting++;
    }
}

/* The order, then victory and defeat (outcome.cpp). After the end of the
   game no faction acts. */
internal void
sim_start_turn(sim_state *sim, u32 cause, sim_event_sink *events)
{
    sim_compute_turn_order(sim);
    sim_check_outcome(sim, cause, events);
    if (sim_game_over(sim)) return;
    sim_recover_all_action_points(sim, cause, events);
    sim_run_factions_until_human(sim, cause, events);
}

/* "Proceed": the faction that acts ends its turn. When every faction has
   acted, the date advances and the next turn starts. After the end of the
   game it does nothing. */
internal void
sim_end_faction_turn(sim_state *sim, u32 cause, sim_event_sink *events)
{
    if (sim_game_over(sim)) return;
    sim_turn_state *turns = &sim->turns;
    if (turns->acting < turns->order_count)
    {
        sim_event ended = sim_event_make(sim, SIM_EVENT_FACTION_TURN_ENDED, cause);
        ended.faction = turns->order[turns->acting].force_id;
        ended.delta   = (s32)turns->acting;
        sim_emit(events, ended);
        turns->acting++;
        sim_run_factions_until_human(sim, cause, events);
    }
    if (turns->acting >= turns->order_count) sim_advance_turn(sim, cause, events);
}

/* ---- the calendar events ------------------------------------------------ */

internal void
sim_emit_hook(sim_state *sim, sim_event_kind kind, u32 cause, sim_event_sink *events)
{
    sim_emit(events, sim_event_make(sim, kind, cause));
}

/* 1 January: the game removes every black market (PK; manual p.29). */
internal void
sim_remove_new_year_facilities(sim_state *sim, u32 cause, sim_event_sink *events)
{
    for (u32 city_id = 0; city_id < SAVE_CITY_COUNT; ++city_id)
    {
        city_component *city = sim_find_city(sim, (u8)city_id);
        sim_city_site *site = sim->sites + city_id;
        for (u32 slot_index = 0; slot_index < site->slot_count; ++slot_index)
        {
            sim_slot *slot = site->slots + slot_index;
            if (slot->state == SIM_SLOT_EMPTY || slot->facility >= sim->content.facility_count ||
                !sim->content.facilities[slot->facility].new_year_removal) continue;
            for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place)
            {
                officer_component *officer = sim_find_officer(sim, slot->officers[place]);
                if (officer) officer->job_city = SIM_NO_CITY;
                slot->officers[place] = SIM_NO_OFFICER;
            }
            slot->state = SIM_SLOT_EMPTY;
            slot->facility = CONTENT_NO_FACILITY;
            slot->turns_left = 0;
            slot->level = 0;
            slot->job_weapon = CONTENT_NO_WEAPON;
            sim_event event = sim_event_make(sim, SIM_EVENT_BLACK_MARKET_REMOVED, cause);
            event.subject = city ? sim_city_entity(sim, (u8)city_id) : 0;
            event.faction = city ? city->owner_force_id : SIM_NO_FORCE;
            event.delta   = (s32)slot_index;
            sim_emit(events, event);
        }
    }
}

/* The calendar events of the new date (calendar-and-turn.md, "Calendar
   events"). The production system (economy.cpp) gives the gold, the grain,
   the salaries and the consumption. The hook events mark the dates when the
   calendar rolls the disasters; disasters.cpp rolls them after the income
   of the day, so that a new disaster changes the next payment. The officer
   changes stay hooks until F-051. */
internal void
sim_run_calendar(sim_state *sim, u32 cause, sim_event_sink *events)
{
    calendar_date date = sim->turns.date;
    sim_progress_jobs(sim, cause, events);
    sim_progress_research(sim, cause, events);
    if (calendar_starts_year(date))
    {
        sim_event year = sim_event_make(sim, SIM_EVENT_YEAR_STARTED, cause);
        year.delta = date.year;
        sim_emit(events, year);
        sim_remove_new_year_facilities(sim, cause, events);
        sim_emit_hook(sim, SIM_EVENT_HOOK_OFFICERS_AGE, cause, events);
        sim_run_officers_year(sim, cause, events);   /* M7: growth with age and the lifespan */
        sim_emit_hook(sim, SIM_EVENT_HOOK_NEW_OFFICERS, cause, events);
    }
    if (calendar_starts_month(date))
    {
        sim_event month = sim_event_make(sim, SIM_EVENT_MONTH_STARTED, cause);
        month.delta = date.month;
        sim_emit(events, month);
        sim_run_production_month(sim, cause, events);
        sim_run_officers_month(sim, cause, events);   /* M7: loyalty, ranks, ceasefires */
        sim_emit_hook(sim, SIM_EVENT_HOOK_BANDITS, cause, events);
    }
    if (calendar_starts_season(date))
    {
        sim_event season = sim_event_make(sim, SIM_EVENT_SEASON_STARTED, cause);
        season.delta = calendar_season_of(date);
        sim_emit(events, season);
        sim_run_production_season(sim, cause, events);
        if (date.month == 7) sim_emit_hook(sim, SIM_EVENT_HOOK_GOOD_HARVEST, cause, events);
        if (date.month != 10) sim_emit_hook(sim, SIM_EVENT_HOOK_LOCUSTS, cause, events);
        sim_emit_hook(sim, SIM_EVENT_HOOK_PLAGUE, cause, events);
    }
    if (calendar_starts_month(date)) sim_roll_disasters(sim, date.month, cause, events);
    sim_run_production_turn(sim, cause, events);
    sim_run_units_turn(sim, cause, events);
    sim_run_field_turn(sim, cause, events);
    sim_run_officers_turn(sim, cause, events);
    sim_run_durability(sim, cause, events);
    sim_run_disasters_turn(sim, cause, events);
}
