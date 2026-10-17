/* Victory and defeat (calendar-and-turn.md, "Victory and defeat"; F-008).
   Included by sim.cpp.

   The rules apply to the human factions, at the start of each turn:
   - Defeat: the year is game.end_year (351); or the faction has no city;
     or it has no active officer; or its ruler accepted a surrender demand
     (M7 sets that outcome).
   - Victory: the faction owns every city that has an owner. Empty cities
     do not count (manual p.9).
   The game is over when a human faction wins, or when every human faction
   lost. Then each command refuses with SIM_REFUSAL_GAME_OVER. */

internal b32
sim_game_over(sim_state *sim)
{
    return sim->outcome.kind != SIM_OUTCOME_NONE;
}

internal const char *
sim_outcome_name(sim_outcome_kind kind)
{
    switch (kind)
    {
        case SIM_OUTCOME_NONE:              return "none";
        case SIM_OUTCOME_VICTORY:           return "victory";
        case SIM_OUTCOME_DEFEAT_TIME:       return "defeat_time";
        case SIM_OUTCOME_DEFEAT_NO_CITY:    return "defeat_no_city";
        case SIM_OUTCOME_DEFEAT_NO_OFFICER: return "defeat_no_officer";
        case SIM_OUTCOME_DEFEAT_SURRENDER:  return "defeat_surrender";
        default:                            return "?";
    }
}

internal void
sim_emit_outcome(sim_state *sim, sim_event_kind kind, u8 force, sim_outcome_kind outcome, u32 cause, sim_event_sink *events)
{
    sim_event event = sim_event_make(sim, kind, cause);
    event.faction = force;
    event.detail  = outcome;
    sim_emit(events, event);
}

/* The reason why a human faction lost now, or SIM_OUTCOME_NONE. */
internal sim_outcome_kind
sim_faction_defeat(sim_state *sim, u8 force, u32 *cities_of)
{
    sim_rules *rules = &sim->content.rules;
    if (rules->game_end_year && sim->turns.date.year >= rules->game_end_year) return SIM_OUTCOME_DEFEAT_TIME;
    if (cities_of[force] == 0) return SIM_OUTCOME_DEFEAT_NO_CITY;
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (sim_officer_is_active(officer) && officer->force_id == force) return SIM_OUTCOME_NONE;
    }
    return SIM_OUTCOME_DEFEAT_NO_OFFICER;
}

internal void
sim_check_outcome(sim_state *sim, u32 cause, sim_event_sink *events)
{
    if (sim_game_over(sim) || sim->cities.count == 0) return;
    u32 cities_of[SIM_FORCE_CAPACITY] = {};
    u32 owners = 0;
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        city_component *city = sim->cities.data + index;
        if (city->owner_force_id == SIM_NO_FORCE) continue;
        if (cities_of[city->owner_force_id]++ == 0) owners++;
    }

    u32 humans = 0, humans_left = 0;
    sim_outcome_kind last_defeat = SIM_OUTCOME_NONE;
    u8 last_force = SIM_NO_FORCE;
    for (u32 force = 0; force < SIM_FORCE_CAPACITY; ++force)
    {
        if (!sim->turns.human[force]) continue;
        humans++;
        if (sim->outcome.lost[force]) continue;
        sim_outcome_kind defeat = sim_faction_defeat(sim, (u8)force, cities_of);
        if (defeat != SIM_OUTCOME_NONE)
        {
            sim->outcome.lost[force] = true;
            sim_emit_outcome(sim, SIM_EVENT_GAME_LOST, (u8)force, defeat, cause, events);
            last_defeat = defeat;
            last_force  = (u8)force;
            continue;
        }
        humans_left++;
        if (owners == 1 && cities_of[force] > 0)
        {
            sim->outcome.kind     = SIM_OUTCOME_VICTORY;
            sim->outcome.force_id = (u8)force;
            sim->outcome.ending   = sim_ending_for(sim, (u8)force);   /* M9: events.md, endings */
            sim_emit_outcome(sim, SIM_EVENT_GAME_WON, (u8)force, SIM_OUTCOME_VICTORY, cause, events);
            sim_emit_outcome(sim, SIM_EVENT_ENDING, (u8)force, (sim_outcome_kind)sim->outcome.ending, cause, events);
            return;
        }
    }
    if (humans > 0 && humans_left == 0 && last_defeat != SIM_OUTCOME_NONE)
    {
        sim->outcome.kind     = last_defeat;
        sim->outcome.force_id = last_force;
    }
}
