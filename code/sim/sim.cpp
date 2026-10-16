#include "sim/sim.h"
#include "sim/save_format.cpp"
#include "sim/scenario_format.cpp"

internal void
sim_init(sim_state *sim, memory_arena *arena, u64 seed)
{
    sim->rng = seed ? seed : 0x9E3779B97F4A7C15ull;
    sim->turn = 0;
    sim->next_command_id = 1;
    em_init(&sim->entities, arena, SIM_ENTITY_CAPACITY);
    table_init(&sim->officers, arena, SIM_ENTITY_CAPACITY, SIM_OFFICER_CAPACITY);
    table_init(&sim->cities,   arena, SIM_ENTITY_CAPACITY, SIM_CITY_CAPACITY);
    sim->map.width = sim->map.height = 0;
    sim->map.cost = 0;
}

/* The real map's size and terrain come from spec-src/spec/formats/ once the
   scenario format is documented; until then callers pass a placeholder. */
internal void
sim_init_map(sim_state *sim, memory_arena *arena, u32 width, u32 height, u8 default_cost)
{
    hex_map_init(&sim->map, arena, width, height, default_cost);
}

/* xorshift64*: small, fast, deterministic across compilers. */
internal u32
sim_random_u32(sim_state *sim)
{
    u64 x = sim->rng;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    sim->rng = x;
    return (u32)((x * 0x2545F4914F6CDD1Dull) >> 32);
}

/* Unbiased integer in [0, bound). */
internal u32
sim_random_below(sim_state *sim, u32 bound)
{
    Assert(bound > 0);
    u32 threshold = (0u - bound) % bound;
    for (;;)
    {
        u32 r = sim_random_u32(sim);
        if (r >= threshold) return r % bound;
    }
}

internal sim_event_sink
sim_sink_from_arena(memory_arena *arena, u32 capacity)
{
    sim_event_sink sink;
    sink.events   = PushArray(arena, capacity, sim_event);
    sink.count    = 0;
    sink.capacity = capacity;
    return sink;
}

internal void
sim_emit(sim_event_sink *sink, sim_event event)
{
    /* A full sink drops the event; callers size sinks for the command they run. */
    if (sink->count < sink->capacity) sink->events[sink->count++] = event;
}

internal sim_command
sim_make_command(sim_state *sim, sim_command_kind kind)
{
    sim_command command;
    command.kind = kind;
    command.id   = sim->next_command_id++;
    return command;
}

internal void
sim_advance_turn(sim_state *sim, sim_event_sink *events)
{
    /* Per-turn systems run here in fixed order once they exist:
       TurnScheduler -> CommandResolution -> Production -> Movement -> Combat
       -> Diplomacy -> AIPlanning -> EventTriggers -> LedgerApply.
       Each is a free function taking sim_state* and an event sink. */
    sim->turn++;
    sim_event event = {};
    event.kind  = SIM_EVENT_TURN_ADVANCED;
    event.turn  = sim->turn;
    event.delta = 1;
    sim_emit(events, event);
}

internal void
sim_apply(sim_state *sim, sim_command command, sim_event_sink *events)
{
    switch (command.kind)
    {
        case SIM_COMMAND_NONE:
            break;
        case SIM_COMMAND_END_TURN:
        {
            u32 first = events->count;
            sim_advance_turn(sim, events);
            for (u32 i = first; i < events->count; ++i) events->events[i].cause = command.id;
            break;
        }
        case SIM_COMMAND_LOAD_SAVE:
            InvalidCodePath;   /* needs its save: see sim_load_save */
            break;
        default:
            InvalidCodePath;
            break;
    }
}

/* Destroys every officer and city entity and empties their tables. */
internal void
sim_unload_world(sim_state *sim)
{
    while (sim->officers.count)
    {
        entity_id owner = sim->officers.dense_owner[sim->officers.count - 1];
        table_remove(&sim->officers, owner);
        em_destroy(&sim->entities, owner);
    }
    while (sim->cities.count)
    {
        entity_id owner = sim->cities.dense_owner[sim->cities.count - 1];
        table_remove(&sim->cities, owner);
        em_destroy(&sim->entities, owner);
    }
}

internal officer_component
sim_officer_from_record(officer_record *record)
{
    officer_component officer = {};
    officer.officer_id = record->id;
    memcpy(officer.family_name,   record->family_name,   OFFICER_FAMILY_NAME_SIZE);
    memcpy(officer.given_name,    record->given_name,    OFFICER_GIVEN_NAME_SIZE);
    memcpy(officer.courtesy_name, record->courtesy_name, OFFICER_COURTESY_NAME_SIZE);
    officer.appearance_year          = record->appearance_year;
    officer.birth_year               = record->birth_year;
    officer.death_year               = record->death_year;
    officer.leadership               = record->leadership;
    officer.war                      = record->war;
    officer.intelligence             = record->intelligence;
    officer.politics                 = record->politics;
    officer.charm                    = record->charm;
    officer.force_id                 = record->force_id;
    officer.base_location            = record->base_location;
    officer.current_position         = record->current_position;
    officer.status                   = record->status;
    officer.merit                    = record->merit;
    officer.skill_number             = record->skill_number;
    officer.compatibility            = record->compatibility;
    officer.father                   = record->father;
    officer.spouse                   = record->spouse;
    officer.sworn_brother_group_head = record->sworn_brother_group_head;
    memcpy(officer.unit_aptitudes, record->unit_aptitudes, OFFICER_UNIT_APTITUDE_COUNT);
    return officer;
}

internal city_component
sim_city_from_record(city_record *record)
{
    city_component city = {};
    city.city_id               = record->id;
    city.owner_force_id        = record->owner_force_id;
    city.commanding_officer_id = record->commanding_officer_id;
    city.capacity_constant     = record->capacity_constant;
    city.gold                  = record->gold;
    city.troops                = record->troops;
    city.food                  = record->food;
    memcpy(city.equipment_stocks, record->equipment_stocks, sizeof(city.equipment_stocks));
    city.durability_maximum    = record->durability_maximum;
    city.public_order          = record->public_order;
    return city;
}

/* Replaces the officers and cities with those of a decoded save: one entity
   per slot that holds an officer, in slot order, then one per city in id
   order. Emits one SIM_EVENT_SAVE_LOADED. The turn counter does not change,
   because the spec has not found the game date (QUESTIONS.md, Q-004).

   Returns false, and changes nothing, when the entities or the tables
   cannot hold the new world. */
internal b32
sim_load_save(sim_state *sim, save_file *save, sim_command command, sim_event_sink *events)
{
    u32 officers_needed = 0;
    for (u32 slot = 0; slot < save->officer_slot_count; ++slot)
    {
        if (!officer_slot_is_empty(save->officers + slot)) officers_needed++;
    }
    entity_manager *entities = &sim->entities;
    u32 entities_free_after_unload = (entities->capacity - entities->next_unused) + entities->free_count +
                                     sim->officers.count + sim->cities.count;
    if (officers_needed > sim->officers.capacity || save->city_count > sim->cities.capacity ||
        officers_needed + save->city_count > entities_free_after_unload)
    {
        return false;
    }

    sim_unload_world(sim);
    for (u32 slot = 0; slot < save->officer_slot_count; ++slot)
    {
        officer_record *record = save->officers + slot;
        if (officer_slot_is_empty(record)) continue;
        table_add(&sim->officers, em_create(entities), sim_officer_from_record(record));
    }
    for (u32 index = 0; index < save->city_count; ++index)
    {
        table_add(&sim->cities, em_create(entities), sim_city_from_record(save->cities + index));
    }

    sim_event event = {};
    event.kind  = SIM_EVENT_SAVE_LOADED;
    event.turn  = sim->turn;
    event.delta = (s32)sim->officers.count;
    event.cause = command.id;
    sim_emit(events, event);
    return true;
}

internal const char *
sim_event_name(sim_event_kind kind)
{
    switch (kind)
    {
        case SIM_EVENT_NONE:          return "none";
        case SIM_EVENT_TURN_ADVANCED: return "turn_advanced";
        case SIM_EVENT_SAVE_LOADED:   return "save_loaded";
        default:                      return "?";
    }
}

internal const char *
sim_command_name(sim_command_kind kind)
{
    switch (kind)
    {
        case SIM_COMMAND_NONE:      return "none";
        case SIM_COMMAND_END_TURN:  return "end_turn";
        case SIM_COMMAND_LOAD_SAVE: return "load_save";
        default:                   return "?";
    }
}
