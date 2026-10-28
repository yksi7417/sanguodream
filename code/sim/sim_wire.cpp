/* The step codec (docs/MULTIPLAYER.md, "The transport"). It writes a
   sim_command to bytes and reads it back. Each field goes in a fixed
   order as a little-endian integer of the width of its type. The codec
   does not copy the struct, so a Windows client and a web client make
   the same bytes. It has no I/O. Included by sim.cpp. */

/* The fields of sim_command in the order of the wire. F is one field, A is
   an array of fields. A new field of sim_command or of one of its orders
   must go into this list: the check of SIM_WIRE_COMMAND_STRUCT_SIZE below
   stops the build until it does. */
#define SIM_WIRE_COMMAND_FIELDS(F, A) \
    F(kind, sim_command_kind) F(id, u32) \
    A(newcomer.family_name, u8, OFFICER_FAMILY_NAME_SIZE) A(newcomer.given_name, u8, OFFICER_GIVEN_NAME_SIZE) \
    F(newcomer.female, b32) F(newcomer.type, u8) F(newcomer.personality, u8) F(newcomer.birth_year, u16) \
    F(newcomer.city_id, u8) F(newcomer.found_force, b32) F(newcomer.ancient, u8) \
    F(pk.city_id, u8) F(pk.officer, u16) F(pk.row, u8) \
    F(corps.action, u8) F(corps.corps, u8) F(corps.viceroy, u16) F(corps.cities, u64) F(corps.policy, u8) \
    F(corps.target, u8) F(corps.flags, u32) F(corps.transport_city, u8) \
    F(m8.force, u8) F(m8.city_id, u8) F(m8.officer, u16) F(m8.value, u8) \
    F(court.city_id, u8) F(court.officer, u16) A(court.rewards, u16, SIM_REWARD_OFFICERS) F(court.reward_count, u32) \
    F(court.target, u16) F(court.item, u8) F(court.post, u8) F(court.kind, u8) F(court.force, u8) F(court.force_2, u8) \
    F(court.target_city, u8) F(court.gold, u32) F(court.months, u32) \
    F(move.city_id, u8) F(move.target_city, u8) A(move.officers, u16, SIM_MOVE_OFFICERS) \
    F(mini.action, u8) F(mini.value, u8) F(mini.officer_a, u16) F(mini.officer_b, u16) \
    F(develop.city_id, u8) F(develop.facility, u8) F(develop.slot, u8) A(develop.officers, u16, SIM_DEVELOP_OFFICERS) \
    F(city.city_id, u8) F(city.weapon, u8) F(city.mode, u8) F(city.amount, u32) A(city.officers, u16, SIM_COMMAND_OFFICERS) \
    F(damage.city_id, u8) F(damage.slot, u8) F(damage.amount, u32) \
    F(research.city_id, u8) F(research.technique, u8) A(research.officers, u16, SIM_COMMAND_OFFICERS) \
    F(merge.city_id, u8) F(merge.absorber, u8) F(merge.absorbed, u8) F(merge.officer, u16) \
    F(disaster.city_id, u8) F(disaster.disaster, u8) \
    F(sortie.city_id, u8) F(sortie.unit_type, u8) A(sortie.officers, u16, SIM_COMMAND_OFFICERS) F(sortie.soldiers, u32) \
    F(sortie.gold, u32) F(sortie.grain, u32) F(sortie.destination, u8) A(sortie.cargo, u32, CITY_EQUIPMENT_STOCK_COUNT) \
    F(sortie.ship, u8) \
    F(unit.unit, u16) F(unit.to.q, s32) F(unit.to.r, s32) F(unit.target, u16) F(unit.soldiers, u32) F(unit.gold, u32) \
    F(unit.grain, u32) F(unit.kind, u8)

#define SIM_WIRE_FIELD_SIZE(member, type)        + (u32)sizeof(type)
#define SIM_WIRE_ARRAY_SIZE(member, type, count) + (u32)sizeof(type) * (count)
#define SIM_WIRE_COMMAND_SIZE (0 SIM_WIRE_COMMAND_FIELDS(SIM_WIRE_FIELD_SIZE, SIM_WIRE_ARRAY_SIZE))

/* The size of sim_command when this list was last checked. If the struct
   changes, add the new field to SIM_WIRE_COMMAND_FIELDS, then set this
   value to the new size. */
#define SIM_WIRE_COMMAND_STRUCT_SIZE 416
static_assert(sizeof(sim_command) == SIM_WIRE_COMMAND_STRUCT_SIZE, "sim_command changed: update SIM_WIRE_COMMAND_FIELDS");

struct sim_wire_cursor
{
    u8       *at;
    const u8 *read_at;
    const u8 *end;
    b32       ok;
};

internal void
sim_wire_put(sim_wire_cursor *cursor, u64 value, u32 bytes)
{
    if (!cursor->ok || (u64)(cursor->end - cursor->at) < bytes)
    {
        cursor->ok = false;
        return;
    }
    for (u32 index = 0; index < bytes; ++index) *cursor->at++ = (u8)(value >> (8 * index));
}

internal u64
sim_wire_get(sim_wire_cursor *cursor, u32 bytes)
{
    if (!cursor->ok || (u64)(cursor->end - cursor->read_at) < bytes)
    {
        cursor->ok = false;
        return 0;
    }
    u64 value = 0;
    for (u32 index = 0; index < bytes; ++index) value |= (u64)*cursor->read_at++ << (8 * index);
    return value;
}

/* Writes the command; returns the bytes, or 0 if the buffer is too short. */
internal u32
sim_wire_write_command(const sim_command *command, u8 *buffer, u32 capacity)
{
    sim_wire_cursor cursor = { buffer, 0, buffer + capacity, true };
#define SIM_WIRE_PUT_FIELD(member, type) sim_wire_put(&cursor, (u64)command->member, (u32)sizeof(type));
#define SIM_WIRE_PUT_ARRAY(member, type, count) \
    for (u32 index = 0; index < (count); ++index) sim_wire_put(&cursor, (u64)command->member[index], (u32)sizeof(type));
    SIM_WIRE_COMMAND_FIELDS(SIM_WIRE_PUT_FIELD, SIM_WIRE_PUT_ARRAY)
#undef SIM_WIRE_PUT_FIELD
#undef SIM_WIRE_PUT_ARRAY
    return cursor.ok ? SIM_WIRE_COMMAND_SIZE : 0;
}

/* Reads a command; returns the bytes, or 0 if the bytes are too few or
   the kind is no command kind. On 0 the command is SIM_COMMAND_NONE. */
internal u32
sim_wire_read_command(sim_command *command, const u8 *buffer, u32 size)
{
    sim_wire_cursor cursor = { 0, buffer, buffer + size, true };
    sim_command read = {};
#define SIM_WIRE_GET_FIELD(member, type) read.member = (type)sim_wire_get(&cursor, (u32)sizeof(type));
#define SIM_WIRE_GET_ARRAY(member, type, count) \
    for (u32 index = 0; index < (count); ++index) read.member[index] = (type)sim_wire_get(&cursor, (u32)sizeof(type));
    SIM_WIRE_COMMAND_FIELDS(SIM_WIRE_GET_FIELD, SIM_WIRE_GET_ARRAY)
#undef SIM_WIRE_GET_FIELD
#undef SIM_WIRE_GET_ARRAY
    *command = {};
    if (!cursor.ok || (u32)read.kind >= SIM_COMMAND_COUNT) return 0;
    *command = read;
    return SIM_WIRE_COMMAND_SIZE;
}
