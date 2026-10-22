/* The look of a facility on the map: the placeholder shape and the colour
   of each kind of facilities.tsv, by its key. A kind that the table does
   not know gets the office shape in grey. The scene draws the shape on the
   slot. The Hex map panel fills the slot hex with the colour. The counts
   of the panels show the built facilities with the builds in progress.
   Included by ui.cpp before the panels. */

struct ui_facility_look
{
    const char *key;
    u32         mesh;      /* render_mesh_id */
    u32         rgba;      /* the tint of the model and the fill of the hex */
};

global_variable const ui_facility_look ui_facility_looks[] =
{
    { "market",               RENDER_MESH_STALL,  IM_COL32(240, 180,  60, 255) },
    { "farm",                 RENDER_MESH_FIELD,  IM_COL32(120, 190,  70, 255) },
    { "barracks",             RENDER_MESH_HALL,   IM_COL32(200,  70,  60, 255) },
    { "smithy",               RENDER_MESH_FORGE,  IM_COL32(120, 120, 135, 255) },
    { "stable",               RENDER_MESH_SHED,   IM_COL32(170, 120,  70, 255) },
    { "workshop",             RENDER_MESH_FORGE,  IM_COL32(150, 100, 180, 255) },
    { "shipyard",             RENDER_MESH_DOCK,   IM_COL32( 60, 130, 200, 255) },
    { "mint",                 RENDER_MESH_SILO,   IM_COL32(230, 200,  80, 255) },
    { "granary",              RENDER_MESH_SILO,   IM_COL32(200, 160,  90, 255) },
    { "bronze_sparrow_tower", RENDER_MESH_TOWER,  IM_COL32(180, 120,  60, 255) },
    { "tally_office",         RENDER_MESH_OFFICE, IM_COL32(230, 230, 230, 255) },
    { "personnel_office",     RENDER_MESH_OFFICE, IM_COL32(100, 200, 200, 255) },
    { "plot_office",          RENDER_MESH_OFFICE, IM_COL32( 90,  60, 120, 255) },
    { "great_market",         RENDER_MESH_STALL,  IM_COL32(255, 150,  40, 255) },
    { "black_market",         RENDER_MESH_STALL,  IM_COL32( 60,  60,  60, 255) },
    { "diplomacy_office",     RENDER_MESH_OFFICE, IM_COL32(220, 140, 200, 255) },
    { "military_office",      RENDER_MESH_HALL,   IM_COL32(160,  40,  40, 255) },
    { "drill_ground",         RENDER_MESH_HALL,   IM_COL32(190, 160,  60, 255) },
    { "fish_market",          RENDER_MESH_STALL,  IM_COL32( 80, 200, 220, 255) },
    { "military_farm",        RENDER_MESH_FIELD,  IM_COL32( 70, 140,  60, 255) },
};

#define UI_FACILITY_MODEL_SCALE    1.6f    /* a built facility: the shapes are one unit wide, a hex is two */
#define UI_FACILITY_BUILDING_SCALE 1.0f    /* the facility under its scaffold */
#define UI_FACILITY_GLYPH_LIFT     1.3f    /* the glyph of a slot sits above the roof of its model */
#define UI_SCAFFOLD_RGBA           IM_COL32(150, 150, 150, 255)

internal ui_facility_look
ui_facility_look_of(const char *key)
{
    for (u32 index = 0; index < ArrayCount(ui_facility_looks); ++index)
    {
        if (strcmp(ui_facility_looks[index].key, key) == 0) return ui_facility_looks[index];
    }
    ui_facility_look unknown = { key, RENDER_MESH_OFFICE, IM_COL32(160, 160, 160, 255) };
    return unknown;
}

/* The same colour at a part of its brightness: a build in progress. */
internal u32
ui_rgba_dim(u32 rgba, f32 factor)
{
    u32 red   = (u32)((f32)(rgba & 0xFF) * factor);
    u32 green = (u32)((f32)((rgba >> 8) & 0xFF) * factor);
    u32 blue  = (u32)((f32)((rgba >> 16) & 0xFF) * factor);
    return (rgba & 0xFF000000u) | (blue << 16) | (green << 8) | red;
}

/* "5" or "5 (+2)": the built facilities and the builds in progress. */
internal void
ui_format_facility_count(char *out, u32 out_size, u32 built, u32 building)
{
    if (building) snprintf(out, out_size, "%u (+%u)", built, building);
    else          snprintf(out, out_size, "%u", built);
}
