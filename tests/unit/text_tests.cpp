/* Unit tests for code/text: the Big5 table, the UTF-8 encoder, the TSV
   tables and the locale layer. The pairs come from data/unicode/BIG5.TXT.
   The locale tests read files through an in-memory reader, not the disk.
   The strings use \x escapes, because the compiler does not read the
   source as UTF-8. */

/* UTF-8 of the characters in the tests. */
#define TEST_UTF8_CAO1   "\xE6\x9B\xB9"   /* U+66F9, Big5 B1E4 */
#define TEST_UTF8_CAO2   "\xE6\x93\x8D"   /* U+64CD, Big5 BEDE */
#define TEST_UTF8_FFFD   "\xEF\xBF\xBD"   /* U+FFFD */
#define TEST_UTF8_WINDOW "\xE8\xA6\x96\xE7\xAA\x97"   /* U+8996 U+7A97 */

/* ---- the in-memory reader --------------------------------------------- */

struct test_text_file
{
    const char *path;
    const char *contents;
};

global_variable const test_text_file test_text_files[] =
{
    { "root\\pack\\text\\locales.tsv",
      "code\tname\n"
      "zh-TW\t\xE7\xB9\x81\xE9\xAB\x94\xE4\xB8\xAD\xE6\x96\x87\n"
      "en\tEnglish\n"
      "ko\t\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4\n"
      "../bad\tnot a locale code\n" },
    { "root\\pack\\text\\en.tsv",
      "id\ttext\tnote\n"
      "menu.windows\tWindows\tthe main menu\n"
      "only.en\tOnly English\n" },
    { "root\\pack\\text\\zh-TW.tsv",
      "\xEF\xBB\xBFid\ttext\r\n"
      "# a comment\r\n"
      "menu.windows\t" TEST_UTF8_WINDOW "\r\n" },
    { "root\\pack\\names\\en.tsv",
      "key\ttext\n"
      "B1E4\tCao\n" },
};

internal PLATFORM_READ_ENTIRE_FILE(test_text_read_file)
{
    read_file_result result = {};
    for (u32 index = 0; index < ArrayCount(test_text_files); ++index)
    {
        if (strcmp(test_text_files[index].path, path) != 0) continue;
        u64 size = strlen(test_text_files[index].contents);
        result.contents = arena_push_size(arena, size + 1);
        memcpy(result.contents, test_text_files[index].contents, (size_t)size);
        result.size = size;
    }
    return result;
}

/* ---- the Big5 table ---------------------------------------------------- */

internal void
unit_text_big5_pair_tests(test_counts *counts)
{
    TEST_CHECK(counts, text_big5_pair_to_code_point(0xA4, 0x40) == 0x4E00, "A440 is U+4E00");
    TEST_CHECK(counts, text_big5_pair_to_code_point(0xA4, 0xA4) == 0x4E2D, "A4A4 is U+4E2D");
    TEST_CHECK(counts, text_big5_pair_to_code_point(0xB1, 0xE4) == 0x66F9, "B1E4 is U+66F9");
    TEST_CHECK(counts, text_big5_pair_to_code_point(0xBE, 0xDE) == 0x64CD, "BEDE is U+64CD");
    TEST_CHECK(counts, text_big5_pair_to_code_point(0xB3, 0x5C) == 0x8A31, "B35C is U+8A31 (trail byte 0x5C)");
    TEST_CHECK(counts, text_big5_pair_to_code_point(0xA1, 0x40) == 0x3000, "A140 is U+3000, the first pair");
    TEST_CHECK(counts, text_big5_pair_to_code_point(0xF9, 0xD5) == 0x9F98, "F9D5 is U+9F98, the last pair");
    TEST_CHECK(counts, text_big5_pair_to_code_point(0xA2, 0xCC) == TEXT_REPLACEMENT_CHARACTER, "A2CC is U+FFFD in BIG5.TXT");
    TEST_CHECK(counts, text_big5_pair_to_code_point(0xC8, 0xA1) == TEXT_REPLACEMENT_CHARACTER, "lead C8 has no row");
    TEST_CHECK(counts, text_big5_pair_to_code_point(0xF9, 0xFE) == TEXT_REPLACEMENT_CHARACTER, "F9FE is not in BIG5.TXT");

    /* The user-defined area: the Private Use Area. */
    TEST_CHECK(counts, text_big5_pair_to_code_point(0xFA, 0x40) == 0xE000, "FA40 is U+E000");
    TEST_CHECK(counts, text_big5_pair_to_code_point(0xFA, 0x45) == 0xE005, "FA45 is U+E005");
    TEST_CHECK(counts, text_big5_pair_to_code_point(0xFA, 0xA1) == 0xE03F, "FAA1 is U+E03F");
    TEST_CHECK(counts, text_big5_pair_to_code_point(0xFE, 0xFE) == 0xE310, "FEFE is U+E310");
    TEST_CHECK(counts, text_big5_pair_to_code_point(0x8E, 0x40) == 0xE311, "8E40 is U+E311");
    TEST_CHECK(counts, text_big5_pair_to_code_point(0xA0, 0xFE) == 0xEEB7, "A0FE is U+EEB7");
    TEST_CHECK(counts, text_big5_pair_to_code_point(0x81, 0x40) == 0xEEB8, "8140 is U+EEB8");
    TEST_CHECK(counts, text_big5_pair_to_code_point(0x8D, 0xFE) == 0xF6B0, "8DFE is U+F6B0");

    /* Bytes that are not a Big5 trail byte. */
    TEST_CHECK(counts, text_big5_pair_to_code_point(0xA4, 0x30) == TEXT_REPLACEMENT_CHARACTER, "trail 0x30 is not valid");
    TEST_CHECK(counts, text_big5_pair_to_code_point(0xA4, 0x7F) == TEXT_REPLACEMENT_CHARACTER, "trail 0x7F is not valid");
    TEST_CHECK(counts, text_big5_pair_to_code_point(0xA4, 0xA0) == TEXT_REPLACEMENT_CHARACTER, "trail 0xA0 is not valid");
    TEST_CHECK(counts, text_big5_pair_to_code_point(0xA4, 0xFF) == TEXT_REPLACEMENT_CHARACTER, "trail 0xFF is not valid");
    TEST_CHECK(counts, text_big5_pair_to_code_point(0xFA, 0x30) == TEXT_REPLACEMENT_CHARACTER, "the user-defined area also needs a valid trail");
    TEST_CHECK(counts, text_big5_pair_to_code_point(0x80, 0x40) == TEXT_REPLACEMENT_CHARACTER, "lead 0x80 is not valid");
    TEST_CHECK(counts, text_big5_pair_to_code_point(0xFF, 0x40) == TEXT_REPLACEMENT_CHARACTER, "lead 0xFF is not valid");

    /* The generated table holds every pair of BIG5.TXT that has a code point. */
    u32 pairs = 0;
    for (u32 row = 0; row < BIG5_ROW_COUNT; ++row)
    {
        for (u32 index = 0; index < BIG5_TRAIL_COUNT; ++index) if (big5_rows[row][index]) pairs++;
    }
    TEST_CHECK(counts, pairs == BIG5_PAIR_COUNT, "the table has BIG5_PAIR_COUNT pairs");
    TEST_CHECK(counts, BIG5_PAIR_COUNT > 13000, "the table has the 13,000 and more pairs of BIG5.TXT");
}

/* Each pair of the user-defined area decodes to its own code point, and the
   code point gives back the same pair. */
internal void
unit_text_big5_round_trip_tests(test_counts *counts)
{
    u32 pairs = 0;
    u32 failures = 0;
    u32 last_code_point = 0;
    for (u32 lead = 0x81; lead <= 0xFE; ++lead)
    {
        b32 user_defined = lead <= 0xA0 || lead >= 0xFA;
        if (!user_defined) continue;
        for (u32 trail = 0x40; trail <= 0xFE; ++trail)
        {
            if (trail > 0x7E && trail < 0xA1) continue;
            u32 code_point = text_big5_pair_to_code_point((u8)lead, (u8)trail);
            u8 back_lead = 0, back_trail = 0;
            b32 found = text_big5_from_private_use(code_point, &back_lead, &back_trail);
            if (!found || back_lead != lead || back_trail != trail) failures++;
            if (code_point > last_code_point) last_code_point = code_point;
            pairs++;
        }
    }
    TEST_CHECK(counts, failures == 0, "every user-defined pair round-trips through the Private Use Area");
    TEST_CHECK(counts, pairs == (13 + 19 + 5) * 157, "the user-defined area has 37 rows of 157 pairs");
    TEST_CHECK(counts, last_code_point == 0xF6B0, "the user-defined area ends at U+F6B0");

    u8 lead = 0, trail = 0;
    TEST_CHECK(counts, !text_big5_from_private_use(0x4E00, &lead, &trail), "U+4E00 is not in the user-defined area");
    TEST_CHECK(counts, !text_big5_from_private_use(0xF6B1, &lead, &trail), "U+F6B1 is after the user-defined area");
    TEST_CHECK(counts, text_big5_from_private_use(0xE005, &lead, &trail) && lead == 0xFA && trail == 0x45, "U+E005 is FA45");
}

/* ---- UTF-8 ------------------------------------------------------------- */

internal b32
test_utf8_is(text_utf8 utf8, const char *expected)
{
    u32 length = (u32)strlen(expected);
    return utf8.length == length && memcmp(utf8.bytes, expected, length) == 0;
}

internal void
unit_text_utf8_tests(test_counts *counts)
{
    TEST_CHECK(counts, test_utf8_is(text_utf8_encode(0x41), "A"), "U+0041 is one byte");
    TEST_CHECK(counts, test_utf8_is(text_utf8_encode(0x7F), "\x7F"), "U+007F is one byte");
    TEST_CHECK(counts, test_utf8_is(text_utf8_encode(0x80), "\xC2\x80"), "U+0080 is two bytes");
    TEST_CHECK(counts, test_utf8_is(text_utf8_encode(0xE9), "\xC3\xA9"), "U+00E9 is C3 A9");
    TEST_CHECK(counts, test_utf8_is(text_utf8_encode(0x7FF), "\xDF\xBF"), "U+07FF is two bytes");
    TEST_CHECK(counts, test_utf8_is(text_utf8_encode(0x800), "\xE0\xA0\x80"), "U+0800 is three bytes");
    TEST_CHECK(counts, test_utf8_is(text_utf8_encode(0x66F9), TEST_UTF8_CAO1), "U+66F9 is E6 9B B9");
    TEST_CHECK(counts, test_utf8_is(text_utf8_encode(0xE000), "\xEE\x80\x80"), "U+E000 is EE 80 80");
    TEST_CHECK(counts, test_utf8_is(text_utf8_encode(0xFFFF), "\xEF\xBF\xBF"), "U+FFFF is three bytes");
    TEST_CHECK(counts, test_utf8_is(text_utf8_encode(0x10000), "\xF0\x90\x80\x80"), "U+10000 is four bytes");
    TEST_CHECK(counts, test_utf8_is(text_utf8_encode(0x1F600), "\xF0\x9F\x98\x80"), "U+1F600 is F0 9F 98 80");
    TEST_CHECK(counts, test_utf8_is(text_utf8_encode(0x10FFFF), "\xF4\x8F\xBF\xBF"), "U+10FFFF is the last code point");
    TEST_CHECK(counts, test_utf8_is(text_utf8_encode(0xD800), TEST_UTF8_FFFD), "a surrogate encodes as U+FFFD");
    TEST_CHECK(counts, test_utf8_is(text_utf8_encode(0xDFFF), TEST_UTF8_FFFD), "a low surrogate encodes as U+FFFD");
    TEST_CHECK(counts, test_utf8_is(text_utf8_encode(0x110000), TEST_UTF8_FFFD), "a value above U+10FFFF encodes as U+FFFD");
}

/* ---- decode ------------------------------------------------------------ */

internal void
unit_text_decode_tests(test_counts *counts)
{
    char out[64];

    const u8 cao_cao[] = { 0xB1, 0xE4, 0xBE, 0xDE, 0x00, 0x00 };
    u32 written = text_decode_big5_into(cao_cao, sizeof(cao_cao), out, sizeof(out));
    TEST_CHECK(counts, written == 6 && strcmp(out, TEST_UTF8_CAO1 TEST_UTF8_CAO2) == 0, "B1E4 BEDE decodes to two characters");

    const u8 padded[] = { 0xB1, 0xE4, 0x00, 0xBE, 0xDE };
    text_decode_big5_into(padded, sizeof(padded), out, sizeof(out));
    TEST_CHECK(counts, strcmp(out, TEST_UTF8_CAO1) == 0, "the decoder stops at the first NUL byte");

    const u8 ascii[] = { 'A', 'b', '1' };
    text_decode_big5_into(ascii, sizeof(ascii), out, sizeof(out));
    TEST_CHECK(counts, strcmp(out, "Ab1") == 0, "ASCII bytes stay as they are");

    const u8 bad_trail[] = { 0xA4, 0x30 };
    text_decode_big5_into(bad_trail, sizeof(bad_trail), out, sizeof(out));
    TEST_CHECK(counts, strcmp(out, TEST_UTF8_FFFD "0") == 0, "an invalid trail byte gives U+FFFD and is read again");

    const u8 lone_lead[] = { 0xB1 };
    text_decode_big5_into(lone_lead, sizeof(lone_lead), out, sizeof(out));
    TEST_CHECK(counts, strcmp(out, TEST_UTF8_FFFD) == 0, "a lead byte at the end gives U+FFFD");

    const u8 lead_then_nul[] = { 0xB1, 0x00, 0xA4 };
    text_decode_big5_into(lead_then_nul, sizeof(lead_then_nul), out, sizeof(out));
    TEST_CHECK(counts, strcmp(out, TEST_UTF8_FFFD) == 0, "a lead byte before the NUL gives U+FFFD");

    const u8 not_lead[] = { 0x80, 0xFF };
    text_decode_big5_into(not_lead, sizeof(not_lead), out, sizeof(out));
    TEST_CHECK(counts, strcmp(out, TEST_UTF8_FFFD TEST_UTF8_FFFD) == 0, "0x80 and 0xFF give U+FFFD");

    const u8 user_defined[] = { 0xFA, 0x40, 0xFA, 0x45 };
    text_decode_big5_into(user_defined, sizeof(user_defined), out, sizeof(out));
    TEST_CHECK(counts, strcmp(out, "\xEE\x80\x80\xEE\x80\x85") == 0, "FA40 FA45 decode to U+E000 U+E005");

    char small[5];
    written = text_decode_big5_into(cao_cao, sizeof(cao_cao), small, sizeof(small));
    TEST_CHECK(counts, written == 3 && strcmp(small, TEST_UTF8_CAO1) == 0, "a small buffer gets whole characters only");
    TEST_CHECK(counts, text_decode_big5_into(cao_cao, sizeof(cao_cao), small, 0) == 0, "a buffer of size 0 gets nothing");

    u8 buffer[256];
    memory_arena arena;
    arena_init(&arena, buffer, sizeof(buffer));
    char *decoded = text_decode_big5(cao_cao, sizeof(cao_cao), &arena);
    TEST_CHECK(counts, decoded && strcmp(decoded, TEST_UTF8_CAO1 TEST_UTF8_CAO2) == 0, "text_decode_big5 decodes into the arena");
    memory_arena full;
    arena_init(&full, buffer, 8);
    TEST_CHECK(counts, text_decode_big5(cao_cao, sizeof(cao_cao), &full) == 0, "a full arena gives no string");

    text_copy_utf8(TEST_UTF8_CAO1 TEST_UTF8_CAO2, small, sizeof(small));
    TEST_CHECK(counts, strcmp(small, TEST_UTF8_CAO1) == 0, "text_copy_utf8 cuts at a whole character");
}

/* ---- TSV tables -------------------------------------------------------- */

internal void
unit_text_table_tests(test_counts *counts, memory_arena *arena)
{
    const char file[] =
        "\xEF\xBB\xBF"
        "# comment before the header\n"
        "id\ttext\tnote\r\n"
        "greeting\tHello\tfor the title bar\r\n"
        "\r\n"
        "two.lines\tfirst\\nsecond\\\\end\\tx\n"
        "greeting\tsecond row of the same id\n"
        "no tab here\n"
        "\tempty key\n"
        "empty.text\t\n"
        "last\tno new line at the end";
    text_table table = text_table_parse(file, sizeof(file) - 1, "id", arena);
    TEST_CHECK(counts, table.header_ok, "the header names the id column");
    TEST_CHECK(counts, table.row_count == 4, "four rows: comments, blank rows, bad rows and repeats do not count");
    TEST_CHECK(counts, table.bad_row_count == 2, "a row without a tab and a row with an empty key are bad");
    const char *greeting = text_table_find(&table, "greeting");
    TEST_CHECK(counts, greeting && strcmp(greeting, "Hello") == 0, "the first row of an id wins and the note column is not text");
    const char *two_lines = text_table_find(&table, "two.lines");
    TEST_CHECK(counts, two_lines && strcmp(two_lines, "first\nsecond\\end\tx") == 0, "escapes become a new line, a backslash and a tab");
    const char *empty = text_table_find(&table, "empty.text");
    TEST_CHECK(counts, empty && empty[0] == 0, "an empty text is a row");
    const char *last = text_table_find(&table, "last");
    TEST_CHECK(counts, last && strcmp(last, "no new line at the end") == 0, "the last row needs no new line");
    TEST_CHECK(counts, text_table_find(&table, "missing") == 0, "a missing id gives 0");
    TEST_CHECK(counts, strcmp(table.row_keys[0], "greeting") == 0 && strcmp(table.row_keys[3], "last") == 0, "rows keep the file order");

    const char wrong[] = "key\ttext\nB1E4\tCao\n";
    text_table refused = text_table_parse(wrong, sizeof(wrong) - 1, "id", arena);
    TEST_CHECK(counts, !refused.header_ok && refused.row_count == 0, "a file with another key column gives an empty table");
    TEST_CHECK(counts, text_table_find(&refused, "B1E4") == 0, "an empty table finds nothing");

    text_table nothing = text_table_parse(0, 0, "id", arena);
    TEST_CHECK(counts, nothing.row_count == 0 && text_table_find(&nothing, "x") == 0, "no file gives an empty table");

    u8 tiny_buffer[64];
    memory_arena tiny;
    arena_init(&tiny, tiny_buffer, sizeof(tiny_buffer));
    text_table too_big = text_table_parse(file, sizeof(file) - 1, "id", &tiny);
    TEST_CHECK(counts, too_big.row_count == 0 && text_table_find(&too_big, "greeting") == 0, "a full arena gives an empty table");
}

/* ---- the locale layer -------------------------------------------------- */

internal void
unit_text_catalog_tests(test_counts *counts, memory_arena *arena)
{
    text_catalog catalog;
    text_catalog_init(&catalog, arena_sub(arena, Kilobytes(64)), test_text_read_file, "root", "pack", "en");
    TEST_CHECK(counts, strcmp(text_get(&catalog, "menu.windows"), "menu.windows") == 0, "before a locale loads, text_get gives the id");

    TEST_CHECK(counts, text_catalog_set_locale(&catalog, "zh-TW"), "zh-TW is a valid locale");
    TEST_CHECK(counts, text_catalog_has_strings(&catalog), "zh-TW has its strings file");
    TEST_CHECK(counts, catalog.locale_count == 3, "locales.tsv lists three valid locales and skips a bad code");
    TEST_CHECK(counts, strcmp(catalog.locales[0].code, "zh-TW") == 0 && strcmp(catalog.locales[2].code, "ko") == 0,
               "the locales keep the file order");
    TEST_CHECK(counts, strcmp(catalog.locales[1].name, "English") == 0, "each locale has its own name");
    TEST_CHECK(counts, strcmp(text_get(&catalog, "menu.windows"), TEST_UTF8_WINDOW) == 0, "the active locale gives the text");
    TEST_CHECK(counts, strcmp(text_get(&catalog, "only.en"), "Only English") == 0, "a missing id comes from the fallback locale");
    u64 missing_before = catalog.stats.missing_strings;
    TEST_CHECK(counts, strcmp(text_get(&catalog, "no.such.id"), "no.such.id") == 0, "an id that no locale has gives the id");
    TEST_CHECK(counts, catalog.stats.missing_strings == missing_before + 1, "the catalog counts the missing id");

    TEST_CHECK(counts, text_catalog_set_locale(&catalog, "ko"), "ko is a valid locale without files");
    TEST_CHECK(counts, !text_catalog_has_strings(&catalog), "ko has no strings file");
    TEST_CHECK(counts, strcmp(text_get(&catalog, "menu.windows"), "Windows") == 0, "a locale without a file uses the fallback locale");

    TEST_CHECK(counts, !text_catalog_set_locale(&catalog, "..\\en"), "a code with a path is refused");
    TEST_CHECK(counts, !text_catalog_set_locale(&catalog, ""), "an empty code is refused");
    TEST_CHECK(counts, !text_catalog_set_locale(&catalog, "a-code-that-is-too-long"), "a long code is refused");
    TEST_CHECK(counts, strcmp(catalog.locale, "ko") == 0, "a refused code changes nothing");

    TEST_CHECK(counts, text_catalog_set_locale(&catalog, catalog.locales[1].code), "a code from the locale list works");
    TEST_CHECK(counts, strcmp(catalog.locale, "en") == 0 && text_catalog_has_strings(&catalog), "en is active");
    TEST_CHECK(counts, catalog.load_count == 3, "each valid locale change loads the files again");
}

internal void
unit_text_name_tests(test_counts *counts, memory_arena *arena)
{
    text_catalog catalog;
    text_catalog_init(&catalog, arena_sub(arena, Kilobytes(64)), test_text_read_file, "root", "pack", "en");
    char out[64];
    char key[64];
    const u8 family[4] = { 0xB1, 0xE4, 0x00, 0x00 };
    const u8 given[4]  = { 0xBE, 0xDE, 0x00, 0x00 };
    const u8 empty[4]  = { 0x00, 0x00, 0x00, 0x00 };
    const u8 user_defined[4] = { 0xFA, 0x40, 0xFA, 0x45 };

    TEST_CHECK(counts, text_name_key(family, sizeof(family), key, sizeof(key)) == 4 && strcmp(key, "B1E4") == 0,
               "the key is the bytes before the NUL as upper-case hex");
    TEST_CHECK(counts, text_name_key(user_defined, sizeof(user_defined), key, sizeof(key)) == 8 && strcmp(key, "FA40FA45") == 0,
               "a name without a NUL uses all its bytes");

    text_catalog_set_locale(&catalog, "en");
    TEST_CHECK(counts, strcmp(text_name(&catalog, family, sizeof(family), out, sizeof(out)), "Cao") == 0,
               "en: a name with a row gives the row");
    TEST_CHECK(counts, strcmp(text_name(&catalog, given, sizeof(given), out, sizeof(out)), TEST_UTF8_CAO2) == 0,
               "en: a name without a row gives the decoded bytes (zh-TW)");
    TEST_CHECK(counts, catalog.stats.name_lookups == 2 && catalog.stats.decoded_names == 1, "the catalog counts the name lookups");
    TEST_CHECK(counts, strcmp(text_name(&catalog, empty, sizeof(empty), out, sizeof(out)), "") == 0, "an empty name gives an empty string");

    text_catalog_set_locale(&catalog, "zh-TW");
    TEST_CHECK(counts, strcmp(text_name(&catalog, family, sizeof(family), out, sizeof(out)), TEST_UTF8_CAO1) == 0,
               "zh-TW without a names file gives the decoded bytes");
    TEST_CHECK(counts, strcmp(text_name(&catalog, user_defined, sizeof(user_defined), out, sizeof(out)), "\xEE\x80\x80\xEE\x80\x85") == 0,
               "a name in the user-defined area decodes to the Private Use Area");

    char small[3];
    text_catalog_set_locale(&catalog, "en");
    TEST_CHECK(counts, strcmp(text_name(&catalog, family, sizeof(family), small, sizeof(small)), "Ca") == 0,
               "a small buffer cuts the name");
    TEST_CHECK(counts, strcmp(text_name(0, family, sizeof(family), out, sizeof(out)), TEST_UTF8_CAO1) == 0,
               "no catalog gives the decoded bytes");
}

internal void
unit_text_tests(test_counts *counts)
{
    memory_arena arena = test_arena_create(Megabytes(1));
    unit_text_big5_pair_tests(counts);
    unit_text_big5_round_trip_tests(counts);
    unit_text_utf8_tests(counts);
    unit_text_decode_tests(counts);
    unit_text_table_tests(counts, &arena);
    unit_text_catalog_tests(counts, &arena);
    unit_text_name_tests(counts, &arena);
    test_arena_destroy(&arena);
}
