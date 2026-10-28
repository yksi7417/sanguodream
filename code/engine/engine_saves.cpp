/* M9 save and load (game-setup-and-modes.md, manual p.6: 32 slots with a
   comment, no autosave). The format is the engine's own, not the format
   of the original game (formats/save-file.md describes that one, and the
   engine only reads it). A slot file holds:
   1. a header: a magic, the format version, the sizes of the structs, the
      capacities, the map size, the content pack and the slot info;
   2. the bytes of sim_state;
   3. the arrays that sim_state points to: the entities, the officer and
      city tables, the map, the fire;
   4. the ledger.
   The pointers in the file are not used: a load keeps the pointers of the
   live sim and copies the arrays into them. The tables and the rules of
   the content pack (sim_state.content) come from the pack of this build,
   not from the file. A file of another build or another pack does not
   load: the header carries the layout of sim_state (engine_save_layout),
   because equal struct sizes do not prove equal layouts. The play report
   of 2026-10-23 found that: a new rule had moved every member after
   content by four bytes with the same total size, and the game showed the
   year 12. Version 1 files have the layout of 2026-10-22, and the load
   migrates them (see engine_api_load_game). Included by engine.cpp. */

#define ENGINE_SAVE_MAGIC   0x56535848u   /* "HXSV" */
#define ENGINE_SAVE_VERSION 3
#define ENGINE_SAVE_MEMBER_CAPACITY   128

/* The top-level members of sim_state, in the order of their stable ids.
   Version 3 writes this table after the header and the load copies each
   member by its id, so a new member or a member that grows at its end
   keeps the old saves. Add a new member at the END of this list, never
   in the middle, and add a new field at the end of its struct. A field
   in the middle of a member needs a migration (engine_api_load_game). */
#define ENGINE_SAVE_MEMBER_LIST(X) \
    X(rng) X(turn) X(next_command_id) X(entities) X(map) X(officers) X(cities) X(names) X(has_names) X(content) X(has_content) \
    X(turns) X(sites) X(units) X(unit_count) X(scratch) X(buildings) X(building_count) X(fire) X(map_layers) X(ruleset) \
    X(technique_points) X(techniques) X(research) X(settings) X(outcome) X(officer_index) X(city_index) X(items) X(diplomacy) \
    X(rank) X(duel) X(debate) X(bases) X(corps) X(corps_ap) X(adviser) X(missions) X(region_links) X(region_links_built) \
    X(region_links_version) X(emperor) X(relics) X(pending) X(event_turn) X(new_officer_count) X(ability_uses) X(ability_known) \
    X(ability_job) X(scenario_number) X(history_turn) X(ai_records) X(base_stocks)

/* One entry of the member table of a version 3 file. */
struct engine_save_member
{
    u32 id;
    u32 offset;
    u32 size;
};

internal u32
engine_save_members(engine_save_member *out)
{
    u32 count = 0;
#define ENGINE_SAVE_MEMBER_ENTRY(member) \
    out[count].id = count; out[count].offset = (u32)offsetof(sim_state, member); out[count].size = (u32)sizeof(((sim_state *)0)->member); ++count;
    ENGINE_SAVE_MEMBER_LIST(ENGINE_SAVE_MEMBER_ENTRY)
#undef ENGINE_SAVE_MEMBER_ENTRY
    return count;
}

/* The member tables of the files without a table, measured with offsetof
   against the headers of their builds (the probe of 2026-10-23). Version
   1 is the build of 2026-10-22. Version 2 is the builds of 2026-10-23 up
   to commit 9b03c31. Both have sim_size 511304. */
global_variable const engine_save_member engine_save_members_v1[] =
{
    { 0, 0, 8 }, { 1, 8, 4 }, { 2, 12, 4 }, { 3, 16, 48 }, { 4, 64, 40 }, { 5, 104, 32 },
    { 6, 136, 32 }, { 7, 168, 2564 }, { 8, 2732, 4 }, { 9, 2736, 94176 }, { 10, 96912, 4 }, { 11, 96916, 6164 },
    { 12, 103080, 32760 }, { 13, 135840, 31744 }, { 14, 167584, 4 }, { 15, 167592, 24 }, { 16, 167616, 7168 }, { 17, 174784, 4 },
    { 18, 174792, 8 }, { 19, 174800, 9120 }, { 20, 183920, 16 }, { 21, 183936, 1024 }, { 22, 184960, 2048 }, { 23, 187008, 2560 },
    { 24, 189568, 40 }, { 25, 189608, 1032 }, { 26, 190640, 2200 }, { 27, 192840, 42 }, { 28, 192884, 3200 }, { 29, 196084, 196608 },
    { 30, 392692, 256 }, { 31, 392948, 96 }, { 32, 393044, 272 }, { 33, 393316, 1080 }, { 34, 394396, 32768 }, { 35, 427164, 8192 },
    { 36, 435356, 512 }, { 37, 435868, 5504 }, { 38, 441372, 9216 }, { 39, 450588, 4 }, { 40, 450592, 4 }, { 41, 450596, 2 },
    { 42, 450600, 192 }, { 43, 450792, 20 }, { 44, 450812, 128 }, { 45, 450940, 4 }, { 46, 450944, 8192 }, { 47, 459136, 1024 },
    { 48, 460160, 1024 }, { 49, 461184, 1 }, { 50, 461188, 192 }, { 51, 461380, 49920 },
};
global_variable const engine_save_member engine_save_members_v2[] =
{
    { 0, 0, 8 }, { 1, 8, 4 }, { 2, 12, 4 }, { 3, 16, 48 }, { 4, 64, 40 }, { 5, 104, 32 },
    { 6, 136, 32 }, { 7, 168, 2564 }, { 8, 2732, 4 }, { 9, 2736, 94180 }, { 10, 96916, 4 }, { 11, 96920, 6164 },
    { 12, 103084, 32760 }, { 13, 135844, 31744 }, { 14, 167588, 4 }, { 15, 167592, 24 }, { 16, 167616, 7168 }, { 17, 174784, 4 },
    { 18, 174792, 8 }, { 19, 174800, 9120 }, { 20, 183920, 16 }, { 21, 183936, 1024 }, { 22, 184960, 2048 }, { 23, 187008, 2560 },
    { 24, 189568, 40 }, { 25, 189608, 1032 }, { 26, 190640, 2200 }, { 27, 192840, 42 }, { 28, 192884, 3200 }, { 29, 196084, 196608 },
    { 30, 392692, 256 }, { 31, 392948, 96 }, { 32, 393044, 272 }, { 33, 393316, 1080 }, { 34, 394396, 32768 }, { 35, 427164, 8192 },
    { 36, 435356, 512 }, { 37, 435868, 5504 }, { 38, 441372, 9216 }, { 39, 450588, 4 }, { 40, 450592, 4 }, { 41, 450596, 2 },
    { 42, 450600, 192 }, { 43, 450792, 20 }, { 44, 450812, 128 }, { 45, 450940, 4 }, { 46, 450944, 8192 }, { 47, 459136, 1024 },
    { 48, 460160, 1024 }, { 49, 461184, 1 }, { 50, 461188, 192 }, { 51, 461380, 49920 },
};

/* The header of version 1 (the builds until 2026-10-22). */
struct engine_save_header_v1
{
    u32  magic;
    u32  version;
    u32  sim_size;
    u32  officer_size;
    u32  city_size;
    u32  event_size;
    u32  entity_capacity;
    u32  officer_capacity;
    u32  city_capacity;
    u32  map_width;
    u32  map_height;
    u32  ledger_count;
    char pack[128];
    engine_save_slot slot;
};

struct engine_save_header
{
    u32  magic;
    u32  version;
    u32  sim_size;
    u32  officer_size;
    u32  city_size;
    u32  event_size;
    u32  entity_capacity;
    u32  officer_capacity;
    u32  city_capacity;
    u32  map_width;
    u32  map_height;
    u32  ledger_count;
    u32  layout;           /* version 2 and 3: engine_save_layout() of the build that wrote the file */
    u32  member_count;     /* version 3: the entries of the member table that follows the header; 0 in version 2 */
    char pack[128];
    engine_save_slot slot;
};

/* The layout of sim_state: a hash of the offset and the size of each
   member, and of the sizes of the structs in the arrays. A member that
   moves or grows changes it, also when sizeof(sim_state) stays the same.
   The inside of content does not count: the load takes the content of
   the pack. */
#define ENGINE_LAYOUT_MEMBER(member) { (u32)offsetof(sim_state, member), (u32)sizeof(((sim_state *)0)->member) }
internal u32
engine_save_layout(void)
{
#define ENGINE_LAYOUT_ENTRY(member) ENGINE_LAYOUT_MEMBER(member),
    struct { u32 offset, size; } members[] =
    {
        ENGINE_SAVE_MEMBER_LIST(ENGINE_LAYOUT_ENTRY)
        { 0, (u32)sizeof(sim_state) }, { 1, (u32)sizeof(sim_unit) }, { 2, (u32)sizeof(sim_turn_state) }, { 3, (u32)sizeof(officer_component) },
        { 4, (u32)sizeof(city_component) }, { 5, (u32)sizeof(sim_event) }, { 6, (u32)sizeof(sim_building) }, { 7, (u32)sizeof(sim_base_state) },
    };
#undef ENGINE_LAYOUT_ENTRY
    u32 hash = 2166136261u;   /* FNV-1a */
    for (u32 index = 0; index < ArrayCount(members); ++index)
    {
        u32 words[2] = { members[index].offset, members[index].size };
        u8 *bytes = (u8 *)words;
        for (u32 byte = 0; byte < sizeof(words); ++byte) hash = (hash ^ bytes[byte]) * 16777619u;
    }
    return hash;
}

/* A cursor over a byte buffer: each put or get moves it, and a short
   buffer clears ok. */
struct engine_save_cursor
{
    u8  *at;
    u8  *end;
    b32  ok;
};

internal void
engine_save_put(engine_save_cursor *cursor, const void *data, u64 size)
{
    if (!cursor->ok || (u64)(cursor->end - cursor->at) < size) { cursor->ok = false; return; }
    memcpy(cursor->at, data, size);
    cursor->at += size;
}

internal void
engine_save_get(engine_save_cursor *cursor, void *data, u64 size)
{
    if (!cursor->ok || (u64)(cursor->end - cursor->at) < size) { cursor->ok = false; return; }
    memcpy(data, cursor->at, size);
    cursor->at += size;
}

internal void
engine_save_path(engine *e, u32 slot, char *out, u32 out_size)
{
    snprintf(out, out_size, "%s/slot_%02u.hxs", e->save_folder, slot + 1);
}

/* ---- the .meta file of a slot (docs/MAPS.md) ---------------------------------
   slot_NN.meta stands next to slot_NN.hxs. It holds the facts that the save
   file does not hold, as key and value lines. The save file keeps its format.
   A save without the .meta file is a save of HEXDOM_CONTENT_PACK. */

#define ENGINE_META_PACK_KEY "pack\t"

internal void
engine_meta_path(engine *e, u32 slot, char *out, u32 out_size)
{
    snprintf(out, out_size, "%s/slot_%02u.meta", e->save_folder, slot + 1);
}

/* The name of the pack: the last folder of the content folder ("../data/medieval_europe" gives "medieval_europe"). */
internal const char *
engine_pack_name(const char *folder)
{
    const char *slash = strrchr(folder, '/');
    const char *backslash = strrchr(folder, '\\');
    if (backslash > slash) slash = backslash;
    return slash ? slash + 1 : folder;
}

/* Writes slot_NN.meta for a save of this engine. It always writes the file,
   so that a new save in the slot replaces the facts of an old save. */
internal b32
engine_write_slot_meta(engine *e, u32 slot)
{
    char path[256], text[256];
    engine_meta_path(e, slot, path, sizeof(path));
    s32 size = snprintf(text, sizeof(text), "key\tvalue\n# The facts of slot_%02u.hxs that the save file does not hold (docs/MAPS.md).\n"
                        ENGINE_META_PACK_KEY "%s\n", slot + 1, engine_pack_name(e->content_folder));
    return size > 0 && (u32)size < sizeof(text) && e->write_entire_file(path, text, (u64)size);
}

/* The pack of a slot: the pack line of slot_NN.meta, or HEXDOM_CONTENT_PACK without the file. */
internal void
engine_read_slot_pack(engine *e, u32 slot, char *out, u32 out_size)
{
    snprintf(out, out_size, "%s", HEXDOM_CONTENT_PACK);
    char path[256];
    engine_meta_path(e, slot, path, sizeof(path));
    u64 mark = e->transient->used;
    read_file_result file = e->read_entire_file ? e->read_entire_file(path, e->transient) : read_file_result{};
    const char *text = (const char *)file.contents;
    for (u64 at = 0; text && at < file.size; )
    {
        u64 end = at;
        while (end < file.size && text[end] != '\n') end++;
        u64 key = sizeof(ENGINE_META_PACK_KEY) - 1;
        if (end - at > key && memcmp(text + at, ENGINE_META_PACK_KEY, key) == 0)
        {
            u64 stop = end;
            while (stop > at + key && (text[stop - 1] == '\r' || text[stop - 1] == ' ')) stop--;
            snprintf(out, out_size, "%.*s", (int)(stop - at - key), text + at + key);
            break;
        }
        at = end + 1;
    }
    e->transient->used = mark;
}

internal engine_save_header
engine_save_header_of(engine *e)
{
    sim_state *sim = e->sim;
    engine_save_header header = {};
    header.magic            = ENGINE_SAVE_MAGIC;
    header.version          = ENGINE_SAVE_VERSION;
    header.sim_size         = (u32)sizeof(sim_state);
    header.officer_size     = (u32)sizeof(officer_component);
    header.city_size        = (u32)sizeof(city_component);
    header.event_size       = (u32)sizeof(sim_event);
    header.entity_capacity  = sim->entities.capacity;
    header.officer_capacity = sim->officers.capacity;
    header.city_capacity    = sim->cities.capacity;
    header.map_width        = sim->map.width;
    header.map_height       = sim->map.height;
    header.ledger_count     = e->ledger_count;
    header.layout           = engine_save_layout();
    snprintf(header.pack, sizeof(header.pack), "%s", e->content_folder);
    return header;
}

/* The records of a component table: each record of the file has file_size
   bytes, each record of the live table live_size. A load copies the
   common bytes and zeroes the rest of the live record, so a field at the
   end of a record keeps the old saves. */
internal void
engine_save_records(engine_save_cursor *cursor, void *data, u32 count, u32 live_size, u32 file_size, b32 write)
{
    u8 *at = (u8 *)data;
    u32 common = live_size < file_size ? live_size : file_size;
    for (u32 index = 0; index < count; ++index, at += live_size)
    {
        if (write) { engine_save_put(cursor, at, live_size); continue; }
        engine_save_get(cursor, at, common);
        if (common < live_size) memset(at + common, 0, live_size - common);
        if (common < file_size)
        {
            if (!cursor->ok || (u64)(cursor->end - cursor->at) < file_size - common) { cursor->ok = false; return; }
            cursor->at += file_size - common;
        }
    }
}

/* The arrays behind the pointers of sim_state, in file order. officer_size
   and city_size are the record sizes of the file (the live sizes on a
   write). */
internal void
engine_save_arrays(engine_save_cursor *cursor, sim_state *sim, b32 write, u32 officer_size, u32 city_size)
{
    u32 entities = sim->entities.capacity, cells = sim->map.width * sim->map.height;
    struct { void *data; u64 size; } parts[] =
    {
        { sim->entities.generation,   sizeof(u16) * entities },
        { sim->entities.alive,        sizeof(u8) * entities },
        { sim->entities.free_list,    sizeof(u32) * entities },
        { sim->officers.sparse,       sizeof(u32) * entities },
        { sim->officers.dense_owner,  sizeof(entity_id) * sim->officers.capacity },
    };
    for (u32 index = 0; index < ArrayCount(parts); ++index)
    {
        if (write) engine_save_put(cursor, parts[index].data, parts[index].size);
        else       engine_save_get(cursor, parts[index].data, parts[index].size);
    }
    engine_save_records(cursor, sim->officers.data, sim->officers.capacity, (u32)sizeof(officer_component), officer_size, write);
    struct { void *data; u64 size; } middle[] =
    {
        { sim->cities.sparse,         sizeof(u32) * entities },
        { sim->cities.dense_owner,    sizeof(entity_id) * sim->cities.capacity },
    };
    for (u32 index = 0; index < ArrayCount(middle); ++index)
    {
        if (write) engine_save_put(cursor, middle[index].data, middle[index].size);
        else       engine_save_get(cursor, middle[index].data, middle[index].size);
    }
    engine_save_records(cursor, sim->cities.data, sim->cities.capacity, (u32)sizeof(city_component), city_size, write);
    struct { void *data; u64 size; } tail[] =
    {
        { sim->map.cost,              cells },
        { sim->map.kind,              cells },
        { sim->map.level,             cells },
        { sim->fire,                  sim->fire ? cells : 0 },
    };
    for (u32 index = 0; index < ArrayCount(tail); ++index)
    {
        if (write) engine_save_put(cursor, tail[index].data, tail[index].size);
        else       engine_save_get(cursor, tail[index].data, tail[index].size);
    }
}

internal b32
engine_has_game(engine *e)
{
    return e->sim->officers.count > 0 && e->sim->cities.count > 0;
}

/* The bytes of an image of the game with this many ledger events: the
   header, the member table, sim_state, the arrays and the ledger. A slot
   file is one image. The replay of a game keeps its start state as one
   image (engine_game_replay.cpp). */
internal u64
engine_save_image_size(engine *e, u32 ledger_count)
{
    sim_state *sim = e->sim;
    engine_save_member members[ENGINE_SAVE_MEMBER_CAPACITY];
    u32 member_count = engine_save_members(members);
    u64 cells = (u64)sim->map.width * sim->map.height;
    return sizeof(engine_save_header) + sizeof(engine_save_member) * member_count + sizeof(sim_state) +
           (sizeof(u16) + sizeof(u8) + 3 * sizeof(u32)) * sim->entities.capacity +
           (sizeof(entity_id) + sizeof(officer_component)) * sim->officers.capacity +
           (sizeof(entity_id) + sizeof(city_component)) * sim->cities.capacity + 4 * cells + sizeof(sim_event) * (u64)ledger_count;
}

/* Writes the image of the live game with this slot info. An image
   without the ledger has a ledger count of 0. */
internal void
engine_save_write_image(engine *e, engine_save_cursor *cursor, engine_save_slot slot, b32 with_ledger = true)
{
    engine_save_header header = engine_save_header_of(e);
    header.slot = slot;
    if (!with_ledger) header.ledger_count = 0;
    engine_save_member members[ENGINE_SAVE_MEMBER_CAPACITY];
    header.member_count = engine_save_members(members);
    engine_save_put(cursor, &header, sizeof(header));
    engine_save_put(cursor, members, sizeof(engine_save_member) * header.member_count);
    engine_save_put(cursor, e->sim, sizeof(sim_state));
    engine_save_arrays(cursor, e->sim, true, (u32)sizeof(officer_component), (u32)sizeof(city_component));
    engine_save_put(cursor, e->ledger, sizeof(sim_event) * (u64)header.ledger_count);
}

internal void engine_game_replay_return_live(engine *e);   /* engine_game_replay.cpp */
internal u64  engine_game_replay_saved_size(engine *e);
internal void engine_game_replay_write(engine *e, engine_save_cursor *cursor);
internal void engine_game_replay_read(engine *e, engine_save_cursor *cursor);

/* A save while a replay view is open saves the live game: the view
   closes first. The record of the replay follows the image. */
internal engine_save_status
engine_api_save_game(void *opaque, u32 slot, const char *comment)
{
    engine *e = (engine *)opaque;
    if (slot >= ENGINE_SAVE_SLOTS) return ENGINE_SAVE_BAD_SLOT;
    if (!e->write_entire_file)     return ENGINE_SAVE_NO_WRITER;
    engine_game_replay_return_live(e);
    if (!engine_has_game(e))       return ENGINE_SAVE_NO_GAME;
    sim_state *sim = e->sim;
    engine_save_slot info = {};
    info.used  = true;
    snprintf(info.comment, sizeof(info.comment), "%s", comment ? comment : "");
    info.year  = sim->turns.date.year;
    info.month = sim->turns.date.month;
    info.day   = (u8)(1 + 10 * sim->turns.date.turn_of_month);
    u8 player = sim_whose_turn(sim);
    info.player = player == SIM_NO_FORCE ? (u8)ENGINE_NO_FACTION : player;
    info.turn  = sim->turn;

    u64 size = engine_save_image_size(e, e->ledger_count) + engine_game_replay_saved_size(e);
    memory_arena *arena = e->transient;
    u64 mark = arena->used;
    if (arena->size - arena->used < size + 16) return ENGINE_SAVE_NO_MEMORY;
    u8 *buffer = (u8 *)arena_push_size(arena, size);
    engine_save_cursor cursor = { buffer, buffer + size, true };
    engine_save_write_image(e, &cursor, info);
    engine_game_replay_write(e, &cursor);
    char path[256];
    engine_save_path(e, slot, path, sizeof(path));
    b32 written = cursor.ok && e->write_entire_file(path, buffer, (u64)(cursor.at - buffer));
    arena->used = mark;
    if (!written || !engine_write_slot_meta(e, slot)) return ENGINE_SAVE_WRITE_FAILED;
    e->slots[slot] = info;
    e->slot_packs[slot] = {};
    return ENGINE_SAVE_OK;
}

/* True if two paths name the same folder. A '\' and a '/' are the same
   character, because the saves of the builds before 2026-10-27 have
   "..\data\placeholder" and the later builds have "../data/placeholder". */
internal b32
engine_same_path(const char *a, const char *b)
{
    for (;; ++a, ++b)
    {
        b32 a_separator = *a == '\\' || *a == '/';
        b32 b_separator = *b == '\\' || *b == '/';
        if (a_separator != b_separator) return false;
        if (!a_separator && *a != *b) return false;
        if (!*a) return true;
    }
}

/* Reads and checks the header of an image at the cursor: a slot file, or
   the start state of a replay. */
internal engine_save_status
engine_save_read_header(engine *e, engine_save_header *header, engine_save_cursor *cursor)
{
    u32 magic_version[2];
    engine_save_get(cursor, magic_version, sizeof(magic_version));
    if (!cursor->ok || magic_version[0] != ENGINE_SAVE_MAGIC) return ENGINE_SAVE_BAD_FILE;
    cursor->at -= sizeof(magic_version);
    *header = {};
    if (magic_version[1] == 1)
    {
        /* The header of 2026-10-22, without the layout. The file holds the
           sim_state of that day: the same size as today, with content four
           bytes shorter. engine_api_load_game inserts the four bytes. */
        engine_save_header_v1 old;
        engine_save_get(cursor, &old, sizeof(old));
        if (!cursor->ok) return ENGINE_SAVE_BAD_FILE;
        header->magic            = old.magic;
        header->version          = 1;
        header->sim_size         = old.sim_size;
        header->officer_size     = old.officer_size;
        header->city_size        = old.city_size;
        header->event_size       = old.event_size;
        header->entity_capacity  = old.entity_capacity;
        header->officer_capacity = old.officer_capacity;
        header->city_capacity    = old.city_capacity;
        header->map_width        = old.map_width;
        header->map_height       = old.map_height;
        header->ledger_count     = old.ledger_count;
        header->layout           = 0;
        memcpy(header->pack, old.pack, sizeof(header->pack));
        header->slot             = old.slot;
    }
    else
    {
        engine_save_get(cursor, header, sizeof(*header));
        if (!cursor->ok) return ENGINE_SAVE_BAD_FILE;
        /* Version 2 needs the same layout. Version 3 carries a member table
           and loads with another layout (the cursor stands on the table). */
        if (header->version == 2 && header->layout != engine_save_layout()) return ENGINE_SAVE_OTHER_VERSION;
        if (header->version != 2 && header->version != ENGINE_SAVE_VERSION) return ENGINE_SAVE_OTHER_VERSION;
        if (header->version == 3 && header->member_count > ENGINE_SAVE_MEMBER_CAPACITY) return ENGINE_SAVE_BAD_FILE;
    }
    /* Versions 1 and 2 have the member tables of their builds in this file
       (engine_save_members_v1, _v2), so their sim_size must be the one of
       those builds. The records of the officers and the cities copy record
       by record (engine_save_records), so a record that grew at its end
       still loads. */
    if (header->version < 3 && header->sim_size != 511304) return ENGINE_SAVE_OTHER_VERSION;
    header->pack[sizeof(header->pack) - 1] = 0;
    if (!engine_same_path(header->pack, e->content_folder)) return ENGINE_SAVE_OTHER_PACK;
    return ENGINE_SAVE_OK;
}

/* Reads and checks the header of a slot file. The file stays in the
   transient arena above mark; the caller resets it. */
internal engine_save_status
engine_read_slot(engine *e, u32 slot, engine_save_header *header, engine_save_cursor *cursor)
{
    if (slot >= ENGINE_SAVE_SLOTS) return ENGINE_SAVE_BAD_SLOT;
    if (!e->read_entire_file)      return ENGINE_SAVE_NO_READER;
    char path[256];
    engine_save_path(e, slot, path, sizeof(path));
    read_file_result file = e->read_entire_file(path, e->transient);
    if (!file.contents) return ENGINE_SAVE_NO_FILE;
    *cursor = { (u8 *)file.contents, (u8 *)file.contents + file.size, true };
    return engine_save_read_header(e, header, cursor);
}

/* Loads the image after its header into the live sim and the ledger. The
   cursor then stands after the ledger. The copies of the load stay in the
   transient arena; the caller resets it. */
internal engine_save_status
engine_save_load_image(engine *e, const engine_save_header *in, engine_save_cursor *cursor)
{
    sim_state *sim = e->sim;
    memory_arena *arena = e->transient;
    engine_save_header header = *in;
    if (header.entity_capacity != sim->entities.capacity || header.officer_capacity != sim->officers.capacity ||
        header.city_capacity != sim->cities.capacity || header.map_width != sim->map.width || header.map_height != sim->map.height ||
        header.ledger_count > e->ledger_capacity)
    {
        return ENGINE_SAVE_OTHER_VERSION;
    }

    /* Version 3: the member table stands after the header. */
    engine_save_member members[ENGINE_SAVE_MEMBER_CAPACITY];
    if (header.version == 3) engine_save_get(cursor, members, sizeof(engine_save_member) * header.member_count);

    /* Check the length before the copy: a short file changes nothing. */
    u64 cells = (u64)sim->map.width * sim->map.height;
    u64 rest  = (u64)header.sim_size + (sizeof(u16) + sizeof(u8) + 3 * sizeof(u32)) * sim->entities.capacity +
                (sizeof(entity_id) + header.officer_size) * sim->officers.capacity +
                (sizeof(entity_id) + header.city_size) * sim->cities.capacity + (sim->fire ? 4 : 3) * cells +
                (u64)header.event_size * (u64)header.ledger_count;
    if (!cursor->ok || (u64)(cursor->end - cursor->at) < rest) return ENGINE_SAVE_BAD_FILE;

    sim_state *live = PushStruct(arena, sim_state);
    *live = *sim;
    /* Member by member, by the stable id: a member that the file lacks
       stays zero, a member that grew keeps its old bytes at its start. The
       table of a version 3 file stands in the file; the tables of the
       versions 1 and 2 are the measured ones above. The inside of content
       does not matter: the load takes the content of the pack.
       save_compat_tests.cpp checks the members after content against a
       fresh load of the sample. */
    const engine_save_member *table = members;
    u32 table_count = header.member_count;
    if (header.version == 1) { table = engine_save_members_v1; table_count = ArrayCount(engine_save_members_v1); }
    if (header.version == 2) { table = engine_save_members_v2; table_count = ArrayCount(engine_save_members_v2); }
    u8 *old = PushArray(arena, header.sim_size, u8);
    engine_save_get(cursor, old, header.sim_size);
    engine_save_member fresh[ENGINE_SAVE_MEMBER_CAPACITY];
    u32 fresh_count = engine_save_members(fresh);
    memset(sim, 0, sizeof(sim_state));
    for (u32 index = 0; index < fresh_count; ++index)
    {
        for (u32 file_index = 0; file_index < table_count; ++file_index)
        {
            const engine_save_member *from = table + file_index;
            if (from->id != fresh[index].id) continue;
            u32 common = from->size < fresh[index].size ? from->size : fresh[index].size;
            if ((u64)from->offset + common <= header.sim_size) memcpy((u8 *)sim + fresh[index].offset, old + from->offset, common);
            break;
        }
    }
    /* The pointers and the hooks stay those of the live sim. */
    sim->content              = live->content;   /* the tables and the rules of the pack of this build */
    sim->entities.generation  = live->entities.generation;
    sim->entities.alive       = live->entities.alive;
    sim->entities.free_list   = live->entities.free_list;
    sim->officers.sparse      = live->officers.sparse;
    sim->officers.dense_owner = live->officers.dense_owner;
    sim->officers.data        = live->officers.data;
    sim->cities.sparse        = live->cities.sparse;
    sim->cities.dense_owner   = live->cities.dense_owner;
    sim->cities.data          = live->cities.data;
    sim->map.cost             = live->map.cost;
    sim->map.kind             = live->map.kind;
    sim->map.level            = live->map.level;
    sim->map.version          = live->map.version + 1;   /* the renderer builds the map again */
    sim->fire                 = live->fire;
    sim->map_layers           = live->map_layers;   /* M10: the map data comes from the pack, not from the save */
    sim->scratch              = live->scratch;
    sim->ruleset              = live->ruleset;
    engine_save_arrays(cursor, sim, false, header.officer_size, header.city_size);
    if (header.event_size == sizeof(sim_event))
    {
        engine_save_get(cursor, e->ledger, sizeof(sim_event) * (u64)header.ledger_count);
        e->ledger_count = header.ledger_count;
    }
    else
    {
        e->ledger_count = 0;   /* the events of another build do not read; the game goes on without its history */
        cursor->at += (u64)header.event_size * (u64)header.ledger_count;
    }
    return ENGINE_SAVE_OK;
}

/* A file that does not load changes nothing. A file that loads gives the
   game, its ledger, and the record of its replay if the file has one. */
internal engine_save_status
engine_api_load_game(void *opaque, u32 slot)
{
    engine *e = (engine *)opaque;
    memory_arena *arena = e->transient;
    u64 mark = arena->used;
    engine_save_header header;
    engine_save_cursor cursor;
    engine_save_status status = engine_read_slot(e, slot, &header, &cursor);
    if (status == ENGINE_SAVE_OK) status = engine_save_load_image(e, &header, &cursor);
    if (status == ENGINE_SAVE_OK) engine_game_replay_read(e, &cursor);
    arena->used = mark;
    return status;
}

internal engine_save_slot
engine_api_save_slot(void *opaque, u32 slot)
{
    engine *e = (engine *)opaque;
    engine_save_slot empty = {};
    empty.player = ENGINE_NO_FACTION;
    return slot < ENGINE_SAVE_SLOTS ? e->slots[slot] : empty;
}

internal void
engine_api_refresh_save_slots(void *opaque)
{
    engine *e = (engine *)opaque;
    for (u32 slot = 0; slot < ENGINE_SAVE_SLOTS; ++slot)
    {
        u64 mark = e->transient->used;
        engine_save_header header;
        engine_save_cursor cursor;
        e->slots[slot] = {};
        e->slots[slot].player = ENGINE_NO_FACTION;
        engine_save_status status = engine_read_slot(e, slot, &header, &cursor);
        char pack[CONTENT_KEY_SIZE] = "";
        /* A save of another pack shows with its pack, if its .meta file names a pack that is not this one. */
        if (status == ENGINE_SAVE_OTHER_PACK) engine_read_slot_pack(e, slot, pack, sizeof(pack));
        b32 other_pack = status == ENGINE_SAVE_OTHER_PACK && strcmp(pack, engine_pack_name(e->content_folder)) != 0;
        e->slot_packs[slot] = {};
        if ((status == ENGINE_SAVE_OK || other_pack) && header.slot.used)
        {
            e->slots[slot] = header.slot;
            e->slots[slot].comment[ENGINE_SAVE_COMMENT_SIZE - 1] = 0;
            if (other_pack) snprintf(e->slot_packs[slot].pack, sizeof(e->slot_packs[slot].pack), "%s", pack);
        }
        e->transient->used = mark;
    }
}

internal engine_slot_pack
engine_api_slot_pack(void *opaque, u32 slot)
{
    engine *e = (engine *)opaque;
    engine_slot_pack none = {};
    return slot < ENGINE_SAVE_SLOTS ? e->slot_packs[slot] : none;
}

internal void
engine_bind_saves_api(engine_api *api)
{
    api->slot_pack          = engine_api_slot_pack;
    api->save_game          = engine_api_save_game;
    api->load_game          = engine_api_load_game;
    api->save_slot          = engine_api_save_slot;
    api->refresh_save_slots = engine_api_refresh_save_slots;
}
