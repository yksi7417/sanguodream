/* Diplomacy and the plots of factions of M7 (diplomacy.md, city-commands.md):
   friendliness, the relation of two factions, the seven diplomacy commands
   and 二虎競食, 驅虎吞狼 and 流言. Included by sim.cpp after officers.cpp.

   The spec gives the inputs of each chance and the costs. The chances and
   amounts are assumed numbers of rules.tsv (Q-039, Q-180, Q-181). */

/* ---- the state of two factions --------------------------------------------------- */

internal void
sim_init_diplomacy(sim_state *sim)
{
    u8 start = (u8)Minimum(sim->content.rules.friendliness_start, 100u);
    for (u32 a = 0; a < SIM_DIPLOMACY_FORCES; ++a)
    {
        for (u32 b = 0; b < SIM_DIPLOMACY_FORCES; ++b)
        {
            sim->diplomacy.friendliness[a][b] = a == b ? 100 : start;
            sim->diplomacy.relation[a][b]     = RELATION_NONE;
            sim->diplomacy.months[a][b]       = 0;
        }
    }
}

internal u32
sim_friendliness(sim_state *sim, u8 a, u8 b)
{
    if (a >= SIM_DIPLOMACY_FORCES || b >= SIM_DIPLOMACY_FORCES) return 0;
    return sim->diplomacy.friendliness[a][b];
}

internal u8
sim_relation(sim_state *sim, u8 a, u8 b)
{
    if (a >= SIM_DIPLOMACY_FORCES || b >= SIM_DIPLOMACY_FORCES) return RELATION_NONE;
    return sim->diplomacy.relation[a][b];
}

/* The five steps of friendliness (diplomacy.md): 4 親密, 3 友好, 2 中立, 1 不和,
   0 險惡. The steps are 20 points wide (assumed, Q-180). */
internal u32
sim_friendliness_level(u32 friendliness)
{
    return friendliness >= 80 ? 4 : friendliness >= 60 ? 3 : friendliness >= 40 ? 2 : friendliness >= 20 ? 1 : 0;
}

/* Two factions fight unless they are allies or under a ceasefire. A unit
   or a city without an owner is no enemy. */
internal b32
sim_at_war(sim_state *sim, u8 a, u8 b)
{
    if (a == b || a == SIM_NO_FORCE || b == SIM_NO_FORCE) return false;
    return sim_relation(sim, a, b) == RELATION_NONE;
}

internal void
sim_change_friendliness(sim_state *sim, u8 a, u8 b, s32 change, u32 cause, sim_event_sink *events)
{
    if (a >= SIM_DIPLOMACY_FORCES || b >= SIM_DIPLOMACY_FORCES || a == b) return;
    s32 value = (s32)sim->diplomacy.friendliness[a][b] + change;
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    s32 delta = value - (s32)sim->diplomacy.friendliness[a][b];
    sim->diplomacy.friendliness[a][b] = (u8)value;
    sim->diplomacy.friendliness[b][a] = (u8)value;
    if (!delta) return;
    sim_event event = sim_event_make(sim, SIM_EVENT_FRIENDLINESS_CHANGED, cause);
    event.faction = a;
    event.detail  = b;
    event.delta   = delta;
    event.assumed = true;
    sim_emit(events, event);
}

internal void
sim_set_relation(sim_state *sim, u8 a, u8 b, u8 relation, u32 months, u32 cause, sim_event_sink *events)
{
    if (a >= SIM_DIPLOMACY_FORCES || b >= SIM_DIPLOMACY_FORCES || a == b) return;
    sim->diplomacy.relation[a][b] = relation;
    sim->diplomacy.relation[b][a] = relation;
    sim->diplomacy.months[a][b]   = (u8)months;
    sim->diplomacy.months[b][a]   = (u8)months;
    sim_event event = sim_event_make(sim, SIM_EVENT_RELATION_CHANGED, cause);
    event.faction = a;
    event.detail  = b;
    event.delta   = relation;
    sim_emit(events, event);
}

/* Each month a ceasefire counts down; at 0 the factions fight again. */
internal void
sim_run_diplomacy_month(sim_state *sim, u32 cause, sim_event_sink *events)
{
    for (u32 a = 0; a < SIM_DIPLOMACY_FORCES; ++a)
    {
        for (u32 b = a + 1; b < SIM_DIPLOMACY_FORCES; ++b)
        {
            if (sim->diplomacy.relation[a][b] != RELATION_CEASEFIRE) continue;
            u8 months = sim->diplomacy.months[a][b];
            if (months > 1)
            {
                sim->diplomacy.months[a][b] = sim->diplomacy.months[b][a] = (u8)(months - 1);
                continue;
            }
            sim_set_relation(sim, (u8)a, (u8)b, RELATION_NONE, 0, cause, events);
        }
    }
}

/* The strength of a faction: its soldiers in cities and units. */
internal u64
sim_faction_strength(sim_state *sim, u8 force)
{
    u64 soldiers = 0;
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        if (sim->cities.data[index].owner_force_id == force) soldiers += sim->cities.data[index].troops;
    }
    for (u32 index = 0; index < sim->unit_count; ++index)
    {
        if (sim->units[index].alive && sim->units[index].force_id == force) soldiers += sim->units[index].soldiers;
    }
    return soldiers;
}

internal b32
sim_faction_exists(sim_state *sim, u8 force)
{
    return force != SIM_NO_FORCE && force < SIM_DIPLOMACY_FORCES && sim_force_ruler(sim, force) != SIM_NO_OFFICER;
}

/* An alive aid unit of the faction. */
internal b32
sim_aid_running(sim_state *sim, u8 ally)
{
    for (u32 index = 0; index < sim->unit_count; ++index)
    {
        if (sim->units[index].alive && sim->units[index].aid_turns && sim->units[index].force_id == ally) return true;
    }
    return false;
}

internal s32
sim_clamp_percent(s32 value)
{
    return value < 0 ? 0 : (value > 100 ? 100 : value);
}

/* The chance of a diplomacy command in percent (assumed, Q-180). */
internal u32
sim_diplomacy_chance(sim_state *sim, u8 force, officer_component *envoy, sim_court_order *order)
{
    sim_rules *rules = &sim->content.rules;
    s32 friendliness = (s32)sim_friendliness(sim, force, order->force);
    officer_component *ruler = sim_find_officer(sim, sim_force_ruler(sim, order->force));
    s32 relation = 0;
    if (ruler && (sim_officer_likes(ruler, envoy->officer_id) || sim_officers_blood(ruler, envoy))) relation = 15;
    if (ruler && sim_officer_dislikes(ruler, envoy->officer_id)) relation = -30;
    switch (order->kind)
    {
        case SIM_DIPLOMACY_GOODWILL:
        case SIM_DIPLOMACY_BREAK:
            return 100;
        case SIM_DIPLOMACY_ALLIANCE:
            return (u32)sim_clamp_percent((s32)rules->alliance_base + friendliness - 40 + envoy->politics / 5 + (s32)(order->gold / 200) + relation);
        case SIM_DIPLOMACY_CEASEFIRE:
            return (u32)sim_clamp_percent((s32)rules->ceasefire_base + friendliness / 2 + envoy->politics / 5 + (s32)(order->gold / 100) -
                                          (s32)(order->months * rules->ceasefire_month_cost) + relation);
        case SIM_DIPLOMACY_SURRENDER:
        {
            u64 mine = sim_faction_strength(sim, force), theirs = sim_faction_strength(sim, order->force);
            s32 ratio = theirs ? (s32)Minimum(mine / theirs, (u64)20) : 20;
            return (u32)sim_clamp_percent((s32)rules->surrender_base + envoy->charm / 5 + 10 * (ratio - 3) + relation);
        }
        case SIM_DIPLOMACY_EXCHANGE:
            return (u32)sim_clamp_percent((s32)rules->exchange_base + envoy->politics / 4 + (s32)(order->gold / 100) + relation);
        case SIM_DIPLOMACY_AID:
            return (u32)sim_clamp_percent((s32)rules->aid_base + friendliness / 2 + (s32)(order->gold / 100) + relation);
        default:
            return 0;
    }
}

/* The gold of a diplomacy command: 1000 for 親善 (half with a diplomacy
   office), the offer of the order for the others. */
internal u32
sim_diplomacy_gold(sim_state *sim, city_component *city, sim_court_order *order)
{
    if (order->kind == SIM_DIPLOMACY_GOODWILL)
    {
        u32 gold = sim->content.rules.goodwill_gold;
        return sim_city_has_built(sim, city->city_id, FACILITY_EFFECT_DIPLOMACY) ? gold / 2 : gold;
    }
    if (order->kind == SIM_DIPLOMACY_BREAK || order->kind == SIM_DIPLOMACY_SURRENDER) return 0;
    return order->gold;
}

internal sim_check
sim_check_diplomacy(sim_state *sim, sim_court_order *order)
{
    sim_check check = {};
    check.slot    = SIM_ANY_SLOT;
    check.assumed = true;
    sim_rules *rules = &sim->content.rules;
    city_component *city = 0;
    sim_refusal refusal = sim_check_court_common(sim, order->city_id, &city);
    if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
    u8 force = city->owner_force_id;
    officer_component *envoy = sim_find_officer(sim, order->officer);
    refusal = sim_check_develop_officer(sim, city, envoy);
    if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
    if (order->kind >= SIM_DIPLOMACY_KIND_COUNT) return sim_refuse(check, SIM_REFUSAL_NO_FACTION);
    if (order->force == force || !sim_faction_exists(sim, order->force)) return sim_refuse(check, SIM_REFUSAL_NO_FACTION);
    u8 relation = sim_relation(sim, force, order->force);
    switch (order->kind)
    {
        case SIM_DIPLOMACY_ALLIANCE:
            if (relation == RELATION_ALLIANCE) return sim_refuse(check, SIM_REFUSAL_RELATION);
            if (order->gold < rules->alliance_gold) return sim_refuse(check, SIM_REFUSAL_NO_AMOUNT);
            break;
        case SIM_DIPLOMACY_BREAK:
            if (relation != RELATION_ALLIANCE) return sim_refuse(check, SIM_REFUSAL_RELATION);
            break;
        case SIM_DIPLOMACY_CEASEFIRE:
            if (relation == RELATION_ALLIANCE) return sim_refuse(check, SIM_REFUSAL_RELATION);
            if (order->months < 2 || order->months > 24) return sim_refuse(check, SIM_REFUSAL_NO_AMOUNT);
            break;
        case SIM_DIPLOMACY_EXCHANGE:
        {
            officer_component *captive = sim_find_officer(sim, order->target);
            if (!captive || captive->status != SIM_STATUS_CAPTIVE || captive->force_id != order->force || captive->former_force != force)
            {
                return sim_refuse(check, SIM_REFUSAL_NOT_CAPTIVE);
            }
            break;
        }
        case SIM_DIPLOMACY_AID:
        {
            city_component *target = sim_find_city(sim, order->target_city);
            if (relation != RELATION_ALLIANCE) return sim_refuse(check, SIM_REFUSAL_RELATION);
            if (!target || target->owner_force_id != force) return sim_refuse(check, SIM_REFUSAL_NO_CITY);
            if (sim_aid_running(sim, order->force)) return sim_refuse(check, SIM_REFUSAL_LIMIT_REACHED);
            break;
        }
        default:
            break;
    }
    check.gold          = sim_diplomacy_gold(sim, city, order);
    check.action_points = sim_office_ap(sim, city, FACILITY_EFFECT_DIPLOMACY, rules->ap_diplomacy);
    check.quantity      = sim_diplomacy_chance(sim, force, envoy, order);
    check.value_before  = sim_friendliness(sim, force, order->force);
    if (city->gold < check.gold) return sim_refuse(check, SIM_REFUSAL_NO_GOLD);
    return sim_check_points(sim, force, check);
}

/* ---- the effects of diplomacy ------------------------------------------------------ */

/* The target faction joins: its cities, officers and units. */
internal void
sim_faction_joins(sim_state *sim, u8 winner, u8 loser, u32 cause, sim_event_sink *events)
{
    for (u32 index = 0; index < sim->cities.count; ++index)
    {
        if (sim->cities.data[index].owner_force_id == loser) sim->cities.data[index].owner_force_id = winner;
    }
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (officer->force_id != loser) continue;
        if (officer->status == SIM_STATUS_CAPTIVE) { officer->force_id = winner; continue; }
        if (!sim_officer_is_active(officer)) continue;
        officer->force_id = winner;
        if (officer->status <= OFFICER_STATUS_GOVERNOR) officer->status = OFFICER_STATUS_REGULAR_OFFICER;
        officer->loyalty = (u8)Minimum(sim->content.rules.recruit_new_loyalty, 100u);
    }
    for (u32 index = 0; index < sim->unit_count; ++index)
    {
        if (sim->units[index].alive && sim->units[index].force_id == loser) sim->units[index].force_id = winner;
    }
    sim_event event = sim_event_make(sim, SIM_EVENT_FACTION_SURRENDERED, cause);
    event.faction = winner;
    event.detail  = loser;
    sim_emit(events, event);
    if (sim->turns.human[loser])
    {
        sim->outcome.lost[loser] = true;
        sim_check_outcome(sim, cause, events);
    }
}

/* An aid unit of the ally on a free hex next to the city, for aid.turns. */
internal void
sim_send_aid(sim_state *sim, u8 ally, u8 city_id, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    officer_component *commander = 0;
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (!sim_officer_is_active(officer) || officer->force_id != ally || officer->status == OFFICER_STATUS_RULER ||
            officer->current_position >= SIM_UNIT_POSITION_BASE) continue;
        if (!commander || officer->leadership > commander->leadership) commander = officer;
    }
    u8 pike = content_find_unit_type(&sim->content, "pike");
    hex_coord center = sim->sites[city_id].hex;
    u32 unit_id = SIM_UNIT_CAPACITY;
    for (u32 index = 0; index < sim->unit_count && unit_id == SIM_UNIT_CAPACITY; ++index) if (!sim->units[index].alive) unit_id = index;
    if (unit_id == SIM_UNIT_CAPACITY && sim->unit_count < SIM_UNIT_CAPACITY) unit_id = sim->unit_count++;
    if (!commander || pike == CONTENT_NO_UNIT_TYPE || unit_id >= SIM_UNIT_CAPACITY) return;
    for (u32 d = 0; d < HEX_DIRECTION_COUNT; ++d)
    {
        hex_coord hex = hex_neighbor(center, d);
        if (!sim_entry_cost(sim, ally, MOVEMENT_FOOT, hex, SIM_NO_UNIT) || sim_city_at(sim, hex) != SIM_NO_CITY) continue;
        sim_unit *unit = sim->units + unit_id;
        *unit = {};
        unit->alive     = true;
        unit->force_id  = ally;
        unit->hex       = hex;
        unit->unit_type = pike;
        unit->officers[0] = commander->officer_id;
        unit->officers[1] = unit->officers[2] = SIM_NO_OFFICER;
        unit->soldiers  = rules->aid_soldiers;
        unit->weapons   = rules->aid_soldiers;
        unit->morale    = rules->city_start_morale;
        unit->grain     = rules->aid_soldiers;
        unit->home_city = sim_refuge_city(sim, ally, SIM_NO_CITY);
        unit->destination = SIM_NO_CITY;
        unit->aid_turns = (u16)rules->aid_turns;
        commander->current_position = (u16)(SIM_UNIT_POSITION_BASE + unit_id);
        sim_event event = sim_event_make(sim, SIM_EVENT_AID_ARRIVED, cause);
        event.faction = ally;
        event.detail  = unit_id;
        sim_emit(events, event);
        return;
    }
}

/* The result of a diplomacy command, after the roll or the debate. */
internal void
sim_diplomacy_result(sim_state *sim, u8 force, sim_court_order *order, b32 success, u32 cause, sim_event_sink *events)
{
    sim_rules *rules = &sim->content.rules;
    officer_component *envoy = sim_find_officer(sim, order->officer);
    u8 other = order->force;
    switch (order->kind)
    {
        case SIM_DIPLOMACY_GOODWILL:
            sim_change_friendliness(sim, force, other, (s32)(rules->goodwill_base + (envoy ? envoy->politics : 0) * rules->goodwill_politics_percent / 100),
                                    cause, events);
            sim_add_technique_points(sim, force, (s32)rules->technique_goodwill, cause, events);
            break;
        case SIM_DIPLOMACY_BREAK:
            sim_set_relation(sim, force, other, RELATION_NONE, 0, cause, events);
            sim_change_friendliness(sim, force, other, -(s32)rules->break_loss, cause, events);
            /* 解除同盟 gives technique points to the side that proposes it (tech-tree.md: 50; claimed). */
            sim_add_technique_points(sim, force, (s32)rules->break_technique, cause, events);
            break;
        default:
            if (!success)
            {
                sim_change_friendliness(sim, force, other, -(s32)rules->diplomacy_fail_loss, cause, events);
                sim_event failed = sim_event_make(sim, SIM_EVENT_DIPLOMACY_FAILED, cause);
                failed.faction = force;
                failed.detail  = other;
                failed.assumed = true;
                sim_emit(events, failed);
                break;
            }
            if (order->kind == SIM_DIPLOMACY_ALLIANCE)  sim_set_relation(sim, force, other, RELATION_ALLIANCE, 0, cause, events);
            if (order->kind == SIM_DIPLOMACY_CEASEFIRE) sim_set_relation(sim, force, other, RELATION_CEASEFIRE, order->months, cause, events);
            if (order->kind == SIM_DIPLOMACY_SURRENDER) sim_faction_joins(sim, force, other, cause, events);
            if (order->kind == SIM_DIPLOMACY_AID)       sim_send_aid(sim, other, order->target_city, cause, events);
            if (order->kind == SIM_DIPLOMACY_EXCHANGE)
            {
                officer_component *captive = sim_find_officer(sim, order->target);
                u8 refuge = sim_refuge_city(sim, force, order->city_id);
                captive->status           = OFFICER_STATUS_REGULAR_OFFICER;
                captive->force_id         = force;
                captive->former_force     = SIM_NO_FORCE;
                captive->current_position = refuge == SIM_NO_CITY ? order->city_id : refuge;
                captive->base_location    = captive->current_position;
                sim_emit_officer(sim, SIM_EVENT_CAPTIVE_EXCHANGED, captive, 0, other, false, cause, events);
            }
            /* The technique points of the kind (tech-tree.md, the table by action; claimed). */
            u32 points = order->kind == SIM_DIPLOMACY_ALLIANCE  ? rules->technique_alliance :
                         order->kind == SIM_DIPLOMACY_CEASEFIRE ? rules->technique_ceasefire :
                         order->kind == SIM_DIPLOMACY_SURRENDER ? rules->technique_surrender :
                         order->kind == SIM_DIPLOMACY_AID       ? rules->technique_aid :
                         order->kind == SIM_DIPLOMACY_EXCHANGE  ? rules->technique_exchange_sender : 0;
            sim_add_technique_points(sim, force, (s32)points, cause, events);
            if (order->kind == SIM_DIPLOMACY_EXCHANGE) sim_add_technique_points(sim, other, (s32)rules->technique_exchange_releaser, cause, events);
            break;
    }
    if (envoy) sim_reward_officer(sim, force, envoy->officer_id, 0, CONTENT_ABILITY_POLITICS, cause, events);
}

internal void
sim_pay_command(sim_state *sim, city_component *city, sim_check *check, u32 cause, sim_event_sink *events)
{
    if (check->gold)
    {
        city->gold -= check->gold;
        sim_emit_city(sim, SIM_EVENT_GOLD_SPENT, city, -(s32)check->gold, 0, false, cause, events);
    }
    if (check->action_points)
    {
        sim->turns.action_points[city->owner_force_id] -= check->action_points;
        sim_event points = sim_event_make(sim, SIM_EVENT_ACTION_POINTS_SPENT, cause);
        points.faction = city->owner_force_id;
        points.delta   = -(s32)check->action_points;
        sim_emit(events, points);
    }
}

/* 外交: an envoy with 論客 goes to a debate for 同盟, 停戰協定 and 交換俘虜
   (skills.md #87); the others roll the chance. */
internal void
sim_diplomacy(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_court_order *order = &command.court;
    sim_check check = sim_check_diplomacy(sim, order);
    if (check.refusal != SIM_REFUSAL_NONE) return;
    city_component *city = sim_find_city(sim, order->city_id);
    u8 force = city->owner_force_id;
    officer_component *envoy = sim_find_officer(sim, order->officer);
    envoy->acted_turn = sim->turn + 1;
    sim_pay_command(sim, city, &check, command.id, events);
    /* G2-063: the envoy goes on the road for the days of 抵達日數 (travel.cpp). The result comes at the arrival. */
    sim_start_mission(sim, order, force, check.quantity, MISSION_DIPLOMACY, command.id, events);
}

/* The result of a diplomacy command at the arrival of the envoy. An envoy
   with 論客 of a player goes to a debate for 同盟, 停戰協定 and 交換俘虜
   (skills.md #87); the others roll the chance of the order. A target that
   is gone, a captive that is gone or a city that changed hands gives no
   result. */
internal void
sim_resolve_diplomacy(sim_state *sim, sim_mission *mission, sim_event_sink *events)
{
    sim_court_order *order = &mission->order;
    u8 force = mission->force;
    u32 cause = mission->command_id;
    officer_component *envoy = sim_find_officer(sim, order->officer);
    sim_event arrived = sim_event_make(sim, SIM_EVENT_ENVOY_ARRIVED, cause);
    arrived.subject = envoy ? sim_officer_entity(sim, envoy->officer_id) : 0;
    arrived.faction = force;
    arrived.detail  = order->force;
    arrived.assumed = true;
    sim_emit(events, arrived);
    if (!envoy || !sim_officer_is_active(envoy) || envoy->force_id != force) return;
    if (!sim_faction_exists(sim, force) || !sim_faction_exists(sim, order->force)) return;
    if (order->kind == SIM_DIPLOMACY_EXCHANGE)
    {
        officer_component *captive = sim_find_officer(sim, order->target);
        if (!captive || captive->status != SIM_STATUS_CAPTIVE || captive->force_id != order->force) return;
    }
    if (order->kind == SIM_DIPLOMACY_AID)
    {
        city_component *target = sim_find_city(sim, order->target_city);
        if (!target || target->owner_force_id != force) return;
    }
    b32 debater = sim_officer_has_skill(envoy, sim->content.rules.skill_debater) &&
                  (order->kind == SIM_DIPLOMACY_ALLIANCE || order->kind == SIM_DIPLOMACY_CEASEFIRE || order->kind == SIM_DIPLOMACY_EXCHANGE);
    u16 their_ruler = sim_force_ruler(sim, order->force);
    if (debater && their_ruler != SIM_NO_OFFICER && force < SIM_FORCE_CAPACITY && sim->turns.human[force] && !sim_busy(sim))
    {
        sim_begin_debate(sim, envoy->officer_id, their_ruler, DEBATE_FOR_DIPLOMACY, order, force, cause, events);
        return;
    }
    b32 success = mission->chance >= 100 || sim_random_below(sim, 100) < mission->chance;
    sim_diplomacy_result(sim, force, order, success, cause, events);
}

/* ---- the plots of factions ---------------------------------------------------------- */

/* The first force id with no ruler and no city, for a new faction. */
internal u8
sim_free_force_id(sim_state *sim)
{
    for (u32 force = 0; force < SIM_DIPLOMACY_FORCES - 1; ++force)
    {
        if (sim_force_ruler(sim, (u8)force) == SIM_NO_OFFICER && sim_faction_city_count(sim, (u8)force) == 0) return (u8)force;
    }
    return SIM_NO_FORCE;
}

internal sim_check
sim_check_faction_plot(sim_state *sim, sim_court_order *order)
{
    sim_check check = {};
    check.slot    = SIM_ANY_SLOT;
    check.assumed = true;
    sim_rules *rules = &sim->content.rules;
    city_component *city = 0;
    sim_refusal refusal = sim_check_court_common(sim, order->city_id, &city);
    if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
    u8 force = city->owner_force_id;
    officer_component *actor = sim_find_officer(sim, order->officer);
    refusal = sim_check_develop_officer(sim, city, actor);
    if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
    s32 chance = 0;
    b32 office = sim_city_has_built(sim, city->city_id, FACILITY_EFFECT_PLOT);
    switch (order->kind)
    {
        case SIM_PLOT_TWO_TIGERS:
        {
            if (order->force == order->force_2 || order->force == force || order->force_2 == force ||
                !sim_faction_exists(sim, order->force) || !sim_faction_exists(sim, order->force_2))
            {
                return sim_refuse(check, SIM_REFUSAL_NO_FACTION);
            }
            officer_component *a = sim_find_officer(sim, sim_force_ruler(sim, order->force));
            officer_component *b = sim_find_officer(sim, sim_force_ruler(sim, order->force_2));
            s32 best = (s32)Maximum(a->intelligence, b->intelligence);
            chance = (s32)rules->two_tigers_base + (s32)actor->intelligence - best;
            break;
        }
        case SIM_PLOT_DRIVE_TIGER:
        case SIM_PLOT_RUMOUR:
        {
            city_component *target = sim_find_city(sim, order->target_city);
            if (!target || target->owner_force_id == force || target->owner_force_id == SIM_NO_FORCE) return sim_refuse(check, SIM_REFUSAL_NO_CITY);
            officer_component *governor = sim_find_officer(sim, sim_city_governor(sim, target->city_id));
            if (!governor) return sim_refuse(check, SIM_REFUSAL_NO_OFFICER);
            if (order->kind == SIM_PLOT_DRIVE_TIGER)
            {
                if (governor->status == OFFICER_STATUS_RULER) return sim_refuse(check, SIM_REFUSAL_NOT_RECRUITABLE);
                if (sim_free_force_id(sim) == SIM_NO_FORCE) return sim_refuse(check, SIM_REFUSAL_NO_FACTION);
                chance = (s32)rules->drive_tiger_base + actor->intelligence / 4 + 10 * governor->ambition + (100 - governor->loyalty) / 2 -
                         governor->intelligence / 4;
            }
            else
            {
                chance = (s32)rules->rumour_base + (s32)actor->intelligence - (s32)governor->intelligence + (office ? 10 : 0);
            }
            break;
        }
        default:
            return sim_refuse(check, SIM_REFUSAL_NO_FACTION);
    }
    check.gold          = rules->faction_plot_gold;
    check.action_points = office ? rules->ap_faction_plot / 2 : rules->ap_faction_plot;
    check.quantity      = (u32)sim_clamp_percent(chance);
    if (city->gold < check.gold) return sim_refuse(check, SIM_REFUSAL_NO_GOLD);
    return sim_check_points(sim, force, check);
}

/* 驅虎吞狼: the governor becomes the ruler of a new faction with its city
   and the officers of its old faction there. */
internal void
sim_found_faction(sim_state *sim, city_component *city, officer_component *governor, u32 cause, sim_event_sink *events)
{
    u8 old = city->owner_force_id;
    u8 fresh = sim_free_force_id(sim);
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (officer->force_id == old && sim_officer_is_active(officer) && officer->current_position == city->city_id &&
            officer->status != OFFICER_STATUS_RULER)
        {
            officer->force_id = fresh;
            officer->status   = OFFICER_STATUS_REGULAR_OFFICER;
        }
    }
    sim_clear_post(sim, governor);
    governor->status  = OFFICER_STATUS_RULER;
    governor->loyalty = 100;
    city->owner_force_id = fresh;
    for (u32 other = 0; other < SIM_DIPLOMACY_FORCES; ++other)
    {
        if (other == fresh) continue;
        u8 start = other == old ? 0 : (u8)Minimum(sim->content.rules.friendliness_start, 100u);
        sim->diplomacy.friendliness[fresh][other] = sim->diplomacy.friendliness[other][fresh] = start;
        sim->diplomacy.relation[fresh][other] = sim->diplomacy.relation[other][fresh] = RELATION_NONE;
    }
    sim->rank[fresh] = sim_rank_for(sim, fresh);
    sim_event event = sim_event_make(sim, SIM_EVENT_FACTION_FOUNDED, cause);
    event.subject = sim_city_entity(sim, city->city_id);
    event.faction = fresh;
    event.detail  = old;
    sim_emit(events, event);
}

/* The order of a plot: the officer acts, the city pays, and the plot goes
   on the road (travel.cpp). The result comes at the arrival (G2-065). */
internal void
sim_faction_plot(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_court_order *order = &command.court;
    sim_check check = sim_check_faction_plot(sim, order);
    if (check.refusal != SIM_REFUSAL_NONE) return;
    city_component *city = sim_find_city(sim, order->city_id);
    u8 force = city->owner_force_id;
    officer_component *actor = sim_find_officer(sim, order->officer);
    actor->acted_turn = sim->turn + 1;
    sim_pay_command(sim, city, &check, command.id, events);
    sim_start_mission(sim, order, force, check.quantity, MISSION_PLOT, command.id, events);
}

/* The result of a plot at the arrival of its officer. The events carry the
   command of the plot as their cause, so that the report names the
   officer. A target that changed hands on the way gives a failure. */
internal void
sim_resolve_plot(sim_state *sim, sim_mission *mission, sim_event_sink *events)
{
    sim_court_order *order = &mission->order;
    sim_rules *rules = &sim->content.rules;
    u8 force = mission->force;
    u32 cause = mission->command_id;
    city_component *target = sim_find_city(sim, order->target_city);
    b32 valid = order->kind == SIM_PLOT_TWO_TIGERS ? sim_faction_exists(sim, order->force) && sim_faction_exists(sim, order->force_2)
                                                   : target && target->owner_force_id != force && target->owner_force_id != SIM_NO_FORCE;
    b32 success = valid && (mission->chance >= 100 || sim_random_below(sim, 100) < mission->chance);
    sim_event event = sim_event_make(sim, success ? SIM_EVENT_FACTION_PLOT : SIM_EVENT_FACTION_PLOT_FAILED, cause);
    event.faction = force;
    event.detail  = order->kind;
    event.delta   = success ? 1 : 0;
    event.subject = target ? sim_city_entity(sim, target->city_id) : 0;
    event.assumed = true;
    sim_emit(events, event);
    if (!valid) return;
    if (!success)
    {
        u8 victim = order->kind == SIM_PLOT_TWO_TIGERS ? order->force : target->owner_force_id;
        sim_change_friendliness(sim, force, victim, -(s32)rules->diplomacy_fail_loss, cause, events);
        if (order->kind == SIM_PLOT_TWO_TIGERS) sim_change_friendliness(sim, force, order->force_2, -(s32)rules->diplomacy_fail_loss, cause, events);
    }
    else if (order->kind == SIM_PLOT_TWO_TIGERS)
    {
        sim_change_friendliness(sim, order->force, order->force_2, -(s32)rules->two_tigers_loss, cause, events);
        if (sim_relation(sim, order->force, order->force_2) == RELATION_ALLIANCE)
        {
            sim_set_relation(sim, order->force, order->force_2, RELATION_NONE, 0, cause, events);
        }
    }
    else if (order->kind == SIM_PLOT_DRIVE_TIGER)
    {
        officer_component *governor = sim_find_officer(sim, sim_city_governor(sim, target->city_id));
        if (governor && governor->status != OFFICER_STATUS_RULER && sim_free_force_id(sim) != SIM_NO_FORCE)
        {
            sim_found_faction(sim, target, governor, cause, events);
        }
    }
    else
    {
        u32 lost = Minimum((u32)target->public_order, rules->rumour_order_loss);
        target->public_order = (u8)(target->public_order - lost);
        if (lost) sim_emit_city(sim, SIM_EVENT_ORDER_DROPPED, target, -(s32)lost, 0, true, cause, events);
        for (u32 index = 0; index < sim->officers.count; ++index)
        {
            officer_component *officer = sim->officers.data + index;
            if (sim_officer_is_active(officer) && officer->force_id == target->owner_force_id && officer->current_position == target->city_id &&
                officer->status != OFFICER_STATUS_RULER)
            {
                sim_change_loyalty(sim, officer, -(s32)rules->rumour_loyalty_loss, cause, events);
            }
        }
    }
    /* A plot that succeeds gives the technique points of its kind (tech-tree.md; claimed). */
    u32 points = order->kind == SIM_PLOT_TWO_TIGERS  ? rules->technique_two_tigers :
                 order->kind == SIM_PLOT_DRIVE_TIGER ? rules->technique_drive_tiger : rules->technique_rumour;
    if (success) sim_add_technique_points(sim, force, (s32)points, cause, events);
    if (sim_find_officer(sim, order->officer)) sim_reward_officer(sim, force, order->officer, 0, CONTENT_ABILITY_INTELLECT, cause, events);
}
