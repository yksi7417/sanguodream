/* The AI planner of M8 (knowledge/ai.md, diplomacy.md): a computer faction
   and a delegated corps issue the same commands as a player. The planner
   reads ai_priorities.tsv: each row proposes one kind of command when its
   condition holds. A proposal must pass the check of its command; the
   highest score goes first. The council (評定) and the adviser (軍師) show
   the same proposals. Units move toward the nearest hostile city and
   attack what stands next to them. Included by sim.cpp after events.cpp.

   The spec describes the behaviour of the original AI only as player
   claims; the table, the urgency and the unit rule are assumed (Q-185). */

/* ---- one proposal ---------------------------------------------------------------- */

/* The free officer of the city with the best ability, or SIM_NO_OFFICER. */
internal u16
sim_ai_best_officer(sim_state *sim, city_component *city, sim_ability ability, u16 exclude)
{
    officer_component *best = 0;
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (officer->officer_id == exclude || sim_check_develop_officer(sim, city, officer) != SIM_REFUSAL_NONE) continue;
        if (officer->status == OFFICER_STATUS_RULER && sim->turns.human[city->owner_force_id]) continue;
        if (!best || sim_officer_ability(officer, ability) > sim_officer_ability(best, ability)) best = officer;
    }
    return best ? best->officer_id : (u16)SIM_NO_OFFICER;
}

internal sim_city_order
sim_ai_city_order(city_component *city, u16 officer)
{
    sim_city_order order = {};
    order.city_id     = city->city_id;
    order.officers[0] = officer;
    order.officers[1] = order.officers[2] = SIM_NO_OFFICER;
    return order;
}

internal sim_court_order
sim_ai_court_order(city_component *city, u16 officer)
{
    sim_court_order order = {};
    order.city_id     = city->city_id;
    order.officer     = officer;
    order.target      = SIM_NO_OFFICER;
    order.officers[0] = order.officers[1] = order.officers[2] = SIM_NO_OFFICER;
    order.item        = 0xFF;
    order.post        = CONTENT_NO_POST;
    order.force       = order.force_2 = SIM_NO_FORCE;
    order.target_city = SIM_NO_CITY;
    return order;
}

/* The urgency of a condition, 0 .. ai.urgency, and whether it holds. */
internal b32
sim_ai_condition(sim_state *sim, city_component *city, ai_priority *row, u32 *urgency)
{
    sim_rules *rules = &sim->content.rules;
    u32 scale = rules->ai_urgency;
    *urgency = 0;
    switch (row->condition)
    {
        case AI_ORDER_BELOW:
            if (city->public_order >= row->threshold) return false;
            *urgency = row->threshold ? scale * (row->threshold - city->public_order) / row->threshold : 0;
            return true;
        case AI_TROOPS_BELOW_PERCENT:
        {
            u32 wanted = sim_percent_of(sim_city_soldier_cap(sim, city->city_id), row->threshold);
            if (city->troops >= wanted) return false;
            *urgency = wanted ? scale * (wanted - city->troops) / wanted : 0;
            return true;
        }
        case AI_FREE_SLOT:
            return sim_city_used_slots(sim, city->city_id) < sim->sites[city->city_id].slot_count;
        case AI_FREE_OFFICER:
            for (u32 index = 0; index < sim->officers.count; ++index)
            {
                officer_component *officer = sim->officers.data + index;
                if (officer->status == OFFICER_STATUS_FREE_OR_NOT_APPEARED && officer->base_location == city->city_id &&
                    officer->appearance_year <= sim->turns.date.year) return true;
            }
            return false;
        case AI_MORALE_BELOW:
            if (!city->troops || city->morale >= row->threshold) return false;
            *urgency = scale * (row->threshold - city->morale) / Maximum(row->threshold, 1u);
            return true;
        case AI_WEAPONS_BELOW_TROOPS:
        {
            u32 weapons = 0;
            for (u32 stock = 0; stock < 4; ++stock) weapons += city->equipment_stocks[stock];
            return weapons < city->troops;
        }
        case AI_LOYALTY_BELOW:
            for (u32 index = 0; index < sim->officers.count; ++index)
            {
                officer_component *officer = sim->officers.data + index;
                if (sim_officer_is_active(officer) && officer->force_id == city->owner_force_id && officer->current_position == city->city_id &&
                    officer->status != OFFICER_STATUS_RULER && officer->loyalty < row->threshold) return true;
            }
            return false;
        case AI_TROOPS_ABOVE:    return city->troops > row->threshold;
        case AI_TECHNIQUE_ABOVE: return sim->technique_points[city->owner_force_id] >= row->threshold;
        case AI_GOLD_ABOVE:      return city->gold > row->threshold;
        default:                 return false;
    }
}

/* The nearest hostile city of the force, or SIM_NO_CITY. */
internal u8
sim_ai_target_city(sim_state *sim, u8 force, hex_coord from, u32 max_distance)
{
    u8 best = SIM_NO_CITY;
    s32 best_distance = 0x7FFFFFFF;
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        city_component *other = sim->cities.data + index;
        if (!sim_at_war(sim, force, other->owner_force_id)) continue;
        s32 distance = hex_distance(sim->sites[other->city_id].hex, from);
        if ((u32)distance <= max_distance && distance < best_distance) { best = other->city_id; best_distance = distance; }
    }
    return best;
}

/* The command of a row for the city, or kind SIM_COMMAND_NONE if none fits. */
internal sim_command
sim_ai_command_for(sim_state *sim, city_component *city, ai_priority *row)
{
    sim_command command = {};
    command.kind = SIM_COMMAND_NONE;
    sim_command_kind kind = (sim_command_kind)row->command;
    switch (kind)
    {
        case SIM_COMMAND_PATROL:
        case SIM_COMMAND_CONSCRIPT:
        case SIM_COMMAND_TRAIN:
        case SIM_COMMAND_PRODUCE:
        {
            sim_ability ability = kind == SIM_COMMAND_PATROL ? SIM_ABILITY_LEADERSHIP : kind == SIM_COMMAND_CONSCRIPT ? SIM_ABILITY_CHARM :
                                  kind == SIM_COMMAND_TRAIN ? SIM_ABILITY_FORCE : SIM_ABILITY_INTELLECT;
            sim_city_order order = sim_ai_city_order(city, sim_ai_best_officer(sim, city, ability, SIM_NO_OFFICER));
            if (kind == SIM_COMMAND_PRODUCE)
            {
                u32 lowest = 0xFFFFFFFFu;
                for (u32 weapon = 0; weapon < sim->content.weapon_count; ++weapon)
                {
                    weapon_kind *kind_row = sim->content.weapons + weapon;
                    if (kind_row->max_days || kind_row->stock >= CITY_EQUIPMENT_STOCK_COUNT) continue;
                    order.weapon = (u8)weapon;
                    if (sim_check_city_command(sim, kind, &order).refusal != SIM_REFUSAL_NONE) continue;
                    if (city->equipment_stocks[kind_row->stock] < lowest) { lowest = city->equipment_stocks[kind_row->stock]; command.city.weapon = (u8)weapon; }
                }
                if (lowest == 0xFFFFFFFFu) return command;
                order.weapon = command.city.weapon;
            }
            if (sim_check_city_command(sim, kind, &order).refusal != SIM_REFUSAL_NONE) return command;
            command.kind = kind;
            command.city = order;
            return command;
        }
        case SIM_COMMAND_DEVELOP:
        {
            static const facility_effect wanted[] = { FACILITY_EFFECT_GOLD, FACILITY_EFFECT_GRAIN, FACILITY_EFFECT_CONSCRIPTION, FACILITY_EFFECT_WEAPONS };
            sim_develop_order order = {};
            order.city_id     = city->city_id;
            order.slot        = SIM_ANY_SLOT;
            order.officers[0] = sim_ai_best_officer(sim, city, SIM_ABILITY_POLITICS, SIM_NO_OFFICER);
            order.officers[1] = order.officers[2] = SIM_NO_OFFICER;
            for (u32 want = 0; want < ArrayCount(wanted); ++want)
            {
                for (u32 facility = 0; facility < sim->content.facility_count; ++facility)
                {
                    if (sim->content.facilities[facility].effect != wanted[want] || sim->content.facilities[facility].pk) continue;
                    order.facility = (u8)facility;
                    if (sim_check_develop(sim, &order).refusal != SIM_REFUSAL_NONE) continue;
                    command.kind    = SIM_COMMAND_DEVELOP;
                    command.develop = order;
                    return command;
                }
            }
            return command;
        }
        case SIM_COMMAND_RECRUIT:
        {
            sim_court_order order = sim_ai_court_order(city, sim_ai_best_officer(sim, city, SIM_ABILITY_CHARM, SIM_NO_OFFICER));
            for (u32 index = 0; index < sim->officers.count; ++index)
            {
                officer_component *officer = sim->officers.data + index;
                if (officer->status != OFFICER_STATUS_FREE_OR_NOT_APPEARED || officer->base_location != city->city_id) continue;
                order.target = officer->officer_id;
                if (sim_check_court(sim, SIM_COMMAND_RECRUIT, &order).refusal != SIM_REFUSAL_NONE) continue;
                command.kind  = SIM_COMMAND_RECRUIT;
                command.court = order;
                return command;
            }
            return command;
        }
        case SIM_COMMAND_REWARD:
        {
            sim_court_order order = sim_ai_court_order(city, SIM_NO_OFFICER);
            u32 count = 0;
            for (u32 index = 0; index < sim->officers.count && count < SIM_COMMAND_OFFICERS; ++index)
            {
                officer_component *officer = sim->officers.data + index;
                if (sim_officer_is_active(officer) && officer->force_id == city->owner_force_id && officer->current_position == city->city_id &&
                    officer->status != OFFICER_STATUS_RULER && officer->loyalty < row->threshold && officer->rewarded_turn != sim->turn + 1)
                {
                    order.officers[count++] = officer->officer_id;
                }
            }
            if (!count || sim_check_court(sim, SIM_COMMAND_REWARD, &order).refusal != SIM_REFUSAL_NONE) return command;
            command.kind  = SIM_COMMAND_REWARD;
            command.court = order;
            return command;
        }
        case SIM_COMMAND_RESEARCH:
        {
            sim_research_order order = {};
            order.city_id     = city->city_id;
            order.officers[0] = sim_ai_best_officer(sim, city, SIM_ABILITY_INTELLECT, SIM_NO_OFFICER);
            order.officers[1] = order.officers[2] = SIM_NO_OFFICER;
            for (u32 technique = 0; technique < sim->content.technique_count; ++technique)
            {
                order.technique = (u8)technique;
                if (sim_check_research(sim, &order).refusal != SIM_REFUSAL_NONE) continue;
                command.kind     = SIM_COMMAND_RESEARCH;
                command.research = order;
                return command;
            }
            return command;
        }
        case SIM_COMMAND_SORTIE:
        {
            sim_rules *rules = &sim->content.rules;
            if (sim_ai_target_city(sim, city->owner_force_id, sim->sites[city->city_id].hex, 12) == SIM_NO_CITY) return command;
            u8 pike = content_find_unit_type(&sim->content, "pike");
            if (pike == CONTENT_NO_UNIT_TYPE || city->troops <= rules->ai_sortie_reserve) return command;
            sim_sortie_order order = sim_sortie_proposal(sim, city->city_id, pike);
            order.officers[1] = order.officers[2] = SIM_NO_OFFICER;   /* one officer: the others stay to govern */
            order.soldiers = Minimum(Minimum(order.soldiers, rules->ai_sortie_soldiers), city->troops - rules->ai_sortie_reserve);
            order.grain    = Minimum(order.grain, city->food);
            order.gold     = 0;
            if (sim_check_sortie(sim, &order, false).refusal != SIM_REFUSAL_NONE) return command;
            command.kind   = SIM_COMMAND_SORTIE;
            command.sortie = order;
            return command;
        }
        case SIM_COMMAND_DIPLOMACY:
        {
            sim_court_order order = sim_ai_court_order(city, sim_ai_best_officer(sim, city, SIM_ABILITY_POLITICS, SIM_NO_OFFICER));
            order.kind = SIM_DIPLOMACY_GOODWILL;
            u32 lowest = 101;
            for (u32 other = 0; other < SIM_DIPLOMACY_FORCES; ++other)
            {
                if (other == city->owner_force_id || sim_force_ruler(sim, (u8)other) == SIM_NO_OFFICER) continue;
                u32 friendliness = sim_friendliness(sim, city->owner_force_id, (u8)other);
                if (friendliness < lowest) { lowest = friendliness; order.force = (u8)other; }
            }
            if (lowest > 100 || sim_check_diplomacy(sim, &order).refusal != SIM_REFUSAL_NONE) return command;
            command.kind  = SIM_COMMAND_DIPLOMACY;
            command.court = order;
            return command;
        }
        default:
            return command;
    }
}

/* The commands that a corps setting allows. */
internal b32
sim_ai_allowed(sim_command_kind kind, u32 flags)
{
    switch (kind)
    {
        case SIM_COMMAND_PRODUCE:   return (flags & CORPS_FLAG_PRODUCE) != 0;
        case SIM_COMMAND_CONSCRIPT: return (flags & CORPS_FLAG_CONSCRIPT) != 0;
        case SIM_COMMAND_RECRUIT:   return (flags & CORPS_FLAG_RECRUIT) != 0;
        case SIM_COMMAND_SORTIE:    return (flags & CORPS_FLAG_ATTACK) != 0;
        case SIM_COMMAND_DEVELOP:
        case SIM_COMMAND_RESEARCH:  return (flags & CORPS_FLAG_SAVE_GOLD) == 0;
        default:                    return true;
    }
}

/* The proposals for the cities of a force: all of them, one city, or the
   cities of one corps. Sorted by score, the highest first. */
internal void
sim_plan_faction(sim_state *sim, u8 force, u8 city_filter, u8 corps_filter, u32 flags, sim_plan *plan)
{
    plan->count = 0;
    sim_rules *rules = &sim->content.rules;
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        city_component *city = sim->cities.data + index;
        if (city->owner_force_id != force) continue;
        if (city_filter != SIM_NO_CITY && city->city_id != city_filter) continue;
        if (corps_filter != 0xFF && city->corps != corps_filter) continue;
        for (u32 row = 0; row < sim->content.priority_count; ++row)
        {
            ai_priority *priority = sim->content.priorities + row;
            if (!sim_ai_allowed((sim_command_kind)priority->command, flags)) continue;
            if (priority->command == SIM_COMMAND_DIPLOMACY && city->gold < rules->ai_goodwill_gold) continue;
            u32 urgency = 0;
            if (!sim_ai_condition(sim, city, priority, &urgency)) continue;
            sim_command command = sim_ai_command_for(sim, city, priority);
            if (command.kind == SIM_COMMAND_NONE || plan->count >= SIM_PLAN_CAPACITY) continue;
            sim_proposal proposal = {};
            proposal.command = command;
            proposal.score   = priority->priority + Minimum(urgency, rules->ai_urgency);
            proposal.row     = (u8)row;
            u32 at = plan->count++;
            while (at > 0 && plan->items[at - 1].score < proposal.score) { plan->items[at] = plan->items[at - 1]; --at; }
            plan->items[at] = proposal;
        }
    }
}

/* ---- the turn of a computer faction ------------------------------------------------- */

/* A unit of the AI: attack what stands next to it, else march toward the
   nearest hostile city and attack after the move. */
internal u32
sim_ai_units(sim_state *sim, u8 force, u32 cause, sim_event_sink *events)
{
    u32 issued = 0;
    for (u32 unit_id = 0; unit_id < sim->unit_count; ++unit_id)
    {
        sim_unit *unit = sim->units + unit_id;
        if (!unit->alive || unit->force_id != force || unit->aid_turns || !sim_unit_type(sim, unit->unit_type) || unit->acted_turn == sim->turn + 1) continue;
        u8 target_city = sim_ai_target_city(sim, force, unit->hex, 1000);
        if (unit->moved_turn != sim->turn + 1 && target_city != SIM_NO_CITY)
        {
            arena_reset(&sim->scratch);
            sim_reach reach = sim_unit_reach(sim, unit_id, &sim->scratch);
            hex_coord goal = sim->sites[target_city].hex;
            u32 best = 0;
            for (u32 index = 1; index < reach.count; ++index)
            {
                if (hex_distance(reach.cells[index], goal) < hex_distance(reach.cells[best], goal)) best = index;
            }
            if (best)
            {
                sim_command move = sim_make_command(sim, SIM_COMMAND_MOVE);
                move.unit.unit = (u16)unit_id;
                move.unit.to   = reach.cells[best];
                sim_apply(sim, move, events);
                issued++;
            }
        }
        if (!unit->alive) continue;
        for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d)
        {
            sim_command attack = sim_make_command(sim, SIM_COMMAND_ATTACK);
            attack.unit.unit = (u16)unit_id;
            attack.unit.to   = hex_neighbor(unit->hex, d);
            if (sim_check_attack(sim, &attack.unit).refusal != SIM_REFUSAL_NONE) continue;
            sim_apply(sim, attack, events);
            issued++;
            break;
        }
    }
    (void)cause;
    return issued;
}

/* Runs a plan: the best proposal, then a new plan, up to the budget. */
internal u32
sim_ai_run_plan(sim_state *sim, u8 force, u8 corps_filter, u32 flags, u32 budget, sim_event_sink *events)
{
    u32 issued = 0;
    for (u32 step = 0; step < budget; ++step)
    {
        sim_plan plan;
        sim_plan_faction(sim, force, SIM_NO_CITY, corps_filter, flags, &plan);
        if (!plan.count) break;
        sim_command command = plan.items[0].command;
        command.id = sim->next_command_id++;
        sim_apply(sim, command, events);
        issued++;
    }
    return issued;
}

/* The turn of a computer faction: its cities, then its units. */
internal void
sim_run_ai_faction(sim_state *sim, u8 force, u32 cause, sim_event_sink *events)
{
    if (!sim->settings.ai || !sim->has_content) return;
    u32 issued = sim_ai_run_plan(sim, force, 0xFF, CORPS_FLAG_ALL, sim->content.rules.ai_commands, events);
    issued += sim_ai_units(sim, force, cause, events);
    if (sim->pending.active && sim->pending.force == force) sim->pending.active = false;   /* the AI takes the first choice */
    sim_event planned = sim_event_make(sim, SIM_EVENT_AI_PLANNED, cause);
    planned.faction = force;
    planned.delta   = (s32)issued;
    planned.assumed = true;
    sim_emit(events, planned);
}

/* The delegated corps of a human faction act at the start of its turn,
   each with its own action points (knowledge/ai.md: the points come from
   max(統率, 魅力) of the leader; assumed amount, Q-185). */
internal void
sim_run_delegated_corps(sim_state *sim, u8 force, u32 cause, sim_event_sink *events)
{
    if (!sim->settings.ai || !sim->has_content) return;
    for (u32 index = 1; index < SIM_CORPS_CAPACITY; ++index)
    {
        sim_corps *corps = sim->corps[force] + index;
        officer_component *viceroy = sim_find_officer(sim, corps->viceroy);
        if (!corps->alive || !viceroy) continue;
        u32 gain = Maximum((u32)viceroy->leadership, (u32)viceroy->charm) / Maximum(sim->content.rules.corps_ap_divisor, 1u);
        sim->corps_ap[force][index] = Minimum(sim->corps_ap[force][index] + gain, sim->content.rules.ap_cap);
        u32 saved = sim->turns.action_points[force];
        sim->turns.action_points[force] = sim->corps_ap[force][index];
        sim_ai_run_plan(sim, force, (u8)index, corps->flags, sim->content.rules.corps_commands, events);
        sim->corps_ap[force][index] = sim->turns.action_points[force];
        sim->turns.action_points[force] = saved;
    }
    (void)cause;
}
