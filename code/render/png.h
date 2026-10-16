#ifndef PNG_H
#define PNG_H
/* A small PNG decoder over the engine's own inflate: 8-bit RGB or RGBA,
   non-interlaced, the five standard filters. Enough for the placeholder
   materials and sprites. The platform layer uses it to load textures; the
   output is RGBA8, row-major, in the arena. */

#include "base.h"
#include "sim/inflate.h"

struct png_image
{
    u32  width;
    u32  height;
    u8  *rgba;        /* width * height * 4 bytes, or zero on failure */
    const char *error;
};

internal u32
png_read_u32(const u8 *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

internal u8
png_paeth(u8 a, u8 b, u8 c)
{
    s32 p = (s32)a + (s32)b - (s32)c;
    s32 pa = p > a ? p - a : a - p;
    s32 pb = p > b ? p - b : b - p;
    s32 pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

internal png_image
png_decode(const u8 *data, u64 size, memory_arena *arena)
{
    png_image image = {};
    static const u8 signature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    if (size < 8 || memcmp(data, signature, 8) != 0) { image.error = "not a PNG"; return image; }

    u32 width = 0, height = 0;
    u8 bit_depth = 0, color_type = 0, interlace = 0;
    u8 *idat = 0;
    u64 idat_size = 0, idat_capacity = 0;
    u64 at = 8;
    b32 done = false;
    while (!done && at + 12 <= size)
    {
        u32 length = png_read_u32(data + at);
        const u8 *type = data + at + 4;
        const u8 *body = data + at + 8;
        if (at + 12 + length > size) { image.error = "truncated chunk"; return image; }
        if (memcmp(type, "IHDR", 4) == 0)
        {
            if (length != 13) { image.error = "bad IHDR"; return image; }
            width = png_read_u32(body); height = png_read_u32(body + 4);
            bit_depth = body[8]; color_type = body[9]; interlace = body[12];
            if (bit_depth != 8 || (color_type != 2 && color_type != 6) || interlace != 0)
            {
                image.error = "unsupported PNG: need 8-bit RGB or RGBA, no interlace";
                return image;
            }
        }
        else if (memcmp(type, "IDAT", 4) == 0)
        {
            if (!idat)
            {
                idat_capacity = size;   /* the compressed data cannot exceed the file */
                idat = PushArray(arena, idat_capacity, u8);
            }
            if (idat_size + length > idat_capacity) { image.error = "IDAT overflow"; return image; }
            memcpy(idat + idat_size, body, length);
            idat_size += length;
        }
        else if (memcmp(type, "IEND", 4) == 0)
        {
            done = true;
        }
        at += 12 + length;
    }
    if (!width || !height || !idat) { image.error = "missing IHDR or IDAT"; return image; }

    u32 channels = color_type == 6 ? 4 : 3;
    u64 stride   = (u64)width * channels;
    u64 raw_size = (stride + 1) * height;
    u8 *raw = PushArray(arena, raw_size, u8);
    inflate_result inflated = zlib_inflate(arena, idat, idat_size, raw, raw_size);
    if (inflated.status != INFLATE_OK || inflated.bytes_written != raw_size) { image.error = "inflate failed or wrong size"; return image; }

    u8 *rgba = PushArray(arena, (u64)width * height * 4, u8);
    u8 *previous = 0;
    for (u32 y = 0; y < height; ++y)
    {
        u8  filter = raw[y * (stride + 1)];
        u8 *line   = raw + y * (stride + 1) + 1;
        for (u64 i = 0; i < stride; ++i)
        {
            u8 a = i >= channels ? line[i - channels] : 0;
            u8 b = previous ? previous[i] : 0;
            u8 c = (previous && i >= channels) ? previous[i - channels] : 0;
            u8 x = line[i];
            switch (filter)
            {
                case 0: break;
                case 1: x = (u8)(x + a); break;
                case 2: x = (u8)(x + b); break;
                case 3: x = (u8)(x + (u8)(((u32)a + (u32)b) / 2)); break;
                case 4: x = (u8)(x + png_paeth(a, b, c)); break;
                default: image.error = "bad filter"; return image;
            }
            line[i] = x;
        }
        for (u32 px = 0; px < width; ++px)
        {
            u8 *out = rgba + ((u64)y * width + px) * 4;
            u8 *in  = line + (u64)px * channels;
            out[0] = in[0]; out[1] = in[1]; out[2] = in[2];
            out[3] = channels == 4 ? in[3] : 255;
        }
        previous = line;
    }
    image.width = width; image.height = height; image.rgba = rgba; image.error = 0;
    return image;
}

#endif /* PNG_H */
