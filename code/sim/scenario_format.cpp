/* Scenario file and name index decoders. See sim/scenario_format.h for the
   source of every offset. */

#include "sim/scenario_format.h"

/* Copies a NUL-terminated Big5 string of at most size bytes and zeroes the
   bytes after the NUL, so that leftovers in the file never reach the game.
   The last byte of out is always zero. */
internal void
format_copy_terminated(u8 *out, u32 out_size, const u8 *bytes, u32 size)
{
    memset(out, 0, out_size);
    u32 limit = Minimum(size, out_size - 1);
    for (u32 index = 0; index < limit && bytes[index]; ++index) out[index] = bytes[index];
}

/* The file must hold at least SCENARIO_SCENARIO_TITLE + SCENARIO_TITLE_MAX_SIZE bytes. */
internal scenario_header
scenario_decode_header(const u8 *file)
{
    scenario_header header = {};
    memcpy(header.constant_marker, file + SCENARIO_CONSTANT_MARKER, sizeof(header.constant_marker));
    header.file_kind = format_read_u32(file + SCENARIO_FILE_KIND);
    memcpy(header.magic_text, file + SCENARIO_MAGIC_TEXT, SAVE_MAGIC_TEXT_SIZE);
    header.constant_at_0x018    = format_read_u32(file + SCENARIO_CONSTANT_AT_0X018);
    header.constant_at_0x01c    = format_read_u32(file + SCENARIO_CONSTANT_AT_0X01C);
    header.scenario_number      = file[SCENARIO_SCENARIO_NUMBER];
    header.start_year           = format_read_u16(file + SCENARIO_START_YEAR);
    header.start_month          = file[SCENARIO_START_MONTH];
    header.start_ten_day_period = file[SCENARIO_START_TEN_DAY_PERIOD];
    format_copy_terminated(header.scenario_title, SCENARIO_TITLE_MAX_SIZE,
                           file + SCENARIO_SCENARIO_TITLE, SCENARIO_TITLE_MAX_SIZE);
    return header;
}

internal void
scenario_decode_force(const u8 *record, u8 id, force_record *out)
{
    *out = {};
    out->id                 = id;
    out->ruler_officer_id   = format_read_u16(record + SCENARIO_FORCE_RULER_OFFICER_ID);
    out->advisor_officer_id = format_read_u16(record + SCENARIO_FORCE_ADVISOR_OFFICER_ID);
    memcpy(out->relations, record + SCENARIO_FORCE_RELATIONS, FORCE_RELATION_COUNT);
}

/* Fills the fields that a scenario city carries. The commander is none;
   flags and public order stay zero, because a scenario has no such bytes. */
internal void
scenario_decode_city(const u8 *record, u8 id, city_record *out)
{
    *out = {};
    out->id                    = id;
    out->commanding_officer_id = CITY_COMMANDER_NONE;
    out->owner_force_id        = record[SCENARIO_CITY_OWNER_FORCE];
    out->capacity_constant     = format_read_u32(record + SCENARIO_CITY_CAPACITY_CONSTANT);
    out->troops                = format_read_u32(record + SCENARIO_CITY_STARTING_TROOPS);
    out->gold                  = format_read_u32(record + SCENARIO_CITY_STARTING_GOLD);
    out->food                  = format_read_u32(record + SCENARIO_CITY_STARTING_FOOD);
    for (u32 stock = 0; stock < CITY_EQUIPMENT_STOCK_COUNT; ++stock)
    {
        out->equipment_stocks[stock] = format_read_u32(record + SCENARIO_CITY_EQUIPMENT_STOCKS + 4 * stock);
    }
    out->large_city_flag    = record[SCENARIO_CITY_LARGE_CITY_FLAG];
    out->durability_maximum = format_read_u16(record + SCENARIO_CITY_DURABILITY_MAXIMUM);
}

internal b32
scenario_kind_matches(const u8 *file, u32 file_kind)
{
    return memcmp(file + SCENARIO_MAGIC_TEXT, save_magic_text, SAVE_MAGIC_TEXT_SIZE) == 0 &&
           format_read_u32(file + SCENARIO_FILE_KIND) == file_kind;
}

/* Reads a SCEN0nn.s11 file. The records go into arena. */
internal scenario_load_status
scenario_load(memory_arena *arena, const u8 *file, u64 file_size, scenario_file *out)
{
    *out = {};
    u64 last_byte_read = SCENARIO_CITY_ARRAY + (u64)SCENARIO_CITY_RECORD_SIZE * SCENARIO_CITY_COUNT;
    if (file_size < last_byte_read) return SCENARIO_LOAD_FILE_TOO_SMALL;
    if (!scenario_kind_matches(file, SCENARIO_FILE_KIND_SCENARIO)) return SCENARIO_LOAD_WRONG_KIND;

    out->header = scenario_decode_header(file);

    out->officer_slot_count = SCENARIO_OFFICER_SLOTS;
    out->officers = PushArray(arena, SCENARIO_OFFICER_SLOTS, officer_record);
    for (u32 slot = 0; slot < SCENARIO_OFFICER_SLOTS; ++slot)
    {
        officer_decode_shared(file + SCENARIO_OFFICER_ARRAY + SCENARIO_OFFICER_RECORD_SIZE * slot,
                              (u16)slot, out->officers + slot);
    }

    out->item_count = SAVE_ITEM_COUNT;
    out->items = PushArray(arena, SAVE_ITEM_COUNT, item_record);
    for (u32 item = 0; item < SAVE_ITEM_COUNT; ++item)
    {
        item_decode(file + SCENARIO_ITEM_ARRAY + SAVE_ITEM_RECORD_SIZE * item, (u8)item, out->items + item);
    }

    out->force_count = SCENARIO_FORCE_COUNT;
    out->forces = PushArray(arena, SCENARIO_FORCE_COUNT, force_record);
    for (u32 force = 0; force < SCENARIO_FORCE_COUNT; ++force)
    {
        scenario_decode_force(file + SCENARIO_FORCE_ARRAY + SCENARIO_FORCE_RECORD_SIZE * force, (u8)force, out->forces + force);
    }

    out->city_count = SCENARIO_CITY_COUNT;
    out->cities = PushArray(arena, SCENARIO_CITY_COUNT, city_record);
    for (u32 city = 0; city < SCENARIO_CITY_COUNT; ++city)
    {
        scenario_decode_city(file + SCENARIO_CITY_ARRAY + SCENARIO_CITY_RECORD_SIZE * city, (u8)city, out->cities + city);
    }
    return SCENARIO_LOAD_OK;
}

internal void
name_index_decode_city(const u8 *entry, u8 id, name_index_city *out)
{
    *out = {};
    out->id = id;
    format_copy_terminated(out->name, NAME_INDEX_CITY_NAME_SIZE, entry, NAME_INDEX_CITY_NAME_SIZE);
    out->province_index = entry[NAME_INDEX_CITY_PROVINCE];
    for (u32 slot = 0; slot < NAME_INDEX_ADJACENT_MAX; ++slot)
    {
        u8 adjacent = entry[NAME_INDEX_CITY_ADJACENT + slot];
        if (adjacent == NAME_INDEX_ADJACENT_END) break;
        out->adjacent_city_ids[out->adjacent_count++] = adjacent;
    }
}

/* Reads Scenario.s11: the city and the gate/port name tables. */
internal scenario_load_status
name_index_load(const u8 *file, u64 file_size, name_index *out)
{
    *out = {};
    u64 last_byte_read = NAME_INDEX_GATE_AND_PORT_TABLE + (u64)NAME_INDEX_GATE_ENTRY_SIZE * NAME_INDEX_GATE_COUNT;
    if (file_size < last_byte_read) return SCENARIO_LOAD_FILE_TOO_SMALL;
    if (!scenario_kind_matches(file, SCENARIO_FILE_KIND_NAME_INDEX)) return SCENARIO_LOAD_WRONG_KIND;

    out->file_kind = format_read_u32(file + SCENARIO_FILE_KIND);
    for (u32 city = 0; city < SCENARIO_CITY_COUNT; ++city)
    {
        name_index_decode_city(file + NAME_INDEX_CITY_TABLE + NAME_INDEX_CITY_ENTRY_SIZE * city, (u8)city, out->cities + city);
    }
    for (u32 gate = 0; gate < NAME_INDEX_GATE_COUNT; ++gate)
    {
        name_index_gate *entry = out->gates_and_ports + gate;
        entry->id = (u8)(NAME_INDEX_FIRST_GATE_ID + gate);
        format_copy_terminated(entry->name, NAME_INDEX_GATE_ENTRY_SIZE,
                               file + NAME_INDEX_GATE_AND_PORT_TABLE + NAME_INDEX_GATE_ENTRY_SIZE * gate,
                               NAME_INDEX_GATE_ENTRY_SIZE);
    }
    return SCENARIO_LOAD_OK;
}

internal const char *
scenario_load_status_name(scenario_load_status status)
{
    switch (status)
    {
        case SCENARIO_LOAD_OK:             return "ok";
        case SCENARIO_LOAD_FILE_TOO_SMALL: return "file too small";
        case SCENARIO_LOAD_WRONG_KIND:     return "wrong kind";
        default:                           return "?";
    }
}
