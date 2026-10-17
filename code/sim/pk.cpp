/* The officer rules of M9 (game-setup-and-modes.md, pk-officer-training.md):
   新武將 and the ancient officers, 研究能力 and 培育武將. Included by
   sim.cpp after ai.cpp.

   The costs of research and training, the 30 days away, the counts 1, 3
   and 5 and the limit of 8 new officers are documented. The chart rows,
   the abilities of a type and the ancient officers are assumed (Q-040,
   Q-188). */

/* ---- 新武將 -------------------------------------------------------------------------- */

/* The free officer slot with the highest id: the save leaves the last
   slots empty, and the officers of the scenario keep their ids. */
internal u16
sim_free_officer_slot(sim_state *sim)
{
    for (u32 id = SAVE_OFFICER_SLOTS; id-- > 0;)
    {
        if (sim->officer_index[id] == SIM_NO_OFFICER) return (u16)id;
    }
    return SIM_NO_OFFICER;
}

internal sim_check
sim_check_create_officer(sim_state *sim, sim_new_officer_order *order)
{
    sim_check check = {};
    check.slot = SIM_ANY_SLOT;
    if (!sim->has_content) return sim_refuse(check, SIM_REFUSAL_NO_CONTENT);
    if (sim_game_over(sim)) return sim_refuse(check, SIM_REFUSAL_GAME_OVER);
    if (sim->new_officer_count >= sim->content.rules.new_officer_max) return sim_refuse(check, SIM_REFUSAL_LIMIT_REACHED);
    city_component *city = sim_find_city(sim, order->city_id);
    if (!city) return sim_refuse(check, SIM_REFUSAL_NO_CITY);
    if (order->found_force && city->owner_force_id != SIM_NO_FORCE) return sim_refuse(check, SIM_REFUSAL_NO_CITY);   /* a new faction needs an empty city */
    if (order->ancient != SIM_NO_ANCIENT && order->ancient >= sim->content.ancient_count) return sim_refuse(check, SIM_REFUSAL_NO_OFFICER);
    if (order->ancient == SIM_NO_ANCIENT && (!order->family_name[0] || order->type >= OFFICER_TYPE_COUNT || order->personality >= PERSONALITY_COUNT))
    {
        return sim_refuse(check, SIM_REFUSAL_NO_OFFICER);
    }
    if (order->found_force && sim_free_force_id(sim) == SIM_NO_FORCE) return sim_refuse(check, SIM_REFUSAL_NO_FACTION);
    if (sim_free_officer_slot(sim) == SIM_NO_OFFICER) return sim_refuse(check, SIM_REFUSAL_LIMIT_REACHED);
    check.quantity = sim->content.rules.new_officer_max - sim->new_officer_count;
    return check;
}

/* The abilities of a type (guide book p.52): high, normal or low in the
   order leadership, force, intellect, politics, charm. */
internal void
sim_type_abilities(sim_state *sim, u8 type, u8 *abilities)
{
    static const u8 grades[OFFICER_TYPE_COUNT][CONTENT_ABILITY_COUNT] =
    {
        { 2, 2, 2, 2, 2 },   /* 萬能 */
        { 1, 1, 2, 2, 2 },   /* 軍師 */
        { 2, 2, 1, 1, 2 },   /* 將軍 */
        { 0, 0, 0, 0, 0 },   /* 凡才 */
    };
    sim_rules *rules = &sim->content.rules;
    u32 values[3] = { rules->new_officer_low, rules->new_officer_normal, rules->new_officer_high };
    for (u32 ability = 0; ability < CONTENT_ABILITY_COUNT; ++ability) abilities[ability] = (u8)values[grades[type][ability]];
}

internal void
sim_create_officer_command(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_new_officer_order *order = &command.newcomer;
    if (sim_check_create_officer(sim, order).refusal != SIM_REFUSAL_NONE) return;
    officer_record record = {};
    record.id = sim_free_officer_slot(sim);
    u8 abilities[CONTENT_ABILITY_COUNT];
    u8 personality = order->personality, skill = 0;
    b32 female = order->female;
    if (order->ancient != SIM_NO_ANCIENT)
    {
        ancient_officer *ancient = sim->content.ancients + order->ancient;
        memcpy(record.family_name, ancient->family_name, OFFICER_FAMILY_NAME_SIZE);
        memcpy(record.given_name, ancient->given_name, OFFICER_GIVEN_NAME_SIZE);
        memcpy(abilities, ancient->abilities, CONTENT_ABILITY_COUNT);
        personality = ancient->personality;
        skill       = ancient->skill;
        female      = ancient->female;
    }
    else
    {
        memcpy(record.family_name, order->family_name, OFFICER_FAMILY_NAME_SIZE);
        memcpy(record.given_name, order->given_name, OFFICER_GIVEN_NAME_SIZE);
        sim_type_abilities(sim, order->type, abilities);
    }
    u16 year = sim->turns.date.year;
    record.birth_year      = order->birth_year ? order->birth_year : (u16)(year - 20);
    record.appearance_year = year;
    record.death_year      = (u16)(record.birth_year + 70);   /* assumed (Q-188) */
    record.leadership      = abilities[CONTENT_ABILITY_LEADERSHIP];
    record.war             = abilities[CONTENT_ABILITY_FORCE];
    record.intelligence    = abilities[CONTENT_ABILITY_INTELLECT];
    record.politics        = abilities[CONTENT_ABILITY_POLITICS];
    record.charm           = abilities[CONTENT_ABILITY_CHARM];
    record.force_id        = SIM_NO_FORCE;
    record.base_location   = order->city_id;
    record.current_position = order->city_id;
    record.status          = OFFICER_STATUS_FREE_OR_NOT_APPEARED;
    record.skill_number    = skill ? (u8)(skill - 1) : (u8)255;   /* the save stores the number - 1 */
    record.compatibility   = (u8)(sim_random_below(sim, 150));
    record.father = record.spouse = record.sworn_brother_group_head = SIM_NO_OFFICER;
    for (u32 slot = 0; slot < OFFICER_UNIT_APTITUDE_COUNT; ++slot) record.unit_aptitudes[slot] = 1;   /* B (assumed) */
    if (!sim_add_officer(sim, sim_officer_from_record(&record))) return;
    officer_component *officer = sim_find_officer(sim, record.id);
    officer->female      = female;
    officer->personality = personality;
    officer->created     = true;
    sim->new_officer_count++;
    u8 founded = SIM_NO_FORCE;
    if (order->found_force)
    {
        city_component *city = sim_find_city(sim, order->city_id);
        sim_found_faction(sim, city, officer, command.id, events);
        officer->force_id = city->owner_force_id;
        founded = city->owner_force_id;
    }
    sim_event event = sim_event_make(sim, SIM_EVENT_OFFICER_CREATED, command.id);
    event.subject = sim_officer_entity(sim, officer->officer_id);
    event.detail  = order->city_id;
    event.faction = founded;
    sim_emit(events, event);
}

/* ---- 研究能力 ------------------------------------------------------------------------ */

internal sim_check
sim_check_research_ability(sim_state *sim, sim_pk_order *order)
{
    sim_check check = {};
    check.slot = SIM_ANY_SLOT;
    if (!sim->has_content) return sim_refuse(check, SIM_REFUSAL_NO_CONTENT);
    if (sim_game_over(sim)) return sim_refuse(check, SIM_REFUSAL_GAME_OVER);
    if (sim_busy(sim)) return sim_refuse(check, SIM_REFUSAL_BUSY);
    city_component *city = sim_find_city(sim, order->city_id);
    if (!city || city->owner_force_id == SIM_NO_FORCE) return sim_refuse(check, SIM_REFUSAL_NO_CITY);
    u8 force = city->owner_force_id;
    if (sim_whose_turn(sim) != force) return sim_refuse(check, SIM_REFUSAL_NOT_YOUR_TURN);
    if (order->row >= sim->content.chart_count) return sim_refuse(check, SIM_REFUSAL_NO_CHART_ROW);
    chart_row *row = sim->content.chart + order->row;
    if ((sim->ability_known[force] & row->requires) != row->requires) return sim_refuse(check, SIM_REFUSAL_NEEDS_TECHNIQUE);
    if (sim->ability_job[force].row != CONTENT_NO_CHART_ROW) return sim_refuse(check, SIM_REFUSAL_LIMIT_REACHED);   /* one research at a time */
    sim_rules *rules = &sim->content.rules;
    if (sim->turns.action_points[force] < rules->ability_research_ap) return sim_refuse(check, SIM_REFUSAL_NO_ACTION_POINTS);
    if (city->gold < rules->ability_research_gold) return sim_refuse(check, SIM_REFUSAL_NO_GOLD);
    check.gold          = rules->ability_research_gold;
    check.action_points = rules->ability_research_ap;
    check.quantity      = row->months * 3;   /* the turns: three in a month */
    return check;
}

internal void
sim_research_ability_command(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_check check = sim_check_research_ability(sim, &command.pk);
    if (check.refusal != SIM_REFUSAL_NONE) return;
    city_component *city = sim_find_city(sim, command.pk.city_id);
    u8 force = city->owner_force_id;
    sim_pay_command(sim, city, &check, command.id, events);
    sim_ability_job *job = sim->ability_job + force;
    job->row        = command.pk.row;
    job->city       = city->city_id;
    job->turns_left = (u16)check.quantity;
    sim_event event = sim_event_make(sim, SIM_EVENT_ABILITY_RESEARCH_STARTED, command.id);
    event.faction = force;
    event.detail  = command.pk.row;
    event.delta   = (s32)check.quantity;
    sim_emit(events, event);
}

/* ---- 培育武將 ------------------------------------------------------------------------ */

/* True if the row can still raise the officer: a floor above the ability,
   an aptitude one grade below the target, a plus below 100. */
internal b32
sim_chart_fits(sim_state *sim, chart_row *row, officer_component *officer)
{
    (void)sim;
    switch (row->kind)
    {
        case CHART_PLUS:     { u8 *field = sim_ability_field(officer, row->target); return field && *field < 100; }
        case CHART_FLOOR:    { u8 *field = sim_ability_field(officer, row->target); return field && *field < row->amount; }
        case CHART_APTITUDE: return row->target < OFFICER_UNIT_APTITUDE_COUNT && officer->unit_aptitudes[row->target] + 1 == row->amount;
        case CHART_SKILL:    return officer->skill_number != (u8)(row->target - 1);
        default:             return false;
    }
}

internal sim_check
sim_check_train_officer(sim_state *sim, sim_pk_order *order)
{
    sim_check check = {};
    check.slot = SIM_ANY_SLOT;
    if (!sim->has_content) return sim_refuse(check, SIM_REFUSAL_NO_CONTENT);
    if (sim_game_over(sim)) return sim_refuse(check, SIM_REFUSAL_GAME_OVER);
    if (sim_busy(sim)) return sim_refuse(check, SIM_REFUSAL_BUSY);
    officer_component *officer = sim_find_officer(sim, order->officer);
    if (!officer || !sim_officer_is_active(officer) || officer->current_position >= SAVE_CITY_COUNT) return sim_refuse(check, SIM_REFUSAL_NO_OFFICER);
    u8 force = officer->force_id;
    if (sim_whose_turn(sim) != force) return sim_refuse(check, SIM_REFUSAL_NOT_YOUR_TURN);
    city_component *city = sim_find_city(sim, (u8)officer->current_position);
    sim_refusal refusal = city ? sim_check_develop_officer(sim, city, officer) : SIM_REFUSAL_OFFICER_NOT_IN_CITY;
    if (refusal != SIM_REFUSAL_NONE) return sim_refuse(check, refusal);
    if (order->row >= sim->content.chart_count) return sim_refuse(check, SIM_REFUSAL_NO_CHART_ROW);
    chart_row *row = sim->content.chart + order->row;
    if (!sim->ability_uses[force][order->row]) return sim_refuse(check, SIM_REFUSAL_NO_USES);
    if (!sim_chart_fits(sim, row, officer)) return sim_refuse(check, SIM_REFUSAL_VALUE_AT_MAXIMUM);
    b32 base = row->kind == CHART_PLUS || row->kind == CHART_FLOOR;
    for (u32 index = 0; index < sim->officers.count && base; ++index)
    {
        officer_component *other = sim->officers.data + index;
        if (other->force_id != force || !other->training_turns || other->training_row >= sim->content.chart_count) continue;
        u8 kind = sim->content.chart[other->training_row].kind;
        if (kind == CHART_PLUS || kind == CHART_FLOOR) return sim_refuse(check, SIM_REFUSAL_LIMIT_REACHED);   /* one base ability at a time */
    }
    if (sim->turns.action_points[force] < sim->content.rules.train_ap) return sim_refuse(check, SIM_REFUSAL_NO_ACTION_POINTS);
    check.action_points = sim->content.rules.train_ap;
    check.quantity      = sim->content.rules.train_turns;
    return check;
}

internal void
sim_train_officer_command(sim_state *sim, sim_command command, sim_event_sink *events)
{
    sim_check check = sim_check_train_officer(sim, &command.pk);
    if (check.refusal != SIM_REFUSAL_NONE) return;
    officer_component *officer = sim_find_officer(sim, command.pk.officer);
    city_component *city = sim_find_city(sim, (u8)officer->current_position);
    sim_pay_command(sim, city, &check, command.id, events);
    sim->ability_uses[officer->force_id][command.pk.row]--;
    officer->training_turns  = (u8)Maximum(check.quantity, 1u);
    officer->training_row    = command.pk.row;
    officer->current_position = 0xFFFF;   /* away for 30 days: no command reaches the officer */
    officer->acted_turn      = sim->turn + 1;
    sim_emit_officer(sim, SIM_EVENT_TRAINING_STARTED, officer, 0, command.pk.row, false, command.id, events);
}

/* The officer returns to its base and gets the ability of the row. */
internal void
sim_finish_training(sim_state *sim, officer_component *officer, u32 cause, sim_event_sink *events)
{
    chart_row *row = sim->content.chart + officer->training_row;
    officer->current_position = officer->base_location;
    officer->training_turns   = 0;
    s32 rise = 0;
    switch (row->kind)
    {
        case CHART_PLUS:
        {
            u8 *field = sim_ability_field(officer, row->target);
            rise = (s32)Minimum((u32)row->amount, 100u - Minimum((u32)*field, 100u));
            sim_change_ability(sim, officer, row->target, rise, false, cause, events);
        } break;
        case CHART_FLOOR:
        {
            u8 *field = sim_ability_field(officer, row->target);
            if (*field < row->amount) rise = (s32)row->amount - (s32)*field;
            sim_change_ability(sim, officer, row->target, rise, false, cause, events);
        } break;
        case CHART_APTITUDE:
        {
            if (officer->unit_aptitudes[row->target] < row->amount) { rise = row->amount - officer->unit_aptitudes[row->target]; officer->unit_aptitudes[row->target] = row->amount; }
        } break;
        case CHART_SKILL:
        {
            officer->skill_number = (u8)(row->target - 1);
            rise = 1;
        } break;
    }
    sim_emit_officer(sim, SIM_EVENT_OFFICER_TRAINED, officer, rise, officer->training_row, false, cause, events);
}

/* Each turn: the research of each faction and the officers in training. */
internal void
sim_progress_pk(sim_state *sim, u32 cause, sim_event_sink *events)
{
    for (u32 force = 0; force < SIM_FORCE_CAPACITY; ++force)
    {
        sim_ability_job *job = sim->ability_job + force;
        if (job->row == CONTENT_NO_CHART_ROW || job->row >= sim->content.chart_count) continue;
        if (job->turns_left > 1) { job->turns_left--; continue; }
        chart_row *row = sim->content.chart + job->row;
        sim->ability_uses[force][job->row] = (u8)Minimum((u32)sim->ability_uses[force][job->row] + row->count, 255u);
        sim->ability_known[force] |= 1u << job->row;
        sim_event event = sim_event_make(sim, SIM_EVENT_ABILITY_RESEARCHED, cause);
        event.faction = (u8)force;
        event.detail  = job->row;
        event.delta   = row->count;
        sim_emit(events, event);
        job->row = CONTENT_NO_CHART_ROW;
    }
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (!officer->training_turns) continue;
        if (officer->training_row >= sim->content.chart_count || !sim_officer_is_active(officer)) { officer->training_turns = 0; continue; }
        if (--officer->training_turns == 0) sim_finish_training(sim, officer, cause, events);
    }
}

internal void
sim_reset_pk(sim_state *sim)
{
    sim->new_officer_count = 0;
    for (u32 force = 0; force < SIM_FORCE_CAPACITY; ++force)
    {
        sim->ability_known[force] = 0;
        sim->ability_job[force]   = {};
        sim->ability_job[force].row = CONTENT_NO_CHART_ROW;
        for (u32 row = 0; row < CONTENT_CHART_CAPACITY; ++row) sim->ability_uses[force][row] = 0;
    }
}
