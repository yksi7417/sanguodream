#ifndef SIM_H
#define SIM_H
/* Headless, deterministic simulation core. No Windows headers, no I/O, no
   ImGui. Everything lives in the permanent arena. Commands in, events out.

   Mechanics are implemented one at a time from spec-src/spec/mechanics/ with
   fixtures from spec-src/spec/fixtures/. Until a mechanic has a spec entry
   and fixtures, it does not exist here. */

#include "base.h"
#include "ecs/ecs.h"
#include "sim/hex.h"
#include "sim/inflate.h"
#include "sim/save_format.h"
#include "sim/scenario_format.h"

enum sim_event_kind
{
    SIM_EVENT_NONE,
    SIM_EVENT_TURN_ADVANCED,
    SIM_EVENT_SAVE_LOADED,     /* delta = officers loaded; the old officers and cities are gone */
    SIM_EVENT_COUNT
};

/* Every change the sim makes is an event with a cause, so a save-file diff
   from the original maps straight onto expected events. */
struct sim_event
{
    sim_event_kind kind;
    u32            turn;
    entity_id      subject;
    s32            delta;
    u32            cause;   /* id of the command that caused it, 0 for the turn itself */
};

struct sim_event_sink
{
    sim_event *events;
    u32        count;
    u32        capacity;
};

enum sim_command_kind
{
    SIM_COMMAND_NONE,
    SIM_COMMAND_END_TURN,
    SIM_COMMAND_LOAD_SAVE,     /* carries a decoded save: run it with sim_load_save, not sim_apply */
    SIM_COMMAND_COUNT
};

struct sim_command
{
    sim_command_kind kind;
    u32              id;      /* assigned by sim_make_command */
};

/* An officer of the loaded game: the fields of the officer record that the
   game uses (save-file.md, Officer record). Names are Big5 bytes, verbatim. */
struct officer_component
{
    u16 officer_id;                      /* the slot in the save */
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
    u8  force_id;
    u16 base_location;
    u16 current_position;
    u8  status;
    u16 merit;
    u8  skill_number;
    u8  compatibility;
    u16 father;
    u16 spouse;
    u16 sworn_brother_group_head;
    u8  unit_aptitudes[OFFICER_UNIT_APTITUDE_COUNT];
};

/* A city of the loaded game (save-file.md, City record). */
struct city_component
{
    u8  city_id;
    u8  owner_force_id;
    u16 commanding_officer_id;
    u32 capacity_constant;
    u32 gold;
    u32 troops;
    u32 food;
    u32 equipment_stocks[CITY_EQUIPMENT_STOCK_COUNT];
    u16 durability_maximum;              /* hypothesis */
    u8  public_order;                    /* hypothesis */
};

struct sim_state
{
    u64            rng;             /* seeded PRNG state; the only randomness source */
    u32            turn;
    u32            next_command_id;
    entity_manager entities;
    hex_map        map;             /* dimensions and costs come from the scenario; see sim_init_map */
    component_table<officer_component> officers;   /* one entity per officer of the loaded save */
    component_table<city_component>    cities;     /* one entity per city */
    /* more component tables are added mechanic by mechanic */
};

#define SIM_ENTITY_CAPACITY  4096
#define SIM_OFFICER_CAPACITY SAVE_OFFICER_SLOTS
#define SIM_CITY_CAPACITY    SAVE_CITY_COUNT

internal void        sim_init(sim_state *sim, memory_arena *arena, u64 seed);
internal void        sim_init_map(sim_state *sim, memory_arena *arena, u32 width, u32 height, u8 default_cost);
internal u32         sim_random_u32(sim_state *sim);
internal u32         sim_random_below(sim_state *sim, u32 bound);
internal sim_command sim_make_command(sim_state *sim, sim_command_kind kind);
internal void        sim_apply(sim_state *sim, sim_command command, sim_event_sink *events);
internal void        sim_advance_turn(sim_state *sim, sim_event_sink *events);
internal b32         sim_load_save(sim_state *sim, save_file *save, sim_command command, sim_event_sink *events);
internal void        sim_unload_world(sim_state *sim);
internal const char *sim_event_name(sim_event_kind kind);
internal const char *sim_command_name(sim_command_kind kind);

internal sim_event_sink sim_sink_from_arena(memory_arena *arena, u32 capacity);
internal void           sim_emit(sim_event_sink *sink, sim_event event);

#endif /* SIM_H */
