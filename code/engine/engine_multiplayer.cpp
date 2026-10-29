/* A shared game (docs/MULTIPLAYER.md, MP1). Each browser runs its own
   engine. The engine of the player who acts is the author of the steps of
   that player: it runs each step at once, and the end of the step puts its
   bytes into a queue for the network. Another engine applies the bytes
   with step_apply, and both engines compare the checksum of the replay
   record (docs/REPLAY.md) after the step.

   The bytes of a step:
     0  u8   the version of the wire (ENGINE_STEP_WIRE_VERSION)
     1  u8   engine_step_kind
     2  u8   the force of ENGINE_STEP_HUMAN
     3  u8   the faction that acted before the step; ENGINE_NO_FACTION if none.
             The room server reads it to check the seat of the author.
     4  u32  the value of ENGINE_STEP_HUMAN, little-endian
     8  ...  ENGINE_STEP_COMMAND: the command (sim_wire.cpp)
              ENGINE_STEP_SETTINGS: the fields of ENGINE_STEP_SETTINGS_FIELDS
   Included by engine.cpp. */

#define ENGINE_STEP_WIRE_VERSION 1
#define ENGINE_STEP_WIRE_HEADER  8

/* The fields of engine_settings in the order of the wire, one byte each.
   A new field must go into this list: the check of the size below stops
   the build until it does. */
#define ENGINE_STEP_SETTINGS_FIELDS(F) \
    F(disasters, b32) F(ability_change, b32) F(lifespan, b32) F(ai, b32) F(events, b32) F(long_lifespan, b32) \
    F(female_officers, b32) F(strong_plots, b32) F(relations, b32) F(duel_mode, u8) F(difficulty, u8)
#define ENGINE_STEP_SETTINGS_STRUCT_SIZE 40
static_assert(sizeof(engine_settings) == ENGINE_STEP_SETTINGS_STRUCT_SIZE, "engine_settings changed: update ENGINE_STEP_SETTINGS_FIELDS");

/* ---- the seats ------------------------------------------------------------------ */

/* The engine authors a step for the network: a shared game runs, and the
   step does not come from step_apply. A view of the replay record applies
   the recorded steps again: they are no new steps, and no lock applies. */
internal b32
engine_multiplayer_authors(engine *e)
{
    return e->multiplayer.shared && !e->multiplayer.applying && !e->game_replay.applying;
}

/* A setter outside the steps changes the state of one engine only. Thus
   a shared game locks it. step_apply can still use it for a step. */
internal b32
engine_setter_locked(engine *e)
{
    return engine_multiplayer_authors(e);
}

/* The commands that are no step of a shared game: the loads, the editor
   and the test commands. */
internal b32
engine_multiplayer_shares_kind(sim_command_kind kind)
{
    switch (kind)
    {
        case SIM_COMMAND_NONE:
        case SIM_COMMAND_LOAD_SAVE:
        case SIM_COMMAND_LOAD_NAME_INDEX:
        case SIM_COMMAND_DAMAGE_FACILITY:
        case SIM_COMMAND_START_DISASTER:
        case SIM_COMMAND_TRIGGER_EVENT:
        case SIM_COMMAND_CREATE_OFFICER:
        case SIM_COMMAND_TRIGGER_HISTORY:
            return false;
        default:
            return (u32)kind < SIM_COMMAND_COUNT;
    }
}

/* The local seat acts: its faction acts. The host also acts when no
   faction acts, and when a faction without a player acts. The second
   case comes after a load: the save can stop in the turn of a faction of
   the AI. Thus only the host ends a turn after the last faction. */
internal b32
engine_multiplayer_local_acts(engine *e)
{
    engine_multiplayer_state *mp = &e->multiplayer;
    if (!mp->shared) return true;
    u8 acting = sim_whose_turn(e->sim);
    if (acting == SIM_NO_FORCE || !e->sim->turns.human[acting]) return mp->host;
    return mp->local_force != ENGINE_NO_FACTION && acting == mp->local_force;
}

internal sim_refusal
engine_seat_refusal(engine *e, sim_command_kind kind)
{
    if (!engine_multiplayer_authors(e)) return SIM_REFUSAL_NONE;
    if (!engine_multiplayer_shares_kind(kind)) return SIM_REFUSAL_NOT_YOUR_SEAT;
    return engine_multiplayer_local_acts(e) ? SIM_REFUSAL_NONE : SIM_REFUSAL_NOT_YOUR_SEAT;
}

/* MP4: the AI finishes the faction turn of a faction that is no longer
   human (sim_ai_finish_faction_turn). It is a step of the replay record, so
   a view of the record rebuilds it. */
internal void
engine_ai_finish_turn(engine *e, u8 force)
{
    engine_game_replay_pending pending = engine_game_replay_begin_step(e, true);
    arena_reset(&e->scratch);
    sim_event_sink sink = sim_sink_from_arena(&e->scratch, ENGINE_SINK_CAPACITY);
    sim_ai_finish_faction_turn(e->sim, force, 0, &sink);
    engine_ledger_append(e, &sink);
    engine_game_replay_end_step(e, &pending, ENGINE_STEP_AI_TURN, 0, 0, force, 0);
}

/* ---- the bytes of a step ---------------------------------------------------------- */

internal u32
engine_step_write(u8 kind, const sim_command *command, const engine_settings *settings, u8 force, u32 value,
                  u8 acting, u8 *buffer, u32 capacity)
{
    sim_wire_cursor cursor = { buffer, 0, buffer + capacity, true };
    sim_wire_put(&cursor, ENGINE_STEP_WIRE_VERSION, 1);
    sim_wire_put(&cursor, kind, 1);
    sim_wire_put(&cursor, force, 1);
    sim_wire_put(&cursor, acting, 1);
    sim_wire_put(&cursor, value, 4);
    if (!cursor.ok) return 0;
    if (kind == ENGINE_STEP_COMMAND)
    {
        if (!command) return 0;
        u32 size = sim_wire_write_command(command, cursor.at, (u32)(cursor.end - cursor.at));
        if (!size) return 0;
        cursor.at += size;
    }
    if (kind == ENGINE_STEP_SETTINGS)
    {
        if (!settings) return 0;
#define ENGINE_STEP_PUT_SETTING(member, type) sim_wire_put(&cursor, (u64)settings->member, 1);
        ENGINE_STEP_SETTINGS_FIELDS(ENGINE_STEP_PUT_SETTING)
#undef ENGINE_STEP_PUT_SETTING
    }
    return cursor.ok ? (u32)(cursor.at - buffer) : 0;
}

/* After a step of the local seat (engine_game_replay_end_step): the bytes,
   the number and the checksum go into the queue. A full queue loses the
   step and sets overflow. */
internal void
engine_multiplayer_queue(engine *e, u8 kind, const sim_command *command, const engine_settings *settings, u8 force, u32 value,
                         u8 acting)
{
    engine_multiplayer_state *mp = &e->multiplayer;
    u32 step = mp->step_count++;
    if (mp->queue_count == ENGINE_STEP_QUEUE_CAPACITY)
    {
        mp->overflow = true;
        return;
    }
    u32 slot = (mp->queue_first + mp->queue_count) % ENGINE_STEP_QUEUE_CAPACITY;
    u32 size = engine_step_write(kind, command, settings, force, value, acting, mp->queue_bytes[slot], ENGINE_STEP_WIRE_CAPACITY);
    if (!size)
    {
        mp->overflow = true;
        return;
    }
    mp->queue_sizes[slot]     = size;
    mp->queue_steps[slot]     = step;
    mp->queue_checksums[slot] = engine_game_replay_checksum(e);
    u8 after = sim_whose_turn(e->sim);
    mp->queue_acting[slot]    = after == SIM_NO_FORCE ? (u8)ENGINE_NO_FACTION : after;
    mp->queue_count++;
}

/* ---- engine_api ---------------------------------------------------------------- */

internal void
engine_api_multiplayer_begin(void *opaque, u8 local_force, b32 host)
{
    engine_multiplayer_state *mp = &((engine *)opaque)->multiplayer;
    *mp = {};
    mp->shared      = true;
    mp->host        = host;
    mp->local_force = local_force;
}

internal void
engine_api_multiplayer_end(void *opaque)
{
    engine_multiplayer_state *mp = &((engine *)opaque)->multiplayer;
    *mp = {};
    mp->local_force = ENGINE_NO_FACTION;
}

internal void
engine_api_multiplayer_first_step(void *opaque, u32 step)
{
    engine_multiplayer_state *mp = &((engine *)opaque)->multiplayer;
    if (!mp->shared || mp->step_count || mp->first_step_set) return;
    mp->step_count     = step;
    mp->first_step_set = true;
}

internal engine_multiplayer_info
engine_api_multiplayer_info(void *opaque)
{
    engine *e = (engine *)opaque;
    engine_multiplayer_state *mp = &e->multiplayer;
    engine_multiplayer_info info = {};
    u8 acting = sim_whose_turn(e->sim);
    info.shared       = mp->shared;
    info.host         = mp->host;
    info.local_force  = mp->shared ? mp->local_force : (u8)ENGINE_NO_FACTION;
    info.acting_force = acting == SIM_NO_FORCE ? (u8)ENGINE_NO_FACTION : acting;
    info.local_acts   = engine_multiplayer_local_acts(e);
    info.step_count   = mp->step_count;
    info.outgoing     = mp->queue_count;
    info.overflow     = mp->overflow;
    return info;
}

internal engine_outgoing_step
engine_api_steps_outgoing(void *opaque, u8 *buffer, u32 capacity)
{
    engine_multiplayer_state *mp = &((engine *)opaque)->multiplayer;
    engine_outgoing_step out = {};
    if (!mp->queue_count || !buffer) return out;
    u32 slot = mp->queue_first;
    if (mp->queue_sizes[slot] > capacity) return out;
    memcpy(buffer, mp->queue_bytes[slot], mp->queue_sizes[slot]);
    out.size     = mp->queue_sizes[slot];
    out.step     = mp->queue_steps[slot];
    out.checksum = mp->queue_checksums[slot];
    out.acting   = mp->queue_acting[slot];
    mp->queue_first = (mp->queue_first + 1) % ENGINE_STEP_QUEUE_CAPACITY;
    mp->queue_count--;
    return out;
}

/* Applies a step of another engine. A view of the replay closes first, so
   that the command keeps the id of its author. The sim checks the command
   as for a local command: a command that breaks a rule changes nothing. */
internal engine_step_apply_result
engine_api_step_apply(void *opaque, const u8 *bytes, u32 size)
{
    engine *e = (engine *)opaque;
    engine_multiplayer_state *mp = &e->multiplayer;
    engine_step_apply_result result = {};
    result.checksum = engine_game_replay_checksum(e);
    if (!mp->shared)
    {
        result.status = ENGINE_STEP_NOT_SHARED;
        return result;
    }
    sim_wire_cursor cursor = { 0, bytes, bytes + (bytes ? size : 0), true };
    u8  version = (u8)sim_wire_get(&cursor, 1);
    u8  kind    = (u8)sim_wire_get(&cursor, 1);
    u8  force   = (u8)sim_wire_get(&cursor, 1);
    sim_wire_get(&cursor, 1);
    u32 value   = (u32)sim_wire_get(&cursor, 4);
    result.kind = kind;
    result.status = ENGINE_STEP_BAD_BYTES;
    if (!cursor.ok || version != ENGINE_STEP_WIRE_VERSION || kind >= ENGINE_STEP_KIND_COUNT) return result;

    sim_command command = {};
    engine_settings settings = {};
    if (kind == ENGINE_STEP_COMMAND)
    {
        if (!sim_wire_read_command(&command, cursor.read_at, (u32)(cursor.end - cursor.read_at))) return result;
        if (!engine_multiplayer_shares_kind(command.kind))
        {
            result.status = ENGINE_STEP_KIND_REFUSED;
            return result;
        }
    }
    if (kind == ENGINE_STEP_SETTINGS)
    {
#define ENGINE_STEP_GET_SETTING(member, type) settings.member = (type)sim_wire_get(&cursor, 1);
        ENGINE_STEP_SETTINGS_FIELDS(ENGINE_STEP_GET_SETTING)
#undef ENGINE_STEP_GET_SETTING
        if (!cursor.ok) return result;
    }

    engine_game_replay_return_live(e);
    mp->applying = true;
    switch (kind)
    {
        case ENGINE_STEP_COMMAND:
            /* The author took the id before the step: its counter stood one after the id. */
            e->sim->next_command_id = command.id + 1;
            engine_run(e, command, 0);
            break;
        case ENGINE_STEP_SETTINGS:     engine_api_set_settings(e, settings);                 break;
        case ENGINE_STEP_HUMAN:        engine_api_set_faction_human(e, force, value != 0);  break;
        case ENGINE_STEP_LEDGER_CLEAR: engine_api_ledger_clear(e);                          break;
        case ENGINE_STEP_END_TURN:
            /* A step of the server: the timer of the faction ended. If that faction
               no longer acts, nothing changes, and the step still counts. */
            if (sim_whose_turn(e->sim) == force) engine_run_command(e, SIM_COMMAND_END_TURN, 0);
            break;
        case ENGINE_STEP_AI_TURN:
            /* The AI takes the faction in the same step. With a step between the two
               parts, the host can act for a faction without a player. */
            engine_api_set_faction_human(e, force, false);
            engine_ai_finish_turn(e, force);
            break;
        default: break;
    }
    mp->applying = false;
    mp->step_count++;
    result.status   = ENGINE_STEP_APPLIED;
    result.checksum = engine_game_replay_checksum(e);
    return result;
}

internal u64
engine_api_state_checksum(void *opaque)
{
    return engine_game_replay_checksum((engine *)opaque);
}

internal void
engine_api_set_seed(void *opaque, u64 seed)
{
    engine *e = (engine *)opaque;
    if (engine_setter_locked(e)) return;
    e->sim->rng = seed ? seed : 0x9E3779B97F4A7C15ull;   /* the rule of sim_init: the PRNG has no zero state */
}

internal u32
engine_api_start_image_size(void *opaque)
{
    engine *e = (engine *)opaque;
    return (u32)engine_save_image_size(e, e->ledger_count);
}

internal u32
engine_api_start_image(void *opaque, u8 *buffer, u32 capacity)
{
    engine *e = (engine *)opaque;
    if (!buffer) return 0;
    return (u32)engine_game_replay_take_image(e, buffer, capacity, true);
}

/* The image goes through a copy in the transient arena: the loader reads
   from a buffer of the engine, and the bytes of the caller stay as they are. */
internal b32
engine_api_load_start_image(void *opaque, const u8 *bytes, u32 size)
{
    engine *e = (engine *)opaque;
    if (engine_setter_locked(e) || !bytes || !size) return false;
    memory_arena *arena = e->transient;
    if (arena->size - arena->used < (u64)size + 64) return false;
    u64 mark = arena->used;
    u8 *copy = (u8 *)arena_push_size(arena, size);
    memcpy(copy, bytes, size);
    engine_game_replay_return_live(e);
    b32 loaded = engine_game_replay_load_image(e, copy, size);
    arena->used = mark;
    if (loaded) engine_game_replay_reset(e);   /* the start image is the start of a new record */
    return loaded;
}

internal engine_slot_pack
engine_api_content_pack(void *opaque)
{
    engine *e = (engine *)opaque;
    engine_slot_pack pack = {};
    snprintf(pack.pack, sizeof(pack.pack), "%s", engine_pack_name(e->content_folder));
    return pack;
}

internal void
engine_bind_multiplayer_api(engine_api *api)
{
    api->content_pack      = engine_api_content_pack;
    api->multiplayer_first_step = engine_api_multiplayer_first_step;
    api->start_image_size  = engine_api_start_image_size;
    api->start_image       = engine_api_start_image;
    api->load_start_image  = engine_api_load_start_image;
    api->multiplayer_begin = engine_api_multiplayer_begin;
    api->multiplayer_end   = engine_api_multiplayer_end;
    api->multiplayer_info  = engine_api_multiplayer_info;
    api->steps_outgoing    = engine_api_steps_outgoing;
    api->step_apply        = engine_api_step_apply;
    api->state_checksum    = engine_api_state_checksum;
    api->set_seed          = engine_api_set_seed;
}
