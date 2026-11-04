#ifndef PICTURE_H
#define PICTURE_H
/* A decoder for a picture of the pack. It reads the first bytes of the file
   and calls the PNG decoder or the JPEG decoder at full size. The output is
   RGBA8, row-major, in the arena. A JPEG picture is opaque. */

#include "base.h"
#include "render/png.h"
#include "render/jpeg.h"

/* A model has its texture in one of these files. The renderers try them in
   this order, so a JPEG texture wins over a PNG texture of the same name. */
#define PICTURE_MODEL_TEXTURE_EXTENSIONS 2
global_variable const char *picture_model_texture_extensions[PICTURE_MODEL_TEXTURE_EXTENSIONS] = { "jpg", "png" };

internal png_image
picture_decode(const u8 *data, u64 size, memory_arena *arena)
{
    if (size >= 2 && data[0] == 0xFF && data[1] == JPEG_SOI)
    {
        jpeg_image decoded = jpeg_decode_full(data, size, arena);
        png_image image = {};
        image.width  = decoded.width;
        image.height = decoded.height;
        image.rgba   = decoded.rgba;
        image.error  = decoded.error;
        if (image.error) image.rgba = 0;
        return image;
    }
    return png_decode(data, size, arena);
}

#endif /* PICTURE_H */
