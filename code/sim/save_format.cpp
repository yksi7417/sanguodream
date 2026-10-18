/* Save file decoder. See sim/save_format.h for the source of every offset. */

#include "sim/save_format.h"

/* All multi-byte values in the save and scenario files are little endian. */
internal u16
format_read_u16(const u8 *bytes)
{
    return (u16)(bytes[0] | (bytes[1] << 8));
}

internal u32
format_read_u32(const u8 *bytes)
{
    return (u32)bytes[0] | ((u32)bytes[1] << 8) | ((u32)bytes[2] << 16) | ((u32)bytes[3] << 24);
}

internal void
format_read_u16_array(const u8 *bytes, u16 *out, u32 count)
{
    for (u32 index = 0; index < count; ++index) out[index] = format_read_u16(bytes + 2 * index);
}

/* The file must hold at least SAVE_ZLIB_STREAM bytes. */
internal save_header
save_decode_header(const u8 *file)
{
    save_header header = {};
    header.constant_at_0x000 = format_read_u16(file + SAVE_CONSTANT_AT_0X000);
    memcpy(header.constant_marker, file + SAVE_CONSTANT_MARKER, sizeof(header.constant_marker));
    header.file_kind = format_read_u32(file + SAVE_FILE_KIND);
    memcpy(header.magic_text, file + SAVE_MAGIC_TEXT, SAVE_MAGIC_TEXT_SIZE);
    header.constant_at_0x018 = format_read_u32(file + SAVE_CONSTANT_AT_0X018);
    header.constant_at_0x01c = format_read_u32(file + SAVE_CONSTANT_AT_0X01C);
    header.constant_at_0x020 = format_read_u32(file + SAVE_CONSTANT_AT_0X020);
    memcpy(header.player_ruler_name, file + SAVE_PLAYER_RULER_NAME, SAVE_HEADER_NAME_SIZE);
    header.unknown_at_0x063              = format_read_u16(file + SAVE_UNKNOWN_AT_0X063);
    header.constant_at_0x065             = file[SAVE_CONSTANT_AT_0X065];
    header.scenario_number               = file[SAVE_SCENARIO_NUMBER];
    header.scenario_start_year           = format_read_u16(file + SAVE_SCENARIO_START_YEAR);
    header.scenario_start_month          = file[SAVE_SCENARIO_START_MONTH];
    header.scenario_start_ten_day_period = file[SAVE_SCENARIO_START_TEN_DAY_PERIOD];
    memcpy(header.scenario_name, file + SAVE_SCENARIO_NAME, SAVE_HEADER_NAME_SIZE);
    header.unknown_at_0x07c = format_read_u32(file + SAVE_UNKNOWN_AT_0X07C);
    memcpy(header.tag, file + SAVE_TAG, SAVE_TAG_SIZE);
    header.uncompressed_payload_length = format_read_u32(file + SAVE_UNCOMPRESSED_PAYLOAD_LENGTH);
    header.compressed_stream_length    = format_read_u32(file + SAVE_COMPRESSED_STREAM_LENGTH);
    return header;
}

/* save-file.md, Officer array slots: an empty slot has all name bytes zero.
   The status byte does not decide it (QUESTIONS.md, Q-012). */
internal b32
officer_slot_is_empty(officer_record *officer)
{
    u8 any = 0;
    for (u32 index = 0; index < OFFICER_FAMILY_NAME_SIZE; ++index)   any |= officer->family_name[index];
    for (u32 index = 0; index < OFFICER_GIVEN_NAME_SIZE; ++index)    any |= officer->given_name[index];
    for (u32 index = 0; index < OFFICER_COURTESY_NAME_SIZE; ++index) any |= officer->courtesy_name[index];
    return any == 0;
}

/* Officer bytes +0..+151: the part that scenario records share. */
internal void
officer_decode_shared(const u8 *record, u16 id, officer_record *out)
{
    *out = {};
    out->id = id;
    memcpy(out->family_name,   record + OFFICER_FAMILY_NAME,   OFFICER_FAMILY_NAME_SIZE);
    memcpy(out->given_name,    record + OFFICER_GIVEN_NAME,    OFFICER_GIVEN_NAME_SIZE);
    memcpy(out->courtesy_name, record + OFFICER_COURTESY_NAME, OFFICER_COURTESY_NAME_SIZE);
    out->portrait_number          = format_read_u16(record + OFFICER_PORTRAIT_NUMBER);
    out->appearance_year          = format_read_u16(record + OFFICER_APPEARANCE_YEAR);
    out->birth_year               = format_read_u16(record + OFFICER_BIRTH_YEAR);
    out->death_year               = format_read_u16(record + OFFICER_DEATH_YEAR);
    out->family_line_head         = format_read_u16(record + OFFICER_FAMILY_LINE_HEAD);
    out->father                   = format_read_u16(record + OFFICER_FATHER);
    out->related_officer          = format_read_u16(record + OFFICER_RELATED_OFFICER);
    out->spouse                   = format_read_u16(record + OFFICER_SPOUSE);
    out->sworn_brother_group_head = format_read_u16(record + OFFICER_SWORN_BROTHER_GROUP_HEAD);
    out->compatibility            = record[OFFICER_COMPATIBILITY];
    format_read_u16_array(record + OFFICER_LIKED_OFFICERS,    out->liked_officers,    OFFICER_RELATION_SLOTS);
    format_read_u16_array(record + OFFICER_DISLIKED_OFFICERS, out->disliked_officers, OFFICER_RELATION_SLOTS);
    out->force_id         = record[OFFICER_FORCE_ID];
    out->base_location    = format_read_u16(record + OFFICER_BASE_LOCATION);
    out->current_position = format_read_u16(record + OFFICER_CURRENT_POSITION);
    out->status           = record[OFFICER_STATUS];
    out->merit            = format_read_u16(record + OFFICER_MERIT);
    memcpy(out->unit_aptitudes, record + OFFICER_UNIT_APTITUDES, OFFICER_UNIT_APTITUDE_COUNT);
    const u8 *stats = record + OFFICER_STATS;
    out->leadership   = stats[0];
    out->war          = stats[1];
    out->intelligence = stats[2];
    out->politics     = stats[3];
    out->charm        = stats[4];
    out->skill_number = record[OFFICER_SKILL_NUMBER];
    out->flags               = format_read_u16(record + OFFICER_FLAGS);
    out->has_acted_this_turn = (out->flags & OFFICER_FLAG_HAS_ACTED) != 0;
}

/* The full 203-byte save record. */
internal void
save_decode_officer(const u8 *record, u16 id, officer_record *out)
{
    officer_decode_shared(record, id, out);
    out->has_save_fields = true;
    format_read_u16_array(record + OFFICER_PER_STAT_EXPERIENCE, out->per_stat_experience, OFFICER_STAT_COUNT);
    out->order_kind = record[OFFICER_ORDER_KIND];
}

internal void
save_decode_city(const u8 *record, u8 id, city_record *out)
{
    *out = {};
    out->id                    = id;
    out->flags                 = record[SAVE_CITY_FLAGS];
    out->commanding_officer_id = format_read_u16(record + SAVE_CITY_COMMANDING_OFFICER_ID);
    out->owner_force_id        = record[SAVE_CITY_OWNER_FORCE_ID];
    out->capacity_constant     = format_read_u32(record + SAVE_CITY_CAPACITY_CONSTANT);
    out->gold                  = format_read_u32(record + SAVE_CITY_GOLD);
    out->troops                = format_read_u32(record + SAVE_CITY_TROOPS);
    out->food                  = format_read_u32(record + SAVE_CITY_FOOD);
    for (u32 stock = 0; stock < CITY_EQUIPMENT_STOCK_COUNT; ++stock)
    {
        out->equipment_stocks[stock] = format_read_u32(record + SAVE_CITY_EQUIPMENT_STOCKS + 4 * stock);
    }
    out->large_city_flag    = record[SAVE_CITY_LARGE_CITY_FLAG];
    out->durability_maximum = format_read_u16(record + SAVE_CITY_DURABILITY_MAXIMUM);
    out->public_order       = record[SAVE_CITY_PUBLIC_ORDER];
}

/* The 57-byte item record; the save and the scenario share it. */
internal void
item_decode(const u8 *record, u8 id, item_record *out)
{
    *out = {};
    out->id = id;
    memcpy(out->name, record + ITEM_NAME, ITEM_NAME_SIZE);
    out->effect_strength      = record[ITEM_EFFECT_STRENGTH];
    out->item_template_number = record[ITEM_TEMPLATE_NUMBER];
    out->owner_officer_id     = format_read_u16(record + ITEM_OWNER_OFFICER_ID);
    out->holding_city         = record[ITEM_HOLDING_CITY];
    out->owned                = record[ITEM_OWNED];
}

/* Decodes every record array of an inflated payload of SAVE_PAYLOAD_LENGTH
   bytes into arrays that the caller pushed. */
internal void
save_decode_payload(const u8 *payload, save_file *out)
{
    out->payload_scenario_number               = payload[SAVE_PAYLOAD_SCENARIO_NUMBER];
    out->payload_scenario_start_year           = format_read_u16(payload + SAVE_PAYLOAD_SCENARIO_START_YEAR);
    out->payload_scenario_start_month          = payload[SAVE_PAYLOAD_SCENARIO_START_MONTH];
    out->payload_scenario_start_ten_day_period = payload[SAVE_PAYLOAD_SCENARIO_START_TEN_DAY_PERIOD];

    for (u32 slot = 0; slot < out->officer_slot_count; ++slot)
    {
        save_decode_officer(payload + SAVE_PAYLOAD_OFFICER_ARRAY + SAVE_OFFICER_RECORD_SIZE * slot,
                            (u16)slot, out->officers + slot);
    }
    for (u32 city = 0; city < out->city_count; ++city)
    {
        save_decode_city(payload + SAVE_PAYLOAD_CITY_ARRAY + SAVE_CITY_RECORD_SIZE * city, (u8)city, out->cities + city);
    }
    for (u32 item = 0; item < out->item_count; ++item)
    {
        item_decode(payload + SAVE_PAYLOAD_ITEM_ARRAY + SAVE_ITEM_RECORD_SIZE * item, (u8)item, out->items + item);
    }
    /* M11: the building array and the troop unit array. */
    for (u32 index = 0; index < out->building_count; ++index)
    {
        const u8 *record = payload + SAVE_PAYLOAD_BUILDING_ARRAY + SAVE_BUILDING_RECORD_SIZE * index;
        building_record *building = out->buildings + index;
        building->index          = (u16)index;
        building->type           = record[BUILDING_TYPE];
        building->owner_force_id = record[BUILDING_OWNER_FORCE];
        building->durability     = format_read_u16(record + BUILDING_DURABILITY);
        building->x              = format_read_u16(record + BUILDING_X);
        building->y              = format_read_u16(record + BUILDING_Y);
    }
    for (u32 index = 0; index < out->unit_count; ++index)
    {
        const u8 *record = payload + SAVE_PAYLOAD_UNIT_ARRAY + SAVE_UNIT_RECORD_SIZE * index;
        unit_record *unit = out->units + index;
        unit->index       = (u8)index;
        unit->officers[0] = format_read_u16(record + UNIT_LEADER);
        unit->officers[1] = format_read_u16(record + UNIT_DEPUTY_1);
        unit->officers[2] = format_read_u16(record + UNIT_DEPUTY_2);
        unit->troops      = format_read_u16(record + UNIT_TROOPS);
        unit->x           = format_read_u16(record + UNIT_HEX_X);
        unit->y           = format_read_u16(record + UNIT_HEX_Y);
    }
}

/* The most arena memory save_load uses at one time: the records, the
   payload and the decoder state, each with 16 bytes for its alignment. */
#define SAVE_LOAD_ARENA_NEED (sizeof(officer_record) * SAVE_OFFICER_SLOTS + sizeof(city_record) * SAVE_CITY_COUNT + \
                              sizeof(item_record) * SAVE_ITEM_COUNT + sizeof(building_record) * SAVE_BUILDING_COUNT + \
                              sizeof(unit_record) * SAVE_UNIT_CAPACITY + SAVE_PAYLOAD_LENGTH + sizeof(inflate_state) + 7 * 16)

/* The loader rule of save-file.md: read the two lengths at 0x1B7, inflate
   the bytes from 0x1BF to the end of the file, check the inflated length
   against the first length. The magic text and the tag are the same in all
   19 files the spec saw; a file without them is refused as not a save.

   The decoded records stay in arena. The payload and the decoder state are
   temporary: the arena gets them back before the function returns. A
   refused file leaves the arena as it was. */
internal save_load_result
save_load(memory_arena *arena, const u8 *file, u64 file_size, save_file *out)
{
    save_load_result result = {};
    *out = {};
    if (file_size < SAVE_ZLIB_STREAM)
    {
        result.status = SAVE_LOAD_FILE_TOO_SMALL;
        return result;
    }
    out->header = save_decode_header(file);
    if (memcmp(out->header.magic_text, save_magic_text, SAVE_MAGIC_TEXT_SIZE) != 0 ||
        memcmp(out->header.tag, save_tag, SAVE_TAG_SIZE) != 0)
    {
        result.status = SAVE_LOAD_NOT_A_SAVE;
        return result;
    }
    if (out->header.uncompressed_payload_length != SAVE_PAYLOAD_LENGTH)
    {
        result.status = SAVE_LOAD_UNEXPECTED_PAYLOAD_LENGTH;
        return result;
    }

    u64 entry_mark = arena->used;
    if (arena->size - arena->used < SAVE_LOAD_ARENA_NEED)
    {
        result.status = SAVE_LOAD_OUT_OF_MEMORY;
        return result;
    }
    out->officer_slot_count = SAVE_OFFICER_SLOTS;
    out->city_count         = SAVE_CITY_COUNT;
    out->item_count         = SAVE_ITEM_COUNT;
    out->officers = PushArray(arena, SAVE_OFFICER_SLOTS, officer_record);
    out->cities   = PushArray(arena, SAVE_CITY_COUNT, city_record);
    out->items    = PushArray(arena, SAVE_ITEM_COUNT, item_record);
    out->building_count = SAVE_BUILDING_COUNT;   /* M11 */
    out->unit_count     = SAVE_UNIT_CAPACITY;
    out->buildings      = PushArray(arena, SAVE_BUILDING_COUNT, building_record);
    out->units          = PushArray(arena, SAVE_UNIT_CAPACITY, unit_record);

    u64 records_mark = arena->used;
    u8 *payload = (u8 *)arena_push_size(arena, SAVE_PAYLOAD_LENGTH, 1);
    result.inflate = zlib_inflate(arena, file + SAVE_ZLIB_STREAM, file_size - SAVE_ZLIB_STREAM,
                                  payload, SAVE_PAYLOAD_LENGTH);
    if (result.inflate.status != INFLATE_OK)
    {
        result.status = SAVE_LOAD_INFLATE_FAILED;
    }
    else if (result.inflate.bytes_written != out->header.uncompressed_payload_length)
    {
        result.status = SAVE_LOAD_LENGTH_MISMATCH;
    }
    else
    {
        save_decode_payload(payload, out);
        result.status = SAVE_LOAD_OK;
    }

    if (result.status == SAVE_LOAD_OK)
    {
        arena->used = records_mark;
    }
    else
    {
        arena->used = entry_mark;
        save_header header = out->header;
        *out = {};
        out->header = header;
    }
    return result;
}

internal const char *
save_load_status_name(save_load_status status)
{
    switch (status)
    {
        case SAVE_LOAD_OK:                        return "ok";
        case SAVE_LOAD_FILE_TOO_SMALL:            return "file too small";
        case SAVE_LOAD_NOT_A_SAVE:                return "not a save";
        case SAVE_LOAD_UNEXPECTED_PAYLOAD_LENGTH: return "unexpected payload length";
        case SAVE_LOAD_INFLATE_FAILED:            return "inflate failed";
        case SAVE_LOAD_LENGTH_MISMATCH:           return "length mismatch";
        case SAVE_LOAD_OUT_OF_MEMORY:             return "out of memory";
        default:                                  return "?";
    }
}

/* The labels of officer +100; unlabelled values give "?". */
internal const char *
officer_status_name(u8 status)
{
    switch (status)
    {
        case OFFICER_STATUS_RULER:                return "ruler";
        case OFFICER_STATUS_VICEROY:              return "viceroy";
        case OFFICER_STATUS_GOVERNOR:             return "governor";
        case OFFICER_STATUS_REGULAR_OFFICER:      return "regular officer";
        case OFFICER_STATUS_FREE_OR_NOT_APPEARED: return "free or not yet appeared";
        case OFFICER_STATUS_DEAD:                 return "dead";
        case OFFICER_STATUS_EMPTY_SLOT:           return "empty slot";
        default:                                  return "?";
    }
}
