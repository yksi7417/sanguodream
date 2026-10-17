/* The engine_api of M4 (growth): the technique table and the research of a
   faction, the merge command, the disasters, the game settings and the end
   of the game. Included by engine.cpp, before engine_make_api. */

internal u32
engine_api_technique_kind_count(void *opaque)
{
    return ((engine *)opaque)->sim->content.technique_count;
}

internal engine_technique_kind
engine_api_technique_kind_at(void *opaque, u32 index)
{
    sim_content *content = &((engine *)opaque)->sim->content;
    engine_technique_kind result = {};
    result.id = ENGINE_NO_TECHNIQUE;
    if (index >= content->technique_count) return result;
    technique_kind *kind = content->techniques + index;
    result.id             = kind->id;
    memcpy(result.key, kind->key, sizeof(result.key));
    result.line           = (u8)kind->line;
    result.level          = kind->level;
    result.points         = kind->points;
    result.gold           = kind->gold;
    result.ability        = (u8)kind->ability;
    result.min_days       = kind->min_days;
    result.max_days       = kind->max_days;
    result.period_assumed = kind->period_assumed;
    result.effect         = (u8)kind->effect;
    result.unit_class     = (u8)kind->unit_class;
    result.value          = kind->value;
    result.previous       = sim_previous_technique(content, (u8)index);
    return result;
}

internal engine_faction_techniques
engine_api_faction_techniques(void *opaque, u8 force_id)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_faction_techniques result = {};
    result.force_id    = ENGINE_NO_FACTION;
    result.researching = ENGINE_NO_TECHNIQUE;
    if (force_id == SIM_NO_FORCE) return result;
    sim_research_job *research = sim->research + force_id;
    result.force_id         = force_id;
    result.owned            = sim->techniques[force_id];
    result.technique_points = sim->technique_points[force_id];
    result.researching      = research->technique;
    result.research_city    = research->city_id;
    result.turns_left       = research->turns_left;
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
    {
        result.officers[place] = research->technique != CONTENT_NO_TECHNIQUE ? research->officers[place] : ENGINE_NO_OFFICER;
    }
    return result;
}

internal u32
engine_api_disaster_kind_count(void *opaque)
{
    return ((engine *)opaque)->sim->content.disaster_count;
}

internal engine_disaster_kind
engine_api_disaster_kind_at(void *opaque, u32 index)
{
    sim_content *content = &((engine *)opaque)->sim->content;
    engine_disaster_kind result = {};
    result.id = ENGINE_NO_DISASTER;
    if (index >= content->disaster_count) return result;
    disaster_kind *kind = content->disasters + index;
    result.id             = kind->id;
    memcpy(result.key, kind->key, sizeof(result.key));
    result.month_mask     = kind->month_mask;
    result.chance_percent = kind->chance_percent;
    result.turns          = kind->turns;
    return result;
}

internal engine_city_disasters
engine_api_city_disasters(void *opaque, u8 city_id)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_city_disasters result = {};
    result.city_id = ENGINE_NO_CITY;
    city_component *city = sim_find_city(sim, city_id);
    if (!city) return result;
    result.city_id = city_id;
    for (u32 disaster = 0; disaster < CONTENT_DISASTER_CAPACITY; ++disaster)
    {
        result.turns[disaster] = city->disaster_turns[disaster];
        if (city->disaster_turns[disaster]) result.running++;
    }
    return result;
}

internal engine_outcome
engine_api_outcome(void *opaque)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_outcome result = {};
    result.kind     = (u8)sim->outcome.kind;
    result.force_id = sim->outcome.kind != SIM_OUTCOME_NONE ? sim->outcome.force_id : ENGINE_NO_FACTION;
    result.end_year = sim->content.rules.game_end_year;
    return result;
}

internal engine_settings
engine_api_settings(void *opaque)
{
    engine_settings result = {};
    result.disasters      = ((engine *)opaque)->sim->settings.disasters;
    result.ability_change = ((engine *)opaque)->sim->settings.ability_change;
    result.lifespan       = ((engine *)opaque)->sim->settings.lifespan;
    return result;
}

/* Checks first: a refused command is not a command, so it gets no id and
   ledgers nothing. */
internal engine_command_result
engine_api_command_research(void *opaque, u8 city_id, u8 technique, const u16 officers[SIM_DEVELOP_OFFICERS])
{
    engine *e = (engine *)opaque;
    engine_command_result result = {};
    sim_research_order order = engine_research_order(city_id, technique, officers);
    result.refusal = sim_check_research(e->sim, &order).refusal;
    if (result.refusal != SIM_REFUSAL_NONE) return result;
    sim_command command = sim_make_command(e->sim, SIM_COMMAND_RESEARCH);
    command.research = order;
    engine_run(e, command, 0);
    result.command_id = command.id;
    return result;
}

internal engine_command_result
engine_api_command_merge(void *opaque, u8 city_id, u8 absorber, u8 absorbed, u16 officer)
{
    engine *e = (engine *)opaque;
    engine_command_result result = {};
    sim_merge_order order = engine_merge_order(city_id, absorber, absorbed, officer);
    result.refusal = sim_check_merge(e->sim, &order).refusal;
    if (result.refusal != SIM_REFUSAL_NONE) return result;
    sim_command command = sim_make_command(e->sim, SIM_COMMAND_MERGE);
    command.merge = order;
    engine_run(e, command, 0);
    result.command_id = command.id;
    return result;
}

internal engine_command_result
engine_api_start_disaster(void *opaque, u8 city_id, u8 disaster)
{
    engine *e = (engine *)opaque;
    engine_command_result result = {};
    sim_disaster_order order = {};
    order.city_id  = city_id;
    order.disaster = disaster;
    result.refusal = sim_check_start_disaster(e->sim, &order);
    if (result.refusal != SIM_REFUSAL_NONE) return result;
    sim_command command = sim_make_command(e->sim, SIM_COMMAND_START_DISASTER);
    command.disaster = order;
    engine_run(e, command, 0);
    result.command_id = command.id;
    return result;
}

internal void
engine_api_set_settings(void *opaque, engine_settings settings)
{
    ((engine *)opaque)->sim->settings.disasters      = settings.disasters;
    ((engine *)opaque)->sim->settings.ability_change = settings.ability_change;
    ((engine *)opaque)->sim->settings.lifespan       = settings.lifespan;
}

internal void
engine_api_grant_technique(void *opaque, u8 force_id, u8 technique)
{
    sim_grant_technique(((engine *)opaque)->sim, force_id, technique);
}

internal void
engine_api_set_technique_points(void *opaque, u8 force_id, u32 points)
{
    if (force_id != SIM_NO_FORCE) ((engine *)opaque)->sim->technique_points[force_id] = points;
}

/* The editor sets the date. The save date is not decoded (Q-004), so a
   scenario editor and the tests of the year 351 need it. */
internal void
engine_api_set_date(void *opaque, u16 year, u8 month, u8 day)
{
    if (month < 1 || month > 12 || (day != 1 && day != 11 && day != 21)) return;
    ((engine *)opaque)->sim->turns.date = calendar_make(year, month, day);
}

internal void
engine_bind_growth_api(engine_api *api)
{
    api->set_date             = engine_api_set_date;
    api->technique_kind_count = engine_api_technique_kind_count;
    api->technique_kind_at    = engine_api_technique_kind_at;
    api->faction_techniques   = engine_api_faction_techniques;
    api->disaster_kind_count  = engine_api_disaster_kind_count;
    api->disaster_kind_at     = engine_api_disaster_kind_at;
    api->city_disasters       = engine_api_city_disasters;
    api->outcome              = engine_api_outcome;
    api->settings             = engine_api_settings;
    api->command_research     = engine_api_command_research;
    api->command_merge        = engine_api_command_merge;
    api->start_disaster       = engine_api_start_disaster;
    api->set_settings         = engine_api_set_settings;
    api->grant_technique      = engine_api_grant_technique;
    api->set_technique_points = engine_api_set_technique_points;
}
