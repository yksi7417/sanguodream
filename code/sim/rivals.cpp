/* The ruler commands of M8 (factions-corps-and-posts.md, city-commands.md)
   and the Han emperor (calendar-and-turn.md, officers-and-relations.md,
   events.md): 軍團 (create, change, dissolve a delegated corps), 軍師, 評定,
   and the emperor with his protector. Included by sim.cpp after debate.cpp.

   The settings of a corps and the rules of the council are documented; the
   action points of a corps, the accuracy of the adviser and the gains of
   the protector are assumed (Q-185, Q-186). */

/* ---- 軍團 ----------------------------------------------------------------------------- */

/* The checks every M8 ruler command shares: the content, the game, a
   faction that acts now. */
internal sim_refusal
sim_check_ruler_common(sim_state *sim, u8 force)
{
    if (!sim->has_content) return SIM_REFUSAL_NO_CONTENT;
    if (sim_game_over(sim)) return SIM_REFUSAL_GAME_OVER;
    if (sim_busy(sim) || sim->pending.active) return SIM_REFUSAL_BUSY;
    if (force == SIM_NO_FORCE || sim_whose_turn(sim) != force) return SIM_REFUSAL_NOT_YOUR_TURN;
    return SIM_REFUSAL_NONE;
}

/* The faction of a corps order: the faction of its viceroy, or of its corps. */
internal u8
sim_corps_order_force(sim_state *sim, sim_corps_order *order)
{
    officer_component *viceroy = sim_find_officer(sim, order->viceroy);
    if (viceroy) return viceroy->force_id;
    return sim_whose_turn(sim);
}

internal sim_check
sim_check_corps(sim_state *sim, sim_corps_order *order)
{
    sim_check check = {};
    check.slot = SIM_ANY_SLOT;
    u8 force = order->action == CORPS_DISSOLVE ? sim_whose_turn(sim) : sim_corps_order_force(sim, order);
    sim_refusal refusal = sim_check_ruler_common(sim, force);
    if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
    if (order->action >= CORPS_ACTION_COUNT) return sim_refuse(check, SIM_REFUSAL_NO_CORPS);
    if (order->action != CORPS_CREATE && (order->corps == 0 || order->corps >= SIM_CORPS_CAPACITY || !sim->corps[force][order->corps].alive))
    {
        return sim_refuse(check, SIM_REFUSAL_NO_CORPS);
    }
    if (order->action == CORPS_DISSOLVE)
    {
        check.slot = order->corps;
        return check;
    }
    officer_component *viceroy = sim_find_officer(sim, order->viceroy);
    if (!viceroy || !sim_officer_is_active(viceroy) || viceroy->force_id != force || viceroy->status == OFFICER_STATUS_RULER)
    {
        return sim_refuse(check, SIM_REFUSAL_NO_OFFICER);
    }
    for (u32 index = 1; index < SIM_CORPS_CAPACITY; ++index)
    {
        sim_corps *other = sim->corps[force] + index;
        if (other->alive && other->viceroy == order->viceroy && index != order->corps) return sim_refuse(check, SIM_REFUSAL_OFFICER_BUSY);
    }
    if (order->policy >= CORPS_POLICY_COUNT) return sim_refuse(check, SIM_REFUSAL_NO_CORPS);
    u32 cities = 0;
    for (u32 city_id = 0; city_id < SAVE_CITY_COUNT; ++city_id)
    {
        if (!((order->cities >> city_id) & 1)) continue;
        city_component *city = sim_find_city(sim, (u8)city_id);
        if (!city || city->owner_force_id != force) return sim_refuse(check, SIM_REFUSAL_NO_CITY);
        u16 ruler = sim_force_ruler(sim, force);
        officer_component *ruler_officer = sim_find_officer(sim, ruler);
        if (ruler_officer && ruler_officer->current_position == city_id) return sim_refuse(check, SIM_REFUSAL_NO_CITY);   /* the ruler's city stays */
        cities++;
    }
    if (!cities) return sim_refuse(check, SIM_REFUSAL_NO_CITY);
    if (order->action == CORPS_CREATE)
    {
        u32 free = 0;
        for (u32 index = 1; index < SIM_CORPS_CAPACITY && !free; ++index) if (!sim->corps[force][index].alive) free = index;
        if (!free) return sim_refuse(check, SIM_REFUSAL_NO_CORPS);
        check.slot = (u8)free;
    }
    else check.slot = order->corps;
    check.quantity = cities;
    return check;
}

/* 軍團: create or change a delegated corps (its 都督, cities and settings),
   or dissolve it: its cities return to corps 1 (manual p.24). */
internal void
sim_corps_command(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_corps_order *order = &command.corps;
    sim_check check = sim_check_corps(sim, order);
    if (check.refusal != SIM_REFUSAL_NONE) return;
    u8 force = order->action == CORPS_DISSOLVE ? sim_whose_turn(sim) : sim_corps_order_force(sim, order);
    u8 index = check.slot;
    sim_corps *corps = sim->corps[force] + index;
    for (u32 slot = 0; slot < sim->cities.count; ++slot)
    {
        city_component *city = sim->cities.data + slot;
        if (city->owner_force_id == force && city->corps == index) city->corps = 0;
    }
    officer_component *old = sim_find_officer(sim, corps->viceroy);
    if (old && old->status == OFFICER_STATUS_VICEROY) old->status = OFFICER_STATUS_REGULAR_OFFICER;
    sim_event event = sim_event_make(sim, order->action == CORPS_CREATE ? SIM_EVENT_CORPS_CREATED :
                                          order->action == CORPS_CHANGE ? SIM_EVENT_CORPS_CHANGED : SIM_EVENT_CORPS_DISSOLVED, command.id);
    event.faction = force;
    event.detail  = index;
    if (order->action == CORPS_DISSOLVE)
    {
        *corps = {};
        sim_emit(events, event);
        return;
    }
    if (order->action == CORPS_CREATE) sim->corps_ap[force][index] = 0;
    corps->alive          = true;
    corps->viceroy        = order->viceroy;
    corps->policy         = order->policy;
    corps->target         = order->target;
    corps->flags          = order->flags;
    corps->transport_city = order->transport_city;
    for (u32 city_id = 0; city_id < SAVE_CITY_COUNT; ++city_id)
    {
        city_component *city = (order->cities >> city_id) & 1 ? sim_find_city(sim, (u8)city_id) : 0;
        if (city) city->corps = index;
    }
    officer_component *viceroy = sim_find_officer(sim, order->viceroy);
    viceroy->status = OFFICER_STATUS_VICEROY;
    event.subject = sim_officer_entity(sim, viceroy->officer_id);
    sim_emit(events, event);
}

/* ---- 軍師 and 評定 ------------------------------------------------------------------------ */

internal sim_check
sim_check_adviser(sim_state *sim, sim_m8_order *order)
{
    sim_check check = {};
    check.slot = SIM_ANY_SLOT;
    sim_refusal refusal = sim_check_ruler_common(sim, order->force);
    if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
    officer_component *officer = sim_find_officer(sim, order->officer);
    if (!officer || !sim_officer_is_active(officer) || officer->force_id != order->force || officer->status == OFFICER_STATUS_RULER)
    {
        return sim_refuse(check, SIM_REFUSAL_NO_OFFICER);
    }
    if (officer->intelligence < sim->content.rules.adviser_intellect) return sim_refuse(check, SIM_REFUSAL_NO_APTITUDE);
    return check;
}

internal void
sim_adviser_command(sim_state *sim, sim_command command, sim_event_sink *events)
{
    if (sim_check_adviser(sim, &command.m8).refusal != SIM_REFUSAL_NONE) return;
    sim->adviser[command.m8.force] = command.m8.officer;
    officer_component *officer = sim_find_officer(sim, command.m8.officer);
    sim_emit_officer(sim, SIM_EVENT_ADVISER_APPOINTED, officer, 0, 0, false, command.id, events);
}

/* The advice of the adviser: the first proposals of the planner for the
   whole faction, one for each advice.per_intellect points of intellect
   (assumed, Q-186). None without an adviser. */
internal u32
sim_advice(sim_state *sim, u8 force, sim_plan *plan)
{
    plan->count = 0;
    officer_component *adviser = force < SIM_FORCE_CAPACITY ? sim_find_officer(sim, sim->adviser[force]) : 0;
    if (!adviser || !sim_officer_is_active(adviser) || adviser->force_id != force) return 0;
    sim_plan_faction(sim, force, SIM_NO_CITY, 0xFF, CORPS_FLAG_ALL, plan);
    u32 shown = Maximum(1u, adviser->intelligence / Maximum(sim->content.rules.advice_per_intellect, 1u));
    if (plan->count > shown) plan->count = shown;
    return plan->count;
}

/* 評定: the proposals of the planner for the city where the ruler stands
   (city-commands.md: only when the ruler is in the base). */
internal sim_refusal
sim_council_plan(sim_state *sim, u8 force, u8 city_id, sim_plan *plan)
{
    plan->count = 0;
    city_component *city = sim_find_city(sim, city_id);
    if (!city || city->owner_force_id != force) return SIM_REFUSAL_NO_CITY;
    officer_component *ruler = sim_find_officer(sim, sim_force_ruler(sim, force));
    if (!ruler || ruler->current_position != city_id) return SIM_REFUSAL_RULER_ABSENT;
    sim_plan_faction(sim, force, city_id, 0xFF, CORPS_FLAG_ALL, plan);
    return SIM_REFUSAL_NONE;
}

internal sim_check
sim_check_council(sim_state *sim, sim_m8_order *order)
{
    sim_check check = {};
    check.slot = SIM_ANY_SLOT;
    sim_refusal refusal = sim_check_ruler_common(sim, order->force);
    if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
    sim_plan plan;
    refusal = sim_council_plan(sim, order->force, order->city_id, &plan);
    if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
    if (order->value >= plan.count) return sim_refuse(check, SIM_REFUSAL_NO_TARGET);
    check.quantity = plan.count;
    return check;
}

/* The officers act on one proposal of the council: its command runs. */
internal void
sim_council_command(sim_state *sim, sim_command command, sim_event_sink *events)
{
    if (sim_check_council(sim, &command.m8).refusal != SIM_REFUSAL_NONE) return;
    sim_plan plan;
    sim_council_plan(sim, command.m8.force, command.m8.city_id, &plan);
    sim_command chosen = plan.items[command.m8.value].command;
    chosen.id = command.id;
    sim_apply(sim, chosen, events);
}

/* ---- the Han emperor -------------------------------------------------------------------- */

/* The emperor of a date (events.md: 少帝 from 189/4, 獻帝 from 189/9, none
   after 234/4) in the city emperor.city (assumed: 洛陽, Q-186). */
internal void
sim_init_emperor(sim_state *sim)
{
    calendar_date date = sim->turns.date;
    u32 stamp = (u32)date.year * 12 + date.month;
    if (stamp < 189u * 12 + 4)      sim->emperor.who = EMPEROR_LING;
    else if (stamp < 189u * 12 + 9) sim->emperor.who = EMPEROR_SHAO;
    else if (stamp < 234u * 12 + 4) sim->emperor.who = EMPEROR_XIAN;
    else                            sim->emperor.who = EMPEROR_NONE;
    sim->emperor.city = (u8)Minimum(sim->content.rules.emperor_city, (u32)SAVE_CITY_COUNT - 1);
}

/* The faction that protects the emperor: the owner of his city. */
internal u8
sim_emperor_protector(sim_state *sim)
{
    if (sim->emperor.who == EMPEROR_NONE) return SIM_NO_FORCE;
    city_component *city = sim_find_city(sim, sim->emperor.city);
    return city ? city->owner_force_id : (u8)SIM_NO_FORCE;
}

/* Each month the protector gains technique points, and its Han-respecting
   officers gain loyalty (calendar-and-turn.md, officers-and-relations.md). */
internal void
sim_run_emperor_month(sim_state *sim, u32 cause, sim_event_sink *events)
{
    u8 protector = sim_emperor_protector(sim);
    if (protector == SIM_NO_FORCE) return;
    sim_add_technique_points(sim, protector, (s32)sim->content.rules.emperor_technique, cause, events);
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (sim_officer_is_active(officer) && officer->force_id == protector && officer->han_respect == 2 && officer->status != OFFICER_STATUS_RULER)
        {
            sim_change_loyalty(sim, officer, (s32)sim->content.rules.emperor_loyalty, cause, events);
        }
    }
}

internal void
sim_set_emperor(sim_state *sim, u8 who, u32 cause, sim_event_sink *events)
{
    sim->emperor.who = who;
    sim_event event = sim_event_make(sim, SIM_EVENT_EMPEROR_CHANGED, cause);
    event.detail = who;
    event.delta  = sim->emperor.city;
    sim_emit(events, event);
}
