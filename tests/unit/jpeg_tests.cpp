/* Unit tests for code/render/jpeg.h: hand-built baseline JPEG files with
   only DC coefficients. The DC table has a 4-bit code for each category
   0..11 (the code is the category). The AC table has one code, "0", for
   the end of block. With a quantization of 1, a block of mean M has the
   DC coefficient 8 * (M - 128), so the expected output is exact. */

struct jpeg_test_writer
{
    u8  bytes[1024];
    u32 size;
    u32 bit_buffer;
    u32 bit_count;
};

internal void
jpeg_test_byte(jpeg_test_writer *writer, u32 value)
{
    writer->bytes[writer->size++] = (u8)value;
}

internal void
jpeg_test_u16(jpeg_test_writer *writer, u32 value)
{
    jpeg_test_byte(writer, value >> 8);
    jpeg_test_byte(writer, value & 0xFF);
}

/* Entropy bits, high bit first, with a stuffed zero after each 0xFF. */
internal void
jpeg_test_bits(jpeg_test_writer *writer, u32 value, u32 count)
{
    for (u32 bit = count; bit-- > 0;)
    {
        writer->bit_buffer = (writer->bit_buffer << 1) | ((value >> bit) & 1);
        writer->bit_count += 1;
        if (writer->bit_count == 8)
        {
            jpeg_test_byte(writer, writer->bit_buffer);
            if (writer->bit_buffer == 0xFF) jpeg_test_byte(writer, 0);
            writer->bit_buffer = writer->bit_count = 0;
        }
    }
}

/* Pads the last byte with one bits. */
internal void
jpeg_test_flush(jpeg_test_writer *writer)
{
    while (writer->bit_count) jpeg_test_bits(writer, 1, 1);
}

/* One block: the DC difference and the end of block. */
internal void
jpeg_test_block(jpeg_test_writer *writer, s32 difference)
{
    u32 magnitude = (u32)(difference < 0 ? -difference : difference), category = 0;
    while (magnitude >> category) category += 1;
    jpeg_test_bits(writer, category, 4);
    if (category) jpeg_test_bits(writer, (u32)(difference < 0 ? difference + (1 << category) - 1 : difference), category);
    jpeg_test_bits(writer, 0, 1);
}

/* The DC coefficient of a block with this mean, at quantization 1. */
internal s32
jpeg_test_dc(s32 mean)
{
    return 8 * (mean - 128);
}

/* SOI, DQT, SOF0, DHT, an optional DRI, and SOS. sampling[] has one byte
   (horizontal << 4 | vertical) for each component. */
internal void
jpeg_test_header(jpeg_test_writer *writer, u32 width, u32 height, u32 component_count, const u8 *sampling,
                 u32 restart_interval, u8 frame_marker)
{
    jpeg_test_u16(writer, 0xFFD8);
    jpeg_test_u16(writer, 0xFFDB); jpeg_test_u16(writer, 67); jpeg_test_byte(writer, 0);
    for (u32 entry = 0; entry < 64; ++entry) jpeg_test_byte(writer, 1);
    jpeg_test_byte(writer, 0xFF); jpeg_test_byte(writer, frame_marker);
    jpeg_test_u16(writer, 8 + 3 * component_count); jpeg_test_byte(writer, 8);
    jpeg_test_u16(writer, height); jpeg_test_u16(writer, width); jpeg_test_byte(writer, component_count);
    for (u32 index = 0; index < component_count; ++index)
    {
        jpeg_test_byte(writer, index + 1); jpeg_test_byte(writer, sampling[index]); jpeg_test_byte(writer, 0);
    }
    jpeg_test_u16(writer, 0xFFC4); jpeg_test_u16(writer, 2 + 17 + 12 + 17 + 1);
    jpeg_test_byte(writer, 0x00);
    for (u32 length = 1; length <= 16; ++length) jpeg_test_byte(writer, length == 4 ? 12 : 0);
    for (u32 symbol = 0; symbol < 12; ++symbol) jpeg_test_byte(writer, symbol);
    jpeg_test_byte(writer, 0x10);
    for (u32 length = 1; length <= 16; ++length) jpeg_test_byte(writer, length == 1 ? 1 : 0);
    jpeg_test_byte(writer, 0x00);
    if (restart_interval) { jpeg_test_u16(writer, 0xFFDD); jpeg_test_u16(writer, 4); jpeg_test_u16(writer, restart_interval); }
    jpeg_test_u16(writer, 0xFFDA); jpeg_test_u16(writer, 6 + 2 * component_count); jpeg_test_byte(writer, component_count);
    for (u32 index = 0; index < component_count; ++index) { jpeg_test_byte(writer, index + 1); jpeg_test_byte(writer, 0x00); }
    jpeg_test_byte(writer, 0); jpeg_test_byte(writer, 63); jpeg_test_byte(writer, 0);
}

internal void
jpeg_test_end(jpeg_test_writer *writer)
{
    jpeg_test_flush(writer);
    jpeg_test_u16(writer, 0xFFD9);
}

internal b32
jpeg_test_near(u8 value, s32 expected)
{
    s32 difference = (s32)value - expected;
    return difference >= -2 && difference <= 2;
}

internal void
unit_jpeg_tests(test_counts *counts)
{
    memory_arena arena = test_arena_create(Megabytes(1));
    jpeg_test_writer *writer = PushStruct(&arena, jpeg_test_writer);

    /* grey 16 x 8: two blocks of mean 200 and 50 */
    const u8 grey[1] = { 0x11 };
    jpeg_test_header(writer, 16, 8, 1, grey, 0, JPEG_SOF0);
    jpeg_test_block(writer, jpeg_test_dc(200));
    jpeg_test_block(writer, jpeg_test_dc(50) - jpeg_test_dc(200));
    jpeg_test_end(writer);
    jpeg_image image = jpeg_decode_eighth(writer->bytes, writer->size, &arena);
    TEST_CHECK(counts, image.error == 0 && image.rgba != 0, "a grey baseline file decodes");
    TEST_CHECK(counts, image.width == 2 && image.height == 1, "one pixel for each 8 x 8 block");
    TEST_CHECK(counts, image.rgba[0] == 200 && image.rgba[1] == 200 && image.rgba[2] == 200 && image.rgba[3] == 255, "the first pixel is the mean of the first block");
    TEST_CHECK(counts, image.rgba[4] == 50 && image.rgba[6] == 50, "the DC predictor adds the difference");

    /* the same file with a restart marker after each block */
    memset(writer, 0, sizeof(*writer));
    jpeg_test_header(writer, 16, 8, 1, grey, 1, JPEG_SOF0);
    jpeg_test_block(writer, jpeg_test_dc(200));
    jpeg_test_flush(writer);
    jpeg_test_u16(writer, 0xFFD0);
    jpeg_test_block(writer, jpeg_test_dc(50));   /* the predictor starts at 0 again */
    jpeg_test_end(writer);
    image = jpeg_decode_eighth(writer->bytes, writer->size, &arena);
    TEST_CHECK(counts, image.error == 0 && image.rgba[0] == 200 && image.rgba[4] == 50, "a restart marker resets the predictor");

    /* YCbCr 4:2:0, 16 x 16: four Y blocks (76, 150, 76, 150), one Cb (85), one Cr (255) */
    memset(writer, 0, sizeof(*writer));
    const u8 colour[3] = { 0x22, 0x11, 0x11 };
    jpeg_test_header(writer, 16, 16, 3, colour, 0, JPEG_SOF0);
    s32 luma[4] = { 76, 150, 76, 150 }, previous = 0;
    for (u32 block = 0; block < 4; ++block) { jpeg_test_block(writer, jpeg_test_dc(luma[block]) - previous); previous = jpeg_test_dc(luma[block]); }
    jpeg_test_block(writer, jpeg_test_dc(85));
    jpeg_test_block(writer, jpeg_test_dc(255));
    jpeg_test_end(writer);
    image = jpeg_decode_eighth(writer->bytes, writer->size, &arena);
    TEST_CHECK(counts, image.error == 0 && image.width == 2 && image.height == 2, "a 4:2:0 file decodes to 2 x 2");
    TEST_CHECK(counts, jpeg_test_near(image.rgba[0], 254) && jpeg_test_near(image.rgba[1], 0) && jpeg_test_near(image.rgba[2], 0), "Y 76, Cb 85, Cr 255 is red");
    TEST_CHECK(counts, jpeg_test_near(image.rgba[4], 255) && jpeg_test_near(image.rgba[5], 74) && jpeg_test_near(image.rgba[6], 74), "the second Y block with the same chroma");
    TEST_CHECK(counts, image.rgba[8] == image.rgba[0] && image.rgba[12] == image.rgba[4], "the chroma block covers all four pixels");

    /* refusals */
    memset(writer, 0, sizeof(*writer));
    jpeg_test_header(writer, 16, 8, 1, grey, 0, JPEG_SOF2);
    jpeg_test_end(writer);
    TEST_CHECK(counts, jpeg_decode_eighth(writer->bytes, writer->size, &arena).error != 0, "a progressive file is refused");
    TEST_CHECK(counts, jpeg_decode_eighth((const u8 *)"\x89PNG", 4, &arena).error != 0, "a PNG file is not a JPEG file");
    memset(writer, 0, sizeof(*writer));
    jpeg_test_header(writer, 16, 8, 1, grey, 0, JPEG_SOF0);
    TEST_CHECK(counts, jpeg_decode_eighth(writer->bytes, 40, &arena).error != 0, "a file cut before the scan is refused");

    /* the bit reader removes the zero after 0xFF and stops at a marker */
    const u8 stuffed[] = { 0xFF, 0x00, 0x12, 0xFF, 0xD9 };
    jpeg_bits bits = {};
    bits.data = stuffed;
    bits.size = sizeof(stuffed);
    TEST_CHECK(counts, jpeg_bits_take(&bits, 8) == 0xFF && jpeg_bits_take(&bits, 8) == 0x12, "a stuffed 0xFF reads as one byte");
    TEST_CHECK(counts, jpeg_bits_take(&bits, 8) == 0 && bits.at_marker, "the reader gives zero bits at a marker");
    test_arena_destroy(&arena);
}
