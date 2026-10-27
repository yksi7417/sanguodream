#ifndef WASM_PORTRAITS_H
#define WASM_PORTRAITS_H
/* The default portraits and the generated faces for the web platform layer
   (docs/ASSETS.md). It is the port of win32_portraits.h and win32_faces.h.
   It reads the portrait archive whole into memory, because the web build
   has no memory map. It makes a WebGL2 texture of a picture or of a face
   when the game first asks for it. The ImTextureID of a texture is its
   GL name, as in the OpenGL3 backend of ImGui. */

#include <stdlib.h>
#include <GLES3/gl3.h>
#include "hexdom.h"               /* PLATFORM_PORTRAIT_TEXTURE, PLATFORM_FACE_TEXTURE */
#include "render/zip.h"
#include "render/jpeg.h"
#include "render/portraits.h"
#include "render/faces.h"

#define WASM_PORTRAIT_SCRATCH_SIZE    Megabytes(24)   /* the largest member, its inflate state and the decoder */
#define WASM_PORTRAIT_TARGET_WIDTH    224             /* the texture of a portrait: the full size of a picture of the archive */
#define WASM_PORTRAIT_LOADS_PER_FRAME 1
#define WASM_FACES_PER_FRAME          64

/* An RGBA8 texture from pixels of width * height * 4 bytes. Zero on failure. */
internal GLuint
wasm_make_rgba_texture(u32 width, u32 height, const u8 *rgba)
{
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)width, (GLsizei)height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

/* ---- the default portraits ------------------------------------------------ */

struct wasm_portraits
{
    u8           *file;
    u64           size;
    zip_archive   archive;
    portrait_list list;
    GLuint        textures[PORTRAIT_LIST_CAPACITY];
    b32           failed[PORTRAIT_LIST_CAPACITY];
    u32           loads_this_frame;
    memory_arena  scratch;
    void         *scratch_memory;
    void        (*log)(const char *message);
};

global_variable wasm_portraits g_wasm_portraits;

/* Reads the archive at path and lists its pictures. The message says what
   the platform found, for the log. False if there is no usable archive. */
internal b32
wasm_portraits_open(wasm_portraits *portraits, const char *path, void (*log)(const char *message),
                    char *message, u32 message_size)
{
    memset(portraits, 0, sizeof(*portraits));
    portraits->log = log;
    FILE *file = fopen(path, "rb");
    if (!file)
    {
        snprintf(message, message_size, "platform: no portrait archive at %s; the UI shows the first character of each name", path);
        return false;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size > 0) portraits->file = (u8 *)malloc((size_t)size);
    b32 read = portraits->file && fread(portraits->file, 1, (size_t)size, file) == (size_t)size;
    fclose(file);
    if (!read) { snprintf(message, message_size, "platform: the portrait archive does not read"); return false; }
    portraits->size = (u64)size;
    if (!zip_open(&portraits->archive, portraits->file, portraits->size))
    {
        snprintf(message, message_size, "platform: the portrait archive is not a usable zip file: %s", portraits->archive.error);
        return false;
    }
    for (u32 member = 0; member < portraits->archive.member_count; ++member)
    {
        zip_member *entry = portraits->archive.members + member;
        portrait_list_add(&portraits->list, member, entry->name, entry->name_size);
    }
    portraits->scratch_memory = malloc((size_t)WASM_PORTRAIT_SCRATCH_SIZE);
    if (!portraits->scratch_memory) { snprintf(message, message_size, "platform: no memory for the portrait decoder"); return false; }
    arena_init(&portraits->scratch, portraits->scratch_memory, WASM_PORTRAIT_SCRATCH_SIZE);
    snprintf(message, message_size, "platform: %u portraits (%u numbered) in %s", portraits->list.count, portraits->list.numbered_count, path);
    return portraits->list.count > 0;
}

/* Extracts and decodes one picture into a texture. Zero on failure. */
internal GLuint
wasm_portraits_make_texture(wasm_portraits *portraits, u32 picture)
{
    arena_reset(&portraits->scratch);
    const char *error = 0;
    zip_contents file = zip_extract(&portraits->archive, portraits->list.pictures[picture].member, &portraits->scratch);
    jpeg_image image = {};
    if (!file.data) error = file.error;
    else
    {
        image = jpeg_decode_fitted(file.data, file.size, &portraits->scratch, WASM_PORTRAIT_TARGET_WIDTH);
        if (!image.rgba) error = image.error;
    }
    GLuint texture = error ? 0 : wasm_make_rgba_texture(image.width, image.height, image.rgba);
    if (!error && !texture) error = "the WebGL2 texture was not made";
    if (error && portraits->log)
    {
        char message[256];
        snprintf(message, sizeof(message), "platform: portrait %s does not decode: %s", portraits->list.pictures[picture].name, error);
        portraits->log(message);
    }
    return texture;
}

internal PLATFORM_PORTRAIT_TEXTURE(wasm_portrait_texture)
{
    wasm_portraits *portraits = &g_wasm_portraits;
    if (!portraits->file || picture >= portraits->list.count) return 0;
    if (portraits->textures[picture]) return (u64)portraits->textures[picture];
    if (portraits->failed[picture] || portraits->loads_this_frame >= WASM_PORTRAIT_LOADS_PER_FRAME) return 0;
    portraits->loads_this_frame += 1;
    portraits->textures[picture] = wasm_portraits_make_texture(portraits, picture);
    if (!portraits->textures[picture]) portraits->failed[picture] = true;
    return (u64)portraits->textures[picture];
}

/* ---- the generated faces -------------------------------------------------- */

/* The platform paints the face of an officer one time and keeps its
   texture by officer id. If the description of an officer changes, the
   platform paints it again (win32_faces.h). */
struct wasm_faces
{
    GLuint textures[FACE_SLOTS];
    u32    keys[FACE_SLOTS];
    u32    made_this_frame;
    u8     pixels[FACE_WIDTH * FACE_HEIGHT * 4];
};

global_variable wasm_faces g_wasm_faces;

internal PLATFORM_FACE_TEXTURE(wasm_face_texture)
{
    wasm_faces *faces = &g_wasm_faces;
    if (officer_id >= FACE_SLOTS || !(key & FACE_KEY_VALID)) return 0;
    if (faces->textures[officer_id] && faces->keys[officer_id] == key) return (u64)faces->textures[officer_id];
    if (faces->made_this_frame >= WASM_FACES_PER_FRAME) return (u64)faces->textures[officer_id];
    faces->made_this_frame += 1;
    if (faces->textures[officer_id]) glDeleteTextures(1, &faces->textures[officer_id]);
    face_description face = face_key_unpack(key);
    face_paint(&face, faces->pixels);
    faces->textures[officer_id] = wasm_make_rgba_texture(FACE_WIDTH, FACE_HEIGHT, faces->pixels);
    faces->keys[officer_id]     = key;
    return (u64)faces->textures[officer_id];
}

internal void
wasm_images_begin_frame(void)
{
    g_wasm_portraits.loads_this_frame = 0;
    g_wasm_faces.made_this_frame      = 0;
}

#endif /* WASM_PORTRAITS_H */
