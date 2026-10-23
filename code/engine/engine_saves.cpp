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
#define ENGINE_SAVE_VERSION 2

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
    u32  layout;           /* version 2: engine_save_layout() of the build that wrote the file */
    u32  reserved;
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
    struct { u32 offset, size; } members[] =
    {
        ENGINE_LAYOUT_MEMBER(rng), ENGINE_LAYOUT_MEMBER(turn), ENGINE_LAYOUT_MEMBER(next_command_id), ENGINE_LAYOUT_MEMBER(entities),
        ENGINE_LAYOUT_MEMBER(map), ENGINE_LAYOUT_MEMBER(officers), ENGINE_LAYOUT_MEMBER(cities), ENGINE_LAYOUT_MEMBER(names),
        ENGINE_LAYOUT_MEMBER(has_names), ENGINE_LAYOUT_MEMBER(content), ENGINE_LAYOUT_MEMBER(has_content), ENGINE_LAYOUT_MEMBER(turns),
        ENGINE_LAYOUT_MEMBER(sites), ENGINE_LAYOUT_MEMBER(units), ENGINE_LAYOUT_MEMBER(unit_count), ENGINE_LAYOUT_MEMBER(scratch),
        ENGINE_LAYOUT_MEMBER(buildings), ENGINE_LAYOUT_MEMBER(building_count), ENGINE_LAYOUT_MEMBER(fire), ENGINE_LAYOUT_MEMBER(map_layers),
        ENGINE_LAYOUT_MEMBER(ruleset), ENGINE_LAYOUT_MEMBER(technique_points), ENGINE_LAYOUT_MEMBER(techniques), ENGINE_LAYOUT_MEMBER(research),
        ENGINE_LAYOUT_MEMBER(settings), ENGINE_LAYOUT_MEMBER(outcome), ENGINE_LAYOUT_MEMBER(officer_index), ENGINE_LAYOUT_MEMBER(city_index),
        ENGINE_LAYOUT_MEMBER(items), ENGINE_LAYOUT_MEMBER(diplomacy), ENGINE_LAYOUT_MEMBER(rank), ENGINE_LAYOUT_MEMBER(duel),
        ENGINE_LAYOUT_MEMBER(debate), ENGINE_LAYOUT_MEMBER(bases), ENGINE_LAYOUT_MEMBER(corps), ENGINE_LAYOUT_MEMBER(corps_ap),
        ENGINE_LAYOUT_MEMBER(adviser), ENGINE_LAYOUT_MEMBER(missions), ENGINE_LAYOUT_MEMBER(region_links), ENGINE_LAYOUT_MEMBER(region_links_built),
        ENGINE_LAYOUT_MEMBER(region_links_version), ENGINE_LAYOUT_MEMBER(emperor), ENGINE_LAYOUT_MEMBER(relics), ENGINE_LAYOUT_MEMBER(pending),
        ENGINE_LAYOUT_MEMBER(event_turn), ENGINE_LAYOUT_MEMBER(new_officer_count), ENGINE_LAYOUT_MEMBER(ability_uses), ENGINE_LAYOUT_MEMBER(ability_known),
        ENGINE_LAYOUT_MEMBER(ability_job), ENGINE_LAYOUT_MEMBER(scenario_number), ENGINE_LAYOUT_MEMBER(history_turn), ENGINE_LAYOUT_MEMBER(ai_records),
        { 0, (u32)sizeof(sim_state) }, { 1, (u32)sizeof(sim_unit) }, { 2, (u32)sizeof(sim_turn_state) }, { 3, (u32)sizeof(officer_component) },
        { 4, (u32)sizeof(city_component) }, { 5, (u32)sizeof(sim_event) }, { 6, (u32)sizeof(sim_building) }, { 7, (u32)sizeof(sim_base_state) },
    };
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
    snprintf(out, out_size, "%s\\slot_%02u.hxs", e->save_folder, slot + 1);
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

/* The arrays behind the pointers of sim_state, in file order. */
internal void
engine_save_arrays(engine_save_cursor *cursor, sim_state *sim, b32 write)
{
    u32 entities = sim->entities.capacity, cells = sim->map.width * sim->map.height;
    struct { void *data; u64 size; } parts[] =
    {
        { sim->entities.generation,   sizeof(u16) * entities },
        { sim->entities.alive,        sizeof(u8) * entities },
        { sim->entities.free_list,    sizeof(u32) * entities },
        { sim->officers.sparse,       sizeof(u32) * entities },
        { sim->officers.dense_owner,  sizeof(entity_id) * sim->officers.capacity },
        { sim->officers.data,         sizeof(officer_component) * sim->officers.capacity },
        { sim->cities.sparse,         sizeof(u32) * entities },
        { sim->cities.dense_owner,    sizeof(entity_id) * sim->cities.capacity },
        { sim->cities.data,           sizeof(city_component) * sim->cities.capacity },
        { sim->map.cost,              cells },
        { sim->map.kind,              cells },
        { sim->map.level,             cells },
        { sim->fire,                  sim->fire ? cells : 0 },
    };
    for (u32 index = 0; index < ArrayCount(parts); ++index)
    {
        if (write) engine_save_put(cursor, parts[index].data, parts[index].size);
        else       engine_save_get(cursor, parts[index].data, parts[index].size);
    }
}

internal b32
engine_has_game(engine *e)
{
    return e->sim->officers.count > 0 && e->sim->cities.count > 0;
}

internal engine_save_status
engine_api_save_game(void *opaque, u32 slot, const char *comment)
{
    engine *e = (engine *)opaque;
    if (slot >= ENGINE_SAVE_SLOTS) return ENGINE_SAVE_BAD_SLOT;
    if (!e->write_entire_file)     return ENGINE_SAVE_NO_WRITER;
    if (!engine_has_game(e))       return ENGINE_SAVE_NO_GAME;
    sim_state *sim = e->sim;
    engine_save_header header = engine_save_header_of(e);
    header.slot.used  = true;
    snprintf(header.slot.comment, sizeof(header.slot.comment), "%s", comment ? comment : "");
    header.slot.year  = sim->turns.date.year;
    header.slot.month = sim->turns.date.month;
    header.slot.day   = (u8)(1 + 10 * sim->turns.date.turn_of_month);
    u8 player = sim_whose_turn(sim);
    header.slot.player = player == SIM_NO_FORCE ? (u8)ENGINE_NO_FACTION : player;
    header.slot.turn  = sim->turn;

    u64 cells = (u64)sim->map.width * sim->map.height;
    u64 size  = sizeof(header) + sizeof(sim_state) + (sizeof(u16) + sizeof(u8) + 3 * sizeof(u32)) * sim->entities.capacity +
                (sizeof(entity_id) + sizeof(officer_component)) * sim->officers.capacity +
                (sizeof(entity_id) + sizeof(city_component)) * sim->cities.capacity + 4 * cells + sizeof(sim_event) * (u64)e->ledger_count;
    memory_arena *arena = e->transient;
    u64 mark = arena->used;
    if (arena->size - arena->used < size + 16) return ENGINE_SAVE_NO_MEMORY;
    u8 *buffer = (u8 *)arena_push_size(arena, size);
    engine_save_cursor cursor = { buffer, buffer + size, true };
    engine_save_put(&cursor, &header, sizeof(header));
    engine_save_put(&cursor, sim, sizeof(sim_state));
    engine_save_arrays(&cursor, sim, true);
    engine_save_put(&cursor, e->ledger, sizeof(sim_event) * (u64)e->ledger_count);
    char path[256];
    engine_save_path(e, slot, path, sizeof(path));
    b32 written = cursor.ok && e->write_entire_file(path, buffer, (u64)(cursor.at - buffer));
    arena->used = mark;
    if (!written) return ENGINE_SAVE_WRITE_FAILED;
    e->slots[slot] = header.slot;
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
        if (header->version != ENGINE_SAVE_VERSION || header->layout != engine_save_layout()) return ENGINE_SAVE_OTHER_VERSION;
    }
    if (header->sim_size != sizeof(sim_state) || header->officer_size != sizeof(officer_component) ||
        header->city_size != sizeof(city_component) || header->event_size != sizeof(sim_event))
    {
        return ENGINE_SAVE_OTHER_VERSION;
    }
    header->pack[sizeof(header->pack) - 1] = 0;
    if (strcmp(header->pack, e->content_folder) != 0) return ENGINE_SAVE_OTHER_PACK;
    return ENGINE_SAVE_OK;
}

internal engine_save_status
engine_api_load_game(void *opaque, u32 slot)
{
    engine *e = (engine *)opaque;
    sim_state *sim = e->sim;
    memory_arena *arena = e->transient;
    u64 mark = arena->used;
    engine_save_header header;
    engine_save_cursor cursor;
    engine_save_status status = engine_read_slot(e, slot, &header, &cursor);
    if (status == ENGINE_SAVE_OK &&
        (header.entity_capacity != sim->entities.capacity || header.officer_capacity != sim->officers.capacity ||
         header.city_capacity != sim->cities.capacity || header.map_width != sim->map.width || header.map_height != sim->map.height ||
         header.ledger_count > e->ledger_capacity))
    {
        status = ENGINE_SAVE_OTHER_VERSION;
    }
    if (status != ENGINE_SAVE_OK) { arena->used = mark; return status; }

    /* Check the length before the copy: a short file changes nothing. */
    u64 cells = (u64)sim->map.width * sim->map.height;
    u64 rest  = sizeof(sim_state) + (sizeof(u16) + sizeof(u8) + 3 * sizeof(u32)) * sim->entities.capacity +
                (sizeof(entity_id) + sizeof(officer_component)) * sim->officers.capacity +
                (sizeof(entity_id) + sizeof(city_component)) * sim->cities.capacity + (sim->fire ? 4 : 3) * cells +
                sizeof(sim_event) * (u64)header.ledger_count;
    if ((u64)(cursor.end - cursor.at) < rest) { arena->used = mark; return ENGINE_SAVE_BAD_FILE; }

    sim_state *live = PushStruct(arena, sim_state);
    *live = *sim;
    if (header.version == 1)
    {
        /* The sim_state of 2026-10-22: its content is four bytes shorter
           (one rule less), so every member after content sits four bytes
           earlier, and the tail padding took the difference. The copy
           inserts four bytes at the end of content. The inside of content
           does not matter: the load takes the content of the pack. */
        u8 *old   = PushArray(arena, sizeof(sim_state), u8);
        u8 *fresh = (u8 *)sim;
        u64 shift_at = offsetof(sim_state, content) + sizeof(sim_content) - sizeof(u32);
        engine_save_get(&cursor, old, sizeof(sim_state));
        memcpy(fresh, old, shift_at);
        memset(fresh + shift_at, 0, sizeof(u32));
        memcpy(fresh + shift_at + sizeof(u32), old + shift_at, sizeof(sim_state) - shift_at - sizeof(u32));
    }
    else
    {
        engine_save_get(&cursor, sim, sizeof(sim_state));
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
    engine_save_arrays(&cursor, sim, false);
    engine_save_get(&cursor, e->ledger, sizeof(sim_event) * (u64)header.ledger_count);
    e->ledger_count = header.ledger_count;
    arena->used = mark;
    return ENGINE_SAVE_OK;
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
        if (engine_read_slot(e, slot, &header, &cursor) == ENGINE_SAVE_OK && header.slot.used)
        {
            e->slots[slot] = header.slot;
            e->slots[slot].comment[ENGINE_SAVE_COMMENT_SIZE - 1] = 0;
        }
        e->transient->used = mark;
    }
}

internal void
engine_bind_saves_api(engine_api *api)
{
    api->save_game          = engine_api_save_game;
    api->load_game          = engine_api_load_game;
    api->save_slot          = engine_api_save_slot;
    api->refresh_save_slots = engine_api_refresh_save_slots;
}
