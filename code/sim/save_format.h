#ifndef SAVE_FORMAT_H
#define SAVE_FORMAT_H
/* The save file (.s11 in SaveData), decoded from memory into named fields.

   Source: spec-src/spec/formats/save-file.md. Every offset below is a row of
   a table in that file, and every field name is the "Meaning" column of its
   row in snake case. The decoder reads each field at its offset. Nothing
   here mirrors the binary layout as a struct: the structs hold decoded
   values in the order that suits the game, not the order of the bytes.

   Fields that the spec marks "unknown" are not decoded, with two
   exceptions in the header (unknown_at_0x063 and the constants), which the
   load menu and the tests can show, and the loyalty of the officer
   record (+104). Names are Big5
   bytes, copied verbatim. The spec gives no character mapping, so the game
   never converts them. */

#include "base.h"
#include "sim/inflate.h"

/* ---- container (save-file.md, Container) -------------------------------- */

enum save_container_offset
{
    SAVE_CONSTANT_AT_0X000             = 0x000,   /* u16, 0 */
    SAVE_CONSTANT_MARKER               = 0x002,   /* 2 bytes, FE FF */
    SAVE_FILE_KIND                     = 0x004,   /* u32, 0x1A; scenario-file.md calls it the file kind */
    SAVE_MAGIC_TEXT                    = 0x008,   /* 10 ASCII bytes */
    SAVE_CONSTANT_AT_0X018             = 0x018,   /* u32, 1 */
    SAVE_CONSTANT_AT_0X01C             = 0x01C,   /* u32, 2 */
    SAVE_CONSTANT_AT_0X020             = 0x020,   /* u32, 1 */
    SAVE_PLAYER_RULER_NAME             = 0x05A,   /* 9 bytes Big5, NUL padded */
    SAVE_UNKNOWN_AT_0X063              = 0x063,   /* u16 */
    SAVE_CONSTANT_AT_0X065             = 0x065,   /* u8, 255 */
    SAVE_SCENARIO_NUMBER               = 0x066,   /* u8 */
    SAVE_SCENARIO_START_YEAR           = 0x067,   /* u16 */
    SAVE_SCENARIO_START_MONTH          = 0x069,   /* u8 */
    SAVE_SCENARIO_START_TEN_DAY_PERIOD = 0x06A,   /* u8 */
    SAVE_SCENARIO_NAME                 = 0x06B,   /* 9 bytes Big5, NUL padded */
    SAVE_TURN_COUNTER                  = 0x07C,   /* u32: the ten-day turns since the scenario start (Q-004) */
    SAVE_TAG                           = 0x1B3,   /* 4 bytes, "MWC" + 0x1A */
    SAVE_UNCOMPRESSED_PAYLOAD_LENGTH   = 0x1B7,   /* u32 */
    SAVE_COMPRESSED_STREAM_LENGTH      = 0x1BB,   /* u32 */
    SAVE_ZLIB_STREAM                   = 0x1BF    /* the rest of the file */
};

#define SAVE_MAGIC_TEXT_SIZE    10
#define SAVE_TAG_SIZE           4
#define SAVE_HEADER_NAME_SIZE   9
#define SAVE_PAYLOAD_LENGTH     1154785u   /* the same in every save the spec saw */

global_variable const u8 save_magic_text[SAVE_MAGIC_TEXT_SIZE] = { 'K', 'O', 'E', 'I', '%', 'S', 'A', 'N', '1', '1' };
global_variable const u8 save_tag[SAVE_TAG_SIZE]               = { 'M', 'W', 'C', 0x1A };

struct save_header
{
    u16 constant_at_0x000;
    u8  constant_marker[2];
    u32 file_kind;
    u8  magic_text[SAVE_MAGIC_TEXT_SIZE];
    u32 constant_at_0x018;
    u32 constant_at_0x01c;
    u32 constant_at_0x020;
    u8  player_ruler_name[SAVE_HEADER_NAME_SIZE];   /* Big5 */
    u16 unknown_at_0x063;
    u8  constant_at_0x065;
    u8  scenario_number;
    u16 scenario_start_year;
    u8  scenario_start_month;
    u8  scenario_start_ten_day_period;
    u8  scenario_name[SAVE_HEADER_NAME_SIZE];       /* Big5 */
    u32 turn_counter;                               /* the date is the start date plus 10 days for each count */
    u8  tag[SAVE_TAG_SIZE];
    u32 uncompressed_payload_length;
    u32 compressed_stream_length;
};

/* ---- payload map (save-file.md, Payload map) ---------------------------- */

enum save_payload_offset
{
    SAVE_PAYLOAD_SCENARIO_NUMBER               = 0x000000,   /* u8 */
    SAVE_PAYLOAD_SCENARIO_START_YEAR           = 0x000001,   /* u16 */
    SAVE_PAYLOAD_SCENARIO_START_MONTH          = 0x000003,   /* u8 */
    SAVE_PAYLOAD_SCENARIO_START_TEN_DAY_PERIOD = 0x000004,   /* u8 */
    SAVE_PAYLOAD_BUILDING_ARRAY                = 0x0008A3,   /* M11: 16384 records of 18 bytes */
    SAVE_PAYLOAD_OFFICER_ARRAY                 = 0x0488A4,
    SAVE_PAYLOAD_UNIT_ARRAY                    = 0x086B71,   /* M11: the troop unit array, 98 bytes a record */
    SAVE_PAYLOAD_AFTER_UNIT_ARRAY              = 0x08803B,   /* the next table of the payload */
    SAVE_PAYLOAD_ITEM_ARRAY                    = 0x07F0E8,
    SAVE_PAYLOAD_CITY_ARRAY                    = 0x0830FF,
    SAVE_PAYLOAD_FORCE_TABLE_B                 = 0x0807A9,   /* M13: 213 bytes a force */
    SAVE_PAYLOAD_FORCE_TABLE_A                 = 0x082E49,   /* M13: 15 bytes a force */
    SAVE_PAYLOAD_BASE_ARRAY                    = 0x08428D    /* the gate and port records, 70 bytes a record (spec 9632e15) */
};

#define SAVE_OFFICER_RECORD_SIZE  203
#define SAVE_OFFICER_SLOTS        1100   /* slot = officer id */
#define SAVE_ITEM_RECORD_SIZE     57
#define SAVE_ITEM_COUNT           50
#define SAVE_CITY_RECORD_SIZE     107
#define SAVE_CITY_COUNT           42

/* ---- officer record (save-file.md, Officer record) ---------------------- */

/* Bytes +0..+151 are shared with the scenario file's 152-byte record
   (scenario-file.md, Officer record). Bytes from +152 on are save only. */
enum officer_record_offset
{
    OFFICER_FAMILY_NAME              = 0,     /* 5 bytes Big5 */
    OFFICER_GIVEN_NAME               = 5,     /* 5 bytes Big5 */
    OFFICER_COURTESY_NAME            = 10,    /* 7 bytes Big5 */
    OFFICER_PORTRAIT_NUMBER          = 53,    /* u16 */
    OFFICER_SEX                      = 55,    /* u8: 1 marks a woman, 0 a man (G2-020, the answers of 2026-10-23: 27 named women in the sample) */
    OFFICER_APPEARANCE_YEAR          = 56,    /* u16 */
    OFFICER_BIRTH_YEAR               = 58,    /* u16 */
    OFFICER_DEATH_YEAR               = 60,    /* u16 */
    OFFICER_FAMILY_LINE_HEAD         = 63,    /* u16 */
    OFFICER_FATHER                   = 65,    /* u16 */
    OFFICER_RELATED_OFFICER          = 67,    /* u16 */
    OFFICER_SPOUSE                   = 70,    /* u16 */
    OFFICER_SWORN_BROTHER_GROUP_HEAD = 72,    /* u16 */
    OFFICER_COMPATIBILITY            = 74,    /* u8 */
    OFFICER_LIKED_OFFICERS           = 75,    /* 5 x u16 */
    OFFICER_DISLIKED_OFFICERS        = 85,    /* 5 x u16 */
    OFFICER_FORCE_ID                 = 95,    /* u8 */
    OFFICER_BASE_LOCATION            = 96,    /* u16 */
    OFFICER_CURRENT_POSITION         = 98,    /* u16 */
    OFFICER_STATUS                   = 100,   /* u8 */
    OFFICER_LOYALTY                  = 104,   /* u8, confirmed 2026-10-22; the scenario record has it too */
    OFFICER_MERIT                    = 105,   /* u16 */
    OFFICER_UNIT_APTITUDES           = 107,   /* 6 x u8 */
    OFFICER_STATS                    = 113,   /* 5 x u8: leadership, war, intelligence, politics, charm */
    OFFICER_SKILL_NUMBER             = 124,   /* u8 */
    OFFICER_FLAGS                    = 148,   /* u16 */
    OFFICER_SHARED_RECORD_END        = 152,   /* scenario records stop here */
    OFFICER_PER_STAT_EXPERIENCE      = 153,   /* 5 x u16, save only */
    OFFICER_ORDER_KIND               = 194    /* u8, save only */
};

#define OFFICER_FAMILY_NAME_SIZE      5
#define OFFICER_GIVEN_NAME_SIZE       5
#define OFFICER_COURTESY_NAME_SIZE    7
#define OFFICER_RELATION_SLOTS        5
#define OFFICER_UNIT_APTITUDE_COUNT   6
#define OFFICER_STAT_COUNT            5
#define OFFICER_FLAG_HAS_ACTED        0x0001   /* +148 bit 0, confirmed by diff */
#define OFFICER_ID_NONE               0xFFFF
#define OFFICER_FORCE_NONE            255
#define OFFICER_SKILL_NONE            255
#define OFFICER_LOCATION_NONE         0xFFFF
#define OFFICER_FIRST_UNIT_POSITION   87       /* current position 87 + unit index: in a troop unit */

/* Officer +100. Values 4, 5 and 7 are observed but have no label yet. The
   spec labels 255 "empty slot", but named built-in officers also carry 255
   (QUESTIONS.md, Q-012). To find empty slots, use officer_slot_is_empty. */
enum officer_status
{
    OFFICER_STATUS_RULER                = 0,
    OFFICER_STATUS_VICEROY              = 1,
    OFFICER_STATUS_GOVERNOR             = 2,
    OFFICER_STATUS_REGULAR_OFFICER      = 3,
    OFFICER_STATUS_FREE_OR_NOT_APPEARED = 6,
    OFFICER_STATUS_DEAD                 = 8,
    OFFICER_STATUS_EMPTY_SLOT           = 255
};

/* Officer +107, in this order; the spec still has the order "to verify". */
enum officer_unit_aptitude
{
    OFFICER_APTITUDE_SPEAR,
    OFFICER_APTITUDE_HALBERD,
    OFFICER_APTITUDE_CROSSBOW,
    OFFICER_APTITUDE_CAVALRY,
    OFFICER_APTITUDE_SIEGE,
    OFFICER_APTITUDE_NAVY
};

struct officer_record
{
    u16 id;                                   /* the slot */
    u8  family_name[OFFICER_FAMILY_NAME_SIZE];
    u8  given_name[OFFICER_GIVEN_NAME_SIZE];
    u8  courtesy_name[OFFICER_COURTESY_NAME_SIZE];
    u16 portrait_number;
    b32 female;                  /* +55 is 1 (G2-020) */
    u16 appearance_year;
    u16 birth_year;
    u16 death_year;
    u16 family_line_head;
    u16 father;
    u16 related_officer;
    u16 spouse;
    u16 sworn_brother_group_head;
    u8  compatibility;
    u16 liked_officers[OFFICER_RELATION_SLOTS];
    u16 disliked_officers[OFFICER_RELATION_SLOTS];
    u8  force_id;
    u16 base_location;
    u16 current_position;
    u8  status;
    u16 merit;
    u8  unit_aptitudes[OFFICER_UNIT_APTITUDE_COUNT];   /* 0 = C, 1 = B, 2 = A, 3 = S */
    u8  leadership;
    u8  war;
    u8  intelligence;
    u8  politics;
    u8  charm;
    u8  skill_number;                         /* the file id of the skill, not the number of skills.md (content_skill_of_file_id) */
    u16 flags;
    b32 has_acted_this_turn;                  /* flags bit 0 */
    /* +104 is the loyalty (save-file.md, confirmed on 2026-10-22: 武將一覽
       shows the byte, at most 100). 100 for rulers, 0 for inactive
       officers, 78..255 otherwise. The byte is in the shared part, so a
       scenario record has it too. has_loyalty is false for a record that
       a decoder did not fill. */
    b32 has_loyalty;
    u8  loyalty;
    /* save only; zero for a scenario record */
    b32 has_save_fields;
    u16 per_stat_experience[OFFICER_STAT_COUNT];
    u8  order_kind;
};

/* ---- city record (save-file.md, City record) ---------------------------- */

enum save_city_offset
{
    SAVE_CITY_FLAGS                   = 0,    /* u8 */
    SAVE_CITY_COMMANDING_OFFICER_ID   = 9,    /* u16 */
    SAVE_CITY_OWNER_FORCE_ID          = 11,   /* u8 */
    SAVE_CITY_CAPACITY_CONSTANT       = 12,   /* u32 */
    SAVE_CITY_TROOPS                  = 16,   /* u32; the spec swapped +16 and +20 on 2026-10-17 */
    SAVE_CITY_GOLD                    = 20,   /* u32 */
    SAVE_CITY_FOOD                    = 24,   /* u32 */
    SAVE_CITY_EQUIPMENT_STOCKS        = 32,   /* 10 x u32 */
    SAVE_CITY_LARGE_CITY_FLAG         = 77,   /* u8, hypothesis */
    SAVE_CITY_DURABILITY_MAXIMUM      = 80,   /* u16, hypothesis */
    SAVE_CITY_WALL_MAXIMUM            = 82,   /* u16: the maximum of the wall (G3-011, Q-222: 4600, 4000, 2400, 2800 on the screen) */
    SAVE_CITY_MORALE                  = 84,   /* u8: the morale of the city (spec 2026-10-18; G2-012) */
    SAVE_CITY_PUBLIC_ORDER            = 85    /* u8, hypothesis */
};

#define CITY_EQUIPMENT_STOCK_COUNT  10
#define CITY_OWNER_NONE             255
#define CITY_COMMANDER_NONE         0xFFFF

/* The order of save +32 as the spec lists it: spears, halberds, crossbows,
   war horses, battering rams, siege towers, catapults, then three ship
   types. The order is still "to verify" (QUESTIONS.md, Q-006). */
enum city_equipment_stock
{
    CITY_STOCK_SPEARS,
    CITY_STOCK_HALBERDS,
    CITY_STOCK_CROSSBOWS,
    CITY_STOCK_WAR_HORSES,
    CITY_STOCK_BATTERING_RAMS,
    CITY_STOCK_SIEGE_TOWERS,
    CITY_STOCK_CATAPULTS,
    CITY_STOCK_SHIP_TYPE_1,
    CITY_STOCK_SHIP_TYPE_2,
    CITY_STOCK_SHIP_TYPE_3
};

struct city_record
{
    u8  id;
    u8  flags;                   /* save only */
    u16 commanding_officer_id;   /* save only; CITY_COMMANDER_NONE in a scenario record */
    u8  owner_force_id;
    u32 capacity_constant;
    u32 gold;
    u32 troops;
    u32 food;
    u32 equipment_stocks[CITY_EQUIPMENT_STOCK_COUNT];
    u8  large_city_flag;         /* hypothesis */
    u16 durability_maximum;      /* hypothesis */
    u16 wall_maximum;            /* +82 of a save: the maximum of the wall (G3-011); 0 for a scenario record */
    u8  morale;                  /* save only: the 81 bytes of a scenario record stop before +84 */
    b32 has_morale;              /* false for a scenario record */
    u8  public_order;            /* hypothesis; save only */
};

/* ---- item record (save-file.md, Item record) ---------------------------- */

enum item_record_offset
{
    ITEM_NAME                 = 0,    /* 51 bytes Big5 */
    ITEM_EFFECT_STRENGTH      = 51,   /* u8 */
    ITEM_TEMPLATE_NUMBER      = 52,   /* u8 */
    ITEM_OWNER_OFFICER_ID     = 53,   /* u16 */
    ITEM_HOLDING_CITY         = 55,   /* u8 */
    ITEM_OWNED                = 56    /* u8 */
};

#define ITEM_NAME_SIZE   51

struct item_record
{
    u8  id;
    u8  name[ITEM_NAME_SIZE];
    u8  effect_strength;
    u8  item_template_number;
    u16 owner_officer_id;
    u8  holding_city;
    u8  owned;
};

/* ---- the whole save ----------------------------------------------------- */

enum save_load_status
{
    SAVE_LOAD_OK,
    SAVE_LOAD_FILE_TOO_SMALL,             /* shorter than the header */
    SAVE_LOAD_NOT_A_SAVE,                 /* magic text or tag is wrong */
    SAVE_LOAD_UNEXPECTED_PAYLOAD_LENGTH,  /* the header declares a length the payload map does not describe */
    SAVE_LOAD_INFLATE_FAILED,             /* see save_load_result.inflate */
    SAVE_LOAD_LENGTH_MISMATCH,            /* inflated length is not the declared length */
    SAVE_LOAD_OUT_OF_MEMORY,              /* the arena cannot hold the records, the payload and the decoder */
    SAVE_LOAD_STATUS_COUNT
};

/* ---- building record and troop unit record (save-file.md, M11) ------------ */

#define SAVE_BUILDING_RECORD_SIZE 18
#define SAVE_BUILDING_COUNT       16384
#define SAVE_BUILDING_BASE_COUNT  87      /* records 0..86: the bases in region order */
#define SAVE_UNIT_RECORD_SIZE     98
/* The unit array does not end at 0x08803B: in sample.s11 the records 54 to
   56 hold units at the same stride (spec 7fff546 counts 56 units). The spec
   gives no end. The engine reads the records that fit before the end of
   the 98 x 334 table at 0x08803B, 388 records (assumed, Q-216). */
#define SAVE_UNIT_TABLE_RECORDS   334
#define SAVE_UNIT_CAPACITY        ((SAVE_PAYLOAD_AFTER_UNIT_ARRAY + SAVE_UNIT_RECORD_SIZE * SAVE_UNIT_TABLE_RECORDS - SAVE_PAYLOAD_UNIT_ARRAY) / SAVE_UNIT_RECORD_SIZE)
#define SAVE_UNIT_OFFICER_SLOTS   3       /* the leader and two deputies */
#define SAVE_UNIT_LOCATION_BASE   87      /* officer +98: 87 + the unit index */

enum building_record_offset
{
    BUILDING_TYPE        = 1,    /* u8: 0 city, 1 gate, 2 port, then the facility types of facility-types.tsv */
    BUILDING_OWNER_FORCE = 2,    /* u8 */
    BUILDING_DURABILITY  = 3,    /* u16 */
    BUILDING_X           = 14,   /* u16 */
    BUILDING_Y           = 16    /* u16 */
};

struct building_record
{
    u16 index;
    u8  type;
    u8  owner_force_id;
    u16 durability;
    u16 x, y;
};

enum unit_record_offset
{
    UNIT_LEADER   = 0,   /* u16 officer id */
    UNIT_DEPUTY_1 = 2,   /* u16 officer id */
    UNIT_DEPUTY_2 = 4,   /* u16 officer id */
    UNIT_TROOPS   = 6,   /* u16 */
    UNIT_MORALE   = 8,   /* u8: 氣力 (spec b3ed57d) */
    UNIT_GOLD     = 9,   /* u32 */
    UNIT_GRAIN    = 13,  /* u32 */
    UNIT_STATE    = 17,  /* u8: 1 = 混亂; the other values are open (Q-211) */
    UNIT_TASK     = 19,  /* u16: the task 任務; 1 = 移動 (spec 819d36f); the other codes are open */
    UNIT_TARGET   = 21,  /* u16: the target 對象 as a base id 0..86 when the task targets a base */
    UNIT_HEX_X    = 23,  /* u16: the hex x on the map axes of map-data.md (spec 7fff546, Q-211) */
    UNIT_HEX_Y    = 25,  /* u16: the hex y */
    UNIT_ARMAMENT = 36,  /* u8: the land armament, a row of movement-costs.tsv (1 槍, 2 戟, 4 軍馬); 0 for a transport */
    UNIT_EQUIPMENT = 41  /* SAVE_UNIT_EQUIPMENT_COUNT entries: u8 kind, u32 count (Q-217) */
};

#define SAVE_UNIT_STATE_CONFUSED  1
#define SAVE_UNIT_TASK_MOVE       1     /* 任務 移動; 255 = no order; the other codes are open (spec 84714de) */
#define SAVE_UNIT_EQUIPMENT_COUNT 11
#define SAVE_UNIT_EQUIPMENT_SIZE  5     /* u8 kind, u32 count */

struct unit_equipment
{
    u8  kind;     /* the armament id: a row of movement-costs.tsv (1 槍 .. 11 鬥艦; spec 84714de) */
    u32 count;
};

struct unit_record
{
    u8  index;
    u16 officers[SAVE_UNIT_OFFICER_SLOTS];   /* [0] the leader; OFFICER_ID_NONE for an empty slot */
    u16 troops;
    u16 x, y;                                /* the hex: x the column, y the row */
    u8  morale;
    u32 gold, grain;
    u8  state;                               /* SAVE_UNIT_STATE_CONFUSED or another value (open) */
    u16 task, target;                        /* 任務 and 對象 */
    u8  armament;                            /* a row of movement-costs.tsv; 0 for a transport */
    unit_equipment equipment[SAVE_UNIT_EQUIPMENT_COUNT];   /* a transport: its cargo; a battle unit: [0] its ship */
};

/* ---- the per-hex block (save-file.md, spec b3ed57d; Q-200, Q-205) ---------- */

#define SAVE_HEX_SIDE        200                          /* the map is 200 x 200 hexes */
#define SAVE_HEX_COUNT       (SAVE_HEX_SIDE * SAVE_HEX_SIDE)
#define SAVE_HEX_RECORD_SIZE 4                            /* u32 for each hex, index x * 200 + y */
#define SAVE_HEX_FIRE_MASK   0x07                         /* the turns of fire left, 1 to 7 */
#define SAVE_HEX_SITE_LOW    16                           /* one kind of event site (temple or ruin, open: Q-205) */
#define SAVE_HEX_SITE_HIGH   32                           /* the other kind */

/* ---- gate and port record (save-file.md, spec 9632e15; Q-183) ------------- */

#define SAVE_BASE_RECORD_SIZE   70
#define SAVE_BASE_RECORD_COUNT  45      /* record k is base 42 + k: gates 42..51, ports 52..86 */
#define SAVE_BASE_FIRST_ID      42
#define SAVE_BASE_NO_OWNER      255     /* +9: no owner; the other fields are then 0 */

enum base_record_offset
{
    BASE_OWNER_FORCE = 9,    /* u8 */
    BASE_SOLDIERS    = 10    /* u16. +14 and +18 are perhaps the gold and the grain: not verified, not read */
};

struct base_record
{
    u8  id;                  /* the base id, 42..86 */
    u8  owner_force_id;      /* SAVE_BASE_NO_OWNER for none */
    u16 soldiers;
};

/* ---- the per-force tables (save-file.md, payload map; Q-202) --------------- */

#define SAVE_FORCE_COUNT                47       /* one record for each force id */
#define SAVE_FORCE_TABLE_A_RECORD_SIZE  15
#define SAVE_FORCE_TABLE_B_RECORD_SIZE  213
#define SAVE_FORCE_NO_RESEARCH          255
/* Record 46 of table A ends 11 bytes inside the city array, so one of the
   two bases is off (Q-202). The loader reads the action points of the
   forces before it only. */
#define SAVE_FORCE_ACTION_POINT_FORCES  46

enum force_table_offset
{
    FORCE_A_RULER            = 0,    /* u16 officer id */
    FORCE_A_ACTION_POINTS    = 12,   /* u8: 行動力 */
    FORCE_B_TECHNIQUE_POINTS = 0,    /* u8: 技巧P */
    FORCE_B_RESEARCH         = 6,    /* u8: the technique under research; SAVE_FORCE_NO_RESEARCH for none */
    FORCE_B_RESEARCH_TURNS   = 7     /* u8: the turns that remain */
};

struct save_force_record
{
    u8  id;
    u16 ruler;
    u8  action_points;
    b32 has_action_points;   /* false for force 46 (Q-202) */
    u8  technique_points;
    u8  research;
    u8  research_turns;
};

struct save_file
{
    save_header     header;
    /* payload bytes 0..4 */
    u8              payload_scenario_number;
    u16             payload_scenario_start_year;
    u8              payload_scenario_start_month;
    u8              payload_scenario_start_ten_day_period;
    officer_record *officers;        /* SAVE_OFFICER_SLOTS, index = officer id; empty slots included */
    u32             officer_slot_count;
    city_record    *cities;          /* SAVE_CITY_COUNT, index = city id */
    u32             city_count;
    item_record    *items;           /* SAVE_ITEM_COUNT, index = item id */
    u32             item_count;
    building_record *buildings;      /* M11: SAVE_BUILDING_COUNT, every record, empty ones too */
    u32             building_count;
    unit_record    *units;           /* M11: SAVE_UNIT_CAPACITY, index = unit index */
    u32             unit_count;
    base_record    *bases;           /* SAVE_BASE_RECORD_COUNT, index = base id - SAVE_BASE_FIRST_ID */
    u32             base_count;
    u32            *hex_states;      /* SAVE_HEX_COUNT, index = x * SAVE_HEX_SIDE + y: the fire and the event sites */
    u32             hex_state_count;
    save_force_record   *forces;          /* M13: SAVE_FORCE_COUNT, index = force id */
    u32             force_count;
};

struct save_load_result
{
    save_load_status status;
    inflate_result   inflate;
};

internal u16 format_read_u16(const u8 *bytes);
internal u32 format_read_u32(const u8 *bytes);

internal save_header      save_decode_header(const u8 *file);
internal b32              officer_slot_is_empty(officer_record *officer);
internal void             officer_decode_shared(const u8 *record, u16 id, officer_record *out);
internal void             save_decode_officer(const u8 *record, u16 id, officer_record *out);
internal void             save_decode_city(const u8 *record, u8 id, city_record *out);
internal void             item_decode(const u8 *record, u8 id, item_record *out);
internal save_load_result save_load(memory_arena *arena, const u8 *file, u64 file_size, save_file *out);
internal const char      *save_load_status_name(save_load_status status);
internal const char      *officer_status_name(u8 status);

#endif /* SAVE_FORMAT_H */
