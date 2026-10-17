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
   live sim and copies the arrays into them. A file of another build (other
   struct sizes) or another pack does not load. Included by engine.cpp. */

#define ENGINE_SAVE_MAGIC   0x56535848u   /* "HXSV" */
#define ENGINE_SAVE_VERSION 1

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
    char pack[128];
    engine_save_slot slot;
};

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
    engine_save_get(cursor, header, sizeof(*header));
    if (!cursor->ok || header->magic != ENGINE_SAVE_MAGIC) return ENGINE_SAVE_BAD_FILE;
    if (header->version != ENGINE_SAVE_VERSION || header->sim_size != sizeof(sim_state) || header->officer_size != sizeof(officer_component) ||
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
    engine_save_get(&cursor, sim, sizeof(sim_state));
    /* The pointers and the hooks stay those of the live sim. */
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
