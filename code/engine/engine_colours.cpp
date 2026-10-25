/* The flag colours of the factions: faction_colours.tsv of the pack and the
   faction_colour query (sim/faction_colours.h, docs/GRAPHICS.md). The spec
   gives no colour for a faction, so the colours are a choice of the clone.

   The engine assigns the rows again when the turn order or a ruler of the
   turn order changes. It keeps the answer until the next change, so that a
   query costs only the check of the turn order. Included by engine.cpp. */

/* Reads faction_colours.tsv. Without the file, or with a bad row, every
   faction gets grey. */
internal void
engine_load_faction_colours(engine *e, const char *folder)
{
    e->faction_palette = {};
    e->faction_colour_stamp = 0;
    tsv_table table = {};
    u32 bad_row = 0;
    arena_reset(&e->scratch);
    if (folder && engine_read_table(e, folder, "faction_colours.tsv", &table)) faction_palette_load(&e->faction_palette, &table, &bad_row);
    arena_reset(&e->scratch);
}

/* A hash of the turn order: the force id and the ruler of each faction. It
   is never 0, so 0 means "not assigned". */
internal u32
engine_faction_colour_stamp(engine *e)
{
    sim_turn_state *turns = &e->sim->turns;
    u32 hash = 2166136261u ^ turns->order_count;
    for (u32 index = 0; index < turns->order_count; ++index)
    {
        hash = (hash ^ turns->order[index].force_id) * 16777619u;
        hash = (hash ^ turns->order[index].leader_officer_id) * 16777619u;
    }
    return hash ? hash : 1;
}

internal void
engine_assign_faction_colours(engine *e)
{
    sim_turn_state *turns = &e->sim->turns;
    faction_colour_input factions[SIM_FORCE_CAPACITY];
    u8 rows[SIM_FORCE_CAPACITY];
    u32 count = turns->order_count < SIM_FORCE_CAPACITY ? turns->order_count : SIM_FORCE_CAPACITY;
    for (u32 index = 0; index < count; ++index)
    {
        factions[index] = {};
        factions[index].force_id = turns->order[index].force_id;
        officer_component *ruler = sim_find_officer(e->sim, turns->order[index].leader_officer_id);
        if (ruler) memcpy(factions[index].family_name, ruler->family_name, sizeof(factions[index].family_name));
    }
    faction_colours_assign(&e->faction_palette, factions, count, rows);
    for (u32 force = 0; force < SIM_FORCE_CAPACITY; ++force)
    {
        e->faction_colour_rows[force] = e->faction_palette.count ? (u8)(force % e->faction_palette.count) : FACTION_COLOUR_NO_ROW;
    }
    for (u32 index = 0; index < count; ++index) e->faction_colour_rows[factions[index].force_id] = rows[index];
}

/* A force outside the turn order (a faction that fell in this turn) keeps
   the row of its force id. */
internal u32
engine_api_faction_colour(void *opaque, u8 force_id)
{
    engine *e = (engine *)opaque;
    if (force_id == ENGINE_NO_FACTION || !e->faction_palette.count) return faction_rgb_to_rgba(FACTION_COLOUR_NONE_RGB);
    u32 stamp = engine_faction_colour_stamp(e);
    if (stamp != e->faction_colour_stamp)
    {
        engine_assign_faction_colours(e);
        e->faction_colour_stamp = stamp;
    }
    u8 row = e->faction_colour_rows[force_id];
    return faction_rgb_to_rgba(row == FACTION_COLOUR_NO_ROW ? FACTION_COLOUR_NONE_RGB : e->faction_palette.rgb[row]);
}

internal void
engine_bind_colours_api(engine_api *api)
{
    api->faction_colour = engine_api_faction_colour;
}
