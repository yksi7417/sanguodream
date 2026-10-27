/* Unit tests for code/sim/inflate.h: adler32, hand-built stored and fixed
   blocks, a dynamic block, the error statuses, and the zlib stream of the
   sample save.

   The sample save test is the one unit test that reads a file. The spec
   gives the expected numbers for that file only, so the test reads it.
   It reads one 59 KB file and stays fast. */

/* save-file.md, Container: the zlib stream starts at 0x1BF and inflates to
   the uncompressed payload length (1,154,785 in all 19 files). */
#define SAMPLE_SAVE_STREAM_OFFSET     0x1BF
#define SAMPLE_SAVE_PAYLOAD_LENGTH    1154785u
#define SAMPLE_SAVE_FILE_SIZE         59603u

/* zlib header 78 01, then one final stored block (BFINAL 1, BTYPE 00) with
   LEN 5 and NLEN ~5, the bytes "hello", and adler32("hello") big endian. */
global_variable const u8 inflate_vector_stored[] = {
    0x78, 0x01,
    0x01, 0x05, 0x00, 0xFA, 0xFF,
    'h', 'e', 'l', 'l', 'o',
    0x06, 0x2C, 0x02, 0x15 };

/* zlib header 78 01, then one final fixed-Huffman block for "abcabcabcabc":
   the literals a, b, c and one back reference of length 9 at distance 3,
   then end of block. Bits, least significant first:
     1 01                 BFINAL, BTYPE 01
     10010001 10010010 10010011   literals 0x61 0x62 0x63 (8-bit codes 0x30 + byte)
     0000111              length symbol 263 (length 9, no extra bits)
     00010                distance symbol 2 (distance 3, no extra bits)
     0000000              end of block
   Trailer: adler32("abcabcabcabc") = 0x1DE00499. */
global_variable const u8 inflate_vector_fixed[] = {
    0x78, 0x01, 0x4B, 0x4C, 0x4A, 0x4E, 0x84, 0x21, 0x00,
    0x1D, 0xE0, 0x04, 0x99 };

/* A dynamic-Huffman block (BTYPE 10) from a reference compressor at level 9.
   It decodes to inflate_vector_dynamic_text. */
global_variable const u8 inflate_vector_dynamic[] = {
    0x78, 0xDA, 0xB5, 0xCC, 0x4B, 0x0E, 0x02, 0x21, 0x10, 0x45, 0xD1, 0xAD, 0xBC, 0x05, 0x74, 0x8C,
    0xBF, 0xF6, 0x13, 0x57, 0x53, 0x40, 0x21, 0x44, 0x9A, 0x32, 0x54, 0x39, 0xC0, 0xD5, 0x4B, 0xEC,
    0x91, 0x0B, 0x70, 0x72, 0x47, 0x27, 0xD7, 0x12, 0x43, 0x62, 0xCC, 0x9E, 0x1B, 0x8C, 0x5C, 0x61,
    0x24, 0x29, 0x41, 0x51, 0x69, 0x61, 0x9D, 0xD0, 0x99, 0x9A, 0x82, 0x6A, 0x80, 0x1A, 0x99, 0xDE,
    0x60, 0xC3, 0xFB, 0x6C, 0xFD, 0x07, 0xDF, 0x47, 0x27, 0x58, 0x13, 0x79, 0xAE, 0x38, 0x8A, 0x84,
    0xCD, 0xD7, 0xFE, 0xEB, 0xFD, 0x2E, 0xD9, 0x21, 0xF4, 0xB1, 0xCA, 0x1E, 0xE9, 0x15, 0xE3, 0x42,
    0x15, 0xAE, 0x88, 0x7F, 0xC0, 0x58, 0x0D, 0xDB, 0xDD, 0xFE, 0x70, 0x9C, 0x4F, 0xE7, 0xCB, 0xF5,
    0x03, 0x05, 0xAD, 0x4F, 0xB4 };

/* A hand-built dynamic block with no distance codes (HDIST field 0 and
   that one distance length 0): literal 'a' and end of block have 1-bit
   codes, and the data is "aa". RFC 1951 allows this for a block of
   literals only, and a reference decoder accepts it. */
global_variable const u8 inflate_vector_no_distance_codes[] = {
    0x78, 0x01, 0x05, 0xC0, 0x01, 0x05, 0x00, 0x00, 0x00, 0x00, 0xA0, 0xAD, 0xFD, 0x3F, 0x11, 0x02,
    0x01, 0x25, 0x00, 0xC3 };

global_variable const char inflate_vector_dynamic_text[] =
    "the officer table holds names, years and stats; the city table holds gold, troops and food. "
    "the officer table holds names, years and stats; the city table holds gold, troops and food. "
    "zlib dynamic huffman block test 0123456789";

internal inflate_result
inflate_test_run(memory_arena *arena, const u8 *source, u64 source_size, u8 *dest, u64 dest_capacity)
{
    u64 mark = arena->used;
    inflate_result result = zlib_inflate(arena, source, source_size, dest, dest_capacity);
    arena->used = mark;
    return result;
}

internal void
unit_inflate_vector_tests(test_counts *counts, memory_arena *arena)
{
    TEST_CHECK(counts, adler32((const u8 *)"Wikipedia", 9) == 0x11E60398, "adler32 of Wikipedia (RFC 1950 example)");
    TEST_CHECK(counts, adler32(0, 0) == 1, "adler32 of nothing is 1");

    u8 out[512];
    inflate_result stored = inflate_test_run(arena, inflate_vector_stored, sizeof(inflate_vector_stored), out, sizeof(out));
    TEST_CHECK(counts, stored.status == INFLATE_OK, "stored block inflates");
    TEST_CHECK(counts, stored.bytes_written == 5 && memcmp(out, "hello", 5) == 0, "stored block copies its bytes");
    TEST_CHECK(counts, stored.adler32_stored == 0x062C0215 && stored.adler32_computed == 0x062C0215, "stored block adler32");
    TEST_CHECK(counts, stored.bytes_read == sizeof(inflate_vector_stored), "stored block reads the whole stream");

    inflate_result fixed = inflate_test_run(arena, inflate_vector_fixed, sizeof(inflate_vector_fixed), out, sizeof(out));
    TEST_CHECK(counts, fixed.status == INFLATE_OK, "fixed block inflates");
    TEST_CHECK(counts, fixed.bytes_written == 12 && memcmp(out, "abcabcabcabc", 12) == 0, "fixed block: literals and an overlapping back reference");
    TEST_CHECK(counts, fixed.bytes_read == sizeof(inflate_vector_fixed), "fixed block reads the whole stream");

    u64 text_length = sizeof(inflate_vector_dynamic_text) - 1;
    TEST_CHECK(counts, ((inflate_vector_dynamic[2] >> 1) & 3) == 2, "the dynamic vector starts with a dynamic block");
    inflate_result dynamic = inflate_test_run(arena, inflate_vector_dynamic, sizeof(inflate_vector_dynamic), out, sizeof(out));
    TEST_CHECK(counts, dynamic.status == INFLATE_OK, "dynamic block inflates");
    TEST_CHECK(counts, dynamic.bytes_written == text_length && memcmp(out, inflate_vector_dynamic_text, text_length) == 0,
               "dynamic block gives the original text");

    inflate_result literals_only = inflate_test_run(arena, inflate_vector_no_distance_codes,
                                                    sizeof(inflate_vector_no_distance_codes), out, sizeof(out));
    TEST_CHECK(counts, literals_only.status == INFLATE_OK && literals_only.bytes_written == 2 && memcmp(out, "aa", 2) == 0,
               "a dynamic block with no distance codes inflates");
}

internal void
unit_inflate_error_tests(test_counts *counts, memory_arena *arena)
{
    u8 out[64];
    u8 broken[sizeof(inflate_vector_stored)];

    memcpy(broken, inflate_vector_stored, sizeof(broken));
    broken[1] = 0x02;   /* (0x78 * 256 + 0x02) is not a multiple of 31 */
    TEST_CHECK(counts, inflate_test_run(arena, broken, sizeof(broken), out, sizeof(out)).status == INFLATE_BAD_ZLIB_HEADER,
               "a wrong header check is refused");

    memcpy(broken, inflate_vector_stored, sizeof(broken));
    broken[0] = 0x79;   /* method 9 */
    broken[1] = 0x00;
    TEST_CHECK(counts, inflate_test_run(arena, broken, sizeof(broken), out, sizeof(out)).status == INFLATE_BAD_ZLIB_HEADER,
               "a method other than deflate is refused");

    u8 dictionary[] = { 0x78, 0xBB, 0, 0, 0, 0 };   /* FDICT set, check bits valid */
    TEST_CHECK(counts, inflate_test_run(arena, dictionary, sizeof(dictionary), out, sizeof(out)).status == INFLATE_PRESET_DICTIONARY,
               "a preset dictionary is refused");

    memcpy(broken, inflate_vector_stored, sizeof(broken));
    broken[5] = 0xFB;   /* NLEN is no longer the complement of LEN */
    TEST_CHECK(counts, inflate_test_run(arena, broken, sizeof(broken), out, sizeof(out)).status == INFLATE_BAD_STORED_LENGTH,
               "stored LEN and NLEN must agree");

    memcpy(broken, inflate_vector_stored, sizeof(broken));
    broken[2] = 0x07;   /* BFINAL 1, BTYPE 11 */
    TEST_CHECK(counts, inflate_test_run(arena, broken, sizeof(broken), out, sizeof(out)).status == INFLATE_BAD_BLOCK_TYPE,
               "block type 3 is refused");

    memcpy(broken, inflate_vector_stored, sizeof(broken));
    broken[sizeof(broken) - 1] ^= 1;
    inflate_result mismatch = inflate_test_run(arena, broken, sizeof(broken), out, sizeof(out));
    TEST_CHECK(counts, mismatch.status == INFLATE_ADLER32_MISMATCH && mismatch.bytes_written == 5,
               "a wrong adler32 trailer is reported after the data decodes");

    TEST_CHECK(counts, inflate_test_run(arena, inflate_vector_stored, sizeof(inflate_vector_stored) - 2, out, sizeof(out)).status == INFLATE_TRUNCATED_INPUT,
               "a cut trailer is truncated input");
    TEST_CHECK(counts, inflate_test_run(arena, inflate_vector_fixed, 5, out, sizeof(out)).status == INFLATE_TRUNCATED_INPUT,
               "a cut fixed block is truncated input");
    TEST_CHECK(counts, inflate_test_run(arena, inflate_vector_dynamic, 20, out, sizeof(out)).status == INFLATE_TRUNCATED_INPUT,
               "a cut dynamic block is truncated input");
    TEST_CHECK(counts, inflate_test_run(arena, inflate_vector_stored, 1, out, sizeof(out)).status == INFLATE_TRUNCATED_INPUT,
               "one byte is truncated input");

    TEST_CHECK(counts, inflate_test_run(arena, inflate_vector_stored, sizeof(inflate_vector_stored), out, 4).status == INFLATE_OUTPUT_FULL,
               "a stored block larger than the output is refused");
    TEST_CHECK(counts, inflate_test_run(arena, inflate_vector_fixed, sizeof(inflate_vector_fixed), out, 11).status == INFLATE_OUTPUT_FULL,
               "a back reference past the end of the output is refused");

    /* Fixed block: literal 'a', then length 3 at distance 2, one byte too far back. */
    u8 far_back[] = { 0x78, 0x01, 0x4B, 0x04, 0x42, 0x00 };
    TEST_CHECK(counts, inflate_test_run(arena, far_back, sizeof(far_back), out, sizeof(out)).status == INFLATE_BAD_DISTANCE,
               "a distance before the start of the output is refused");

    TEST_CHECK(counts, strcmp(inflate_status_name(INFLATE_ADLER32_MISMATCH), "adler32 mismatch") == 0, "status name");
}

internal void
unit_inflate_sample_save_tests(test_counts *counts, memory_arena *arena)
{
    char path[MAX_PATH];
    test_fixture_path(path, MAX_PATH, "saves/sample.s11");
    test_file file = test_read_file(path, arena);
    TEST_CHECK(counts, file.contents != 0, "sample.s11 is readable");
    if (!file.contents) return;
    TEST_CHECK(counts, file.size == SAMPLE_SAVE_FILE_SIZE, "sample.s11 has the size of its README");

    u8 *payload = (u8 *)arena_push_size(arena, SAMPLE_SAVE_PAYLOAD_LENGTH);
    const u8 *stream = file.contents + SAMPLE_SAVE_STREAM_OFFSET;
    u64 stream_size  = file.size - SAMPLE_SAVE_STREAM_OFFSET;
    TEST_CHECK(counts, stream[0] == 0x78 && stream[1] == 0x9C, "the sample stream starts with the zlib header 78 9C");

    inflate_result result = inflate_test_run(arena, stream, stream_size, payload, SAMPLE_SAVE_PAYLOAD_LENGTH);
    TEST_CHECK(counts, result.status == INFLATE_OK, "sample.s11 inflates");
    TEST_CHECK(counts, result.bytes_written == SAMPLE_SAVE_PAYLOAD_LENGTH, "sample.s11 inflates to exactly 1,154,785 bytes");
    TEST_CHECK(counts, result.adler32_stored == result.adler32_computed, "sample.s11 adler32 trailer matches the payload");
    TEST_CHECK(counts, result.bytes_read == stream_size, "the zlib stream runs to the end of the file");

    inflate_result short_output = inflate_test_run(arena, stream, stream_size, payload, SAMPLE_SAVE_PAYLOAD_LENGTH - 1);
    TEST_CHECK(counts, short_output.status == INFLATE_OUTPUT_FULL, "one byte less output space is output full");
}

internal void
unit_inflate_tests(test_counts *counts)
{
    memory_arena arena = test_arena_create(Megabytes(4));
    unit_inflate_vector_tests(counts, &arena);
    unit_inflate_error_tests(counts, &arena);
    unit_inflate_sample_save_tests(counts, &arena);
    test_arena_destroy(&arena);
}
