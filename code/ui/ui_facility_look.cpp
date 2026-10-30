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
    u32         model;     /* render_model_id: the 3D model of the kind */
};

global_variable const ui_facility_look ui_facility_looks[] =
{
    { "market",               RENDER_MESH_STALL,  IM_COL32(240, 180,  60, 255), RENDER_MESH_STALL },
    { "farm",                 RENDER_MESH_FIELD,  IM_COL32(120, 190,  70, 255), RENDER_MESH_FIELD },
    { "barracks",             RENDER_MESH_HALL,   IM_COL32(200,  70,  60, 255), RENDER_MESH_HALL },
    { "smithy",               RENDER_MESH_FORGE,  IM_COL32(120, 120, 135, 255), RENDER_MESH_FORGE },
    { "stable",               RENDER_MESH_SHED,   IM_COL32(170, 120,  70, 255), RENDER_MESH_SHED },
    { "workshop",             RENDER_MESH_FORGE,  IM_COL32(150, 100, 180, 255), RENDER_MODEL_WORKSHOP },
    { "shipyard",             RENDER_MESH_DOCK,   IM_COL32( 60, 130, 200, 255), RENDER_MESH_DOCK },
    { "mint",                 RENDER_MESH_SILO,   IM_COL32(230, 200,  80, 255), RENDER_MODEL_MINT },
    { "granary",              RENDER_MESH_SILO,   IM_COL32(200, 160,  90, 255), RENDER_MESH_SILO },
    { "bronze_sparrow_tower", RENDER_MESH_TOWER,  IM_COL32(180, 120,  60, 255), RENDER_MESH_TOWER },
    { "tally_office",         RENDER_MESH_OFFICE, IM_COL32(230, 230, 230, 255), RENDER_MESH_OFFICE },
    { "personnel_office",     RENDER_MESH_OFFICE, IM_COL32(100, 200, 200, 255), RENDER_MODEL_PERSONNEL_OFFICE },
    { "plot_office",          RENDER_MESH_OFFICE, IM_COL32( 90,  60, 120, 255), RENDER_MODEL_PLOT_OFFICE },
    { "great_market",         RENDER_MESH_STALL,  IM_COL32(255, 150,  40, 255), RENDER_MODEL_GREAT_MARKET },
    { "black_market",         RENDER_MESH_STALL,  IM_COL32( 60,  60,  60, 255), RENDER_MODEL_BLACK_MARKET },
    { "diplomacy_office",     RENDER_MESH_OFFICE, IM_COL32(220, 140, 200, 255), RENDER_MODEL_DIPLOMACY_OFFICE },
    { "military_office",      RENDER_MESH_HALL,   IM_COL32(160,  40,  40, 255), RENDER_MODEL_MILITARY_OFFICE },
    { "drill_ground",         RENDER_MESH_HALL,   IM_COL32(190, 160,  60, 255), RENDER_MODEL_DRILL_GROUND },
    { "fish_market",          RENDER_MESH_STALL,  IM_COL32( 80, 200, 220, 255), RENDER_MODEL_FISH_MARKET },
    { "military_farm",        RENDER_MESH_FIELD,  IM_COL32( 70, 140,  60, 255), RENDER_MODEL_MILITARY_FARM },
};

#define UI_FACILITY_MODEL_SCALE    1.6f    /* a built facility: the shapes are one unit wide, a hex is two */
#define UI_FACILITY_BUILDING_SCALE 1.0f    /* the facility under its scaffold */
#define UI_FACILITY_GLYPH_LIFT     1.3f    /* the glyph of a slot sits above the roof of its model */
#define UI_SCAFFOLD_RGBA           IM_COL32(150, 150, 150, 255)
#define UI_SCAFFOLD_FACILITY_LEVEL 2       /* a facility gets the medium scaffold model, a field building the small one */

internal ui_facility_look
ui_facility_look_of(const char *key)
{
    for (u32 index = 0; index < ArrayCount(ui_facility_looks); ++index)
    {
        if (strcmp(ui_facility_looks[index].key, key) == 0) return ui_facility_looks[index];
    }
    ui_facility_look unknown = { key, RENDER_MESH_OFFICE, IM_COL32(160, 160, 160, 255), RENDER_MESH_OFFICE };
    return unknown;
}

/* U-001: the placeholder shape of a field building by its building_type
   (buildings.tsv). The colour is the colour of the owner. */
internal u32
ui_building_mesh(u8 type)
{
    switch (type)
    {
        case BUILDING_CAMP:     return RENDER_MESH_CAMP;
        case BUILDING_TOWER:    return RENDER_MESH_WATCHTOWER;
        case BUILDING_DRUM:
        case BUILDING_MUSIC:    return RENDER_MESH_STAGE;
        case BUILDING_MAZE:     return RENDER_MESH_STONES;
        case BUILDING_WALL:
        case BUILDING_DIKE:     return RENDER_MESH_WALL;
        default:                return RENDER_MESH_TRAP;   /* the fire seeds, the fire balls, the fire ship, the rockfall */
    }
}

/* The 3D model of a field building by the key of its kind (buildings.tsv).
   A kind that the table does not know uses the model of its shape. */
struct ui_building_look
{
    const char *key;
    u32         model;     /* render_model_id */
};

global_variable const ui_building_look ui_building_looks[] =
{
    { "fort",            RENDER_MODEL_FORT },
    { "citadel",         RENDER_MODEL_CITADEL },
    { "repeating_tower", RENDER_MODEL_REPEATING_TOWER },
    { "catapult_tower",  RENDER_MODEL_CATAPULT_TOWER },
    { "music_tower",     RENDER_MODEL_MUSIC_TOWER },
    { "stone_wall",      RENDER_MODEL_STONE_WALL },
    { "dike",            RENDER_MODEL_DIKE },
    { "fire_seed_2",     RENDER_MODEL_FIRE_SEED_2 },
    { "fire_seed_3",     RENDER_MODEL_FIRE_SEED_3 },
    { "fire_ball",       RENDER_MODEL_FIRE_BALL },
    { "fire_ball_2",     RENDER_MODEL_FIRE_BALL_2 },
    { "fire_ball_3",     RENDER_MODEL_FIRE_BALL_3 },
    { "fire_ship",       RENDER_MODEL_FIRE_SHIP },
    { "rockfall",        RENDER_MODEL_ROCKFALL },
};

internal u32
ui_building_model(const char *key, u32 mesh)
{
    for (u32 index = 0; index < ArrayCount(ui_building_looks); ++index)
    {
        if (strcmp(ui_building_looks[index].key, key) == 0) return ui_building_looks[index].model;
    }
    return mesh;
}

#define UI_BUILDING_NO_OWNER_RGBA IM_COL32(150, 150, 150, 255)   /* a trap or a natural trap: grey, as on the Hex map */

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
