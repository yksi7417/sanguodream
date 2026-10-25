/* The flag colours of the factions (faction_colours.h). */

/* Reads six hexadecimal digits RRGGBB. */
internal b32
faction_parse_rgb(const char *text, u32 *rgb)
{
    u32 value = 0, digits = 0;
    for (; text[digits]; ++digits)
    {
        char c = text[digits];
        u32 digit;
        if      (c >= '0' && c <= '9') digit = (u32)(c - '0');
        else if (c >= 'A' && c <= 'F') digit = (u32)(c - 'A' + 10);
        else if (c >= 'a' && c <= 'f') digit = (u32)(c - 'a' + 10);
        else return false;
        value = (value << 4) | digit;
    }
    if (digits != 6) return false;
    *rgb = value;
    return true;
}

/* The columns are id (the row number), key and rgb. Other columns are notes
   for the reader. Two rows with the same colour are a bad row. */
internal b32
faction_palette_load(faction_palette *palette, tsv_table *table, u32 *bad_row)
{
    palette->count = 0;
    if (table->row_count > FACTION_COLOUR_CAPACITY) { *bad_row = FACTION_COLOUR_CAPACITY; return false; }
    for (u32 row = 0; row < table->row_count; ++row)
    {
        u32 id = 0, rgb = 0;
        const char *key = tsv_cell(table, row, "key");
        b32 good = tsv_cell_u32(table, row, "id", &id) && id == row && key[0] && strlen(key) < FACTION_COLOUR_KEY_SIZE &&
                   faction_parse_rgb(tsv_cell(table, row, "rgb"), &rgb);
        for (u32 other = 0; good && other < row; ++other) good = palette->rgb[other] != rgb;
        if (!good)
        {
            *bad_row = row;
            palette->count = 0;
            return false;
        }
        palette->rgb[row] = rgb;
        memcpy(palette->key[row], key, strlen(key) + 1);
        palette->count = row + 1;
    }
    return true;
}

/* The "redmean" distance of two sRGB colours: the red, green and blue
   differences with weights that follow the mean red. It is a simple
   approximation of the distance that the eye sees. 0 is the same colour,
   765 is black against white. */
internal u32
faction_colour_distance(u32 rgb_a, u32 rgb_b)
{
    s32 red_a = (s32)((rgb_a >> 16) & 0xFF), green_a = (s32)((rgb_a >> 8) & 0xFF), blue_a = (s32)(rgb_a & 0xFF);
    s32 red_b = (s32)((rgb_b >> 16) & 0xFF), green_b = (s32)((rgb_b >> 8) & 0xFF), blue_b = (s32)(rgb_b & 0xFF);
    s32 mean_red = (red_a + red_b) / 2;
    s32 red = red_a - red_b, green = green_a - green_b, blue = blue_a - blue_b;
    /* 256 times (2 + r/256) dR^2 + 4 dG^2 + (2 + (255 - r)/256) dB^2 */
    u64 scaled = (u64)((512 + mean_red) * red * red) + (u64)(1024 * green * green) + (u64)((767 - mean_red) * blue * blue);
    u64 square = scaled / 256;
    u64 root = 0;
    while ((root + 1) * (root + 1) <= square) root++;
    return (u32)root;
}

/* Two family names are the same if both have a character and all bytes match. */
internal b32
faction_same_family(const u8 *family_a, const u8 *family_b)
{
    if (!family_a[0] || !family_b[0]) return false;
    return memcmp(family_a, family_b, OFFICER_FAMILY_NAME_SIZE) == 0;
}

/* True if the faction can take the row. No faction before it has the row.
   With check_family, each faction before it with the same family name
   has a colour at least FACTION_COLOUR_SAME_NAME_DISTANCE away. */
internal b32
faction_row_fits(const faction_palette *palette, const faction_colour_input *factions, const u32 *order, const u8 *rows,
                 u32 done, u32 index, u32 row, b32 check_family)
{
    for (u32 before = 0; before < done; ++before)
    {
        u32 other = order[before];
        if (rows[other] == row) return false;
        if (check_family && faction_same_family(factions[index].family_name, factions[other].family_name) &&
            faction_colour_distance(palette->rgb[row], palette->rgb[rows[other]]) < FACTION_COLOUR_SAME_NAME_DISTANCE) return false;
    }
    return true;
}

/* True if a faction of the list has the force id. */
internal b32
faction_force_listed(const faction_colour_input *factions, u32 count, u32 force_id)
{
    for (u32 index = 0; index < count; ++index) if (factions[index].force_id == force_id) return true;
    return false;
}

/* The factions take their rows in the order of their force ids. A faction
   that cannot keep its own row looks for a row after it, in this order:
   1. a free row of a force id that the list does not have, with a far colour;
   2. any free row with a far colour;
   3. any free row;
   4. its own row, if the table is too small for the list. */
internal void
faction_colours_assign(const faction_palette *palette, const faction_colour_input *factions, u32 count, u8 *rows)
{
    u32 order[256];
    if (count > ArrayCount(order)) count = ArrayCount(order);
    for (u32 index = 0; index < count; ++index)
    {
        u32 place = index;
        while (place > 0 && factions[order[place - 1]].force_id > factions[index].force_id) { order[place] = order[place - 1]; place--; }
        order[place] = index;
        rows[index] = FACTION_COLOUR_NO_ROW;
    }
    if (!palette->count) return;
    for (u32 done = 0; done < count; ++done)
    {
        u32 index = order[done];
        u32 own = factions[index].force_id;
        if (own < palette->count && faction_row_fits(palette, factions, order, rows, done, index, own, true))
        {
            rows[index] = (u8)own;
            continue;
        }
        u32 start = own < palette->count ? own + 1 : 0, found = FACTION_COLOUR_NO_ROW;
        for (u32 pass = 0; pass < 3 && found == FACTION_COLOUR_NO_ROW; ++pass)
        {
            for (u32 step = 0; step < palette->count && found == FACTION_COLOUR_NO_ROW; ++step)
            {
                u32 row = (start + step) % palette->count;
                if (pass == 0 && faction_force_listed(factions, count, row)) continue;
                if (faction_row_fits(palette, factions, order, rows, done, index, row, pass < 2)) found = row;
            }
        }
        if (found == FACTION_COLOUR_NO_ROW) found = own < palette->count ? own : own % palette->count;
        rows[index] = (u8)found;
    }
}

internal u32
faction_rgb_to_rgba(u32 rgb)
{
    u32 red = (rgb >> 16) & 0xFF, green = (rgb >> 8) & 0xFF, blue = rgb & 0xFF;
    return 0xFF000000u | (blue << 16) | (green << 8) | red;
}
