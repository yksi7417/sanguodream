/* Unit tests of code/sim/sim_wire.cpp: the step codec of a shared game
   (docs/MULTIPLAYER.md, MP1). A command of each kind goes to bytes and
   back with each field the same. A short buffer gives no command. */

/* Fills each field of the command with a value from a counter. Each byte
   of each value differs, so a field that the codec skips or swaps shows. */
internal void
unit_wire_fill(sim_command *command, u32 kind, u64 *counter)
{
    *command = {};
#define UNIT_WIRE_FILL_FIELD(member, type) command->member = (type)(*counter += 0x0102030405060709ull);
#define UNIT_WIRE_FILL_ARRAY(member, type, count) \
    for (u32 index = 0; index < (count); ++index) command->member[index] = (type)(*counter += 0x0102030405060709ull);
    SIM_WIRE_COMMAND_FIELDS(UNIT_WIRE_FILL_FIELD, UNIT_WIRE_FILL_ARRAY)
#undef UNIT_WIRE_FILL_FIELD
#undef UNIT_WIRE_FILL_ARRAY
    command->kind = (sim_command_kind)kind;
}

/* True if each field of the two commands is the same. */
internal b32
unit_wire_same(const sim_command *a, const sim_command *b)
{
    b32 same = true;
#define UNIT_WIRE_SAME_FIELD(member, type) if (a->member != b->member) same = false;
#define UNIT_WIRE_SAME_ARRAY(member, type, count) \
    for (u32 index = 0; index < (count); ++index) if (a->member[index] != b->member[index]) same = false;
    SIM_WIRE_COMMAND_FIELDS(UNIT_WIRE_SAME_FIELD, UNIT_WIRE_SAME_ARRAY)
#undef UNIT_WIRE_SAME_FIELD
#undef UNIT_WIRE_SAME_ARRAY
    return same;
}

internal void
unit_wire_tests(test_counts *counts)
{
    u8 buffer[2 * SIM_WIRE_COMMAND_SIZE];
    u64 counter = 0;
    b32 round_trip = true, sizes = true;
    for (u32 kind = 0; kind < SIM_COMMAND_COUNT; ++kind)
    {
        sim_command command, back;
        unit_wire_fill(&command, kind, &counter);
        u32 written = sim_wire_write_command(&command, buffer, sizeof(buffer));
        u32 read    = sim_wire_read_command(&back, buffer, written);
        if (written != SIM_WIRE_COMMAND_SIZE || read != written) sizes = false;
        if (!unit_wire_same(&command, &back)) round_trip = false;
    }
    TEST_CHECK(counts, sizes, "each command takes SIM_WIRE_COMMAND_SIZE bytes, and the read takes the same bytes");
    TEST_CHECK(counts, round_trip, "a command of each kind goes to bytes and back with each field the same");
    TEST_CHECK(counts, SIM_WIRE_COMMAND_SIZE < 1024 - 8, "a command and the header of a step fit in 1 KB");

    sim_command move = {};
    move.kind    = SIM_COMMAND_MOVE;
    move.id      = 0x01020304;
    move.unit.to = hex_make(-3, 7);
    sim_wire_write_command(&move, buffer, sizeof(buffer));
    TEST_CHECK(counts, buffer[0] == SIM_COMMAND_MOVE && buffer[1] == 0 && buffer[4] == 0x04 && buffer[5] == 0x03 &&
                       buffer[6] == 0x02 && buffer[7] == 0x01,
               "the kind and the id are little-endian integers at the start of the bytes");
    sim_command back = {};
    sim_wire_read_command(&back, buffer, SIM_WIRE_COMMAND_SIZE);
    TEST_CHECK(counts, back.unit.to.q == -3 && back.unit.to.r == 7, "a negative hex coordinate comes back negative");

    sim_command full;
    unit_wire_fill(&full, SIM_COMMAND_PATROL, &counter);
    TEST_CHECK(counts, sim_wire_write_command(&full, buffer, SIM_WIRE_COMMAND_SIZE - 1) == 0,
               "a write to a short buffer gives 0");
    sim_wire_write_command(&full, buffer, sizeof(buffer));
    back.kind = SIM_COMMAND_PATROL;
    TEST_CHECK(counts, sim_wire_read_command(&back, buffer, SIM_WIRE_COMMAND_SIZE - 1) == 0 && back.kind == SIM_COMMAND_NONE,
               "a read of too few bytes gives 0 and no command");
    buffer[0] = (u8)SIM_COMMAND_COUNT;
    buffer[1] = buffer[2] = buffer[3] = 0;
    TEST_CHECK(counts, sim_wire_read_command(&back, buffer, SIM_WIRE_COMMAND_SIZE) == 0 && back.kind == SIM_COMMAND_NONE,
               "a read of an unknown kind gives 0 and no command");
}
