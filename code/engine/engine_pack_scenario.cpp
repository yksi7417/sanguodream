/* A scenario of the content pack in TSV files (docs/MAPS.md, step W2). One
   folder holds scenario.tsv, forces.tsv, officers.tsv and cities.tsv. The
   Big5 names of the cities, the passes and the ports come from the name_big5
   column of cities.tsv and bases.tsv of the pack. The loader makes a save in
   the scratch arena, as engine_api_load_scenario does with a scenario file.
   The sim takes it with the commands of a new game and of a name index.
   Included by engine.cpp, before engine_playable.cpp. */

#define ENGINE_PACK_SCENARIO_FILE   "scenario.tsv"
#define ENGINE_PACK_CAPACITY        100000u   /* the capacity constant of a city record (SCEN002) */
#define ENGINE_PACK_BIG_CAPACITY    150000u   /* the same for a big city */

/* The parts of one load: the tables in the scratch arena and the save that
   they make. detail says why the load failed; 0 while it is good. */
struct engine_pack_load
{
    tsv_table   settings, forces, officers, cities, city_names, base_names;
    save_file   save;
    name_index *names;
    const char *detail;
};

/* "A564A8D8" into the bytes A5 64 A8 D8 and a NUL. False if the text is not
   pairs of hex digits or if the bytes and the NUL do not fit. */
internal b32
engine_pack_big5(const char *text, u8 *out, u32 size)
{
    u32 length = (u32)strlen(text);
    if (length == 0 || length % 2 || length / 2 + 1 > size) return false;
    memset(out, 0, size);
    for (u32 index = 0; index < length / 2; ++index)
    {
        u32 high = content_hex_value(text[2 * index]), low = content_hex_value(text[2 * index + 1]);
        if (high > 15 || low > 15) return false;
        out[index] = (u8)(high * 16 + low);
    }
    return true;
}

/* A cell that is a number or "-" (none). */
internal b32
engine_pack_number_or_none(tsv_table *table, u32 row, const char *column, u32 none, u32 *value)
{
    if (strcmp(tsv_cell(table, row, column), "-") == 0) { *value = none; return true; }
    return tsv_cell_u32(table, row, column, value);
}

/* The value of a key in a key/value table such as scenario.tsv. */
internal const char *
engine_pack_setting(tsv_table *table, const char *key)
{
    for (u32 row = 0; row < table->row_count; ++row)
    {
        if (strcmp(tsv_cell(table, row, "key"), key) == 0) return tsv_cell(table, row, "value");
    }
    return "";
}

/* "BACSCC": the six unit aptitudes, C = 0, B = 1, A = 2, S = 3. */
internal b32
engine_pack_aptitudes(const char *text, u8 *out)
{
    static const char grades[] = "CBAS";
    if (strlen(text) != OFFICER_UNIT_APTITUDE_COUNT) return false;
    for (u32 index = 0; index < OFFICER_UNIT_APTITUDE_COUNT; ++index)
    {
        const char *grade = strchr(grades, text[index]);
        if (!grade || !text[index]) return false;
        out[index] = (u8)(grade - grades);
    }
    return true;
}

/* The skill byte of a record (the file id) for a key of skills.tsv; "-" is no skill. */
internal b32
engine_pack_skill(sim_content *content, const char *key, u8 *file_id)
{
    *file_id = OFFICER_SKILL_NONE;
    if (strcmp(key, "-") == 0) return true;
    for (u32 index = 0; index < content->skill_count; ++index)
    {
        if (strcmp(content->skills[index].key, key) == 0) { *file_id = content->skills[index].file_id; return true; }
    }
    return false;
}

internal b32
engine_pack_status(const char *text, u8 *status)
{
    static const char *names[] = { "ruler", "governor", "officer", "free" };
    static const u8 values[]   = { OFFICER_STATUS_RULER, OFFICER_STATUS_GOVERNOR, OFFICER_STATUS_REGULAR_OFFICER, OFFICER_STATUS_FREE_OR_NOT_APPEARED };
    for (u32 index = 0; index < ArrayCount(names); ++index)
    {
        if (strcmp(text, names[index]) == 0) { *status = values[index]; return true; }
    }
    return false;
}

/* One row of officers.tsv into its slot of the save. */
internal b32
engine_pack_officer(sim_content *content, tsv_table *table, u32 row, u32 force_count, officer_record *slots)
{
    u32 id = 0, force = 0, city = 0, years[3] = {}, stats[OFFICER_STAT_COUNT] = {}, loyalty = 0, merit = 0, compatibility = 0;
    static const char *year_columns[3] = { "appear", "birth", "death" };
    static const char *stat_columns[OFFICER_STAT_COUNT] = { "leadership", "war", "intelligence", "politics", "charm" };
    if (!tsv_cell_u32(table, row, "id", &id) || id >= SAVE_OFFICER_SLOTS || !officer_slot_is_empty(slots + id)) return false;
    officer_record *officer = slots + id;
    if (!engine_pack_number_or_none(table, row, "force", OFFICER_FORCE_NONE, &force) || (force != OFFICER_FORCE_NONE && force >= force_count)) return false;
    if (!tsv_cell_u32(table, row, "city", &city) || city >= SAVE_CITY_COUNT) return false;
    for (u32 index = 0; index < 3; ++index) if (!tsv_cell_u32(table, row, year_columns[index], years + index) || years[index] > 0xFFFFu) return false;
    for (u32 index = 0; index < OFFICER_STAT_COUNT; ++index) if (!tsv_cell_u32(table, row, stat_columns[index], stats + index) || stats[index] > 120) return false;
    if (!tsv_cell_u32(table, row, "loyalty", &loyalty) || loyalty > 255 || !tsv_cell_u32(table, row, "merit", &merit) || merit > 0xFFFFu ||
        !tsv_cell_u32(table, row, "compatibility", &compatibility) || compatibility > 149)
    {
        return false;
    }
    if (!engine_pack_big5(tsv_cell(table, row, "family_big5"), officer->family_name, OFFICER_FAMILY_NAME_SIZE) ||
        !engine_pack_big5(tsv_cell(table, row, "given_big5"), officer->given_name, OFFICER_GIVEN_NAME_SIZE) ||
        !engine_pack_status(tsv_cell(table, row, "status"), &officer->status) ||
        !engine_pack_aptitudes(tsv_cell(table, row, "aptitudes"), officer->unit_aptitudes) ||
        !engine_pack_skill(content, tsv_cell(table, row, "skill"), &officer->skill_number))
    {
        return false;
    }
    if ((officer->status == OFFICER_STATUS_FREE_OR_NOT_APPEARED) != (force == OFFICER_FORCE_NONE)) return false;
    officer->id = (u16)id;
    officer->female = tsv_cell_yes(table, row, "female");
    officer->devout = tsv_cell_yes(table, row, "devout");   /* Q-E10; a pack without the column has no devout officer */
    officer->appearance_year = (u16)years[0];
    officer->birth_year      = (u16)years[1];
    officer->death_year      = (u16)years[2];
    officer->family_line_head = officer->father = officer->related_officer = officer->spouse = officer->sworn_brother_group_head = OFFICER_ID_NONE;
    for (u32 slot = 0; slot < OFFICER_RELATION_SLOTS; ++slot) officer->liked_officers[slot] = officer->disliked_officers[slot] = OFFICER_ID_NONE;
    officer->compatibility    = (u8)compatibility;
    officer->force_id         = (u8)force;
    officer->base_location    = officer->current_position = (u16)city;
    officer->merit            = (u16)merit;
    officer->leadership       = (u8)stats[0];
    officer->war              = (u8)stats[1];
    officer->intelligence     = (u8)stats[2];
    officer->politics         = (u8)stats[3];
    officer->charm            = (u8)stats[4];
    officer->has_loyalty      = true;
    officer->loyalty          = (u8)loyalty;
    return true;
}

/* forces.tsv: each row is a force; its ruler must be an officer of the force with the status ruler. */
internal b32
engine_pack_forces(engine_pack_load *load)
{
    officer_record *slots = load->save.officers;
    for (u32 row = 0; row < load->forces.row_count; ++row)
    {
        u32 id = 0, ruler = 0;
        if (!tsv_cell_u32(&load->forces, row, "id", &id) || id != row || id >= SIM_FORCE_CAPACITY || !tsv_cell_u32(&load->forces, row, "ruler", &ruler) ||
            ruler >= SAVE_OFFICER_SLOTS || officer_slot_is_empty(slots + ruler) || slots[ruler].force_id != id || slots[ruler].status != OFFICER_STATUS_RULER)
        {
            load->detail = "forces.tsv: a bad row, or the ruler is not a ruler of the force";
            return false;
        }
    }
    for (u32 slot = 0; slot < SAVE_OFFICER_SLOTS; ++slot)
    {
        if (officer_slot_is_empty(slots + slot) || slots[slot].status != OFFICER_STATUS_RULER) continue;
        u32 ruler = 0;
        if (!tsv_cell_u32(&load->forces, slots[slot].force_id, "ruler", &ruler) || ruler != slot)
        {
            load->detail = "officers.tsv: a ruler that forces.tsv does not name";
            return false;
        }
    }
    return true;
}

/* One row of the cities.tsv of the scenario: the owner, the governor and the stocks. */
internal b32
engine_pack_city(engine_pack_load *load, sim_content *content, u32 row)
{
    tsv_table *table = &load->cities;
    u32 id = 0, owner = 0, governor = 0, troops = 0, gold = 0, food = 0, durability = 0;
    static const char *stock_columns[] = { "spears", "halberds", "crossbows", "horses" };
    if (!tsv_cell_u32(table, row, "id", &id) || id != row || id >= SAVE_CITY_COUNT ||
        !engine_pack_number_or_none(table, row, "owner", CITY_OWNER_NONE, &owner) || (owner != CITY_OWNER_NONE && owner >= load->forces.row_count) ||
        !engine_pack_number_or_none(table, row, "governor", CITY_COMMANDER_NONE, &governor) ||
        !tsv_cell_u32(table, row, "troops", &troops) || !tsv_cell_u32(table, row, "gold", &gold) || !tsv_cell_u32(table, row, "food", &food) ||
        !tsv_cell_u32(table, row, "durability", &durability) || durability == 0 || durability > 0xFFFFu)
    {
        return false;
    }
    officer_record *officers = load->save.officers;
    if (governor != CITY_COMMANDER_NONE &&
        (governor >= SAVE_OFFICER_SLOTS || officer_slot_is_empty(officers + governor) || officers[governor].force_id != owner || officers[governor].base_location != id))
    {
        return false;
    }
    city_record *city = load->save.cities + id;
    *city = {};
    city->id                    = (u8)id;
    city->owner_force_id        = (u8)owner;
    city->commanding_officer_id = (u16)governor;
    city->large_city_flag       = content->cities[id].specialty == CITY_SPECIALTY_BIG_CITY;
    city->capacity_constant     = city->large_city_flag ? ENGINE_PACK_BIG_CAPACITY : ENGINE_PACK_CAPACITY;
    city->troops                = owner == CITY_OWNER_NONE ? 0 : troops;
    city->gold                  = gold;
    city->food                  = food;
    city->durability_maximum    = (u16)durability;
    for (u32 index = 0; index < ArrayCount(stock_columns); ++index)
    {
        if (!tsv_cell_u32(table, row, stock_columns[index], city->equipment_stocks + index)) return false;
    }
    if (governor != CITY_COMMANDER_NONE && officers[governor].status == OFFICER_STATUS_REGULAR_OFFICER) officers[governor].status = OFFICER_STATUS_GOVERNOR;
    return true;
}

/* The name index of the pack: name_big5 of cities.tsv (ids 0 to 41) and of bases.tsv (ids 42 to 86). */
internal b32
engine_pack_names(engine_pack_load *load)
{
    name_index *names = load->names;
    *names = {};
    names->file_kind = SCENARIO_FILE_KIND_NAME_INDEX;
    for (u32 row = 0; row < load->city_names.row_count && row < SCENARIO_CITY_COUNT; ++row)
    {
        u32 id = 0;
        if (!tsv_cell_u32(&load->city_names, row, "id", &id) || id >= SCENARIO_CITY_COUNT) return false;
        names->cities[id].id = (u8)id;
        if (!engine_pack_big5(tsv_cell(&load->city_names, row, "name_big5"), names->cities[id].name, NAME_INDEX_CITY_NAME_SIZE)) return false;
    }
    for (u32 row = 0; row < load->base_names.row_count; ++row)
    {
        u32 id = 0;
        if (!tsv_cell_u32(&load->base_names, row, "id", &id) || id < NAME_INDEX_FIRST_GATE_ID || id >= NAME_INDEX_FIRST_GATE_ID + NAME_INDEX_GATE_COUNT) return false;
        name_index_gate *gate = names->gates_and_ports + (id - NAME_INDEX_FIRST_GATE_ID);
        gate->id = (u8)id;
        if (!engine_pack_big5(tsv_cell(&load->base_names, row, "name_big5"), gate->name, NAME_INDEX_GATE_ENTRY_SIZE)) return false;
    }
    return load->city_names.row_count == SCENARIO_CITY_COUNT && load->base_names.row_count == NAME_INDEX_GATE_COUNT;
}

/* Reads the six tables into the scratch arena. folder is the folder of scenario.tsv. */
internal b32
engine_pack_read(engine *e, const char *folder, engine_pack_load *load)
{
    if (!engine_read_table(e, folder, ENGINE_PACK_SCENARIO_FILE, &load->settings)) { load->detail = "scenario.tsv not found"; return false; }
    if (!engine_read_table(e, folder, "forces.tsv", &load->forces) || !engine_read_table(e, folder, "officers.tsv", &load->officers) ||
        !engine_read_table(e, folder, "cities.tsv", &load->cities))
    {
        load->detail = "the scenario folder needs forces.tsv, officers.tsv and cities.tsv";
        return false;
    }
    if (!engine_read_table(e, e->content_folder, "cities.tsv", &load->city_names) || !engine_read_table(e, e->content_folder, "bases.tsv", &load->base_names))
    {
        load->detail = "the pack has no cities.tsv or bases.tsv for the names";
        return false;
    }
    return true;
}

/* The tables into the records of a save in the scratch arena. */
internal b32
engine_pack_build(engine *e, engine_pack_load *load)
{
    sim_content *content = &e->sim->content;
    save_file *save = &load->save;
    u32 year = 0, month = 0, period = 0, number = 0;
    if (!tsv_parse_u32(engine_pack_setting(&load->settings, "year"), &year) || year > 0xFFFFu ||
        !tsv_parse_u32(engine_pack_setting(&load->settings, "month"), &month) || month < 1 || month > 12 ||
        !tsv_parse_u32(engine_pack_setting(&load->settings, "ten_day"), &period) || period < 1 || period > 3 ||
        !tsv_parse_u32(engine_pack_setting(&load->settings, "number"), &number) || number > 255)
    {
        load->detail = "scenario.tsv needs year, month, ten_day and number";
        return false;
    }
    save->header.scenario_number               = (u8)number;
    save->header.scenario_start_year           = (u16)year;
    save->header.scenario_start_month          = (u8)month;
    save->header.scenario_start_ten_day_period = (u8)period;
    save->officers           = PushArray(&e->scratch, SAVE_OFFICER_SLOTS, officer_record);
    save->officer_slot_count = SAVE_OFFICER_SLOTS;
    save->cities             = PushArray(&e->scratch, SAVE_CITY_COUNT, city_record);
    save->city_count         = SAVE_CITY_COUNT;
    memset(save->officers, 0, sizeof(officer_record) * SAVE_OFFICER_SLOTS);
    for (u32 row = 0; row < load->officers.row_count; ++row)
    {
        if (!engine_pack_officer(content, &load->officers, row, load->forces.row_count, save->officers)) { load->detail = "officers.tsv has a bad row"; return false; }
    }
    if (!engine_pack_forces(load)) return false;
    if (load->cities.row_count != SAVE_CITY_COUNT || content->city_count != SAVE_CITY_COUNT) { load->detail = "cities.tsv needs 42 rows"; return false; }
    for (u32 row = 0; row < load->cities.row_count; ++row)
    {
        if (!engine_pack_city(load, content, row)) { load->detail = "cities.tsv of the scenario has a bad row"; return false; }
    }
    u32 order = Minimum(content->rules.scenario_public_order, 100u);
    for (u32 index = 0; index < save->city_count; ++index) save->cities[index].public_order = (u8)order;
    load->names = PushStruct(&e->scratch, name_index);
    if (!engine_pack_names(load)) { load->detail = "name_big5 of cities.tsv or bases.tsv of the pack is missing or too long"; return false; }
    return true;
}

/* A new game from a pack scenario: the new game, then the names. A scenario
   that does not load leaves the game and the ledger as they were. */
internal engine_load_result
engine_load_pack_scenario(engine *e, const char *path)
{
    engine_load_result result = {};
    char folder[ENGINE_PATH_SIZE];
    snprintf(folder, sizeof(folder), "%s", path);
    char *slash = strrchr(folder, '/');
    char *backslash = strrchr(folder, '\\');
    if (backslash > slash) slash = backslash;
    if (slash) *slash = 0;
    arena_reset(&e->scratch);
    engine_pack_load *load = PushStruct(&e->scratch, engine_pack_load);
    *load = {};
    if (!e->content.loaded || !engine_pack_read(e, folder, load) || !engine_pack_build(e, load))
    {
        result.status = load->detail && strcmp(load->detail, "scenario.tsv not found") == 0 ? ENGINE_LOAD_FILE_NOT_FOUND : ENGINE_LOAD_BAD_SAVE;
        result.detail = load->detail ? load->detail : "the pack has no content";
        return result;
    }
    sim_event_sink sink = sim_sink_from_arena(&e->scratch, ENGINE_SINK_CAPACITY);
    sim_command command = sim_make_command(e->sim, SIM_COMMAND_LOAD_SAVE);
    if (!sim_load_new_game(e->sim, &load->save, command, &sink))
    {
        result.status = ENGINE_LOAD_BAD_SAVE;
        result.detail = "the scenario has more officers or cities than the game can hold";
        return result;
    }
    for (u32 force = 0; force < SIM_FORCE_CAPACITY; ++force) e->sim->turns.human[force] = false;
    engine_game_replay_reset(e);   /* a new game: a new record */
    engine_ledger_append(e, &sink);
    sim_event_sink name_sink = sim_sink_from_arena(&e->scratch, ENGINE_SINK_CAPACITY);
    sim_load_name_index(e->sim, load->names, sim_make_command(e->sim, SIM_COMMAND_LOAD_NAME_INDEX), &name_sink);
    engine_ledger_append(e, &name_sink);
    e->names_source = ENGINE_SOURCE_PACK;
    engine_load_scenario_keys(e, folder);   /* the scratch arena has done its work */
    result.status     = ENGINE_LOAD_OK;
    result.command_id = command.id;
    return result;
}
