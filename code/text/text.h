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
   A game pack (text_catalog_set_game_pack) can also have text/<locale>.tsv.
   Its rows replace the rows of the base pack with the same id. Thus a pack
   can give new names to the skills and the tactics of its game.

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
/* M9: the Big5 bytes of UTF-8 text; 0 if a character has no Big5 code. */
internal u32 text_encode_big5(const char *utf8, u8 *out, u32 out_size);

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
    u64 label_lookups;     /* text_label calls */
    u64 unknown_labels;    /* text_label calls that gave "?" and the number */
};

struct text_catalog
{
    platform_read_entire_file *read_entire_file;
    memory_arena arena;                       /* the files and tables; reset by each locale change */
    char         data_root[TEXT_PATH_SIZE];   /* relative to the executable, for example "../data" */
    char         pack[TEXT_PACK_SIZE];
    char         locale[TEXT_LOCALE_SIZE];
    char         fallback_locale[TEXT_LOCALE_SIZE];
    text_table   strings;                     /* text/<locale>.tsv */
    text_table   fallback_strings;            /* text/<fallback locale>.tsv */
    text_table   names;                       /* names/<locale>.tsv; may be empty */
    char         game_pack[TEXT_PACK_SIZE];   /* the pack of the engine; "" or pack for the base pack (docs/EUROPE.md, E2) */
    text_table   pack_names;                  /* names/<locale>.tsv of the game pack, keyed by "<kind>.<English key>" */
    text_table   native_pack_names;           /* names/native.tsv of the game pack, in the Latin script (docs/EUROPE.md, E3) */
    text_table   pack_lore;                   /* lore/<locale>.tsv of the game pack, keyed by "<kind>.<English key>" (E4) */
    text_table   fallback_pack_lore;          /* lore/<fallback locale>.tsv of the game pack */
    text_table   pack_links;                  /* lore/links.tsv of the game pack: the URL of the article of each record */
    text_table   pack_story;                  /* story/<locale>.tsv of the game pack, keyed by "<kind>.<key>" (E7) */
    text_table   pack_strings;                /* text/<locale>.tsv of the game pack: these rows replace the rows of strings */
    text_table   fallback_pack_strings;       /* text/<fallback locale>.tsv of the game pack */
    text_table   fallback_pack_story;         /* story/<fallback locale>.tsv of the game pack */
    text_table   pack_arms;                   /* arms/arms.tsv of the game pack: the PNG file of the arms of each faction (E5) */
    b32          native_names;                /* the Names setting: text_key_name gives the native name first */
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

/* docs/EUROPE.md, E1: the default locale of a pack, from the row
   default_locale of data/<pack>/pack.tsv. False, and out "", if the pack
   has no such row or the row is not a valid locale code. */
internal b32 text_pack_default_locale(text_catalog *catalog, const char *pack, char *out, u32 out_size);

/* docs/EUROPE.md, E2: the pack of the engine. The next
   text_catalog_set_locale loads names/<locale>.tsv of that pack too. The
   base pack of the catalog has no keyed names. */
internal void text_catalog_set_game_pack(text_catalog *catalog, const char *pack);

/* A new engine of a pack: the game pack, then the default locale of that
   pack, or the active locale if the pack names none. The names of the
   game pack load with the locale. */
internal void text_catalog_use_game_pack(text_catalog *catalog, const char *pack);

/* The name of a record of the game pack by its English key: the row
   "<kind>.<key>" of the names of the game pack in the active locale, for
   example "officer.philip_ii". 0 if the key is empty or has no row. The
   fallback locale does not apply, so zh-TW keeps the Big5 names.
   docs/EUROPE.md, E3: if native_names is on, the row of names/native.tsv
   comes first. A key that the native table lacks gives the normal name. The
   native table does not depend on the locale, and the setting does not
   load a file. */
internal const char *text_key_name(text_catalog *catalog, const char *kind, const char *key);

/* docs/EUROPE.md, E4: the life of a record of the game pack by its English
   key: the row "<kind>.<key>" of lore/<locale>.tsv of the game pack. If the
   locale has no row, the row of the fallback locale comes next. 0 if the key
   is empty or no table has a row. */
internal const char *text_key_lore(text_catalog *catalog, const char *kind, const char *key);

/* docs/EUROPE.md, E7: a page of the story mode: the row "<kind>.<key>" of
   story/<locale>.tsv of the game pack. The kinds are "page" and "title"
   for the pages of the world, and "force" for the page of a faction. If the
   locale has no row, the row of the fallback locale comes next. 0 if the
   key is empty or no table has a row. */
internal const char *text_key_story(text_catalog *catalog, const char *kind, const char *key);

/* The link to the article of a record: the row "<kind>.<key>" of
   lore/links.tsv of the game pack. The table does not depend on the locale.
   0 if the key is empty, the row is absent, or text_link_is_allowed refuses
   the URL. */
internal const char *text_key_link(text_catalog *catalog, const char *kind, const char *key);

/* docs/EUROPE.md, E5: the file of the coat of arms of a faction: the row
   "force.<key>" of arms/arms.tsv of the game pack. The path is under the
   data folder, for example "medieval_europe/arms/france.png". The table
   does not depend on the locale. 0 if the key is empty, the row is absent,
   or text_arms_file_is_allowed refuses the path. */
internal const char *text_key_arms(text_catalog *catalog, const char *key);

/* True if the path is a PNG file under the data folder: lower-case letters,
   digits, "_", "-", "." and "/", no "..", no first "/", and the end ".png".
   The platform reads only such a path. */
internal b32 text_arms_file_is_allowed(const char *path);

/* True if the URL is an article of the English Wikipedia: it starts with
   "https://en.wikipedia.org/wiki/", it has a name after that prefix, and it
   has no space, control character or quote. The front end and the platform
   both check a URL with this function before they open it. */
internal b32 text_link_is_allowed(const char *url);

/* The label of an enum code, in out: the text of "<prefix>.<code>" in the
   active locale, else in the fallback locale. A code that no locale has
   gives "?" and the number, for example "?4". An unknown code is not a
   missing string. Returns out. */
internal const char *text_label(text_catalog *catalog, const char *prefix, u32 code, char *out, u32 out_size);

/* The key of a name: the bytes up to the first NUL as upper-case hex,
   "B1E4BEDE". Returns the length of the key. */
internal u32 text_name_key(const u8 *bytes, u32 size, char *out, u32 out_size);

/* ---- M18: text providers ------------------------------------------------
   A text provider turns the facts of one event into a line of words. The
   player decided on 2026-10-20 that a provider is a plug-in: the first one
   fills templates, and a later one can use a small language model. The
   front end fills the facts from the engine. This module knows nothing of
   the engine, so the facts are plain values and UTF-8 names. */
enum text_sign
{
    TEXT_SIGN_NEUTRAL,
    TEXT_SIGN_GOOD,          /* good news for the faction of the event */
    TEXT_SIGN_BAD,
};

enum text_line_form
{
    TEXT_LINE_LOG,           /* one line of the message log, in the third person */
    TEXT_LINE_SPEECH,        /* what the speaker says in the speech bubble */
};

#define TEXT_NO_VALUE (-1)

struct text_event_facts
{
    const char *key;          /* the key of the event kind, for example "conscripted" */
    const char *tone;         /* for example "proud"; "" for none */
    u8          sign;         /* text_sign */
    u8          personality;  /* the personality code of the speaker; the template provider ignores it */
    const char *subject;      /* the name of the city or the officer of the event, or "" */
    const char *speaker;      /* the name of the officer who speaks, or "" */
    const char *faction;      /* the name of the faction of the event, or "" */
    s32         delta;        /* the amount of the event */
    s64         value;        /* the value after the event (the soldiers of the city), or TEXT_NO_VALUE */
};

/* Writes the line into out with a NUL. Returns the bytes written without
   the NUL, or 0 if the provider has no line for the facts. */
typedef u32 text_line_fn(void *user, text_catalog *catalog, const text_event_facts *facts, u32 form, char *out, u32 out_size);

struct text_provider
{
    const char   *name;       /* for the settings and the tests, for example "template" */
    text_line_fn *line;
    void         *user;       /* the data of the provider */
};

/* The template provider. A log line is the string "log.<key>", else
   "log.generic". A speech is "say.<key>", else "say.tone.<tone>", else
   "say.sign.<sign>". A template can hold {subject}, {speaker}, {faction},
   {delta} (with its sign), {amount} (without a sign) and {value}. */
internal text_provider text_template_provider(void);

/* The line of a provider. If the provider gives no line, the template
   provider writes it, so that a line is never empty. */
internal u32 text_write_line(const text_provider *provider, text_catalog *catalog, const text_event_facts *facts, u32 form,
                             char *out, u32 out_size);

#endif /* TEXT_H */
