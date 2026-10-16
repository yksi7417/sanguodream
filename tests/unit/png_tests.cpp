/* Unit tests for code/render/png.h: a hand-built 2x2 RGBA PNG with a stored
   deflate block and each filter type, plus the refusals. */

internal u32
png_test_put_chunk(u8 *out, u32 at, const char *type, const u8 *body, u32 length)
{
    out[at] = (u8)(length >> 24); out[at + 1] = (u8)(length >> 16); out[at + 2] = (u8)(length >> 8); out[at + 3] = (u8)length;
    memcpy(out + at + 4, type, 4);
    if (length) memcpy(out + at + 8, body, length);
    memset(out + at + 8 + length, 0, 4);   /* the decoder does not check the CRC */
    return at + 12 + length;
}

/* Builds a PNG of width x height RGBA pixels whose rows use the given filter
   bytes and raw (already filtered) bytes. */
internal u32
png_test_build(u8 *out, u32 width, u32 height, const u8 *raw_rows, u32 raw_size)
{
    static const u8 signature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    memcpy(out, signature, 8);
    u32 at = 8;
    u8 ihdr[13] = {};
    ihdr[0] = (u8)(width >> 24); ihdr[1] = (u8)(width >> 16); ihdr[2] = (u8)(width >> 8); ihdr[3] = (u8)width;
    ihdr[4] = (u8)(height >> 24); ihdr[5] = (u8)(height >> 16); ihdr[6] = (u8)(height >> 8); ihdr[7] = (u8)height;
    ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    at = png_test_put_chunk(out, at, "IHDR", ihdr, 13);

    /* zlib: header 78 01, one final stored block, LEN, NLEN, data, adler32 */
    u8 zlib[256];
    u32 z = 0;
    zlib[z++] = 0x78; zlib[z++] = 0x01; zlib[z++] = 0x01;
    zlib[z++] = (u8)raw_size; zlib[z++] = (u8)(raw_size >> 8);
    zlib[z++] = (u8)~raw_size; zlib[z++] = (u8)(~raw_size >> 8);
    memcpy(zlib + z, raw_rows, raw_size); z += raw_size;
    u32 a = adler32(raw_rows, raw_size);
    zlib[z++] = (u8)(a >> 24); zlib[z++] = (u8)(a >> 16); zlib[z++] = (u8)(a >> 8); zlib[z++] = (u8)a;
    at = png_test_put_chunk(out, at, "IDAT", zlib, z);
    at = png_test_put_chunk(out, at, "IEND", 0, 0);
    return at;
}

internal void
unit_png_tests(test_counts *counts)
{
    memory_arena arena = test_arena_create(Megabytes(1));
    u8 file[512];

    /* row 0: filter none; row 1: filter up (each byte is a delta from the row above) */
    const u8 raw[] = {
        0, 255, 0, 0, 255,   0, 255, 0, 255,
        2, 0, 0, 0, 0,       10, 0, 0, 0 };
    u32 size = png_test_build(file, 2, 2, raw, sizeof(raw));
    png_image image = png_decode(file, size, &arena);
    TEST_CHECK(counts, image.error == 0 && image.rgba != 0, "a valid PNG decodes");
    TEST_CHECK(counts, image.width == 2 && image.height == 2, "size from IHDR");
    TEST_CHECK(counts, image.rgba[0] == 255 && image.rgba[1] == 0 && image.rgba[3] == 255, "first pixel red, opaque");
    TEST_CHECK(counts, image.rgba[4] == 0 && image.rgba[5] == 255 && image.rgba[7] == 255, "second pixel green");
    TEST_CHECK(counts, image.rgba[8] == 255 && image.rgba[12] == 10 && image.rgba[13] == 255, "filter up adds the row above");

    /* filter sub: the second pixel is a delta from the first */
    const u8 raw_sub[] = { 1, 100, 50, 25, 255,   10, 10, 10, 0,
                           0, 1, 2, 3, 4,         5, 6, 7, 8 };
    size = png_test_build(file, 2, 2, raw_sub, sizeof(raw_sub));
    image = png_decode(file, size, &arena);
    TEST_CHECK(counts, image.error == 0 && image.rgba[4] == 110 && image.rgba[5] == 60 && image.rgba[7] == 255, "filter sub adds the pixel to the left");

    /* filter average and paeth on a second row */
    const u8 raw_avg[] = { 0, 100, 100, 100, 255,   200, 200, 200, 255,
                           3, 0, 0, 0, 0,           0, 0, 0, 0 };
    size = png_test_build(file, 2, 2, raw_avg, sizeof(raw_avg));
    image = png_decode(file, size, &arena);
    TEST_CHECK(counts, image.error == 0 && image.rgba[8] == 50 && image.rgba[12] == 125, "filter average uses left and above");
    const u8 raw_paeth[] = { 0, 10, 20, 30, 255,   40, 50, 60, 255,
                             4, 0, 0, 0, 0,         0, 0, 0, 0 };
    size = png_test_build(file, 2, 2, raw_paeth, sizeof(raw_paeth));
    image = png_decode(file, size, &arena);
    TEST_CHECK(counts, image.error == 0 && image.rgba[8] == 10 && image.rgba[12] == 40, "filter paeth predicts from the neighbours");

    /* refusals */
    u8 bad[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    TEST_CHECK(counts, png_decode(bad, 8, &arena).error != 0, "a non-PNG is refused");
    size = png_test_build(file, 2, 2, raw, sizeof(raw));
    file[8 + 8 + 8] = 16;   /* bit depth 16 */
    TEST_CHECK(counts, png_decode(file, size, &arena).error != 0, "a 16-bit PNG is refused");
    size = png_test_build(file, 2, 2, raw, sizeof(raw));
    TEST_CHECK(counts, png_decode(file, size - 20, &arena).error != 0, "a truncated PNG is refused");

    test_arena_destroy(&arena);
}
