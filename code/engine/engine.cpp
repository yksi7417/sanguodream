/* The real backend engine: owns the simulation, applies commands to it and
   keeps the event ledger. Exposed to front ends only through engine_api. */

#include "engine/engine.h"

#define ENGINE_SCRATCH_SIZE Megabytes(4)
#define ENGINE_SINK_CAPACITY 4096

struct engine
{
    memory_arena *permanent;
    memory_arena *transient;
    memory_arena  scratch;         /* per-call working memory; reset before every command or search */
    sim_state    *sim;
    sim_event    *ledger;          /* every event the sim has emitted this session */
    u32           ledger_count;
    u32           ledger_capacity;
    platform_read_entire_file *read_entire_file;
};

internal engine_config
engine_default_config(void)
{
    engine_config c;
    c.seed             = 0x5EEDull;
    c.ledger_capacity  = 65536;
    c.map_width        = 40;    /* placeholder map until spec/formats/scenario-file.md */
    c.map_height       = 30;
    c.map_default_cost = 1;
    c.read_entire_file = 0;
    return c;
}

/* A deterministic placeholder landscape until spec-src/spec/formats gives
   the real map: a hash of the cell position picks a kind and a level. It
   does not touch the sim's PRNG, so it changes no game outcome. Water (kind
   0) is impassable; hills and mountains (kinds 3 and 4) cost more. */
internal void
engine_placeholder_terrain(hex_map *map)
{
    for (u32 index = 0; index < map->width * map->height; ++index)
    {
        u32 col = index % map->width, row = index / map->width;
        u32 h = col * 0x9E3779B1u ^ (row + 1) * 0x85EBCA77u;
        h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12;
        u32 roll = h % 100;
        u8 kind, level;
        if      (roll < 8)  { kind = 0; level = 0; }   /* water */
        else if (roll < 48) { kind = 1; level = 1; }   /* plain */
        else if (roll < 70) { kind = 2; level = 1; }   /* forest */
        else if (roll < 88) { kind = 3; level = 2; }   /* hill */
        else                { kind = 4; level = 3; }   /* mountain */
        map->kind[index]  = kind;
        map->level[index] = level;
        map->cost[index]  = kind == 0 ? HEX_IMPASSABLE : (kind == 3 ? 2 : (kind == 4 ? 3 : 1));
    }
    map->version++;
}

internal engine *
engine_create(memory_arena *permanent, memory_arena *transient, engine_config config)
{
    engine *e = PushStruct(permanent, engine);
    e->permanent = permanent;
    e->transient = transient;
    e->scratch   = arena_sub(transient, ENGINE_SCRATCH_SIZE);
    e->sim       = PushStruct(permanent, sim_state);
    sim_init(e->sim, permanent, config.seed);
    sim_init_map(e->sim, permanent, config.map_width, config.map_height, config.map_default_cost);
    engine_placeholder_terrain(&e->sim->map);
    e->ledger_capacity = config.ledger_capacity;
    e->ledger_count    = 0;
    e->ledger          = PushArray(permanent, config.ledger_capacity, sim_event);
    e->read_entire_file = config.read_entire_file;
    return e;
}

internal void
engine_ledger_append(engine *e, sim_event_sink *sink)
{
    for (u32 i = 0; i < sink->count; ++i)
    {
        if (e->ledger_count < e->ledger_capacity)
        {
            e->ledger[e->ledger_count++] = sink->events[i];
        }
    }
}

/* Runs one command against the sim and ledgers what it emitted. */
internal sim_command
engine_run_command(engine *e, sim_command_kind kind, u32 *events_emitted)
{
    arena_reset(&e->scratch);
    sim_event_sink sink = sim_sink_from_arena(&e->scratch, ENGINE_SINK_CAPACITY);
    sim_command command = sim_make_command(e->sim, kind);
    sim_apply(e->sim, command, &sink);
    engine_ledger_append(e, &sink);
    if (events_emitted) *events_emitted += sink.count;
    return command;
}

/* ---- engine_api implementation ------------------------------------------ */

internal u32
engine_api_turn(void *opaque)
{
    return ((engine *)opaque)->sim->turn;
}

internal u32
engine_api_entities_alive(void *opaque)
{
    return ((engine *)opaque)->sim->entities.alive_count;
}

internal u32
engine_api_entity_capacity(void *opaque)
{
    return ((engine *)opaque)->sim->entities.capacity;
}

internal u32
engine_api_ledger_count(void *opaque)
{
    return ((engine *)opaque)->ledger_count;
}

internal sim_event
engine_api_ledger_at(void *opaque, u32 index)
{
    engine *e = (engine *)opaque;
    sim_event none = {};
    return index < e->ledger_count ? e->ledger[index] : none;
}

internal engine_memory_stats
engine_api_memory(void *opaque)
{
    engine *e = (engine *)opaque;
    engine_memory_stats stats;
    stats.permanent_used = e->permanent->used;
    stats.permanent_size = e->permanent->size;
    stats.transient_used = e->transient->used;
    stats.transient_size = e->transient->size;
    return stats;
}

internal engine_map_size
engine_api_map_size(void *opaque)
{
    engine *e = (engine *)opaque;
    engine_map_size size;
    size.width  = e->sim->map.width;
    size.height = e->sim->map.height;
    return size;
}

internal u8
engine_api_cell_cost(void *opaque, s32 q, s32 r)
{
    return hex_map_cost(&((engine *)opaque)->sim->map, hex_make(q, r));
}

internal u32
engine_api_path_cost(void *opaque, s32 q0, s32 r0, s32 q1, s32 r1)
{
    engine *e = (engine *)opaque;
    arena_reset(&e->scratch);
    hex_path path = hex_find_path(&e->sim->map, hex_make(q0, r0), hex_make(q1, r1), &e->scratch);
    return path.total_cost;
}

internal engine_terrain_view
engine_api_terrain_view(void *opaque)
{
    hex_map *map = &((engine *)opaque)->sim->map;
    engine_terrain_view view;
    view.width   = map->width;
    view.height  = map->height;
    view.kind    = map->kind;
    view.level   = map->level;
    view.version = map->version;
    return view;
}

internal void
engine_api_set_cell_terrain(void *opaque, s32 q, s32 r, u8 kind, u8 level)
{
    hex_map_set_terrain(&((engine *)opaque)->sim->map, hex_make(q, r), kind, level);
}

internal u32
engine_api_officer_count(void *opaque)
{
    return ((engine *)opaque)->sim->officers.count;
}

internal engine_officer
engine_api_officer_at(void *opaque, u32 index)
{
    engine *e = (engine *)opaque;
    engine_officer result = {};
    result.officer_id = ENGINE_NO_OFFICER;
    if (index >= e->sim->officers.count) return result;
    officer_component *officer = e->sim->officers.data + index;
    result.officer_id = officer->officer_id;
    memcpy(result.family_name,   officer->family_name,   sizeof(result.family_name));
    memcpy(result.given_name,    officer->given_name,    sizeof(result.given_name));
    memcpy(result.courtesy_name, officer->courtesy_name, sizeof(result.courtesy_name));
    result.appearance_year          = officer->appearance_year;
    result.birth_year               = officer->birth_year;
    result.death_year               = officer->death_year;
    result.leadership               = officer->leadership;
    result.war                      = officer->war;
    result.intelligence             = officer->intelligence;
    result.politics                 = officer->politics;
    result.charm                    = officer->charm;
    result.force_id                 = officer->force_id;
    result.base_location            = officer->base_location;
    result.current_position         = officer->current_position;
    result.status                   = officer->status;
    result.merit                    = officer->merit;
    result.skill_number             = officer->skill_number;
    result.compatibility            = officer->compatibility;
    result.father                   = officer->father;
    result.spouse                   = officer->spouse;
    result.sworn_brother_group_head = officer->sworn_brother_group_head;
    return result;
}

internal u32
engine_api_city_count(void *opaque)
{
    return ((engine *)opaque)->sim->cities.count;
}

internal engine_city
engine_api_city_at(void *opaque, u32 index)
{
    engine *e = (engine *)opaque;
    engine_city result = {};
    result.city_id = ENGINE_NO_CITY;
    if (index >= e->sim->cities.count) return result;
    city_component *city = e->sim->cities.data + index;
    result.city_id               = city->city_id;
    result.owner_force_id        = city->owner_force_id;
    result.commanding_officer_id = city->commanding_officer_id;
    result.capacity_constant     = city->capacity_constant;
    result.gold                  = city->gold;
    result.troops                = city->troops;
    result.food                  = city->food;
    memcpy(result.equipment_stocks, city->equipment_stocks, sizeof(result.equipment_stocks));
    result.durability_maximum    = city->durability_maximum;
    result.public_order          = city->public_order;
    return result;
}

internal engine_city_name
engine_api_city_name_at(void *opaque, u32 index)
{
    engine *e = (engine *)opaque;
    engine_city_name result = {};
    result.city_id = ENGINE_NO_CITY;
    if (!e->sim->has_names || index >= ArrayCount(e->sim->names.cities)) return result;
    name_index_city *city = e->sim->names.cities + index;
    result.city_id = city->id;
    memcpy(result.name, city->name, sizeof(result.name));
    return result;
}

/* Resets the scratch arena and reads the whole file into it. If there is
   no file to decode, returns false with the status and detail in result. */
internal b32
engine_read_file_to_scratch(engine *e, const char *path, read_file_result *file, engine_load_result *result)
{
    *result = {};
    result->detail = "ok";
    if (!e->read_entire_file)
    {
        result->status = ENGINE_LOAD_NO_FILE_READER;
        result->detail = "no file reader";
        return false;
    }
    arena_reset(&e->scratch);
    *file = e->read_entire_file(path, &e->scratch);
    if (!file->contents)
    {
        result->status = ENGINE_LOAD_FILE_NOT_FOUND;
        result->detail = "file not found or larger than the scratch arena";
        return false;
    }
    return true;
}

/* Reads the file into the scratch arena, decodes it there, then hands the
   decoded save to the sim as one command. A file that does not load leaves
   the game and the ledger as they were. */
internal engine_load_result
engine_api_load_save(void *opaque, const char *path)
{
    engine *e = (engine *)opaque;
    engine_load_result result;
    read_file_result file;
    if (!engine_read_file_to_scratch(e, path, &file, &result)) return result;

    save_file save;
    save_load_result loaded = save_load(&e->scratch, (const u8 *)file.contents, file.size, &save);
    if (loaded.status != SAVE_LOAD_OK)
    {
        result.status = ENGINE_LOAD_BAD_SAVE;
        result.detail = loaded.status == SAVE_LOAD_INFLATE_FAILED ? inflate_status_name(loaded.inflate.status)
                                                                  : save_load_status_name(loaded.status);
        return result;
    }

    sim_event_sink sink = sim_sink_from_arena(&e->scratch, ENGINE_SINK_CAPACITY);
    sim_command command = sim_make_command(e->sim, SIM_COMMAND_LOAD_SAVE);
    if (!sim_load_save(e->sim, &save, command, &sink))
    {
        result.status = ENGINE_LOAD_BAD_SAVE;
        result.detail = "the save has more officers or cities than the game can hold";
        return result;
    }
    engine_ledger_append(e, &sink);
    result.status     = ENGINE_LOAD_OK;
    result.command_id = command.id;
    return result;
}

/* As load_save: decode in the scratch arena, then one command. A file that
   does not load leaves the names and the ledger as they were. */
internal engine_load_result
engine_api_load_name_index(void *opaque, const char *path)
{
    engine *e = (engine *)opaque;
    engine_load_result result;
    read_file_result file;
    if (!engine_read_file_to_scratch(e, path, &file, &result)) return result;

    name_index *names = PushStruct(&e->scratch, name_index);
    scenario_load_status loaded = name_index_load((const u8 *)file.contents, file.size, names);
    if (loaded != SCENARIO_LOAD_OK)
    {
        result.status = ENGINE_LOAD_BAD_NAME_INDEX;
        result.detail = scenario_load_status_name(loaded);
        return result;
    }

    sim_event_sink sink = sim_sink_from_arena(&e->scratch, ENGINE_SINK_CAPACITY);
    sim_command command = sim_make_command(e->sim, SIM_COMMAND_LOAD_NAME_INDEX);
    sim_load_name_index(e->sim, names, command, &sink);
    engine_ledger_append(e, &sink);
    result.status     = ENGINE_LOAD_OK;
    result.command_id = command.id;
    return result;
}

internal u32
engine_api_end_turn(void *opaque)
{
    sim_command command = engine_run_command((engine *)opaque, SIM_COMMAND_END_TURN, 0);
    return command.id;
}

internal u32
engine_api_advance_turns(void *opaque, u32 count)
{
    u32 emitted = 0;
    for (u32 i = 0; i < count; ++i)
    {
        engine_run_command((engine *)opaque, SIM_COMMAND_END_TURN, &emitted);
    }
    return emitted;
}

internal void
engine_api_ledger_clear(void *opaque)
{
    ((engine *)opaque)->ledger_count = 0;
}

internal void
engine_api_set_cell_cost(void *opaque, s32 q, s32 r, u8 cost)
{
    hex_map_set_cost(&((engine *)opaque)->sim->map, hex_make(q, r), cost);
}

internal engine_api
engine_make_api(engine *e)
{
    engine_api api;
    api.engine          = e;
    api.turn            = engine_api_turn;
    api.entities_alive  = engine_api_entities_alive;
    api.entity_capacity = engine_api_entity_capacity;
    api.ledger_count    = engine_api_ledger_count;
    api.ledger_at       = engine_api_ledger_at;
    api.memory          = engine_api_memory;
    api.map_size        = engine_api_map_size;
    api.cell_cost       = engine_api_cell_cost;
    api.path_cost       = engine_api_path_cost;
    api.officer_count   = engine_api_officer_count;
    api.officer_at      = engine_api_officer_at;
    api.city_count      = engine_api_city_count;
    api.city_at         = engine_api_city_at;
    api.city_name_at    = engine_api_city_name_at;
    api.end_turn        = engine_api_end_turn;
    api.advance_turns   = engine_api_advance_turns;
    api.ledger_clear    = engine_api_ledger_clear;
    api.set_cell_cost   = engine_api_set_cell_cost;
    api.terrain_view    = engine_api_terrain_view;
    api.set_cell_terrain = engine_api_set_cell_terrain;
    api.load_save       = engine_api_load_save;
    api.load_name_index = engine_api_load_name_index;
    return api;
}
