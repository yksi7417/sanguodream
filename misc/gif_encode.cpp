/* gif_encode.exe: the GIF encoder of demo_player.exe when ffmpeg is not
   installed. The capture tool of the Dear ImGui Test Engine starts it as a
   video encoder and writes raw RGBA frames to its stdin.

   Usage: gif_encode.exe <width> <height> <fps> <output.gif>

   The encoder uses one fixed palette (a 6x6x6 color cube and 40 grays). It
   writes only the rectangle that changed since the last frame, and it
   merges identical frames into one longer frame. */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>

#include "base.h"

#define GIF_PALETTE_SIZE   256
#define GIF_CUBE_LEVELS    6
#define GIF_GRAY_COUNT     40
#define GIF_MAX_CODE       4095
#define GIF_HASH_SIZE      8191        /* prime, larger than the 4096 codes of the table */
#define GIF_BLOCK_SIZE     255

struct gif_palette
{
    u8 rgb[GIF_PALETTE_SIZE * 3];
    u8 lookup[32768];                  /* 15-bit color -> palette index */
    u8 lookup_valid[32768];
};

struct gif_bit_writer
{
    FILE *file;
    u8    block[GIF_BLOCK_SIZE];
    u32   block_size;
    u32   bits;
    u32   bit_count;
};

/* The LZW string table: (prefix code, next index) -> code, in an open hash.
   A generation number empties the table without a memset. */
struct gif_lzw_table
{
    u32 key[GIF_HASH_SIZE];
    u16 code[GIF_HASH_SIZE];
    u32 generation[GIF_HASH_SIZE];
    u32 current_generation;
};

struct gif_rect
{
    u32 left;
    u32 top;
    u32 width;
    u32 height;
};

/* A frame waits until the next different frame arrives, so that it gets the
   full time that it stays on the screen. */
struct gif_frame
{
    gif_rect rect;
    u32      delay_centiseconds;
    b32      is_pending;
};

internal void
gif_build_palette(gif_palette *palette)
{
    u32 entry = 0;
    for (u32 red = 0; red < GIF_CUBE_LEVELS; ++red)
    {
        for (u32 green = 0; green < GIF_CUBE_LEVELS; ++green)
        {
            for (u32 blue = 0; blue < GIF_CUBE_LEVELS; ++blue)
            {
                palette->rgb[entry * 3 + 0] = (u8)(red * 51);
                palette->rgb[entry * 3 + 1] = (u8)(green * 51);
                palette->rgb[entry * 3 + 2] = (u8)(blue * 51);
                entry++;
            }
        }
    }
    for (u32 gray = 0; gray < GIF_GRAY_COUNT; ++gray)
    {
        u8 level = (u8)((gray * 255) / (GIF_GRAY_COUNT - 1));
        palette->rgb[entry * 3 + 0] = level;
        palette->rgb[entry * 3 + 1] = level;
        palette->rgb[entry * 3 + 2] = level;
        entry++;
    }
    memset(palette->lookup_valid, 0, sizeof(palette->lookup_valid));
}

/* The nearest palette entry to a color, found one time for each 15-bit color. */
internal u8
gif_palette_index(gif_palette *palette, u32 rgba)
{
    u32 red   = rgba & 0xFF;
    u32 green = (rgba >> 8) & 0xFF;
    u32 blue  = (rgba >> 16) & 0xFF;
    u32 key   = ((red >> 3) << 10) | ((green >> 3) << 5) | (blue >> 3);
    if (palette->lookup_valid[key]) return palette->lookup[key];

    s32 sample_red   = (s32)((red >> 3) << 3) + 4;
    s32 sample_green = (s32)((green >> 3) << 3) + 4;
    s32 sample_blue  = (s32)((blue >> 3) << 3) + 4;
    u32 best_entry    = 0;
    s32 best_distance = 0x7FFFFFFF;
    for (u32 entry = 0; entry < GIF_PALETTE_SIZE; ++entry)
    {
        s32 delta_red   = sample_red   - (s32)palette->rgb[entry * 3 + 0];
        s32 delta_green = sample_green - (s32)palette->rgb[entry * 3 + 1];
        s32 delta_blue  = sample_blue  - (s32)palette->rgb[entry * 3 + 2];
        s32 distance = 3 * delta_red * delta_red + 4 * delta_green * delta_green + 2 * delta_blue * delta_blue;
        if (distance < best_distance)
        {
            best_distance = distance;
            best_entry    = entry;
        }
    }
    palette->lookup[key]       = (u8)best_entry;
    palette->lookup_valid[key] = 1;
    return (u8)best_entry;
}

/* ---- bit output in sub-blocks of 255 bytes ------------------------------- */

internal void
gif_flush_block(gif_bit_writer *writer)
{
    if (writer->block_size == 0) return;
    fputc((int)writer->block_size, writer->file);
    fwrite(writer->block, 1, writer->block_size, writer->file);
    writer->block_size = 0;
}

internal void
gif_put_byte(gif_bit_writer *writer, u8 value)
{
    writer->block[writer->block_size++] = value;
    if (writer->block_size == GIF_BLOCK_SIZE) gif_flush_block(writer);
}

internal void
gif_put_code(gif_bit_writer *writer, u32 code, u32 code_size)
{
    writer->bits      |= code << writer->bit_count;
    writer->bit_count += code_size;
    while (writer->bit_count >= 8)
    {
        gif_put_byte(writer, (u8)(writer->bits & 0xFF));
        writer->bits      >>= 8;
        writer->bit_count -= 8;
    }
}

internal void
gif_finish_bits(gif_bit_writer *writer)
{
    if (writer->bit_count > 0) gif_put_byte(writer, (u8)(writer->bits & 0xFF));
    writer->bits      = 0;
    writer->bit_count = 0;
    gif_flush_block(writer);
    fputc(0, writer->file);   /* block terminator */
}

/* ---- LZW ---------------------------------------------------------------- */

internal u32
gif_lzw_slot(gif_lzw_table *table, u32 key)
{
    u32 slot = (key * 2654435761u) % GIF_HASH_SIZE;
    while (table->generation[slot] == table->current_generation && table->key[slot] != key)
    {
        slot = (slot + 1) % GIF_HASH_SIZE;
    }
    return slot;
}

/* Writes the LZW image data of indices, with a minimum code size of 8 bits. */
internal void
gif_write_lzw(FILE *file, gif_lzw_table *table, const u8 *indices, u32 count)
{
    const u32 clear_code = 256;
    const u32 end_code   = 257;
    gif_bit_writer writer = {};
    writer.file = file;
    fputc(8, file);

    u32 code_size = 9;
    u32 max_code  = end_code;
    table->current_generation++;
    gif_put_code(&writer, clear_code, code_size);

    u32 current = indices[0];
    for (u32 index = 1; index < count; ++index)
    {
        u32 next = indices[index];
        u32 key  = (current << 8) | next;
        u32 slot = gif_lzw_slot(table, key);
        if (table->generation[slot] == table->current_generation)
        {
            current = table->code[slot];
            continue;
        }

        gif_put_code(&writer, current, code_size);
        max_code++;
        table->key[slot]        = key;
        table->code[slot]       = (u16)max_code;
        table->generation[slot] = table->current_generation;
        if (max_code >= (1u << code_size)) code_size++;
        if (max_code == GIF_MAX_CODE)
        {
            gif_put_code(&writer, clear_code, code_size);
            table->current_generation++;
            code_size = 9;
            max_code  = end_code;
        }
        current = next;
    }
    gif_put_code(&writer, current, code_size);
    gif_put_code(&writer, end_code, code_size);
    gif_finish_bits(&writer);
}

/* ---- file structure ----------------------------------------------------- */

internal void
gif_put_u16(FILE *file, u32 value)
{
    fputc((int)(value & 0xFF), file);
    fputc((int)((value >> 8) & 0xFF), file);
}

internal void
gif_write_header(FILE *file, gif_palette *palette, u32 width, u32 height)
{
    fwrite("GIF89a", 1, 6, file);
    gif_put_u16(file, width);
    gif_put_u16(file, height);
    fputc(0xF7, file);   /* global color table, 8 bits of color resolution, 256 entries */
    fputc(0, file);      /* background color index */
    fputc(0, file);      /* pixel aspect ratio */
    fwrite(palette->rgb, 1, sizeof(palette->rgb), file);
    /* The application extension that makes viewers loop the animation. */
    fputc(0x21, file); fputc(0xFF, file); fputc(11, file);
    fwrite("NETSCAPE2.0", 1, 11, file);
    fputc(3, file); fputc(1, file); gif_put_u16(file, 0); fputc(0, file);
}

internal void
gif_write_frame(FILE *file, gif_lzw_table *table, const u8 *screen, u32 screen_width,
                gif_frame *frame, u8 *scratch)
{
    gif_rect rect = frame->rect;
    for (u32 row = 0; row < rect.height; ++row)
    {
        memcpy(scratch + row * rect.width, screen + (rect.top + row) * screen_width + rect.left, rect.width);
    }
    u32 delay = frame->delay_centiseconds ? frame->delay_centiseconds : 1;
    fputc(0x21, file); fputc(0xF9, file); fputc(4, file);
    fputc(0x04, file);                        /* disposal: keep this frame under the next one */
    gif_put_u16(file, delay > 0xFFFF ? 0xFFFF : delay);
    fputc(0, file); fputc(0, file);
    fputc(0x2C, file);
    gif_put_u16(file, rect.left);
    gif_put_u16(file, rect.top);
    gif_put_u16(file, rect.width);
    gif_put_u16(file, rect.height);
    fputc(0, file);                           /* no local color table, not interlaced */
    gif_write_lzw(file, table, scratch, rect.width * rect.height);
}

/* The smallest rectangle that holds every index that changed. Returns false
   if the two screens are identical. */
internal b32
gif_changed_rect(const u8 *previous, const u8 *current, u32 width, u32 height, gif_rect *out)
{
    u32 left = width, right = 0, top = height, bottom = 0;
    for (u32 row = 0; row < height; ++row)
    {
        const u8 *previous_row = previous + row * width;
        const u8 *current_row  = current + row * width;
        if (memcmp(previous_row, current_row, width) == 0) continue;
        if (row < top) top = row;
        bottom = row;
        for (u32 column = 0; column < width; ++column)
        {
            if (previous_row[column] != current_row[column])
            {
                if (column < left)  left = column;
                if (column > right) right = column;
            }
        }
    }
    if (top == height) return false;
    out->left   = left;
    out->top    = top;
    out->width  = right - left + 1;
    out->height = bottom - top + 1;
    return true;
}

int
main(int argc, char **argv)
{
    if (argc != 5)
    {
        fprintf(stderr, "usage: gif_encode.exe <width> <height> <fps> <output.gif>\n");
        return 1;
    }
    u32 width  = (u32)strtoul(argv[1], 0, 10);
    u32 height = (u32)strtoul(argv[2], 0, 10);
    u32 fps    = (u32)strtoul(argv[3], 0, 10);
    if (width == 0 || height == 0 || width > 0xFFFF || height > 0xFFFF || fps == 0)
    {
        fprintf(stderr, "gif_encode: bad size or fps\n");
        return 1;
    }
    FILE *file = fopen(argv[4], "wb");
    if (!file)
    {
        fprintf(stderr, "gif_encode: cannot write %s\n", argv[4]);
        return 1;
    }
    _setmode(_fileno(stdin), _O_BINARY);

    u64 pixel_count = (u64)width * (u64)height;
    u32           *rgba     = (u32 *)malloc(pixel_count * 4);
    u8            *incoming = (u8 *)malloc(pixel_count);
    u8            *shown    = (u8 *)malloc(pixel_count);       /* the indices of the pending frame */
    u8            *scratch  = (u8 *)malloc(pixel_count);
    gif_palette   *palette  = (gif_palette *)malloc(sizeof(gif_palette));
    gif_lzw_table *table    = (gif_lzw_table *)calloc(1, sizeof(gif_lzw_table));
    if (!rgba || !incoming || !shown || !scratch || !palette || !table)
    {
        fprintf(stderr, "gif_encode: out of memory\n");
        return 1;
    }
    gif_build_palette(palette);
    gif_write_header(file, palette, width, height);

    gif_frame pending = {};
    u64 frame_count = 0;
    u64 written_centiseconds = 0;
    while (fread(rgba, 4, (size_t)pixel_count, stdin) == (size_t)pixel_count)
    {
        for (u64 pixel = 0; pixel < pixel_count; ++pixel)
        {
            incoming[pixel] = gif_palette_index(palette, rgba[pixel]);
        }
        frame_count++;
        u64 end_centiseconds = (frame_count * 100) / fps;

        gif_rect rect = { 0, 0, width, height };
        if (pending.is_pending)
        {
            if (!gif_changed_rect(shown, incoming, width, height, &rect))
            {
                pending.delay_centiseconds = (u32)(end_centiseconds - written_centiseconds);
                continue;
            }
            gif_write_frame(file, table, shown, width, &pending, scratch);
            written_centiseconds += pending.delay_centiseconds;
        }
        memcpy(shown, incoming, pixel_count);
        pending.rect               = rect;
        pending.delay_centiseconds = (u32)(end_centiseconds - written_centiseconds);
        pending.is_pending         = true;
    }
    if (pending.is_pending)
    {
        gif_write_frame(file, table, shown, width, &pending, scratch);
    }
    fputc(0x3B, file);
    fclose(file);
    printf("gif_encode: %llu frames to %s\n", (unsigned long long)frame_count, argv[4]);
    return frame_count ? 0 : 1;
}
