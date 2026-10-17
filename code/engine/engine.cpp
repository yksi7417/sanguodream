/* The real backend engine: owns the simulation, applies commands to it and
   keeps the event ledger. Exposed to front ends only through engine_api. */

#include <stdio.h>   /* snprintf for the paths of the content pack */
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
    u32          *filter_indices;  /* the answer of ledger_filter; ledger_capacity entries */
    u8           *tiles;           /* the atlas colour of each cell; see terrain_view */
    u32           tiles_version;   /* the map version of tiles */
    hex_coord    *reach_cells;     /* the answer of reachable; one entry for each cell */
    u32          *reach_costs;
    platform_read_entire_file *read_entire_file;
    engine_content_status      content;   /* what engine_create loaded from the content pack */
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
    c.content_folder   = HEXDOM_DATA_ROOT "\\" HEXDOM_CONTENT_PACK;
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

/* Reads one table of the content pack into the scratch arena and parses it.
   The text gets a NUL at its end, because the parser needs one. */
internal b32
engine_read_table(engine *e, const char *folder, const char *file_name, tsv_table *table)
{
    if (!e->read_entire_file || !folder) return false;
    char path[260];
    snprintf(path, sizeof(path), "%s\\%s", folder, file_name);
    read_file_result file = e->read_entire_file(path, &e->scratch);
    if (!file.contents) return false;
    char *text = (char *)arena_push_size(&e->scratch, file.size + 1);
    memcpy(text, file.contents, (size_t)file.size);
    text[file.size] = 0;
    return tsv_parse(text, &e->scratch, table);
}

/* Loads facilities.tsv, cities.tsv, rules.tsv, techniques.tsv,
   weapons.tsv and disasters.tsv. The weapons name the techniques, so the
   techniques load first. If one table does not load, the sim has no
   content and the commands that need it refuse with SIM_REFUSAL_NO_CONTENT. */
internal void
engine_load_content(engine *e, const char *folder)
{
    arena_reset(&e->scratch);
    sim_content *content = PushStruct(&e->scratch, sim_content);
    engine_content_status *status = &e->content;
    *status = {};
    status->detail = "ok";
    tsv_table facilities = {}, cities = {}, rules = {}, weapons = {}, techniques = {}, disasters = {}, terrain = {}, unit_types = {};
    tsv_table tactics = {}, plots = {}, buildings = {}, criticals = {};
    u32 bad_row = 0;
    if (!engine_read_table(e, folder, "facilities.tsv", &facilities))      status->detail = "facilities.tsv not found";
    else if (!engine_read_table(e, folder, "cities.tsv", &cities))         status->detail = "cities.tsv not found";
    else if (!engine_read_table(e, folder, "rules.tsv", &rules))           status->detail = "rules.tsv not found";
    else if (!engine_read_table(e, folder, "weapons.tsv", &weapons))       status->detail = "weapons.tsv not found";
    else if (!engine_read_table(e, folder, "techniques.tsv", &techniques)) status->detail = "techniques.tsv not found";
    else if (!engine_read_table(e, folder, "disasters.tsv", &disasters))   status->detail = "disasters.tsv not found";
    else if (!engine_read_table(e, folder, "terrain.tsv", &terrain))       status->detail = "terrain.tsv not found";
    else if (!engine_read_table(e, folder, "unit_types.tsv", &unit_types)) status->detail = "unit_types.tsv not found";
    else if (!engine_read_table(e, folder, "tactics.tsv", &tactics))       status->detail = "tactics.tsv not found";
    else if (!engine_read_table(e, folder, "plots.tsv", &plots))           status->detail = "plots.tsv not found";
    else if (!engine_read_table(e, folder, "buildings.tsv", &buildings))   status->detail = "buildings.tsv not found";
    else if (!engine_read_table(e, folder, "critical_skills.tsv", &criticals)) status->detail = "critical_skills.tsv not found";
    else if (!content_load_facilities(content, &facilities, &bad_row))    status->detail = "facilities.tsv has a bad row";
    else if (!content_load_cities(content, &cities, &bad_row))            status->detail = "cities.tsv has a bad row";
    else if (!content_load_rules(content, &rules, &bad_row))              status->detail = "rules.tsv has a bad row";
    else if (!content_load_techniques(content, &techniques, &bad_row))    status->detail = "techniques.tsv has a bad row";
    else if (!content_load_weapons(content, &weapons, &bad_row))          status->detail = "weapons.tsv has a bad row";
    else if (!content_load_disasters(content, &disasters, &bad_row))      status->detail = "disasters.tsv has a bad row";
    else if (!content_load_terrain(content, &terrain, &bad_row))          status->detail = "terrain.tsv has a bad row";
    else if (!content_load_unit_types(content, &unit_types, &bad_row))    status->detail = "unit_types.tsv has a bad row";
    else if (!content_load_tactics(content, &tactics, &bad_row))          status->detail = "tactics.tsv has a bad row";
    else if (!content_load_plots(content, &plots, &bad_row))              status->detail = "plots.tsv has a bad row";
    else if (!content_load_buildings(content, &buildings, &bad_row))      status->detail = "buildings.tsv has a bad row";
    else if (!content_load_criticals(content, &criticals, &bad_row))      status->detail = "critical_skills.tsv has a bad row";
    else
    {
        status->loaded          = true;
        status->facility_count  = content->facility_count;
        status->city_count      = content->city_count;
        status->rule_count      = content->rules.loaded_count;
        status->weapon_count    = content->weapon_count;
        status->technique_count = content->technique_count;
        status->disaster_count  = content->disaster_count;
        status->terrain_count   = content->terrain_count;
        status->unit_type_count = content->unit_type_count;
        status->tactic_count    = content->tactic_count;
        status->plot_count      = content->plot_count;
        status->building_count  = content->building_count;
        status->critical_count  = content->critical_count;
        sim_set_content(e->sim, content);
    }
    arena_reset(&e->scratch);
}

internal void engine_content_terrain(engine *e);   /* engine_march.cpp */
internal void engine_place_natural_traps(engine *e);   /* engine_combat.cpp */

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
    e->filter_indices  = PushArray(permanent, config.ledger_capacity, u32);
    e->read_entire_file = config.read_entire_file;
    u32 cells = config.map_width * config.map_height;
    e->tiles         = PushArray(permanent, cells ? cells : 1, u8);
    e->tiles_version = 0;
    e->reach_cells   = PushArray(permanent, cells ? cells : 1, hex_coord);
    e->reach_costs   = PushArray(permanent, cells ? cells : 1, u32);
    engine_load_content(e, config.content_folder);
    if (e->content.loaded) engine_content_terrain(e);   /* engine_march.cpp: the placeholder map from terrain.tsv */
    if (e->content.loaded) engine_place_natural_traps(e);
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
internal void
engine_run(engine *e, sim_command command, u32 *events_emitted)
{
    arena_reset(&e->scratch);
    sim_event_sink sink = sim_sink_from_arena(&e->scratch, ENGINE_SINK_CAPACITY);
    sim_apply(e->sim, command, &sink);
    engine_ledger_append(e, &sink);
    if (events_emitted) *events_emitted += sink.count;
}

internal sim_command
engine_run_command(engine *e, sim_command_kind kind, u32 *events_emitted)
{
    sim_command command = sim_make_command(e->sim, kind);
    engine_run(e, command, events_emitted);
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

/* The tiles follow the kinds through the terrain table. Without the table
   the kind is the tile, as in M0. */
internal engine_terrain_view
engine_api_terrain_view(void *opaque)
{
    engine *e = (engine *)opaque;
    hex_map *map = &e->sim->map;
    if (e->tiles_version != map->version)
    {
        sim_content *content = &e->sim->content;
        for (u32 index = 0; index < map->width * map->height; ++index)
        {
            u8 kind = map->kind[index];
            e->tiles[index] = kind < content->terrain_count ? content->terrains[kind].tile : kind;
        }
        e->tiles_version = map->version;
    }
    engine_terrain_view view;
    view.width   = map->width;
    view.height  = map->height;
    view.kind    = map->kind;
    view.level   = map->level;
    view.version = map->version;
    view.tile    = e->tiles;
    return view;
}

/* The editor sets the kind and the height of a cell. With the terrain table
   the cost of the cell follows the kind (the foot cost). */
internal void
engine_api_set_cell_terrain(void *opaque, s32 q, s32 r, u8 kind, u8 level)
{
    sim_state *sim = ((engine *)opaque)->sim;
    hex_map_set_terrain(&sim->map, hex_make(q, r), kind, level);
    if (kind < sim->content.terrain_count) hex_map_set_cost(&sim->map, hex_make(q, r), sim->content.terrains[kind].cost[MOVEMENT_FOOT]);
}

internal u32
engine_api_officer_count(void *opaque)
{
    return ((engine *)opaque)->sim->officers.count;
}

internal engine_officer
engine_officer_view(engine *e, officer_component *officer)
{
    engine_officer result = {};
    result.officer_id = ENGINE_NO_OFFICER;
    if (!officer) return result;
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
    result.job_city                 = officer->job_city;
    result.acted                    = officer->acted_turn == e->sim->turn + 1;
    return result;
}

internal engine_officer
engine_api_officer_at(void *opaque, u32 index)
{
    engine *e = (engine *)opaque;
    officer_component *officer = index < e->sim->officers.count ? e->sim->officers.data + index : 0;
    return engine_officer_view(e, officer);
}

internal engine_officer
engine_api_officer_by_id(void *opaque, u16 officer_id)
{
    engine *e = (engine *)opaque;
    return engine_officer_view(e, sim_find_officer(e->sim, officer_id));
}

internal u16
engine_api_force_ruler(void *opaque, u8 force_id)
{
    return sim_force_ruler(((engine *)opaque)->sim, force_id);
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
    result.morale                = city->morale;
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

/* A city id gives the city entry; 42..86 give the gate and port entries. */
internal engine_location_name
engine_api_location_name_at(void *opaque, u16 location)
{
    engine *e = (engine *)opaque;
    engine_location_name result = {};
    result.location = ENGINE_NO_LOCATION;
    if (!e->sim->has_names) return result;
    if (location < ArrayCount(e->sim->names.cities))
    {
        name_index_city *city = e->sim->names.cities + location;
        result.location = location;
        memcpy(result.name, city->name, sizeof(city->name));
    }
    else if (location >= NAME_INDEX_FIRST_GATE_ID && location < NAME_INDEX_FIRST_GATE_ID + NAME_INDEX_GATE_COUNT)
    {
        name_index_gate *gate = e->sim->names.gates_and_ports + (location - NAME_INDEX_FIRST_GATE_ID);
        result.location = location;
        memcpy(result.name, gate->name, sizeof(result.name));
    }
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

/* ---- M1 develop and M2 turns -------------------------------------------- */

internal engine_date
engine_api_date(void *opaque)
{
    sim_turn_state *turns = &((engine *)opaque)->sim->turns;
    engine_date date;
    date.year             = turns->date.year;
    date.month            = turns->date.month;
    date.day              = calendar_day(turns->date);
    date.season           = (u8)calendar_season_of(turns->date);
    date.days_since_start = calendar_days_between(turns->start_date, turns->date);
    return date;
}

internal u8
engine_api_whose_turn(void *opaque)
{
    return sim_whose_turn(((engine *)opaque)->sim);
}

internal u32
engine_api_actions_left(void *opaque, u8 force_id)
{
    return ((engine *)opaque)->sim->turns.action_points[force_id];
}

internal u32
engine_api_faction_count(void *opaque)
{
    return ((engine *)opaque)->sim->turns.order_count;
}

internal engine_faction
engine_api_faction_at(void *opaque, u32 index)
{
    sim_turn_state *turns = &((engine *)opaque)->sim->turns;
    engine_faction result = {};
    result.force_id = ENGINE_NO_FACTION;
    if (index >= turns->order_count) return result;
    sim_faction *faction = turns->order + index;
    result.force_id          = faction->force_id;
    result.city_count        = faction->city_count;
    result.officer_count     = faction->officer_count;
    result.troops            = faction->troops;
    result.leader_officer_id = faction->leader_officer_id;
    result.action_points     = turns->action_points[faction->force_id];
    result.human             = turns->human[faction->force_id];
    result.acted             = index < turns->acting;
    return result;
}

internal sim_develop_order
engine_develop_order(u8 city_id, u8 facility, const u16 *officers, u8 slot)
{
    sim_develop_order order = {};
    order.city_id  = city_id;
    order.facility = facility;
    order.slot     = slot;
    for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place) order.officers[place] = officers[place];
    return order;
}

/* The sim command of an engine command of M3, or SIM_COMMAND_NONE. */
internal sim_command_kind
engine_city_command_kind(engine_command_kind kind)
{
    switch (kind)
    {
        case ENGINE_COMMAND_PATROL:    return SIM_COMMAND_PATROL;
        case ENGINE_COMMAND_CONSCRIPT: return SIM_COMMAND_CONSCRIPT;
        case ENGINE_COMMAND_TRAIN:     return SIM_COMMAND_TRAIN;
        case ENGINE_COMMAND_MERCHANT:  return SIM_COMMAND_MERCHANT;
        case ENGINE_COMMAND_PRODUCE:   return SIM_COMMAND_PRODUCE;
        default:                       return SIM_COMMAND_NONE;
    }
}

internal sim_city_order
engine_city_order(u8 city_id, const u16 *officers, u8 weapon, u8 mode, u32 amount)
{
    sim_city_order order = {};
    order.city_id = city_id;
    order.weapon  = weapon;
    order.mode    = mode;
    order.amount  = amount;
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place) order.officers[place] = officers ? officers[place] : SIM_NO_OFFICER;
    return order;
}

internal sim_research_order
engine_research_order(u8 city_id, u8 technique, const u16 *officers)
{
    sim_research_order order = {};
    order.city_id   = city_id;
    order.technique = technique;
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place) order.officers[place] = officers ? officers[place] : SIM_NO_OFFICER;
    return order;
}

internal sim_merge_order
engine_merge_order(u8 city_id, u8 absorber, u8 absorbed, u16 officer)
{
    sim_merge_order order = {};
    order.city_id  = city_id;
    order.absorber = absorber;
    order.absorbed = absorbed;
    order.officer  = officer;
    return order;
}

/* The sim order of 出征 or 輸送. 輸送 always uses the transport row of the
   unit table. */
internal sim_sortie_order
engine_sim_sortie_order(engine *e, const engine_sortie_order *in, b32 transport)
{
    sim_sortie_order order = {};
    order.city_id     = in->city_id;
    order.unit_type   = in->unit_type;
    order.soldiers    = in->soldiers;
    order.gold        = in->gold;
    order.grain       = in->grain;
    order.destination = transport ? in->destination : SIM_NO_CITY;
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place) order.officers[place] = in->officers[place];
    for (u32 stock = 0; stock < CITY_EQUIPMENT_STOCK_COUNT; ++stock) order.cargo[stock] = transport ? in->cargo[stock] : 0;
    if (transport)
    {
        order.unit_type = CONTENT_NO_UNIT_TYPE;
        for (u32 index = 0; index < e->sim->content.unit_type_count; ++index)
        {
            if (e->sim->content.unit_types[index].transport) order.unit_type = (u8)index;
        }
    }
    return order;
}

internal sim_unit_order
engine_unit_order(u32 unit_id, s32 q, s32 r, u32 target, u32 soldiers, u32 gold, u32 grain)
{
    sim_unit_order order = {};
    order.unit     = (u16)(unit_id < SIM_UNIT_CAPACITY ? unit_id : SIM_NO_UNIT);
    order.to       = hex_make(q, r);
    order.target   = (u16)(target < SIM_UNIT_CAPACITY ? target : SIM_NO_UNIT);
    order.soldiers = soldiers;
    order.gold     = gold;
    order.grain    = grain;
    return order;
}

internal sim_command_kind
engine_unit_command_kind(engine_command_kind kind)
{
    switch (kind)
    {
        case ENGINE_COMMAND_MOVE:    return SIM_COMMAND_MOVE;
        case ENGINE_COMMAND_WAIT:    return SIM_COMMAND_WAIT;
        case ENGINE_COMMAND_RETREAT: return SIM_COMMAND_RETREAT;
        case ENGINE_COMMAND_SUPPLY:  return SIM_COMMAND_SUPPLY;
        case ENGINE_COMMAND_ATTACK:       return SIM_COMMAND_ATTACK;
        case ENGINE_COMMAND_JOINT_ATTACK: return SIM_COMMAND_JOINT_ATTACK;
        case ENGINE_COMMAND_TACTIC:       return SIM_COMMAND_TACTIC;
        case ENGINE_COMMAND_PLOT:         return SIM_COMMAND_PLOT;
        case ENGINE_COMMAND_BUILD:        return SIM_COMMAND_BUILD;
        case ENGINE_COMMAND_REPAIR:       return SIM_COMMAND_REPAIR;
        case ENGINE_COMMAND_DISMANTLE:    return SIM_COMMAND_DISMANTLE;
        default:                     return SIM_COMMAND_NONE;
    }
}

internal engine_check
engine_api_can_issue(void *opaque, const engine_command *command)
{
    engine *e = (engine *)opaque;
    switch (command->kind)
    {
        case ENGINE_COMMAND_DEVELOP:
        {
            sim_develop_order order = engine_develop_order(command->city_id, command->facility, command->officers, command->slot);
            return sim_check_develop(e->sim, &order);
        }
        case ENGINE_COMMAND_PATROL:
        case ENGINE_COMMAND_CONSCRIPT:
        case ENGINE_COMMAND_TRAIN:
        case ENGINE_COMMAND_MERCHANT:
        case ENGINE_COMMAND_PRODUCE:
        {
            sim_city_order order = engine_city_order(command->city_id, command->officers, command->weapon,
                                                     command->merchant_mode, command->amount);
            return sim_check_city_command(e->sim, engine_city_command_kind(command->kind), &order);
        }
        case ENGINE_COMMAND_RESEARCH:
        {
            sim_research_order order = engine_research_order(command->city_id, command->technique, command->officers);
            return sim_check_research(e->sim, &order);
        }
        case ENGINE_COMMAND_MERGE:
        {
            sim_merge_order order = engine_merge_order(command->city_id, command->slot, command->absorbed, command->officers[0]);
            return sim_check_merge(e->sim, &order);
        }
        case ENGINE_COMMAND_SORTIE:
        case ENGINE_COMMAND_TRANSPORT:
        {
            sim_sortie_order order = engine_sim_sortie_order(e, &command->sortie, command->kind == ENGINE_COMMAND_TRANSPORT);
            return sim_check_sortie(e->sim, &order, command->kind == ENGINE_COMMAND_TRANSPORT);
        }
        case ENGINE_COMMAND_MOVE:
        case ENGINE_COMMAND_WAIT:
        case ENGINE_COMMAND_RETREAT:
        case ENGINE_COMMAND_SUPPLY:
        case ENGINE_COMMAND_ATTACK:
        case ENGINE_COMMAND_JOINT_ATTACK:
        case ENGINE_COMMAND_TACTIC:
        case ENGINE_COMMAND_PLOT:
        case ENGINE_COMMAND_BUILD:
        case ENGINE_COMMAND_REPAIR:
        case ENGINE_COMMAND_DISMANTLE:
        {
            sim_unit_order order = engine_unit_order(command->unit, command->q, command->r, command->target_unit,
                                                     command->soldiers, command->gold, command->grain);
            order.kind = command->row;
            arena_reset(&e->sim->scratch);
            return sim_check_unit_order(e->sim, engine_unit_command_kind(command->kind), &order, &e->sim->scratch);
        }
        default:
        {
            engine_check none = {};
            none.refusal = SIM_REFUSAL_NO_CITY;
            return none;
        }
    }
}

internal engine_city_slots
engine_api_city_slots(void *opaque, u8 city_id)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_city_slots result = {};
    result.city_id     = ENGINE_NO_CITY;
    result.governor_id = ENGINE_NO_OFFICER;
    if (!sim_find_city(sim, city_id)) return result;
    sim_city_site *site = sim->sites + city_id;
    result.city_id      = city_id;
    result.slot_count   = site->slot_count;
    result.used         = (u8)sim_city_used_slots(sim, city_id);
    result.q            = site->hex.q;
    result.r            = site->hex.r;
    sim_income_forecast forecast = sim_city_forecast(sim, city_id);
    result.gold_income  = forecast.gold_month;
    result.grain_income = forecast.grain_season;
    result.governor_id  = sim_city_governor(sim, city_id);
    if (city_id < sim->content.city_count)
    {
        result.specialty = (u8)sim->content.cities[city_id].specialty;
        result.has_port  = sim->content.cities[city_id].has_port;
    }
    return result;
}

internal engine_facility
engine_api_facility_at(void *opaque, u8 city_id, u8 slot_index)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_facility result = {};
    result.city_id  = ENGINE_NO_CITY;
    result.facility = ENGINE_NO_FACILITY;
    if (!sim_find_city(sim, city_id) || slot_index >= sim->sites[city_id].slot_count) return result;
    sim_slot *slot = sim->sites[city_id].slots + slot_index;
    result.city_id    = city_id;
    result.slot       = slot_index;
    result.state      = slot->state;
    result.facility   = slot->facility;
    result.level      = slot->level;
    result.turns_left = slot->turns_left;
    result.q          = slot->hex.q;
    result.r          = slot->hex.r;
    facility_effect effect = FACILITY_EFFECT_NONE;
    result.yield        = sim_slot_yield(sim, city_id, slot_index, &effect);
    result.yield_effect = (u8)effect;
    for (u32 place = 0; place < SIM_DEVELOP_OFFICERS; ++place) result.officers[place] = slot->officers[place];
    result.durability     = slot->durability;
    result.durability_max = slot->facility < sim->content.facility_count ? sim->content.facilities[slot->facility].durability : 0;
    result.job_weapon     = slot->job_weapon;
    result.used           = slot->used_turn == sim->turn + 1;
    return result;
}

internal u32
engine_api_facility_kind_count(void *opaque)
{
    return ((engine *)opaque)->sim->content.facility_count;
}

internal engine_facility_kind
engine_api_facility_kind_at(void *opaque, u32 index)
{
    sim_content *content = &((engine *)opaque)->sim->content;
    engine_facility_kind result = {};
    result.id = ENGINE_NO_FACILITY;
    if (index >= content->facility_count) return result;
    facility_kind *kind = content->facilities + index;
    result.id         = kind->id;
    memcpy(result.key, kind->key, sizeof(result.key));
    result.cost       = kind->cost;
    result.durability = kind->durability;
    result.min_days   = kind->min_days;
    result.effect     = (u8)kind->effect;
    result.value      = kind->value;
    result.limit      = (u8)kind->limit;
    result.pk         = kind->pk;
    result.needs_item = kind->needs_item;
    result.value_assumed  = kind->value_assumed;
    result.period_assumed = kind->period_assumed;
    return result;
}

internal engine_content_status
engine_api_content_status(void *opaque)
{
    return ((engine *)opaque)->content;
}

/* Checks first: a refused develop is not a command, so it gets no id and
   ledgers nothing. */
internal engine_command_result
engine_api_command_develop(void *opaque, u8 city_id, u8 facility, const u16 officers[SIM_DEVELOP_OFFICERS], u8 slot)
{
    engine *e = (engine *)opaque;
    engine_command_result result = {};
    sim_develop_order order = engine_develop_order(city_id, facility, officers, slot);
    result.refusal = sim_check_develop(e->sim, &order).refusal;
    if (result.refusal != SIM_REFUSAL_NONE) return result;
    sim_command command = sim_make_command(e->sim, SIM_COMMAND_DEVELOP);
    command.develop = order;
    engine_run(e, command, 0);
    result.command_id = command.id;
    return result;
}

internal void
engine_api_set_faction_human(void *opaque, u8 force_id, b32 human)
{
    ((engine *)opaque)->sim->turns.human[force_id] = human;
}

internal void
engine_api_set_city_owner(void *opaque, u8 city_id, u8 force_id)
{
    city_component *city = sim_find_city(((engine *)opaque)->sim, city_id);
    if (city) city->owner_force_id = force_id;
}

internal void
engine_api_set_city_gold(void *opaque, u8 city_id, u32 gold)
{
    city_component *city = sim_find_city(((engine *)opaque)->sim, city_id);
    if (city) city->gold = gold;
}

internal void
engine_api_set_actions(void *opaque, u8 force_id, u32 action_points)
{
    sim_state *sim = ((engine *)opaque)->sim;
    u32 cap = sim->content.rules.ap_cap;
    sim->turns.action_points[force_id] = action_points < cap ? action_points : cap;
}

/* A test and editor unit: no type, no officers. It blocks its hex and has
   a zone of control. */
internal u32
engine_api_place_unit(void *opaque, u8 force_id, s32 q, s32 r)
{
    sim_state *sim = ((engine *)opaque)->sim;
    u32 unit_id = sim_new_unit_id(sim);
    if (unit_id == SIM_NO_UNIT) return ENGINE_NO_UNIT;
    sim_unit *unit = sim->units + unit_id;
    *unit = {};
    unit->alive       = true;
    unit->force_id    = force_id;
    unit->hex         = hex_make(q, r);
    unit->unit_type   = CONTENT_NO_UNIT_TYPE;
    unit->home_city   = SIM_NO_CITY;
    unit->destination = SIM_NO_CITY;
    for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place) unit->officers[place] = SIM_NO_OFFICER;
    return unit_id;
}

/* ---- M3 economy --------------------------------------------------------- */

internal engine_city_economy
engine_api_city_economy(void *opaque, u8 city_id)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_city_economy result = {};
    result.city_id = ENGINE_NO_CITY;
    city_component *city = sim_find_city(sim, city_id);
    if (!city) return result;
    sim_rules *rules = &sim->content.rules;
    result.city_id         = city_id;
    result.gold            = city->gold;
    result.gold_cap        = rules->city_gold_cap;
    result.grain           = city->food;
    result.grain_cap       = rules->city_grain_cap;
    result.public_order    = city->public_order;
    result.order_max       = rules->order_max;
    result.bandits_blocked = rules->order_bandit_threshold && city->public_order >= rules->order_bandit_threshold;
    result.morale          = city->morale;
    result.morale_max      = sim_city_morale_max(sim, city);
    result.soldiers        = city->troops;
    result.soldier_cap     = sim_city_soldier_cap(sim, city_id);
    result.weapon_cap      = rules->city_weapon_cap;
    for (u32 index = 0; index < CITY_EQUIPMENT_STOCK_COUNT; ++index) result.stocks[index] = city->equipment_stocks[index];
    result.grain_per_gold  = rules->merchant_grain_per_gold;
    result.patrolled       = city->patrolled_turn == sim->turn + 1;
    result.trained         = city->trained_turn == sim->turn + 1;
    sim_city_site *site = sim->sites + city_id;
    for (u32 slot = 0; slot < site->slot_count; ++slot)
    {
        sim_slot *candidate = site->slots + slot;
        if (candidate->state != SIM_SLOT_BUILT || candidate->facility >= sim->content.facility_count ||
            sim->content.facilities[candidate->facility].effect != FACILITY_EFFECT_CONSCRIPTION) continue;
        result.barracks++;
        if (candidate->used_turn != sim->turn + 1) result.barracks_free++;
    }
    result.technique_points = city->owner_force_id != SIM_NO_FORCE ? sim->technique_points[city->owner_force_id] : 0;
    return result;
}

internal engine_income_forecast
engine_api_city_income_forecast(void *opaque, u8 city_id)
{
    sim_state *sim = ((engine *)opaque)->sim;
    engine_income_forecast result = {};
    result.city_id = ENGINE_NO_CITY;
    if (!sim_find_city(sim, city_id)) return result;
    result.city_id = city_id;
    result.assumed = true;    /* Q-021, Q-027, Q-119, Q-151 */
    result.amounts = sim_city_forecast(sim, city_id);
    return result;
}

internal u32
engine_api_technique_points(void *opaque, u8 force_id)
{
    return ((engine *)opaque)->sim->technique_points[force_id];
}

internal u32
engine_api_weapon_kind_count(void *opaque)
{
    return ((engine *)opaque)->sim->content.weapon_count;
}

internal engine_weapon_kind
engine_api_weapon_kind_at(void *opaque, u32 index)
{
    sim_content *content = &((engine *)opaque)->sim->content;
    engine_weapon_kind result = {};
    result.id = ENGINE_NO_WEAPON;
    if (index >= content->weapon_count) return result;
    weapon_kind *kind = content->weapons + index;
    result.id              = kind->id;
    memcpy(result.key, kind->key, sizeof(result.key));
    result.cost            = kind->cost;
    result.facility        = (u8)kind->facility;
    result.specialty       = (u8)kind->specialty;
    result.stock           = kind->stock;
    result.min_days        = kind->min_days;
    result.max_days        = kind->max_days;
    result.technique       = kind->technique;
    result.replaces        = kind->replaces;
    return result;
}

/* A query: the events of a city that change a resource. A city event has
   the entity of the city as its subject (economy.cpp, sim_emit_city). */
internal engine_ledger_view
engine_api_ledger_filter(void *opaque, u8 city_id, u8 resource)
{
    engine *e = (engine *)opaque;
    engine_ledger_view view = {};
    view.indices = e->filter_indices;
    entity_id subject = 0;
    if (city_id != ENGINE_ANY_CITY)
    {
        subject = sim_city_entity(e->sim, city_id);
        if (!subject) return view;
    }
    for (u32 index = 0; index < e->ledger_count; ++index)
    {
        sim_event *event = e->ledger + index;
        if (city_id != ENGINE_ANY_CITY && event->subject != subject) continue;
        if (resource != ENGINE_ANY_RESOURCE && sim_event_resource(event->kind) != resource) continue;
        e->filter_indices[view.count++] = index;
    }
    return view;
}

/* Checks first: a refused command is not a command, so it gets no id and
   ledgers nothing. */
internal engine_command_result
engine_run_city_command(engine *e, sim_command_kind kind, sim_city_order order)
{
    engine_command_result result = {};
    result.refusal = sim_check_city_command(e->sim, kind, &order).refusal;
    if (result.refusal != SIM_REFUSAL_NONE) return result;
    sim_command command = sim_make_command(e->sim, kind);
    command.city = order;
    engine_run(e, command, 0);
    result.command_id = command.id;
    return result;
}

internal engine_command_result
engine_api_command_patrol(void *opaque, u8 city_id, const u16 officers[SIM_DEVELOP_OFFICERS])
{
    return engine_run_city_command((engine *)opaque, SIM_COMMAND_PATROL, engine_city_order(city_id, officers, CONTENT_NO_WEAPON, 0, 0));
}

internal engine_command_result
engine_api_command_conscript(void *opaque, u8 city_id, const u16 officers[SIM_DEVELOP_OFFICERS])
{
    return engine_run_city_command((engine *)opaque, SIM_COMMAND_CONSCRIPT, engine_city_order(city_id, officers, CONTENT_NO_WEAPON, 0, 0));
}

internal engine_command_result
engine_api_command_train(void *opaque, u8 city_id, const u16 officers[SIM_DEVELOP_OFFICERS])
{
    return engine_run_city_command((engine *)opaque, SIM_COMMAND_TRAIN, engine_city_order(city_id, officers, CONTENT_NO_WEAPON, 0, 0));
}

internal engine_command_result
engine_api_command_merchant(void *opaque, u8 city_id, u16 officer, u8 mode, u32 amount)
{
    u16 officers[SIM_COMMAND_OFFICERS] = { officer, SIM_NO_OFFICER, SIM_NO_OFFICER };
    return engine_run_city_command((engine *)opaque, SIM_COMMAND_MERCHANT, engine_city_order(city_id, officers, CONTENT_NO_WEAPON, mode, amount));
}

internal engine_command_result
engine_api_command_produce(void *opaque, u8 city_id, u8 weapon, const u16 officers[SIM_DEVELOP_OFFICERS])
{
    return engine_run_city_command((engine *)opaque, SIM_COMMAND_PRODUCE, engine_city_order(city_id, officers, weapon, 0, 0));
}

internal engine_command_result
engine_api_damage_facility(void *opaque, u8 city_id, u8 slot, u32 amount)
{
    engine *e = (engine *)opaque;
    engine_command_result result = {};
    sim_damage_order order = {};
    order.city_id = city_id;
    order.slot    = slot;
    order.amount  = amount;
    result.refusal = sim_check_damage(e->sim, &order);
    if (result.refusal != SIM_REFUSAL_NONE) return result;
    sim_command command = sim_make_command(e->sim, SIM_COMMAND_DAMAGE_FACILITY);
    command.damage = order;
    engine_run(e, command, 0);
    result.command_id = command.id;
    return result;
}

/* The editor sets the level that the PK merge (F-024, M4) gives. */
internal b32
engine_api_set_facility_level(void *opaque, u8 city_id, u8 slot_index, u8 level)
{
    sim_state *sim = ((engine *)opaque)->sim;
    if (!sim_find_city(sim, city_id) || slot_index >= sim->sites[city_id].slot_count || level < 1 || level > 3) return false;
    sim_slot *slot = sim->sites[city_id].slots + slot_index;
    if (slot->state != SIM_SLOT_BUILT || slot->facility >= sim->content.facility_count ||
        !sim->content.facilities[slot->facility].mergeable) return false;
    slot->level = level;
    return true;
}

internal void
engine_api_set_city_resource(void *opaque, u8 city_id, u8 resource, u32 index, u32 value)
{
    city_component *city = sim_find_city(((engine *)opaque)->sim, city_id);
    if (!city) return;
    u8 byte_value = (u8)Minimum(value, 255u);
    switch (resource)
    {
        case SIM_RESOURCE_GOLD:         city->gold   = value; break;
        case SIM_RESOURCE_GRAIN:        city->food   = value; break;
        case SIM_RESOURCE_SOLDIERS:     city->troops = value; break;
        case SIM_RESOURCE_MORALE:       city->morale = byte_value; break;
        case SIM_RESOURCE_PUBLIC_ORDER: city->public_order = byte_value; break;
        case SIM_RESOURCE_WEAPONS:      if (index < CITY_EQUIPMENT_STOCK_COUNT) city->equipment_stocks[index] = value; break;
        default: break;
    }
}

#include "engine/engine_growth.cpp"
#include "engine/engine_march.cpp"
#include "engine/engine_combat.cpp"

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
    api.date            = engine_api_date;
    api.whose_turn      = engine_api_whose_turn;
    api.actions_left    = engine_api_actions_left;
    api.faction_count   = engine_api_faction_count;
    api.faction_at      = engine_api_faction_at;
    api.can_issue       = engine_api_can_issue;
    api.city_slots      = engine_api_city_slots;
    api.facility_at     = engine_api_facility_at;
    api.facility_kind_count = engine_api_facility_kind_count;
    api.facility_kind_at    = engine_api_facility_kind_at;
    api.content_status  = engine_api_content_status;
    api.command_develop = engine_api_command_develop;
    api.set_faction_human = engine_api_set_faction_human;
    api.set_city_owner  = engine_api_set_city_owner;
    api.set_city_gold   = engine_api_set_city_gold;
    api.set_actions     = engine_api_set_actions;
    api.place_unit      = engine_api_place_unit;
    api.officer_by_id   = engine_api_officer_by_id;
    api.force_ruler     = engine_api_force_ruler;
    api.location_name_at = engine_api_location_name_at;
    api.city_economy         = engine_api_city_economy;
    api.city_income_forecast = engine_api_city_income_forecast;
    api.technique_points     = engine_api_technique_points;
    api.weapon_kind_count    = engine_api_weapon_kind_count;
    api.weapon_kind_at       = engine_api_weapon_kind_at;
    api.ledger_filter        = engine_api_ledger_filter;
    api.command_patrol       = engine_api_command_patrol;
    api.command_conscript    = engine_api_command_conscript;
    api.command_train        = engine_api_command_train;
    api.command_merchant     = engine_api_command_merchant;
    api.command_produce      = engine_api_command_produce;
    api.damage_facility      = engine_api_damage_facility;
    api.set_facility_level   = engine_api_set_facility_level;
    api.set_city_resource    = engine_api_set_city_resource;
    engine_bind_growth_api(&api);
    engine_bind_march_api(&api);
    engine_bind_combat_api(&api);
    return api;
}
