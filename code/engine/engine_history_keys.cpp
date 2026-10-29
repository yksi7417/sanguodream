/* The story of a pack (docs/EUROPE.md, E7): two things that the engine adds
   to the tables of the pack before the sim reads them.
   - A row of history.tsv names an officer as @key, for example @baybars.
     The engine finds the key in officers.tsv of the first scenario of the
     pack. It adds the Big5 name of the officer to the names column and
     writes the index in place of the key. A row with @0 stays as it is.
     The rows of the Three Kingdoms have no keys.
   - The column church of forces.tsv gives the church of each faction.
   Included by engine.cpp, after engine_keys.cpp. */

/* The folder of the first scenario of the pack, from scenarios.tsv. False
   if the pack has no scenario of its own. */
internal b32
engine_first_scenario_folder(engine *e, const char *content_folder, char *out, u32 out_size)
{
    tsv_table scenarios = {};
    if (!engine_read_table(e, content_folder, "scenarios.tsv", &scenarios)) return false;
    for (u32 row = 0; row < scenarios.row_count; ++row)
    {
        const char *pack = tsv_cell(&scenarios, row, "pack"), *file = tsv_cell(&scenarios, row, "file");
        if (strcmp(tsv_cell(&scenarios, row, "kind"), "pack") != 0 || (pack[0] && strcmp(pack, "-") != 0)) continue;
        const char *slash = strrchr(file, '/');
        u32 length = slash ? (u32)(slash - file) : 0;
        return (u32)snprintf(out, out_size, "%s/%.*s", content_folder, (int)length, file) < out_size;
    }
    return false;
}

/* True if the text has an @ with a letter after it: the key of an officer. */
internal b32
engine_history_has_key(const char *text)
{
    for (const char *at = text; *at; ++at)
    {
        if (*at == '@' && ((at[1] >= 'a' && at[1] <= 'z') || at[1] == '_')) return true;
    }
    return false;
}

/* The row of officers.tsv with this key, or -1. */
internal s32
engine_officer_row_by_key(tsv_table *officers, const char *key, u32 length)
{
    for (u32 row = 0; row < officers->row_count; ++row)
    {
        const char *cell = tsv_cell(officers, row, "key");
        if (strlen(cell) == length && strncmp(cell, key, length) == 0) return (s32)row;
    }
    return -1;
}

/* The state of the rewrite of one row. */
struct engine_history_names
{
    char  names[CONTENT_HISTORY_TEXT];
    u32   count;
    char  keys[CONTENT_HISTORY_NAMES][CONTENT_KEY_SIZE];   /* the key of each name that the rewrite added */
};

/* The index of the officer with this key in the names of the row. It adds the name if it is not there. */
internal s32
engine_history_name_index(engine_history_names *state, tsv_table *officers, const char *key, u32 length)
{
    if (length >= CONTENT_KEY_SIZE) return -1;
    for (u32 index = 0; index < state->count; ++index)
    {
        if (strlen(state->keys[index]) == length && strncmp(state->keys[index], key, length) == 0) return (s32)index;
    }
    s32 row = engine_officer_row_by_key(officers, key, length);
    if (row < 0 || state->count >= CONTENT_HISTORY_NAMES) return -1;
    const char *family = tsv_cell(officers, (u32)row, "family_big5"), *given = tsv_cell(officers, (u32)row, "given_big5");
    u32 used = (u32)strlen(state->names);
    if (!family[0] || !given[0] || used + strlen(family) + strlen(given) + 2 > sizeof(state->names)) return -1;
    used += (u32)snprintf(state->names + used, sizeof(state->names) - used, "%s%s%s", state->count ? "|" : "", family, given);
    memcpy(state->keys[state->count], key, length);
    state->keys[state->count][length] = 0;
    return (s32)state->count++;
}

/* Copies a cell and writes each @key as @index. False if a key has no officer or the text is too long. */
internal b32
engine_history_rewrite_cell(engine_history_names *state, tsv_table *officers, const char *in, char *out, u32 out_size)
{
    u32 used = 0;
    for (const char *at = in; *at;)
    {
        if (used + 12 >= out_size) return false;
        b32 is_key = *at == '@' && ((at[1] >= 'a' && at[1] <= 'z') || at[1] == '_');
        if (!is_key) { out[used++] = *at++; continue; }
        const char *start = ++at;
        while ((*at >= 'a' && *at <= 'z') || (*at >= '0' && *at <= '9') || *at == '_') ++at;
        s32 index = engine_history_name_index(state, officers, start, (u32)(at - start));
        if (index < 0) return false;
        used += (u32)snprintf(out + used, out_size - used, "@%d", index);
    }
    out[used] = 0;
    return true;
}

/* Rewrites the keys of the rows of history.tsv. False if a row names an officer that the pack does not have. */
internal b32
engine_rewrite_history_keys(engine *e, const char *content_folder, tsv_table *history)
{
    s32 conditions_column = tsv_column(history, "conditions"), effects_column = tsv_column(history, "effects"), names_column = tsv_column(history, "names");
    if (conditions_column < 0 || effects_column < 0 || names_column < 0) return true;
    b32 any = false;
    for (u32 row = 0; row < history->row_count && !any; ++row)
    {
        any = engine_history_has_key(tsv_cell(history, row, "conditions")) || engine_history_has_key(tsv_cell(history, row, "effects"));
    }
    if (!any) return true;
    char folder[ENGINE_PATH_SIZE];
    tsv_table officers = {};
    if (!engine_first_scenario_folder(e, content_folder, folder, sizeof(folder)) || !engine_read_table(e, folder, "officers.tsv", &officers)) return false;
    for (u32 row = 0; row < history->row_count; ++row)
    {
        const char *conditions = tsv_cell(history, row, "conditions"), *effects = tsv_cell(history, row, "effects");
        if (!engine_history_has_key(conditions) && !engine_history_has_key(effects)) continue;
        engine_history_names state = {};
        const char *names = tsv_cell(history, row, "names");
        if (strcmp(names, "-") != 0)
        {
            if (strlen(names) >= sizeof(state.names)) return false;
            memcpy(state.names, names, strlen(names) + 1);
            state.count = 1;
            for (const char *at = names; *at; ++at) state.count += *at == '|';
        }
        char *new_conditions = PushArray(&e->scratch, CONTENT_HISTORY_TEXT, char);
        char *new_effects    = PushArray(&e->scratch, CONTENT_HISTORY_TEXT, char);
        char *new_names      = PushArray(&e->scratch, CONTENT_HISTORY_TEXT, char);
        /* The old names have no key, so the first new name gets the index after them. */
        if (!engine_history_rewrite_cell(&state, &officers, conditions, new_conditions, CONTENT_HISTORY_TEXT) ||
            !engine_history_rewrite_cell(&state, &officers, effects, new_effects, CONTENT_HISTORY_TEXT)) return false;
        snprintf(new_names, CONTENT_HISTORY_TEXT, "%s", state.count ? state.names : "-");
        history->cells[row * history->column_count + (u32)conditions_column] = new_conditions;
        history->cells[row * history->column_count + (u32)effects_column]    = new_effects;
        history->cells[row * history->column_count + (u32)names_column]      = new_names;
    }
    return true;
}

/* The church of each faction from the column church of forces.tsv. A faction without a church has none. */
internal void
engine_fill_church(engine *e, tsv_table *forces)
{
    if (!e->content.loaded) return;
    memset(e->sim->content.force_church, 0, sizeof(e->sim->content.force_church));
    for (u32 row = 0; row < forces->row_count; ++row)
    {
        u32 id = 0;
        if (!tsv_cell_u32(forces, row, "id", &id) || id >= CONTENT_FORCE_CAPACITY) continue;
        u32 church = content_find_church(tsv_cell(forces, row, "church"));
        e->sim->content.force_church[id] = (u8)(church < CONTENT_CHURCH_COUNT ? church : CONTENT_CHURCH_NONE);
    }
}

/* The authority of each faction from the column authority of forces.tsv (E6). The value is the
   place in authorities.tsv plus one. A faction without an authority, or a pack without the column, has 0. */
internal void
engine_fill_authority(engine *e, tsv_table *forces)
{
    if (!e->content.loaded) return;
    memset(e->sim->content.force_authority, 0, sizeof(e->sim->content.force_authority));
    for (u32 row = 0; row < forces->row_count; ++row)
    {
        u32 id = 0;
        if (!tsv_cell_u32(forces, row, "id", &id) || id >= CONTENT_FORCE_CAPACITY) continue;
        u32 authority = content_find_authority(&e->sim->content, tsv_cell(forces, row, "authority"));
        if (authority < e->sim->content.authority_count) e->sim->content.force_authority[id] = (u8)(authority + 1);
    }
}
