#include "sim/sim.h"
#include "sim/save_format.cpp"
#include "sim/scenario_format.cpp"
#include "sim/content.cpp"
#include "sim/map.cpp"

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
    sim->settings.ability_change = true;   /* M7: 能力變動 and the historical lifespan (officers-and-relations.md) */
    sim->settings.lifespan       = true;
    sim->settings.ai             = true;    /* M8: the computer factions act, and 事件 is on */
    sim->settings.events         = true;
    sim->settings.female_officers = true;   /* M9: the defaults of a new game */
    sim->settings.strong_plots    = true;
    sim->settings.relations       = true;
    sim->settings.duel_mode       = DUEL_MODE_MANUAL;
    sim->settings.difficulty      = DIFFICULTY_NORMAL;
    sim->duel   = {};
    sim->debate = {};
    for (u32 index = 0; index < SAVE_ITEM_COUNT; ++index) sim->items[index] = {};
    for (u32 force = 0; force < SIM_FORCE_CAPACITY; ++force) sim->adviser[force] = SIM_NO_OFFICER;   /* M8 */
    sim->pending = {};
    sim_reset_pk(sim);   /* M9 */
    sim->scenario_number = SIM_NO_SCENARIO;
    for (u32 row = 0; row < CONTENT_HISTORY_CAPACITY; ++row) sim->history_turn[row] = 0;
    sim->emperor = {};
    sim->ruleset.combat_damage = sim_rule_combat_damage;   /* the generic rule; a pack can set its own */
    sim->ruleset.loyalty_drift = sim_monthly_loyalty_loss;
    sim->building_count = 0;
    sim->fire           = 0;
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

/* The size of the map: the size of map\hexes.tsv of the pack (M10), or the
   size of the placeholder map that the caller gives. */
internal void
sim_init_map(sim_state *sim, memory_arena *arena, u32 width, u32 height, u8 default_cost)
{
    hex_map_init(&sim->map, arena, width, height, default_cost);
    /* The working memory of a move range: six arrays and a heap for each
       cell, with room. */
    sim->scratch = arena_sub(arena, (u64)width * height * 128 + Kilobytes(64));
    sim->fire    = PushArray(arena, (u64)width * height, u8);   /* the turns of fire of each cell (M6) */
    /* M10: the layers of the real map; map.cpp fills them when the pack has a map. */
    sim_map_layers *layers = &sim->map_layers;
    *layers = {};
    layers->region = PushArray(arena, (u64)width * height, u8);
    layers->trap   = PushArray(arena, (u64)width * height, u8);
    layers->flags  = PushArray(arena, (u64)width * height, u8);
    layers->height = PushArray(arena, (u64)width * height, u8);
    for (u32 cell = 0; cell < width * height; ++cell) layers->region[cell] = SIM_MAP_NO_REGION;
    for (u32 region = 0; region < SIM_MAP_REGION_CAPACITY; ++region) layers->base_city[region] = SIM_NO_CITY;
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
   order. Emits one SIM_EVENT_SAVE_LOADED. The sim turn does not change:
   the date comes from the turn counter of the header (G2-001).

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
            /* A duel or a debate that runs ends by the computer rule first (M7). */
            if (sim->duel.active) sim_duel_auto(sim, command.id, events);
            if (sim->debate.active)
            {
                sim_debate_auto(sim, command.id, events);
                sim_command finish = command;
                finish.mini.action = SIM_DEBATE_FINISH;
                finish.mini.value  = 0;
                sim_debate_action(sim, finish, events);
            }
            if (sim->pending.active) sim_resolve_event(sim, 0, command.id, events);   /* M8: an event that waits takes its first choice */
            sim_end_faction_turn(sim, command.id, events);
            break;
        case SIM_COMMAND_SEARCH:
        case SIM_COMMAND_RECRUIT:
        case SIM_COMMAND_REWARD:
        case SIM_COMMAND_GIVE_ITEM:
        case SIM_COMMAND_APPOINT:
        case SIM_COMMAND_MATCHMAKE:
        case SIM_COMMAND_RELEASE:
        case SIM_COMMAND_EXILE:        sim_court_command(sim, command, events);        break;
        case SIM_COMMAND_DIPLOMACY:    sim_diplomacy(sim, command, events);            break;
        case SIM_COMMAND_FACTION_PLOT: sim_faction_plot(sim, command, events);         break;
        case SIM_COMMAND_DUEL:         sim_start_duel(sim, command, events);           break;
        case SIM_COMMAND_DUEL_ACTION:  sim_duel_action(sim, command, events);          break;
        case SIM_COMMAND_DEBATE:       sim_start_debate_command(sim, command, events); break;
        case SIM_COMMAND_DEBATE_ACTION: sim_debate_action(sim, command, events);       break;
        case SIM_COMMAND_CORPS:         sim_corps_command(sim, command, events);       break;
        case SIM_COMMAND_ADVISER:       sim_adviser_command(sim, command, events);     break;
        case SIM_COMMAND_COUNCIL:       sim_council_command(sim, command, events);     break;
        case SIM_COMMAND_EVENT_CHOICE:  sim_event_choice_command(sim, command, events); break;
        case SIM_COMMAND_TRIGGER_EVENT: sim_trigger_event_command(sim, command, events); break;
        case SIM_COMMAND_CREATE_OFFICER:   sim_create_officer_command(sim, command, events); break;
        case SIM_COMMAND_RESEARCH_ABILITY: sim_research_ability_command(sim, command, events); break;
        case SIM_COMMAND_TRAIN_OFFICER:    sim_train_officer_command(sim, command, events); break;
        case SIM_COMMAND_TRIGGER_HISTORY:  sim_trigger_history_command(sim, command, events); break;
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
        case SIM_COMMAND_DESTROY_TARGET:
        case SIM_COMMAND_DRIVE_TARGET:
        case SIM_COMMAND_FOLLOW:
        case SIM_COMMAND_DELEGATE:     sim_long_order(sim, command, events);   break;   /* orders.cpp */
        case SIM_COMMAND_ATTACK:       sim_attack(sim, command, events);       break;
        case SIM_COMMAND_JOINT_ATTACK: sim_joint_attack(sim, command, events); break;
        case SIM_COMMAND_TACTIC:       sim_use_tactic(sim, command, events);   break;
        case SIM_COMMAND_PLOT:         sim_use_plot(sim, command, events);     break;
        case SIM_COMMAND_BUILD:
        case SIM_COMMAND_REPAIR:
        case SIM_COMMAND_DISMANTLE:    sim_build(sim, command, events);        break;
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
    sim->duel    = {};
    sim->debate  = {};
    for (u32 index = 0; index < SAVE_ITEM_COUNT; ++index) sim->items[index] = {};
    for (u32 force = 0; force < SIM_FORCE_CAPACITY; ++force)
    {
        sim->rank[force]    = 0;
        sim->adviser[force] = SIM_NO_OFFICER;
        for (u32 index = 0; index < SIM_CORPS_CAPACITY; ++index) { sim->corps[force][index] = {}; sim->corps_ap[force][index] = 0; }
    }
    sim->pending = {};
    sim_reset_pk(sim);   /* M9 */
    sim->scenario_number = SIM_NO_SCENARIO;
    for (u32 row = 0; row < CONTENT_HISTORY_CAPACITY; ++row) sim->history_turn[row] = 0;
    for (u32 index = 0; index < CONTENT_EVENT_CAPACITY; ++index) sim->event_turn[index] = 0;
    for (u32 index = 0; index < CONTENT_BASE_COUNT; ++index) sim->bases[index] = {};
    for (u32 index = 0; index < SIM_RELIC_COUNT; ++index) sim->relics[index] = {};
    sim_init_sites(sim);
    /* The natural traps belong to the map and stay; the rest go (M6). */
    for (u32 index = 0; index < sim->building_count; ++index)
    {
        sim_building *building = sim->buildings + index;
        if (building->kind >= sim->content.building_count || !sim->content.buildings[building->kind].natural) building->alive = false;
    }
    if (sim->fire)
    {
        for (u32 cell = 0; cell < (u32)sim->map.width * sim->map.height; ++cell) sim->fire[cell] = 0;
    }
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
    sim_init_officer_state(sim, &officer, 0);   /* M7: the character and no relations; a save sets its relations after */
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
    if (!city.durability) city.durability = sim_city_durability_max(sim, &city);   /* a new base starts whole (M6) */
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
   order. Emits one SIM_EVENT_SAVE_LOADED. The caller then starts the turn
   (sim_load_new_game) or resumes the turn of the player (sim_load_save).

   The calendar is the scenario start of the header plus its turn counter
   (Q-004, G2-001). The ten-day period of the header is 1 for the first
   turn of the month (assumed: every scenario has 1). The sim turn does
   not change.

   Returns false, and changes nothing, when the entities or the tables
   cannot hold the new world. */
internal b32
sim_load_save_world(sim_state *sim, save_file *save, sim_command command, sim_event_sink *events)
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
        officer_component *added = sim_find_officer(sim, (u16)slot);
        if (!added) continue;
        sim_init_officer_state(sim, added, record);   /* M7: character and relations */
        /* G2-035: the loyalty of a save record (+104, hypothesis Q-005). The game shows at most 100. */
        if (record->has_save_fields) added->loyalty = (u8)Minimum(record->loyalty, 100u);
    }
    sim_load_items(sim, save);
    for (u32 index = 0; index < save->city_count; ++index)
    {
        city_record *record = save->cities + index;
        city_component city = sim_city_from_record(record);
        /* G2-012: the morale of the city record (+84). A scenario record has none: city.start_morale (assumed, Q-154). */
        city.morale = record->has_morale ? record->morale : (u8)Minimum(sim->content.rules.city_start_morale, 255u);
        sim_add_city(sim, city);
    }

    u8 day = (u8)(1 + (save->header.scenario_start_ten_day_period > 0 ? save->header.scenario_start_ten_day_period - 1 : 0) *
                      CALENDAR_DAYS_PER_TURN);
    sim->turns.start_date = calendar_make(save->header.scenario_start_year, save->header.scenario_start_month, day);
    /* G2-001: the date is the start date plus the turn counter of the header (save-file.md, 0x07C). */
    sim->turns.date       = calendar_add_turns(sim->turns.start_date, save->header.turn_counter);
    u8 player = sim_player_force(sim, &save->header);
    if (player != SIM_NO_FORCE) sim->turns.human[player] = true;
    sim_init_diplomacy(sim);                      /* M7: the save fields of diplomacy are not decoded (Q-180) */
    sim->scenario_number = save->header.scenario_number;   /* M9: the history.tsv clause scenario */
    sim_init_emperor(sim);                        /* M8: the emperor, the bases and the ruins and temples */
    sim_update_ranks(sim, true, command.id, events);
    sim_init_bases(sim);
    sim_load_save_base_records(sim, save);   /* the owner and the soldiers of each gate and port of a save */
    sim_init_relics(sim);
    sim_load_save_map(sim, save, command.id, events);   /* M11: the facilities, the bases and the units of the save */

    sim_event event = sim_event_make(sim, SIM_EVENT_SAVE_LOADED, command.id);
    event.delta = (s32)sim->officers.count;
    sim_emit(events, event);
    return true;
}

/* A new game from a scenario (engine_api_load_scenario): the world, then
   the start of a turn. The AI factions before the player act, and every
   faction recovers its action points. */
internal b32
sim_load_new_game(sim_state *sim, save_file *save, sim_command command, sim_event_sink *events)
{
    if (!sim_load_save_world(sim, save, command, events)) return false;
    sim_start_turn(sim, command.id, events);
    return true;
}

/* The load of a save, in the middle of the player's turn (G2-001 to
   G2-004; M13 first used it for the replay of a save pair). The original
   saves while the player acts, so the factions before the player already
   acted. This load runs no AI command, starts no delegated corps and
   recovers no action points. It takes from the save:
   - the action points and the technique points of each force (Q-202);
   - the acted flag of each officer (officer +148 bit 0); the units take
     the flag of their leader (save_map.cpp).
   The acting faction is the first human faction of the order. The load
   emits SIM_EVENT_FACTION_TURN_STARTED for it, as the start of a turn does. */
internal b32
sim_load_save(sim_state *sim, save_file *save, sim_command command, sim_event_sink *events)
{
    if (!sim_load_save_world(sim, save, command, events)) return false;
    sim_turn_state *turns = &sim->turns;
    sim_compute_turn_order(sim);
    turns->acting = 0;
    while (turns->acting < turns->order_count && !turns->human[turns->order[turns->acting].force_id]) turns->acting++;
    if (turns->acting == turns->order_count) turns->acting = 0;
    for (u32 index = 0; index < save->force_count && index < SIM_FORCE_CAPACITY; ++index)
    {
        save_force_record *force = save->forces + index;
        if (force->has_action_points) turns->action_points[index] = force->action_points;
        sim->technique_points[index] = force->technique_points;
    }
    for (u32 index = 0; index < sim->officers.count; ++index)
    {
        officer_component *officer = sim->officers.data + index;
        if (officer->officer_id < save->officer_slot_count && save->officers[officer->officer_id].has_acted_this_turn)
        {
            officer->acted_turn = sim->turn + 1;
        }
    }
    if (turns->acting < turns->order_count && turns->human[turns->order[turns->acting].force_id])
    {
        sim_event started = sim_event_make(sim, SIM_EVENT_FACTION_TURN_STARTED, command.id);
        started.faction = turns->order[turns->acting].force_id;
        started.delta   = (s32)turns->acting;
        sim_emit(events, started);
    }
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
        case SIM_EVENT_UNIT_DAMAGED:            return "unit_damaged";
        case SIM_EVENT_COUNTERATTACK:           return "counterattack";
        case SIM_EVENT_CRITICAL_HIT:            return "critical_hit";
        case SIM_EVENT_TACTIC_USED:             return "tactic_used";
        case SIM_EVENT_PLOT_USED:               return "plot_used";
        case SIM_EVENT_MORALE_CHANGED:          return "morale_changed";
        case SIM_EVENT_UNIT_PUSHED:             return "unit_pushed";
        case SIM_EVENT_UNIT_STATE:              return "unit_state";
        case SIM_EVENT_UNIT_DESTROYED:          return "unit_destroyed";
        case SIM_EVENT_OFFICER_CAPTURED:        return "officer_captured";
        case SIM_EVENT_OFFICER_ESCAPED:         return "officer_escaped";
        case SIM_EVENT_FIRE_STARTED:            return "fire_started";
        case SIM_EVENT_FIRE_ENDED:              return "fire_ended";
        case SIM_EVENT_BUILDING_STARTED:        return "building_started";
        case SIM_EVENT_BUILDING_COMPLETED:      return "building_completed";
        case SIM_EVENT_BUILDING_DAMAGED:        return "building_damaged";
        case SIM_EVENT_BUILDING_DESTROYED:      return "building_destroyed";
        case SIM_EVENT_TRAP_TRIGGERED:          return "trap_triggered";
        case SIM_EVENT_BASE_DAMAGED:            return "base_damaged";
        case SIM_EVENT_BASE_SOLDIERS_LOST:      return "base_soldiers_lost";
        case SIM_EVENT_BASE_RECOVERED:          return "base_recovered";
        case SIM_EVENT_BASE_CAPTURED:           return "base_captured";
        case SIM_EVENT_UNIT_LOOT:               return "unit_loot";
        case SIM_EVENT_OFFICER_FOUND:           return "officer_found";
        case SIM_EVENT_GOLD_FOUND:              return "gold_found";
        case SIM_EVENT_OFFICER_RECRUITED:       return "officer_recruited";
        case SIM_EVENT_RECRUIT_FAILED:          return "recruit_failed";
        case SIM_EVENT_LOYALTY_CHANGED:         return "loyalty_changed";
        case SIM_EVENT_ITEM_GIVEN:              return "item_given";
        case SIM_EVENT_POST_APPOINTED:          return "post_appointed";
        case SIM_EVENT_RANK_CHANGED:            return "rank_changed";
        case SIM_EVENT_SWORN:                   return "sworn";
        case SIM_EVENT_MARRIED:                 return "married";
        case SIM_EVENT_CAPTIVE_RELEASED:        return "captive_released";
        case SIM_EVENT_OFFICER_EXILED:          return "officer_exiled";
        case SIM_EVENT_OFFICER_LEFT:            return "officer_left";
        case SIM_EVENT_OFFICER_DIED:            return "officer_died";
        case SIM_EVENT_ABILITY_CHANGED:         return "ability_changed";
        case SIM_EVENT_HEALTH_CHANGED:          return "health_changed";
        case SIM_EVENT_RULER_SUCCEEDED:         return "ruler_succeeded";
        case SIM_EVENT_FRIENDLINESS_CHANGED:    return "friendliness_changed";
        case SIM_EVENT_RELATION_CHANGED:        return "relation_changed";
        case SIM_EVENT_DIPLOMACY_FAILED:        return "diplomacy_failed";
        case SIM_EVENT_FACTION_SURRENDERED:     return "faction_surrendered";
        case SIM_EVENT_CAPTIVE_EXCHANGED:       return "captive_exchanged";
        case SIM_EVENT_AID_ARRIVED:             return "aid_arrived";
        case SIM_EVENT_AID_LEFT:                return "aid_left";
        case SIM_EVENT_UNIT_ORDERED:            return "unit_ordered";
        case SIM_EVENT_UNIT_ORDER_DONE:         return "unit_order_done";
        case SIM_EVENT_FACTION_PLOT:            return "faction_plot";
        case SIM_EVENT_FACTION_FOUNDED:         return "faction_founded";
        case SIM_EVENT_ORDER_DROPPED:           return "order_dropped";
        case SIM_EVENT_DUEL_STARTED:            return "duel_started";
        case SIM_EVENT_DUEL_REFUSED:            return "duel_refused";
        case SIM_EVENT_DUEL_STRIKE:             return "duel_strike";
        case SIM_EVENT_DUEL_SPECIAL:            return "duel_special";
        case SIM_EVENT_DUEL_ENDED:              return "duel_ended";
        case SIM_EVENT_DEBATE_STARTED:          return "debate_started";
        case SIM_EVENT_DEBATE_EXCHANGE:         return "debate_exchange";
        case SIM_EVENT_DEBATE_ENDED:            return "debate_ended";
        case SIM_EVENT_CORPS_CREATED:           return "corps_created";
        case SIM_EVENT_CORPS_CHANGED:           return "corps_changed";
        case SIM_EVENT_CORPS_DISSOLVED:         return "corps_dissolved";
        case SIM_EVENT_ADVISER_APPOINTED:       return "adviser_appointed";
        case SIM_EVENT_EMPEROR_CHANGED:         return "emperor_changed";
        case SIM_EVENT_EVENT_FIRED:             return "event_fired";
        case SIM_EVENT_EVENT_CHOSEN:            return "event_chosen";
        case SIM_EVENT_BASE_INCOME:             return "base_income";
        case SIM_EVENT_BASE_TAKEN:              return "base_taken";
        case SIM_EVENT_AI_PLANNED:              return "ai_planned";
        case SIM_EVENT_OFFICER_CREATED:         return "officer_created";
        case SIM_EVENT_ABILITY_RESEARCH_STARTED: return "ability_research_started";
        case SIM_EVENT_ABILITY_RESEARCHED:      return "ability_researched";
        case SIM_EVENT_TRAINING_STARTED:        return "training_started";
        case SIM_EVENT_OFFICER_TRAINED:         return "officer_trained";
        case SIM_EVENT_HISTORY_FIRED:           return "history_fired";
        case SIM_EVENT_ENDING:                  return "ending";
        case SIM_EVENT_BANDIT_SITE:             return "bandit_site";
        case SIM_EVENT_ROCKFALL_RETURNED:       return "rockfall_returned";
        case SIM_EVENT_SAVE_MAP_LOADED:         return "save_map_loaded";
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
        case SIM_COMMAND_DESTROY_TARGET:  return "destroy_target";
        case SIM_COMMAND_DRIVE_TARGET:    return "drive_target";
        case SIM_COMMAND_FOLLOW:          return "follow";
        case SIM_COMMAND_DELEGATE:        return "delegate";
        case SIM_COMMAND_WAIT:            return "wait";
        case SIM_COMMAND_RETREAT:         return "retreat";
        case SIM_COMMAND_SUPPLY:          return "supply";
        case SIM_COMMAND_ATTACK:          return "attack";
        case SIM_COMMAND_JOINT_ATTACK:    return "joint_attack";
        case SIM_COMMAND_TACTIC:          return "tactic";
        case SIM_COMMAND_PLOT:            return "plot";
        case SIM_COMMAND_BUILD:           return "build";
        case SIM_COMMAND_REPAIR:          return "repair";
        case SIM_COMMAND_DISMANTLE:       return "dismantle";
        case SIM_COMMAND_SEARCH:          return "search";
        case SIM_COMMAND_RECRUIT:         return "recruit";
        case SIM_COMMAND_REWARD:          return "reward";
        case SIM_COMMAND_GIVE_ITEM:       return "give_item";
        case SIM_COMMAND_APPOINT:         return "appoint";
        case SIM_COMMAND_MATCHMAKE:       return "matchmake";
        case SIM_COMMAND_RELEASE:         return "release";
        case SIM_COMMAND_EXILE:           return "exile";
        case SIM_COMMAND_DIPLOMACY:       return "diplomacy";
        case SIM_COMMAND_FACTION_PLOT:    return "faction_plot";
        case SIM_COMMAND_DUEL:            return "duel";
        case SIM_COMMAND_DUEL_ACTION:     return "duel_action";
        case SIM_COMMAND_DEBATE:          return "debate";
        case SIM_COMMAND_DEBATE_ACTION:   return "debate_action";
        case SIM_COMMAND_CORPS:           return "corps";
        case SIM_COMMAND_ADVISER:         return "adviser";
        case SIM_COMMAND_COUNCIL:         return "council";
        case SIM_COMMAND_EVENT_CHOICE:    return "event_choice";
        case SIM_COMMAND_TRIGGER_EVENT:   return "trigger_event";
        case SIM_COMMAND_CREATE_OFFICER:  return "create_officer";
        case SIM_COMMAND_RESEARCH_ABILITY: return "research_ability";
        case SIM_COMMAND_TRAIN_OFFICER:   return "train_officer";
        case SIM_COMMAND_TRIGGER_HISTORY: return "trigger_history";
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
        case SIM_REFUSAL_UNIT_CONFUSED:         return "unit_confused";
        case SIM_REFUSAL_NO_TARGET:             return "no_target";
        case SIM_REFUSAL_NO_MORALE:             return "no_morale";
        case SIM_REFUSAL_NO_APTITUDE:           return "no_aptitude";
        case SIM_REFUSAL_NO_PARTNER:            return "no_partner";
        case SIM_REFUSAL_BAD_TERRAIN:           return "bad_terrain";
        case SIM_REFUSAL_NOT_BUILDABLE:         return "not_buildable";
        case SIM_REFUSAL_NOT_RECRUITABLE:       return "not_recruitable";
        case SIM_REFUSAL_NO_ITEM:               return "no_item";
        case SIM_REFUSAL_NO_POST:               return "no_post";
        case SIM_REFUSAL_POST_TAKEN:            return "post_taken";
        case SIM_REFUSAL_NO_MERIT:              return "no_merit";
        case SIM_REFUSAL_NO_RANK:               return "no_rank";
        case SIM_REFUSAL_NOT_MATCHABLE:         return "not_matchable";
        case SIM_REFUSAL_NO_FACTION:            return "no_faction";
        case SIM_REFUSAL_RELATION:              return "relation";
        case SIM_REFUSAL_AT_PEACE:              return "at_peace";
        case SIM_REFUSAL_BUSY:                  return "busy";
        case SIM_REFUSAL_NO_DUEL:               return "no_duel";
        case SIM_REFUSAL_NO_SPIRIT:             return "no_spirit";
        case SIM_REFUSAL_NOT_CAPTIVE:           return "not_captive";
        case SIM_REFUSAL_NO_CORPS:              return "no_corps";
        case SIM_REFUSAL_RULER_ABSENT:          return "ruler_absent";
        case SIM_REFUSAL_NO_EVENT:              return "no_event";
        case SIM_REFUSAL_NO_CHART_ROW:          return "no_chart_row";
        case SIM_REFUSAL_NO_USES:               return "no_uses";
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
#include "sim/combat.cpp"
#include "sim/field.cpp"
#include "sim/officers.cpp"
#include "sim/diplomacy.cpp"
#include "sim/duel.cpp"
#include "sim/debate.cpp"
#include "sim/rivals.cpp"
#include "sim/bases.cpp"
#include "sim/events.cpp"
#include "sim/ai.cpp"
#include "sim/orders.cpp"
#include "sim/pk.cpp"
#include "sim/history.cpp"
#include "sim/save_map.cpp"
