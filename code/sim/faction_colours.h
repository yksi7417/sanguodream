#ifndef FACTION_COLOURS_H
#define FACTION_COLOURS_H
/* The flag colours of the factions: data/<pack>/faction_colours.tsv. The
   spec gives no colour for a faction, so the table is a choice of the
   clone (docs/GRAPHICS.md, "Faction colours"). Row N is the colour of
   force N. The other rows are the spare colours.

   The engine gives each faction of the turn order one row of the table.
   A faction keeps its own row. It moves to a spare row in two cases:
   - a faction with a smaller force id already has that row;
   - a faction with a smaller force id has the same family name and a
     near colour.
   Thus the colour of a faction comes from the force ids and the family
   names alone. The same factions get the same colours after a save and
   a load.

   This module has no I/O. The engine reads the file. */

#include "base.h"
#include "sim/tables.h"
#include "sim/save_format.h"

#define FACTION_COLOUR_CAPACITY      64
#define FACTION_COLOUR_KEY_SIZE      32
#define FACTION_COLOUR_NO_ROW        0xFF
/* The distances are "redmean" distances of two sRGB colours (0 to 765).
   A palette of the shipped packs keeps every pair of rows at least
   FACTION_COLOUR_ANY_DISTANCE apart. Two factions with the same family
   name get colours at least FACTION_COLOUR_SAME_NAME_DISTANCE apart. */
#define FACTION_COLOUR_ANY_DISTANCE       60
#define FACTION_COLOUR_SAME_NAME_DISTANCE 200
#define FACTION_COLOUR_NONE_RGB      0x787878u   /* no faction: grey */

struct faction_palette
{
    u32  rgb[FACTION_COLOUR_CAPACITY];   /* 0xRRGGBB */
    char key[FACTION_COLOUR_CAPACITY][FACTION_COLOUR_KEY_SIZE];
    u32  count;
};

/* One faction that needs a colour: its force id and the family name of
   its ruler. The name is Big5 bytes, NUL padded. It is all zero if the
   faction has no ruler. */
struct faction_colour_input
{
    u8 force_id;
    u8 family_name[OFFICER_FAMILY_NAME_SIZE];
};

internal b32 faction_palette_load(faction_palette *palette, tsv_table *table, u32 *bad_row);
internal u32 faction_colour_distance(u32 rgb_a, u32 rgb_b);
internal b32 faction_same_family(const u8 *family_a, const u8 *family_b);
/* Writes the row of each faction to rows[index]. The factions can come in
   any order. */
internal void faction_colours_assign(const faction_palette *palette, const faction_colour_input *factions, u32 count, u8 *rows);
/* RGBA8 with the red in the low byte, as render_mesh_instance.tint and IM_COL32. */
internal u32 faction_rgb_to_rgba(u32 rgb);

#endif /* FACTION_COLOURS_H */
