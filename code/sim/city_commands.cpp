/* The five city commands of M3 (city-commands.md, weapons-and-production.md):
   巡察 patrol, 徵兵 conscript, 訓練 train, 商人 merchant and 生產兵裝
   produce. Included by sim.cpp.

   Each command has the costs of the manual (20 action points; the gold of
   the command) and the refusal reasons of the develop command. A refused
   command changes nothing. A command that runs pays when it starts, marks
   its officers as acted, gives each officer merit, and (all but the
   merchant) gives technique points to the faction. The spec names the
   inputs of each effect but no formula: each effect is an assumed rule of
   rules.tsv, and its events have the assumed mark. */

enum sim_ability
{
    SIM_ABILITY_LEADERSHIP,
    SIM_ABILITY_FORCE,
    SIM_ABILITY_INTELLECT,
    SIM_ABILITY_POLITICS,
    SIM_ABILITY_CHARM,
};

internal u32
sim_officer_ability(officer_component *officer, sim_ability ability)
{
    switch (ability)
    {
        case SIM_ABILITY_LEADERSHIP: return officer->leadership;
        case SIM_ABILITY_FORCE:      return officer->war;
        case SIM_ABILITY_INTELLECT:  return officer->intelligence;
        case SIM_ABILITY_POLITICS:   return officer->politics;
        case SIM_ABILITY_CHARM:      return officer->charm;
        default:                     return 0;
    }
}

/* ---- the effect of each command (assumed rules) ------------------------- */

/* ASSUMED (Q-152): the sum of leadership raises public order by
   patrol.leadership_percent of the sum, at least 1. With an enemy within 2
   hexes the rise is smaller (city-commands.md): patrol.enemy_percent of it. */
internal u32
sim_patrol_rise(u32 leadership_sum, b32 enemy_near, sim_rules *rules)
{
    u32 rise = sim_percent_of(leadership_sum, rules->patrol_leadership_percent);
    if (rise < 1) rise = 1;
    if (enemy_near)
    {
        rise = sim_percent_of(rise, rules->patrol_enemy_percent);
        if (rise < 1) rise = 1;
    }
    return rise;
}

/* city-commands.md "Conscription yield": floor(F x (1000 + 5.6 x S)). S is
   the charm sum and F the factor of the barracks level (1.0, 1.2, 1.5). The
   rules hold 1000 and 56 tenths. The rule fits the dialogs of the sample
   within 7 soldiers. With an enemy within 2 hexes fewer soldiers come:
   conscript.enemy_percent of them (assumed, Q-153). */
internal u32
sim_conscript_soldiers(u32 charm_sum, u32 level, b32 enemy_near, sim_rules *rules)
{
    u64 tenths   = (u64)rules->conscript_base_soldiers * 10 + (u64)charm_sum * rules->conscript_charm_tenths;
    u32 soldiers = (u32)(tenths * sim_level_percent(level, rules) / 1000);
    return enemy_near ? sim_percent_of(soldiers, rules->conscript_enemy_percent) : soldiers;
}

/* The technique points of one 徵兵: technique.conscript_min, plus one for
   each technique.conscript_soldiers soldiers, at most technique.conscript_max
   (Q-158: 2; G2-014: 5 for 3439 soldiers; the steps are assumed). */
internal u32
sim_conscript_technique(u32 soldiers, sim_rules *rules)
{
    u32 points = rules->technique_conscript_min + (rules->technique_conscript_soldiers ? soldiers / rules->technique_conscript_soldiers : 0);
    return points < rules->technique_conscript_max ? points : rules->technique_conscript_max;
}

/* ASSUMED (Q-154): the sum of force raises morale by train.force_percent of
   the sum, at least 1. A drill ground gives a larger effect (manual p.29):
   train.drill_percent of it. */
internal u32
sim_train_rise(u32 force_sum, b32 drill_ground, sim_rules *rules)
{
    u32 rise = sim_percent_of(force_sum, rules->train_force_percent);
    if (rise < 1) rise = 1;
    return drill_ground ? sim_percent_of(rise, rules->train_drill_percent) : rise;
}

/* city-commands.md "Merchant rate": the trade effect E = 400 / (450 - P)
   from the politics P of the officer. The dialog shows E as a whole percent
   (P 99 shows 113). The rules hold 400 and 450. */
internal u32
sim_merchant_divisor(u32 politics, sim_rules *rules)
{
    return rules->merchant_effect_offset > politics ? rules->merchant_effect_offset - politics : 1;
}

internal u32
sim_merchant_percent(u32 politics, sim_rules *rules)
{
    return rules->merchant_effect_scale * 100 / sim_merchant_divisor(politics, rules);
}

/* A purchase for G gold gives floor(G x R x E) grain at the city price R
   (observed: 32890 gold at R 3 and P 99 gave 112444 grain). */
internal u32
sim_merchant_grain_for_gold(u32 gold, u32 politics, u32 price, sim_rules *rules)
{
    return (u32)((u64)gold * price * rules->merchant_effect_scale / sim_merchant_divisor(politics, rules));
}

/* A sale of N grain gives floor(N x E x 0.8 / R) gold: a sale loses 20
   percent (observed: 67447 grain at R 3 and P 99 gave 20496 gold). */
internal u32
sim_merchant_gold_for_grain(u32 grain, u32 politics, u32 price, sim_rules *rules)
{
    if (!price) return 0;
    return (u32)((u64)grain * rules->merchant_effect_scale * rules->merchant_sale_percent /
                 ((u64)sim_merchant_divisor(politics, rules) * price * 100));
}

/* Day 1 of each month: the price R of each city changes by -1, 0 or +1 with
   equal chance and stays from merchant.price_min to merchant.price_max
   (city-commands.md; assumed, a decision of the player, Q-155). */
internal void
sim_run_prices_month(sim_state *sim, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    if (rules->merchant_price_max <= rules->merchant_price_min) return;
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        city_component *city = sim->cities.data + index;
        s32 change = (s32)sim_random_below(sim, 3) - 1;
        s32 price  = (s32)city->price + change;
        if (price < (s32)rules->merchant_price_min) price = (s32)rules->merchant_price_min;
        if (price > (s32)rules->merchant_price_max) price = (s32)rules->merchant_price_max;
        if ((u8)price == city->price) continue;
        change = price - (s32)city->price;
        city->price = (u8)price;
        sim_emit_city(sim, SIM_EVENT_PRICE_CHANGED, city, change, (u32)price, true, cause, events);
    }
}

/* ASSUMED (Q-023, Q-156): produce.intellect_quantity weapons for each point
   of the intellect sum, times the level of the facility. 能吏 and 繁殖
   raise the quantity (manual p.12): produce.skill_percent of it. */
internal u32
sim_produce_quantity(u32 intellect_sum, u32 level, b32 skill, sim_rules *rules)
{
    u32 quantity = sim_percent_of(intellect_sum * rules->produce_intellect_quantity, sim_level_percent(level, rules));
    return skill ? sim_percent_of(quantity, rules->produce_skill_percent) : quantity;
}

/* ASSUMED LINE (Q-156). A siege weapon or a ship takes 30 to 90 days
   (documented); the sum of intellect shortens it. The line goes from the
   longest period at a sum of 0 to the shortest at produce.full_intellect.
   發明 and 造船 halve the time (skills.md). The result goes up to whole
   turns. A weapon that arrives at once has no period. */
internal u32
sim_produce_days(u32 intellect_sum, u32 min_days, u32 max_days, b32 skill, sim_rules *rules)
{
    if (!max_days) return 0;
    u32 days = max_days;
    if (rules->produce_full_intellect)
    {
        u32 sum = intellect_sum < rules->produce_full_intellect ? intellect_sum : rules->produce_full_intellect;
        days = max_days - (max_days - min_days) * sum / rules->produce_full_intellect;
    }
    if (skill && rules->produce_skill_period_percent) days = sim_percent_of(days, rules->produce_skill_period_percent);
    u32 turns = calendar_turns_for_days(days);
    return (turns ? turns : 1) * CALENDAR_DAYS_PER_TURN;
}

/* ASSUMED (Q-022, Q-156): a specialty city pays produce.specialty_percent of
   the cost of its weapon (manual p.8: "cheaper"). */
internal u32
sim_weapon_cost(u32 cost, b32 specialty, sim_rules *rules)
{
    return specialty && rules->produce_specialty_percent ? sim_percent_of(cost, rules->produce_specialty_percent) : cost;
}

/* ---- the checks --------------------------------------------------------- */

/* The checks of the officers of an order, as for the develop command. The
   sum of one ability is in *sum. */
internal sim_refusal
sim_check_city_officers(sim_state *sim, city_component *city, const u16 *officers, u32 max_officers,
                        sim_ability ability, u32 *sum)
{
    u32 named = 0;
    *sum = 0;
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
    {
        u16 officer_id = officers[place];
        if (officer_id == SIM_NO_OFFICER) continue;
        named++;
        officer_component *officer = sim_find_officer(sim, officer_id);
        sim_refusal refusal = sim_check_develop_officer(sim, city, officer);
        if (refusal != SIM_REFUSAL_NONE) return refusal;
        for (u32 earlier = 0; earlier < place; ++earlier)
        {
            if (officers[earlier] == officer_id) return SIM_REFUSAL_DUPLICATE_OFFICER;
        }
        *sum += sim_officer_ability(officer, ability);
    }
    if (named == 0)           return SIM_REFUSAL_NO_OFFICER;
    if (named > max_officers) return SIM_REFUSAL_TOO_MANY_OFFICERS;
    return SIM_REFUSAL_NONE;
}

/* True if an officer of the order has the skill (domestic skills apply when
   the officer does the command; skills.md). */
internal b32
sim_order_has_skill(sim_state *sim, const u16 *officers, u32 skill)
{
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
    {
        if (sim_officer_has_skill(sim_find_officer(sim, officers[place]), skill)) return true;
    }
    return false;
}

/* The built facility with this effect that no command used in this turn
   and that has no job, with the highest level first. SIM_ANY_SLOT if none;
   *any_built says if the city has one at all. */
internal u32
sim_free_facility_slot(sim_state *sim, u8 city_id, facility_effect effect, b32 *any_built)
{
    sim_city_site *site = sim->sites + city_id;
    u32 best = SIM_ANY_SLOT;
    *any_built = false;
    for (u32 slot_index = 0; slot_index < site->slot_count; ++slot_index)
    {
        sim_slot *slot = site->slots + slot_index;
        if (slot->state != SIM_SLOT_BUILT || slot->facility >= sim->content.facility_count ||
            sim->content.facilities[slot->facility].effect != effect) continue;
        *any_built = true;
        if (slot->used_turn == sim->turn + 1 || slot->job_weapon != CONTENT_NO_WEAPON) continue;
        if (best == SIM_ANY_SLOT || slot->level > site->slots[best].level) best = slot_index;
    }
    return best;
}

internal u32
sim_add_within(u32 value, u32 amount, u32 cap)
{
    u64 total = (u64)value + amount;
    return total < cap ? (u32)total : cap;
}

internal sim_check
sim_check_patrol(sim_state *sim, city_component *city, sim_city_order *order, sim_check check)
{
    sim_rules *rules = &sim->content.rules;
    if (city->patrolled_turn == sim->turn + 1)    return sim_refuse(check, SIM_REFUSAL_LIMIT_REACHED);
    if (city->public_order >= rules->order_max)   return sim_refuse(check, SIM_REFUSAL_VALUE_AT_MAXIMUM);
    u32 leadership = 0;
    sim_refusal refusal = sim_check_city_officers(sim, city, order->officers, SIM_COMMAND_OFFICERS, SIM_ABILITY_LEADERSHIP, &leadership);
    if (refusal != SIM_REFUSAL_NONE)              return sim_refuse(check, refusal);
    if (sim->turns.action_points[city->owner_force_id] < rules->ap_patrol) return sim_refuse(check, SIM_REFUSAL_NO_ACTION_POINTS);
    if (city->gold < rules->patrol_gold)          return sim_refuse(check, SIM_REFUSAL_NO_GOLD);
    check.gold          = rules->patrol_gold;
    check.action_points = rules->ap_patrol;
    check.value_before  = city->public_order;
    check.value_after   = sim_add_within(city->public_order, sim_patrol_rise(leadership, sim_enemy_near(sim, city), rules), rules->order_max);
    check.quantity      = check.value_after - check.value_before;
    return check;
}

internal sim_check
sim_check_conscript(sim_state *sim, city_component *city, sim_city_order *order, sim_check check)
{
    sim_rules *rules = &sim->content.rules;
    b32 any_built = false;
    u32 slot = sim_free_facility_slot(sim, city->city_id, FACILITY_EFFECT_CONSCRIPTION, &any_built);
    if (!any_built)                               return sim_refuse(check, SIM_REFUSAL_NO_FACILITY);
    if (slot == SIM_ANY_SLOT)                     return sim_refuse(check, SIM_REFUSAL_LIMIT_REACHED);
    u32 cap = sim_city_soldier_cap(sim, city->city_id);
    if (city->troops >= cap)                      return sim_refuse(check, SIM_REFUSAL_VALUE_AT_MAXIMUM);
    u32 charm = 0;
    sim_refusal refusal = sim_check_city_officers(sim, city, order->officers, SIM_COMMAND_OFFICERS, SIM_ABILITY_CHARM, &charm);
    if (refusal != SIM_REFUSAL_NONE)              return sim_refuse(check, refusal);
    if (sim->turns.action_points[city->owner_force_id] < rules->ap_conscript) return sim_refuse(check, SIM_REFUSAL_NO_ACTION_POINTS);
    if (city->gold < rules->conscript_gold)       return sim_refuse(check, SIM_REFUSAL_NO_GOLD);
    u32 level = sim->sites[city->city_id].slots[slot].level;
    check.gold          = rules->conscript_gold;
    check.action_points = rules->ap_conscript;
    check.slot          = (u8)slot;
    check.value_before  = city->troops;
    u32 soldiers = sim_conscript_soldiers(charm, level, sim_enemy_near(sim, city), rules);
    if (sim_order_has_skill(sim, order->officers, rules->skill_fame) && rules->skill_fame_percent) soldiers = sim_percent_of(soldiers, rules->skill_fame_percent);   /* 名聲 #80 */
    check.value_after   = sim_add_within(city->troops, soldiers, cap);
    check.quantity      = check.value_after - check.value_before;
    return check;
}

/* The action points of a military command (軍事: 訓練, 出征, 輸送). A
   military office (軍事府, PK) in the city halves them (manual p.29). */
internal u32
sim_military_ap(sim_state *sim, city_component *city, u32 action_points)
{
    sim_rules *rules = &sim->content.rules;
    if (!rules->military_office_ap_percent || !sim_city_has_built(sim, city->city_id, FACILITY_EFFECT_MILITARY)) return action_points;
    return sim_percent_of(action_points, rules->military_office_ap_percent);
}

internal sim_check
sim_check_train(sim_state *sim, city_component *city, sim_city_order *order, sim_check check)
{
    sim_rules *rules = &sim->content.rules;
    u32 morale_max = sim_city_morale_max(sim, city);   /* 120 with 熟練兵 */
    if (city->trained_turn == sim->turn + 1)      return sim_refuse(check, SIM_REFUSAL_LIMIT_REACHED);
    if (city->morale >= morale_max)               return sim_refuse(check, SIM_REFUSAL_VALUE_AT_MAXIMUM);
    u32 force = 0;
    sim_refusal refusal = sim_check_city_officers(sim, city, order->officers, SIM_COMMAND_OFFICERS, SIM_ABILITY_FORCE, &force);
    if (refusal != SIM_REFUSAL_NONE)              return sim_refuse(check, refusal);
    u32 action_points = sim_military_ap(sim, city, rules->ap_train);
    if (sim->turns.action_points[city->owner_force_id] < action_points) return sim_refuse(check, SIM_REFUSAL_NO_ACTION_POINTS);
    b32 drill = sim_city_has_built(sim, city->city_id, FACILITY_EFFECT_DRILL);
    check.action_points = action_points;
    check.value_before  = city->morale;
    check.value_after   = sim_add_within(city->morale, sim_train_rise(force, drill, rules), morale_max);
    check.quantity      = check.value_after - check.value_before;
    return check;
}

internal sim_check
sim_check_merchant(sim_state *sim, city_component *city, sim_city_order *order, sim_check check)
{
    sim_rules *rules = &sim->content.rules;
    if (sim_enemy_near(sim, city))                return sim_refuse(check, SIM_REFUSAL_ENEMY_NEAR);
    if (order->mode >= SIM_MERCHANT_MODE_COUNT || order->amount == 0) return sim_refuse(check, SIM_REFUSAL_NO_AMOUNT);
    b32 buy = order->mode == SIM_MERCHANT_BUY;
    if (buy ? city->food >= rules->city_grain_cap : city->gold >= rules->city_gold_cap)
    {
        return sim_refuse(check, SIM_REFUSAL_VALUE_AT_MAXIMUM);
    }
    u32 politics = 0;
    sim_refusal refusal = sim_check_city_officers(sim, city, order->officers, 1, SIM_ABILITY_POLITICS, &politics);
    if (refusal != SIM_REFUSAL_NONE)              return sim_refuse(check, refusal);
    if (!buy && city->food < order->amount)       return sim_refuse(check, SIM_REFUSAL_NO_GRAIN);
    if (sim->turns.action_points[city->owner_force_id] < rules->ap_merchant) return sim_refuse(check, SIM_REFUSAL_NO_ACTION_POINTS);
    if (buy && city->gold < order->amount)        return sim_refuse(check, SIM_REFUSAL_NO_GOLD);
    check.action_points = rules->ap_merchant;
    check.value_before  = city->food;
    check.effect_percent = sim_merchant_percent(politics, rules);
    if (buy)
    {
        check.gold        = order->amount;
        check.quantity    = sim_merchant_grain_for_gold(order->amount, politics, city->price, rules);
        check.value_after = sim_add_within(city->food, check.quantity, rules->city_grain_cap);
    }
    else
    {
        check.quantity    = order->amount;
        check.gold_gain   = sim_merchant_gold_for_grain(order->amount, politics, city->price, rules);
        check.value_after = city->food - order->amount;
    }
    return check;
}

internal sim_check
sim_check_produce(sim_state *sim, city_component *city, sim_city_order *order, sim_check check)
{
    sim_rules *rules = &sim->content.rules;
    if (order->weapon >= sim->content.weapon_count) return sim_refuse(check, SIM_REFUSAL_NO_FACILITY);
    weapon_kind *weapon = sim->content.weapons + order->weapon;
    sim_refusal technique = sim_check_weapon_technique(sim, city->owner_force_id, weapon);
    if (technique != SIM_REFUSAL_NONE)            return sim_refuse(check, technique);
    b32 any_built = false;
    u32 slot = sim_free_facility_slot(sim, city->city_id, weapon->facility, &any_built);
    if (!any_built)                               return sim_refuse(check, SIM_REFUSAL_NO_FACILITY);
    if (slot == SIM_ANY_SLOT)                     return sim_refuse(check, SIM_REFUSAL_LIMIT_REACHED);
    u32 stock = city->equipment_stocks[weapon->stock];
    if (stock >= rules->city_weapon_cap)          return sim_refuse(check, SIM_REFUSAL_VALUE_AT_MAXIMUM);
    u32 intellect = 0;
    sim_refusal refusal = sim_check_city_officers(sim, city, order->officers, SIM_COMMAND_OFFICERS, SIM_ABILITY_INTELLECT, &intellect);
    if (refusal != SIM_REFUSAL_NONE)              return sim_refuse(check, refusal);
    if (sim->turns.action_points[city->owner_force_id] < rules->ap_produce) return sim_refuse(check, SIM_REFUSAL_NO_ACTION_POINTS);
    b32 specialty = city->city_id < sim->content.city_count && sim->content.cities[city->city_id].specialty == weapon->specialty;
    u32 cost = sim_weapon_cost(weapon->cost, specialty, rules);
    if (city->gold < cost)                        return sim_refuse(check, SIM_REFUSAL_NO_GOLD);

    check.gold          = cost;
    check.action_points = rules->ap_produce;
    check.slot          = (u8)slot;
    check.value_before  = stock;
    if (weapon->max_days == 0)
    {
        u32 skill = weapon->facility == FACILITY_EFFECT_HORSES ? rules->skill_breeding : rules->skill_capable;
        u32 level = sim->sites[city->city_id].slots[slot].level;
        check.quantity = sim_produce_quantity(intellect, level, sim_order_has_skill(sim, order->officers, skill), rules);
    }
    else
    {
        u32 skill = weapon->facility == FACILITY_EFFECT_SHIPS ? rules->skill_shipbuilding : rules->skill_invention;
        check.days     = sim_produce_days(intellect, weapon->min_days, weapon->max_days, sim_order_has_skill(sim, order->officers, skill), rules);
        /* A drill ground (鍊兵所, PK) shortens ships (manual p.29; assumed amount, Q-164). */
        if (weapon->facility == FACILITY_EFFECT_SHIPS && rules->drill_ship_period_percent &&
            sim_city_has_built(sim, city->city_id, FACILITY_EFFECT_DRILL))
        {
            check.days = sim_round_to_turns(sim_percent_of(check.days, rules->drill_ship_period_percent));
        }
        check.turns    = check.days / CALENDAR_DAYS_PER_TURN;
        check.quantity = 1;
    }
    check.value_after = sim_add_within(stock, check.quantity, rules->city_weapon_cap);
    return check;
}

/* The checks of the five commands. The common checks come first, as for the
   develop command; then the limit of the turn, the maximum, the officers,
   the action points and the gold. The first reason that holds is the
   answer. The spec does not give the sequence (city-commands.md). */
internal sim_check
sim_check_city_command(sim_state *sim, sim_command_kind kind, sim_city_order *order)
{
    sim_check check = {};
    check.slot    = SIM_ANY_SLOT;
    check.assumed = true;
    if (!sim->has_content) return sim_refuse(check, SIM_REFUSAL_NO_CONTENT);
    if (sim_game_over(sim)) return sim_refuse(check, SIM_REFUSAL_GAME_OVER);
    if (sim_busy(sim)) return sim_refuse(check, SIM_REFUSAL_BUSY);   /* M7: a duel or a debate runs */
    city_component *city = sim_find_city(sim, order->city_id);
    if (!city || city->owner_force_id == SIM_NO_FORCE) return sim_refuse(check, SIM_REFUSAL_NO_CITY);
    if (sim_whose_turn(sim) != city->owner_force_id)    return sim_refuse(check, SIM_REFUSAL_NOT_YOUR_TURN);
    switch (kind)
    {
        case SIM_COMMAND_PATROL:    return sim_check_patrol(sim, city, order, check);
        case SIM_COMMAND_CONSCRIPT: return sim_check_conscript(sim, city, order, check);
        case SIM_COMMAND_TRAIN:     return sim_check_train(sim, city, order, check);
        case SIM_COMMAND_MERCHANT:  return sim_check_merchant(sim, city, order, check);
        case SIM_COMMAND_PRODUCE:   return sim_check_produce(sim, city, order, check);
        default:                    return sim_refuse(check, SIM_REFUSAL_NO_CITY);
    }
}

/* ---- the commands ------------------------------------------------------- */

/* Takes a loss from a value of the city, down to 0, with an event. */
internal void
sim_city_lose(sim_state *sim, city_component *city, u8 *value, u32 loss, sim_event_kind kind, u32 cause, sim_event_sink *events)
{
    u32 lost = loss < *value ? loss : *value;
    *value = (u8)(*value - lost);
    if (lost) sim_emit_city(sim, kind, city, -(s32)lost, 0, true, cause, events);
}

/* Starts a production job of a workshop or a shipyard: the facility and
   the officers are busy for the period (assumed for the facility, Q-156). */
internal void
sim_start_production(sim_state *sim, city_component *city, sim_city_order *order, sim_check *check, u32 cause, sim_event_sink *events)
{
    sim_slot *slot = sim->sites[city->city_id].slots + check->slot;
    slot->job_weapon = order->weapon;
    slot->turns_left = (u16)check->turns;
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
    {
        slot->officers[place] = order->officers[place];
        officer_component *officer = sim_find_officer(sim, order->officers[place]);
        if (!officer) continue;
        officer->job_city = city->city_id;
        officer->job_slot = check->slot;
    }
    sim_emit_city(sim, SIM_EVENT_PRODUCTION_STARTED, city, (s32)check->turns, order->weapon, true, cause, events);
}

/* Applies the effect of a checked command to the city. */
internal void
sim_apply_city_effect(sim_state *sim, sim_command_kind kind, city_component *city, sim_city_order *order, sim_check *check,
                      u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    switch (kind)
    {
        case SIM_COMMAND_PATROL:
            city->public_order   = (u8)check->value_after;
            city->patrolled_turn = sim->turn + 1;
            sim_emit_city(sim, SIM_EVENT_PATROLLED, city, (s32)check->quantity, 0, true, cause, events);
            break;
        case SIM_COMMAND_CONSCRIPT:
            city->troops = check->value_after;
            sim->sites[city->city_id].slots[check->slot].used_turn = sim->turn + 1;
            sim_emit_city(sim, SIM_EVENT_CONSCRIPTED, city, (s32)check->quantity, check->slot, true, cause, events);
            sim_city_lose(sim, city, &city->public_order, sim_order_loss(sim, city->owner_force_id, rules->conscript_order_loss),
                          SIM_EVENT_ORDER_LOST, cause, events);
            sim_city_lose(sim, city, &city->morale, rules->conscript_morale_loss, SIM_EVENT_MORALE_LOST, cause, events);
            break;
        case SIM_COMMAND_TRAIN:
            city->morale       = (u8)check->value_after;
            city->trained_turn = sim->turn + 1;
            sim_emit_city(sim, SIM_EVENT_TRAINED, city, (s32)check->quantity, 0, true, cause, events);
            break;
        case SIM_COMMAND_MERCHANT:
            if (order->mode == SIM_MERCHANT_BUY)
            {
                sim_city_receive(sim, city, &city->food, rules->city_grain_cap, check->quantity, SIM_EVENT_GRAIN_BOUGHT,
                                 SIM_EVENT_GRAIN_OVER_CAP, true, cause, events);
            }
            else
            {
                city->food -= check->quantity;
                sim_emit_city(sim, SIM_EVENT_GRAIN_SOLD, city, -(s32)check->quantity, 0, false, cause, events);
                sim_city_receive(sim, city, &city->gold, rules->city_gold_cap, check->gold_gain, SIM_EVENT_GOLD_EARNED,
                                 SIM_EVENT_GOLD_OVER_CAP, true, cause, events);
            }
            break;
        case SIM_COMMAND_PRODUCE:
            sim->sites[city->city_id].slots[check->slot].used_turn = sim->turn + 1;
            if (check->turns)
            {
                sim_start_production(sim, city, order, check, cause, events);
            }
            else
            {
                city->equipment_stocks[sim->content.weapons[order->weapon].stock] = check->value_after;
                sim_emit_city(sim, SIM_EVENT_WEAPONS_PRODUCED, city, (s32)(check->value_after - check->value_before), order->weapon,
                              true, cause, events);
            }
            break;
        default:
            break;
    }
}

/* The technique points of a city command (tech-tree.md, the table by
   action; G2-014): 巡察 3, 訓練 3, 徵兵 2 to 5 by the soldiers, 商人 0,
   生產兵裝 5 for a weapon and 10 for a siege weapon or a ship. */
internal u32
sim_city_command_technique(sim_state *sim, sim_command_kind kind, sim_city_order *order, sim_check *check)
{
    sim_rules *rules = &sim->content.rules;
    switch (kind)
    {
        case SIM_COMMAND_PATROL:    return rules->technique_patrol;
        case SIM_COMMAND_TRAIN:     return rules->technique_train;
        case SIM_COMMAND_CONSCRIPT: return sim_conscript_technique(check->quantity, rules);
        case SIM_COMMAND_MERCHANT:  return rules->technique_merchant;
        case SIM_COMMAND_PRODUCE:
        {
            weapon_kind *weapon = order->weapon < sim->content.weapon_count ? sim->content.weapons + order->weapon : 0;
            if (!weapon) return 0;
            if (weapon->facility == FACILITY_EFFECT_SHIPS)  return rules->technique_produce_ship;
            if (weapon->max_days)                           return rules->technique_produce_siege;
            return rules->technique_produce_weapon;
        }
        default: return 0;
    }
}

/* Runs one of the five commands. It checks first: a refused order changes
   nothing. */
internal void
sim_city_command(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_city_order *order = &command.city;
    sim_check check = sim_check_city_command(sim, command.kind, order);
    if (check.refusal != SIM_REFUSAL_NONE) return;
    city_component *city = sim_find_city(sim, order->city_id);
    u8 force = city->owner_force_id;

    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
    {
        officer_component *officer = sim_find_officer(sim, order->officers[place]);
        if (officer) officer->acted_turn = sim->turn + 1;
    }
    sim_apply_city_effect(sim, command.kind, city, order, &check, command.id, events);
    if (check.gold)
    {
        city->gold -= check.gold;
        sim_emit_city(sim, SIM_EVENT_GOLD_SPENT, city, -(s32)check.gold, 0, false, command.id, events);
    }
    sim->turns.action_points[force] -= check.action_points;
    sim_event points = sim_event_make(sim, SIM_EVENT_ACTION_POINTS_SPENT, command.id);
    points.faction = force;
    points.delta   = -(s32)check.action_points;
    sim_emit(events, points);
    sim_reward_command(sim, force, order->officers, sim_city_command_technique(sim, command.kind, order, &check), command.id, events);
    /* M7: the command gives experience to the ability it uses (officers-and-relations.md). */
    u32 ability = command.kind == SIM_COMMAND_PATROL    ? CONTENT_ABILITY_LEADERSHIP :
                  command.kind == SIM_COMMAND_CONSCRIPT ? CONTENT_ABILITY_CHARM :
                  command.kind == SIM_COMMAND_TRAIN     ? CONTENT_ABILITY_FORCE :
                  command.kind == SIM_COMMAND_MERCHANT  ? CONTENT_ABILITY_POLITICS : CONTENT_ABILITY_INTELLECT;
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
    {
        sim_gain_experience(sim, sim_find_officer(sim, order->officers[place]), ability, sim->content.rules.experience_command, command.id, events);
    }
}
