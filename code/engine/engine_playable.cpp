/* The engine_api of M9 (playable): new officers and the ancient officers,
   the PK ability chart, research of abilities and training of officers.
   Included by engine.cpp, before engine_make_api. */

/* ---- the chart and the ancient officers ------------------------------------------------ */

internal u32
engine_api_chart_count(void *opaque)
{
    return ((engine *)opaque)->sim->content.chart_count;
}

internal engine_chart_row
engine_api_chart_row_at(void *opaque, u32 index)
{
    sim_content *content = &((engine *)opaque)->sim->content;
    engine_chart_row result = {};
    result.id = ENGINE_NO_ROW;
    if (index >= content->chart_count) return result;
    chart_row *row = content->chart + index;
    result.id       = row->id;
    memcpy(result.key, row->key, sizeof(result.key));
    result.zone     = row->zone;
    result.kind     = row->kind;
    result.target   = row->target;
    result.amount   = row->amount;
    result.count    = row->count;
    result.months   = row->months;
    result.requires = row->requires;
    return result;
}

internal engine_faction_chart
engine_api_faction_chart(void *opaque, u8 force_id)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_faction_chart result = {};
    result.job_row  = ENGINE_NO_ROW;
    result.job_city = ENGINE_NO_CITY;
    if (force_id >= SIM_FORCE_CAPACITY) return result;
    memcpy(result.uses, sim->ability_uses[force_id], sizeof(result.uses));
    result.known = sim->ability_known[force_id];
    sim_ability_job *job = sim->ability_job + force_id;
    if (job->row != CONTENT_NO_CHART_ROW)
    {
        result.job_row    = job->row;
        result.job_city   = job->city;
        result.turns_left = job->turns_left;
    }
    return result;
}

internal u32
engine_api_ancient_count(void *opaque)
{
    return ((engine *)opaque)->sim->content.ancient_count;
}

internal engine_ancient
engine_api_ancient_at(void *opaque, u32 index)
{
    sim_content *content = &((engine *)opaque)->sim->content;
    engine_ancient result = {};
    result.id = ENGINE_NO_ROW;
    if (index >= content->ancient_count) return result;
    ancient_officer *row = content->ancients + index;
    result.id          = row->id;
    memcpy(result.key, row->key, sizeof(result.key));
    memcpy(result.family_name, row->family_name, sizeof(result.family_name));
    memcpy(result.given_name, row->given_name, sizeof(result.given_name));
    memcpy(result.abilities, row->abilities, sizeof(result.abilities));
    result.skill       = row->skill;
    result.personality = row->personality;
    result.female      = row->female;
    return result;
}

internal u32
engine_api_new_officer_count(void *opaque)
{
    return ((engine *)opaque)->sim->new_officer_count;
}

/* ---- the commands ---------------------------------------------------------------------- */

internal engine_command_result
engine_api_create_officer(void *opaque, const sim_new_officer_order *order)
{
    engine *e = (engine *)opaque;
    engine_command_result result = {};
    if (!order) { result.refusal = SIM_REFUSAL_NO_OFFICER; return result; }
    sim_new_officer_order copy = *order;
    if (copy.city_id == ENGINE_NO_CITY) copy.city_id = SIM_NO_CITY;
    result.refusal = sim_check_create_officer(e->sim, &copy).refusal;
    if (result.refusal != SIM_REFUSAL_NONE) return result;
    sim_command command = sim_make_command(e->sim, SIM_COMMAND_CREATE_OFFICER);
    command.newcomer = copy;
    engine_run(e, command, 0);
    result.command_id = command.id;
    return result;
}

internal engine_command_result
engine_run_pk(engine *e, sim_command_kind kind, sim_pk_order order)
{
    engine_command_result result = {};
    result.refusal = kind == SIM_COMMAND_RESEARCH_ABILITY ? sim_check_research_ability(e->sim, &order).refusal : sim_check_train_officer(e->sim, &order).refusal;
    if (result.refusal != SIM_REFUSAL_NONE) return result;
    sim_command command = sim_make_command(e->sim, kind);
    command.pk = order;
    engine_run(e, command, 0);
    result.command_id = command.id;
    return result;
}

internal engine_command_result
engine_api_command_research_ability(void *opaque, u8 city_id, u8 row)
{
    sim_pk_order order = {};
    order.city_id = city_id;
    order.officer = SIM_NO_OFFICER;
    order.row     = row;
    return engine_run_pk((engine *)opaque, SIM_COMMAND_RESEARCH_ABILITY, order);
}

internal engine_command_result
engine_api_command_train_officer(void *opaque, u16 officer, u8 row)
{
    sim_pk_order order = {};
    order.city_id = SIM_NO_CITY;
    order.officer = officer;
    order.row     = row;
    return engine_run_pk((engine *)opaque, SIM_COMMAND_TRAIN_OFFICER, order);
}

/* ---- the historical events ------------------------------------------------------------- */

internal u32
engine_api_history_count(void *opaque)
{
    return ((engine *)opaque)->sim->content.history_count;
}

internal engine_history_kind
engine_api_history_at(void *opaque, u32 index)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_history_kind result = {};
    result.id = ENGINE_NO_ROW;
    if (index >= sim->content.history_count) return result;
    history_kind *kind = sim->content.history + index;
    result.id         = kind->id;
    memcpy(result.key, kind->key, sizeof(result.key));
    result.chance     = kind->chance;
    result.fired_turn = sim->history_turn[index];
    result.name_count = kind->name_count;
    for (u32 name = 0; name < kind->name_count; ++name) result.names_found += sim_officer_by_name(sim, kind->names[name], kind->name_sizes[name]) != 0;
    return result;
}

internal b32
engine_api_history_ready(void *opaque, u32 row, u8 force_id)
{
    sim_state *sim = ((engine *)opaque)->sim;
    return row < sim->content.history_count && !sim->history_turn[row] && sim_history_ready(sim, row, force_id);
}

internal engine_command_result
engine_api_trigger_history(void *opaque, u32 row, u8 force_id)
{
    engine *e = (engine *)opaque;
    engine_command_result result = {};
    sim_m8_order order = {};
    order.force = force_id;
    order.value = (u8)Minimum(row, 255u);
    order.city_id = SIM_NO_CITY;
    order.officer = SIM_NO_OFFICER;
    result.refusal = row > 255 ? SIM_REFUSAL_NO_EVENT : sim_check_trigger_history(e->sim, &order).refusal;
    if (result.refusal != SIM_REFUSAL_NONE) return result;
    sim_command command = sim_make_command(e->sim, SIM_COMMAND_TRIGGER_HISTORY);
    command.m8 = order;
    engine_run(e, command, 0);
    result.command_id = command.id;
    return result;
}

internal void
engine_bind_playable_api(engine_api *api)
{
    api->history_count            = engine_api_history_count;
    api->history_at               = engine_api_history_at;
    api->history_ready            = engine_api_history_ready;
    api->trigger_history          = engine_api_trigger_history;
    api->chart_count              = engine_api_chart_count;
    api->chart_row_at             = engine_api_chart_row_at;
    api->faction_chart            = engine_api_faction_chart;
    api->ancient_count            = engine_api_ancient_count;
    api->ancient_at               = engine_api_ancient_at;
    api->new_officer_count        = engine_api_new_officer_count;
    api->create_officer           = engine_api_create_officer;
    api->command_research_ability = engine_api_command_research_ability;
    api->command_train_officer    = engine_api_command_train_officer;
}
