#ifndef SCENARIO_FORMAT_H
#define SCENARIO_FORMAT_H
/* Scenario files (SCEN0nn.s11) and the name index (Scenario.s11), decoded
   from memory into named fields.

   Source: spec-src/spec/formats/scenario-file.md. The approach is the one of
   sim/save_format.h: each offset is a row of a spec table, each field name
   is its Meaning column in snake case, and the decoder reads one field at a
   time. Scenario files are not compressed.

   A scenario officer record is the save's officer record cut at +152, so it
   uses officer_decode_shared. A scenario city record is save bytes +3..+83
   with its own offsets below. The province table of the name index is not
   decoded: its layout is not in the spec yet (QUESTIONS.md, Q-015). */

#include "base.h"
#include "sim/save_format.h"

/* ---- container (scenario-file.md, Container) ---------------------------- */

enum scenario_container_offset
{
    SCENARIO_CONSTANT_MARKER        = 0x000,   /* 4 bytes, 00 00 FE FF */
    SCENARIO_FILE_KIND              = 0x004,   /* u32 */
    SCENARIO_MAGIC_TEXT             = 0x008,   /* 10 ASCII bytes */
    SCENARIO_CONSTANT_AT_0X018      = 0x018,   /* u32, 1 */
    SCENARIO_CONSTANT_AT_0X01C      = 0x01C,   /* u32, 2 */
    SCENARIO_SCENARIO_NUMBER        = 0x05A,   /* u8 */
    SCENARIO_START_YEAR             = 0x05B,   /* u16 */
    SCENARIO_START_MONTH            = 0x05D,   /* u8 */
    SCENARIO_START_TEN_DAY_PERIOD   = 0x05E,   /* u8 */
    SCENARIO_SCENARIO_TITLE         = 0x05F    /* Big5 text, NUL terminated */
};

#define SCENARIO_FILE_KIND_SCENARIO    0x16
#define SCENARIO_FILE_KIND_NAME_INDEX  0x18
#define SCENARIO_TITLE_MAX_SIZE        64    /* bytes kept from the title; the spec gives no limit */

/* ---- layout (scenario-file.md, Layout) ---------------------------------- */

enum scenario_layout_offset
{
    SCENARIO_OFFICER_ARRAY  = 0x04560,
    SCENARIO_ITEM_ARRAY     = 0x23E10,
    SCENARIO_FORCE_ARRAY    = 0x25454,
    SCENARIO_CITY_ARRAY     = 0x262FC
};

#define SCENARIO_OFFICER_RECORD_SIZE  152
#define SCENARIO_OFFICER_SLOTS        850
#define SCENARIO_FORCE_RECORD_SIZE    72
#define SCENARIO_FORCE_COUNT          47
#define SCENARIO_CITY_RECORD_SIZE     81
#define SCENARIO_CITY_COUNT           42
#define SCENARIO_FILE_SIZE            170010u   /* all fourteen files */

/* ---- force record (scenario-file.md, Force record) ---------------------- */

enum scenario_force_offset
{
    SCENARIO_FORCE_RULER_OFFICER_ID   = 0,   /* u16 */
    SCENARIO_FORCE_ADVISOR_OFFICER_ID = 2,   /* u16 */
    SCENARIO_FORCE_RELATIONS          = 4    /* 46 x u8 */
};

#define FORCE_RELATION_COUNT  46

struct force_record
{
    u8  id;
    u16 ruler_officer_id;
    u16 advisor_officer_id;
    u8  relations[FORCE_RELATION_COUNT];   /* toward each force, 0..100, own entry 100 */
};

/* ---- city record (scenario-file.md, City record) ------------------------ */

enum scenario_city_offset
{
    SCENARIO_CITY_OWNER_FORCE        = 8,    /* u8 */
    SCENARIO_CITY_CAPACITY_CONSTANT  = 9,    /* u32 */
    SCENARIO_CITY_STARTING_GOLD      = 13,   /* u32 */
    SCENARIO_CITY_STARTING_TROOPS    = 17,   /* u32 */
    SCENARIO_CITY_STARTING_FOOD      = 21,   /* u32 */
    SCENARIO_CITY_EQUIPMENT_STOCKS   = 29,   /* 10 x u32 */
    SCENARIO_CITY_LARGE_CITY_FLAG    = 74,   /* u8 */
    SCENARIO_CITY_DURABILITY_MAXIMUM = 77    /* u16, hypothesis */
};

/* ---- the name index (scenario-file.md, Scenario.s11) -------------------- */

enum name_index_offset
{
    NAME_INDEX_CITY_TABLE          = 0x05A,
    NAME_INDEX_GATE_AND_PORT_TABLE = 0x474
};

#define NAME_INDEX_CITY_ENTRY_SIZE    25
#define NAME_INDEX_GATE_ENTRY_SIZE    26
#define NAME_INDEX_GATE_COUNT         45
#define NAME_INDEX_FIRST_GATE_ID      42     /* gates and ports are locations 42..86 */
#define NAME_INDEX_CITY_NAME_SIZE     17     /* +0..+16: the name, then leftovers */
#define NAME_INDEX_CITY_PROVINCE      17     /* u8 */
#define NAME_INDEX_CITY_ADJACENT      18     /* up to 7 city ids, 0xFF terminated */
#define NAME_INDEX_ADJACENT_MAX       7
#define NAME_INDEX_ADJACENT_END       0xFF
#define NAME_INDEX_FILE_SIZE          47928u

struct name_index_city
{
    u8  id;
    u8  name[NAME_INDEX_CITY_NAME_SIZE];   /* Big5 up to the NUL; the bytes after it are zero */
    u8  province_index;
    u8  adjacent_city_ids[NAME_INDEX_ADJACENT_MAX];
    u32 adjacent_count;
};

struct name_index_gate
{
    u8 id;                                  /* 42..86 */
    u8 name[NAME_INDEX_GATE_ENTRY_SIZE];    /* Big5 up to the NUL; the rest of the entry is "to verify" */
};

/* ---- the whole files ---------------------------------------------------- */

enum scenario_load_status
{
    SCENARIO_LOAD_OK,
    SCENARIO_LOAD_FILE_TOO_SMALL,   /* shorter than the last table the decoder reads */
    SCENARIO_LOAD_WRONG_KIND,       /* magic text or file kind is not the one asked for */
    SCENARIO_LOAD_STATUS_COUNT
};

struct scenario_header
{
    u8  constant_marker[4];
    u32 file_kind;
    u8  magic_text[SAVE_MAGIC_TEXT_SIZE];
    u32 constant_at_0x018;
    u32 constant_at_0x01c;
    u8  scenario_number;
    u16 start_year;
    u8  start_month;
    u8  start_ten_day_period;
    u8  scenario_title[SCENARIO_TITLE_MAX_SIZE];   /* Big5 up to the NUL, at most 63 bytes */
};

struct scenario_file
{
    scenario_header header;
    officer_record *officers;      /* SCENARIO_OFFICER_SLOTS, index = officer id */
    u32             officer_slot_count;
    item_record    *items;
    u32             item_count;
    force_record   *forces;
    u32             force_count;
    city_record    *cities;        /* commander, flags and public order are not in a scenario */
    u32             city_count;
};

struct name_index
{
    u32             file_kind;
    name_index_city cities[SCENARIO_CITY_COUNT];
    name_index_gate gates_and_ports[NAME_INDEX_GATE_COUNT];
};

internal scenario_header      scenario_decode_header(const u8 *file);
internal void                 scenario_decode_force(const u8 *record, u8 id, force_record *out);
internal void                 scenario_decode_city(const u8 *record, u8 id, city_record *out);
internal scenario_load_status scenario_load(memory_arena *arena, const u8 *file, u64 file_size, scenario_file *out);
internal scenario_load_status name_index_load(const u8 *file, u64 file_size, name_index *out);
internal const char          *scenario_load_status_name(scenario_load_status status);

#endif /* SCENARIO_FORMAT_H */
