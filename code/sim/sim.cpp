#include "sim/sim.h"
#include "sim/save_format.cpp"
#include "sim/scenario_format.cpp"
#include "sim/content.cpp"

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
    sim->has_names = false;
    sim->has_content = false;
    sim->content = {};
    sim->turns = {};
    sim->turns.start_date = calendar_make(SIM_DEFAULT_START_YEAR, SIM_DEFAULT_START_MONTH, 1);
    sim->turns.date       = sim->turns.start_date;
    sim->unit_count = 0;
    for (u32 force = 0; force < SIM_FORCE_CAPACITY; ++force)
    {
        sim->technique_points[force]     = 0;
        sim->techniques[force]           = 0;
        sim->research[force]             = {};
        sim->research[force].technique   = CONTENT_NO_TECHNIQUE;
    }
    sim->settings           = {};
    sim->settings.disasters = true;   /* a new game has disasters (game-setup-and-modes.md) */
    sim->outcome            = {};
    for (u32 index = 0; index < ArrayCount(sim->officer_index); ++index) sim->officer_index[index] = SIM_NO_OFFICER;
    for (u32 index = 0; index < ArrayCount(sim->city_index); ++index)    sim->city_index[index]    = SIM_NO_CITY;
    sim_init_sites(sim);
}

/* Copies the tables of the content pack into the sim. The slots of the
   cities are laid out again, so call it before a save loads. */
internal void
sim_set_content(sim_state *sim, sim_content *content)
{
    sim->content     = *content;
    sim->has_content = true;
    sim_init_sites(sim);
}

/* The real map's size and terrain come from spec-src/spec/formats/ once the
   scenario format is documented; until then callers pass a placeholder. */
internal void
sim_init_map(sim_state *sim, memory_arena *arena, u32 width, u32 height, u8 default_cost)
{
    hex_map_init(&sim->map, arena, width, height, default_cost);
    /* The working memory of a move range: six arrays and a heap for each
       cell, with room. */
    sim->scratch = arena_sub(arena, (u64)width * height * 128 + Kilobytes(64));
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

internal officer_component
sim_officer_from_record(officer_record *record)
{
    officer_component officer = {};
    officer.job_city = SIM_NO_CITY;
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

/* An event of this turn without a subject, a faction or an amount. */
internal sim_event
sim_event_make(sim_state *sim, sim_event_kind kind, u32 cause)
{
    sim_event event = {};
    event.kind    = kind;
    event.turn    = sim->turn;
    event.cause   = cause;
    event.faction = SIM_NO_FORCE;
    return event;
}

internal sim_command
sim_make_command(sim_state *sim, sim_command_kind kind)
{
    sim_command command = {};
    command.kind = kind;
    command.id   = sim->next_command_id++;
    return command;
}

/* Moves the date one turn and starts the new turn: the calendar events,
   the turn order, the action points and the jobs (turn.cpp). */
internal void
sim_advance_turn(sim_state *sim, u32 cause, sim_event_sink *events)
{
    sim->turn++;
    sim->turns.date = calendar_next_turn(sim->turns.date);
    sim_event event = sim_event_make(sim, SIM_EVENT_TURN_ADVANCED, cause);
    event.delta = 1;
    sim_emit(events, event);
    sim_run_calendar(sim, cause, events);
    sim_start_turn(sim, cause, events);
}

internal void
sim_apply(sim_state *sim, sim_command command, sim_event_sink *events)
{
    switch (command.kind)
    {
        case SIM_COMMAND_NONE:
            break;
        case SIM_COMMAND_END_TURN:
            sim_end_faction_turn(sim, command.id, events);
            break;
        case SIM_COMMAND_DEVELOP:
            sim_develop(sim, command, events);
            break;
        case SIM_COMMAND_PATROL:
        case SIM_COMMAND_CONSCRIPT:
        case SIM_COMMAND_TRAIN:
        case SIM_COMMAND_MERCHANT:
        case SIM_COMMAND_PRODUCE:
            sim_city_command(sim, command, events);
            break;
        case SIM_COMMAND_DAMAGE_FACILITY:
            sim_damage_facility(sim, command, events);
            break;
        case SIM_COMMAND_RESEARCH:
            sim_research(sim, command, events);
            break;
        case SIM_COMMAND_MERGE:
            sim_merge(sim, command, events);
            break;
        case SIM_COMMAND_START_DISASTER:
            sim_start_disaster_command(sim, command, events);
            break;
        case SIM_COMMAND_SORTIE:
        case SIM_COMMAND_TRANSPORT:
            sim_sortie(sim, command, events);
            break;
        case SIM_COMMAND_MOVE:
            arena_reset(&sim->scratch);
            sim_move_unit(sim, command, &sim->scratch, events);
            break;
        case SIM_COMMAND_WAIT:
        case SIM_COMMAND_RETREAT:
        case SIM_COMMAND_SUPPLY:
            arena_reset(&sim->scratch);
            sim_unit_action(sim, command, &sim->scratch, events);
            break;
        case SIM_COMMAND_LOAD_SAVE:
            InvalidCodePath;   /* needs its save: see sim_load_save */
            break;
        case SIM_COMMAND_LOAD_NAME_INDEX:
            InvalidCodePath;   /* needs its name index: see sim_load_name_index */
            break;
        default:
            InvalidCodePath;
            break;
    }
}

/* Destroys every officer and city entity and empties their tables, the
   slots and the units. The calendar and the content stay. */
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
    for (u32 index = 0; index < ArrayCount(sim->officer_index); ++index) sim->officer_index[index] = SIM_NO_OFFICER;
    for (u32 index = 0; index < ArrayCount(sim->city_index); ++index)    sim->city_index[index]    = SIM_NO_CITY;
    sim->unit_count = 0;
    sim->turns.order_count = 0;
    sim->turns.acting      = 0;
    for (u32 force = 0; force < SIM_FORCE_CAPACITY; ++force)
    {
        sim->turns.action_points[force] = 0;
        sim->turns.human[force]         = false;
        sim->technique_points[force]    = 0;
        sim->techniques[force]          = 0;
        sim->research[force]            = {};
        sim->research[force].technique  = CONTENT_NO_TECHNIQUE;
    }
    sim->outcome = {};
    sim_init_sites(sim);
}

internal b32
sim_add_officer(sim_state *sim, officer_component officer)
{
    if (officer.officer_id >= ArrayCount(sim->officer_index) || sim->officer_index[officer.officer_id] != SIM_NO_OFFICER ||
        sim->officers.count >= sim->officers.capacity || sim->entities.alive_count >= sim->entities.capacity)
    {
        return false;
    }
    sim->officer_index[officer.officer_id] = (u16)sim->officers.count;
    table_add(&sim->officers, em_create(&sim->entities), officer);
    return true;
}

internal b32
sim_add_city(sim_state *sim, city_component city)
{
    if (city.city_id >= ArrayCount(sim->city_index) || sim->city_index[city.city_id] != SIM_NO_CITY ||
        sim->cities.count >= sim->cities.capacity || sim->entities.alive_count >= sim->entities.capacity)
    {
        return false;
    }
    sim->city_index[city.city_id] = (u8)sim->cities.count;
    table_add(&sim->cities, em_create(&sim->entities), city);
    return true;
}

/* True if the NUL-padded name of the header is the family name and then
   the given name of the officer (save-file.md, header 0x05A). */
internal b32
sim_officer_has_header_name(officer_component *officer, const u8 *header_name, u32 header_size)
{
    u8 joined[OFFICER_FAMILY_NAME_SIZE + OFFICER_GIVEN_NAME_SIZE] = {};
    u32 length = 0;
    for (u32 index = 0; index < OFFICER_FAMILY_NAME_SIZE && officer->family_name[index]; ++index) joined[length++] = officer->family_name[index];
    for (u32 index = 0; index < OFFICER_GIVEN_NAME_SIZE && officer->given_name[index]; ++index)   joined[length++] = officer->given_name[index];
    u32 header_length = 0;
    while (header_length < header_size && header_name[header_length]) header_length++;
    return length > 0 && length == header_length && memcmp(joined, header_name, length) == 0;
}

/* The force of the player: the force of the ruler that the header names.
   SIM_NO_FORCE if no ruler has that name. */
internal u8
sim_player_force(sim_state *sim, save_header *header)
{
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (officer->status == 0 && sim_officer_has_header_name(officer, header->player_ruler_name, SAVE_HEADER_NAME_SIZE))
        {
            return officer->force_id;
        }
    }
    return SIM_NO_FORCE;
}

/* Replaces the officers and cities with those of a decoded save: one entity
   per slot that holds an officer, in slot order, then one per city in id
   order. Emits one SIM_EVENT_SAVE_LOADED, then starts the turn (the order,
   the action points, the AI factions before the player).

   The calendar starts at the scenario start of the header, because the
   spec has not found the game date (Q-004). The ten-day period of the
   header is 1 for the first turn of the month (assumed: every scenario
   has 1). The turn counter does not change.

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
        sim_add_officer(sim, sim_officer_from_record(record));
    }
    for (u32 index = 0; index < save->city_count; ++index)
    {
        city_component city = sim_city_from_record(save->cities + index);
        city.morale = (u8)Minimum(sim->content.rules.city_start_morale, 255u);   /* the save field is not decoded (Q-154) */
        sim_add_city(sim, city);
    }

    u8 day = (u8)(1 + (save->header.scenario_start_ten_day_period > 0 ? save->header.scenario_start_ten_day_period - 1 : 0) *
                      CALENDAR_DAYS_PER_TURN);
    sim->turns.start_date = calendar_make(save->header.scenario_start_year, save->header.scenario_start_month, day);
    sim->turns.date       = sim->turns.start_date;
    u8 player = sim_player_force(sim, &save->header);
    if (player != SIM_NO_FORCE) sim->turns.human[player] = true;

    sim_event event = sim_event_make(sim, SIM_EVENT_SAVE_LOADED, command.id);
    event.delta = (s32)sim->officers.count;
    sim_emit(events, event);
    sim_start_turn(sim, command.id, events);
    return true;
}

/* Replaces the city, gate and port names with those of a decoded name
   index. The officers, the cities and the turn do not change. Emits one
   SIM_EVENT_NAME_INDEX_LOADED. */
internal void
sim_load_name_index(sim_state *sim, name_index *names, sim_command command, sim_event_sink *events)
{
    sim->names     = *names;
    sim->has_names = true;

    sim_event event = sim_event_make(sim, SIM_EVENT_NAME_INDEX_LOADED, command.id);
    event.delta = (s32)ArrayCount(sim->names.cities);
    sim_emit(events, event);
}

internal const char *
sim_event_name(sim_event_kind kind)
{
    switch (kind)
    {
        case SIM_EVENT_NONE:                    return "none";
        case SIM_EVENT_TURN_ADVANCED:           return "turn_advanced";
        case SIM_EVENT_SAVE_LOADED:             return "save_loaded";
        case SIM_EVENT_NAME_INDEX_LOADED:       return "name_index_loaded";
        case SIM_EVENT_YEAR_STARTED:            return "year_started";
        case SIM_EVENT_MONTH_STARTED:           return "month_started";
        case SIM_EVENT_SEASON_STARTED:          return "season_started";
        case SIM_EVENT_GOLD_INCOME:             return "gold_income";
        case SIM_EVENT_SALARIES_PAID:           return "salaries_paid";
        case SIM_EVENT_GRAIN_INCOME:            return "grain_income";
        case SIM_EVENT_GRAIN_CONSUMED:          return "grain_consumed";
        case SIM_EVENT_HOOK_BANDITS:            return "hook_bandits";
        case SIM_EVENT_HOOK_LOCUSTS:            return "hook_locusts";
        case SIM_EVENT_HOOK_PLAGUE:             return "hook_plague";
        case SIM_EVENT_HOOK_GOOD_HARVEST:       return "hook_good_harvest";
        case SIM_EVENT_HOOK_OFFICERS_AGE:       return "hook_officers_age";
        case SIM_EVENT_HOOK_NEW_OFFICERS:       return "hook_new_officers";
        case SIM_EVENT_BLACK_MARKET_REMOVED:    return "black_market_removed";
        case SIM_EVENT_FACTION_TURN_STARTED:    return "faction_turn_started";
        case SIM_EVENT_FACTION_TURN_ENDED:      return "faction_turn_ended";
        case SIM_EVENT_AI_PASSED:               return "ai_passed";
        case SIM_EVENT_ACTION_POINTS_RECOVERED: return "action_points_recovered";
        case SIM_EVENT_ACTION_POINTS_SPENT:     return "action_points_spent";
        case SIM_EVENT_DEVELOP_STARTED:         return "develop_started";
        case SIM_EVENT_GOLD_SPENT:              return "gold_spent";
        case SIM_EVENT_FACILITY_COMPLETED:      return "facility_completed";
        case SIM_EVENT_OFFICER_RELEASED:        return "officer_released";
        case SIM_EVENT_GOLD_OVER_CAP:           return "gold_over_cap";
        case SIM_EVENT_GRAIN_OVER_CAP:          return "grain_over_cap";
        case SIM_EVENT_SALARIES_UNPAID:         return "salaries_unpaid";
        case SIM_EVENT_SOLDIERS_DESERTED:       return "soldiers_deserted";
        case SIM_EVENT_WEAPONS_FREE:            return "weapons_free";
        case SIM_EVENT_FACILITY_REPAIRED:       return "facility_repaired";
        case SIM_EVENT_FACILITY_DAMAGED:        return "facility_damaged";
        case SIM_EVENT_FACILITY_DESTROYED:      return "facility_destroyed";
        case SIM_EVENT_TECHNIQUE_GAINED:        return "technique_gained";
        case SIM_EVENT_MERIT_GAINED:            return "merit_gained";
        case SIM_EVENT_PATROLLED:               return "patrolled";
        case SIM_EVENT_CONSCRIPTED:             return "conscripted";
        case SIM_EVENT_ORDER_LOST:              return "order_lost";
        case SIM_EVENT_MORALE_LOST:             return "morale_lost";
        case SIM_EVENT_TRAINED:                 return "trained";
        case SIM_EVENT_GRAIN_BOUGHT:            return "grain_bought";
        case SIM_EVENT_GRAIN_SOLD:              return "grain_sold";
        case SIM_EVENT_GOLD_EARNED:             return "gold_earned";
        case SIM_EVENT_PRODUCTION_STARTED:      return "production_started";
        case SIM_EVENT_WEAPONS_PRODUCED:        return "weapons_produced";
        case SIM_EVENT_TECHNIQUE_SPENT:         return "technique_spent";
        case SIM_EVENT_RESEARCH_STARTED:        return "research_started";
        case SIM_EVENT_TECHNIQUE_RESEARCHED:    return "technique_researched";
        case SIM_EVENT_MERGE_STARTED:           return "merge_started";
        case SIM_EVENT_FACILITY_MERGED:         return "facility_merged";
        case SIM_EVENT_DISASTER_STARTED:        return "disaster_started";
        case SIM_EVENT_DISASTER_ENDED:          return "disaster_ended";
        case SIM_EVENT_GRAIN_LOST:              return "grain_lost";
        case SIM_EVENT_GRAIN_HARVEST:           return "grain_harvest";
        case SIM_EVENT_SOLDIERS_LOST:           return "soldiers_lost";
        case SIM_EVENT_GAME_WON:                return "game_won";
        case SIM_EVENT_GAME_LOST:               return "game_lost";
        case SIM_EVENT_UNIT_SORTIED:            return "unit_sortied";
        case SIM_EVENT_UNIT_SUPPLIES:           return "unit_supplies";
        case SIM_EVENT_UNIT_GRAIN_TAKEN:        return "unit_grain_taken";
        case SIM_EVENT_UNIT_WEAPONS_TAKEN:      return "unit_weapons_taken";
        case SIM_EVENT_UNIT_MOVED:              return "unit_moved";
        case SIM_EVENT_UNIT_SOLDIERS_LOST:      return "unit_soldiers_lost";
        case SIM_EVENT_UNIT_WAITED:             return "unit_waited";
        case SIM_EVENT_UNIT_ENTERED_CITY:       return "unit_entered_city";
        case SIM_EVENT_UNIT_GOLD_RETURNED:      return "unit_gold_returned";
        case SIM_EVENT_UNIT_GRAIN_RETURNED:     return "unit_grain_returned";
        case SIM_EVENT_UNIT_RETREATED:          return "unit_retreated";
        case SIM_EVENT_UNIT_SUPPLIED:           return "unit_supplied";
        case SIM_EVENT_UNIT_GRAIN_EATEN:        return "unit_grain_eaten";
        case SIM_EVENT_UNIT_DESERTED:           return "unit_deserted";
        default:                                return "?";
    }
}

internal const char *
sim_command_name(sim_command_kind kind)
{
    switch (kind)
    {
        case SIM_COMMAND_NONE:            return "none";
        case SIM_COMMAND_END_TURN:        return "end_turn";
        case SIM_COMMAND_LOAD_SAVE:       return "load_save";
        case SIM_COMMAND_LOAD_NAME_INDEX: return "load_name_index";
        case SIM_COMMAND_DEVELOP:         return "develop";
        case SIM_COMMAND_PATROL:          return "patrol";
        case SIM_COMMAND_CONSCRIPT:       return "conscript";
        case SIM_COMMAND_TRAIN:           return "train";
        case SIM_COMMAND_MERCHANT:        return "merchant";
        case SIM_COMMAND_PRODUCE:         return "produce";
        case SIM_COMMAND_DAMAGE_FACILITY: return "damage_facility";
        case SIM_COMMAND_RESEARCH:        return "research";
        case SIM_COMMAND_MERGE:           return "merge";
        case SIM_COMMAND_START_DISASTER:  return "start_disaster";
        case SIM_COMMAND_SORTIE:          return "sortie";
        case SIM_COMMAND_TRANSPORT:       return "transport";
        case SIM_COMMAND_MOVE:            return "move";
        case SIM_COMMAND_WAIT:            return "wait";
        case SIM_COMMAND_RETREAT:         return "retreat";
        case SIM_COMMAND_SUPPLY:          return "supply";
        default:                          return "?";
    }
}

internal const char *
sim_refusal_name(sim_refusal refusal)
{
    switch (refusal)
    {
        case SIM_REFUSAL_NONE:                  return "none";
        case SIM_REFUSAL_NO_CONTENT:            return "no_content";
        case SIM_REFUSAL_NO_CITY:               return "no_city";
        case SIM_REFUSAL_NOT_YOUR_TURN:         return "not_your_turn";
        case SIM_REFUSAL_NO_FACILITY:           return "no_facility";
        case SIM_REFUSAL_ENEMY_NEAR:            return "enemy_near";
        case SIM_REFUSAL_VALUE_AT_MAXIMUM:      return "value_at_maximum";
        case SIM_REFUSAL_NO_SLOT:               return "no_slot";
        case SIM_REFUSAL_SLOT_TAKEN:            return "slot_taken";
        case SIM_REFUSAL_NO_OFFICER:            return "no_officer";
        case SIM_REFUSAL_OFFICER_NOT_IN_CITY:   return "officer_not_in_city";
        case SIM_REFUSAL_DUPLICATE_OFFICER:     return "duplicate_officer";
        case SIM_REFUSAL_OFFICER_ALREADY_ACTED: return "officer_already_acted";
        case SIM_REFUSAL_OFFICER_BUSY:          return "officer_busy";
        case SIM_REFUSAL_NO_ACTION_POINTS:      return "no_action_points";
        case SIM_REFUSAL_NO_GOLD:               return "no_gold";
        case SIM_REFUSAL_LIMIT_REACHED:         return "limit_reached";
        case SIM_REFUSAL_TOO_MANY_OFFICERS:     return "too_many_officers";
        case SIM_REFUSAL_NO_AMOUNT:             return "no_amount";
        case SIM_REFUSAL_NO_GRAIN:              return "no_grain";
        case SIM_REFUSAL_NEEDS_TECHNIQUE:       return "needs_technique";
        case SIM_REFUSAL_NO_TECHNIQUE_POINTS:   return "no_technique_points";
        case SIM_REFUSAL_WEAPON_REPLACED:       return "weapon_replaced";
        case SIM_REFUSAL_NOT_MERGEABLE:         return "not_mergeable";
        case SIM_REFUSAL_GAME_OVER:             return "game_over";
        case SIM_REFUSAL_NO_DISASTER:           return "no_disaster";
        case SIM_REFUSAL_NO_TECHNIQUE:          return "no_technique";
        case SIM_REFUSAL_NO_UNIT:               return "no_unit";
        case SIM_REFUSAL_UNIT_ACTED:            return "unit_acted";
        case SIM_REFUSAL_UNREACHABLE:           return "unreachable";
        case SIM_REFUSAL_NO_ROOM:               return "no_room";
        case SIM_REFUSAL_NO_SOLDIERS:           return "no_soldiers";
        case SIM_REFUSAL_NO_WEAPONS:            return "no_weapons";
        case SIM_REFUSAL_NOT_ADJACENT:          return "not_adjacent";
        default:                                return "?";
    }
}

#include "sim/turn.cpp"
#include "sim/develop.cpp"
#include "sim/economy.cpp"
#include "sim/city_commands.cpp"
#include "sim/research.cpp"
#include "sim/merge.cpp"
#include "sim/disasters.cpp"
#include "sim/outcome.cpp"
#include "sim/units.cpp"
