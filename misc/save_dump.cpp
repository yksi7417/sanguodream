/* save_dump.exe: loads a .s11 save with the clean-room loader
   (code/sim/save_format) and prints its header, officers, cities and items
   as JSON. The keys are the field names of spec-src/spec/formats/save-file.md
   in snake case, so the output can be compared with the dump of the
   analysis side.

   Usage: save_dump.exe <file.s11> [--all-slots] [--utf8-names]
     --all-slots   also print the empty officer slots (all name bytes zero)
     --utf8-names  also print each name as UTF-8 through Windows code page
                   950. That code page is the operating system's Big5 table,
                   not a mapping from the spec; use it only to read the
                   output. The game never converts names.

   Names are always printed as the hex of their Big5 bytes up to the first
   NUL, under "<field>_big5_hex". Unknown bytes are left out.

   Exit code: 0 when the file loads and the cross-checks of save-file.md
   (City record) pass, 1 when a cross-check fails, 2 when the file does not
   load. */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>

#include "base.h"
#include "sim/inflate.h"
#include "sim/save_format.h"
#include "sim/save_format.cpp"
#include "test_framework.h"
#include "test_files.h"

global_variable b32 g_utf8_names;

/* ---- JSON output -------------------------------------------------------- */

struct json_writer
{
    u32 depth;
    b32 first_in_scope;
};

internal void
json_indent(json_writer *json)
{
    for (u32 level = 0; level < json->depth; ++level) printf("  ");
}

internal void
json_key(json_writer *json, const char *key)
{
    printf(json->first_in_scope ? "\n" : ",\n");
    json->first_in_scope = false;
    json_indent(json);
    if (key) printf("\"%s\": ", key);
}

internal void
json_open(json_writer *json, const char *key, char bracket)
{
    json_key(json, key);
    printf("%c", bracket);
    json->depth++;
    json->first_in_scope = true;
}

internal void
json_close(json_writer *json, char bracket)
{
    json->depth--;
    if (!json->first_in_scope)
    {
        printf("\n");
        json_indent(json);
    }
    printf("%c", bracket);
    json->first_in_scope = false;
}

internal void
json_uint(json_writer *json, const char *key, u32 value)
{
    json_key(json, key);
    printf("%u", value);
}

internal void
json_bool(json_writer *json, const char *key, b32 value)
{
    json_key(json, key);
    printf(value ? "true" : "false");
}

internal void
json_string(json_writer *json, const char *key, const char *value)
{
    json_key(json, key);
    printf("\"");
    for (const char *at = value; *at; ++at)
    {
        if (*at == '"' || *at == '\\') printf("\\%c", *at);
        else if ((u8)*at < 0x20)       printf("\\u%04x", (u8)*at);
        else                           printf("%c", *at);
    }
    printf("\"");
}

internal void
json_u16_array(json_writer *json, const char *key, const u16 *values, u32 count)
{
    json_key(json, key);
    printf("[");
    for (u32 index = 0; index < count; ++index) printf(index ? ", %u" : "%u", values[index]);
    printf("]");
}

internal void
json_u8_array(json_writer *json, const char *key, const u8 *values, u32 count)
{
    json_key(json, key);
    printf("[");
    for (u32 index = 0; index < count; ++index) printf(index ? ", %u" : "%u", values[index]);
    printf("]");
}

internal void
json_u32_array(json_writer *json, const char *key, const u32 *values, u32 count)
{
    json_key(json, key);
    printf("[");
    for (u32 index = 0; index < count; ++index) printf(index ? ", %u" : "%u", values[index]);
    printf("]");
}

/* A Big5 name field: the hex of its bytes up to the first NUL, and with
   --utf8-names the text through code page 950. */
internal void
json_big5(json_writer *json, const char *field, const u8 *bytes, u32 size)
{
    u32 length = 0;
    while (length < size && bytes[length]) length++;

    char hex[2 * 64 + 1];
    for (u32 index = 0; index < length && index < 64; ++index) snprintf(hex + 2 * index, 3, "%02X", bytes[index]);
    hex[2 * Minimum(length, 64u)] = 0;
    char key[64];
    snprintf(key, sizeof(key), "%s_big5_hex", field);
    json_string(json, key, hex);

    if (!g_utf8_names) return;
    wchar_t wide[64];
    char    utf8[256];
    int wide_length = length ? MultiByteToWideChar(950, 0, (const char *)bytes, (int)length, wide, 63) : 0;
    int utf8_length = wide_length ? WideCharToMultiByte(CP_UTF8, 0, wide, wide_length, utf8, 255, 0, 0) : 0;
    utf8[utf8_length > 0 ? utf8_length : 0] = 0;
    json_string(json, field, utf8);
}

/* ---- the records -------------------------------------------------------- */

internal void
dump_header(json_writer *json, save_file *save)
{
    save_header *header = &save->header;
    json_open(json, "header", '{');
    json_uint(json, "file_kind", header->file_kind);
    json_big5(json, "player_ruler_name", header->player_ruler_name, SAVE_HEADER_NAME_SIZE);
    json_uint(json, "unknown_at_0x063", header->unknown_at_0x063);
    json_uint(json, "scenario_number", header->scenario_number);
    json_uint(json, "scenario_start_year", header->scenario_start_year);
    json_uint(json, "scenario_start_month", header->scenario_start_month);
    json_uint(json, "scenario_start_ten_day_period", header->scenario_start_ten_day_period);
    json_big5(json, "scenario_name", header->scenario_name, SAVE_HEADER_NAME_SIZE);
    json_uint(json, "unknown_at_0x07c", header->unknown_at_0x07c);
    json_uint(json, "uncompressed_payload_length", header->uncompressed_payload_length);
    json_uint(json, "compressed_stream_length", header->compressed_stream_length);
    json_close(json, '}');
}

internal void
dump_officer(json_writer *json, officer_record *officer)
{
    json_open(json, 0, '{');
    json_uint(json, "id", officer->id);
    json_big5(json, "family_name", officer->family_name, OFFICER_FAMILY_NAME_SIZE);
    json_big5(json, "given_name", officer->given_name, OFFICER_GIVEN_NAME_SIZE);
    json_big5(json, "courtesy_name", officer->courtesy_name, OFFICER_COURTESY_NAME_SIZE);
    json_uint(json, "portrait_number", officer->portrait_number);
    json_uint(json, "appearance_year", officer->appearance_year);
    json_uint(json, "birth_year", officer->birth_year);
    json_uint(json, "death_year", officer->death_year);
    json_uint(json, "family_line_head", officer->family_line_head);
    json_uint(json, "father", officer->father);
    json_uint(json, "related_officer", officer->related_officer);
    json_uint(json, "spouse", officer->spouse);
    json_uint(json, "sworn_brother_group_head", officer->sworn_brother_group_head);
    json_uint(json, "compatibility", officer->compatibility);
    json_u16_array(json, "liked_officers", officer->liked_officers, OFFICER_RELATION_SLOTS);
    json_u16_array(json, "disliked_officers", officer->disliked_officers, OFFICER_RELATION_SLOTS);
    json_uint(json, "force_id", officer->force_id);
    json_uint(json, "base_location", officer->base_location);
    json_uint(json, "current_position", officer->current_position);
    json_uint(json, "status", officer->status);
    json_string(json, "status_label", officer_status_name(officer->status));
    json_uint(json, "merit", officer->merit);
    json_u8_array(json, "unit_aptitudes", officer->unit_aptitudes, OFFICER_UNIT_APTITUDE_COUNT);
    json_uint(json, "leadership", officer->leadership);
    json_uint(json, "war", officer->war);
    json_uint(json, "intelligence", officer->intelligence);
    json_uint(json, "politics", officer->politics);
    json_uint(json, "charm", officer->charm);
    json_uint(json, "skill_number", officer->skill_number);
    json_bool(json, "has_acted_this_turn", officer->has_acted_this_turn);
    json_u16_array(json, "per_stat_experience", officer->per_stat_experience, OFFICER_STAT_COUNT);
    json_uint(json, "order_kind", officer->order_kind);
    json_close(json, '}');
}

internal void
dump_city(json_writer *json, city_record *city)
{
    json_open(json, 0, '{');
    json_uint(json, "id", city->id);
    json_uint(json, "flags", city->flags);
    json_uint(json, "commanding_officer_id", city->commanding_officer_id);
    json_uint(json, "owner_force_id", city->owner_force_id);
    json_uint(json, "capacity_constant", city->capacity_constant);
    json_uint(json, "gold", city->gold);
    json_uint(json, "troops", city->troops);
    json_uint(json, "food", city->food);
    json_u32_array(json, "equipment_stocks", city->equipment_stocks, CITY_EQUIPMENT_STOCK_COUNT);
    json_uint(json, "large_city_flag", city->large_city_flag);
    json_uint(json, "durability_maximum", city->durability_maximum);
    json_uint(json, "public_order", city->public_order);
    json_close(json, '}');
}

internal void
dump_item(json_writer *json, item_record *item)
{
    json_open(json, 0, '{');
    json_uint(json, "id", item->id);
    json_big5(json, "item_name", item->name, ITEM_NAME_SIZE);
    json_uint(json, "effect_strength", item->effect_strength);
    json_uint(json, "item_template_number", item->item_template_number);
    json_uint(json, "owner_officer_id", item->owner_officer_id);
    json_uint(json, "holding_city", item->holding_city);
    json_uint(json, "owned", item->owned);
    json_close(json, '}');
}

/* ---- cross-checks (save-file.md, City record) --------------------------- */

/* For every city with a commander: the commander has status ruler, viceroy
   or governor, sits in that city and belongs to the owner force. For every
   owned city: each officer with status 0..3 based in the city belongs to
   the owner force. Prints each failure to stderr; returns the count. */
internal u32
save_cross_check(save_file *save)
{
    u32 failures = 0;
    for (u32 city_index = 0; city_index < save->city_count; ++city_index)
    {
        city_record *city = save->cities + city_index;
        if (city->commanding_officer_id != CITY_COMMANDER_NONE)
        {
            if (city->commanding_officer_id >= save->officer_slot_count)
            {
                fprintf(stderr, "check: city %u commander %u is not an officer slot\n", city->id, city->commanding_officer_id);
                failures++;
                continue;
            }
            officer_record *commander = save->officers + city->commanding_officer_id;
            if (commander->status > OFFICER_STATUS_GOVERNOR || commander->base_location != city->id ||
                commander->force_id != city->owner_force_id)
            {
                fprintf(stderr, "check: city %u commander %u has status %u, location %u, force %u (owner %u)\n",
                        city->id, commander->id, commander->status, commander->base_location, commander->force_id,
                        city->owner_force_id);
                failures++;
            }
        }
        if (city->owner_force_id == CITY_OWNER_NONE) continue;
        for (u32 slot = 0; slot < save->officer_slot_count; ++slot)
        {
            officer_record *officer = save->officers + slot;
            if (officer->status > OFFICER_STATUS_REGULAR_OFFICER || officer->base_location != city->id) continue;
            if (officer->force_id != city->owner_force_id)
            {
                fprintf(stderr, "check: officer %u in city %u has force %u, owner is %u\n",
                        officer->id, city->id, officer->force_id, city->owner_force_id);
                failures++;
            }
        }
    }
    return failures;
}

int
main(int argc, char **argv)
{
    const char *path = 0;
    b32 all_slots = false;
    for (int index = 1; index < argc; ++index)
    {
        if (strcmp(argv[index], "--all-slots") == 0)       all_slots = true;
        else if (strcmp(argv[index], "--utf8-names") == 0) g_utf8_names = true;
        else                                               path = argv[index];
    }
    if (!path)
    {
        fprintf(stderr, "usage: save_dump <file.s11> [--all-slots] [--utf8-names]\n");
        return 2;
    }

    memory_arena arena = test_arena_create(Megabytes(16));
    test_file file = test_read_file(path, &arena);
    if (!file.contents)
    {
        fprintf(stderr, "save_dump: cannot read %s\n", path);
        return 2;
    }
    save_file save;
    save_load_result result = save_load(&arena, file.contents, file.size, &save);
    if (result.status != SAVE_LOAD_OK)
    {
        fprintf(stderr, "save_dump: %s: %s (inflate: %s)\n", path, save_load_status_name(result.status),
                inflate_status_name(result.inflate.status));
        return 2;
    }

    json_writer json = {};
    json.first_in_scope = true;
    printf("{");
    json.depth = 1;
    dump_header(&json, &save);
    json_open(&json, "officers", '[');
    for (u32 slot = 0; slot < save.officer_slot_count; ++slot)
    {
        if (!all_slots && officer_slot_is_empty(save.officers + slot)) continue;
        dump_officer(&json, save.officers + slot);
    }
    json_close(&json, ']');
    json_open(&json, "cities", '[');
    for (u32 city = 0; city < save.city_count; ++city) dump_city(&json, save.cities + city);
    json_close(&json, ']');
    json_open(&json, "items", '[');
    for (u32 item = 0; item < save.item_count; ++item) dump_item(&json, save.items + item);
    json_close(&json, ']');
    printf("\n}\n");

    u32 failures = save_cross_check(&save);
    if (failures) fprintf(stderr, "save_dump: %u cross-check failures\n", failures);
    test_arena_destroy(&arena);
    return failures ? 1 : 0;
}
