/* Unit tests for code/render/picture.h: the decoder finds the format from
   the first bytes of the file, not from the name of the file. */

internal void
unit_picture_tests(test_counts *counts)
{
    memory_arena arena = test_arena_create(Megabytes(1));

    /* a JPEG file: full size, opaque */
    jpeg_test_writer *writer = PushStruct(&arena, jpeg_test_writer);
    const u8 grey[1] = { 0x11 };
    jpeg_test_header(writer, 16, 8, 1, grey, 0, JPEG_SOF0);
    jpeg_test_block(writer, jpeg_test_dc(200));
    jpeg_test_block(writer, jpeg_test_dc(50) - jpeg_test_dc(200));
    jpeg_test_end(writer);
    png_image from_jpeg = picture_decode(writer->bytes, writer->size, &arena);
    TEST_CHECK(counts, from_jpeg.error == 0 && from_jpeg.rgba != 0, "a JPEG file decodes");
    TEST_CHECK(counts, from_jpeg.width == 16 && from_jpeg.height == 8, "a JPEG file decodes at its full size");
    TEST_CHECK(counts, jpeg_test_near(from_jpeg.rgba[0], 200) && from_jpeg.rgba[3] == 255, "the first pixel of the JPEG file has its grey and is opaque");
    TEST_CHECK(counts, jpeg_test_near(from_jpeg.rgba[8 * 4], 50), "the second block of the JPEG file has its grey");

    /* a PNG file */
    u8 file[512];
    const u8 raw[] = {
        0, 255, 0, 0, 255,   0, 255, 0, 255,
        0, 0, 0, 255, 255,   9, 9, 9, 255 };
    u32 size = png_test_build(file, 2, 2, raw, sizeof(raw));
    png_image from_png = picture_decode(file, size, &arena);
    TEST_CHECK(counts, from_png.error == 0 && from_png.width == 2 && from_png.height == 2, "a PNG file decodes");
    TEST_CHECK(counts, from_png.rgba[0] == 255 && from_png.rgba[1] == 0, "the first pixel of the PNG file is red");

    /* refusals */
    const u8 neither[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    TEST_CHECK(counts, picture_decode(neither, sizeof(neither), &arena).rgba == 0, "a file of another format gives no pixels");
    TEST_CHECK(counts, picture_decode(neither, sizeof(neither), &arena).error != 0, "a file of another format gives an error");
    TEST_CHECK(counts, picture_decode(writer->bytes, 40, &arena).rgba == 0, "a JPEG file cut before the scan gives no pixels");
    TEST_CHECK(counts, picture_decode(writer->bytes, 1, &arena).rgba == 0, "a file of one byte gives no pixels");
    test_arena_destroy(&arena);
}
