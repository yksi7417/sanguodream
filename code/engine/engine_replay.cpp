/* M13 the replay of the save pairs of the original (tutorial 26).

   A save pair is a save of the original before one command and a save
   after it (spec/fixtures/saves/pairs, experiments/ab-testing-plan.md,
   level A). The replay:
   1. loads the second save in the middle of the player's turn and takes a
      snapshot of the compared fields (the original);
   2. loads the first save the same way and takes a snapshot (the before);
   3. runs the command through the engine and takes a snapshot (the replay);
   4. compares the replay with the original, field by field.
   Both saves go through the same loader, so a field differs only when the
   engine does the command in another way than the original. The replay
   replaces the game. Included by engine.cpp. */

/* The fields of each kind of record, in snapshot order. */
#define REPLAY_CITY_FIELDS     (ENGINE_REPLAY_CITY_EQUIPMENT + CITY_EQUIPMENT_STOCK_COUNT)
#define REPLAY_OFFICER_FIELDS  (ENGINE_REPLAY_OFFICER_ACTED - ENGINE_REPLAY_OFFICER_FORCE + 1)
#define REPLAY_FORCE_FIELDS    2
#define REPLAY_OFFICER_START   (SAVE_CITY_COUNT * REPLAY_CITY_FIELDS)
#define REPLAY_FORCE_START     (REPLAY_OFFICER_START + SAVE_OFFICER_SLOTS * REPLAY_OFFICER_FIELDS)
#define REPLAY_VALUE_COUNT     (REPLAY_FORCE_START + SAVE_FORCE_COUNT * REPLAY_FORCE_FIELDS)
#define REPLAY_ABSENT          (-1)   /* the record does not exist in the state */
#define REPLAY_NO_SLOT         0xFF   /* the city has no free slot; the develop command refuses it */

struct replay_field_info
{
    const char *name;       /* the name in the save dump of the spec */
    const char *rule;
    const char *q_id;
    b32         verified;   /* save-file.md marks the field confirmed */
};

/* save-file.md gives the status of each field. Gold, troops and grain are
   confirmed by the play session and by the grain fixture. The order of the
   equipment stocks (Q-006) and the per-force tables (Q-202) are not. */
global_variable const replay_field_info replay_fields[ENGINE_REPLAY_FIELD_COUNT] =
{
    { "owner_force_id",        "ownership",           "",      true  },
    { "commanding_officer_id", "governor",            "Q-147", true  },
    { "gold",                  "income and costs",    "Q-151", true  },
    { "troops",                "conscription",        "Q-153", true  },
    { "food",                  "grain",               "",      true  },
    { "public_order",          "public order",        "Q-152", true  },
    { "equipment",             "weapons",             "Q-006", false },
    { "force_id",              "officers",            "",      true  },
    { "base_location",         "officers",            "",      true  },
    { "current_position",      "officers",            "",      true  },
    { "status",                "officer status",      "Q-150", true  },
    { "merit",                 "merit",               "",      true  },
    { "acted",                 "acted flag",          "",      true  },
    { "action_points",         "action points",       "Q-202", false },
    { "technique_points",      "technique points",    "Q-158", false },
};

/* The names of the stocks in the save dump of the spec. */
global_variable const char *replay_stock_names[CITY_EQUIPMENT_STOCK_COUNT] =
{
    "spears", "halberds", "crossbows", "horses", "rams", "towers", "catapults", "ship1", "ship2", "ship3"
};

internal void
engine_replay_snapshot(engine *e, s64 *values)
{
    sim_state *sim = e->sim;
    for (u32 index = 0; index < REPLAY_VALUE_COUNT; ++index) values[index] = REPLAY_ABSENT;
    for (u32 city_id = 0; city_id < SAVE_CITY_COUNT; ++city_id)
    {
        city_component *city = sim_find_city(sim, (u8)city_id);
        if (!city) continue;
        s64 *at = values + city_id * REPLAY_CITY_FIELDS;
        at[ENGINE_REPLAY_CITY_OWNER]        = city->owner_force_id;
        at[ENGINE_REPLAY_CITY_COMMANDER]    = city->commanding_officer_id;
        at[ENGINE_REPLAY_CITY_GOLD]         = city->gold;
        at[ENGINE_REPLAY_CITY_TROOPS]       = city->troops;
        at[ENGINE_REPLAY_CITY_FOOD]         = city->food;
        at[ENGINE_REPLAY_CITY_PUBLIC_ORDER] = city->public_order;
        for (u32 stock = 0; stock < CITY_EQUIPMENT_STOCK_COUNT; ++stock) at[ENGINE_REPLAY_CITY_EQUIPMENT + stock] = city->equipment_stocks[stock];
    }
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (officer->officer_id >= SAVE_OFFICER_SLOTS) continue;
        s64 *at = values + REPLAY_OFFICER_START + officer->officer_id * REPLAY_OFFICER_FIELDS - ENGINE_REPLAY_OFFICER_FORCE;
        at[ENGINE_REPLAY_OFFICER_FORCE]    = officer->force_id;
        at[ENGINE_REPLAY_OFFICER_LOCATION] = officer->base_location;
        at[ENGINE_REPLAY_OFFICER_POSITION] = officer->current_position;
        at[ENGINE_REPLAY_OFFICER_STATUS]   = officer->status;
        at[ENGINE_REPLAY_OFFICER_MERIT]    = officer->merit;
        at[ENGINE_REPLAY_OFFICER_ACTED]    = officer->acted_turn == sim->turn + 1;
    }
    for (u32 force = 0; force < SAVE_FORCE_COUNT; ++force)
    {
        s64 *at = values + REPLAY_FORCE_START + force * REPLAY_FORCE_FIELDS;
        at[0] = sim->turns.action_points[force];
        at[1] = sim->technique_points[force];
    }
}

/* The field kind and the path of a snapshot value. */
internal u8
engine_replay_describe(u32 value_index, char *path, u32 path_size)
{
    if (value_index < REPLAY_OFFICER_START)
    {
        u32 city_id = value_index / REPLAY_CITY_FIELDS, field = value_index % REPLAY_CITY_FIELDS;
        if (field >= ENGINE_REPLAY_CITY_EQUIPMENT)
        {
            snprintf(path, path_size, "cities[%u].equipment.%s", city_id, replay_stock_names[field - ENGINE_REPLAY_CITY_EQUIPMENT]);
            return ENGINE_REPLAY_CITY_EQUIPMENT;
        }
        snprintf(path, path_size, "cities[%u].%s", city_id, replay_fields[field].name);
        return (u8)field;
    }
    if (value_index < REPLAY_FORCE_START)
    {
        u32 offset = value_index - REPLAY_OFFICER_START;
        u32 field  = ENGINE_REPLAY_OFFICER_FORCE + offset % REPLAY_OFFICER_FIELDS;
        snprintf(path, path_size, "officers[%u].%s", offset / REPLAY_OFFICER_FIELDS, replay_fields[field].name);
        return (u8)field;
    }
    u32 offset = value_index - REPLAY_FORCE_START;
    u32 field  = ENGINE_REPLAY_FORCE_ACTION_POINTS + offset % REPLAY_FORCE_FIELDS;
    snprintf(path, path_size, "forces[%u].%s", offset / REPLAY_FORCE_FIELDS, replay_fields[field].name);
    return (u8)field;
}

/* Decodes the save in the scratch arena and loads it in the middle of the
   player's turn. */
internal b32
engine_replay_resume(engine *e, const char *path)
{
    engine_load_result result;
    read_file_result file;
    if (!engine_read_file_to_scratch(e, path, &file, &result)) return false;
    save_file save;
    if (save_load(&e->scratch, (const u8 *)file.contents, file.size, &save).status != SAVE_LOAD_OK) return false;
    sim_event_sink sink = sim_sink_from_arena(&e->scratch, ENGINE_SINK_CAPACITY);
    sim_command command = sim_make_command(e->sim, SIM_COMMAND_LOAD_SAVE);
    if (!sim_resume_save(e->sim, &save, command, &sink)) return false;
    engine_ledger_append(e, &sink);
    return true;
}

/* The first free slot of the city, or REPLAY_NO_SLOT. pair.json does not
   name the slot of a develop command (assumed: the first free one). */
internal u8
engine_replay_free_slot(engine *e, u8 city_id)
{
    if (city_id >= SAVE_CITY_COUNT) return REPLAY_NO_SLOT;
    sim_city_site *site = e->sim->sites + city_id;
    for (u32 slot = 0; slot < site->slot_count; ++slot)
    {
        if (site->slots[slot].state == SIM_SLOT_EMPTY) return (u8)slot;
    }
    return REPLAY_NO_SLOT;
}

internal engine_command_result
engine_replay_run(engine *e, const engine_replay_command *command)
{
    engine_command_result result = {};
    switch (command->kind)
    {
        case ENGINE_REPLAY_END_TURN:
            result.command_id = engine_api_end_turn(e);
            break;
        case ENGINE_REPLAY_DEVELOP:
            result = engine_api_command_develop(e, command->city_id, command->facility, command->officers,
                                                engine_replay_free_slot(e, command->city_id));
            break;
        case ENGINE_REPLAY_CONSCRIPT: result = engine_api_command_conscript(e, command->city_id, command->officers); break;
        case ENGINE_REPLAY_PATROL:    result = engine_api_command_patrol(e, command->city_id, command->officers);    break;
        case ENGINE_REPLAY_TRAIN:     result = engine_api_command_train(e, command->city_id, command->officers);     break;
        case ENGINE_REPLAY_MERCHANT:
            result = engine_api_command_merchant(e, command->city_id, command->officers[0], command->merchant_mode, command->amount);
            break;
        default: break;
    }
    return result;
}

internal engine_replay_report
engine_api_replay_pair(void *opaque, const char *before, const engine_replay_command *command, const char *after)
{
    engine *e = (engine *)opaque;
    engine_replay_report report = {};
    e->replay_diff_count = 0;
    if (!command || command->kind == ENGINE_REPLAY_NO_COMMAND || command->kind >= ENGINE_REPLAY_COMMAND_COUNT)
    {
        report.status = ENGINE_REPLAY_NO_HANDLER;
        return report;
    }
    memory_arena *arena = e->transient;
    u64 mark = arena->used;
    s64 *original = PushArray(arena, REPLAY_VALUE_COUNT, s64);
    s64 *start    = PushArray(arena, REPLAY_VALUE_COUNT, s64);
    s64 *replayed = PushArray(arena, REPLAY_VALUE_COUNT, s64);
    if (!engine_replay_resume(e, after))
    {
        report.status = ENGINE_REPLAY_BAD_AFTER;
    }
    else
    {
        engine_replay_snapshot(e, original);
        if (!engine_replay_resume(e, before)) report.status = ENGINE_REPLAY_BAD_BEFORE;
    }
    if (report.status == ENGINE_REPLAY_OK)
    {
        engine_replay_snapshot(e, start);
        /* The original always plays its AI factions. */
        b32 ai = e->sim->settings.ai;
        e->sim->settings.ai = true;
        engine_command_result result = engine_replay_run(e, command);
        e->sim->settings.ai = ai;
        if (!result.command_id)
        {
            report.status  = ENGINE_REPLAY_REFUSED;
            report.refusal = (u8)result.refusal;
        }
    }
    if (report.status != ENGINE_REPLAY_OK) { arena->used = mark; return report; }

    engine_replay_snapshot(e, replayed);
    for (u32 index = 0; index < REPLAY_VALUE_COUNT; ++index)
    {
        if (original[index] == REPLAY_ABSENT && replayed[index] == REPLAY_ABSENT) continue;
        report.fields++;
        if (original[index] == replayed[index]) continue;
        char path[48];
        u8 field = engine_replay_describe(index, path, sizeof(path));
        report.differ++;
        if (replay_fields[field].verified) report.differ_verified++;
        if (e->replay_diff_count >= ENGINE_REPLAY_DIFF_CAPACITY) continue;
        engine_replay_diff *diff = e->replay_diffs + e->replay_diff_count++;
        memcpy(diff->path, path, sizeof(diff->path));
        diff->field    = field;
        diff->verified = replay_fields[field].verified;
        diff->before   = start[index];
        diff->replayed = replayed[index];
        diff->original = original[index];
        diff->rule     = replay_fields[field].rule;
        diff->q_id     = replay_fields[field].q_id;
    }
    report.diff_count = e->replay_diff_count;
    arena->used = mark;
    return report;
}

internal engine_replay_diff
engine_api_replay_diff_at(void *opaque, u32 index)
{
    engine *e = (engine *)opaque;
    engine_replay_diff empty = {};
    empty.rule = "";
    empty.q_id = "";
    return index < e->replay_diff_count ? e->replay_diffs[index] : empty;
}

/* ---- pairs.tsv ---------------------------------------------------------- */

internal void
engine_replay_officers(const char *text, u16 *officers)
{
    for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place) officers[place] = ENGINE_NO_OFFICER;
    u32 place = 0;
    while (*text && place < SIM_DEVELOP_OFFICERS)
    {
        if (*text < '0' || *text > '9') { ++text; continue; }
        u32 id = 0;
        while (*text >= '0' && *text <= '9') id = id * 10 + (u32)(*text++ - '0');
        officers[place++] = (u16)id;
    }
}

/* Reads pairs.tsv of the pack. A pack without the table has no pairs. A
   bad row ends the list. */
internal void
engine_load_replay_pairs(engine *e, const char *folder)
{
    static const char *kinds[ENGINE_REPLAY_COMMAND_COUNT] = { "-", "end_turn", "develop", "conscript", "patrol", "train", "merchant" };
    e->replay_pair_count = 0;
    tsv_table table = {};
    arena_reset(&e->scratch);
    if (!folder || !engine_read_table(e, folder, "pairs.tsv", &table)) return;
    for (u32 row = 0; row < table.row_count && e->replay_pair_count < ENGINE_REPLAY_PAIR_CAPACITY; ++row)
    {
        engine_replay_pair *pair = e->replay_pairs + e->replay_pair_count;
        *pair = {};
        u32 id = 0, number = 0, city = 0, facility = 0;
        const char *rule = tsv_cell(&table, row, "rule"), *path = tsv_cell(&table, row, "folder"), *q_ids = tsv_cell(&table, row, "q_ids");
        u32 kind = content_find_name(kinds, ENGINE_REPLAY_COMMAND_COUNT, tsv_cell(&table, row, "command"));
        if (!tsv_cell_u32(&table, row, "id", &id) || id != row || !tsv_cell_u32(&table, row, "number", &number) ||
            kind == ENGINE_REPLAY_COMMAND_COUNT || !rule[0] || strlen(rule) >= sizeof(pair->rule) ||
            !path[0] || strlen(path) >= sizeof(pair->folder) || strlen(q_ids) >= sizeof(pair->q_ids))
        {
            break;
        }
        memcpy(pair->rule, rule, strlen(rule) + 1);
        memcpy(pair->folder, path, strlen(path) + 1);
        memcpy(pair->q_ids, q_ids, strlen(q_ids) + 1);
        pair->number           = number;
        pair->command.kind     = (u8)kind;
        pair->command.city_id  = tsv_cell_u32(&table, row, "city", &city) ? (u8)city : SIM_NO_CITY;
        pair->command.facility = tsv_cell_u32(&table, row, "facility", &facility) ? (u8)facility : CONTENT_NO_FACILITY;
        engine_replay_officers(tsv_cell(&table, row, "officers"), pair->command.officers);
        u32 mode = 0, amount = 0;
        pair->command.merchant_mode = tsv_cell_u32(&table, row, "mode", &mode) ? (u8)mode : 0;
        pair->command.amount        = tsv_cell_u32(&table, row, "amount", &amount) ? amount : 0;
        e->replay_pair_count++;
    }
    arena_reset(&e->scratch);
}

internal u32
engine_api_replay_pair_count(void *opaque)
{
    return ((engine *)opaque)->replay_pair_count;
}

internal engine_replay_pair
engine_api_replay_pair_at(void *opaque, u32 index)
{
    engine *e = (engine *)opaque;
    engine_replay_pair empty = {};
    return index < e->replay_pair_count ? e->replay_pairs[index] : empty;
}

internal void
engine_bind_replay_api(engine_api *api)
{
    api->replay_pair_count = engine_api_replay_pair_count;
    api->replay_pair_at    = engine_api_replay_pair_at;
    api->replay_pair       = engine_api_replay_pair;
    api->replay_diff_at    = engine_api_replay_diff_at;
}
