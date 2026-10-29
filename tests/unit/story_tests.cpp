/* Unit tests of the event rows of the Europe packs (docs/EUROPE.md, E7).
   Each test reads the real history.tsv of a pack. It builds the state of the
   preconditions of one row and checks that the row is ready, that it fires
   once, and that its effects change the state. Then it builds the state again
   with one precondition false for each variant and checks that the row is not
   ready. The engine changes @key to an officer before the sim reads a row.
   These tests do the same with a small change of their own. Each key gets a
   name of two ASCII bytes, so no Big5 table is needed. */

#define UNIT_STORY_KEYS        32
#define UNIT_STORY_CELLS       9
#define UNIT_STORY_ROW_NAMES   8
#define UNIT_STORY_FIRST_KEY   200     /* the officer id of the first key */
#define UNIT_STORY_RULER_ID    100     /* the officer id of an anonymous ruler is this + the force id */
#define UNIT_STORY_MEMBER_ID   150     /* the officer id of an anonymous member is this + the force id */

struct unit_story_keys
{
    char key[UNIT_STORY_KEYS][CONTENT_KEY_SIZE];
    u32  count;
};

/* The place of a key in the list. A key that the list lacks is added. */
internal u32
unit_story_key_index(unit_story_keys *keys, const char *key, u32 length)
{
    for (u32 index = 0; index < keys->count; ++index)
    {
        if (strlen(keys->key[index]) == length && memcmp(keys->key[index], key, length) == 0) return index;
    }
    u32 index = keys->count++;
    memcpy(keys->key[index], key, length);
    keys->key[index][length] = 0;
    return index;
}

struct unit_story_row_names
{
    u32 key[UNIT_STORY_ROW_NAMES];     /* the index in the list of keys */
    u32 count;
};

/* Copies a cell and changes each @key to @n, where n is the place of the key in the names of the row. */
internal void
unit_story_rewrite_cell(char *out, u32 *used, const char *cell, u32 size, unit_story_keys *keys, unit_story_row_names *names)
{
    for (u32 at = 0; at < size; ++at)
    {
        b32 is_key = cell[at] == '@' && at + 1 < size && ((cell[at + 1] >= 'a' && cell[at + 1] <= 'z') || cell[at + 1] == '_');
        if (!is_key)
        {
            out[(*used)++] = cell[at];
            continue;
        }
        u32 start = at + 1, end = start;
        while (end < size && ((cell[end] >= 'a' && cell[end] <= 'z') || cell[end] == '_' || (cell[end] >= '0' && cell[end] <= '9'))) ++end;
        u32 global = unit_story_key_index(keys, cell + start, end - start);
        u32 local = names->count;
        for (u32 index = 0; index < names->count; ++index) if (names->key[index] == global) local = index;
        if (local == names->count && names->count < UNIT_STORY_ROW_NAMES) names->key[names->count++] = global;
        out[(*used)++] = '@';
        out[(*used)++] = (char)('0' + local);
        at = end - 1;
    }
}

/* One row of the table: the cells 4 and 5 change, and the cell 7 gets the names of the keys. */
internal void
unit_story_rewrite_row(char *out, u32 *used, const char *line, u32 size, unit_story_keys *keys)
{
    const char *cell[UNIT_STORY_CELLS];
    u32 cell_size[UNIT_STORY_CELLS];
    u32 count = 0, start = 0;
    for (u32 at = 0; at <= size && count < UNIT_STORY_CELLS; ++at)
    {
        if (at == size || line[at] == '\t')
        {
            cell[count]      = line + start;
            cell_size[count] = at - start;
            ++count;
            start = at + 1;
        }
    }
    unit_story_row_names names = {};
    for (u32 index = 0; index < count; ++index)
    {
        if (index == 7) continue;
        if (index > 0) out[(*used)++] = '\t';
        if (index == 4 || index == 5) unit_story_rewrite_cell(out, used, cell[index], cell_size[index], keys, &names);
        else { memcpy(out + *used, cell[index], cell_size[index]); *used += cell_size[index]; }
        if (index == 6)
        {
            out[(*used)++] = '\t';
            /* The names column goes here, after the conditions and the effects have given the keys. */
            if (names.count == 0) out[(*used)++] = '-';
            for (u32 name = 0; name < names.count; ++name)
            {
                if (name) out[(*used)++] = '|';
                *used += (u32)snprintf(out + *used, 8, "4B%02X", 0x61 + names.key[name]);
            }
        }
    }
    out[(*used)++] = '\n';
}

/* The text of history.tsv with the keys changed. The lines of the header and the comments stay. */
internal char *
unit_story_rewrite(memory_arena *arena, const char *text, unit_story_keys *keys)
{
    char *out = (char *)arena_push_size(arena, strlen(text) * 2 + 4096);
    u32 used = 0;
    const char *line = text;
    while (*line)
    {
        const char *end = strchr(line, '\n');
        u32 length = end ? (u32)(end - line) : (u32)strlen(line);
        u32 clean  = length;
        if (clean && line[clean - 1] == '\r') --clean;
        b32 plain = clean == 0 || line[0] == '#' || strncmp(line, "id\t", 3) == 0;
        if (plain) { memcpy(out + used, line, clean); used += clean; out[used++] = '\n'; }
        else unit_story_rewrite_row(out, &used, line, clean, keys);
        line += length + (end ? 1 : 0);
    }
    out[used] = 0;
    return out;
}

/* Loads the history.tsv of the pack into the content of the sim. False if the file or a row is bad. */
internal b32
unit_story_load(sim_state *sim, memory_arena *arena, const char *pack, unit_story_keys *keys)
{
    char exe_dir[MAX_PATH], path[MAX_PATH];
    test_exe_dir(exe_dir, MAX_PATH);
    snprintf(path, MAX_PATH, "%s/../data/%s/history.tsv", exe_dir, pack);
    u64 mark = arena->used;
    char *text = (char *)test_read_file(path, arena).contents;
    keys->count = 0;
    tsv_table table = {};
    u32 bad_row = 0;
    b32 loaded = text && tsv_parse(unit_story_rewrite(arena, text, keys), arena, &table) && content_load_history(&sim->content, &table, &bad_row);
    arena->used = mark;
    return loaded;
}

/* Loads authorities.tsv of the pack, with the cities that its seats name. The rows of E6 need it. */
internal void
unit_story_load_authorities(sim_state *sim, memory_arena *arena, const char *pack)
{
    char exe_dir[MAX_PATH], path[MAX_PATH];
    test_exe_dir(exe_dir, MAX_PATH);
    u64 mark = arena->used;
    snprintf(path, MAX_PATH, "%s/../data/%s/authorities.tsv", exe_dir, pack);
    char *authorities_text = (char *)test_read_file(path, arena).contents;
    snprintf(path, MAX_PATH, "%s/../data/%s/cities.tsv", exe_dir, pack);
    char *cities_text = (char *)test_read_file(path, arena).contents;
    tsv_table authorities = {}, cities = {};
    u32 bad_row = 0;
    b32 loaded = authorities_text && cities_text && tsv_parse(authorities_text, arena, &authorities) && tsv_parse(cities_text, arena, &cities) &&
                 content_load_authorities(&sim->content, &authorities, &cities, &bad_row);
    arena->used = mark;
    if (!loaded) sim->content.authority_count = 0;
}

/* ---- the world of a row ------------------------------------------------------------------------ */

/* The home city of each faction. The ids stay away from the cities that the rows name. */
global_variable const u8 unit_story_homes[17] = { 0, 1, 2, 5, 7, 8, 9, 10, 12, 13, 15, 16, 17, 19, 20, 21, 22 };

/* A sim at the date with 42 cities that nobody owns, and the rows of the pack. */
internal sim_state *
unit_story_world(memory_arena *arena, unit_story_keys *keys, const char *pack, u32 year, u32 month)
{
    sim_state *sim = unit_turn_sim(arena);
    sim->has_content      = true;
    sim->turns.date       = calendar_make((u16)year, (u8)month, 1);
    sim->turns.start_date = calendar_make(1200, 1, 1);
    sim->turn             = 100;
    for (u8 city = 0; city < 42; ++city)
    {
        city_component each = unit_city(city, SIM_NO_FORCE, 1000);
        each.public_order = 50;
        each.morale       = 50;
        each.durability_maximum = 1000;
        each.durability   = 800;
        sim_add_city(sim, each);
    }
    if (!unit_story_load(sim, arena, pack, keys)) sim->content.history_count = 0;
    unit_story_load_authorities(sim, arena, pack);
    return sim;
}

internal void
unit_story_give(sim_state *sim, u8 city, u8 owner)
{
    sim_find_city(sim, city)->owner_force_id = owner;
}

/* The officer of a key. The key is one that the rows name, or a new one. */
internal officer_component *
unit_story_officer(sim_state *sim, unit_story_keys *keys, const char *key, u8 force, u8 status, u8 city, u16 appear, u16 merit)
{
    u32 index = unit_story_key_index(keys, key, (u32)strlen(key));
    officer_component officer = unit_officer((u16)(UNIT_STORY_FIRST_KEY + index), force, status, city, 60, 60, 60, 60, merit);
    officer.appearance_year = appear;
    officer.loyalty         = 80;
    unit_name_officer(&officer, 'K', (char)('a' + index));
    sim_add_officer(sim, officer);
    return sim_find_officer(sim, officer.officer_id);
}

internal officer_component *
unit_story_key_officer(sim_state *sim, unit_story_keys *keys, const char *key)
{
    return sim_find_officer(sim, (u16)(UNIT_STORY_FIRST_KEY + unit_story_key_index(keys, key, (u32)strlen(key))));
}

/* A faction: it owns its home city and has a ruler. The ruler is the officer of the key, or a new one. */
internal void
unit_story_faction(sim_state *sim, unit_story_keys *keys, u8 force, const char *ruler_key)
{
    u8 home = unit_story_homes[force];
    unit_story_give(sim, home, force);
    if (ruler_key)
    {
        unit_story_officer(sim, keys, ruler_key, force, OFFICER_STATUS_RULER, home, 1100, 1000);
        return;
    }
    officer_component ruler = unit_officer((u16)(UNIT_STORY_RULER_ID + force), force, OFFICER_STATUS_RULER, home, 70, 70, 70, 70, 1000);
    unit_name_officer(&ruler, 'Z', (char)('a' + force));
    sim_add_officer(sim, ruler);
}

/* An officer of the faction who is not the ruler, with loyalty 80. */
internal void
unit_story_member(sim_state *sim, u8 force)
{
    officer_component member = unit_officer((u16)(UNIT_STORY_MEMBER_ID + force), force, OFFICER_STATUS_REGULAR_OFFICER, unit_story_homes[force], 50, 50, 50, 50, 300);
    unit_name_officer(&member, 'Y', (char)('a' + force));
    sim_add_officer(sim, member);
    sim_find_officer(sim, member.officer_id)->loyalty = 80;   /* the sim sets the loyalty when it adds an officer */
}

internal void
unit_story_started(sim_state *sim, const char *row_key)
{
    sim->history_turn[sim_history_row_of(sim, row_key)] = sim->turn + 1;
}

/* ---- the cases --------------------------------------------------------------------------------- */

struct unit_story_case
{
    const char *key;                 /* the key of the row */
    u8          force;               /* the faction that gets the row */
    u32         variants;            /* the variants 1 to n each make one precondition false */
    sim_state *(*build)(memory_arena *arena, unit_story_keys *keys, u32 variant);
    void      (*effects)(test_counts *counts, sim_state *sim, unit_story_keys *keys);
};

/* Checks each case: ready with all preconditions, not ready with one false, one start in two months. */
internal void
unit_story_run(test_counts *counts, memory_arena *arena, const char *pack, const unit_story_case *cases, u32 case_count)
{
    char name[160];
    u64 mark = arena->used;
    unit_story_keys keys = {};
    sim_state *sim = unit_story_world(arena, &keys, pack, 1200, 1);
    snprintf(name, sizeof(name), "%s: history.tsv loads with %u rows that name only officers of the pack", pack, case_count);
    TEST_CHECK(counts, sim->content.history_count == case_count, name);
    for (u32 row = 0; row < sim->content.history_count; ++row)
    {
        history_kind *kind = sim->content.history + row;
        snprintf(name, sizeof(name), "%s: row %s has a chance of 1 to 100 percent", pack, kind->key);
        TEST_CHECK(counts, kind->chance >= 1 && kind->chance <= 100, name);
    }
    arena->used = mark;

    for (u32 index = 0; index < case_count; ++index)
    {
        const unit_story_case *each = cases + index;
        sim = each->build(arena, &keys, 0);
        u32 row = sim_history_row_of(sim, each->key);
        snprintf(name, sizeof(name), "%s: row %s is ready when every precondition holds", pack, each->key);
        TEST_CHECK(counts, row < sim->content.history_count && sim_history_ready(sim, row, each->force), name);
        sim->content.history[row].chance = 100;
        sim_event_sink sink = sim_sink_from_arena(arena, 256);
        sim_run_history_month(sim, 0, &sink);
        sim_run_history_month(sim, 0, &sink);
        u32 fired = 0;
        for (u32 at = 0; at < sink.count; ++at) fired += sink.events[at].kind == SIM_EVENT_HISTORY_FIRED && sink.events[at].detail == row && sink.events[at].faction == each->force;
        snprintf(name, sizeof(name), "%s: row %s fires once for its faction in two months", pack, each->key);
        TEST_CHECK(counts, fired == 1 && sim->history_turn[row] == sim->turn + 1, name);
        each->effects(counts, sim, &keys);
        arena->used = mark;

        for (u32 variant = 1; variant <= each->variants; ++variant)
        {
            sim = each->build(arena, &keys, variant);
            row = sim_history_row_of(sim, each->key);
            snprintf(name, sizeof(name), "%s: row %s is not ready when precondition %u fails", pack, each->key, variant);
            TEST_CHECK(counts, row < sim->content.history_count && !sim_history_ready(sim, row, each->force), name);
            arena->used = mark;
        }
    }
}

/* ---- the east map ------------------------------------------------------------------------------ */

/* Forces of the east: 0 Byzantium, 3 Ayyubid, 5 Austria, 6 papacy, 7 Venice, 12 Antioch.
   Cities: 4 Cairo, 6 Constantinople, 11 Rome, 14 Venice, 34 Antioch. */

internal sim_state *
unit_east_fourth_crusade(memory_arena *arena, unit_story_keys *keys, u32 variant)
{
    u32 year = 1204, month = 3;
    if (variant == 1) { year = 1211; month = 1; }
    if (variant == 2) { year = 1201; month = 12; }
    sim_state *sim = unit_story_world(arena, keys, "medieval_east", year, month);
    unit_story_faction(sim, keys, 7, 0);
    unit_story_faction(sim, keys, 6, 0);
    unit_story_give(sim, 14, variant == 3 ? 0 : 7);
    unit_story_give(sim, 11, variant == 4 ? 5 : 6);
    sim->content.force_church[7] = (u8)(variant == 5 ? CONTENT_CHURCH_GREEK : CONTENT_CHURCH_LATIN);
    unit_story_officer(sim, keys, "baldwin_ix", SIM_NO_FORCE, OFFICER_STATUS_FREE_OR_NOT_APPEARED, 14, 1202, 0);
    unit_story_officer(sim, keys, "henry_flanders", SIM_NO_FORCE, OFFICER_STATUS_FREE_OR_NOT_APPEARED, 14, 1202, 0);
    unit_story_officer(sim, keys, "montferrat", SIM_NO_FORCE, OFFICER_STATUS_FREE_OR_NOT_APPEARED, 15, 1150, 0);
    return sim;
}

internal void
unit_east_fourth_crusade_effects(test_counts *counts, sim_state *sim, unit_story_keys *keys)
{
    officer_component *baldwin = unit_story_key_officer(sim, keys, "baldwin_ix");
    officer_component *henry   = unit_story_key_officer(sim, keys, "henry_flanders");
    officer_component *margrave = unit_story_key_officer(sim, keys, "montferrat");
    TEST_CHECK(counts, baldwin->force_id == 7 && henry->force_id == 7 && margrave->force_id == 7 &&
                       baldwin->status != OFFICER_STATUS_FREE_OR_NOT_APPEARED && baldwin->loyalty == 100 && baldwin->merit == 1500 && henry->merit == 1200,
               "medieval_east: the Fourth Crusade gives Baldwin, Henry and Montferrat to Venice with loyalty 100 and their merit");
    TEST_CHECK(counts, sim_find_city(sim, 14)->public_order == 60, "medieval_east: the Fourth Crusade raises the public order of Venice by 10");
}

internal sim_state *
unit_east_sack(memory_arena *arena, unit_story_keys *keys, u32 variant)
{
    u32 year = 1205, month = 1;
    if (variant == 2) { year = 1213; month = 1; }
    if (variant == 3) { year = 1202; month = 12; }
    sim_state *sim = unit_story_world(arena, keys, "medieval_east", year, month);
    unit_story_faction(sim, keys, 0, 0);
    unit_story_member(sim, 0);
    unit_story_give(sim, 6, variant == 4 ? 2 : 0);
    sim->content.force_church[0] = (u8)(variant == 5 ? CONTENT_CHURCH_LATIN : CONTENT_CHURCH_GREEK);
    if (variant != 1) unit_story_started(sim, "fourth_crusade");
    return sim;
}

internal void
unit_east_sack_effects(test_counts *counts, sim_state *sim, unit_story_keys *keys)
{
    (void)keys;
    city_component *city = sim_find_city(sim, 6);
    TEST_CHECK(counts, city->public_order == 30 && city->morale == 40 && sim_find_officer(sim, UNIT_STORY_MEMBER_ID + 0)->loyalty == 70,
               "medieval_east: the sack lowers the public order by 20, the morale by 10 and the loyalty of the officers by 10");
}

internal sim_state *
unit_east_mamluks(memory_arena *arena, unit_story_keys *keys, u32 variant)
{
    u32 year = 1250, month = 6;
    if (variant == 1) { year = 1239; month = 12; }
    if (variant == 2) { year = 1271; month = 1; }
    sim_state *sim = unit_story_world(arena, keys, "medieval_east", year, month);
    unit_story_faction(sim, keys, 3, 0);
    unit_story_give(sim, 4, variant == 3 ? 2 : 3);
    unit_story_officer(sim, keys, "aybak", SIM_NO_FORCE, OFFICER_STATUS_FREE_OR_NOT_APPEARED, 4, 1228, 0);
    unit_story_officer(sim, keys, "shajar_al_durr", SIM_NO_FORCE, OFFICER_STATUS_FREE_OR_NOT_APPEARED, 4, 1229, 0);
    if (variant == 4) unit_story_officer(sim, keys, "qutuz", 5, OFFICER_STATUS_REGULAR_OFFICER, 4, 1240, 0);
    else              unit_story_officer(sim, keys, "qutuz", SIM_NO_FORCE, OFFICER_STATUS_FREE_OR_NOT_APPEARED, 4, variant == 5 ? 1260 : 1240, 0);
    unit_story_officer(sim, keys, "baybars", SIM_NO_FORCE, OFFICER_STATUS_FREE_OR_NOT_APPEARED, 4, 1240, 0);
    return sim;
}

internal void
unit_east_mamluks_effects(test_counts *counts, sim_state *sim, unit_story_keys *keys)
{
    officer_component *aybak   = unit_story_key_officer(sim, keys, "aybak");
    officer_component *baybars = unit_story_key_officer(sim, keys, "baybars");
    TEST_CHECK(counts, aybak->force_id == 3 && baybars->force_id == 3 && unit_story_key_officer(sim, keys, "qutuz")->force_id == 3 &&
                       unit_story_key_officer(sim, keys, "shajar_al_durr")->force_id == 3 && baybars->merit == 2500,
               "medieval_east: the four officers of Cairo join the Ayyubids with their merit");
    TEST_CHECK(counts, sim_force_ruler(sim, 3) == aybak->officer_id && sim_find_officer(sim, UNIT_STORY_RULER_ID + 3)->force_id == 3,
               "medieval_east: Aybak takes the rule of the faction and the old ruler stays");
    char key[CONTENT_KEY_SIZE] = {};
    TEST_CHECK(counts, sim_force_rename_key(sim, 3, key, sizeof(key)) && strcmp(key, "mamluk") == 0, "medieval_east: the faction gets the name mamluk");
}

internal sim_state *
unit_east_sultan(memory_arena *arena, unit_story_keys *keys, u32 variant)
{
    u32 year = 1260, month = 1;
    if (variant == 2) { year = 1256; month = 12; }
    if (variant == 3) { year = 1276; month = 1; }
    sim_state *sim = unit_story_world(arena, keys, "medieval_east", year, month);
    unit_story_faction(sim, keys, 3, "aybak");
    unit_story_officer(sim, keys, "baybars", variant == 4 ? 5 : 3, OFFICER_STATUS_REGULAR_OFFICER, unit_story_homes[3], 1240, variant == 5 ? 2999 : 3000);
    if (variant != 1) unit_story_started(sim, "mamluks_take_cairo");
    return sim;
}

internal void
unit_east_sultan_effects(test_counts *counts, sim_state *sim, unit_story_keys *keys)
{
    TEST_CHECK(counts, sim_force_ruler(sim, 3) == unit_story_key_officer(sim, keys, "baybars")->officer_id &&
                       unit_story_key_officer(sim, keys, "aybak")->status == OFFICER_STATUS_REGULAR_OFFICER,
               "medieval_east: Baybars takes the rule and Aybak stays in the faction");
}

internal sim_state *
unit_east_siege(memory_arena *arena, unit_story_keys *keys, u32 variant)
{
    u32 year = 1265, month = 1;
    if (variant == 1) { year = 1259; month = 12; }
    if (variant == 2) { year = 1291; month = 1; }
    sim_state *sim = unit_story_world(arena, keys, "medieval_east", year, month);
    if (variant == 3)
    {
        unit_story_faction(sim, keys, 3, 0);
        unit_story_officer(sim, keys, "baybars", 3, OFFICER_STATUS_REGULAR_OFFICER, unit_story_homes[3], 1240, 3000);
    }
    else unit_story_faction(sim, keys, 3, "baybars");
    unit_story_give(sim, 34, variant == 4 ? 3 : 12);
    return sim;
}

internal void
unit_east_siege_effects(test_counts *counts, sim_state *sim, unit_story_keys *keys)
{
    (void)keys;
    TEST_CHECK(counts, sim_history_target(sim, 3) == 34 && sim_history_target(sim, 12) == SIM_NO_CITY,
               "medieval_east: the siege gives Antioch as the target of the faction of Baybars and no other");
}

internal sim_state *
unit_east_fall(memory_arena *arena, unit_story_keys *keys, u32 variant)
{
    u32 year = 1270, month = 1;
    if (variant == 2) { year = 1296; month = 1; }
    sim_state *sim = unit_story_world(arena, keys, "medieval_east", year, month);
    if (variant == 3)
    {
        unit_story_faction(sim, keys, 3, 0);
        unit_story_officer(sim, keys, "baybars", 3, OFFICER_STATUS_REGULAR_OFFICER, unit_story_homes[3], 1240, 3000);
    }
    else unit_story_faction(sim, keys, 3, "baybars");
    unit_story_give(sim, 34, variant == 4 ? 12 : 3);
    sim_find_city(sim, 34)->troops = 6000;
    if (variant != 1) unit_story_started(sim, "siege_of_antioch");
    return sim;
}

internal void
unit_east_fall_effects(test_counts *counts, sim_state *sim, unit_story_keys *keys)
{
    (void)keys;
    city_component *city = sim_find_city(sim, 34);
    TEST_CHECK(counts, city->troops == 3000 && city->durability == 400, "medieval_east: the fall of Antioch takes half of its troops and of its walls");
}

/* The sees of E6. Authority keys: constantinople, antioch, abbasid. City 38 is Baghdad. */

internal sim_state *
unit_east_latin_patriarch(memory_arena *arena, unit_story_keys *keys, u32 variant)
{
    u32 year = 1210, month = 1;
    if (variant == 1) { year = 1261; month = 1; }
    if (variant == 2) { year = 1203; month = 12; }
    sim_state *sim = unit_story_world(arena, keys, "medieval_east", year, month);
    unit_story_faction(sim, keys, 7, 0);
    unit_story_faction(sim, keys, 0, 0);
    unit_story_give(sim, 6, variant == 4 ? 0 : 7);
    sim->content.force_church[7] = (u8)(variant == 5 ? CONTENT_CHURCH_GREEK : CONTENT_CHURCH_LATIN);
    if (variant != 3) unit_story_started(sim, "fourth_crusade");
    return sim;
}

internal void
unit_east_latin_patriarch_effects(test_counts *counts, sim_state *sim, unit_story_keys *keys)
{
    (void)keys;
    u32 see = content_find_authority(&sim->content, "constantinople");
    TEST_CHECK(counts, sim_authority_church(sim, see) == CONTENT_CHURCH_LATIN && sim_authority_keeper(sim, see) == 7,
               "medieval_east: the see of Constantinople gets a Latin holder, and the owner of the city is the keeper");
}

internal sim_state *
unit_east_antioch_patriarch(memory_arena *arena, unit_story_keys *keys, u32 variant)
{
    u32 year = 1270, month = 1;
    if (variant == 1) { year = 1301; month = 1; }
    sim_state *sim = unit_story_world(arena, keys, "medieval_east", year, month);
    if (variant == 3)
    {
        unit_story_faction(sim, keys, 3, 0);
        unit_story_officer(sim, keys, "baybars", 3, OFFICER_STATUS_REGULAR_OFFICER, unit_story_homes[3], 1240, 3000);
    }
    else unit_story_faction(sim, keys, 3, "baybars");
    unit_story_give(sim, 34, variant == 4 ? 12 : 3);
    if (variant != 2) unit_story_started(sim, "fall_of_antioch");
    return sim;
}

internal void
unit_east_antioch_patriarch_effects(test_counts *counts, sim_state *sim, unit_story_keys *keys)
{
    (void)keys;
    u32 see = content_find_authority(&sim->content, "antioch");
    TEST_CHECK(counts, sim_authority_church(sim, see) == CONTENT_CHURCH_NONE && sim_authority_keeper(sim, see) == SIM_NO_FORCE,
               "medieval_east: the fall of Antioch leaves the see with no holder and no keeper");
}

internal sim_state *
unit_east_caliph(memory_arena *arena, unit_story_keys *keys, u32 variant)
{
    u32 year = 1265, month = 1;
    if (variant == 1) { year = 1260; month = 12; }
    if (variant == 2) { year = 1276; month = 1; }
    sim_state *sim = unit_story_world(arena, keys, "medieval_east", year, month);
    if (variant == 3)
    {
        unit_story_faction(sim, keys, 3, 0);
        unit_story_officer(sim, keys, "baybars", 3, OFFICER_STATUS_REGULAR_OFFICER, unit_story_homes[3], 1240, 3000);
    }
    else unit_story_faction(sim, keys, 3, "baybars");
    unit_story_faction(sim, keys, 16, 0);
    unit_story_give(sim, 4, variant == 4 ? 12 : 3);
    unit_story_give(sim, 38, variant == 5 ? 16 : 5);
    return sim;
}

internal void
unit_east_caliph_effects(test_counts *counts, sim_state *sim, unit_story_keys *keys)
{
    (void)keys;
    u32 see = content_find_authority(&sim->content, "abbasid");
    TEST_CHECK(counts, sim_authority_seat(sim, see) == 4 && sim_authority_keeper(sim, see) == 3,
               "medieval_east: the caliph moves to Cairo, and its owner becomes the keeper");
}

internal void
unit_story_east_tests(test_counts *counts, memory_arena *arena)
{
    static const unit_story_case cases[] =
    {
        { "fourth_crusade",       7, 5, unit_east_fourth_crusade, unit_east_fourth_crusade_effects },
        { "sack_of_constantinople", 0, 5, unit_east_sack,         unit_east_sack_effects },
        { "mamluks_take_cairo",   3, 5, unit_east_mamluks,        unit_east_mamluks_effects },
        { "baybars_sultan",       3, 5, unit_east_sultan,         unit_east_sultan_effects },
        { "siege_of_antioch",     3, 4, unit_east_siege,          unit_east_siege_effects },
        { "fall_of_antioch",      3, 4, unit_east_fall,           unit_east_fall_effects },
        { "latin_patriarch", 7, 5, unit_east_latin_patriarch, unit_east_latin_patriarch_effects },
        { "antioch_patriarch_lost", 3, 4, unit_east_antioch_patriarch, unit_east_antioch_patriarch_effects },
        { "caliph_to_cairo",      3, 5, unit_east_caliph,         unit_east_caliph_effects },
    };
    unit_story_run(counts, arena, "medieval_east", cases, ArrayCount(cases));
}

/* ---- the west map ------------------------------------------------------------------------------ */

/* Forces of the west: 0 France, 1 Angevin, 2 Toulouse, 3 Aragon, 4 Castile, 7 Almohad, 10 Welf, 13 papacy.
   Cities: 3 Seville, 18 Toulouse. */

internal sim_state *
unit_west_albigensian(memory_arena *arena, unit_story_keys *keys, u32 variant)
{
    u32 year = 1210, month = 5;
    if (variant == 1) { year = 1204; month = 12; }
    if (variant == 2) { year = 1221; month = 1; }
    sim_state *sim = unit_story_world(arena, keys, "medieval_west", year, month);
    if (variant == 3)
    {
        unit_story_faction(sim, keys, 2, 0);
        unit_story_officer(sim, keys, "raymond_vi", 2, OFFICER_STATUS_REGULAR_OFFICER, unit_story_homes[2], 1180, 500);
    }
    else unit_story_faction(sim, keys, 2, "raymond_vi");
    unit_story_member(sim, 2);
    unit_story_give(sim, 18, variant == 4 ? 0 : 2);
    return sim;
}

internal void
unit_west_albigensian_effects(test_counts *counts, sim_state *sim, unit_story_keys *keys)
{
    (void)keys;
    TEST_CHECK(counts, sim_find_city(sim, 18)->public_order == 40 && sim_find_officer(sim, UNIT_STORY_MEMBER_ID + 2)->loyalty == 70 &&
                       sim_find_city(sim, 18)->morale == 50,
               "medieval_west: the crusade lowers the public order of Toulouse and the loyalty of its officers by 10");
}

internal sim_state *
unit_west_france(memory_arena *arena, unit_story_keys *keys, u32 variant)
{
    u32 year = 1212, month = 1;
    if (variant == 2) { year = 1226; month = 1; }
    sim_state *sim = unit_story_world(arena, keys, "medieval_west", year, month);
    if (variant == 3)
    {
        unit_story_faction(sim, keys, 0, 0);
        unit_story_officer(sim, keys, "philip_ii", 0, OFFICER_STATUS_REGULAR_OFFICER, unit_story_homes[0], 1165, 500);
    }
    else unit_story_faction(sim, keys, 0, "philip_ii");
    unit_story_faction(sim, keys, 13, "innocent_iii");
    if (variant != 1) unit_story_started(sim, "albigensian_crusade");
    return sim;
}

internal void
unit_west_france_effects(test_counts *counts, sim_state *sim, unit_story_keys *keys)
{
    (void)keys;
    TEST_CHECK(counts, sim_friendliness(sim, 0, 13) == 10, "medieval_west: France gains 10 friendliness with the pope");
}

internal sim_state *
unit_west_navas(memory_arena *arena, unit_story_keys *keys, u32 variant)
{
    u32 year = 1212, month = 7;
    if (variant == 1) { year = 1207; month = 12; }
    if (variant == 2) { year = 1226; month = 1; }
    sim_state *sim = unit_story_world(arena, keys, "medieval_west", year, month);
    unit_story_faction(sim, keys, 7, 0);
    unit_story_faction(sim, keys, 4, "alfonso_viii");
    unit_story_faction(sim, keys, 3, "peter_ii");
    unit_story_give(sim, 3, variant == 3 ? 4 : 7);
    if (variant != 4) sim->diplomacy.relation[4][3] = sim->diplomacy.relation[3][4] = RELATION_ALLIANCE;
    if (variant == 5) sim->diplomacy.relation[4][7] = sim->diplomacy.relation[7][4] = RELATION_ALLIANCE;
    sim->content.force_church[7] = (u8)(variant == 6 ? CONTENT_CHURCH_LATIN : CONTENT_CHURCH_MUSLIM);
    return sim;
}

internal void
unit_west_navas_effects(test_counts *counts, sim_state *sim, unit_story_keys *keys)
{
    TEST_CHECK(counts, sim_find_city(sim, unit_story_homes[7])->morale == 35 && sim_find_city(sim, unit_story_homes[4])->morale == 50,
               "medieval_west: the defeat lowers the morale of the Almohad cities by 15 and no other");
    TEST_CHECK(counts, unit_story_key_officer(sim, keys, "alfonso_viii")->merit == 1500 && unit_story_key_officer(sim, keys, "peter_ii")->merit == 1500,
               "medieval_west: the two kings gain 500 merit");
}

internal sim_state *
unit_west_bouvines(memory_arena *arena, unit_story_keys *keys, u32 variant)
{
    u32 year = 1214, month = 7;
    if (variant == 1) { year = 1209; month = 12; }
    if (variant == 2) { year = 1226; month = 1; }
    sim_state *sim = unit_story_world(arena, keys, "medieval_west", year, month);
    if (variant == 3)
    {
        unit_story_faction(sim, keys, 0, 0);
        unit_story_officer(sim, keys, "philip_ii", 0, OFFICER_STATUS_REGULAR_OFFICER, unit_story_homes[0], 1165, 500);
    }
    else unit_story_faction(sim, keys, 0, "philip_ii");
    if (variant == 4) sim->diplomacy.relation[0][10] = sim->diplomacy.relation[10][0] = RELATION_ALLIANCE;
    if (variant == 5) sim->diplomacy.relation[0][1]  = sim->diplomacy.relation[1][0]  = RELATION_ALLIANCE;
    return sim;
}

internal void
unit_west_bouvines_effects(test_counts *counts, sim_state *sim, unit_story_keys *keys)
{
    TEST_CHECK(counts, unit_story_key_officer(sim, keys, "philip_ii")->merit == 1600 && sim_find_city(sim, unit_story_homes[0])->morale == 60,
               "medieval_west: Philip gains 600 merit and the cities of France gain 10 morale");
}

internal sim_state *
unit_west_interdict(memory_arena *arena, unit_story_keys *keys, u32 variant)
{
    sim_state *sim = unit_story_world(arena, keys, "medieval_west", variant == 1 ? 1201 : 1200, 1);
    if (variant == 2)
    {
        unit_story_faction(sim, keys, 0, 0);
        unit_story_officer(sim, keys, "philip_ii", 0, OFFICER_STATUS_REGULAR_OFFICER, unit_story_homes[0], 1165, 500);
    }
    else unit_story_faction(sim, keys, 0, "philip_ii");
    return sim;
}

internal void
unit_west_interdict_effects(test_counts *counts, sim_state *sim, unit_story_keys *keys)
{
    (void)keys;
    TEST_CHECK(counts, sim->authorities.interdict[0] == 8 && sim->authorities.interdict[1] == 0,
               "medieval_west: France starts in 1200 under an interdict of 8 months, and no other faction does");
}

internal void
unit_story_west_tests(test_counts *counts, memory_arena *arena)
{
    static const unit_story_case cases[] =
    {
        { "albigensian_crusade", 2, 4, unit_west_albigensian, unit_west_albigensian_effects },
        { "albigensian_france",  0, 3, unit_west_france,      unit_west_france_effects },
        { "las_navas_de_tolosa", 7, 6, unit_west_navas,       unit_west_navas_effects },
        { "bouvines",            0, 5, unit_west_bouvines,    unit_west_bouvines_effects },
        { "interdict_on_france", 0, 2, unit_west_interdict,   unit_west_interdict_effects },
    };
    unit_story_run(counts, arena, "medieval_west", cases, ArrayCount(cases));
}

internal void
unit_story_tests(test_counts *counts, memory_arena *arena)
{
    unit_story_east_tests(counts, arena);
    unit_story_west_tests(counts, arena);
}
