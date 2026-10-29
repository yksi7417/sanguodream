/* The English keys of the records of a pack (docs/EUROPE.md, E2). The keys
   identify the officers, the factions and the cities for the files of the
   front end: the names, the lives and the arms. The Big5 bytes of a record
   are only for the display of Chinese text.
   - officers.tsv and forces.tsv of a pack scenario give the keys of the
     officers and of the factions, by id.
   - cities.tsv of the pack gives the keys of the cities, by id.
   A save keeps the ids of the scenario, so the keys of a save of the pack
   come from the same files. The engine reads the keys of the first
   scenario of its own pack at the start, and the keys of a pack scenario
   when it loads that scenario. A record of an original save has no key.
   Included by engine.cpp, after engine_pack_scenario.cpp. */

struct engine_record_keys
{
    char officers[SAVE_OFFICER_SLOTS][CONTENT_KEY_SIZE];
    char forces[SIM_FORCE_CAPACITY][CONTENT_KEY_SIZE];
    char cities[SAVE_CITY_COUNT][CONTENT_KEY_SIZE];
};

/* The keys of one table into slots by the column id. A row with a bad id
   or a long key gives no key. */
internal void
engine_keys_from_table(tsv_table *table, char (*slots)[CONTENT_KEY_SIZE], u32 slot_count)
{
    for (u32 row = 0; row < table->row_count; ++row)
    {
        u32 id = 0;
        const char *key = tsv_cell(table, row, "key");
        if (!tsv_cell_u32(table, row, "id", &id) || id >= slot_count || strlen(key) >= CONTENT_KEY_SIZE) continue;
        snprintf(slots[id], CONTENT_KEY_SIZE, "%s", key);
    }
}

/* The keys of the cities of the pack. A pack without the key column gives no keys. */
internal void
engine_load_city_keys(engine *e, const char *content_folder)
{
    if (!e->keys) return;
    memset(e->keys->cities, 0, sizeof(e->keys->cities));
    arena_reset(&e->scratch);
    tsv_table cities = {};
    if (engine_read_table(e, content_folder, "cities.tsv", &cities)) engine_keys_from_table(&cities, e->keys->cities, SAVE_CITY_COUNT);
}

/* The keys of the officers and the factions of a pack scenario. folder is
   the folder of its scenario.tsv. The old keys go away first. */
internal void
engine_load_scenario_keys(engine *e, const char *folder)
{
    if (!e->keys) return;
    memset(e->keys->officers, 0, sizeof(e->keys->officers));
    memset(e->keys->forces, 0, sizeof(e->keys->forces));
    arena_reset(&e->scratch);
    tsv_table officers = {}, forces = {};
    if (engine_read_table(e, folder, "officers.tsv", &officers)) engine_keys_from_table(&officers, e->keys->officers, SAVE_OFFICER_SLOTS);
    if (engine_read_table(e, folder, "forces.tsv", &forces))
    {
        engine_keys_from_table(&forces, e->keys->forces, SIM_FORCE_CAPACITY);
        engine_fill_church(e, &forces);   /* docs/EUROPE.md, E7: the condition church of history.tsv */
    }
}

/* At the start: the keys of the cities, and the keys of the first scenario
   of this pack, for a save of the pack. */
internal void
engine_load_record_keys(engine *e, const char *content_folder)
{
    engine_load_city_keys(e, content_folder);
    for (u32 index = 0; index < e->scenario_count; ++index)
    {
        engine_scenario *scenario = e->scenarios + index;
        if (scenario->kind != ENGINE_SCENARIO_PACK || scenario->pack[0]) continue;
        char folder[ENGINE_PATH_SIZE];
        snprintf(folder, sizeof(folder), "%s", scenario->file);
        char *slash = strrchr(folder, '/');
        if (slash) *slash = 0;
        engine_load_scenario_keys(e, folder);
        return;
    }
}

/* The key of an id, or "" for an id without a key. */
internal const char *
engine_officer_key(engine *e, u32 officer_id)
{
    return e->keys && officer_id < SAVE_OFFICER_SLOTS ? e->keys->officers[officer_id] : "";
}

internal const char *
engine_force_key(engine *e, u32 force_id)
{
    return e->keys && force_id < SIM_FORCE_CAPACITY ? e->keys->forces[force_id] : "";
}

internal const char *
engine_city_key(engine *e, u32 city_id)
{
    return e->keys && city_id < SAVE_CITY_COUNT ? e->keys->cities[city_id] : "";
}
