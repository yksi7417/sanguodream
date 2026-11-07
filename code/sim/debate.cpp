/* The debate of M7 (debate.md): the hand, the topic cards and the skill
   cards, the strength order, the mind gauge with its rocks, anger and the
   rage of each personality, and the result. Included by sim.cpp after
   duel.cpp.

   The spec gives the cards, the order and the hand sizes. The damage of
   each card, the skill cards of an officer and the answer of a defender
   are assumed (Q-038), and the exchange events say so. Side 0 is the
   officer of the faction that acts; side 1 answers by the computer rule:
   the weakest card that beats the attack. The answer of side 0 to an
   attack of side 1 uses the same rule (assumed: the screen of the debate
   is not in the spec). */

/* ---- cards ------------------------------------------------------------------- */

/* The hand size by intellect (guide book p.30). */
internal u32
sim_debate_hand_size(u32 intellect)
{
    if (intellect >= 90) return 6;
    if (intellect >= 80) return 5;
    if (intellect >= 70) return 4;
    return 3;
}

/* The skill cards that an officer can draw (assumed by intellect, Q-038);
   書物 gives all of them (manual p.21). A bit for each sim_card_type. */
internal u32
sim_debate_skill_mask(u32 intellect, b32 all_cards)
{
    u32 all = (1u << CARD_IGNORE) | (1u << CARD_SHOUT) | (1u << CARD_SOPHISTRY) | (1u << CARD_CALM) | (1u << CARD_ANGER) | (1u << CARD_RETHINK);
    if (all_cards || intellect >= 90) return all;
    if (intellect >= 80) return all & ~(1u << CARD_IGNORE);
    if (intellect >= 70) return (1u << CARD_SOPHISTRY) | (1u << CARD_CALM) | (1u << CARD_ANGER) | (1u << CARD_RETHINK);
    return (1u << CARD_CALM) | (1u << CARD_ANGER);
}

internal sim_card
sim_card_make(u8 type, u8 topic, u8 strength)
{
    sim_card card = {};
    card.type     = type;
    card.topic    = topic;
    card.strength = strength;
    return card;
}

internal sim_card
sim_debate_draw(sim_state *sim, sim_debate_side *side)
{
    officer_component *officer = sim_find_officer(sim, side->officer);
    u32 mask = sim_debate_skill_mask(officer ? officer->intelligence : 0, side->all_cards);
    if (sim_random_below(sim, 100) < sim->content.rules.debate_skill_card_percent)
    {
        u8 types[CARD_TYPE_COUNT];
        u32 count = 0;
        for (u32 type = CARD_IGNORE; type < CARD_TYPE_COUNT; ++type) if (mask & (1u << type)) types[count++] = (u8)type;
        if (count) return sim_card_make(types[sim_random_below(sim, count)], 0, 0);
    }
    /* Two draws in two statements: C++ does not fix the order of the
       arguments of a call, and MSVC and clang take them in another order. */
    u8 topic = (u8)sim_random_below(sim, TOPIC_COUNT);
    u8 power = (u8)(1 + sim_random_below(sim, 3));
    return sim_card_make(CARD_TOPIC, topic, power);
}

internal void
sim_debate_fill(sim_state *sim, sim_debate_side *side)
{
    officer_component *officer = sim_find_officer(sim, side->officer);
    u32 size = sim_debate_hand_size(officer ? officer->intelligence : 0);
    while (side->hand_count < size && side->hand_count < SIM_HAND_CAPACITY) side->hand[side->hand_count++] = sim_debate_draw(sim, side);
}

internal void
sim_debate_take(sim_debate_side *side, u32 place)
{
    for (u32 index = place; index + 1 < side->hand_count; ++index) side->hand[index] = side->hand[index + 1];
    side->hand_count--;
}

/* The strength order (manual p.21): 無視 > 大喝 > 詭辯 > topic cards > 鎮靜 >
   憤怒. */
internal u32
sim_card_power(sim_card card)
{
    switch (card.type)
    {
        case CARD_IGNORE:    return 6;
        case CARD_SHOUT:     return 5;
        case CARD_SOPHISTRY: return 4;
        case CARD_TOPIC:     return 3;
        case CARD_CALM:      return 1;
        default:             return 0;
    }
}

/* +1 if card a beats card b on the topic, -1 if b beats a, 0 for a draw.
   A card of the current topic beats a card of another topic; between two
   cards of the same standing, 大 > 中 > 小 (manual p.21). 鎮靜, 憤怒 and 熟慮
   beat nothing. */
internal s32
sim_card_compare(sim_card a, sim_card b, u8 topic)
{
    u32 pa = sim_card_power(a), pb = sim_card_power(b);
    b32 a_passive = a.type == CARD_CALM || a.type == CARD_ANGER || a.type == CARD_RETHINK;
    b32 b_passive = b.type == CARD_CALM || b.type == CARD_ANGER || b.type == CARD_RETHINK;
    if (a_passive) return b_passive ? 0 : -1;
    if (b_passive) return 1;
    if (pa != pb) return pa > pb ? 1 : -1;
    if (a.type != CARD_TOPIC) return 0;
    b32 a_fits = a.topic == topic, b_fits = b.topic == topic;
    if (a_fits != b_fits) return a_fits ? 1 : -1;
    if (a.strength != b.strength) return a.strength > b.strength ? 1 : -1;
    return 0;
}

/* The mind that a winning card takes. */
internal u32
sim_card_damage(sim_state *sim, sim_card card, u8 topic)
{
    sim_rules *rules = &sim->content.rules;
    switch (card.type)
    {
        case CARD_TOPIC:
        {
            u32 damage = rules->debate_card_damage[card.strength <= 3 ? card.strength : 3];
            return card.topic == topic ? sim_percent_of(damage, rules->debate_topic_percent) : damage;
        }
        case CARD_SHOUT:     return rules->debate_shout;
        case CARD_IGNORE:    return rules->debate_ignore;
        case CARD_SOPHISTRY: return rules->debate_card_damage[2];
        default:             return 0;
    }
}

/* The rocks of a side: one for each fifth of the mind gauge. */
internal u32
sim_debate_rocks(sim_state *sim, s32 mind)
{
    u32 full = Maximum(sim->content.rules.debate_mind, 1u);
    if (mind <= 0) return 0;
    return ((u32)mind * 5 + full - 1) / full;
}

/* ---- the start --------------------------------------------------------------- */

internal void
sim_debate_end(sim_state *sim, u8 winner, u32 cause, sim_event_sink *events);

/* Starts a debate of two officers. A large gap of intellect ends it at
   once (guide book p.33). The higher intellect speaks first (assumed). */
internal void sim_debate_auto(sim_state *sim, u32 cause, sim_event_sink *events);
internal void sim_debate_close(sim_state *sim, u8 choice, u32 cause, sim_event_sink *events);

internal void
sim_begin_debate(sim_state *sim, u16 officer_a, u16 officer_b, u8 purpose, sim_court_order *order, u8 force, u32 cause, sim_event_sink *events)
{
    sim_debate *debate = &sim->debate;
    *debate = {};
    debate->active  = true;
    debate->purpose = purpose;
    debate->force   = force;
    debate->cause   = cause;
    debate->topic   = (u8)sim_random_below(sim, TOPIC_COUNT);
    debate->last[0].type = debate->last[1].type = CARD_TYPE_COUNT;
    if (order) debate->order = *order;
    u16 officers[2] = { officer_a, officer_b };
    for (u32 index = 0; index < 2; ++index)
    {
        sim_debate_side *side = debate->side + index;
        side->officer   = officers[index];
        side->mind      = (s32)sim->content.rules.debate_mind;
        side->all_cards = sim_officer_item(sim, officers[index], ITEM_BOOK) != 0;
        sim_debate_fill(sim, side);
    }
    officer_component *a = sim_find_officer(sim, officer_a), *b = sim_find_officer(sim, officer_b);
    debate->turn = (a && b && b->intelligence > a->intelligence) ? 1 : 0;
    sim_event event = sim_event_make(sim, SIM_EVENT_DEBATE_STARTED, cause);
    event.subject = sim_officer_entity(sim, officer_a);
    event.faction = force;
    event.detail  = officer_b;
    sim_emit(events, event);
    u32 gap = sim->content.rules.debate_instant_gap;
    if (a && b && gap)
    {
        if (a->intelligence >= b->intelligence + gap) sim_debate_end(sim, 0, cause, events);
        else if (b->intelligence >= a->intelligence + gap) sim_debate_end(sim, 1, cause, events);
    }
    /* M9: 單挑・舌戰 not manual: the computer plays the exchanges of a human faction;
       automatic also takes the first result choice. */
    b32 human = force < SIM_FORCE_CAPACITY && sim->turns.human[force];
    if (human && sim->settings.duel_mode != DUEL_MODE_MANUAL)
    {
        sim_debate_auto(sim, cause, events);
        if (sim->settings.duel_mode == DUEL_MODE_AUTO && debate->active && debate->finished) sim_debate_close(sim, 0, cause, events);
    }
    /* The play run of 2026-11-07: the debate of a recruit, a search or an envoy
       of the computer runs to its end without a pause, as a duel of two computer
       officers does. An open debate refuses the commands of the other
       factions (SIM_REFUSAL_BUSY). The test command start_debate keeps its
       debate open for the steps of the tests. */
    if (!human && purpose != DEBATE_FOR_NOTHING && debate->active)
    {
        sim_debate_auto(sim, cause, events);
        if (debate->active && debate->finished) sim_debate_close(sim, 0, cause, events);
    }
}

/* ---- one exchange ----------------------------------------------------------------- */

internal void
sim_debate_anger(sim_state *sim, u8 side_index, u32 amount)
{
    sim_debate *debate = &sim->debate;
    sim_debate_side *side = debate->side + side_index, *other = debate->side + (1 - side_index);
    if (side->rage_rounds) return;
    side->anger += amount;
    if (side->anger < 100) return;
    side->anger = 0;
    /* A held 鎮靜 cancels the rage; a held 憤怒 stops it and starts the
       holder's own rage (manual p.21). */
    for (u32 place = 0; place < other->hand_count; ++place)
    {
        if (other->hand[place].type == CARD_CALM) { sim_debate_take(other, place); return; }
        if (other->hand[place].type == CARD_ANGER)
        {
            sim_debate_take(other, place);
            other->rage_rounds = (u8)sim->content.rules.debate_rage_rounds;
            return;
        }
    }
    side->rage_rounds = (u8)sim->content.rules.debate_rage_rounds;
}

internal void
sim_debate_hurt(sim_state *sim, u8 side_index, u32 damage, u32 cause, sim_event_sink *events)
{
    sim_debate *debate = &sim->debate;
    sim_debate_side *side = debate->side + side_index;
    if (!damage) return;
    u32 rocks_before = sim_debate_rocks(sim, side->mind);
    side->mind -= (s32)damage;
    if (sim_debate_rocks(sim, side->mind) < rocks_before) side->rethink_used = false;   /* a broken rock allows 熟慮 again */
    sim_event event = sim_event_make(sim, SIM_EVENT_DEBATE_EXCHANGE, cause);
    event.subject = sim_officer_entity(sim, side->officer);
    event.faction = debate->force;
    event.delta   = -(s32)damage;
    event.assumed = true;
    sim_emit(events, event);
    sim_debate_anger(sim, side_index, sim->content.rules.debate_anger_hit);
}

/* The place of the weakest card of the hand that beats the attack, or the
   hand count if none. During a rage of the attacker, 剛膽 lets only 無視 and
   大喝 answer, and 冷靜 forbids the skill cards. */
internal u32
sim_debate_answer(sim_state *sim, sim_debate_side *defender, sim_card attack, sim_debate_side *attacker)
{
    sim_debate *debate = &sim->debate;
    officer_component *raging = attacker->rage_rounds ? sim_find_officer(sim, attacker->officer) : 0;
    u32 best = defender->hand_count;
    for (u32 place = 0; place < defender->hand_count; ++place)
    {
        sim_card card = defender->hand[place];
        if (raging && raging->personality == PERSONALITY_BOLD && card.type != CARD_IGNORE && card.type != CARD_SHOUT) continue;
        if (raging && raging->personality == PERSONALITY_CALM && card.type != CARD_TOPIC) continue;
        if (raging && raging->personality == PERSONALITY_RECKLESS) continue;
        if (sim_card_compare(card, attack, debate->topic) <= 0) continue;
        if (best == defender->hand_count || sim_card_power(card) < sim_card_power(defender->hand[best])) best = place;
    }
    return best;
}

/* The card that a computer side plays: the strongest attack card; 熟慮 if
   the hand has none. */
internal u32
sim_debate_choose(sim_state *sim, sim_debate_side *side)
{
    u32 best = 0;
    for (u32 place = 1; place < side->hand_count; ++place)
    {
        s32 order = sim_card_compare(side->hand[place], side->hand[best], sim->debate.topic);
        if (order > 0) best = place;
    }
    return best;
}

/* One exchange: the side that has the turn plays the card at the place. */
internal void
sim_debate_exchange(sim_state *sim, u32 place, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    sim_debate *debate = &sim->debate;
    u8 attacker_index = debate->turn, defender_index = (u8)(1 - debate->turn);
    sim_debate_side *attacker = debate->side + attacker_index, *defender = debate->side + defender_index;
    if (place >= attacker->hand_count) return;
    sim_card attack = attacker->hand[place];
    sim_debate_take(attacker, place);
    debate->last[attacker_index] = attack;
    debate->last[defender_index].type = CARD_TYPE_COUNT;
    debate->round++;
    officer_component *raging = attacker->rage_rounds ? sim_find_officer(sim, attacker->officer) : 0;
    switch (attack.type)
    {
        case CARD_CALM:
            defender->anger = defender->anger > rules->debate_calm_card ? defender->anger - rules->debate_calm_card : 0;
            debate->turn = defender_index;
            break;
        case CARD_ANGER:
            sim_debate_anger(sim, attacker_index, rules->debate_anger_card);
            debate->turn = defender_index;
            break;
        case CARD_RETHINK:
            attacker->hand_count = 0;
            attacker->rethink_used = true;
            debate->turn = defender_index;
            break;
        default:
        {
            u32 answer = sim_debate_answer(sim, defender, attack, attacker);
            if (answer < defender->hand_count)
            {
                sim_card counter = defender->hand[answer];
                sim_debate_take(defender, answer);
                debate->last[defender_index] = counter;
                u32 damage = counter.type == CARD_SOPHISTRY ? sim_card_damage(sim, attack, debate->topic) : sim_card_damage(sim, counter, debate->topic);
                if (counter.type == CARD_SOPHISTRY) debate->topic = (u8)((debate->topic + 1 + sim_random_below(sim, TOPIC_COUNT - 1)) % TOPIC_COUNT);
                if (counter.type == CARD_IGNORE) sim_debate_anger(sim, attacker_index, rules->debate_anger_card);
                sim_debate_hurt(sim, attacker_index, damage, cause, events);
                debate->turn = defender_index;   /* the winner plays next */
                break;
            }
            u32 damage = sim_card_damage(sim, attack, debate->topic);
            if (raging && raging->personality == PERSONALITY_CAREFUL)  damage = damage * (attacker->hand_count + 2) / 2;
            if (raging && raging->personality == PERSONALITY_RECKLESS) damage *= 3;
            officer_component *defending = sim_find_officer(sim, defender->officer);
            if (attack.type == CARD_SHOUT && defender->rage_rounds && defending && defending->personality == PERSONALITY_CALM) damage = 0;
            if (attack.type == CARD_IGNORE) sim_debate_anger(sim, defender_index, rules->debate_anger_card);
            if (damage) sim_debate_hurt(sim, defender_index, damage, cause, events);
            else        { sim_debate_anger(sim, 0, rules->debate_anger_draw); sim_debate_anger(sim, 1, rules->debate_anger_draw); }
            break;
        }
    }
    if (attacker->rage_rounds) attacker->rage_rounds--;
    if (rules->debate_topic_rounds && debate->round % rules->debate_topic_rounds == 0) debate->topic = (u8)sim_random_below(sim, TOPIC_COUNT);
    sim_debate_fill(sim, attacker);
    sim_debate_fill(sim, defender);
    if (debate->side[0].mind <= 0)      sim_debate_end(sim, 1, cause, events);
    else if (debate->side[1].mind <= 0) sim_debate_end(sim, 0, cause, events);
}

/* ---- the end ---------------------------------------------------------------------- */

internal void
sim_debate_end(sim_state *sim, u8 winner, u32 cause, sim_event_sink *events)
{
    sim_debate *debate = &sim->debate;
    debate->finished = true;
    debate->winner   = winner;
    officer_component *victor = sim_find_officer(sim, debate->side[winner].officer);
    sim_event event = sim_event_make(sim, SIM_EVENT_DEBATE_ENDED, cause);
    event.subject = victor ? sim_officer_entity(sim, victor->officer_id) : 0;
    event.faction = debate->force;
    event.detail  = winner == 0 ? 1 : 0;
    sim_emit(events, event);
}

/* The result of a finished debate: merit for the winner, then 施予同情
   (technique points) or 進逼 (perhaps 1 intellect), and the command that
   waited for it. */
internal void
sim_debate_close(sim_state *sim, u8 choice, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    sim_debate *debate = &sim->debate;
    officer_component *victor = sim_find_officer(sim, debate->side[debate->winner].officer);
    officer_component *other  = sim_find_officer(sim, debate->side[1 - debate->winner].officer);
    b32 won = debate->winner == 0;
    if (victor)
    {
        u32 merit = Minimum((u32)victor->merit + rules->debate_merit, 0xFFFFu);
        sim_emit_officer(sim, SIM_EVENT_MERIT_GAINED, victor, (s32)(merit - victor->merit), 0, false, cause, events);
        victor->merit = (u16)merit;
    }
    if (won && choice == 0) sim_add_technique_points(sim, debate->force, (s32)rules->debate_technique, cause, events);
    if (won && choice == 1 && victor && victor->intelligence < 100 && sim_random_below(sim, 100) < rules->debate_press_chance)
    {
        sim_change_ability(sim, victor, CONTENT_ABILITY_INTELLECT, 1, true, cause, events);
    }
    debate->active = false;
    sim_court_order order = debate->order;
    switch (debate->purpose)
    {
        case DEBATE_FOR_RECRUIT:
        case DEBATE_FOR_SEARCH:
        {
            officer_component *target = sim_find_officer(sim, order.target);
            if (!target) break;
            if (won) sim_join_faction(sim, target, debate->force, order.city_id, cause, events);
            else     sim_emit_officer(sim, SIM_EVENT_RECRUIT_FAILED, target, 0, 0, true, cause, events);
            break;
        }
        case DEBATE_FOR_DIPLOMACY:
            sim_diplomacy_result(sim, debate->force, &order, won, cause, events);
            break;
        default:
            break;
    }
    (void)other;
}

/* ---- the actions of the running debate --------------------------------------------- */

internal sim_check
sim_check_debate_action(sim_state *sim, sim_mini_order *order)
{
    sim_check check = {};
    check.slot = SIM_ANY_SLOT;
    sim_debate *debate = &sim->debate;
    if (!debate->active) return sim_refuse(check, SIM_REFUSAL_NO_DUEL);
    switch (order->action)
    {
        case SIM_DEBATE_PLAY:
        {
            if (debate->finished || debate->turn != 0 || order->value >= debate->side[0].hand_count) return sim_refuse(check, SIM_REFUSAL_NO_DUEL);
            sim_card card = debate->side[0].hand[order->value];
            b32 raging = debate->side[0].rage_rounds != 0;
            if (raging && (card.type == CARD_CALM || card.type == CARD_ANGER)) return sim_refuse(check, SIM_REFUSAL_NO_DUEL);
            if (card.type == CARD_RETHINK && debate->side[0].rethink_used)     return sim_refuse(check, SIM_REFUSAL_NO_DUEL);
            return check;
        }
        case SIM_DEBATE_ROUND:
        case SIM_DEBATE_AUTO:
            if (debate->finished) return sim_refuse(check, SIM_REFUSAL_NO_DUEL);
            return check;
        case SIM_DEBATE_FINISH:
            if (!debate->finished || order->value > 1) return sim_refuse(check, SIM_REFUSAL_NO_DUEL);
            return check;
        default:
            return sim_refuse(check, SIM_REFUSAL_NO_DUEL);
    }
}

/* Plays exchanges by the computer rule on both sides until a side wins. */
internal void
sim_debate_auto(sim_state *sim, u32 cause, sim_event_sink *events)
{
    sim_debate *debate = &sim->debate;
    for (u32 guard = 0; guard < 500 && debate->active && !debate->finished; ++guard)
    {
        sim_debate_exchange(sim, sim_debate_choose(sim, debate->side + debate->turn), cause, events);
    }
    if (debate->active && !debate->finished) sim_debate_end(sim, debate->side[0].mind >= debate->side[1].mind ? 0 : 1, cause, events);
}

internal void
sim_debate_action(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_mini_order *order = &command.mini;
    if (sim_check_debate_action(sim, order).refusal != SIM_REFUSAL_NONE) return;
    sim_debate *debate = &sim->debate;
    switch (order->action)
    {
        case SIM_DEBATE_PLAY:   sim_debate_exchange(sim, order->value, command.id, events); break;
        case SIM_DEBATE_ROUND:  sim_debate_exchange(sim, sim_debate_choose(sim, debate->side + debate->turn), command.id, events); break;
        case SIM_DEBATE_AUTO:   sim_debate_auto(sim, command.id, events); break;
        case SIM_DEBATE_FINISH: sim_debate_close(sim, order->value, command.id, events); break;
        default: break;
    }
}

/* SIM_COMMAND_DEBATE: a test and editor command that starts a debate of two
   officers without a purpose. */
internal sim_check
sim_check_start_debate(sim_state *sim, sim_mini_order *order)
{
    sim_check check = {};
    check.slot = SIM_ANY_SLOT;
    if (sim->duel.active || sim->debate.active) return sim_refuse(check, SIM_REFUSAL_BUSY);
    officer_component *a = sim_find_officer(sim, order->officer_a), *b = sim_find_officer(sim, order->officer_b);
    if (!a || !b || a == b) return sim_refuse(check, SIM_REFUSAL_NO_OFFICER);
    return check;
}

internal void
sim_start_debate_command(sim_state *sim, sim_command command, sim_event_sink *events)
{
    if (sim_check_start_debate(sim, &command.mini).refusal != SIM_REFUSAL_NONE) return;
    officer_component *a = sim_find_officer(sim, command.mini.officer_a);
    sim_begin_debate(sim, command.mini.officer_a, command.mini.officer_b, DEBATE_FOR_NOTHING, 0, a->force_id, command.id, events);
}
