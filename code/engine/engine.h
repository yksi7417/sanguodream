#ifndef ENGINE_H
#define ENGINE_H
/* The line between the backend engine and whatever drives it.

   Everything a front end may do to the game goes through engine_api, a table
   of plain function pointers over an opaque engine. The UI never touches
   sim_state, the ECS or the ledger directly. Feature tests drive the real
   engine through this same table; UI tests substitute a recording double
   (tests/ui/engine_recorder.h) and assert which calls the UI made.

   Growing the game means growing this table: a new mechanic adds the query
   and command functions a front end needs, and nothing else. */

#include "base.h"
#include "hexdom.h"     /* platform_read_entire_file: the engine reads files only through it */
#include "sim/sim.h"

struct engine_memory_stats
{
    u64 permanent_used;
    u64 permanent_size;
    u64 transient_used;
    u64 transient_size;
};

struct engine_map_size
{
    u32 width;
    u32 height;
};

/* An officer of the loaded game, as a front end sees it. Names are the
   Big5 bytes of the save, verbatim and NUL padded: the spec gives no
   character mapping, so a front end shows them as bytes. */
struct engine_officer
{
    u16 officer_id;                 /* the save slot; ENGINE_NO_OFFICER for an index out of range */
    u8  family_name[OFFICER_FAMILY_NAME_SIZE];
    u8  given_name[OFFICER_GIVEN_NAME_SIZE];
    u8  courtesy_name[OFFICER_COURTESY_NAME_SIZE];
    u16 appearance_year;
    u16 birth_year;
    u16 death_year;
    u8  leadership;
    u8  war;
    u8  intelligence;
    u8  politics;
    u8  charm;
    u8  force_id;                   /* 255 = none */
    u16 base_location;              /* city 0..41, gate or port 42..86, 0xFFFF = none */
    u16 current_position;           /* as base_location, or 87 + unit index */
    u8  status;                     /* 0 ruler, 1 viceroy, 2 governor, 3 regular, 6 free, 8 dead */
    u16 merit;
    u8  skill_number;               /* 255 = none */
    u8  compatibility;
    u16 father;
    u16 spouse;
    u16 sworn_brother_group_head;   /* 0xFFFF = none */
};

/* A city of the loaded game, as a front end sees it. */
struct engine_city
{
    u8  city_id;                    /* ENGINE_NO_CITY for an index out of range */
    u8  owner_force_id;             /* 255 = none */
    u16 commanding_officer_id;      /* 0xFFFF = none */
    u32 capacity_constant;
    u32 gold;
    u32 troops;
    u32 food;
    u32 equipment_stocks[CITY_EQUIPMENT_STOCK_COUNT];   /* spears, halberds, crossbows, war horses, rams, towers, catapults, 3 ships */
    u16 durability_maximum;         /* hypothesis in the spec */
    u8  public_order;               /* hypothesis in the spec */
};

/* The name of a city from the name index (Scenario.s11), as a front end
   sees it: the Big5 bytes of the entry, verbatim and NUL padded. */
struct engine_city_name
{
    u8 city_id;                           /* ENGINE_NO_CITY if no name index is loaded or the id is out of range */
    u8 name[NAME_INDEX_CITY_NAME_SIZE];
};

#define ENGINE_NO_OFFICER 0xFFFF
#define ENGINE_NO_CITY    0xFF

enum engine_load_status
{
    ENGINE_LOAD_OK,
    ENGINE_LOAD_NO_FILE_READER,     /* the engine was created without read_entire_file */
    ENGINE_LOAD_FILE_NOT_FOUND,     /* the reader returned nothing */
    ENGINE_LOAD_BAD_SAVE,           /* the file is not a save the loader can read; see detail */
    ENGINE_LOAD_BAD_NAME_INDEX,     /* the file is not a name index the loader can read; see detail */
    ENGINE_LOAD_STATUS_COUNT
};

struct engine_load_result
{
    engine_load_status status;
    u32                command_id;  /* 0 when the load failed; the game is then unchanged */
    const char        *detail;      /* a static string: why the save was refused, or "ok" */
};

/* The terrain as the renderer needs it: one kind and one level per cell,
   row-major by offset. The pointers stay valid for the life of the engine;
   version changes whenever a cell changes. Placeholder data until the
   spec's map format exists. */
struct engine_terrain_view
{
    u32       width;
    u32       height;
    const u8 *kind;
    const u8 *level;
    u32       version;
};

/* Queries */
typedef u32                 engine_turn_fn(void *engine);
typedef u32                 engine_entities_alive_fn(void *engine);
typedef u32                 engine_entity_capacity_fn(void *engine);
typedef u32                 engine_ledger_count_fn(void *engine);
typedef sim_event           engine_ledger_at_fn(void *engine, u32 index);
typedef engine_memory_stats engine_memory_fn(void *engine);
typedef engine_map_size     engine_map_size_fn(void *engine);
typedef u8                  engine_cell_cost_fn(void *engine, s32 q, s32 r);          /* 0 = impassable or off-map */
typedef u32                 engine_path_cost_fn(void *engine, s32 q0, s32 r0, s32 q1, s32 r1); /* HEX_NO_PATH if none */
typedef u32                 engine_officer_count_fn(void *engine);
typedef engine_officer      engine_officer_at_fn(void *engine, u32 index);   /* index < officer_count */
typedef u32                 engine_city_count_fn(void *engine);
typedef engine_city         engine_city_at_fn(void *engine, u32 index);      /* index < city_count */
typedef engine_city_name    engine_city_name_at_fn(void *engine, u32 index); /* index = city id 0..41 */
typedef engine_terrain_view engine_terrain_view_fn(void *engine);
/* Commands */
typedef u32                 engine_end_turn_fn(void *engine);                 /* returns the command id */
typedef u32                 engine_advance_turns_fn(void *engine, u32 count); /* returns events emitted */
typedef void                engine_ledger_clear_fn(void *engine);
typedef void                engine_set_cell_cost_fn(void *engine, s32 q, s32 r, u8 cost); /* editor/debug */
typedef void                engine_set_cell_terrain_fn(void *engine, s32 q, s32 r, u8 kind, u8 level); /* editor/debug */
/* Replaces the officers and cities with those of a .s11 save. A relative
   path is relative to the folder of the executable. */
typedef engine_load_result  engine_load_save_fn(void *engine, const char *path);
/* Replaces the city names with those of a name index (Scenario.s11). The
   officers and cities do not change. A relative path is relative to the
   folder of the executable. */
typedef engine_load_result  engine_load_name_index_fn(void *engine, const char *path);

struct engine_api
{
    void *engine;
    engine_turn_fn            *turn;
    engine_entities_alive_fn  *entities_alive;
    engine_entity_capacity_fn *entity_capacity;
    engine_ledger_count_fn    *ledger_count;
    engine_ledger_at_fn       *ledger_at;
    engine_memory_fn          *memory;
    engine_map_size_fn        *map_size;
    engine_cell_cost_fn       *cell_cost;
    engine_path_cost_fn       *path_cost;
    engine_officer_count_fn   *officer_count;
    engine_officer_at_fn      *officer_at;
    engine_city_count_fn      *city_count;
    engine_city_at_fn         *city_at;
    engine_city_name_at_fn    *city_name_at;
    engine_end_turn_fn        *end_turn;
    engine_advance_turns_fn   *advance_turns;
    engine_ledger_clear_fn    *ledger_clear;
    engine_set_cell_cost_fn   *set_cell_cost;
    engine_terrain_view_fn    *terrain_view;
    engine_set_cell_terrain_fn *set_cell_terrain;
    engine_load_save_fn       *load_save;
    engine_load_name_index_fn *load_name_index;
};

/* The real engine lives in code/engine/engine.cpp. */
struct engine;
struct engine_config
{
    u64 seed;
    u32 ledger_capacity;
    u32 map_width;        /* placeholder until the scenario format is in the spec */
    u32 map_height;
    u8  map_default_cost;
    platform_read_entire_file *read_entire_file;   /* zero: the load commands refuse with ENGINE_LOAD_NO_FILE_READER */
};
internal engine_config engine_default_config(void);
internal engine    *engine_create(memory_arena *permanent, memory_arena *transient, engine_config config);
internal engine_api engine_make_api(engine *e);

#endif /* ENGINE_H */
