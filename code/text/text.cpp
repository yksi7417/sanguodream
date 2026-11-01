/* The text module. See text.h for the contract. */

#include "text/text.h"
#include "text/big5_table.h"

/* The user-defined area of Big5 in the Private Use Area. Each lead byte has
   BIG5_TRAIL_COUNT positions, so each block is a run of whole rows. */
#define TEXT_UDA_HIGH_LEAD_FIRST   0xFA   /* 0xFA..0xFE */
#define TEXT_UDA_HIGH_LEAD_LAST    0xFE
#define TEXT_UDA_HIGH_FIRST_CODE   0xE000
#define TEXT_UDA_MIDDLE_LEAD_FIRST 0x8E   /* 0x8E..0xA0 */
#define TEXT_UDA_MIDDLE_LEAD_LAST  0xA0
#define TEXT_UDA_MIDDLE_FIRST_CODE 0xE311
#define TEXT_UDA_LOW_LEAD_FIRST    0x81   /* 0x81..0x8D */
#define TEXT_UDA_LOW_LEAD_LAST     0x8D
#define TEXT_UDA_LOW_FIRST_CODE    0xEEB8

#define TEXT_TRAIL_LOW_FIRST  0x40        /* trail bytes 0x40..0x7E: index 0..62 */
#define TEXT_TRAIL_LOW_LAST   0x7E
#define TEXT_TRAIL_HIGH_FIRST 0xA1        /* trail bytes 0xA1..0xFE: index 63..156 */
#define TEXT_TRAIL_HIGH_LAST  0xFE
#define TEXT_TRAIL_LOW_COUNT  (TEXT_TRAIL_LOW_LAST - TEXT_TRAIL_LOW_FIRST + 1)
#define TEXT_NO_TRAIL_INDEX   0xFFFFFFFFu

#define TEXT_MAX_CODE_POINT     0x10FFFF
#define TEXT_SURROGATE_FIRST    0xD800
#define TEXT_SURROGATE_LAST     0xDFFF
#define TEXT_TABLE_MIN_SLOTS    8

/* ---- code points ------------------------------------------------------- */

internal text_utf8
text_utf8_encode(u32 code_point)
{
    text_utf8 result = {};
    if (code_point > TEXT_MAX_CODE_POINT ||
        (code_point >= TEXT_SURROGATE_FIRST && code_point <= TEXT_SURROGATE_LAST))
    {
        code_point = TEXT_REPLACEMENT_CHARACTER;
    }
    if (code_point < 0x80)
    {
        result.bytes[0] = (u8)code_point;
        result.length = 1;
    }
    else if (code_point < 0x800)
    {
        result.bytes[0] = (u8)(0xC0 | (code_point >> 6));
        result.bytes[1] = (u8)(0x80 | (code_point & 0x3F));
        result.length = 2;
    }
    else if (code_point < 0x10000)
    {
        result.bytes[0] = (u8)(0xE0 | (code_point >> 12));
        result.bytes[1] = (u8)(0x80 | ((code_point >> 6) & 0x3F));
        result.bytes[2] = (u8)(0x80 | (code_point & 0x3F));
        result.length = 3;
    }
    else
    {
        result.bytes[0] = (u8)(0xF0 | (code_point >> 18));
        result.bytes[1] = (u8)(0x80 | ((code_point >> 12) & 0x3F));
        result.bytes[2] = (u8)(0x80 | ((code_point >> 6) & 0x3F));
        result.bytes[3] = (u8)(0x80 | (code_point & 0x3F));
        result.length = 4;
    }
    return result;
}

internal u32
text_big5_trail_index(u8 trail)
{
    if (trail >= TEXT_TRAIL_LOW_FIRST && trail <= TEXT_TRAIL_LOW_LAST)   return trail - TEXT_TRAIL_LOW_FIRST;
    if (trail >= TEXT_TRAIL_HIGH_FIRST && trail <= TEXT_TRAIL_HIGH_LAST) return TEXT_TRAIL_LOW_COUNT + trail - TEXT_TRAIL_HIGH_FIRST;
    return TEXT_NO_TRAIL_INDEX;
}

internal u8
text_big5_trail_of_index(u32 index)
{
    return index < TEXT_TRAIL_LOW_COUNT ? (u8)(TEXT_TRAIL_LOW_FIRST + index)
                                        : (u8)(TEXT_TRAIL_HIGH_FIRST + index - TEXT_TRAIL_LOW_COUNT);
}

internal u32
text_big5_pair_to_code_point(u8 lead, u8 trail)
{
    u32 index = text_big5_trail_index(trail);
    if (index == TEXT_NO_TRAIL_INDEX || lead < BIG5_LEAD_FIRST || lead > BIG5_LEAD_LAST)
    {
        return TEXT_REPLACEMENT_CHARACTER;
    }
    if (lead >= TEXT_UDA_HIGH_LEAD_FIRST)
    {
        return TEXT_UDA_HIGH_FIRST_CODE + (lead - TEXT_UDA_HIGH_LEAD_FIRST) * BIG5_TRAIL_COUNT + index;
    }
    if (lead >= TEXT_UDA_MIDDLE_LEAD_FIRST && lead <= TEXT_UDA_MIDDLE_LEAD_LAST)
    {
        return TEXT_UDA_MIDDLE_FIRST_CODE + (lead - TEXT_UDA_MIDDLE_LEAD_FIRST) * BIG5_TRAIL_COUNT + index;
    }
    if (lead <= TEXT_UDA_LOW_LEAD_LAST)
    {
        return TEXT_UDA_LOW_FIRST_CODE + (lead - TEXT_UDA_LOW_LEAD_FIRST) * BIG5_TRAIL_COUNT + index;
    }
    u32 row = big5_row_of_lead[lead - BIG5_LEAD_FIRST];
    if (!row) return TEXT_REPLACEMENT_CHARACTER;
    u32 code_point = big5_rows[row - 1][index];
    return code_point ? code_point : TEXT_REPLACEMENT_CHARACTER;
}

/* One block of the user-defined area: first lead, last lead, first code point. */
internal b32
text_big5_from_private_use_block(u32 code_point, u32 lead_first, u32 lead_last, u32 first_code, u8 *lead, u8 *trail)
{
    u32 last_code = first_code + (lead_last - lead_first + 1) * BIG5_TRAIL_COUNT - 1;
    if (code_point < first_code || code_point > last_code) return false;
    u32 offset = code_point - first_code;
    *lead  = (u8)(lead_first + offset / BIG5_TRAIL_COUNT);
    *trail = text_big5_trail_of_index(offset % BIG5_TRAIL_COUNT);
    return true;
}

internal b32
text_big5_from_private_use(u32 code_point, u8 *lead, u8 *trail)
{
    return text_big5_from_private_use_block(code_point, TEXT_UDA_HIGH_LEAD_FIRST, TEXT_UDA_HIGH_LEAD_LAST,
                                            TEXT_UDA_HIGH_FIRST_CODE, lead, trail) ||
           text_big5_from_private_use_block(code_point, TEXT_UDA_MIDDLE_LEAD_FIRST, TEXT_UDA_MIDDLE_LEAD_LAST,
                                            TEXT_UDA_MIDDLE_FIRST_CODE, lead, trail) ||
           text_big5_from_private_use_block(code_point, TEXT_UDA_LOW_LEAD_FIRST, TEXT_UDA_LOW_LEAD_LAST,
                                            TEXT_UDA_LOW_FIRST_CODE, lead, trail);
}

internal u32
text_decode_big5_into(const u8 *bytes, u32 length, char *out, u32 out_size)
{
    if (!out_size) return 0;
    u32 written = 0;
    u32 cursor  = 0;
    while (cursor < length && bytes[cursor])
    {
        u8  lead = bytes[cursor];
        u32 code_point;
        if (lead < 0x80)
        {
            code_point = lead;
            cursor += 1;
        }
        else if (lead >= BIG5_LEAD_FIRST && lead <= BIG5_LEAD_LAST && cursor + 1 < length &&
                 text_big5_trail_index(bytes[cursor + 1]) != TEXT_NO_TRAIL_INDEX)
        {
            code_point = text_big5_pair_to_code_point(lead, bytes[cursor + 1]);
            cursor += 2;
        }
        else
        {
            code_point = TEXT_REPLACEMENT_CHARACTER;   /* 0x80, 0xFF, or a lead byte without a trail byte */
            cursor += 1;
        }
        text_utf8 utf8 = text_utf8_encode(code_point);
        if (written + utf8.length + 1 > out_size) break;
        for (u32 index = 0; index < utf8.length; ++index) out[written++] = (char)utf8.bytes[index];
    }
    out[written] = 0;
    return written;
}

/* A push that returns 0 instead of an assert when the arena is full. */
internal void *
text_arena_try_push(memory_arena *arena, u64 size)
{
    u64 start = arena_align_forward((u64)(arena->base + arena->used), 16) - (u64)arena->base;
    if (start > arena->size || size > arena->size - start) return 0;
    return arena_push_size(arena, size);
}

internal char *
text_decode_big5(const u8 *bytes, u32 length, memory_arena *arena)
{
    /* A Big5 pair gives at most 3 UTF-8 bytes, and a single byte gives at most 3 (U+FFFD). */
    u64 size = (u64)length * 3 + 1;
    char *out = (char *)text_arena_try_push(arena, size);
    if (out) text_decode_big5_into(bytes, length, out, (u32)size);
    return out;
}

internal u32
text_copy_utf8(const char *text, char *out, u32 out_size)
{
    if (!out_size) return 0;
    u32 length = 0;
    while (text[length] && length + 1 < out_size) length++;
    if (text[length])
    {
        /* Cut before the continuation bytes of a character that does not fit. */
        while (length > 0 && ((u8)text[length] & 0xC0) == 0x80) length--;
    }
    memcpy(out, text, length);
    out[length] = 0;
    return length;
}

/* ---- TSV tables -------------------------------------------------------- */

internal u32
text_hash(const char *key)
{
    u32 hash = 2166136261u;                       /* FNV-1a */
    for (const u8 *cursor = (const u8 *)key; *cursor; ++cursor)
    {
        hash ^= *cursor;
        hash *= 16777619u;
    }
    return hash;
}

internal const char *
text_table_find(text_table *table, const char *key)
{
    if (!table->slot_count) return 0;
    u32 mask = table->slot_count - 1;
    for (u32 slot = text_hash(key) & mask; table->slots[slot]; slot = (slot + 1) & mask)
    {
        u32 row = table->slots[slot] - 1;
        if (strcmp(table->row_keys[row], key) == 0) return table->row_texts[row];
    }
    return 0;
}

/* Adds a row unless the key is already there. The table has free slots,
   because text_table_parse makes at least two slots for each line. */
internal void
text_table_insert(text_table *table, const char *key, const char *text)
{
    u32 mask = table->slot_count - 1;
    u32 slot = text_hash(key) & mask;
    for (; table->slots[slot]; slot = (slot + 1) & mask)
    {
        if (strcmp(table->row_keys[table->slots[slot] - 1], key) == 0) return;
    }
    table->row_keys[table->row_count]  = key;
    table->row_texts[table->row_count] = text;
    table->row_count++;
    table->slots[slot] = table->row_count;
}

/* Replaces "\n", "\t" and "\\" in place. An unknown escape stays as it is. */
internal void
text_unescape(char *text)
{
    char *read  = text;
    char *write = text;
    while (*read)
    {
        if (read[0] == '\\' && (read[1] == 'n' || read[1] == 't' || read[1] == '\\'))
        {
            *write++ = read[1] == 'n' ? '\n' : (read[1] == 't' ? '\t' : '\\');
            read += 2;
        }
        else
        {
            *write++ = *read++;
        }
    }
    *write = 0;
}

/* Cuts one line out of the copy: returns its start, NUL-terminated
   without "\r", and moves the cursor past the "\n". */
internal char *
text_next_line(char *copy, u64 size, u64 *cursor)
{
    char *line = copy + *cursor;
    u64 end = *cursor;
    while (end < size && copy[end] != '\n') end++;
    *cursor = end < size ? end + 1 : end;
    if (end > (u64)(line - copy) && copy[end - 1] == '\r') end--;
    copy[end] = 0;
    return line;
}

/* Splits a data row at its first tab and cuts the text at a second tab.
   Returns false for a row without a tab or with an empty key. */
internal b32
text_split_row(char *line, char **key, char **text)
{
    char *tab = line;
    while (*tab && *tab != '\t') tab++;
    if (!*tab || tab == line) return false;
    *tab = 0;
    char *end = tab + 1;
    while (*end && *end != '\t') end++;
    *end = 0;
    *key  = line;
    *text = tab + 1;
    text_unescape(*text);
    return true;
}

internal text_table
text_table_parse(const char *contents, u64 size, const char *key_column, memory_arena *arena)
{
    text_table table = {};
    if (!contents || !size) return table;

    u64 start = 0;
    if (size >= 3 && (u8)contents[0] == 0xEF && (u8)contents[1] == 0xBB && (u8)contents[2] == 0xBF) start = 3;   /* UTF-8 BOM */

    u32 line_count = 1;
    for (u64 index = start; index < size; ++index) if (contents[index] == '\n') line_count++;
    u32 slot_count = TEXT_TABLE_MIN_SLOTS;
    while (slot_count < 2 * line_count) slot_count *= 2;

    char        *copy      = (char *)text_arena_try_push(arena, size - start + 1);
    const char **row_keys  = (const char **)text_arena_try_push(arena, (u64)line_count * sizeof(char *));
    const char **row_texts = (const char **)text_arena_try_push(arena, (u64)line_count * sizeof(char *));
    u32         *slots     = (u32 *)text_arena_try_push(arena, (u64)slot_count * sizeof(u32));
    if (!copy || !row_keys || !row_texts || !slots) return table;
    memcpy(copy, contents + start, size - start);
    copy[size - start] = 0;
    u64 copy_size = size - start;

    table.row_keys   = row_keys;
    table.row_texts  = row_texts;
    table.slots      = slots;
    table.slot_count = slot_count;

    b32 header_seen = false;
    u64 cursor = 0;
    while (cursor < copy_size)
    {
        char *line = text_next_line(copy, copy_size, &cursor);
        if (!line[0] || line[0] == '#') continue;
        char *key;
        char *text;
        b32 split = text_split_row(line, &key, &text);
        if (!header_seen)
        {
            header_seen = true;
            table.header_ok = split && strcmp(key, key_column) == 0;
            if (!table.header_ok)
            {
                table.slot_count = 0;   /* not the file the caller expects: an empty table */
                return table;
            }
            continue;
        }
        if (!split)
        {
            table.bad_row_count++;
            continue;
        }
        text_table_insert(&table, key, text);
    }
    return table;
}

/* ---- the locale layer -------------------------------------------------- */

internal b32
text_is_locale_code(const char *code)
{
    u32 length = 0;
    for (; code[length]; ++length)
    {
        char c = code[length];
        b32 allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                      c == '-' || c == '_';
        if (!allowed || length + 1 >= TEXT_LOCALE_SIZE) return false;
    }
    return length > 0;
}

/* Appends text to out at *used. Stops at the end of out. */
internal void
text_append(char *out, u32 out_size, u32 *used, const char *text)
{
    while (*text && *used + 1 < out_size) out[(*used)++] = *text++;
    out[*used] = 0;
}

/* Reads <data root>\<pack>\<folder>\<stem>.tsv into the arena of the catalog. */
/* <data root>/<pack>/<folder>/<stem>.tsv as a table; folder "" for a file at the root of the pack. */
internal text_table
text_catalog_load_pack_table(text_catalog *catalog, const char *pack, const char *folder, const char *stem, const char *key_column)
{
    text_table empty = {};
    if (!catalog->read_entire_file) return empty;
    char path[TEXT_PATH_SIZE * 2];
    u32 used = 0;
    path[0] = 0;
    text_append(path, sizeof(path), &used, catalog->data_root);
    text_append(path, sizeof(path), &used, "/");
    text_append(path, sizeof(path), &used, pack);
    text_append(path, sizeof(path), &used, "/");
    if (folder[0])
    {
        text_append(path, sizeof(path), &used, folder);
        text_append(path, sizeof(path), &used, "/");
    }
    text_append(path, sizeof(path), &used, stem);
    text_append(path, sizeof(path), &used, ".tsv");
    read_file_result file = catalog->read_entire_file(path, &catalog->arena);
    if (!file.contents) return empty;
    return text_table_parse((const char *)file.contents, file.size, key_column, &catalog->arena);
}

internal text_table
text_catalog_load_table(text_catalog *catalog, const char *folder, const char *stem, const char *key_column)
{
    return text_catalog_load_pack_table(catalog, catalog->pack, folder, stem, key_column);
}

internal void
text_catalog_load_locales(text_catalog *catalog)
{
    catalog->locale_count = 0;
    text_table list = text_catalog_load_table(catalog, "text", "locales", "code");
    for (u32 row = 0; row < list.row_count && catalog->locale_count < TEXT_MAX_LOCALES; ++row)
    {
        if (!text_is_locale_code(list.row_keys[row])) continue;
        text_locale_info *info = catalog->locales + catalog->locale_count++;
        text_copy_utf8(list.row_keys[row], info->code, sizeof(info->code));
        text_copy_utf8(list.row_texts[row], info->name, sizeof(info->name));
    }
}

internal void
text_catalog_init(text_catalog *catalog, memory_arena arena, platform_read_entire_file *reader,
                  const char *data_root, const char *pack, const char *fallback_locale)
{
    memset(catalog, 0, sizeof(*catalog));
    catalog->arena            = arena;
    catalog->read_entire_file = reader;
    text_copy_utf8(data_root, catalog->data_root, sizeof(catalog->data_root));
    text_copy_utf8(pack, catalog->pack, sizeof(catalog->pack));
    text_copy_utf8(text_is_locale_code(fallback_locale) ? fallback_locale : "en",
                   catalog->fallback_locale, sizeof(catalog->fallback_locale));
}

internal b32
text_catalog_set_locale(text_catalog *catalog, const char *locale)
{
    if (!text_is_locale_code(locale)) return false;
    char code[TEXT_LOCALE_SIZE];
    text_copy_utf8(locale, code, sizeof(code));   /* the caller can pass catalog->locales[n].code */

    arena_reset(&catalog->arena);
    text_table empty = {};
    catalog->strings          = empty;
    catalog->fallback_strings = empty;
    catalog->names            = empty;
    catalog->pack_names       = empty;
    catalog->native_pack_names = empty;
    catalog->pack_lore         = empty;
    catalog->fallback_pack_lore = empty;
    catalog->pack_links        = empty;
    catalog->pack_story        = empty;
    catalog->fallback_pack_story = empty;
    catalog->pack_arms         = empty;
    catalog->pack_strings      = empty;
    catalog->fallback_pack_strings = empty;
    text_copy_utf8(code, catalog->locale, sizeof(catalog->locale));

    text_catalog_load_locales(catalog);
    catalog->fallback_strings = text_catalog_load_table(catalog, "text", catalog->fallback_locale, "id");
    if (strcmp(code, catalog->fallback_locale) == 0) catalog->strings = catalog->fallback_strings;
    else catalog->strings = text_catalog_load_table(catalog, "text", code, "id");
    catalog->names = text_catalog_load_table(catalog, "names", code, "key");
    if (catalog->game_pack[0] && strcmp(catalog->game_pack, catalog->pack) != 0)
    {
        catalog->pack_names        = text_catalog_load_pack_table(catalog, catalog->game_pack, "names", code, "key");
        catalog->native_pack_names = text_catalog_load_pack_table(catalog, catalog->game_pack, "names", "native", "key");
        /* docs/EUROPE.md, E4: the lives and the links of the game pack. */
        catalog->pack_lore = text_catalog_load_pack_table(catalog, catalog->game_pack, "lore", code, "key");
        if (strcmp(code, catalog->fallback_locale) == 0) catalog->fallback_pack_lore = catalog->pack_lore;
        else catalog->fallback_pack_lore = text_catalog_load_pack_table(catalog, catalog->game_pack, "lore", catalog->fallback_locale, "key");
        catalog->pack_links = text_catalog_load_pack_table(catalog, catalog->game_pack, "lore", "links", "key");
        catalog->pack_arms  = text_catalog_load_pack_table(catalog, catalog->game_pack, "arms", "arms", "key");   /* E5 */
        /* docs/EUROPE.md, E7: the pages of the story mode. */
        catalog->pack_strings = text_catalog_load_pack_table(catalog, catalog->game_pack, "text", code, "id");
        if (strcmp(code, catalog->fallback_locale) == 0) catalog->fallback_pack_strings = catalog->pack_strings;
        else catalog->fallback_pack_strings = text_catalog_load_pack_table(catalog, catalog->game_pack, "text", catalog->fallback_locale, "id");
        catalog->pack_story = text_catalog_load_pack_table(catalog, catalog->game_pack, "story", code, "key");
        if (strcmp(code, catalog->fallback_locale) == 0) catalog->fallback_pack_story = catalog->pack_story;
        else catalog->fallback_pack_story = text_catalog_load_pack_table(catalog, catalog->game_pack, "story", catalog->fallback_locale, "key");
    }
    catalog->load_count++;
    return true;
}

internal b32
text_pack_default_locale(text_catalog *catalog, const char *pack, char *out, u32 out_size)
{
    if (!out_size) return false;
    out[0] = 0;
    if (!catalog || !pack || !pack[0]) return false;
    /* The table lives in the arena of the catalog until the next locale change. */
    text_table settings = text_catalog_load_pack_table(catalog, pack, "", "pack", "key");
    const char *locale = text_table_find(&settings, "default_locale");
    if (!locale || !text_is_locale_code(locale)) return false;
    text_copy_utf8(locale, out, out_size);
    return true;
}

/* A pack folder name: letters, digits and '_' only, so that it cannot leave the data root. */
internal b32
text_is_pack_name(const char *name)
{
    u32 length = 0;
    for (; name[length]; ++length)
    {
        char c = name[length];
        b32 allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
        if (!allowed || length + 1 >= TEXT_PACK_SIZE) return false;
    }
    return length > 0;
}

internal b32
text_pack_art(text_catalog *catalog, const char *pack, char *out, u32 out_size)
{
    if (!out_size) return false;
    out[0] = 0;
    if (!catalog || !pack || !text_is_pack_name(pack)) return false;
    text_table settings = text_catalog_load_pack_table(catalog, pack, "", "pack", "key");
    const char *art = text_table_find(&settings, "art_pack");
    text_copy_utf8(art && text_is_pack_name(art) ? art : pack, out, out_size);
    return true;
}

internal void
text_catalog_set_game_pack(text_catalog *catalog, const char *pack)
{
    text_copy_utf8(pack ? pack : "", catalog->game_pack, sizeof(catalog->game_pack));
}

internal void
text_catalog_use_game_pack(text_catalog *catalog, const char *pack)
{
    text_catalog_set_game_pack(catalog, pack);
    char locale[TEXT_LOCALE_SIZE];
    /* The choice of the player is more important than the default of the pack. */
    if (catalog->player_locale || !text_pack_default_locale(catalog, pack, locale, sizeof(locale)))
        text_copy_utf8(catalog->locale, locale, sizeof(locale));
    text_catalog_set_locale(catalog, locale);
}

/* The row id of a record of the game pack: "<kind>.<key>". False if the key is empty. */
internal b32
text_record_id(text_catalog *catalog, const char *kind, const char *key, char *id, u32 id_size)
{
    if (!catalog || !key || !key[0]) return false;
    u32 used = 0;
    id[0] = 0;
    text_append(id, id_size, &used, kind);
    text_append(id, id_size, &used, ".");
    text_append(id, id_size, &used, key);
    return true;
}

internal const char *
text_key_lore(text_catalog *catalog, const char *kind, const char *key)
{
    char id[TEXT_PACK_SIZE + 64];
    if (!text_record_id(catalog, kind, key, id, sizeof(id))) return 0;
    const char *life = text_table_find(&catalog->pack_lore, id);
    return life ? life : text_table_find(&catalog->fallback_pack_lore, id);
}

internal const char *
text_key_story(text_catalog *catalog, const char *kind, const char *key)
{
    char id[TEXT_PACK_SIZE + 64];
    if (!text_record_id(catalog, kind, key, id, sizeof(id))) return 0;
    const char *page = text_table_find(&catalog->pack_story, id);
    return page ? page : text_table_find(&catalog->fallback_pack_story, id);
}

internal b32
text_link_is_allowed(const char *url)
{
    static const char prefix[] = "https://en.wikipedia.org/wiki/";
    if (!url || strncmp(url, prefix, sizeof(prefix) - 1) != 0) return false;
    const u8 *name = (const u8 *)url + sizeof(prefix) - 1;
    if (!*name) return false;
    for (; *name; ++name)
    {
        u8 letter = *name;
        if (letter <= 0x20 || letter == 0x7F) return false;   /* a space or a control character */
        if (letter == '"' || letter == '<' || letter == '>' || letter == '\\' || letter == '^' ||
            letter == '`' || letter == '{' || letter == '|' || letter == '}') return false;
    }
    return true;
}

internal const char *
text_key_link(text_catalog *catalog, const char *kind, const char *key)
{
    char id[TEXT_PACK_SIZE + 64];
    if (!text_record_id(catalog, kind, key, id, sizeof(id))) return 0;
    const char *url = text_table_find(&catalog->pack_links, id);
    return text_link_is_allowed(url) ? url : 0;
}

internal b32
text_arms_file_is_allowed(const char *path)
{
    static const char suffix[] = ".png";
    if (!path || !path[0] || path[0] == '/' || strstr(path, "..")) return false;
    u64 length = strlen(path);
    if (length <= sizeof(suffix) - 1 || strcmp(path + length - (sizeof(suffix) - 1), suffix) != 0) return false;
    for (const char *letter = path; *letter; ++letter)
    {
        char c = *letter;
        b32 allowed = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == '/';
        if (!allowed) return false;
    }
    return true;
}

internal const char *
text_key_arms(text_catalog *catalog, const char *key)
{
    char id[TEXT_PACK_SIZE + 64];
    if (!text_record_id(catalog, "force", key, id, sizeof(id))) return 0;
    const char *file = text_table_find(&catalog->pack_arms, id);
    return text_arms_file_is_allowed(file) ? file : 0;
}

internal const char *
text_key_name(text_catalog *catalog, const char *kind, const char *key)
{
    char id[TEXT_PACK_SIZE + 64];
    if (!text_record_id(catalog, kind, key, id, sizeof(id))) return 0;
    if (catalog->native_names)
    {
        const char *native = text_table_find(&catalog->native_pack_names, id);
        if (native) return native;
    }
    return text_table_find(&catalog->pack_names, id);
}

internal b32
text_catalog_has_strings(text_catalog *catalog)
{
    return catalog->strings.header_ok;
}

/* The UI string of an id: the game pack first, then the base pack, each in the active
   locale and then in the fallback locale. 0 if no table has the id. */
internal const char *
text_find_ui_string(text_catalog *catalog, const char *id)
{
    const char *found = text_table_find(&catalog->pack_strings, id);
    if (!found) found = text_table_find(&catalog->strings, id);
    if (!found) found = text_table_find(&catalog->fallback_pack_strings, id);
    if (!found) found = text_table_find(&catalog->fallback_strings, id);
    return found;
}

internal const char *
text_get(text_catalog *catalog, const char *id)
{
    if (!catalog) return id;
    catalog->stats.string_lookups++;
    const char *found = text_find_ui_string(catalog, id);
    if (found) return found;
    catalog->stats.missing_strings++;
    return id;
}

/* Writes the decimal digits of value at out; returns their count. */
internal u32
text_write_decimal(u32 value, char *out)
{
    char reversed[10];
    u32 count = 0;
    do
    {
        reversed[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    for (u32 index = 0; index < count; ++index) out[index] = reversed[count - 1 - index];
    return count;
}

internal const char *
text_label(text_catalog *catalog, const char *prefix, u32 code, char *out, u32 out_size)
{
    if (!out_size) return out;
    char id[64];
    u32 used = 0;
    for (; prefix[used] && used < sizeof(id) - 12; ++used) id[used] = prefix[used];
    id[used++] = '.';
    u32 digits = text_write_decimal(code, id + used);
    id[used + digits] = 0;

    const char *found = 0;
    if (catalog)
    {
        catalog->stats.label_lookups++;
        found = text_find_ui_string(catalog, id);
        if (!found) catalog->stats.unknown_labels++;
    }
    if (found)
    {
        text_copy_utf8(found, out, out_size);
        return out;
    }
    char unknown[12];
    unknown[0] = '?';
    unknown[1 + text_write_decimal(code, unknown + 1)] = 0;
    text_copy_utf8(unknown, out, out_size);
    return out;
}

internal u32
text_name_key(const u8 *bytes, u32 size, char *out, u32 out_size)
{
    local_persist const char digits[] = "0123456789ABCDEF";
    u32 used = 0;
    if (!out_size) return 0;
    for (u32 index = 0; index < size && bytes[index] && used + 2 < out_size; ++index)
    {
        out[used++] = digits[bytes[index] >> 4];
        out[used++] = digits[bytes[index] & 0xF];
    }
    out[used] = 0;
    return used;
}

internal const char *
text_name(text_catalog *catalog, const u8 *bytes, u32 size, char *out, u32 out_size)
{
    if (!out_size) return out;
    out[0] = 0;
    if (catalog) catalog->stats.name_lookups++;
    char key[2 * 64 + 1];
    if (!text_name_key(bytes, Minimum(size, 64u), key, sizeof(key))) return out;
    const char *found = catalog ? text_table_find(&catalog->names, key) : 0;
    if (found)
    {
        text_copy_utf8(found, out, out_size);
        return out;
    }
    if (catalog) catalog->stats.decoded_names++;
    text_decode_big5_into(bytes, size, out, out_size);
    return out;
}

/* M9: the Big5 bytes of a UTF-8 text, for a name that a player types (新武將).
   Each code point is found by a search of the Big5 table. Returns the byte
   count, or 0 if a character has no Big5 code or out is too small. */
internal u32
text_encode_big5(const char *utf8, u8 *out, u32 out_size)
{
    u32 used = 0;
    for (const u8 *at = (const u8 *)utf8; *at;)
    {
        u32 code = 0, extra = 0;
        if      (*at < 0x80)           { code = *at; extra = 0; }
        else if ((*at & 0xE0) == 0xC0) { code = *at & 0x1F; extra = 1; }
        else if ((*at & 0xF0) == 0xE0) { code = *at & 0x0F; extra = 2; }
        else if ((*at & 0xF8) == 0xF0) { code = *at & 0x07; extra = 3; }
        else return 0;
        ++at;
        for (u32 index = 0; index < extra; ++index)
        {
            if ((*at & 0xC0) != 0x80) return 0;
            code = (code << 6) | (*at++ & 0x3F);
        }
        if (code < 0x80 || used + 2 > out_size) return 0;
        b32 found = false;
        for (u32 lead = 0xA1; lead <= 0xF9 && !found; ++lead)
        {
            for (u32 index = 0; index < BIG5_TRAIL_COUNT && !found; ++index)
            {
                u8 trail = text_big5_trail_of_index(index);
                if (text_big5_pair_to_code_point((u8)lead, trail) != code) continue;
                out[used++] = (u8)lead;
                out[used++] = trail;
                found = true;
            }
        }
        if (!found) return 0;
    }
    return used;
}
/* ---- M18: text providers ------------------------------------------------ */

/* The text of an id in the active or the fallback locale, or 0. A miss here
   is not a missing string: the providers try a chain of ids. */
internal const char *
text_find_string(text_catalog *catalog, const char *id)
{
    if (!catalog) return 0;
    return text_find_ui_string(catalog, id);
}

/* "prefix" + "part" into out (64 bytes at most). */
internal void
text_join_id(const char *prefix, const char *part, char *out, u32 out_size)
{
    u32 used = 0;
    for (const char *at = prefix; *at && used + 1 < out_size; ++at) out[used++] = *at;
    for (const char *at = part; at && *at && used + 1 < out_size; ++at) out[used++] = *at;
    out[used] = 0;
}

/* Appends text to out at *used; stops before the end of out. */
internal void
text_append_text(char *out, u32 out_size, u32 *used, const char *text)
{
    for (const char *at = text; at && *at && *used + 1 < out_size; ++at) out[(*used)++] = *at;
}

/* Appends a signed number; with_sign writes "+" before a positive value. */
internal void
text_append_number(char *out, u32 out_size, u32 *used, s64 value, b32 with_sign)
{
    char digits[24];
    u32 count = 0;
    if (value < 0) digits[count++] = '-';
    else if (with_sign && value > 0) digits[count++] = '+';
    u64 magnitude = value < 0 ? (u64)(-value) : (u64)value;
    char reversed[20];
    u32 length = 0;
    do
    {
        reversed[length++] = (char)('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude && length < sizeof(reversed));
    while (length) digits[count++] = reversed[--length];
    digits[count] = 0;
    text_append_text(out, out_size, used, digits);
}

/* Fills the placeholders of a template. An unknown placeholder stays as it
   is, so that a bad template shows in the game. */
internal u32
text_fill_template(const char *pattern, const text_event_facts *facts, char *out, u32 out_size)
{
    if (!out_size) return 0;
    u32 used = 0;
    for (const char *at = pattern; *at && used + 1 < out_size;)
    {
        if (*at != '{') { out[used++] = *at++; continue; }
        const char *end = at + 1;
        while (*end && *end != '}') ++end;
        if (!*end) { out[used++] = *at++; continue; }
        char name[16];
        u32 length = (u32)(end - at - 1);
        if (length >= sizeof(name)) { out[used++] = *at++; continue; }
        for (u32 index = 0; index < length; ++index) name[index] = at[1 + index];
        name[length] = 0;
        if      (strcmp(name, "subject") == 0) text_append_text(out, out_size, &used, facts->subject);
        else if (strcmp(name, "speaker") == 0) text_append_text(out, out_size, &used, facts->speaker);
        else if (strcmp(name, "faction") == 0) text_append_text(out, out_size, &used, facts->faction);
        else if (strcmp(name, "delta") == 0)   text_append_number(out, out_size, &used, facts->delta, true);
        else if (strcmp(name, "amount") == 0)  text_append_number(out, out_size, &used, facts->delta < 0 ? -(s64)facts->delta : facts->delta, false);
        else if (strcmp(name, "value") == 0)
        {
            if (facts->value == TEXT_NO_VALUE) text_append_text(out, out_size, &used, "?");
            else text_append_number(out, out_size, &used, facts->value, false);
        }
        else
        {
            for (const char *copy = at; copy <= end && used + 1 < out_size; ++copy) out[used++] = *copy;
        }
        at = end + 1;
    }
    out[used] = 0;
    return used;
}

internal u32
text_template_line(void *user, text_catalog *catalog, const text_event_facts *facts, u32 form, char *out, u32 out_size)
{
    (void)user;
    char id[64];
    const char *pattern = 0;
    if (form == TEXT_LINE_SPEECH)
    {
        static const char *signs[] = { "neutral", "good", "bad" };
        text_join_id("say.", facts->key, id, sizeof(id));
        pattern = text_find_string(catalog, id);
        if (!pattern && facts->tone && facts->tone[0])
        {
            text_join_id("say.tone.", facts->tone, id, sizeof(id));
            pattern = text_find_string(catalog, id);
        }
        if (!pattern)
        {
            text_join_id("say.sign.", signs[facts->sign < ArrayCount(signs) ? facts->sign : 0], id, sizeof(id));
            pattern = text_find_string(catalog, id);
        }
    }
    else
    {
        text_join_id("log.", facts->key, id, sizeof(id));
        pattern = text_find_string(catalog, id);
        if (!pattern && facts->delta == 0) pattern = text_find_string(catalog, "log.generic.quiet");   /* no amount to show */
        if (!pattern) pattern = text_find_string(catalog, "log.generic");
    }
    if (!pattern) pattern = form == TEXT_LINE_SPEECH ? "..." : "{subject} {delta}";
    return text_fill_template(pattern, facts, out, out_size);
}

internal text_provider
text_template_provider(void)
{
    text_provider provider = {};
    provider.name = "template";
    provider.line = text_template_line;
    return provider;
}

internal u32
text_write_line(const text_provider *provider, text_catalog *catalog, const text_event_facts *facts, u32 form,
                char *out, u32 out_size)
{
    if (!out_size) return 0;
    out[0] = 0;
    u32 written = provider && provider->line ? provider->line(provider->user, catalog, facts, form, out, out_size) : 0;
    if (!written || !out[0]) written = text_template_line(0, catalog, facts, form, out, out_size);
    return written;
}
