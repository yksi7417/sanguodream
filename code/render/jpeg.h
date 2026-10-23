#ifndef JPEG_H
#define JPEG_H
/* A small decoder of baseline JPEG files, at one eighth of their size or at
   full size. The eighth decode keeps only the DC coefficient of each 8x8
   block, that is the mean of the block, and gives one pixel for each block:
   a 1792 x 2240 file gives 224 x 280 pixels. The full decode runs the
   inverse DCT of each block and gives every pixel of the frame: the 224 x
   280 pictures of the portrait archive decode at full size. The fitted
   decode picks the eighth when it gives the target width. The output is
   RGBA8, row-major, in the arena. The platform layer uses it for the
   default portraits.

   The decoder reads 8-bit sequential Huffman files (SOF0, SOF1) with one
   component (grey) or three (YCbCr), any sampling factors from 1 to 4, and
   restart markers. It refuses progressive and arithmetic files and files
   with more than one scan. */

#include "base.h"

#define JPEG_MAX_COMPONENTS   3
#define JPEG_MAX_SAMPLING     4
#define JPEG_TABLE_SLOTS      4     /* table ids 0..3 for each kind of table */
#define JPEG_MAX_CODE_LENGTH  16
#define JPEG_FAST_BITS        9     /* codes up to this length decode with one table look-up */
#define JPEG_BLOCK_SIDE       8
#define JPEG_COEFFICIENTS     64
#define JPEG_MAX_CATEGORY     15    /* the bit count of a coefficient value */
#define JPEG_LEVEL_SHIFT      128
#define JPEG_FIXED_ONE        65536 /* 16.16 fixed point for the colour transform */

/* The second byte of each marker that the decoder reads. */
enum jpeg_marker
{
    JPEG_SOF0 = 0xC0,   /* baseline */
    JPEG_SOF1 = 0xC1,   /* extended sequential, Huffman */
    JPEG_SOF2 = 0xC2,   /* progressive: refused */
    JPEG_DHT  = 0xC4,
    JPEG_JPG  = 0xC8,   /* reserved: not a frame */
    JPEG_DAC  = 0xCC,   /* arithmetic conditioning: not a frame */
    JPEG_SOF15 = 0xCF,
    JPEG_RST0 = 0xD0,
    JPEG_RST7 = 0xD7,
    JPEG_SOI  = 0xD8,
    JPEG_EOI  = 0xD9,
    JPEG_SOS  = 0xDA,
    JPEG_DQT  = 0xDB,
    JPEG_DRI  = 0xDD
};

struct jpeg_image
{
    u32         width;      /* of the output: one eighth of the file, rounded up, or the full size */
    u32         height;
    u8         *rgba;       /* width * height * 4 bytes, or zero on failure */
    const char *error;
};

/* A canonical Huffman table with a look-up table for the short codes. */
struct jpeg_huffman
{
    b32 defined;
    u8  fast_symbol[1 << JPEG_FAST_BITS];
    u8  fast_length[1 << JPEG_FAST_BITS];      /* 0: the code is longer than JPEG_FAST_BITS */
    u16 count_per_length[JPEG_MAX_CODE_LENGTH + 1];
    u32 first_code[JPEG_MAX_CODE_LENGTH + 1];
    u16 first_index[JPEG_MAX_CODE_LENGTH + 1];
    u8  symbols[256];
};

struct jpeg_component
{
    u8   id;
    u8   horizontal;          /* sampling factors */
    u8   vertical;
    u8   quant_table;
    u8   dc_table;
    u8   ac_table;
    s32  predictor;           /* the DC value of the last block */
    u32  blocks_across;
    u32  blocks_down;
    u8  *means;               /* one mean sample for each block */
    u8  *samples;             /* the full decode: blocks_across * 8 samples in each of blocks_down * 8 rows */
};

/* The natural index (row * 8 + column) of each zigzag position (figure A.6). */
global_variable const u8 jpeg_zigzag[JPEG_COEFFICIENTS] = {
     0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
};

/* The bit reader of the entropy data. It removes the stuffed zero after
   each 0xFF byte and stops at a marker. */
struct jpeg_bits
{
    const u8 *data;
    u64       size;
    u64       position;
    u32       buffer;         /* the next bits, from the high bit down */
    u32       count;
    b32       at_marker;
};

struct jpeg_decoder
{
    const u8      *data;
    u64            size;
    u32            width;
    u32            height;
    u32            component_count;
    jpeg_component components[JPEG_MAX_COMPONENTS];
    u16            quant[JPEG_TABLE_SLOTS][JPEG_COEFFICIENTS];   /* in zigzag order, as the file stores them */
    jpeg_huffman   dc_tables[JPEG_TABLE_SLOTS];
    jpeg_huffman   ac_tables[JPEG_TABLE_SLOTS];
    u32            restart_interval;
    b32            has_frame;
    b32            full;                                        /* decode every sample, not only the means */
    const char    *error;
};

internal u16
jpeg_read_u16(const u8 *p)
{
    return (u16)((p[0] << 8) | p[1]);
}

internal void
jpeg_bits_fill(jpeg_bits *bits)
{
    while (bits->count <= 24)
    {
        u32 next = 0;
        if (!bits->at_marker && bits->position < bits->size)
        {
            next = bits->data[bits->position];
            if (next == 0xFF)
            {
                u8 after = bits->position + 1 < bits->size ? bits->data[bits->position + 1] : 0xD9;
                if (after == 0) bits->position += 2;
                else { bits->at_marker = true; next = 0; }
            }
            else
            {
                bits->position += 1;
            }
        }
        bits->buffer |= next << (24 - bits->count);
        bits->count  += 8;
    }
}

internal u32
jpeg_bits_take(jpeg_bits *bits, u32 count)
{
    if (count == 0) return 0;
    jpeg_bits_fill(bits);
    u32 value = bits->buffer >> (32 - count);
    bits->buffer <<= count;
    bits->count   -= count;
    return value;
}

/* F.2.2.1: a value of category bits, with the sign in the first bit. */
internal s32
jpeg_bits_signed(jpeg_bits *bits, u32 category)
{
    if (category == 0) return 0;
    s32 value = (s32)jpeg_bits_take(bits, category);
    if (value < (1 << (category - 1))) value += 1 - (1 << category);
    return value;
}

/* The next symbol of the table, or -1 for a code that the table does not hold. */
internal s32
jpeg_bits_symbol(jpeg_bits *bits, jpeg_huffman *table)
{
    jpeg_bits_fill(bits);
    u32 peek = bits->buffer >> (32 - JPEG_FAST_BITS);
    u32 fast = table->fast_length[peek];
    if (fast)
    {
        bits->buffer <<= fast;
        bits->count   -= fast;
        return table->fast_symbol[peek];
    }
    for (u32 length = JPEG_FAST_BITS + 1; length <= JPEG_MAX_CODE_LENGTH; ++length)
    {
        u32 code = bits->buffer >> (32 - length);
        if (code - table->first_code[length] < table->count_per_length[length])
        {
            bits->buffer <<= length;
            bits->count   -= length;
            return table->symbols[table->first_index[length] + code - table->first_code[length]];
        }
    }
    return -1;
}

/* Builds the canonical codes (annex C) from the counts and the symbols. */
internal b32
jpeg_build_huffman(jpeg_huffman *table)
{
    memset(table->fast_length, 0, sizeof(table->fast_length));
    u32 code = 0, index = 0;
    for (u32 length = 1; length <= JPEG_MAX_CODE_LENGTH; ++length)
    {
        table->first_code[length]  = code;
        table->first_index[length] = (u16)index;
        for (u32 counted = 0; counted < table->count_per_length[length]; ++counted, ++code, ++index)
        {
            if (code >= (1u << length)) return false;   /* more codes than the length can hold */
            if (length > JPEG_FAST_BITS) continue;
            u32 shift = JPEG_FAST_BITS - length;
            for (u32 fill = 0; fill < (1u << shift); ++fill)
            {
                table->fast_symbol[(code << shift) + fill] = table->symbols[index];
                table->fast_length[(code << shift) + fill] = (u8)length;
            }
        }
        code <<= 1;
    }
    table->defined = true;
    return true;
}

internal b32
jpeg_read_quant(jpeg_decoder *decoder, const u8 *body, u32 length)
{
    u32 at = 0;
    while (at < length)
    {
        u32 precision = body[at] >> 4, slot = body[at] & 0x0F;
        u32 entry_size = precision ? 2 : 1;
        if (slot >= JPEG_TABLE_SLOTS || at + 1 + JPEG_COEFFICIENTS * entry_size > length) return false;
        for (u32 entry = 0; entry < JPEG_COEFFICIENTS; ++entry)
        {
            const u8 *value = body + at + 1 + entry * entry_size;
            decoder->quant[slot][entry] = precision ? jpeg_read_u16(value) : *value;
        }
        at += 1 + JPEG_COEFFICIENTS * entry_size;
    }
    return true;
}

internal b32
jpeg_read_huffman(jpeg_decoder *decoder, const u8 *body, u32 length)
{
    u32 at = 0;
    while (at < length)
    {
        if (at + 1 + JPEG_MAX_CODE_LENGTH > length) return false;
        u32 kind = body[at] >> 4, slot = body[at] & 0x0F;
        if (kind > 1 || slot >= JPEG_TABLE_SLOTS) return false;
        jpeg_huffman *table = kind ? decoder->ac_tables + slot : decoder->dc_tables + slot;
        u32 total = 0;
        for (u32 length_of_code = 1; length_of_code <= JPEG_MAX_CODE_LENGTH; ++length_of_code)
        {
            table->count_per_length[length_of_code] = body[at + length_of_code];
            total += body[at + length_of_code];
        }
        at += 1 + JPEG_MAX_CODE_LENGTH;
        if (total > 256 || at + total > length) return false;
        memcpy(table->symbols, body + at, total);
        at += total;
        if (!jpeg_build_huffman(table)) return false;
    }
    return true;
}

internal b32
jpeg_read_frame(jpeg_decoder *decoder, const u8 *body, u32 length)
{
    if (length < 6 || body[0] != 8) return false;   /* 8-bit samples only */
    decoder->height          = jpeg_read_u16(body + 1);
    decoder->width           = jpeg_read_u16(body + 3);
    decoder->component_count = body[5];
    if (decoder->width == 0 || decoder->height == 0) return false;
    if (decoder->component_count != 1 && decoder->component_count != 3) return false;
    if (length < 6 + 3 * decoder->component_count) return false;
    for (u32 index = 0; index < decoder->component_count; ++index)
    {
        jpeg_component *component = decoder->components + index;
        const u8 *spec = body + 6 + 3 * index;
        component->id          = spec[0];
        component->horizontal  = spec[1] >> 4;
        component->vertical    = spec[1] & 0x0F;
        component->quant_table = spec[2];
        if (component->horizontal < 1 || component->horizontal > JPEG_MAX_SAMPLING ||
            component->vertical < 1 || component->vertical > JPEG_MAX_SAMPLING ||
            component->quant_table >= JPEG_TABLE_SLOTS)
        {
            return false;
        }
    }
    if (decoder->component_count == 1) decoder->components[0].horizontal = decoder->components[0].vertical = 1;
    decoder->has_frame = true;
    return true;
}

/* Decodes one block: the mean of its samples, and with coefficients the 64
   dequantized coefficients in natural order for the inverse DCT. */
internal b32
jpeg_decode_block(jpeg_bits *bits, jpeg_decoder *decoder, jpeg_component *component, u8 *mean, s32 *coefficients)
{
    const u16 *quant = decoder->quant[component->quant_table];
    s32 category = jpeg_bits_symbol(bits, decoder->dc_tables + component->dc_table);
    if (category < 0 || category > JPEG_MAX_CATEGORY) return false;
    component->predictor += jpeg_bits_signed(bits, (u32)category);
    if (coefficients)
    {
        memset(coefficients, 0, JPEG_COEFFICIENTS * sizeof(*coefficients));
        coefficients[0] = component->predictor * quant[0];
    }

    for (u32 coefficient = 1; coefficient < JPEG_COEFFICIENTS;)
    {
        s32 run_and_size = jpeg_bits_symbol(bits, decoder->ac_tables + component->ac_table);
        if (run_and_size < 0) return false;
        u32 run = (u32)run_and_size >> 4, size = (u32)run_and_size & 0x0F;
        if (size == 0)
        {
            if (run != 15) break;   /* end of block */
            coefficient += 16;
            continue;
        }
        coefficient += run;
        if (coefficient >= JPEG_COEFFICIENTS) return false;
        s32 value = jpeg_bits_signed(bits, size);
        if (coefficients) coefficients[jpeg_zigzag[coefficient]] = value * quant[coefficient];
        coefficient += 1;
    }

    /* F(0,0) is eight times the mean of the 64 shifted samples. */
    s32 level = component->predictor * quant[0] / JPEG_BLOCK_SIDE + JPEG_LEVEL_SHIFT;
    *mean = (u8)(level < 0 ? 0 : level > 255 ? 255 : level);
    return true;
}

/* The inverse DCT of one block (A.3.3), separable, in floats: the rows
   first, then the columns. out is the top left sample of the block in a
   plane with this stride. */
internal void
jpeg_idct_block(const s32 *coefficients, u8 *out, u32 stride)
{
    local_persist f32 basis[JPEG_BLOCK_SIDE][JPEG_BLOCK_SIDE];   /* basis[x][u] = C(u) cos((2x + 1) u pi / 16) */
    local_persist b32 basis_ready = false;
    if (!basis_ready)
    {
        for (u32 x = 0; x < JPEG_BLOCK_SIDE; ++x)
        {
            for (u32 u = 0; u < JPEG_BLOCK_SIDE; ++u)
            {
                f32 scale = u == 0 ? 0.70710678f : 1.0f;
                basis[x][u] = scale * cosf((2.0f * (f32)x + 1.0f) * (f32)u * 3.14159265f / 16.0f);
            }
        }
        basis_ready = true;
    }
    f32 rows[JPEG_BLOCK_SIDE][JPEG_BLOCK_SIDE];   /* rows[v][x]: the row transform of each frequency row */
    for (u32 v = 0; v < JPEG_BLOCK_SIDE; ++v)
    {
        for (u32 x = 0; x < JPEG_BLOCK_SIDE; ++x)
        {
            f32 sum = 0.0f;
            for (u32 u = 0; u < JPEG_BLOCK_SIDE; ++u) sum += basis[x][u] * (f32)coefficients[v * JPEG_BLOCK_SIDE + u];
            rows[v][x] = sum;
        }
    }
    for (u32 y = 0; y < JPEG_BLOCK_SIDE; ++y)
    {
        for (u32 x = 0; x < JPEG_BLOCK_SIDE; ++x)
        {
            f32 sum = 0.0f;
            for (u32 v = 0; v < JPEG_BLOCK_SIDE; ++v) sum += basis[y][v] * rows[v][x];
            s32 level = (s32)(sum * 0.25f + (sum < 0.0f ? -0.5f : 0.5f)) + JPEG_LEVEL_SHIFT;
            out[y * stride + x] = (u8)(level < 0 ? 0 : level > 255 ? 255 : level);
        }
    }
}

/* At a restart marker: skip it, empty the bit reader and reset the DC predictors. */
internal b32
jpeg_restart(jpeg_bits *bits, jpeg_decoder *decoder)
{
    jpeg_bits_fill(bits);
    if (!bits->at_marker || bits->position + 1 >= bits->size) return false;
    u8 marker = bits->data[bits->position + 1];
    if (marker < JPEG_RST0 || marker > JPEG_RST7) return false;
    bits->position += 2;
    bits->buffer = bits->count = 0;
    bits->at_marker = false;
    for (u32 index = 0; index < decoder->component_count; ++index) decoder->components[index].predictor = 0;
    return true;
}

/* Reads one scan that holds every component and fills the means. */
internal b32
jpeg_read_scan(jpeg_decoder *decoder, const u8 *body, u32 length, u64 data_start, memory_arena *arena)
{
    if (!decoder->has_frame || length < 1 || body[0] != decoder->component_count || length < 1 + 2 * (u32)body[0] + 3)
    {
        decoder->error = "the decoder reads one scan with all the components";
        return false;
    }
    for (u32 index = 0; index < decoder->component_count; ++index)
    {
        u8 id = body[1 + 2 * index], tables = body[2 + 2 * index];
        jpeg_component *component = decoder->components + index;
        if (component->id != id) { decoder->error = "the scan has the components in another order"; return false; }
        component->dc_table = tables >> 4;
        component->ac_table = tables & 0x0F;
        if (component->dc_table >= JPEG_TABLE_SLOTS || component->ac_table >= JPEG_TABLE_SLOTS ||
            !decoder->dc_tables[component->dc_table].defined || !decoder->ac_tables[component->ac_table].defined)
        {
            decoder->error = "the scan uses a Huffman table that the file does not define";
            return false;
        }
    }

    u32 most_across = 1, most_down = 1;
    for (u32 index = 0; index < decoder->component_count; ++index)
    {
        if (decoder->components[index].horizontal > most_across) most_across = decoder->components[index].horizontal;
        if (decoder->components[index].vertical   > most_down)   most_down   = decoder->components[index].vertical;
    }
    u32 mcus_across = (decoder->width  + JPEG_BLOCK_SIDE * most_across - 1) / (JPEG_BLOCK_SIDE * most_across);
    u32 mcus_down   = (decoder->height + JPEG_BLOCK_SIDE * most_down - 1)   / (JPEG_BLOCK_SIDE * most_down);
    for (u32 index = 0; index < decoder->component_count; ++index)
    {
        jpeg_component *component = decoder->components + index;
        component->blocks_across = mcus_across * component->horizontal;
        component->blocks_down   = mcus_down * component->vertical;
        component->means         = PushArray(arena, (u64)component->blocks_across * component->blocks_down, u8);
        component->samples       = decoder->full ? PushArray(arena, (u64)component->blocks_across * component->blocks_down * JPEG_COEFFICIENTS, u8) : 0;
        component->predictor     = 0;
    }
    s32 coefficients[JPEG_COEFFICIENTS];

    jpeg_bits bits = {};
    bits.data     = decoder->data;
    bits.size     = decoder->size;
    bits.position = data_start;
    u32 until_restart = decoder->restart_interval;
    for (u32 mcu_y = 0; mcu_y < mcus_down; ++mcu_y)
    {
        for (u32 mcu_x = 0; mcu_x < mcus_across; ++mcu_x)
        {
            if (decoder->restart_interval)
            {
                if (until_restart == 0)
                {
                    if (!jpeg_restart(&bits, decoder)) { decoder->error = "a restart marker is missing"; return false; }
                    until_restart = decoder->restart_interval;
                }
                until_restart -= 1;
            }
            for (u32 index = 0; index < decoder->component_count; ++index)
            {
                jpeg_component *component = decoder->components + index;
                for (u32 row = 0; row < component->vertical; ++row)
                {
                    for (u32 column = 0; column < component->horizontal; ++column)
                    {
                        u32 block_x = mcu_x * component->horizontal + column;
                        u32 block_y = mcu_y * component->vertical + row;
                        if (!jpeg_decode_block(&bits, decoder, component, component->means + block_y * component->blocks_across + block_x,
                                               decoder->full ? coefficients : 0))
                        {
                            decoder->error = "bad Huffman data";
                            return false;
                        }
                        if (decoder->full)
                        {
                            u32 stride = component->blocks_across * JPEG_BLOCK_SIDE;
                            jpeg_idct_block(coefficients, component->samples + (u64)block_y * JPEG_BLOCK_SIDE * stride + block_x * JPEG_BLOCK_SIDE, stride);
                        }
                    }
                }
            }
        }
    }
    return true;
}

/* The samples as RGBA: the means of the blocks, one pixel for each block,
   or with full the samples of the frame. A component with less sampling
   than the most covers more output pixels. */
internal void
jpeg_write_pixels(jpeg_decoder *decoder, jpeg_image *image, memory_arena *arena)
{
    u32 most_across = 1, most_down = 1;
    for (u32 index = 0; index < decoder->component_count; ++index)
    {
        if (decoder->components[index].horizontal > most_across) most_across = decoder->components[index].horizontal;
        if (decoder->components[index].vertical   > most_down)   most_down   = decoder->components[index].vertical;
    }
    if (decoder->full)
    {
        image->width  = decoder->width;
        image->height = decoder->height;
    }
    else
    {
        image->width  = (decoder->width  + JPEG_BLOCK_SIDE - 1) / JPEG_BLOCK_SIDE;
        image->height = (decoder->height + JPEG_BLOCK_SIDE - 1) / JPEG_BLOCK_SIDE;
    }
    image->rgba   = PushArray(arena, (u64)image->width * image->height * 4, u8);
    for (u32 pixel_y = 0; pixel_y < image->height; ++pixel_y)
    {
        for (u32 pixel_x = 0; pixel_x < image->width; ++pixel_x)
        {
            s32 samples[JPEG_MAX_COMPONENTS] = {};
            for (u32 index = 0; index < decoder->component_count; ++index)
            {
                jpeg_component *component = decoder->components + index;
                u32 sample_x = pixel_x * component->horizontal / most_across;
                u32 sample_y = pixel_y * component->vertical / most_down;
                samples[index] = decoder->full ? component->samples[(u64)sample_y * component->blocks_across * JPEG_BLOCK_SIDE + sample_x]
                                               : component->means[sample_y * component->blocks_across + sample_x];
            }
            s32 red = samples[0], green = samples[0], blue = samples[0];
            if (decoder->component_count == 3)
            {
                /* JFIF: the full-range YCbCr of ITU-R BT.601. */
                s32 blue_difference = samples[1] - JPEG_LEVEL_SHIFT, red_difference = samples[2] - JPEG_LEVEL_SHIFT;
                red   = samples[0] + (91881 * red_difference + JPEG_FIXED_ONE / 2) / JPEG_FIXED_ONE;
                green = samples[0] - (22554 * blue_difference + 46802 * red_difference - JPEG_FIXED_ONE / 2) / JPEG_FIXED_ONE;
                blue  = samples[0] + (116130 * blue_difference + JPEG_FIXED_ONE / 2) / JPEG_FIXED_ONE;
            }
            u8 *pixel = image->rgba + ((u64)pixel_y * image->width + pixel_x) * 4;
            pixel[0] = (u8)(red   < 0 ? 0 : red   > 255 ? 255 : red);
            pixel[1] = (u8)(green < 0 ? 0 : green > 255 ? 255 : green);
            pixel[2] = (u8)(blue  < 0 ? 0 : blue  > 255 ? 255 : blue);
            pixel[3] = 255;
        }
    }
}

/* Decodes a JPEG file at one eighth of its size, or with full at its full
   size. The decoder state and the output go into the arena. */
internal jpeg_image
jpeg_decode(const u8 *data, u64 size, memory_arena *arena, b32 full)
{
    jpeg_image image = {};
    if (size < 4 || data[0] != 0xFF || data[1] != JPEG_SOI) { image.error = "not a JPEG file"; return image; }
    jpeg_decoder *decoder = PushStruct(arena, jpeg_decoder);
    decoder->data = data;
    decoder->size = size;
    decoder->full = full;

    u64 at = 2;
    for (;;)
    {
        while (at < size && data[at] == 0xFF && at + 1 < size && data[at + 1] == 0xFF) at += 1;   /* fill bytes */
        if (at + 4 > size || data[at] != 0xFF) { image.error = "the file ends before the scan"; return image; }
        u8  marker = data[at + 1];
        u32 length = jpeg_read_u16(data + at + 2);
        if (length < 2 || at + 2 + length > size) { image.error = "a segment goes past the end of the file"; return image; }
        const u8 *body = data + at + 4;
        u32 body_length = length - 2;
        b32 good = true;
        if      (marker == JPEG_DQT)                         good = jpeg_read_quant(decoder, body, body_length);
        else if (marker == JPEG_DHT)                         good = jpeg_read_huffman(decoder, body, body_length);
        else if (marker == JPEG_SOF0 || marker == JPEG_SOF1) good = jpeg_read_frame(decoder, body, body_length);
        else if (marker == JPEG_DRI)                         { good = body_length >= 2; if (good) decoder->restart_interval = jpeg_read_u16(body); }
        else if (marker >= JPEG_SOF2 && marker <= JPEG_SOF15 && marker != JPEG_DHT && marker != JPEG_JPG && marker != JPEG_DAC)
        {
            image.error = "a progressive, lossless or arithmetic JPEG file";
            return image;
        }
        else if (marker == JPEG_SOS)
        {
            if (!jpeg_read_scan(decoder, body, body_length, at + 2 + length, arena)) { image.error = decoder->error; return image; }
            jpeg_write_pixels(decoder, &image, arena);
            return image;
        }
        if (!good) { image.error = "a bad table or frame header"; return image; }
        at += 2 + length;
    }
}

internal jpeg_image
jpeg_decode_eighth(const u8 *data, u64 size, memory_arena *arena)
{
    return jpeg_decode(data, size, arena, false);
}

internal jpeg_image
jpeg_decode_full(const u8 *data, u64 size, memory_arena *arena)
{
    return jpeg_decode(data, size, arena, true);
}

/* The width of the frame, from the frame header, or 0 when the file has
   none before the scan. */
internal u32
jpeg_frame_width(const u8 *data, u64 size)
{
    if (size < 4 || data[0] != 0xFF || data[1] != JPEG_SOI) return 0;
    u64 at = 2;
    while (at + 4 <= size && data[at] == 0xFF)
    {
        u8  marker = data[at + 1];
        if (marker == 0xFF) { at += 1; continue; }
        if (marker == JPEG_SOS || marker == JPEG_EOI) return 0;
        u32 length = jpeg_read_u16(data + at + 2);
        if (length < 2 || at + 2 + length > size) return 0;
        if ((marker == JPEG_SOF0 || marker == JPEG_SOF1) && length >= 7) return jpeg_read_u16(data + at + 4 + 3);
        at += 2 + length;
    }
    return 0;
}

/* Decodes at one eighth when the eighth of the frame is at least
   target_width pixels across, else at full size. The pictures of the
   portrait archive are 224 x 280, so they decode at full size, and a
   1792 x 2240 picture of an older archive decodes at one eighth. */
internal jpeg_image
jpeg_decode_fitted(const u8 *data, u64 size, memory_arena *arena, u32 target_width)
{
    u32 width = jpeg_frame_width(data, size);
    return jpeg_decode(data, size, arena, width / JPEG_BLOCK_SIDE < target_width);
}

#endif /* JPEG_H */
