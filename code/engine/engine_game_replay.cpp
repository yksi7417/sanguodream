/* The replay of a game (docs/REPLAY.md). The engine records the start
   state of the game and each step after it. A step is a command that
   went through engine_run, a change of the settings, a change of the
   human factions, or a clear of the ledger. The sim is deterministic: the
   PRNG is in the start state, so the same steps give the same state.

   The start state is an image in the format of a save slot
   (engine_saves.cpp). The engine takes it at the first step after a load,
   so the setters before that step are in it. A setter after it changes
   the state outside the steps. The checksum before the next step shows
   that change, and the step gets the mark "edited".

   A view rebuilds the state after N steps: it loads the start image and
   applies the first N steps again. A step forward from the view applies
   one step. The image of the live game waits in memory while the view is
   open, and close loads it again. A command while a view is open closes
   the view first. Included by engine.cpp. */

#define ENGINE_GAME_REPLAY_MAGIC         0x50525848u   /* "HXRP" */
#define ENGINE_GAME_REPLAY_VERSION       1
#define ENGINE_GAME_REPLAY_STEP_CAPACITY 8192

/* One step as the record and the save file hold it. */
struct engine_game_replay_step_record
{
    sim_command     command;      /* ENGINE_STEP_COMMAND */
    engine_settings settings;     /* ENGINE_STEP_SETTINGS */
    engine_date     date;
    engine_place    place;
    u64             checksum;     /* the state after the step */
    u32             turn;
    u32             first_event;
    u32             event_count;
    u32             value;        /* ENGINE_STEP_HUMAN: 1 for a player */
    u16             officer;
    u8              kind;         /* engine_step_kind */
    u8              force;
    b32             edited;
};

/* The header of the record in a save file, after the ledger of the
   image. The start image and the steps follow it. A record of another
   layout or another step size does not load: the game loads without it
   and starts a new record. */
struct engine_game_replay_section
{
    u32 magic;
    u32 version;
    u32 layout;            /* engine_save_layout() of the build that wrote it */
    u32 step_size;         /* sizeof(engine_game_replay_step_record) */
    u32 step_count;
    u32 first_edit;
    u64 start_size;        /* the bytes of the start image */
    u64 start_checksum;
};

/* ---- the checksum of the state -------------------------------------------------- */

#define ENGINE_GAME_REPLAY_HASH_PRIME 1099511628211ull

internal u64
engine_game_replay_hash(u64 hash, const void *data, u64 size)
{
    const u8 *bytes = (const u8 *)data;
    u64 words = size / sizeof(u64);
    for (u64 index = 0; index < words; ++index)
    {
        u64 word;
        memcpy(&word, bytes + index * sizeof(u64), sizeof(u64));
        hash = (hash ^ word) * ENGINE_GAME_REPLAY_HASH_PRIME;
        hash ^= hash >> 29;
    }
    for (u64 index = words * sizeof(u64); index < size; ++index) hash = (hash ^ bytes[index]) * ENGINE_GAME_REPLAY_HASH_PRIME;
    return hash;
}

internal u64
engine_game_replay_hash_u32(u64 hash, u32 value)
{
    return engine_game_replay_hash(hash, &value, sizeof(value));
}

/* The members of sim_state that make the game. The checksum leaves out
   the pointers, the content of the pack, the names, the map data of the
   pack, the scratch memory and the caches of the travel search. It also
   leaves out the command counter: the api takes the next id before the
   step starts, and a name load takes an id outside the steps. */
#define ENGINE_GAME_REPLAY_HASHED_MEMBERS(X) \
    X(rng) X(turn) X(has_content) X(turns) X(sites) X(units) X(unit_count) X(buildings) X(building_count) \
    X(technique_points) X(techniques) X(research) X(settings) X(outcome) X(officer_index) X(city_index) X(items) X(diplomacy) \
    X(rank) X(duel) X(debate) X(bases) X(corps) X(corps_ap) X(adviser) X(missions) X(emperor) X(relics) X(pending) X(event_turn) \
    X(new_officer_count) X(ability_uses) X(ability_known) X(ability_job) X(scenario_number) X(history_turn) X(ai_records) X(base_stocks)

internal u64
engine_game_replay_checksum(engine *e)
{
    sim_state *sim = e->sim;
    u64 hash = 14695981039346656037ull;
#define ENGINE_GAME_REPLAY_HASH_MEMBER(member) hash = engine_game_replay_hash(hash, &sim->member, sizeof(sim->member));
    ENGINE_GAME_REPLAY_HASHED_MEMBERS(ENGINE_GAME_REPLAY_HASH_MEMBER)
#undef ENGINE_GAME_REPLAY_HASH_MEMBER
    u32 entities = sim->entities.capacity, cells = sim->map.width * sim->map.height;
    u32 counts[] = { sim->entities.alive_count, sim->entities.next_unused, sim->entities.free_count, sim->officers.count, sim->cities.count };
    hash = engine_game_replay_hash(hash, counts, sizeof(counts));
    hash = engine_game_replay_hash(hash, sim->entities.generation, sizeof(u16) * entities);
    hash = engine_game_replay_hash(hash, sim->entities.alive, sizeof(u8) * entities);
    hash = engine_game_replay_hash(hash, sim->entities.free_list, sizeof(u32) * entities);
    hash = engine_game_replay_hash(hash, sim->officers.sparse, sizeof(u32) * entities);
    hash = engine_game_replay_hash(hash, sim->officers.dense_owner, sizeof(entity_id) * sim->officers.count);
    hash = engine_game_replay_hash(hash, sim->officers.data, sizeof(officer_component) * sim->officers.count);
    hash = engine_game_replay_hash(hash, sim->cities.sparse, sizeof(u32) * entities);
    hash = engine_game_replay_hash(hash, sim->cities.dense_owner, sizeof(entity_id) * sim->cities.count);
    hash = engine_game_replay_hash(hash, sim->cities.data, sizeof(city_component) * sim->cities.count);
    hash = engine_game_replay_hash(hash, sim->map.cost, cells);
    hash = engine_game_replay_hash(hash, sim->map.kind, cells);
    hash = engine_game_replay_hash(hash, sim->map.level, cells);
    if (sim->fire) hash = engine_game_replay_hash(hash, sim->fire, cells);
    return engine_game_replay_hash_u32(hash, e->ledger_count);
}

/* ---- the memory and the images ---------------------------------------------- */

internal void
engine_game_replay_reset(engine *e)
{
    engine_game_replay_state *replay = &e->game_replay;
    if (replay->status != ENGINE_GAME_REPLAY_NO_MEMORY) replay->status = ENGINE_GAME_REPLAY_EMPTY;
    replay->viewing          = false;
    replay->applying         = false;
    replay->step_count       = 0;
    replay->view_step        = 0;
    replay->start_size       = 0;
    replay->start_turn       = 0;
    replay->first_edit       = ENGINE_NO_STEP;
    replay->last_clear       = 0;
    replay->checkpoint_count = 0;
    replay->checkpoint_turns = 1;
}

/* Takes the memory of the record from the permanent arena at the first
   need: two images with a full ledger and the steps. Then the
   checkpoints, as many as the arena holds with a reserve for the game. */
#define ENGINE_GAME_REPLAY_RESERVE Megabytes(8)
internal b32
engine_game_replay_reserve(engine *e)
{
    engine_game_replay_state *replay = &e->game_replay;
    if (replay->steps) return true;
    if (replay->status == ENGINE_GAME_REPLAY_NO_MEMORY) return false;
    u64 image = engine_save_image_size(e, e->ledger_capacity);
    u64 steps = sizeof(engine_game_replay_step_record) * (u64)ENGINE_GAME_REPLAY_STEP_CAPACITY;
    memory_arena *arena = e->permanent;
    if (arena->size - arena->used < 2 * image + steps + 64)
    {
        replay->status = ENGINE_GAME_REPLAY_NO_MEMORY;
        return false;
    }
    replay->start          = (u8 *)arena_push_size(arena, image);
    replay->live           = (u8 *)arena_push_size(arena, image);
    replay->image_capacity = image;
    replay->steps          = PushArray(arena, ENGINE_GAME_REPLAY_STEP_CAPACITY, engine_game_replay_step_record);
    u64 checkpoint = engine_save_image_size(e, 0);
    u32 capacity = ENGINE_GAME_REPLAY_CHECKPOINTS;
    while (capacity && arena->size - arena->used < checkpoint * capacity + ENGINE_GAME_REPLAY_RESERVE) capacity /= 2;
    for (u32 index = 0; index < capacity; ++index) replay->checkpoints[index].image = (u8 *)arena_push_size(arena, checkpoint);
    replay->checkpoint_capacity       = capacity;
    replay->checkpoint_image_capacity = checkpoint;
    if (!replay->checkpoint_turns) replay->checkpoint_turns = 1;
    return true;
}

/* Writes the live game into an image buffer; returns its bytes. */
internal u64
engine_game_replay_take_image(engine *e, u8 *buffer, u64 capacity, b32 with_ledger)
{
    engine_save_cursor cursor = { buffer, buffer + capacity, true };
    engine_save_slot slot = {};
    slot.player = ENGINE_NO_FACTION;
    engine_save_write_image(e, &cursor, slot, with_ledger);
    return cursor.ok ? (u64)(cursor.at - buffer) : 0;
}

/* After `steps` steps: a checkpoint if a turn passed and the last
   checkpoint is checkpoint_turns turns back. The checkpoints only grow
   forward. When the memory is full, every second checkpoint goes and the
   distance doubles, so the checkpoints cover the whole game. */
internal void
engine_game_replay_note_checkpoint(engine *e, u32 steps, u32 turn_before)
{
    engine_game_replay_state *replay = &e->game_replay;
    if (!replay->checkpoint_capacity || e->sim->turn == turn_before) return;
    engine_game_replay_checkpoint *last = replay->checkpoint_count ? replay->checkpoints + replay->checkpoint_count - 1 : 0;
    if (last && steps <= last->step) return;
    if (e->sim->turn < (last ? last->turn : replay->start_turn) + replay->checkpoint_turns) return;
    if (replay->checkpoint_count == replay->checkpoint_capacity)
    {
        engine_game_replay_checkpoint old[ENGINE_GAME_REPLAY_CHECKPOINTS];
        memcpy(old, replay->checkpoints, sizeof(old));
        u32 kept = replay->checkpoint_capacity / 2;
        for (u32 index = 0; index < kept; ++index)
        {
            replay->checkpoints[index]              = old[2 * index + 1];
            replay->checkpoints[kept + index]       = {};
            replay->checkpoints[kept + index].image = old[2 * index].image;
        }
        replay->checkpoint_count  = kept;
        replay->checkpoint_turns *= 2;
        last = replay->checkpoints + kept - 1;
        if (e->sim->turn < last->turn + replay->checkpoint_turns) return;
    }
    engine_game_replay_checkpoint *checkpoint = replay->checkpoints + replay->checkpoint_count;
    checkpoint->size = engine_game_replay_take_image(e, checkpoint->image, replay->checkpoint_image_capacity, false);
    if (!checkpoint->size) return;
    checkpoint->step         = steps;
    checkpoint->turn         = e->sim->turn;
    checkpoint->ledger_count = e->ledger_count;
    replay->checkpoint_count++;
}

/* Loads an image into the live sim and the ledger. The names stay: they
   are no part of the game, and a name index can load after the start.
   The map version stays if the cells are the same, so that the renderer
   does not build the map again for each step. */
internal b32
engine_game_replay_load_image(engine *e, u8 *image, u64 size)
{
    sim_state *sim = e->sim;
    u32 cells = sim->map.width * sim->map.height;
    u64 cells_before = engine_game_replay_hash(engine_game_replay_hash(engine_game_replay_hash(0, sim->map.cost, cells), sim->map.kind, cells), sim->map.level, cells);
    u32 map_version = sim->map.version;
    memory_arena *arena = e->transient;
    u64 mark = arena->used;
    name_index *names = PushStruct(arena, name_index);
    *names = sim->names;
    b32 has_names = sim->has_names;
    engine_save_cursor cursor = { image, image + size, true };
    engine_save_header header;
    engine_save_status status = engine_save_read_header(e, &header, &cursor);
    if (status == ENGINE_SAVE_OK) status = engine_save_load_image(e, &header, &cursor);
    sim->names     = *names;
    sim->has_names = has_names;
    arena->used    = mark;
    u64 cells_after = engine_game_replay_hash(engine_game_replay_hash(engine_game_replay_hash(0, sim->map.cost, cells), sim->map.kind, cells), sim->map.level, cells);
    if (cells_after == cells_before) sim->map.version = map_version;
    return status == ENGINE_SAVE_OK;
}

/* Loads a checkpoint. The ledger of the image of the live game holds the
   events up to the checkpoint, if no ledger_clear came after it. */
internal b32
engine_game_replay_load_checkpoint(engine *e, const engine_game_replay_checkpoint *checkpoint)
{
    engine_game_replay_state *replay = &e->game_replay;
    engine_save_header live;
    memcpy(&live, replay->live, sizeof(live));
    u64 events = sizeof(sim_event) * (u64)live.ledger_count;
    if (checkpoint->ledger_count > live.ledger_count || events > replay->live_size) return false;
    if (!engine_game_replay_load_image(e, checkpoint->image, checkpoint->size)) return false;
    memcpy(e->ledger, replay->live + replay->live_size - events, sizeof(sim_event) * (u64)checkpoint->ledger_count);
    e->ledger_count = checkpoint->ledger_count;
    return true;
}

/* Back to the live game, if a view is open. The speakers of the recent
   commands come back too: the view wrote the speakers of its steps. */
internal void
engine_game_replay_return_live(engine *e)
{
    engine_game_replay_state *replay = &e->game_replay;
    if (!replay->viewing || replay->applying) return;
    engine_game_replay_load_image(e, replay->live, replay->live_size);
    memcpy(e->speaker_commands, replay->live_speaker_commands, sizeof(e->speaker_commands));
    memcpy(e->speaker_officers, replay->live_speaker_officers, sizeof(e->speaker_officers));
    replay->viewing   = false;
    replay->view_step = replay->step_count;
}

/* ---- the record of the steps ------------------------------------------------- */

/* Before a step: closes a view, takes the start image at the first
   command, and notes the facts of the step. A change of the settings or
   of the seats before the first command is a part of the start state,
   not a step. A step that the replay applies again is not recorded
   again. */
internal engine_game_replay_pending
engine_game_replay_begin_step(engine *e, b32 command)
{
    engine_game_replay_state *replay = &e->game_replay;
    engine_game_replay_pending pending = {};
    if (replay->applying) return pending;
    engine_game_replay_return_live(e);
    if (replay->status == ENGINE_GAME_REPLAY_EMPTY && !command) return pending;
    if (replay->status != ENGINE_GAME_REPLAY_EMPTY && replay->status != ENGINE_GAME_REPLAY_RECORDING) return pending;
    if (!engine_game_replay_reserve(e)) return pending;
    if (replay->status == ENGINE_GAME_REPLAY_EMPTY)
    {
        replay->start_size = engine_game_replay_take_image(e, replay->start, replay->image_capacity, true);
        if (!replay->start_size) return pending;
        replay->start_turn     = e->sim->turn;
        replay->start_checksum = replay->last_checksum = engine_game_replay_checksum(e);
        replay->status = ENGINE_GAME_REPLAY_RECORDING;
    }
    if (replay->step_count >= ENGINE_GAME_REPLAY_STEP_CAPACITY)
    {
        replay->status = ENGINE_GAME_REPLAY_FULL;
        return pending;
    }
    pending.record      = true;
    pending.edited      = engine_game_replay_checksum(e) != replay->last_checksum;
    u8 force            = sim_whose_turn(e->sim);
    pending.force       = force == SIM_NO_FORCE ? (u8)ENGINE_NO_FACTION : force;
    pending.turn        = e->sim->turn;
    pending.date        = engine_api_date(e);
    pending.first_event = e->ledger_count;
    return pending;
}

/* After a step: stores it with its events, its place and the checksum. */
internal void
engine_game_replay_end_step(engine *e, const engine_game_replay_pending *pending, u8 kind, const sim_command *command,
                            const engine_settings *settings, u8 force, u32 value)
{
    if (!pending->record) return;
    engine_game_replay_state *replay = &e->game_replay;
    engine_game_replay_step_record *step = replay->steps + replay->step_count;
    *step = {};
    step->kind        = kind;
    step->force       = force;
    step->value       = value;
    step->turn        = pending->turn;
    step->date        = pending->date;
    step->edited      = pending->edited;
    step->officer     = ENGINE_NO_OFFICER;
    step->first_event = Minimum(pending->first_event, e->ledger_count);
    step->event_count = e->ledger_count - step->first_event;
    step->place.kind  = ENGINE_PLACE_NONE;
    if (command)
    {
        step->command = *command;
        step->officer = engine_speaker_of_command(e, command->id);
    }
    if (settings) step->settings = *settings;
    for (u32 index = step->first_event; index < e->ledger_count; ++index)
    {
        engine_event_report report = engine_api_event_report(e, index);
        if (report.place.kind == ENGINE_PLACE_NONE) continue;
        step->place = report.place;
        break;
    }
    step->checksum = replay->last_checksum = engine_game_replay_checksum(e);
    if (step->edited && replay->first_edit == ENGINE_NO_STEP) replay->first_edit = replay->step_count;
    replay->step_count++;
    replay->view_step = replay->step_count;
    if (kind == ENGINE_STEP_LEDGER_CLEAR) replay->last_clear = replay->step_count;
    engine_game_replay_note_checkpoint(e, replay->step_count, pending->turn);
}

/* Applies one recorded step to the state. */
internal void
engine_game_replay_apply(engine *e, const engine_game_replay_step_record *step)
{
    engine_game_replay_state *replay = &e->game_replay;
    replay->applying = true;
    switch (step->kind)
    {
        case ENGINE_STEP_COMMAND:
            /* The api took the id of the command before engine_run: the
               counter stood one after the id when the command ran. */
            e->sim->next_command_id = step->command.id + 1;
            engine_run(e, step->command, 0);
            break;
        case ENGINE_STEP_SETTINGS:     engine_api_set_settings(e, step->settings); break;
        case ENGINE_STEP_HUMAN:        engine_api_set_faction_human(e, step->force, step->value != 0); break;
        case ENGINE_STEP_LEDGER_CLEAR: engine_api_ledger_clear(e); break;
        default: break;
    }
    replay->applying = false;
}

/* ---- the save file ---------------------------------------------------------- */

internal u64
engine_game_replay_saved_size(engine *e)
{
    engine_game_replay_state *replay = &e->game_replay;
    if (!replay->start_size) return 0;
    return sizeof(engine_game_replay_section) + replay->start_size + sizeof(engine_game_replay_step_record) * (u64)replay->step_count;
}

internal void
engine_game_replay_write(engine *e, engine_save_cursor *cursor)
{
    engine_game_replay_state *replay = &e->game_replay;
    if (!replay->start_size) return;
    engine_game_replay_section section = {};
    section.magic          = ENGINE_GAME_REPLAY_MAGIC;
    section.version        = ENGINE_GAME_REPLAY_VERSION;
    section.layout         = engine_save_layout();
    section.step_size      = (u32)sizeof(engine_game_replay_step_record);
    section.step_count     = replay->step_count;
    section.first_edit     = replay->first_edit;
    section.start_size     = replay->start_size;
    section.start_checksum = replay->start_checksum;
    engine_save_put(cursor, &section, sizeof(section));
    engine_save_put(cursor, replay->start, replay->start_size);
    engine_save_put(cursor, replay->steps, sizeof(engine_game_replay_step_record) * (u64)replay->step_count);
}

/* After a load: the record of the file, or a new record. */
internal void
engine_game_replay_read(engine *e, engine_save_cursor *cursor)
{
    engine_game_replay_reset(e);
    engine_game_replay_state *replay = &e->game_replay;
    engine_game_replay_section section;
    if (!cursor->ok || (u64)(cursor->end - cursor->at) < sizeof(section)) return;
    engine_save_get(cursor, &section, sizeof(section));
    if (section.magic != ENGINE_GAME_REPLAY_MAGIC || section.version != ENGINE_GAME_REPLAY_VERSION ||
        section.layout != engine_save_layout() || section.step_size != sizeof(engine_game_replay_step_record) ||
        section.step_count > ENGINE_GAME_REPLAY_STEP_CAPACITY) return;
    if (!engine_game_replay_reserve(e) || section.start_size > replay->image_capacity) return;
    u64 steps = sizeof(engine_game_replay_step_record) * (u64)section.step_count;
    if ((u64)(cursor->end - cursor->at) < section.start_size + steps) return;
    engine_save_get(cursor, replay->start, section.start_size);
    engine_save_get(cursor, replay->steps, steps);
    replay->start_size     = section.start_size;
    replay->start_checksum = section.start_checksum;
    replay->step_count     = section.step_count;
    replay->view_step      = section.step_count;
    replay->first_edit     = section.first_edit;
    replay->last_checksum  = section.step_count ? replay->steps[section.step_count - 1].checksum : section.start_checksum;
    replay->start_turn     = section.step_count ? replay->steps[0].turn : e->sim->turn;
    for (u32 index = 0; index < section.step_count; ++index)
    {
        if (replay->steps[index].kind == ENGINE_STEP_LEDGER_CLEAR) replay->last_clear = index + 1;
    }
    replay->status         = (u8)(section.step_count < ENGINE_GAME_REPLAY_STEP_CAPACITY ? ENGINE_GAME_REPLAY_RECORDING : ENGINE_GAME_REPLAY_FULL);
}

/* ---- engine_api -------------------------------------------------------------- */

internal engine_game_replay_info
engine_api_game_replay_info(void *opaque)
{
    engine *e = (engine *)opaque;
    engine_game_replay_state *replay = &e->game_replay;
    engine_game_replay_info info = {};
    info.status     = replay->status;
    info.viewing    = replay->viewing;
    info.step_count = replay->step_count;
    info.view_step  = replay->viewing ? replay->view_step : replay->step_count;
    info.version    = ENGINE_GAME_REPLAY_VERSION;
    info.first_edit = replay->first_edit;
    if (replay->start_size >= sizeof(engine_save_header) + sizeof(sim_state))
    {
        /* The image of the start: the header, the member table, then sim_state with the PRNG first. */
        engine_save_header header;
        memcpy(&header, replay->start, sizeof(header));
        u64 at = sizeof(header) + sizeof(engine_save_member) * (u64)header.member_count;
        if (at + sizeof(u64) <= replay->start_size) memcpy(&info.seed, replay->start + at, sizeof(u64));
        info.start_turn = header.slot.turn;
    }
    if (replay->step_count) info.start_date = replay->steps[0].date;
    if (replay->step_count) info.start_turn = replay->steps[0].turn;
    return info;
}

internal engine_game_replay_step
engine_api_game_replay_step_at(void *opaque, u32 index)
{
    engine *e = (engine *)opaque;
    engine_game_replay_state *replay = &e->game_replay;
    engine_game_replay_step result = {};
    result.index = ENGINE_NO_STEP;
    if (index >= replay->step_count) return result;
    const engine_game_replay_step_record *step = replay->steps + index;
    result.index       = index;
    result.kind        = step->kind;
    result.command     = (u8)step->command.kind;
    result.force       = step->force;
    result.turn        = step->turn;
    result.date        = step->date;
    result.officer     = step->officer;
    result.place       = step->place;
    result.first_event = step->first_event;
    result.event_count = step->event_count;
    result.value       = step->value;
    result.edited      = step->edited;
    switch (step->kind)
    {
        case ENGINE_STEP_COMMAND:
            result.key        = sim_command_name(step->command.kind);
            result.command_id = step->command.id;
            break;
        case ENGINE_STEP_SETTINGS:     result.key = "settings";     break;
        case ENGINE_STEP_HUMAN:        result.key = "human";        break;
        case ENGINE_STEP_LEDGER_CLEAR: result.key = "ledger_clear"; break;
        default:                       result.key = "none";         break;
    }
    return result;
}

/* Rebuilds the view from the last checkpoint at or before the step, or
   from the start image. A checkpoint before a ledger_clear step has no
   ledger in the image of the live game, so it does not count. */
internal b32
engine_game_replay_rebuild_base(engine *e, u32 step)
{
    engine_game_replay_state *replay = &e->game_replay;
    u32 best = replay->checkpoint_count;
    for (u32 index = 0; index < replay->checkpoint_count; ++index)
    {
        engine_game_replay_checkpoint *checkpoint = replay->checkpoints + index;
        if (checkpoint->step <= step && checkpoint->step >= replay->last_clear) best = index;
    }
    u32 base = best < replay->checkpoint_count ? replay->checkpoints[best].step : 0;
    if (replay->view_step <= step && replay->view_step >= base) return true;   /* forward from the view */
    if (best < replay->checkpoint_count && engine_game_replay_load_checkpoint(e, replay->checkpoints + best))
    {
        replay->view_step = base;
        return true;
    }
    if (!engine_game_replay_load_image(e, replay->start, replay->start_size)) return false;
    replay->view_step = 0;
    return true;
}

/* A view applies the recorded steps from the nearest image before the
   step. A step forward applies the next step to the view. */
internal engine_game_replay_view
engine_api_game_replay_view(void *opaque, u32 step)
{
    engine *e = (engine *)opaque;
    engine_game_replay_state *replay = &e->game_replay;
    engine_game_replay_view view = {};
    view.step = step;
    if (!replay->start_size || step > replay->step_count || replay->applying) return view;
    if (!replay->viewing)
    {
        replay->live_size = engine_game_replay_take_image(e, replay->live, replay->image_capacity, true);
        if (!replay->live_size) return view;
        memcpy(replay->live_speaker_commands, e->speaker_commands, sizeof(e->speaker_commands));
        memcpy(replay->live_speaker_officers, e->speaker_officers, sizeof(e->speaker_officers));
        replay->viewing   = true;
        replay->view_step = replay->step_count;
    }
    if (!engine_game_replay_rebuild_base(e, step)) return view;
    while (replay->view_step < step)
    {
        u32 turn_before = e->sim->turn;
        engine_game_replay_apply(e, replay->steps + replay->view_step++);
        engine_game_replay_note_checkpoint(e, replay->view_step, turn_before);   /* a loaded record gets its checkpoints here */
    }
    u64 expected = step ? replay->steps[step - 1].checksum : replay->start_checksum;
    view.ok      = true;
    view.matches = engine_game_replay_checksum(e) == expected;
    return view;
}

internal void
engine_api_game_replay_close(void *opaque)
{
    engine_game_replay_return_live((engine *)opaque);
}

internal u32
engine_api_game_replay_turn_step(void *opaque, u32 turn)
{
    engine_game_replay_state *replay = &((engine *)opaque)->game_replay;
    for (u32 index = 0; index < replay->step_count; ++index)
    {
        if (replay->steps[index].turn >= turn) return index;
    }
    return replay->step_count;
}

internal void
engine_bind_game_replay_api(engine_api *api)
{
    api->game_replay_info      = engine_api_game_replay_info;
    api->game_replay_step_at   = engine_api_game_replay_step_at;
    api->game_replay_view      = engine_api_game_replay_view;
    api->game_replay_close     = engine_api_game_replay_close;
    api->game_replay_turn_step = engine_api_game_replay_turn_step;
}
