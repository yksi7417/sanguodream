/* The research of techniques (研究技巧; research-techniques.md,
   city-commands.md) and the effects of the techniques that the rules of M4
   read. Included by sim.cpp.

   A faction has a set of techniques (a bit for each row of techniques.tsv).
   Level n of a line needs level n - 1. The command pays its gold, its
   action points and its technique points when it starts. Its officers are
   busy for the whole period. One research runs at a time for each faction
   (assumed, Q-024). The period is an assumed line in the documented range
   (Q-161).

   The effects of the techniques are flags. The rules of M4 read the flags
   that have a rule now: the morale cap (熟練兵), the facility recovery
   (培訓工兵), the strong weapons (開發木獸, 開發投石) and public order
   (整備政令). The movement and combat rules of M5 and M6 read the others
   through sim_has_technique_effect. */

internal b32
sim_has_technique(sim_state *sim, u8 force, u8 technique)
{
    if (force == SIM_NO_FORCE || technique >= CONTENT_TECHNIQUE_CAPACITY) return false;
    return (sim->techniques[force] >> technique) & 1;
}

/* True if the faction has a technique with the effect. CONTENT_CLASS_NONE
   matches every class. */
internal b32
sim_has_technique_effect(sim_state *sim, u8 force, technique_effect effect, content_unit_class unit_class)
{
    for (u32 index = 0; index < sim->content.technique_count; ++index)
    {
        technique_kind *kind = sim->content.techniques + index;
        if (kind->effect != effect || !sim_has_technique(sim, force, (u8)index)) continue;
        if (unit_class == CONTENT_CLASS_NONE || kind->unit_class == unit_class) return true;
    }
    return false;
}

/* The value of the first technique with the effect that the faction has. */
internal u32
sim_technique_effect_value(sim_state *sim, u8 force, technique_effect effect)
{
    for (u32 index = 0; index < sim->content.technique_count; ++index)
    {
        technique_kind *kind = sim->content.techniques + index;
        if (kind->effect == effect && sim_has_technique(sim, force, (u8)index)) return kind->value;
    }
    return 0;
}

/* The technique of the same line one level lower. */
internal u8
sim_previous_technique(sim_content *content, u8 technique)
{
    if (technique >= content->technique_count) return CONTENT_NO_TECHNIQUE;
    technique_kind *kind = content->techniques + technique;
    if (kind->level <= 1) return CONTENT_NO_TECHNIQUE;
    for (u32 index = 0; index < content->technique_count; ++index)
    {
        technique_kind *other = content->techniques + index;
        if (other->line == kind->line && other->level == kind->level - 1) return (u8)index;
    }
    return CONTENT_NO_TECHNIQUE;
}

internal void
sim_grant_technique(sim_state *sim, u8 force, u8 technique)
{
    if (force == SIM_NO_FORCE || technique >= sim->content.technique_count) return;
    sim->techniques[force] |= 1ull << technique;
}

internal u32
sim_round_to_turns(u32 days)
{
    u32 turns = calendar_turns_for_days(days);
    return (turns ? turns : 1) * CALENDAR_DAYS_PER_TURN;
}

/* ASSUMED LINE (Q-161). The guide book gives a range of days for each
   technique, "which change with the abilities of the officers". The line
   goes from the longest period at an ability sum of 0 to the shortest at
   research.full_ability. A personnel office in the city shortens it
   (manual p.29): research.personnel_percent of it. The result goes up to
   whole turns. */
internal u32
sim_research_days(u32 ability_sum, u32 min_days, u32 max_days, b32 personnel_office, sim_rules *rules)
{
    u32 days = max_days;
    if (rules->research_full_ability && max_days > min_days)
    {
        u32 sum = ability_sum < rules->research_full_ability ? ability_sum : rules->research_full_ability;
        days = max_days - (max_days - min_days) * sum / rules->research_full_ability;
    }
    if (personnel_office && rules->research_personnel_percent) days = sim_percent_of(days, rules->research_personnel_percent);
    return sim_round_to_turns(days);
}

/* 指導 halves the gold (documented). */
internal u32
sim_research_gold(u32 gold, b32 teaching, sim_rules *rules)
{
    return teaching && rules->research_teaching_percent ? sim_percent_of(gold, rules->research_teaching_percent) : gold;
}

/* The checks of the research command, in order. The first reason that
   holds is the answer. */
internal sim_check
sim_check_research(sim_state *sim, sim_research_order *order)
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
    if (order->technique >= sim->content.technique_count) return sim_refuse(check, SIM_REFUSAL_NO_TECHNIQUE);
    technique_kind *kind = sim->content.techniques + order->technique;
    if (sim_has_technique(sim, force, order->technique)) return sim_refuse(check, SIM_REFUSAL_VALUE_AT_MAXIMUM);
    u8 previous = sim_previous_technique(&sim->content, order->technique);
    if (previous != CONTENT_NO_TECHNIQUE && !sim_has_technique(sim, force, previous)) return sim_refuse(check, SIM_REFUSAL_NEEDS_TECHNIQUE);
    if (sim->research[force].technique != CONTENT_NO_TECHNIQUE) return sim_refuse(check, SIM_REFUSAL_LIMIT_REACHED);

    u32 ability = 0;
    sim_refusal refusal = sim_check_city_officers(sim, city, order->officers, SIM_COMMAND_OFFICERS, (sim_ability)kind->ability, &ability);
    if (refusal != SIM_REFUSAL_NONE)                    return sim_refuse(check, refusal);
    sim_rules *rules = &sim->content.rules;
    if (sim->turns.action_points[force] < rules->ap_research) return sim_refuse(check, SIM_REFUSAL_NO_ACTION_POINTS);
    if (sim->technique_points[force] < kind->points)    return sim_refuse(check, SIM_REFUSAL_NO_TECHNIQUE_POINTS);
    u32 gold = sim_research_gold(kind->gold, sim_order_has_trait(sim, order->officers, TRAIT_CHEAP_RESEARCH), rules);
    if (city->gold < gold)                              return sim_refuse(check, SIM_REFUSAL_NO_GOLD);

    check.gold          = gold;
    check.action_points = rules->ap_research;
    check.days          = sim_research_days(ability, kind->min_days, kind->max_days,
                                            sim_city_has_built(sim, city->city_id, FACILITY_EFFECT_PERSONNEL), rules);
    check.turns         = check.days / CALENDAR_DAYS_PER_TURN;
    check.quantity      = kind->points;
    check.value_before  = sim->technique_points[force];
    check.value_after   = sim->technique_points[force] - kind->points;
    return check;
}

/* Starts the research: pays the gold, the action points and the technique
   points, and keeps the officers busy for the period. */
internal void
sim_research(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_research_order *order = &command.research;
    sim_check check = sim_check_research(sim, order);
    if (check.refusal != SIM_REFUSAL_NONE) return;
    city_component *city = sim_find_city(sim, order->city_id);
    u8 force = city->owner_force_id;

    city->gold -= check.gold;
    sim_emit_city(sim, SIM_EVENT_GOLD_SPENT, city, -(s32)check.gold, 0, false, command.id, events);
    sim->turns.action_points[force] -= check.action_points;
    sim_event points = sim_event_make(sim, SIM_EVENT_ACTION_POINTS_SPENT, command.id);
    points.faction = force;
    points.delta   = -(s32)check.action_points;
    sim_emit(events, points);
    sim->technique_points[force] -= check.quantity;
    sim_event spent = sim_event_make(sim, SIM_EVENT_TECHNIQUE_SPENT, command.id);
    spent.faction = force;
    spent.delta   = -(s32)check.quantity;
    spent.detail  = order->technique;
    sim_emit(events, spent);

    sim_research_job *research = sim->research + force;
    research->technique  = order->technique;
    research->city_id    = order->city_id;
    research->turns_left = (u16)check.turns;
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
    {
        research->officers[place] = order->officers[place];
        officer_component *officer = sim_find_officer(sim, order->officers[place]);
        if (!officer) continue;
        officer->acted_turn = sim->turn + 1;
        officer->job_city   = order->city_id;
        officer->job_slot   = SIM_JOB_RESEARCH;
    }
    sim_emit_city(sim, SIM_EVENT_RESEARCH_STARTED, city, (s32)check.turns, order->technique, true, command.id, events);
    /* Research raises merit and lowers technique points (city-commands.md). */
    sim_reward_command(sim, force, order->officers, 0, command.id, events);
}

/* At the start of each turn each research advances one turn. At 0 the
   faction has the technique and the officers are free. */
internal void
sim_progress_research(sim_state *sim, u32 cause, sim_event_sink *events)
{
    for (u32 force = 0; force < SIM_FORCE_CAPACITY; ++force)
    {
        sim_research_job *research = sim->research + force;
        if (research->technique == CONTENT_NO_TECHNIQUE) continue;
        if (research->turns_left > 0) research->turns_left--;
        if (research->turns_left > 0) continue;
        sim_grant_technique(sim, (u8)force, research->technique);
        sim_event done = sim_event_make(sim, SIM_EVENT_TECHNIQUE_RESEARCHED, cause);
        done.subject = sim_city_entity(sim, research->city_id);
        done.faction = (u8)force;
        done.detail  = research->technique;
        sim_emit(events, done);
        for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
        {
            officer_component *officer = sim_find_officer(sim, research->officers[place]);
            research->officers[place] = SIM_NO_OFFICER;
            if (!officer) continue;
            officer->job_city = SIM_NO_CITY;
            sim_event released = sim_event_make(sim, SIM_EVENT_OFFICER_RELEASED, cause);
            released.subject = sim_officer_entity(sim, officer->officer_id);
            released.faction = officer->force_id;
            sim_emit(events, released);
        }
        research->technique = CONTENT_NO_TECHNIQUE;
    }
}

/* ---- the effects that the rules of M4 read ------------------------------ */

/* Morale has a maximum of 100, 120 with 熟練兵 (city-commands.md). */
internal u32
sim_city_morale_max(sim_state *sim, city_component *city)
{
    return sim->content.rules.morale_max + sim_technique_effect_value(sim, city->owner_force_id, TECHNIQUE_EFFECT_MORALE_CAP);
}

/* ASSUMED (Q-161): 整備政令 lets public order decrease less: the loss is
   (100 - value) percent of the loss, and at least 1. */
internal u32
sim_order_loss(sim_state *sim, u8 force, u32 loss)
{
    u32 keep = sim_technique_effect_value(sim, force, TECHNIQUE_EFFECT_ORDER_KEEP);
    if (!keep || !loss) return loss;
    u32 reduced = sim_percent_of(loss, keep < 100 ? 100 - keep : 0);
    return reduced ? reduced : 1;
}

/* A strong weapon needs its technique, and a weak weapon cannot be
   produced after the faction has the strong one (manual p.12). */
internal sim_refusal
sim_check_weapon_technique(sim_state *sim, u8 force, weapon_kind *weapon)
{
    if (weapon->technique != CONTENT_NO_TECHNIQUE && !sim_has_technique(sim, force, weapon->technique)) return SIM_REFUSAL_NEEDS_TECHNIQUE;
    for (u32 index = 0; index < sim->content.weapon_count; ++index)
    {
        weapon_kind *strong = sim->content.weapons + index;
        if (strong->replaces != weapon->id) continue;
        if (strong->technique == CONTENT_NO_TECHNIQUE || sim_has_technique(sim, force, strong->technique)) return SIM_REFUSAL_WEAPON_REPLACED;
    }
    return SIM_REFUSAL_NONE;
}
