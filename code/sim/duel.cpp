/* The duel of M7 (duel.md): the challenge and its acceptance, the four
   policies, the gauges of stamina and fighting spirit, the special
   commands, the items, the substitutes and the result. Included by sim.cpp
   after diplomacy.cpp.

   The spec gives the rules but no damage formula: each number is an
   assumed rule of rules.tsv (Q-037), and the strike events say so. Side 0
   is the challenger. The commands of the acting faction drive side 0; side
   1 follows the policy of its personality and uses its special commands
   as soon as it can. */

internal officer_component *
sim_duel_fighter(sim_state *sim, sim_duel_side *side)
{
    return sim_find_officer(sim, side->officers[side->fighter]);
}

/* The policy of a computer side: the personality (guide book p.48). */
internal u8
sim_duel_policy_of(officer_component *officer)
{
    switch (officer ? officer->personality : PERSONALITY_CAREFUL)
    {
        case PERSONALITY_RECKLESS: return DUEL_ONE_BLOW;
        case PERSONALITY_BOLD:     return DUEL_ATTACK;
        case PERSONALITY_CALM:     return DUEL_SPIRIT;
        default:                   return DUEL_DEFENCE;
    }
}

/* The chance in percent that the target accepts (duel.md, "Start"). */
internal u32
sim_duel_accept_chance(sim_state *sim, officer_component *challenger, officer_component *target, sim_unit *target_unit)
{
    sim_rules *rules = &sim->content.rules;
    u32 chance = rules->duel_accept[target->personality < PERSONALITY_COUNT ? target->personality : 0];
    if (target->war < challenger->war) chance = sim_percent_of(chance, rules->duel_lower_force_percent);
    if (target_unit->state == UNIT_STATE_CONFUSED) chance += 30;
    if (sim_officer_dislikes(target, challenger->officer_id)) chance += 30;
    if (sim_building_bonus(sim, challenger->force_id, target_unit->hex, BUILDING_DRUM)) chance += 10;   /* 太鼓台 (PK) */
    return Minimum(chance, 100u);
}

internal sim_check
sim_check_duel(sim_state *sim, sim_unit_order *order)
{
    sim_check check = {};
    check.slot = SIM_ANY_SLOT;
    if (sim->duel.active || sim->debate.active) return sim_refuse(check, SIM_REFUSAL_BUSY);
    sim_unit *unit = 0;
    sim_refusal refusal = sim_check_combat_unit(sim, order, &unit);
    if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
    u32 target_id = sim_unit_at(sim, order->to);
    sim_unit *target = target_id != SIM_NO_UNIT ? sim->units + target_id : 0;
    if (!target || hex_distance(unit->hex, order->to) != 1 || !sim_at_war(sim, unit->force_id, target->force_id)) return sim_refuse(check, SIM_REFUSAL_NO_TARGET);
    officer_component *challenger = sim_find_officer(sim, unit->officers[0]);
    officer_component *defender   = sim_find_officer(sim, target->officers[0]);
    if (!challenger || !defender) return sim_refuse(check, SIM_REFUSAL_NO_OFFICER);
    if (!challenger->stamina) return sim_refuse(check, SIM_REFUSAL_NO_SPIRIT);
    check.quantity = sim_duel_accept_chance(sim, challenger, defender, target);
    check.assumed  = true;
    return check;
}

internal void
sim_duel_setup_side(sim_state *sim, sim_duel_side *side, u32 unit_id, b32 human)
{
    sim_unit *unit = sim->units + unit_id;
    *side = {};
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place) side->officers[place] = unit->officers[place];
    side->unit   = unit_id;
    side->force  = unit->force_id;
    side->human  = human;
    side->policy = human ? (u8)DUEL_SPIRIT : sim_duel_policy_of(sim_duel_fighter(sim, side));
}

/* 單挑: the challenge. The unit acts; the target accepts or refuses. */
internal void sim_duel_auto(sim_state *sim, u32 cause, sim_event_sink *events);

internal void
sim_start_duel(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_unit_order *order = &command.unit;
    sim_check check = sim_check_duel(sim, order);
    if (check.refusal != SIM_REFUSAL_NONE) return;
    sim_unit *unit = sim->units + order->unit;
    u32 target_id = sim_unit_at(sim, order->to);
    unit->acted_turn = sim->turn + 1;
    officer_component *challenger = sim_find_officer(sim, unit->officers[0]);
    officer_component *defender   = sim_find_officer(sim, sim->units[target_id].officers[0]);
    b32 accepted = check.quantity >= 100 || sim_random_below(sim, 100) < check.quantity;
    sim_event event = sim_event_make(sim, accepted ? SIM_EVENT_DUEL_STARTED : SIM_EVENT_DUEL_REFUSED, command.id);
    event.subject = sim_officer_entity(sim, challenger->officer_id);
    event.faction = unit->force_id;
    event.detail  = defender->officer_id;
    event.assumed = true;
    sim_emit(events, event);
    if (!accepted) return;
    sim_duel *duel = &sim->duel;
    *duel = {};
    duel->active = true;
    duel->cause  = command.id;
    duel->winner = 2;
    /* The player plays side 0, the side of the screen that takes the
       commands (play report of 2026-10-23: a duel of two computer officers
       opened the screen for the player). When the computer challenges the
       player, the sides swap. A duel of two computer officers runs to its
       end at once, and the player sees only the result in the log. */
    b32 challenger_human = unit->force_id < SIM_FORCE_CAPACITY && sim->turns.human[unit->force_id];
    b32 defender_human   = sim->units[target_id].force_id < SIM_FORCE_CAPACITY && sim->turns.human[sim->units[target_id].force_id];
    if (!challenger_human && defender_human)
    {
        sim_duel_setup_side(sim, duel->side + 0, target_id, true);
        sim_duel_setup_side(sim, duel->side + 1, order->unit, false);
    }
    else
    {
        sim_duel_setup_side(sim, duel->side + 0, order->unit, challenger_human);
        sim_duel_setup_side(sim, duel->side + 1, target_id, defender_human);
    }
    /* M9: 單挑・舌戰 semi-automatic or automatic: the computer plays the rounds. */
    if (sim->settings.duel_mode != DUEL_MODE_MANUAL || (!challenger_human && !defender_human)) sim_duel_auto(sim, command.id, events);
}

/* ---- one strike and one round ---------------------------------------------------- */

internal u32
sim_duel_spirit_cost(u8 special)
{
    switch (special)
    {
        case DUEL_FINISHER: return 100;
        case DUEL_FOCUS:
        case DUEL_HOLD:     return 50;
        case DUEL_VITAL:    return 200;
        case DUEL_PEERLESS: return 300;
        default:            return 0;
    }
}

/* True if the side can use the special command now. */
internal b32
sim_duel_special_ready(sim_state *sim, sim_duel_side *side, u8 special)
{
    officer_component *fighter = sim_duel_fighter(sim, side);
    if (!fighter || special >= DUEL_SPECIAL_COUNT) return false;
    if (special == DUEL_HIDDEN) return !side->used_hidden && sim_officer_item(sim, fighter->officer_id, ITEM_HIDDEN_WEAPON);
    if (special == DUEL_FEINT)  return !side->used_feint && sim_officer_item(sim, fighter->officer_id, ITEM_BOW);
    return side->spirit >= sim_duel_spirit_cost(special);
}

/* Takes stamina from the fighter of the side. */
internal void
sim_duel_hurt(sim_state *sim, sim_duel_side *side, u32 damage, u32 cause, sim_event_sink *events)
{
    officer_component *fighter = sim_duel_fighter(sim, side);
    if (!fighter || !damage) return;
    u32 lost = Minimum(damage, (u32)fighter->stamina);
    fighter->stamina = (u8)(fighter->stamina - lost);
    sim_emit_officer(sim, SIM_EVENT_DUEL_STRIKE, fighter, -(s32)lost, sim->duel.round, true, cause, events);
}

/* A wound: one step worse health, unless 強運 or 護衛 in the unit. */
internal void
sim_wound_officer(sim_state *sim, officer_component *officer, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    if (!officer || sim_officer_has_skill(officer, rules->skill_fortune) || officer->health >= HEALTH_NEAR_DEATH) return;
    if (officer->current_position >= SIM_UNIT_POSITION_BASE)
    {
        sim_unit *unit = sim_find_unit(sim, officer->current_position - SIM_UNIT_POSITION_BASE);
        if (unit && sim_unit_has_skill(sim, unit->officers, rules->skill_guard)) return;
    }
    officer->health++;
    officer->health_turns = (u8)rules->wound_turns;
    sim_emit_officer(sim, SIM_EVENT_HEALTH_CHANGED, officer, officer->health, 0, true, cause, events);
}

/* The damage of one strike of a side on the other (assumed, Q-037). */
internal u32
sim_duel_strike_damage(sim_state *sim, sim_duel_side *striker, sim_duel_side *target, b32 *critical)
{
    sim_rules *rules = &sim->content.rules;
    officer_component *a = sim_duel_fighter(sim, striker), *b = sim_duel_fighter(sim, target);
    if (!a || !b || target->guard_rounds) return 0;
    u32 force_a = Maximum(1u, (u32)a->war * (100 - 10 * a->health) / 100), force_b = Maximum(1u, (u32)b->war * (100 - 10 * b->health) / 100);
    u32 percent = 100 * force_a / force_b;
    if (striker->policy == DUEL_ATTACK)  percent = sim_percent_of(percent, rules->duel_attack_percent);
    if (striker->policy == DUEL_DEFENCE) percent = sim_percent_of(percent, rules->duel_defence_percent);
    if (striker->policy == DUEL_ONE_BLOW)
    {
        percent = sim_random_below(sim, 100) < rules->duel_one_blow_chance ? sim_percent_of(percent, rules->duel_one_blow_percent) : percent / 2;
    }
    if (target->policy == DUEL_ATTACK)  percent = sim_percent_of(percent, rules->duel_attack_percent);
    if (target->policy == DUEL_DEFENCE) percent = sim_percent_of(percent, rules->duel_defence_percent);
    if (striker->focus_rounds) percent = sim_percent_of(percent, 100 + rules->duel_boost_percent);
    if (target->hold_rounds)   percent = sim_percent_of(percent, 100 - Minimum(rules->duel_boost_percent, 100u));
    sim_item *polearm = sim_officer_item(sim, a->officer_id, ITEM_POLEARM);
    if (polearm) percent = sim_percent_of(percent, 100 + (polearm->polearm == POLEARM_SKY_HALBERD ? rules->duel_sky_halberd_percent : rules->duel_polearm_percent));
    /* To keep one policy long gives its critical (manual p.18); 蛇矛 makes it come sooner. */
    u32 needed = rules->duel_critical_rounds;
    if (polearm && polearm->polearm == POLEARM_SNAKE_SPEAR && needed > 2) needed -= 2;
    *critical = needed && striker->policy_rounds >= needed;
    if (*critical)
    {
        if (striker->policy == DUEL_ATTACK)   percent *= 2;
        if (striker->policy == DUEL_ONE_BLOW) percent *= 3;
    }
    return Maximum(1u, rules->duel_damage * percent / 100);
}

internal void
sim_duel_gain_spirit(sim_state *sim, sim_duel_side *side, b32 hit)
{
    sim_rules *rules = &sim->content.rules;
    u32 rise = rules->duel_spirit_rise;
    if (side->policy == DUEL_SPIRIT) rise *= 2;
    if (side->policy == DUEL_ATTACK) rise /= 2;
    if (hit) rise += rules->duel_spirit_hit * (side->policy == DUEL_SPIRIT ? 2 : 1);
    officer_component *fighter = sim_duel_fighter(sim, side);
    if (fighter && sim_officer_item(sim, fighter->officer_id, ITEM_SWORD)) rise = sim_percent_of(rise, 100 + rules->duel_sword_percent);
    side->spirit = Minimum(side->spirit + rise, 300u);
}

internal void
sim_duel_finish(sim_state *sim, u8 winner, u8 result, u32 cause, sim_event_sink *events);

/* The officer of the side has no stamina: a substitute of the same unit
   enters if one has stamina (duel.md); else the side lost. */
internal b32
sim_duel_side_down(sim_state *sim, sim_duel_side *side)
{
    officer_component *fighter = sim_duel_fighter(sim, side);
    return !fighter || fighter->stamina == 0;
}

/* A computer side: a substitute when its fighter is in danger (a bonded,
   blood or liked officer enters), then the strongest special command. */
internal void
sim_duel_ai(sim_state *sim, sim_duel_side *side, sim_duel_side *enemy, u32 cause, sim_event_sink *events);

internal void
sim_duel_use_special(sim_state *sim, u8 side_index, u8 special, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    sim_duel *duel = &sim->duel;
    sim_duel_side *side = duel->side + side_index, *enemy = duel->side + (1 - side_index);
    officer_component *fighter = sim_duel_fighter(sim, side);
    side->spirit -= sim_duel_spirit_cost(special);
    sim_emit_officer(sim, SIM_EVENT_DUEL_SPECIAL, fighter, 0, special, false, cause, events);
    switch (special)
    {
        case DUEL_FINISHER: sim_duel_hurt(sim, enemy, rules->duel_finisher, cause, events); break;
        case DUEL_FOCUS:    side->focus_rounds = (u8)rules->duel_boost_rounds; break;
        case DUEL_HOLD:     side->hold_rounds  = (u8)rules->duel_boost_rounds; break;
        case DUEL_VITAL:
            sim_duel_hurt(sim, enemy, rules->duel_vital, cause, events);
            sim_wound_officer(sim, sim_duel_fighter(sim, enemy), cause, events);
            break;
        case DUEL_PEERLESS:
            sim_duel_hurt(sim, enemy, rules->duel_peerless, cause, events);
            enemy->focus_rounds = enemy->hold_rounds = enemy->guard_rounds = 0;
            break;
        case DUEL_HIDDEN:
            side->used_hidden = true;
            sim_duel_hurt(sim, enemy, rules->duel_hidden, cause, events);
            enemy->focus_rounds = enemy->hold_rounds = enemy->guard_rounds = 0;
            break;
        case DUEL_FEINT:
            side->used_feint = true;
            sim_duel_hurt(sim, enemy, rules->duel_feint, cause, events);
            sim_wound_officer(sim, sim_duel_fighter(sim, enemy), cause, events);
            break;
        case DUEL_RETREAT:
        {
            officer_component *other = sim_duel_fighter(sim, enemy);
            b32 own_horse = sim_officer_item(sim, fighter->officer_id, ITEM_HORSE) != 0;
            b32 blocked = !own_horse && other && sim_officer_item(sim, other->officer_id, ITEM_HORSE);
            b32 away = own_horse || (!blocked && sim_random_below(sim, 100) < rules->duel_retreat_chance);
            if (away) sim_duel_finish(sim, (u8)(1 - side_index), DUEL_RESULT_RETREAT, cause, events);
            break;
        }
        default: break;
    }
    if (duel->active && sim_duel_side_down(sim, enemy)) sim_duel_finish(sim, side_index, DUEL_RESULT_WIN, cause, events);
}

internal void
sim_duel_ai(sim_state *sim, sim_duel_side *side, sim_duel_side *enemy, u32 cause, sim_event_sink *events)
{
    sim_duel *duel = &sim->duel;
    u8 side_index = (u8)(side - duel->side);
    officer_component *fighter = sim_duel_fighter(sim, side);
    if (fighter && fighter->stamina < 30)
    {
        for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
        {
            officer_component *partner = sim_find_officer(sim, side->officers[place]);
            if (!partner || place == side->fighter || partner->stamina <= fighter->stamina) continue;
            b32 close = sim_officers_bonded(fighter, partner) || sim_officers_blood(fighter, partner) || sim_officer_likes(partner, fighter->officer_id);
            if (!close) continue;
            side->fighter = (u8)place;
            break;
        }
    }
    static const u8 order[] = { DUEL_PEERLESS, DUEL_VITAL, DUEL_HIDDEN, DUEL_FEINT, DUEL_FINISHER };
    for (u32 index = 0; index < ArrayCount(order) && duel->active; ++index)
    {
        if (!sim_duel_special_ready(sim, side, order[index])) continue;
        sim_duel_use_special(sim, side_index, order[index], cause, events);
        break;
    }
    (void)enemy;
}

/* One round: each fighter strikes (青龍偃月刀 first), the gauges move, the
   criticals and the boosts count. */
internal void
sim_duel_round(sim_state *sim, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    sim_duel *duel = &sim->duel;
    if (!duel->active) return;
    duel->round++;
    if (!duel->side[1].human) sim_duel_ai(sim, duel->side + 1, duel->side + 0, cause, events);
    if (!duel->active) return;
    u8 first = 0;
    officer_component *b = sim_duel_fighter(sim, duel->side + 1);
    sim_item *dragon = b ? sim_officer_item(sim, b->officer_id, ITEM_POLEARM) : 0;
    if (dragon && dragon->polearm == POLEARM_DRAGON_BLADE) first = 1;
    for (u32 step = 0; step < 2 && duel->active; ++step)
    {
        u8 striker = (u8)(step == 0 ? first : 1 - first);
        sim_duel_side *side = duel->side + striker, *enemy = duel->side + (1 - striker);
        b32 critical = false;
        u32 damage = sim_duel_strike_damage(sim, side, enemy, &critical);
        if (critical)
        {
            if (side->policy == DUEL_DEFENCE) side->guard_rounds = 2;
            if (side->policy == DUEL_SPIRIT)  side->spirit = Minimum(side->spirit + 50, 300u);
            side->policy_rounds = 0;
        }
        sim_duel_hurt(sim, enemy, damage, cause, events);
        sim_duel_gain_spirit(sim, enemy, damage > 0);
        if (sim_duel_side_down(sim, enemy)) sim_duel_finish(sim, striker, DUEL_RESULT_WIN, cause, events);
    }
    for (u32 index = 0; index < 2 && duel->active; ++index)
    {
        sim_duel_side *side = duel->side + index;
        sim_duel_gain_spirit(sim, side, false);
        side->policy_rounds++;
        if (side->focus_rounds) side->focus_rounds--;
        if (side->hold_rounds)  side->hold_rounds--;
        if (side->guard_rounds) side->guard_rounds--;
    }
    if (duel->active && duel->round >= rules->duel_max_rounds) sim_duel_finish(sim, 2, DUEL_RESULT_DRAW, cause, events);
}

/* The end: merit, technique points and perhaps force for the winner;
   morale and perhaps capture for the loser (duel.md, "Results"). */
internal void
sim_duel_finish(sim_state *sim, u8 winner, u8 result, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    sim_duel *duel = &sim->duel;
    duel->active = false;
    duel->winner = winner;
    duel->result = result;
    officer_component *victor = winner < 2 ? sim_duel_fighter(sim, duel->side + winner) : 0;
    sim_event ended = sim_event_make(sim, SIM_EVENT_DUEL_ENDED, cause);
    ended.subject = victor ? sim_officer_entity(sim, victor->officer_id) : 0;
    ended.faction = victor ? victor->force_id : SIM_NO_FORCE;
    ended.detail  = result;
    sim_emit(events, ended);
    if (!victor) return;
    sim_duel_side *losing = duel->side + (1 - winner);
    officer_component *loser = sim_duel_fighter(sim, losing);
    u32 merit = Minimum((u32)victor->merit + rules->duel_merit, 0xFFFFu);
    sim_emit_officer(sim, SIM_EVENT_MERIT_GAINED, victor, (s32)(merit - victor->merit), 0, false, cause, events);
    victor->merit = (u16)merit;
    sim_add_technique_points(sim, victor->force_id, (s32)rules->duel_technique, cause, events);
    if (sim_random_below(sim, 100) < rules->duel_force_chance && victor->war < 100) sim_change_ability(sim, victor, CONTENT_ABILITY_FORCE, 1, true, cause, events);
    sim_unit *loser_unit = sim_find_unit(sim, losing->unit);
    if (loser_unit) sim_change_morale(sim, losing->unit, -(s32)rules->duel_morale_loss, cause, events);
    b32 safe = !loser || result == DUEL_RESULT_RETREAT || sim_officer_has_skill(loser, rules->skill_fortune) ||
               (loser_unit && sim_unit_has_skill(sim, loser_unit->officers, rules->skill_guard));
    if (safe || sim_random_below(sim, 100) >= rules->duel_capture_percent) return;
    sim_unit *victor_unit = sim_find_unit(sim, duel->side[winner].unit);
    sim_remove_from_unit(sim, loser, cause, events);
    loser->former_force     = loser->force_id;
    loser->force_id         = victor->force_id;
    loser->status           = SIM_STATUS_CAPTIVE;
    loser->current_position = victor_unit ? victor_unit->home_city : 0;
    loser->base_location    = loser->current_position;
    sim_clear_post(sim, loser);
    sim_event captured = sim_event_make(sim, SIM_EVENT_OFFICER_CAPTURED, cause);
    captured.subject = sim_officer_entity(sim, loser->officer_id);
    captured.faction = victor->force_id;
    captured.assumed = true;
    sim_emit(events, captured);
}

/* ---- the actions of the running duel ------------------------------------------------ */

internal sim_check
sim_check_duel_action(sim_state *sim, sim_mini_order *order)
{
    sim_check check = {};
    check.slot = SIM_ANY_SLOT;
    sim_duel *duel = &sim->duel;
    if (!duel->active) return sim_refuse(check, SIM_REFUSAL_NO_DUEL);
    sim_duel_side *side = duel->side + 0;
    switch (order->action)
    {
        case SIM_DUEL_POLICY:
            if (order->value >= DUEL_POLICY_COUNT) return sim_refuse(check, SIM_REFUSAL_NO_DUEL);
            return check;
        case SIM_DUEL_SPECIAL:
            if (!sim_duel_special_ready(sim, side, order->value)) return sim_refuse(check, SIM_REFUSAL_NO_SPIRIT);
            return check;
        case SIM_DUEL_SUBSTITUTE:
        {
            officer_component *partner = order->value < SIM_COMMAND_OFFICERS ? sim_find_officer(sim, side->officers[order->value]) : 0;
            if (!partner || order->value == side->fighter || !partner->stamina) return sim_refuse(check, SIM_REFUSAL_NO_OFFICER);
            return check;
        }
        case SIM_DUEL_ROUND:
        case SIM_DUEL_AUTO:
            return check;
        default:
            return sim_refuse(check, SIM_REFUSAL_NO_DUEL);
    }
}

/* Plays the duel to its end; side 0 also follows the computer rule. */
internal void
sim_duel_auto(sim_state *sim, u32 cause, sim_event_sink *events)
{
    sim_duel *duel = &sim->duel;
    for (u32 guard = 0; guard < 1000 && duel->active; ++guard)
    {
        sim_duel_ai(sim, duel->side + 0, duel->side + 1, cause, events);
        sim_duel_round(sim, cause, events);
    }
}

internal void
sim_duel_action(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_mini_order *order = &command.mini;
    if (sim_check_duel_action(sim, order).refusal != SIM_REFUSAL_NONE) return;
    sim_duel_side *side = sim->duel.side + 0;
    switch (order->action)
    {
        case SIM_DUEL_POLICY:
            if (side->policy != order->value) side->policy_rounds = 0;
            side->policy = order->value;
            break;
        case SIM_DUEL_SPECIAL:    sim_duel_use_special(sim, 0, order->value, command.id, events); break;
        case SIM_DUEL_SUBSTITUTE: side->fighter = order->value; break;
        case SIM_DUEL_ROUND:      sim_duel_round(sim, command.id, events); break;
        case SIM_DUEL_AUTO:       sim_duel_auto(sim, command.id, events); break;
        default: break;
    }
}
