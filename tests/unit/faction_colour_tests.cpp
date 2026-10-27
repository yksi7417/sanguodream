/* Unit tests of code/sim/faction_colours.cpp: the parse of
   faction_colours.tsv, the colour distance, and the assignment of a row to
   each faction. The palettes of the shipped packs come from data\, and the
   factions are synthetic. The feature tests check the real scenarios
   through engine_api (tests/feature/colour_tests.cpp). */

/* data\<pack>\faction_colours.tsv, parsed. The executables live in build\. */
internal b32
unit_colour_pack_palette(const char *pack, memory_arena *arena, faction_palette *palette)
{
    char exe_dir[MAX_PATH], path[MAX_PATH];
    test_exe_dir(exe_dir, MAX_PATH);
    snprintf(path, MAX_PATH, "%s/../data/%s/faction_colours.tsv", exe_dir, pack);
    char *text = (char *)test_read_file(path, arena).contents;
    tsv_table table = {};
    u32 bad_row = 0;
    return text && tsv_parse(text, arena, &table) && faction_palette_load(palette, &table, &bad_row);
}

/* A synthetic faction with a family name of Big5 bytes. */
internal faction_colour_input
unit_colour_faction(u8 force_id, u8 first, u8 second)
{
    faction_colour_input faction = {};
    faction.force_id       = force_id;
    faction.family_name[0] = first;
    faction.family_name[1] = second;
    return faction;
}

/* True if no two factions share a row and each pair with the same family
   name is at least FACTION_COLOUR_SAME_NAME_DISTANCE apart. */
internal b32
unit_colour_rule_holds(const faction_palette *palette, const faction_colour_input *factions, const u8 *rows, u32 count)
{
    for (u32 index = 0; index < count; ++index)
    {
        if (rows[index] >= palette->count) return false;
        for (u32 other = 0; other < index; ++other)
        {
            if (rows[index] == rows[other]) return false;
            if (faction_same_family(factions[index].family_name, factions[other].family_name) &&
                faction_colour_distance(palette->rgb[rows[index]], palette->rgb[rows[other]]) < FACTION_COLOUR_SAME_NAME_DISTANCE) return false;
        }
    }
    return true;
}

internal void
unit_colour_distance_tests(test_counts *counts)
{
    TEST_CHECK(counts, faction_colour_distance(0x2EA043, 0x2EA043) == 0, "a colour is 0 from itself");
    TEST_CHECK(counts, faction_colour_distance(0x000000, 0xFFFFFF) >= 760 && faction_colour_distance(0x000000, 0xFFFFFF) <= 765,
               "black and white are approximately 765 apart, the largest distance");
    TEST_CHECK(counts, faction_colour_distance(0x2F5FD0, 0xD02828) == faction_colour_distance(0xD02828, 0x2F5FD0), "the distance is symmetric");
    TEST_CHECK(counts, faction_colour_distance(0x2EA043, 0x32A447) < FACTION_COLOUR_ANY_DISTANCE &&
                       faction_colour_distance(0x2EA043, 0xF09CC8) > FACTION_COLOUR_SAME_NAME_DISTANCE,
               "two greens are near; green and pink are far");
    TEST_CHECK(counts, faction_rgb_to_rgba(0x112233) == 0xFF332211u, "the RGBA8 word has the red in the low byte and an opaque alpha");
    u8 liu[OFFICER_FAMILY_NAME_SIZE] = { 0xBC, 0x42 }, liu_again[OFFICER_FAMILY_NAME_SIZE] = { 0xBC, 0x42 }, cao[OFFICER_FAMILY_NAME_SIZE] = { 0xB1, 0xE4 };
    u8 none[OFFICER_FAMILY_NAME_SIZE] = {};
    TEST_CHECK(counts, faction_same_family(liu, liu_again) && !faction_same_family(liu, cao) && !faction_same_family(none, none),
               "two 劉 have the same family name; 劉 and 曹 do not; two factions without a ruler do not");
}

internal void
unit_colour_load_tests(test_counts *counts, memory_arena *arena)
{
    faction_palette palette = {};
    tsv_table table = {};
    u32 bad_row = 0;
    char *text = content_test_text(arena, "id\tkey\trgb\tnote\n# a comment\n0\tblue\t2F5FD0\t-\n1\tgreen\t2ea043\t-\n");
    TEST_CHECK(counts, tsv_parse(text, arena, &table) && faction_palette_load(&palette, &table, &bad_row) && palette.count == 2 &&
                       palette.rgb[1] == 0x2EA043 && strcmp(palette.key[0], "blue") == 0,
               "faction_colours.tsv: two rows, the hexadecimal digits in either case");
    text = content_test_text(arena, "id\tkey\trgb\n0\tblue\t2F5FD0\n1\tblue_again\t2F5FD0\n");
    TEST_CHECK(counts, tsv_parse(text, arena, &table) && !faction_palette_load(&palette, &table, &bad_row) && bad_row == 1 && palette.count == 0,
               "refused: two rows with the same colour");
    text = content_test_text(arena, "id\tkey\trgb\n0\tblue\t2F5FD\n");
    TEST_CHECK(counts, tsv_parse(text, arena, &table) && !faction_palette_load(&palette, &table, &bad_row) && bad_row == 0,
               "refused: a colour without six digits");
    text = content_test_text(arena, "id\tkey\trgb\n1\tblue\t2F5FD0\n");
    TEST_CHECK(counts, tsv_parse(text, arena, &table) && !faction_palette_load(&palette, &table, &bad_row), "refused: an id that is not the row number");
}

/* The palettes of the shipped packs: every pair of rows at least
   FACTION_COLOUR_ANY_DISTANCE apart, and a row for each of the 47 forces. */
internal void
unit_colour_pack_tests(test_counts *counts, memory_arena *arena, const char *pack)
{
    faction_palette palette = {};
    b32 loaded = unit_colour_pack_palette(pack, arena, &palette);
    u32 nearest = 765;
    for (u32 row = 0; row < palette.count; ++row)
    {
        for (u32 other = 0; other < row; ++other)
        {
            u32 distance = faction_colour_distance(palette.rgb[row], palette.rgb[other]);
            if (distance < nearest) nearest = distance;
        }
    }
    char what[160];
    snprintf(what, sizeof(what), "%s: faction_colours.tsv loads with %u rows", pack, palette.count);
    TEST_CHECK(counts, loaded && palette.count >= 47, what);
    snprintf(what, sizeof(what), "%s: the nearest two rows are %u apart (at least %u)", pack, nearest, FACTION_COLOUR_ANY_DISTANCE);
    TEST_CHECK(counts, loaded && nearest >= FACTION_COLOUR_ANY_DISTANCE, what);
}

internal void
unit_colour_assign_tests(test_counts *counts, memory_arena *arena)
{
    faction_palette palette = {};
    TEST_CHECK(counts, unit_colour_pack_palette("placeholder", arena, &palette), "the palette of the placeholder pack loads");

    /* Different family names: each faction keeps its own row. */
    faction_colour_input factions[8];
    u8 rows[8];
    factions[0] = unit_colour_faction(0, 0xB1, 0xE4);   /* 曹 */
    factions[1] = unit_colour_faction(2, 0xAE, 0x5D);   /* 孫 */
    factions[2] = unit_colour_faction(7, 0xB0, 0x4B);   /* 袁 */
    faction_colours_assign(&palette, factions, 3, rows);
    TEST_CHECK(counts, rows[0] == 0 && rows[1] == 2 && rows[2] == 7, "factions of different family names keep the rows of their force ids");

    /* Eight factions of 劉 on forces whose rows hold near greens, so the
       engine moves some of them. */
    u8 forces[8] = { 1, 19, 26, 34, 40, 46, 25, 39 };
    for (u32 index = 0; index < 8; ++index) factions[index] = unit_colour_faction(forces[index], 0xBC, 0x42);
    faction_colours_assign(&palette, factions, 8, rows);
    TEST_CHECK(counts, rows[0] == 1, "the first 劉 (the smallest force id) keeps its own row");
    TEST_CHECK(counts, unit_colour_rule_holds(&palette, factions, rows, 8),
               "eight factions of 劉: all colours differ, and each pair is at least FACTION_COLOUR_SAME_NAME_DISTANCE apart");

    /* The order of the list does not change the answer. */
    faction_colour_input reversed[8];
    u8 reversed_rows[8];
    for (u32 index = 0; index < 8; ++index) reversed[index] = factions[7 - index];
    faction_colours_assign(&palette, reversed, 8, reversed_rows);
    b32 same = true;
    for (u32 index = 0; index < 8; ++index) same = same && reversed_rows[index] == rows[7 - index];
    TEST_CHECK(counts, same, "the factions get the same rows in any order of the list");

    /* A force id past the table, and a table too small for the list. */
    factions[0] = unit_colour_faction(200, 0xB1, 0xE4);
    factions[1] = unit_colour_faction(0, 0, 0);
    faction_colours_assign(&palette, factions, 2, rows);
    TEST_CHECK(counts, rows[0] < palette.count && rows[1] == 0 && rows[0] != rows[1], "a force id past the table gets a free row");
    faction_palette small = {};
    small.count  = 1;
    small.rgb[0] = 0x2F5FD0;
    faction_colours_assign(&small, factions, 2, rows);
    TEST_CHECK(counts, rows[0] == 0 && rows[1] == 0, "a table with one row gives that row to each faction");
}

internal void
unit_faction_colour_tests(test_counts *counts, memory_arena *arena)
{
    unit_colour_distance_tests(counts);
    unit_colour_load_tests(counts, arena);
    unit_colour_pack_tests(counts, arena, "placeholder");
    unit_colour_pack_tests(counts, arena, "civil_war");
    unit_colour_assign_tests(counts, arena);
}
