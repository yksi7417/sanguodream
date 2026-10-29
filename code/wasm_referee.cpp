/* The referee of the shared games (docs/MULTIPLAYER.md, MP6): a headless
   engine for node. The room server starts it (web/referee.py) when it must
   decide a desync of two players. The referee loads a start image or a
   snapshot of the room, applies each step of the log after it with
   step_apply, and writes the checksum after each step.

   "build_web.bat referee" builds build\referee.js and build\referee.wasm.
   The referee reads the files of the content pack from ../data/<pack>,
   relative to the current folder, through NODERAWFS.

   The input on stdin, little-endian:
     0  4 bytes  "HXRF"
     4  u32      the version of the input (REFEREE_VERSION)
     8  u32      the flags: REFEREE_PLACEHOLDER_MAP for the map of the tests
    12  u32      the capacity of the ledger; 0 for the capacity of the game
    16  u32      the first step: 0 for the start image, else the step after the snapshot
    20  u32      the size of the name of the pack, then the name
        u32      the number of the human factions, then one u8 force for each.
                 After a start image, the game makes these factions human and the
                 others a faction of the AI (ui_net_join). After a snapshot, nothing.
        u32      the size of the image, then the image
        u32      the number of the steps, then for each step a u32 size and the bytes
   The output on stdout: one line "<step> <checksum>" for each step, then
   "done <count>". A failure gives one line "error <reason>" and exit code 1. */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>   /* a native build for a check: stdin in binary mode */
#endif

#include "base.h"
#include "ecs/ecs.h"
#include "ecs/ecs.cpp"
#include "sim/sim.h"
#include "sim/sim.cpp"
#include "engine/engine.h"
#include "engine/engine.cpp"

#define REFEREE_VERSION         1
#define REFEREE_PLACEHOLDER_MAP 1u
#define REFEREE_PERMANENT_SIZE  Megabytes(96)   /* the game uses approximately 17 MB of its permanent block */
#define REFEREE_TRANSIENT_SIZE  Megabytes(48)
#define REFEREE_PATH_SIZE       260

struct referee_input
{
    const u8 *at;
    const u8 *end;
    b32       ok;
};

internal u32
referee_u32(referee_input *input)
{
    if (input->end - input->at < 4) { input->ok = false; return 0; }
    u32 value = (u32)input->at[0] | (u32)input->at[1] << 8 | (u32)input->at[2] << 16 | (u32)input->at[3] << 24;
    input->at += 4;
    return value;
}

internal const u8 *
referee_bytes(referee_input *input, u32 size)
{
    if ((u64)(input->end - input->at) < size) { input->ok = false; return 0; }
    const u8 *bytes = input->at;
    input->at += size;
    return bytes;
}

internal PLATFORM_READ_ENTIRE_FILE(referee_read_entire_file)
{
    read_file_result result = {};
    FILE *file = fopen(path, "rb");
    if (!file) return result;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size >= 0 && (u64)size + 1 <= arena->size - arena->used)
    {
        u8 *contents = (u8 *)arena_push_size(arena, (u64)size + 1);
        if (fread(contents, 1, (size_t)size, file) == (size_t)size)
        {
            contents[size]  = 0;
            result.size     = (u64)size;
            result.contents = contents;
        }
    }
    fclose(file);
    return result;
}

internal PLATFORM_WRITE_ENTIRE_FILE(referee_write_entire_file)
{
    (void)path; (void)data; (void)size;
    return false;   /* the referee writes no save */
}

/* All of stdin in one block of the heap. */
internal u8 *
referee_read_stdin(u64 *size)
{
    u64 capacity = Megabytes(1), used = 0;
    u8 *data = (u8 *)malloc(capacity);
    while (data)
    {
        if (used == capacity)
        {
            capacity *= 2;
            data = (u8 *)realloc(data, capacity);
            if (!data) break;
        }
        size_t got = fread(data + used, 1, (size_t)(capacity - used), stdin);
        if (!got) break;
        used += got;
    }
    *size = used;
    return data;
}

internal int
referee_fail(const char *reason)
{
    printf("error %s\n", reason);
    return 1;
}

int
main(void)
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    u64 size = 0;
    u8 *data = referee_read_stdin(&size);
    if (!data) return referee_fail("no memory for the input");
    referee_input input = { data, data + size, true };
    const u8 *magic = referee_bytes(&input, 4);
    if (!magic || memcmp(magic, "HXRF", 4) != 0 || referee_u32(&input) != REFEREE_VERSION) return referee_fail("bad input");
    u32 flags        = referee_u32(&input);
    u32 ledger       = referee_u32(&input);
    u32 first_step   = referee_u32(&input);
    u32 pack_size    = referee_u32(&input);
    const u8 *pack   = referee_bytes(&input, pack_size);
    u32 human_count  = referee_u32(&input);
    const u8 *humans = referee_bytes(&input, human_count);
    u32 image_size   = referee_u32(&input);
    const u8 *image  = referee_bytes(&input, image_size);
    u32 step_count   = referee_u32(&input);
    if (!input.ok || !pack_size || pack_size >= CONTENT_KEY_SIZE) return referee_fail("bad input");

    char folder[REFEREE_PATH_SIZE];
    snprintf(folder, sizeof(folder), "%s/%.*s", HEXDOM_DATA_ROOT, (int)pack_size, (const char *)pack);
    memory_arena permanent, transient;
    void *permanent_block = malloc(REFEREE_PERMANENT_SIZE);
    void *transient_block = malloc(REFEREE_TRANSIENT_SIZE);
    if (!permanent_block || !transient_block) return referee_fail("no memory for the engine");
    arena_init(&permanent, permanent_block, REFEREE_PERMANENT_SIZE);
    arena_init(&transient, transient_block, REFEREE_TRANSIENT_SIZE);
    engine_config config = engine_default_config();
    config.read_entire_file  = referee_read_entire_file;
    config.write_entire_file = referee_write_entire_file;
    config.content_folder    = folder;
    if (flags & REFEREE_PLACEHOLDER_MAP) config.use_pack_map = false;
    if (ledger) config.ledger_capacity = ledger;
    engine_api api = engine_make_api(engine_create(&permanent, &transient, config));
    void *e = api.engine;
    if (!api.load_start_image(e, image, image_size)) return referee_fail("the image does not load");
    if (first_step == 0)
    {
        /* The setters of the join of a start image (ui_net_join). */
        for (u32 index = 0; index < api.faction_count(e); ++index)
        {
            u8 force = api.faction_at(e, index).force_id;
            api.set_faction_human(e, force, human_count && memchr(humans, force, human_count) != 0);
        }
    }
    api.multiplayer_begin(e, ENGINE_NO_FACTION, false);
    if (first_step) api.multiplayer_first_step(e, first_step);
    for (u32 index = 0; index < step_count; ++index)
    {
        u32 step_size = referee_u32(&input);
        const u8 *bytes = referee_bytes(&input, step_size);
        if (!input.ok) return referee_fail("a short step");
        engine_step_apply_result applied = api.step_apply(e, bytes, step_size);
        if (applied.status != ENGINE_STEP_APPLIED) return referee_fail("a step does not apply");
        printf("%u %016llx\n", first_step + index, (unsigned long long)applied.checksum);
    }
    printf("done %u\n", step_count);
    return 0;
}
