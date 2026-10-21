#ifndef ZIP_H
#define ZIP_H
/* A small reader of zip archives over the engine's own inflate. It reads
   the central directory of an archive in memory and extracts one member.
   The members can be stored or deflate. The platform layer uses it for the
   default portraits (docs/ASSETS.md). The layout comes from the public
   PKWARE APPNOTE. This reader does not read zip64 archives or encrypted
   members. */

#include "base.h"
#include "sim/inflate.h"

#define ZIP_MEMBER_CAPACITY     512
#define ZIP_MAX_COMMENT_SIZE    0xFFFFu
#define ZIP_SIZE_IN_ZIP64       0xFFFFFFFFu   /* a 32-bit field with this value moves to a zip64 record */

#define ZIP_END_SIGNATURE       0x06054b50u
#define ZIP_CENTRAL_SIGNATURE   0x02014b50u
#define ZIP_LOCAL_SIGNATURE     0x04034b50u

#define ZIP_METHOD_STORED       0
#define ZIP_METHOD_DEFLATE      8
#define ZIP_FLAG_ENCRYPTED      0x0001u

/* The end of central directory record. */
enum zip_end_field
{
    ZIP_END_MEMBER_COUNT     = 10,   /* u16, the members in the whole archive */
    ZIP_END_DIRECTORY_SIZE   = 12,   /* u32 */
    ZIP_END_DIRECTORY_OFFSET = 16,   /* u32 */
    ZIP_END_COMMENT_SIZE     = 20,   /* u16 */
    ZIP_END_RECORD_SIZE      = 22
};

/* One header of the central directory. The name follows the fixed part. */
enum zip_central_field
{
    ZIP_CENTRAL_FLAGS           = 8,    /* u16 */
    ZIP_CENTRAL_METHOD          = 10,   /* u16 */
    ZIP_CENTRAL_CRC32           = 16,   /* u32 */
    ZIP_CENTRAL_COMPRESSED_SIZE = 20,   /* u32 */
    ZIP_CENTRAL_SIZE            = 24,   /* u32 */
    ZIP_CENTRAL_NAME_SIZE       = 28,   /* u16 */
    ZIP_CENTRAL_EXTRA_SIZE      = 30,   /* u16 */
    ZIP_CENTRAL_COMMENT_SIZE    = 32,   /* u16 */
    ZIP_CENTRAL_LOCAL_OFFSET    = 42,   /* u32 */
    ZIP_CENTRAL_HEADER_SIZE     = 46
};

/* The local header in front of the data of each member. */
enum zip_local_field
{
    ZIP_LOCAL_NAME_SIZE   = 26,   /* u16 */
    ZIP_LOCAL_EXTRA_SIZE  = 28,   /* u16 */
    ZIP_LOCAL_HEADER_SIZE = 30
};

struct zip_member
{
    const char *name;          /* into the archive, not zero-terminated; UTF-8 */
    u32         name_size;
    u16         method;
    u16         flags;
    u32         crc32;
    u32         compressed_size;
    u32         size;
    u32         local_offset;
};

struct zip_archive
{
    const u8   *data;
    u64         size;
    zip_member  members[ZIP_MEMBER_CAPACITY];
    u32         member_count;
    const char *error;         /* zero when the directory is good */
};

struct zip_contents
{
    u8         *data;          /* in the arena; zero on failure */
    u64         size;
    const char *error;
};

internal u16
zip_read_u16(const u8 *p)
{
    return (u16)(p[0] | (p[1] << 8));
}

internal u32
zip_read_u32(const u8 *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

/* The CRC-32 of the zip format (polynomial 0xEDB88320, reflected). */
internal u32
zip_crc32(const u8 *data, u64 size)
{
    u32 table[256];
    for (u32 entry = 0; entry < 256; ++entry)
    {
        u32 value = entry;
        for (u32 bit = 0; bit < 8; ++bit) value = (value & 1) ? (0xEDB88320u ^ (value >> 1)) : (value >> 1);
        table[entry] = value;
    }
    u32 crc = 0xFFFFFFFFu;
    for (u64 at = 0; at < size; ++at) crc = table[(crc ^ data[at]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

/* The offset of the end record: the last signature with a comment size
   that ends at the end of the data. False if there is no such record. */
internal b32
zip_find_end_record(const u8 *data, u64 size, u64 *offset)
{
    if (size < ZIP_END_RECORD_SIZE) return false;
    u64 lowest = size > ZIP_END_RECORD_SIZE + ZIP_MAX_COMMENT_SIZE ? size - ZIP_END_RECORD_SIZE - ZIP_MAX_COMMENT_SIZE : 0;
    for (u64 at = size - ZIP_END_RECORD_SIZE + 1; at-- > lowest;)
    {
        if (zip_read_u32(data + at) != ZIP_END_SIGNATURE) continue;
        if (at + ZIP_END_RECORD_SIZE + zip_read_u16(data + at + ZIP_END_COMMENT_SIZE) != size) continue;
        *offset = at;
        return true;
    }
    return false;
}

/* Reads the central directory of the archive in data. The archive keeps
   pointers into data, so the caller keeps data alive. The result is false,
   with an error, if the directory is bad or has more members than
   ZIP_MEMBER_CAPACITY. */
internal b32
zip_open(zip_archive *archive, const u8 *data, u64 size)
{
    archive->data         = data;
    archive->size         = size;
    archive->member_count = 0;
    archive->error        = 0;

    u64 end = 0;
    if (!zip_find_end_record(data, size, &end)) { archive->error = "no end of central directory record"; return false; }
    u32 count     = zip_read_u16(data + end + ZIP_END_MEMBER_COUNT);
    u64 directory = zip_read_u32(data + end + ZIP_END_DIRECTORY_OFFSET);
    u64 extent    = zip_read_u32(data + end + ZIP_END_DIRECTORY_SIZE);
    if (directory + extent > end) { archive->error = "the central directory is outside the archive"; return false; }
    if (count > ZIP_MEMBER_CAPACITY) { archive->error = "too many members"; return false; }

    u64 at = directory;
    for (u32 index = 0; index < count; ++index)
    {
        if (at + ZIP_CENTRAL_HEADER_SIZE > directory + extent || zip_read_u32(data + at) != ZIP_CENTRAL_SIGNATURE)
        {
            archive->error = "bad central directory header";
            return false;
        }
        const u8 *header = data + at;
        zip_member *member = archive->members + index;
        member->name_size       = zip_read_u16(header + ZIP_CENTRAL_NAME_SIZE);
        member->name            = (const char *)header + ZIP_CENTRAL_HEADER_SIZE;
        member->method          = zip_read_u16(header + ZIP_CENTRAL_METHOD);
        member->flags           = zip_read_u16(header + ZIP_CENTRAL_FLAGS);
        member->crc32           = zip_read_u32(header + ZIP_CENTRAL_CRC32);
        member->compressed_size = zip_read_u32(header + ZIP_CENTRAL_COMPRESSED_SIZE);
        member->size            = zip_read_u32(header + ZIP_CENTRAL_SIZE);
        member->local_offset    = zip_read_u32(header + ZIP_CENTRAL_LOCAL_OFFSET);
        at += ZIP_CENTRAL_HEADER_SIZE + member->name_size + zip_read_u16(header + ZIP_CENTRAL_EXTRA_SIZE) +
              zip_read_u16(header + ZIP_CENTRAL_COMMENT_SIZE);
        if (at > directory + extent) { archive->error = "a member name goes past the central directory"; return false; }
        if (member->compressed_size == ZIP_SIZE_IN_ZIP64 || member->size == ZIP_SIZE_IN_ZIP64 ||
            member->local_offset == ZIP_SIZE_IN_ZIP64)
        {
            archive->error = "zip64 members are not read";
            return false;
        }
    }
    archive->member_count = count;
    return true;
}

/* Extracts one member into the arena and checks its CRC-32. The arena also
   holds the inflate state. */
internal zip_contents
zip_extract(zip_archive *archive, u32 index, memory_arena *arena)
{
    zip_contents result = {};
    if (index >= archive->member_count) { result.error = "no such member"; return result; }
    zip_member *member = archive->members + index;
    if (member->flags & ZIP_FLAG_ENCRYPTED) { result.error = "the member is encrypted"; return result; }

    u64 local = member->local_offset;
    if (local + ZIP_LOCAL_HEADER_SIZE > archive->size || zip_read_u32(archive->data + local) != ZIP_LOCAL_SIGNATURE)
    {
        result.error = "bad local header";
        return result;
    }
    const u8 *header = archive->data + local;
    u64 start = local + ZIP_LOCAL_HEADER_SIZE + zip_read_u16(header + ZIP_LOCAL_NAME_SIZE) + zip_read_u16(header + ZIP_LOCAL_EXTRA_SIZE);
    if (start + member->compressed_size > archive->size) { result.error = "the member data goes past the archive"; return result; }
    if ((u64)member->size + sizeof(inflate_state) + 64 > arena->size - arena->used)
    {
        result.error = "the member does not fit in the arena";
        return result;
    }

    u8 *out = (u8 *)arena_push_size(arena, member->size ? member->size : 1);
    const u8 *source = archive->data + start;
    if (member->method == ZIP_METHOD_STORED)
    {
        if (member->compressed_size != member->size) { result.error = "a stored member with two sizes"; return result; }
        memcpy(out, source, member->size);
    }
    else if (member->method == ZIP_METHOD_DEFLATE)
    {
        inflate_result inflated = raw_inflate(arena, source, member->compressed_size, out, member->size);
        if (inflated.status != INFLATE_OK) { result.error = inflate_status_name(inflated.status); return result; }
        if (inflated.bytes_written != member->size) { result.error = "the member inflates to a wrong size"; return result; }
    }
    else
    {
        result.error = "the compression method is not stored or deflate";
        return result;
    }
    if (zip_crc32(out, member->size) != member->crc32) { result.error = "CRC-32 mismatch"; return result; }
    result.data = out;
    result.size = member->size;
    return result;
}

#endif /* ZIP_H */
