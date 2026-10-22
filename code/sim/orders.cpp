/* The long orders of a unit (city-commands.md, "Long orders"; manual p.24;
   clone gap report 2: G2-052, G2-056, G2-058): move to a hex, destroy a
   unit, drive a unit away, follow a unit. The player gives the order in
   its turn. The unit does one step at once. Then it does one step at each
   進行 of its faction until the order ends (the original: the unit of the
   player moved on at the change of the date, G2-056 and G2-058). 委任部隊行動
   runs the steps now (manual p.24).

   The spec gives the kinds of the orders and the time of the steps. It
   gives no path rule. ASSUMED (Q-227): the path is the lowest cost over
   the terrain, own cities and bases are waypoints, and the units on the
   way do not block the plan. A step moves the unit to the hex of the path
   that is farthest along it and in the reach of this turn. 殲滅 attacks
   the target with the normal attack of the class when the target is in
   range, and goes on until the target is gone. 驅趕 ends after one attack
   (assumed). Included by sim.cpp after ai.cpp. */

/* The path of a plan from the unit to the goal, in walk order, without
   the hex of the unit. Returns the count, 0 when the plan has no goal. */
internal u32
sim_plan_path(sim_reach *plan, hex_coord goal, hex_coord *out, u32 capacity)
{
    u32 place = sim_reach_find(plan, goal);
    if (place == U32_MAX || place == 0) return 0;
    u32 length = 0;
    for (u32 at = place; at != U32_MAX && at != 0; at = plan->from[at]) length++;
    if (length > capacity) return 0;
    u32 fill = length;
    for (u32 at = place; at != U32_MAX && at != 0 && fill > 0; at = plan->from[at]) out[--fill] = plan->cells[at];
    return length;
}

/* The days of a plan: one turn of 10 days for each full movement of the
   unit, and one more for a part (the dialog of the original shows 10,
   20 or 30 days). */
internal u32
sim_plan_days(sim_state *sim, sim_unit *unit, u32 cost)
{
    sim_unit_values values = sim_unit_values_of(sim, unit->force_id, unit->unit_type, unit->officers);
    u32 move = values.move ? values.move : 1;
    return (cost + move - 1) / move * CALENDAR_DAYS_PER_TURN;
}

internal void
sim_give_unit_order(sim_state *sim, u32 unit_id, u8 kind, hex_coord hex, u32 target, u32 cause, sim_event_sink *events)
{
    sim_unit *unit = sim->units + unit_id;
    unit->order        = kind;
    unit->order_hex    = hex;
    unit->order_target = target < SIM_UNIT_CAPACITY ? (u16)target : (u16)SIM_NO_UNIT;
    sim_emit_unit(sim, SIM_EVENT_UNIT_ORDERED, unit_id, SIM_NO_CITY, (s32)kind, true, cause, events);
}

internal void
sim_end_unit_order(sim_state *sim, u32 unit_id, u32 cause, sim_event_sink *events)
{
    sim_unit *unit = sim->units + unit_id;
    if (unit->order == UNIT_ORDER_NONE) return;
    s32 kind = unit->order;
    unit->order        = UNIT_ORDER_NONE;
    unit->order_target = SIM_NO_UNIT;
    sim_emit_unit(sim, SIM_EVENT_UNIT_ORDER_DONE, unit_id, SIM_NO_CITY, kind, true, cause, events);
}

/* One step of a plan: the unit moves to the hex of the path that is
   farthest along it and in its reach of this turn. When a unit stands on
   every hex of the path in the reach, the unit moves to the reachable hex
   nearest to the goal. stop_before_goal leaves out the goal, the hex of
   another unit. Returns true after a move. */
internal b32
sim_unit_step(sim_state *sim, u32 unit_id, hex_coord goal, b32 stop_before_goal, u32 cause, sim_event_sink *events)
{
    sim_unit *unit = sim_find_unit(sim, unit_id);
    if (!unit || unit->moved_turn == sim->turn + 1 || unit->acted_turn == sim->turn + 1) return false;
    arena_reset(&sim->scratch);
    sim_reach plan = sim_unit_plan(sim, unit_id, goal, &sim->scratch);
    hex_coord path[SIM_UNIT_PLAN_CAPACITY];
    u32 length = sim_plan_path(&plan, goal, path, ArrayCount(path));
    if (stop_before_goal && length) length--;
    if (!length) return false;
    sim_reach reach = sim_unit_reach(sim, unit_id, &sim->scratch);
    hex_coord to = unit->hex;
    b32 found = false;
    for (u32 index = length; index > 0 && !found; --index)
    {
        if (sim_reach_find(&reach, path[index - 1]) != U32_MAX) { to = path[index - 1]; found = true; }
    }
    if (!found)
    {
        u32 best = 0;
        for (u32 index = 1; index < reach.count; ++index)
        {
            if (hex_distance(reach.cells[index], goal) < hex_distance(reach.cells[best], goal)) best = index;
        }
        to = reach.cells[best];
    }
    if (hex_equal(to, unit->hex)) return false;
    sim_command move = sim_make_command(sim, SIM_COMMAND_MOVE);
    move.unit.unit = (u16)unit_id;
    move.unit.to   = to;
    sim_apply(sim, move, events);
    (void)cause;
    return true;
}

/* The target of a destroy, drive or follow order, or 0 when it is gone or
   changed side. */
internal sim_unit *
sim_order_target(sim_state *sim, sim_unit *unit, b32 own)
{
    sim_unit *target = sim_find_unit(sim, unit->order_target);
    if (!target) return 0;
    if (own ? target->force_id != unit->force_id : !sim_hostile(sim, unit->force_id, target->force_id)) return 0;
    return target;
}

/* The attack of a destroy or drive order: the normal attack of the class
   when the target is in range. Returns true after an attack. */
internal b32
sim_order_attack(sim_state *sim, u32 unit_id, sim_event_sink *events)
{
    sim_unit *unit = sim->units + unit_id;
    sim_unit *target = sim_find_unit(sim, unit->order_target);
    if (!target || (u32)hex_distance(unit->hex, target->hex) > sim_attack_range(sim, unit)) return false;
    return sim_ai_attack(sim, unit_id, target->hex, events);
}

/* One turn of the order of a unit. A unit in 混亂 or 偽報 cannot act: the
   order waits. */
internal void
sim_run_unit_order(sim_state *sim, u32 unit_id, u32 cause, sim_event_sink *events)
{
    sim_unit *unit = sim->units + unit_id;
    if (!unit->alive || unit->order == UNIT_ORDER_NONE || unit->state != UNIT_STATE_NONE) return;
    switch (unit->order)
    {
        case UNIT_ORDER_MOVE:
        {
            if (!hex_equal(unit->hex, unit->order_hex)) sim_unit_step(sim, unit_id, unit->order_hex, false, cause, events);
            if (!unit->alive || hex_equal(unit->hex, unit->order_hex)) sim_end_unit_order(sim, unit_id, cause, events);
        } break;
        case UNIT_ORDER_DESTROY:
        case UNIT_ORDER_DRIVE:
        {
            sim_unit *target = sim_order_target(sim, unit, false);
            if (!target) { sim_end_unit_order(sim, unit_id, cause, events); break; }
            b32 attacked = sim_order_attack(sim, unit_id, events);
            if (!attacked)
            {
                sim_unit_step(sim, unit_id, target->hex, true, cause, events);
                if (unit->alive) attacked = sim_order_attack(sim, unit_id, events);
            }
            if (!unit->alive) break;
            if (!sim_order_target(sim, unit, false) || (attacked && unit->order == UNIT_ORDER_DRIVE))
            {
                sim_end_unit_order(sim, unit_id, cause, events);
            }
        } break;
        case UNIT_ORDER_FOLLOW:
        {
            sim_unit *target = sim_order_target(sim, unit, true);
            if (!target) { sim_end_unit_order(sim, unit_id, cause, events); break; }
            if (hex_distance(unit->hex, target->hex) > 1) sim_unit_step(sim, unit_id, target->hex, true, cause, events);
        } break;
        default:
            break;
    }
}

/* The steps of the units of a faction that have a long order and did not
   act in this turn: at 進行, and at 委任部隊行動. */
internal void
sim_run_unit_orders(sim_state *sim, u8 force, u32 cause, sim_event_sink *events)
{
    for (u32 unit_id = 0; unit_id < sim->unit_count; ++unit_id)
    {
        sim_unit *unit = sim->units + unit_id;
        if (!unit->alive || unit->force_id != force || unit->order == UNIT_ORDER_NONE) continue;
        if (unit->moved_turn == sim->turn + 1 || unit->acted_turn == sim->turn + 1) continue;
        sim_run_unit_order(sim, unit_id, cause, events);
    }
}

/* ---- the commands ------------------------------------------------------------- */

/* The checks of 殲滅, 驅趕, the follow order and 委任部隊行動. The check of
   a move to a far hex is in sim_check_unit_order (SIM_COMMAND_MOVE). */
internal sim_check
sim_check_long_order(sim_state *sim, sim_command_kind kind, sim_unit_order *order, memory_arena *scratch)
{
    sim_check check = {};
    check.slot = SIM_ANY_SLOT;
    if (kind == SIM_COMMAND_DELEGATE)
    {
        if (!sim->has_content) return sim_refuse(check, SIM_REFUSAL_NO_CONTENT);
        if (sim_game_over(sim)) return sim_refuse(check, SIM_REFUSAL_GAME_OVER);
        if (sim_busy(sim))      return sim_refuse(check, SIM_REFUSAL_BUSY);
        return check;
    }
    sim_unit *unit = 0;
    sim_refusal refusal = sim_check_unit_common(sim, order, &unit);
    if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
    if (unit->state != UNIT_STATE_NONE) return sim_refuse(check, SIM_REFUSAL_UNIT_CONFUSED);
    sim_unit *target = sim_find_unit(sim, order->target);
    if (!target || order->target == order->unit) return sim_refuse(check, SIM_REFUSAL_NO_TARGET);
    if (kind == SIM_COMMAND_FOLLOW)
    {
        if (target->force_id != unit->force_id) return sim_refuse(check, SIM_REFUSAL_NO_TARGET);
    }
    else
    {
        unit_type_kind *type = sim_unit_type(sim, unit->unit_type);
        if (!type || !type->can_attack) return sim_refuse(check, SIM_REFUSAL_NO_APTITUDE);
        if (!sim_hostile(sim, unit->force_id, target->force_id)) return sim_refuse(check, SIM_REFUSAL_NO_TARGET);
    }
    sim_reach plan = sim_unit_plan(sim, order->unit, target->hex, scratch);
    u32 place = sim_reach_find(&plan, target->hex);
    if (place == U32_MAX) return sim_refuse(check, SIM_REFUSAL_UNREACHABLE);
    u32 last = plan.from[place] != U32_MAX ? plan.costs[plan.from[place]] : 0;   /* the step before the hex of the target */
    check.days    = sim_plan_days(sim, unit, last);
    check.turns   = check.days / CALENDAR_DAYS_PER_TURN;
    check.assumed = true;
    return check;
}

/* 殲滅, 驅趕, the follow order: the order, then its first step now.
   委任部隊行動: the steps of every unit of the faction that acts. */
internal void
sim_long_order(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_unit_order *order = &command.unit;
    arena_reset(&sim->scratch);
    if (sim_check_long_order(sim, command.kind, order, &sim->scratch).refusal != SIM_REFUSAL_NONE) return;
    if (command.kind == SIM_COMMAND_DELEGATE)
    {
        sim_run_unit_orders(sim, sim_whose_turn(sim), command.id, events);
        return;
    }
    u8 kind = (u8)(command.kind == SIM_COMMAND_DESTROY_TARGET ? UNIT_ORDER_DESTROY :
                   command.kind == SIM_COMMAND_DRIVE_TARGET   ? UNIT_ORDER_DRIVE : UNIT_ORDER_FOLLOW);
    sim_give_unit_order(sim, order->unit, kind, sim->units[order->target].hex, order->target, command.id, events);
    sim_run_unit_order(sim, order->unit, command.id, events);
}
