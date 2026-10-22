/* The state of a save on the real map (M11; formats/save-file.md): the
   building array at 0x0008A3 puts the facilities, the military buildings,
   the traps, the dikes, the rockfalls and the event sites on their hexes
   and gives each base its durability; the troop unit array at 0x086B71
   gives the units. save_types.tsv of the pack says what each facility type
   is. Without the real map or without that table, a save puts nothing on
   the map. Included by sim.cpp after the other rule files. */

#define SAVE_BUILDING_EMPTY_TYPE 255   /* the type of an empty record (observed in sample.s11; Q-212) */
#define SIM_SAVE_UNIT_RINGS      10    /* the search for a hex next to the home base of a unit */

struct sim_save_map_counts
{
    u32 facilities, buildings, naturals, relics, bases, units, skipped;
    u32 unit_fallbacks;   /* units whose hex of the save is off the map, not enterable or taken */
    u32 unknown_classes;  /* units whose land armament (+36) is no unit type of the pack */
    u32 transports;       /* units whose list is the cargo of a transport */
    u32 fires;            /* hexes that burn */
};

/* The city and the slot of a slot hex, or SIM_NO_CITY. */
internal u8
sim_slot_at(sim_state *sim, hex_coord hex, u32 *slot_index)
{
    u32 cell = sim_map_cell(sim, hex);
    if (cell == U32_MAX || !(sim->map_layers.flags[cell] & SIM_MAP_FLAG_SLOT)) return SIM_NO_CITY;
    u8 city_id = sim_map_region_city(sim, sim->map_layers.region[cell]);
    if (city_id >= SAVE_CITY_COUNT) return SIM_NO_CITY;
    sim_city_site *site = sim->sites + city_id;
    for (u32 slot = 0; slot < site->slot_count; ++slot)
    {
        if (hex_equal(site->slots[slot].hex, hex)) { *slot_index = slot; return city_id; }
    }
    return SIM_NO_CITY;
}

/* Records 0..86: the durability of each base. The owner of a city stays
   that of the city record: the byte +2 of a city disagrees with it in 20
   of 42 cities of sample.s11 (Q-212). The owner of a gate or a port comes
   from its own record at 0x08428D (sim_load_save_base_records). */
internal void
sim_load_save_bases(sim_state *sim, save_file *save, sim_save_map_counts *counts)
{
    for (u32 index = 0; index < SAVE_BUILDING_BASE_COUNT && index < save->building_count; ++index)
    {
        building_record *record = save->buildings + index;
        if (record->type >= CONTENT_SAVE_TYPE_COUNT || sim->content.save_types[record->type].kind != SAVE_TYPE_BASE) continue;
        if (index < SAVE_CITY_COUNT)
        {
            city_component *city = sim_find_city(sim, (u8)index);
            if (city) { city->durability = record->durability; counts->bases++; }
            continue;
        }
        for (u32 base = 0; base < sim->content.base_count; ++base)
        {
            if (sim->content.bases[base].id != index || !sim->bases[base].exists) continue;
            sim->bases[base].durability = record->durability;
            counts->bases++;
        }
    }
}

/* One facility, building, trap or event site of a record after 86. */
internal void
sim_load_save_building(sim_state *sim, building_record *record, hex_coord *relic_hexes, u8 *relic_kinds, sim_save_map_counts *counts)
{
    hex_offset offset;
    offset.col = (s32)record->x;
    offset.row = (s32)record->y;
    hex_coord hex = hex_from_offset(offset);
    save_type type = sim->content.save_types[record->type];
    if (!hex_map_contains(&sim->map, hex)) type.kind = SAVE_TYPE_NONE;
    u8 force = record->owner_force_id == OFFICER_FORCE_NONE ? (u8)SIM_NO_FORCE : record->owner_force_id;
    switch (type.kind)
    {
        case SAVE_TYPE_FACILITY:
        {
            u32 slot_index = 0;
            u8 city_id = sim_slot_at(sim, hex, &slot_index);
            if (city_id == SIM_NO_CITY) { counts->skipped++; return; }
            sim_slot *slot = sim->sites[city_id].slots + slot_index;
            slot->state      = SIM_SLOT_BUILT;
            slot->facility   = type.row;
            slot->level      = type.level;
            slot->durability = record->durability;
            slot->turns_left = 0;
            counts->facilities++;
            return;
        }
        case SAVE_TYPE_BUILDING:
            if (sim_add_building(sim, type.row, force, hex, record->durability, SIM_NO_CITY) == SIM_NO_BUILDING) counts->skipped++;
            else counts->buildings++;
            return;
        case SAVE_TYPE_NATURAL:
        {
            u32 id = sim_building_at(sim, hex);
            if (id == SIM_NO_BUILDING || sim->buildings[id].kind != type.row) { counts->skipped++; return; }
            sim->buildings[id].durability = record->durability;
            sim->buildings[id].complete   = record->durability >= sim->content.buildings[type.row].durability;
            counts->naturals++;
            return;
        }
        case SAVE_TYPE_TEMPLE:
        case SAVE_TYPE_RUIN:
            if (counts->relics >= SIM_RELIC_COUNT) { counts->skipped++; return; }
            relic_hexes[counts->relics] = hex;
            relic_kinds[counts->relics] = (u8)(type.kind == SAVE_TYPE_TEMPLE ? RELIC_TEMPLE : RELIC_RUIN);
            counts->relics++;
            return;
        default:
            counts->skipped++;
            return;
    }
}

/* The hex of the home base of a unit: a city centre, a gate or a port. */
internal b32
sim_home_base_hex(sim_state *sim, u16 location, hex_coord *out)
{
    if (location < SAVE_CITY_COUNT) { *out = sim->sites[location].hex; return true; }
    for (u32 base = 0; base < sim->content.base_count; ++base)
    {
        if (sim->content.bases[base].id == location && sim->bases[base].exists) { *out = sim->bases[base].hex; return true; }
    }
    return false;
}

/* The hex of the unit record (+23 x, +25 y; spec 7fff546), if it is on the
   map, a unit can enter its terrain and no other unit stands there. */
internal b32
sim_save_unit_record_hex(sim_state *sim, unit_record *record, hex_coord *out)
{
    hex_offset offset;
    offset.col = (s32)record->x;
    offset.row = (s32)record->y;
    hex_coord hex = hex_from_offset(offset);
    u32 cell = sim_map_cell(sim, hex);
    if (cell == U32_MAX) return false;
    terrain_kind *terrain = sim_terrain_at(sim, hex);
    if (!terrain || !terrain->enterable || sim_unit_at(sim, hex) != SIM_NO_UNIT) return false;
    *out = hex;
    return true;
}

/* The fallback when the hex of the record does not work (ASSUMED, the old
   M11 rule): the first free hex around the home base of the leader, ring
   by ring. */
internal b32
sim_save_unit_hex(sim_state *sim, u16 home, u8 force, hex_coord *out)
{
    hex_coord center = {};
    if (!sim_home_base_hex(sim, home, &center)) return false;
    for (s32 ring = 1; ring <= SIM_SAVE_UNIT_RINGS; ++ring)
    {
        hex_coord hex = center;
        for (s32 step = 0; step < ring; ++step) hex = hex_neighbor(hex, 4);
        for (u32 side = 0; side < HEX_DIRECTION_COUNT; ++side)
        {
            for (s32 step = 0; step < ring; ++step)
            {
                if (sim_entry_cost(sim, force, MOVEMENT_FOOT, hex, SIM_NO_UNIT) && sim_city_at(sim, hex) == SIM_NO_CITY &&
                    sim_base_at(sim, hex) == SIM_NO_BASE)
                {
                    *out = hex;
                    return true;
                }
                hex = hex_neighbor(hex, side);
            }
        }
    }
    return false;
}

/* The unit type of a row of movement-costs.tsv: the land type with that
   armament row (record +36), or the ship type with that ship row (record
   +41). CONTENT_NO_UNIT_TYPE if no type has it. */
internal u8
sim_save_unit_type(sim_state *sim, u8 row, b32 ship)
{
    for (u32 index = 0; index < sim->content.unit_type_count; ++index)
    {
        unit_type_kind *kind = sim->content.unit_types + index;
        if (kind->transport || (kind->movement == MOVEMENT_NAVY) != (ship != 0)) continue;
        if ((ship ? kind->ship : kind->armament) == row) return (u8)index;
    }
    return CONTENT_NO_UNIT_TYPE;
}

/* The city stock of a kind of the equipment list. The kind is an armament
   id of movement-costs.tsv (spec 84714de); the weapon of weapons.tsv with
   the same key gives the stock. 劍 and 走舸 have no weapon, so no stock. */
internal u8
sim_save_armament_stock(sim_state *sim, u8 kind)
{
    if (kind >= sim->content.armament_count) return 0xFF;
    const char *key = sim->content.armaments[kind].key;
    for (u32 index = 0; index < sim->content.weapon_count; ++index)
    {
        if (strcmp(sim->content.weapons[index].key, key) == 0) return (u8)sim->content.weapons[index].stock;
    }
    return 0xFF;
}

/* A transport (輸送隊): the land armament is 0 and the list holds the
   armament ids 1 to 11 in order (save-file.md, Q-217). */
internal b32
sim_save_unit_is_transport(unit_record *record)
{
    if (record->armament != 0) return false;
    for (u32 entry = 0; entry < SAVE_UNIT_EQUIPMENT_COUNT; ++entry)
    {
        if (record->equipment[entry].kind != entry + 1) return false;
    }
    return true;
}

/* The units of the save: each unit that an officer rides in (officer +98 =
   87 + the unit index). The record gives the leader, two deputies, the
   troops, the morale, the gold, the grain, the state, the task, the target,
   the hex, the land class (+36) and the equipment list (+41). The unit id
   is the unit index, so the positions of the officers stay right.
   - A transport carries the cargo of its list. Its target city is its
     destination when its task is 移動.
   - A battle unit owns the ship of its first entry when the count is 1 or
     more (a 樓船). Without one it is a 走舸 on water. On 河 and 海 it moves
     and fights as its ship, on land as its land class (sim_unit_cost_type). */
internal void
sim_load_save_units(sim_state *sim, save_file *save, sim_save_map_counts *counts)
{
    b32 used[SAVE_UNIT_CAPACITY] = {};
    for (u32 slot = 0; slot < save->officer_slot_count; ++slot)
    {
        u16 position = save->officers[slot].current_position;
        if (position != OFFICER_LOCATION_NONE && position >= SAVE_UNIT_LOCATION_BASE && position - SAVE_UNIT_LOCATION_BASE < SAVE_UNIT_CAPACITY)
        {
            used[position - SAVE_UNIT_LOCATION_BASE] = true;
        }
    }
    for (u32 index = 0; index < save->unit_count && index < SIM_UNIT_CAPACITY; ++index)
    {
        if (!used[index]) continue;
        unit_record *record = save->units + index;
        officer_component *leader = sim_find_officer(sim, record->officers[0]);
        hex_coord hex = {};
        if (!leader) { counts->skipped++; continue; }
        if (!sim_save_unit_record_hex(sim, record, &hex))
        {
            if (!sim_save_unit_hex(sim, leader->base_location, leader->force_id, &hex)) { counts->skipped++; continue; }
            counts->unit_fallbacks++;
        }
        sim_unit *unit = sim->units + index;
        *unit = {};
        unit->alive         = true;
        unit->force_id      = leader->force_id;
        unit->hex           = hex;
        b32 transport = sim_save_unit_is_transport(record);
        unit->unit_type     = transport ? content_find_unit_type(&sim->content, "transport") : sim_save_unit_type(sim, record->armament, false);
        unit->ship_type     = CONTENT_NO_UNIT_TYPE;
        if (transport)
        {
            for (u32 entry = 0; entry < SAVE_UNIT_EQUIPMENT_COUNT; ++entry)
            {
                u8 stock = sim_save_armament_stock(sim, record->equipment[entry].kind);
                if (stock < CITY_EQUIPMENT_STOCK_COUNT) unit->cargo[stock] += record->equipment[entry].count;
            }
            counts->transports++;
        }
        else if (record->equipment[0].count)
        {
            unit->ship_type = sim_save_unit_type(sim, record->equipment[0].kind, true);   /* the kind is the armament id of the ship */
        }
        unit->has_ship      = unit->ship_type != CONTENT_NO_UNIT_TYPE;
        if (unit->unit_type == CONTENT_NO_UNIT_TYPE) counts->unknown_classes++;
        unit->soldiers      = record->troops;
        unit->morale        = record->morale;
        unit->gold          = record->gold;
        unit->grain         = record->grain;
        if (record->state == SAVE_UNIT_STATE_CONFUSED)
        {
            unit->state       = UNIT_STATE_CONFUSED;
            unit->state_turns = 0;   /* no count: the unit rolls its recovery from its next turn (Q-218) */
        }
        /* G2-004: a unit whose leader acted in this turn has acted (the plate shows 完). */
        if (record->officers[0] < save->officer_slot_count && save->officers[record->officers[0]].has_acted_this_turn)
        {
            unit->acted_turn = sim->turn + 1;
        }
        unit->home_city     = leader->base_location < SAVE_CITY_COUNT ? (u8)leader->base_location : (u8)SIM_NO_CITY;
        unit->destination   = SIM_NO_CITY;
        if (record->task == SAVE_UNIT_TASK_MOVE && record->target < SAVE_CITY_COUNT) unit->destination = (u8)record->target;   /* a city; a gate or port target is not kept */
        for (u32 place = 0; place < SIM_COMMAND_OFFICERS; ++place)
        {
            u16 officer = place < SAVE_UNIT_OFFICER_SLOTS ? record->officers[place] : OFFICER_ID_NONE;
            unit->officers[place] = sim_find_officer(sim, officer) ? officer : (u16)SIM_NO_OFFICER;
        }
        if (index + 1 > sim->unit_count) sim->unit_count = index + 1;
        counts->units++;
    }
}

/* The per-hex block (save-file.md, spec b3ed57d): the turns of fire left
   go into the fire count of the hex, and the marks 16 and 32 are the
   event sites. ASSUMED (Q-205): 16 is a temple and 32 a ruin. */
internal void
sim_load_save_hex_states(sim_state *sim, save_file *save, hex_coord *relic_hexes, u8 *relic_kinds, sim_save_map_counts *counts)
{
    for (u32 index = 0; index < save->hex_state_count; ++index)
    {
        u32 state = save->hex_states[index];
        if (!state) continue;
        hex_offset offset;
        offset.col = (s32)(index / SAVE_HEX_SIDE);   /* x * 200 + y: column first */
        offset.row = (s32)(index % SAVE_HEX_SIDE);
        hex_coord hex = hex_from_offset(offset);
        if (!hex_map_contains(&sim->map, hex)) continue;
        u32 turns = state & SAVE_HEX_FIRE_MASK;
        if (turns && sim->fire)
        {
            sim->fire[hex_map_index(&sim->map, hex)] = (u8)turns;
            counts->fires++;
        }
        if ((state & (SAVE_HEX_SITE_LOW | SAVE_HEX_SITE_HIGH)) && counts->relics < SIM_RELIC_COUNT)
        {
            relic_hexes[counts->relics] = hex;
            relic_kinds[counts->relics] = (u8)((state & SAVE_HEX_SITE_LOW) ? RELIC_TEMPLE : RELIC_RUIN);
            counts->relics++;
        }
    }
}

/* Puts the state of the save on the map and ledgers one event: delta =
   the facilities, buildings and traps placed; detail = the units. */
internal void
sim_load_save_map(sim_state *sim, save_file *save, u32 cause, sim_event_sink *events)
{
    if (!sim->map_layers.loaded || !sim->content.has_save_types || !save->buildings) return;
    sim_save_map_counts counts = {};
    sim_load_save_bases(sim, save, &counts);
    hex_coord relic_hexes[SIM_RELIC_COUNT];
    u8 relic_kinds[SIM_RELIC_COUNT];
    for (u32 index = SAVE_BUILDING_BASE_COUNT; index < save->building_count; ++index)
    {
        building_record *record = save->buildings + index;
        if (record->type == SAVE_BUILDING_EMPTY_TYPE) continue;
        sim_load_save_building(sim, record, relic_hexes, relic_kinds, &counts);
    }
    if (save->hex_states) sim_load_save_hex_states(sim, save, relic_hexes, relic_kinds, &counts);
    /* The event sites of the save take the place of the random pick. */
    if (counts.relics)
    {
        for (u32 index = 0; index < SIM_RELIC_COUNT; ++index)
        {
            sim->relics[index] = {};
            if (index >= counts.relics) continue;
            sim->relics[index].alive = true;
            sim->relics[index].kind  = relic_kinds[index];
            sim->relics[index].hex   = relic_hexes[index];
        }
    }
    if (save->units) sim_load_save_units(sim, save, &counts);
    /* G2-027: a transport on its way (任務 移動) keeps a long order to its
       city, and moves at 進行 (orders.cpp). */
    for (u32 unit_id = 0; unit_id < sim->unit_count; ++unit_id)
    {
        sim_unit *unit = sim->units + unit_id;
        if (!unit->alive || unit->destination == SIM_NO_CITY || unit->destination == sim_city_at(sim, unit->hex)) continue;
        sim_give_unit_order(sim, unit_id, UNIT_ORDER_MOVE, sim_travel_base_hex(sim, unit->destination), SIM_NO_UNIT, cause, events);
    }
    sim_event event = sim_event_make(sim, SIM_EVENT_SAVE_MAP_LOADED, cause);
    event.delta   = (s32)(counts.facilities + counts.buildings + counts.naturals + counts.relics);
    event.detail  = counts.units;
    event.subject = counts.unit_fallbacks;
    /* Assumed: a fallback hex, a land class that the pack lacks, or the kind of an event site (Q-205). */
    event.assumed = counts.unit_fallbacks > 0 || counts.unknown_classes > 0 || counts.relics > 0;
    sim_emit(events, event);
}
