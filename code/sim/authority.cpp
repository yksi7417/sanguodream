/* The authorities of the church and the caliphs (docs/EUROPE.md, E6).
   A pack with the rule legitimacy.model = authorities has no Han emperor.
   Each faction has one authority. The authority gives recognition for the
   rank of king or emperor. The faction that holds the seat of an authority
   gains technique points each month. Included by sim.cpp, after rivals.cpp. */

internal b32
sim_uses_authorities(sim_state *sim)
{
    return sim->content.rules.legitimacy_model == LEGITIMACY_AUTHORITIES && sim->content.authority_count > 0;
}

/* The index of the authority of a faction, or CONTENT_AUTHORITY_CAPACITY if it has none. */
internal u32
sim_authority_of(sim_state *sim, u8 force)
{
    u32 slot = sim->content.force_authority[force];
    return slot && slot <= sim->content.authority_count ? slot - 1 : (u32)CONTENT_AUTHORITY_CAPACITY;
}

/* The city of a seat. The story can move it, as Baybars moved the caliph to Cairo. */
internal u8
sim_authority_seat(sim_state *sim, u32 authority)
{
    if (authority >= sim->content.authority_count) return SIM_NO_CITY;
    u8 moved = sim->authorities.seat_moved[authority];
    if (moved) return (u8)(moved - 1);
    u8 seat = sim->content.authorities[authority].seat_city;
    return seat == CONTENT_NO_SEAT ? (u8)SIM_NO_CITY : seat;
}

/* The church of the holder: the church of the pack, a church that the story set, or none. */
internal u8
sim_authority_church(sim_state *sim, u32 authority)
{
    if (authority >= sim->content.authority_count) return CONTENT_CHURCH_NONE;
    u8 holder = sim->authorities.holder[authority];
    if (holder == 0) return sim->content.authorities[authority].church;
    return holder == 1 ? (u8)CONTENT_CHURCH_NONE : (u8)(holder - 2);
}

/* The faction that acts for the holder of an authority. It is the owner of the
   seat. A seat that is not on the map has no keeper (the player, Q-E9). A seat
   without a holder has no keeper. */
internal u8
sim_authority_keeper(sim_state *sim, u32 authority)
{
    if (authority >= sim->content.authority_count || sim_authority_church(sim, authority) == CONTENT_CHURCH_NONE) return SIM_NO_FORCE;
    u8 seat = sim_authority_seat(sim, authority);
    city_component *city = seat == SIM_NO_CITY ? 0 : sim_find_city(sim, seat);
    return city ? city->owner_force_id : (u8)SIM_NO_FORCE;
}

/* The authority gives recognition to its keeper, to the allies of the keeper and to a faction with a crown. */
internal b32
sim_recognized_by_authority(sim_state *sim, u8 force)
{
    u32 authority = sim_authority_of(sim, force);
    if (authority == CONTENT_AUTHORITY_CAPACITY) return false;
    if (sim->authorities.crown[force]) return true;
    u8 keeper = sim_authority_keeper(sim, authority);
    if (keeper == SIM_NO_FORCE) return false;
    return keeper == force || sim_relation(sim, keeper, force) == RELATION_ALLIANCE;
}

/* A faction under interdict has no recognition until the interdict ends. */
internal b32
sim_has_recognition(sim_state *sim, u8 force)
{
    return sim_uses_authorities(sim) && !sim->authorities.interdict[force] && sim_recognized_by_authority(sim, force);
}

/* ---- the monthly rule ------------------------------------------------------------------------ */

internal u8
sim_rome_keeper(sim_state *sim)
{
    return sim_authority_keeper(sim, content_find_authority(&sim->content, "rome"));
}

internal void
sim_start_interdict(sim_state *sim, u8 force, u8 by, u32 months, u32 cause, sim_event_sink *events)
{
    if (force == SIM_NO_FORCE || months == 0) return;
    sim->authorities.interdict[force] = (u8)Minimum(months, 255u);
    sim_event event = sim_event_make(sim, SIM_EVENT_INTERDICT_STARTED, cause);
    event.faction = force;
    event.detail  = sim->authorities.interdict[force];
    event.delta   = by;
    sim_emit(events, event);
}

/* One month of an interdict: the officers lose loyalty, then the months count down. A faction that
   is friendly enough to the owner of Rome leaves the interdict at once. */
internal void
sim_run_interdict_month(sim_state *sim, u8 force, u32 cause, sim_event_sink *events)
{
    u8 keeper = sim_rome_keeper(sim);
    b32 ends = keeper != SIM_NO_FORCE && sim_friendliness(sim, keeper, force) > sim->content.rules.interdict_stop;
    if (!ends)
    {
        for (u32 index = 0; index < sim->officers.count; ++index)
        {
            officer_component *officer = sim->officers.data + index;
            if (sim_officer_is_active(officer) && officer->force_id == force && officer->status != OFFICER_STATUS_RULER)
            {
                sim_change_loyalty(sim, officer, -(s32)sim->content.rules.interdict_loyalty, cause, events);
            }
        }
        --sim->authorities.interdict[force];
        ends = sim->authorities.interdict[force] == 0;
    }
    if (!ends) return;
    sim->authorities.interdict[force] = 0;
    sim_event event = sim_event_make(sim, SIM_EVENT_INTERDICT_LIFTED, cause);
    event.faction = force;
    sim_emit(events, event);
}

/* Each month the owner of a seat gains technique points, and its devout officers gain loyalty. Then
   each interdict has its month. */
internal void
sim_run_authority_month(sim_state *sim, u32 cause, sim_event_sink *events)
{
    if (!sim_uses_authorities(sim)) return;
    for (u32 authority = 0; authority < sim->content.authority_count; ++authority)
    {
        u8 seat = sim_authority_seat(sim, authority);
        city_component *city = seat == SIM_NO_CITY ? 0 : sim_find_city(sim, seat);
        if (!city || city->owner_force_id == SIM_NO_FORCE || sim_authority_church(sim, authority) == CONTENT_CHURCH_NONE) continue;
        u8 owner = city->owner_force_id;
        sim_add_technique_points(sim, owner, (s32)sim->content.rules.authority_technique, cause, events);
        for (u32 index = 0; index < sim->officers.count; ++index)
        {
            officer_component *officer = sim->officers.data + index;
            if (sim_officer_is_active(officer) && officer->force_id == owner && officer->han_respect == 2 && officer->status != OFFICER_STATUS_RULER)
            {
                sim_change_loyalty(sim, officer, (s32)sim->content.rules.authority_loyalty, cause, events);
            }
        }
    }
    for (u32 force = 0; force < SIM_FORCE_CAPACITY; ++force)
    {
        if (sim->authorities.interdict[force]) sim_run_interdict_month(sim, (u8)force, cause, events);
    }
}

/* ---- the commands ---------------------------------------------------------------------------- */

/* ask_crown: the ruler asks his authority for a crown. The keeper of the seat gives it if it is
   friendly enough. The command takes no random number. */
internal sim_check
sim_check_ask_crown(sim_state *sim, sim_m8_order *order)
{
    sim_check check = {};
    check.slot = SIM_ANY_SLOT;
    sim_refusal refusal = sim_check_ruler_common(sim, order->force);
    if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
    if (!sim_uses_authorities(sim)) return sim_refuse(check, SIM_REFUSAL_NO_AUTHORITY);
    u32 authority = sim_authority_of(sim, order->force);
    if (authority == CONTENT_AUTHORITY_CAPACITY) return sim_refuse(check, SIM_REFUSAL_NO_AUTHORITY);
    if (sim_recognized_by_authority(sim, order->force)) return sim_refuse(check, SIM_REFUSAL_HAS_RECOGNITION);
    u8 keeper = sim_authority_keeper(sim, authority);
    if (keeper == SIM_NO_FORCE) return sim_refuse(check, SIM_REFUSAL_NO_AUTHORITY);
    if (sim->authorities.interdict[order->force] || sim_friendliness(sim, keeper, order->force) < sim->content.rules.crown_friendliness)
    {
        return sim_refuse(check, SIM_REFUSAL_NOT_FRIENDLY);
    }
    return check;
}

internal void
sim_ask_crown_command(sim_state *sim, sim_command command, sim_event_sink *events)
{
    if (sim_check_ask_crown(sim, &command.m8).refusal != SIM_REFUSAL_NONE) return;
    u8 force = command.m8.force;
    sim->authorities.crown[force] = 1;
    sim_event event = sim_event_make(sim, SIM_EVENT_CROWN_GIVEN, command.id);
    event.faction = force;
    event.detail  = sim_authority_of(sim, force);
    sim_emit(events, event);
    sim_update_ranks(sim, false, command.id, events);   /* the crown can lift the rank at once */
}

/* interdict: the owner of Rome puts a faction under interdict. The target must have a ruler, must
   not be friendlier than interdict.stop and must not be under interdict. */
internal sim_check
sim_check_interdict(sim_state *sim, sim_m8_order *order)
{
    sim_check check = {};
    check.slot = SIM_ANY_SLOT;
    sim_refusal refusal = sim_check_ruler_common(sim, order->force);
    if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
    if (!sim_uses_authorities(sim) || sim_rome_keeper(sim) != order->force) return sim_refuse(check, SIM_REFUSAL_NO_AUTHORITY);
    u8 target = order->value;
    if (target == order->force || sim_force_ruler(sim, target) == SIM_NO_OFFICER || sim->authorities.interdict[target] ||
        sim_friendliness(sim, order->force, target) > sim->content.rules.interdict_stop)
    {
        return sim_refuse(check, SIM_REFUSAL_NO_TARGET);
    }
    return check;
}

internal void
sim_interdict_command(sim_state *sim, sim_command command, sim_event_sink *events)
{
    if (sim_check_interdict(sim, &command.m8).refusal != SIM_REFUSAL_NONE) return;
    sim_start_interdict(sim, command.m8.value, command.m8.force, sim->content.rules.interdict_months, command.id, events);
}

/* ---- the effects of history.tsv --------------------------------------------------------------- */

/* see_holder: the holder of an authority changes church. A dash leaves the see without a holder. */
internal void
sim_authority_see_holder(sim_state *sim, const char *authority_key, const char *church)
{
    u32 authority = content_find_authority(&sim->content, authority_key);
    if (authority == CONTENT_AUTHORITY_CAPACITY) return;
    u32 found = content_find_church(church);
    if (strcmp(church, "-") == 0) sim->authorities.holder[authority] = 1;
    else if (found != CONTENT_CHURCH_COUNT) sim->authorities.holder[authority] = (u8)(2 + found);
}

/* authority_seat: the seat of an authority moves to another city. */
internal void
sim_authority_move_seat(sim_state *sim, const char *authority_key, u32 city)
{
    u32 authority = content_find_authority(&sim->content, authority_key);
    if (authority == CONTENT_AUTHORITY_CAPACITY || city >= SAVE_CITY_COUNT) return;
    sim->authorities.seat_moved[authority] = (u8)(city + 1);
}

/* ---- the AI ---------------------------------------------------------------------------------- */

/* The rules of the computer (docs/EUROPE.md, E6). None takes a random number.
   - A ruler that has the cities for a rank that needs recognition, and has no recognition, asks for a crown.
   - The owner of Rome puts under interdict the first faction that is at war with an ally of Rome. */
internal void
sim_ai_authority(sim_state *sim, u8 force, sim_event_sink *events)
{
    if (!sim_uses_authorities(sim)) return;
    b32 waits_for_crown = false;
    for (u32 rank = (u32)sim->rank[force] + 1; rank < sim->content.rank_count; ++rank)
    {
        waits_for_crown |= sim->content.ranks[rank].needs_emperor && sim_faction_city_count(sim, force) >= sim->content.ranks[rank].cities;
    }
    if (waits_for_crown)
    {
        sim_command ask = sim_make_command(sim, SIM_COMMAND_ASK_CROWN);
        ask.m8.force = force;
        sim_apply(sim, ask, events);
    }
    if (sim_rome_keeper(sim) != force) return;
    for (u32 target = 0; target < SIM_FORCE_CAPACITY; ++target)
    {
        b32 attacks_ally = false;
        for (u32 ally = 0; ally < SIM_FORCE_CAPACITY && !attacks_ally; ++ally)
        {
            attacks_ally = ally != force && ally != target && sim_relation(sim, force, (u8)ally) == RELATION_ALLIANCE && sim_at_war(sim, (u8)target, (u8)ally);
        }
        if (!attacks_ally) continue;
        sim_command interdict = sim_make_command(sim, SIM_COMMAND_INTERDICT);
        interdict.m8.force = force;
        interdict.m8.value = (u8)target;
        if (sim_check_interdict(sim, &interdict.m8).refusal != SIM_REFUSAL_NONE) continue;
        sim_apply(sim, interdict, events);
        break;
    }
}
