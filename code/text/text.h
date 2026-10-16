#ifndef TEXT_H
#define TEXT_H
/* The text module: Big5 to UTF-8, UTF-8 encoding, and the locale layer.

   The save loader keeps names as Big5 bytes (spec question Q-013: the spec
   does not name the Big5 variant). This module decodes the bytes with the
   Unicode BIG5.TXT table (big5_table.h). It maps the user-defined area to
   the Private Use Area, so a name with an unknown character still decodes
   to code points that give back the same bytes.

   The locale layer reads two kinds of TSV file from the content pack:
   - data/<pack>/text/<locale>.tsv   columns "id", "text": the UI strings
   - data/<pack>/names/<locale>.tsv  columns "key", "text": names, keyed by
                                     the Big5 bytes of one name as hex
   data/<pack>/text/locales.tsv (columns "code", "name") lists the locales.
   A new locale is a set of files and one row in locales.tsv, and no code.

   The module has no Windows headers, no CRT I/O and no ImGui. It reads files
   only through the platform reader that the caller gives it. */

#include "base.h"
#include "hexdom.h"     /* platform_read_entire_file */

#define TEXT_REPLACEMENT_CHARACTER 0xFFFD
#define TEXT_UTF8_MAX_BYTES        4
#define TEXT_LOCALE_SIZE           16     /* "zh-TW" and a NUL; codes are [A-Za-z0-9_-] */
#define TEXT_LOCALE_NAME_SIZE      48
#define TEXT_PACK_SIZE             32
#define TEXT_PATH_SIZE             160
#define TEXT_MAX_LOCALES           16

/* ---- code points ------------------------------------------------------- */

/* One code point as UTF-8. A surrogate or a value above U+10FFFF encodes
   as U+FFFD. */
struct text_utf8
{
    u8  bytes[TEXT_UTF8_MAX_BYTES];
    u32 length;
};

internal text_utf8 text_utf8_encode(u32 code_point);

/* The code point of one Big5 pair. U+FFFD if the trail byte is not a Big5
   trail byte (0x40..0x7E, 0xA1..0xFE), or if the table has no code point
   for the pair. The user-defined area maps to the Private Use Area:
     lead 0xFA..0xFE -> U+E000..U+E310
     lead 0x8E..0xA0 -> U+E311..U+EEB7
     lead 0x81..0x8D -> U+EEB8..U+F6B0 */
internal u32 text_big5_pair_to_code_point(u8 lead, u8 trail);

/* The reverse of the user-defined area: true and the pair if the code point
   is in one of the three Private Use ranges above. */
internal b32 text_big5_from_private_use(u32 code_point, u8 *lead, u8 *trail);

/* Decodes Big5 bytes to UTF-8 in out, with a NUL at the end. It stops at
   the first NUL byte or after length bytes, and it writes only whole
   characters. A lead byte without a valid trail byte gives U+FFFD, and the
   decoder reads the next byte as a new character. Returns the number of
   bytes it wrote, without the NUL. */
internal u32 text_decode_big5_into(const u8 *bytes, u32 length, char *out, u32 out_size);

/* As text_decode_big5_into, into a buffer from the arena. */
internal char *text_decode_big5(const u8 *bytes, u32 length, memory_arena *arena);

/* Copies a UTF-8 string into out, with a NUL at the end. If out is too
   small, it cuts the string at the end of a whole character. */
internal u32 text_copy_utf8(const char *text, char *out, u32 out_size);

/* ---- TSV tables -------------------------------------------------------- */

/* A two-column TSV file as a key -> text table. The first row is the
   header, and its first column must be the key column name that the
   caller gives. A row that starts with '#' is a comment. A third column is
   a note for translators; the table ignores it. In the text column, "\n",
   "\t" and "\\" are a new line, a tab and a backslash. If a key occurs two
   times, the first row wins. */
struct text_table
{
    u32          row_count;
    const char **row_keys;      /* in file order */
    const char **row_texts;
    u32          slot_count;    /* power of two; 0 for an empty table */
    u32         *slots;         /* row index + 1; 0 = empty slot */
    u32          bad_row_count; /* rows without a tab or with an empty key */
    b32          header_ok;
};

/* Copies the file into the arena and parses the copy. A full arena gives an
   empty table. */
internal text_table  text_table_parse(const char *contents, u64 size, const char *key_column, memory_arena *arena);
internal const char *text_table_find(text_table *table, const char *key);   /* 0 if absent */

/* ---- the locale layer -------------------------------------------------- */

struct text_locale_info
{
    char code[TEXT_LOCALE_SIZE];
    char name[TEXT_LOCALE_NAME_SIZE];   /* the name of the locale in its own language, UTF-8 */
};

/* Counters for the front end and the ui tests. */
struct text_catalog_stats
{
    u64 string_lookups;    /* text_get calls */
    u64 missing_strings;   /* text_get calls that returned the id */
    u64 name_lookups;      /* text_name calls */
    u64 decoded_names;     /* text_name calls that decoded the bytes (no names row) */
};

struct text_catalog
{
    platform_read_entire_file *read_entire_file;
    memory_arena arena;                       /* the files and tables; reset by each locale change */
    char         data_root[TEXT_PATH_SIZE];   /* relative to the executable, for example "..\\data" */
    char         pack[TEXT_PACK_SIZE];
    char         locale[TEXT_LOCALE_SIZE];
    char         fallback_locale[TEXT_LOCALE_SIZE];
    text_table   strings;                     /* text/<locale>.tsv */
    text_table   fallback_strings;            /* text/<fallback locale>.tsv */
    text_table   names;                       /* names/<locale>.tsv; may be empty */
    u32          locale_count;
    text_locale_info locales[TEXT_MAX_LOCALES];   /* from text/locales.tsv, in file order */
    u32          load_count;                  /* increments with each text_catalog_set_locale */
    text_catalog_stats stats;
};

/* Gives the catalog its arena and reader. It loads nothing; call
   text_catalog_set_locale. */
internal void text_catalog_init(text_catalog *catalog, memory_arena arena, platform_read_entire_file *reader,
                                const char *data_root, const char *pack, const char *fallback_locale);

/* Loads the locale list, the strings of the fallback locale and the strings
   and names of the locale. Returns false, and changes nothing, if the code
   is not a valid locale code. Returns true if the code is valid, also when
   files are missing: then text_get uses the fallback locale and the id. */
internal b32 text_catalog_set_locale(text_catalog *catalog, const char *locale);

/* True if text/<locale>.tsv of the active locale loaded. */
internal b32 text_catalog_has_strings(text_catalog *catalog);

/* The UTF-8 text of an id: the active locale, then the fallback locale,
   then the id itself. The pointer stays valid until the next locale change. */
internal const char *text_get(text_catalog *catalog, const char *id);

/* The display name of NUL-padded Big5 name bytes, in out: the row of
   names/<locale>.tsv with the key of the bytes, else the decoded bytes
   (zh-TW). Returns out. An empty name gives "". */
internal const char *text_name(text_catalog *catalog, const u8 *bytes, u32 size, char *out, u32 out_size);

/* The key of a name: the bytes up to the first NUL as upper-case hex,
   "B1E4BEDE". Returns the length of the key. */
internal u32 text_name_key(const u8 *bytes, u32 size, char *out, u32 out_size);

#endif /* TEXT_H */
