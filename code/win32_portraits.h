#ifndef WIN32_PORTRAITS_H
#define WIN32_PORTRAITS_H
/* The default portraits for the platform layer (docs/ASSETS.md). It maps
   the portrait archive into memory and lists its pictures. It makes a
   D3D11 texture of a picture when the game first asks for it. A picture
   of the archive is 224 x 280 and decodes at full size (render/jpeg.h). A
   1792 x 2240 picture of an older archive decodes at one eighth, to the
   same 224 x 280. Include it after win32_window.h. */

#include "hexdom.h"               /* PLATFORM_PORTRAIT_TEXTURE */
#include "win32_faces.h"          /* win32_make_rgba_texture, and the generated faces */
#include "render/zip.h"
#include "render/jpeg.h"
#include "render/portraits.h"

#define WIN32_PORTRAIT_SCRATCH_SIZE    Megabytes(24)   /* the largest member, its inflate state and the decoder */
#define WIN32_PORTRAIT_TARGET_WIDTH    224             /* the texture of a portrait: the full size of a picture of the archive */
#define WIN32_PORTRAIT_LOADS_PER_FRAME 1               /* one picture takes approximately 35 ms at /O2: one for each frame at most */

struct win32_portraits
{
    HANDLE                    file;
    HANDLE                    mapping;
    const u8                 *view;
    u64                       size;
    ID3D11Device             *device;
    zip_archive               archive;
    portrait_list             list;
    ID3D11ShaderResourceView *textures[PORTRAIT_LIST_CAPACITY];
    b32                       failed[PORTRAIT_LIST_CAPACITY];
    u32                       loads_this_frame;
    memory_arena              scratch;
    void                     *scratch_memory;
    void                    (*log)(const char *message);
};

global_variable win32_portraits g_win32_portraits;

/* Opens the archive at path and lists its pictures. The message says what
   the platform found, for the log. False if there is no usable archive. */
internal b32
win32_portraits_open(win32_portraits *portraits, ID3D11Device *device, const char *path,
                     void (*log)(const char *message), char *message, u32 message_size)
{
    memset(portraits, 0, sizeof(*portraits));
    portraits->device = device;
    portraits->log    = log;
    portraits->file   = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (portraits->file == INVALID_HANDLE_VALUE)
    {
        portraits->file = 0;
        snprintf(message, message_size, "platform: no portrait archive at %s; the UI shows the first character of each name", path);
        return false;
    }
    LARGE_INTEGER size;
    if (!GetFileSizeEx(portraits->file, &size) || size.QuadPart <= 0) { snprintf(message, message_size, "platform: the portrait archive is empty"); return false; }
    portraits->size    = (u64)size.QuadPart;
    portraits->mapping = CreateFileMappingA(portraits->file, 0, PAGE_READONLY, 0, 0, 0);
    if (portraits->mapping) portraits->view = (const u8 *)MapViewOfFile(portraits->mapping, FILE_MAP_READ, 0, 0, 0);
    if (!portraits->view) { snprintf(message, message_size, "platform: the portrait archive does not map into memory"); return false; }
    if (!zip_open(&portraits->archive, portraits->view, portraits->size))
    {
        snprintf(message, message_size, "platform: the portrait archive is not a usable zip file: %s", portraits->archive.error);
        return false;
    }
    for (u32 member = 0; member < portraits->archive.member_count; ++member)
    {
        zip_member *entry = portraits->archive.members + member;
        portrait_list_add(&portraits->list, member, entry->name, entry->name_size);
    }
    portraits->scratch_memory = VirtualAlloc(0, (SIZE_T)WIN32_PORTRAIT_SCRATCH_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!portraits->scratch_memory) { snprintf(message, message_size, "platform: no memory for the portrait decoder"); return false; }
    arena_init(&portraits->scratch, portraits->scratch_memory, WIN32_PORTRAIT_SCRATCH_SIZE);
    snprintf(message, message_size, "platform: %u portraits (%u numbered) in %s", portraits->list.count, portraits->list.numbered_count, path);
    return portraits->list.count > 0;
}

internal void
win32_portraits_begin_frame(win32_portraits *portraits)
{
    portraits->loads_this_frame = 0;
}

/* Extracts and decodes one picture into an immutable texture. Zero on failure. */
internal ID3D11ShaderResourceView *
win32_portraits_make_texture(win32_portraits *portraits, u32 picture)
{
    arena_reset(&portraits->scratch);
    const char *error = 0;
    zip_contents file = zip_extract(&portraits->archive, portraits->list.pictures[picture].member, &portraits->scratch);
    jpeg_image image = {};
    if (!file.data) error = file.error;
    else
    {
        image = jpeg_decode_fitted(file.data, file.size, &portraits->scratch, WIN32_PORTRAIT_TARGET_WIDTH);
        if (!image.rgba) error = image.error;
    }
    ID3D11ShaderResourceView *view = 0;
    if (!error)
    {
        view = win32_make_rgba_texture(portraits->device, image.width, image.height, image.rgba);
        if (!view) error = "the D3D11 texture was not made";
    }
    if (error && portraits->log)
    {
        char message[256];
        snprintf(message, sizeof(message), "platform: portrait %s does not decode: %s", portraits->list.pictures[picture].name, error);
        portraits->log(message);
    }
    return view;
}

internal u64
win32_portraits_texture(win32_portraits *portraits, u32 picture)
{
    if (!portraits->view || !portraits->device || picture >= portraits->list.count) return 0;
    if (portraits->textures[picture]) return (u64)portraits->textures[picture];
    if (portraits->failed[picture] || portraits->loads_this_frame >= WIN32_PORTRAIT_LOADS_PER_FRAME) return 0;
    portraits->loads_this_frame += 1;
    portraits->textures[picture] = win32_portraits_make_texture(portraits, picture);
    if (!portraits->textures[picture]) portraits->failed[picture] = true;
    return (u64)portraits->textures[picture];
}

internal PLATFORM_PORTRAIT_TEXTURE(win32_portrait_texture)
{
    return win32_portraits_texture(&g_win32_portraits, picture);
}

internal void
win32_portraits_close(win32_portraits *portraits)
{
    for (u32 picture = 0; picture < PORTRAIT_LIST_CAPACITY; ++picture)
    {
        if (portraits->textures[picture]) portraits->textures[picture]->Release();
        portraits->textures[picture] = 0;
    }
    if (portraits->view)           UnmapViewOfFile(portraits->view);
    if (portraits->mapping)        CloseHandle(portraits->mapping);
    if (portraits->file)           CloseHandle(portraits->file);
    if (portraits->scratch_memory) VirtualFree(portraits->scratch_memory, 0, MEM_RELEASE);
    memset(portraits, 0, sizeof(*portraits));
}

#endif /* WIN32_PORTRAITS_H */
