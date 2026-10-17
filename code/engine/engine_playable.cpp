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

/* ---- the scenarios --------------------------------------------------------------------- */

/* Reads scenarios.tsv of the pack. A row whose file the engine cannot read
   stays in the list as not available (the six PK scenarios: the clean room
   has only the files of spec/fixtures). A pack without the table has no
   scenarios. */
internal void
engine_load_scenarios(engine *e, const char *folder)
{
    static const char *kinds[ENGINE_SCENARIO_KIND_COUNT] = { "save", "base", "pk" };
    e->scenario_count = 0;
    tsv_table table = {};
    arena_reset(&e->scratch);
    if (!folder || !engine_read_table(e, folder, "scenarios.tsv", &table)) return;
    for (u32 row = 0; row < table.row_count && e->scenario_count < ENGINE_SCENARIO_CAPACITY; ++row)
    {
        engine_scenario *scenario = e->scenarios + e->scenario_count;
        *scenario = {};
        u32 id = 0, year = 0;
        const char *key = tsv_cell(&table, row, "key"), *file = tsv_cell(&table, row, "file");
        u32 kind = content_find_name(kinds, ENGINE_SCENARIO_KIND_COUNT, tsv_cell(&table, row, "kind"));
        if (!tsv_cell_u32(&table, row, "id", &id) || id != row || !key[0] || strlen(key) >= CONTENT_KEY_SIZE || kind == ENGINE_SCENARIO_KIND_COUNT ||
            !tsv_cell_u32(&table, row, "year", &year) || strlen(file) >= sizeof(scenario->file))
        {
            e->scenario_count = 0;   /* a bad table gives no list */
            break;
        }
        scenario->id   = (u8)id;
        scenario->kind = (u8)kind;
        scenario->year = (u16)year;
        memcpy(scenario->key, key, strlen(key) + 1);
        if (strcmp(file, "-") != 0) memcpy(scenario->file, file, strlen(file) + 1);
        e->scenario_count++;
    }
    arena_reset(&e->scratch);
    for (u32 index = 0; index < e->scenario_count && e->read_entire_file; ++index)
    {
        engine_scenario *scenario = e->scenarios + index;
        if (!scenario->file[0]) continue;
        u64 mark = e->transient->used;
        scenario->available = e->read_entire_file(scenario->file, e->transient).contents != 0;
        e->transient->used = mark;
    }
}

/* battles.tsv: the list of 決戰稱霸. A pack without the table has none. */
internal void
engine_load_battles(engine *e, const char *folder)
{
    e->battle_count = 0;
    tsv_table table = {};
    arena_reset(&e->scratch);
    if (!folder || !engine_read_table(e, folder, "battles.tsv", &table)) return;
    for (u32 row = 0; row < table.row_count && e->battle_count < ENGINE_BATTLE_CAPACITY; ++row)
    {
        engine_battle *battle = e->battles + e->battle_count;
        *battle = {};
        u32 id = 0;
        const char *key = tsv_cell(&table, row, "key");
        if (!tsv_cell_u32(&table, row, "id", &id) || id != row || !key[0] || strlen(key) >= CONTENT_KEY_SIZE) { e->battle_count = 0; break; }
        battle->id            = (u8)id;
        battle->open_at_start = strcmp(tsv_cell(&table, row, "open"), "yes") == 0;
        battle->playable      = false;
        memcpy(battle->key, key, strlen(key) + 1);
        e->battle_count++;
    }
    arena_reset(&e->scratch);
}

internal u32
engine_api_battle_count(void *opaque)
{
    return ((engine *)opaque)->battle_count;
}

internal engine_battle
engine_api_battle_at(void *opaque, u32 index)
{
    engine *e = (engine *)opaque;
    engine_battle result = {};
    result.id = ENGINE_NO_ROW;
    return index < e->battle_count ? e->battles[index] : result;
}

internal u32
engine_api_scenario_count(void *opaque)
{
    return ((engine *)opaque)->scenario_count;
}

internal engine_scenario
engine_api_scenario_at(void *opaque, u32 index)
{
    engine *e = (engine *)opaque;
    engine_scenario result = {};
    result.id = ENGINE_NO_ROW;
    return index < e->scenario_count ? e->scenarios[index] : result;
}

/* A new game from a scenario file: the records of the scenario become a
   save in the scratch arena, and the save loader takes it. The scenario has
   no public order and no player: each city starts with
   scenario.public_order (assumed, Q-189), and no faction is human. */
internal engine_load_result
engine_api_load_scenario(void *opaque, const char *path)
{
    engine *e = (engine *)opaque;
    engine_load_result result;
    read_file_result file;
    if (!engine_read_file_to_scratch(e, path, &file, &result)) return result;
    scenario_file scenario;
    scenario_load_status loaded = scenario_load(&e->scratch, (const u8 *)file.contents, file.size, &scenario);
    if (loaded != SCENARIO_LOAD_OK)
    {
        result.status = ENGINE_LOAD_BAD_SAVE;
        result.detail = scenario_load_status_name(loaded);
        return result;
    }
    save_file save = {};
    save.header.scenario_number              = scenario.header.scenario_number;
    save.header.scenario_start_year          = scenario.header.start_year;
    save.header.scenario_start_month         = scenario.header.start_month;
    save.header.scenario_start_ten_day_period = scenario.header.start_ten_day_period;
    save.officers           = scenario.officers;
    save.officer_slot_count = scenario.officer_slot_count;
    save.cities             = scenario.cities;
    save.city_count         = scenario.city_count;
    save.items              = scenario.items;
    save.item_count         = scenario.item_count;
    u32 order = Minimum(e->sim->content.rules.scenario_public_order, 100u);
    for (u32 index = 0; index < save.city_count; ++index) save.cities[index].public_order = (u8)order;
    sim_event_sink sink = sim_sink_from_arena(&e->scratch, ENGINE_SINK_CAPACITY);
    sim_command command = sim_make_command(e->sim, SIM_COMMAND_LOAD_SAVE);
    if (!sim_load_save(e->sim, &save, command, &sink))
    {
        result.status = ENGINE_LOAD_BAD_SAVE;
        result.detail = "the scenario has more officers or cities than the game can hold";
        return result;
    }
    for (u32 force = 0; force < SIM_FORCE_CAPACITY; ++force) e->sim->turns.human[force] = false;
    engine_ledger_append(e, &sink);
    result.status     = ENGINE_LOAD_OK;
    result.command_id = command.id;
    return result;
}

internal void
engine_bind_playable_api(engine_api *api)
{
    api->scenario_count           = engine_api_scenario_count;
    api->scenario_at              = engine_api_scenario_at;
    api->load_scenario            = engine_api_load_scenario;
    api->battle_count             = engine_api_battle_count;
    api->battle_at                = engine_api_battle_at;
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
