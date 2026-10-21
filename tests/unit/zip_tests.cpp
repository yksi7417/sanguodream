/* Unit tests for code/render/zip.h: a hand-built archive with a stored
   member, a deflate member (one stored deflate block) and a folder, plus
   the refusals. */

struct zip_test_member
{
    const char *name;
    const char *text;          /* the contents */
    u16         method;
};

internal u32
zip_test_put16(u8 *out, u32 at, u32 value)
{
    out[at] = (u8)value; out[at + 1] = (u8)(value >> 8);
    return at + 2;
}

internal u32
zip_test_put32(u8 *out, u32 at, u32 value)
{
    at = zip_test_put16(out, at, value & 0xFFFF);
    return zip_test_put16(out, at, value >> 16);
}

/* The compressed form of text: the bytes as they are, or one final stored
   deflate block. */
internal u32
zip_test_compress(u8 *out, const char *text, u16 method)
{
    u32 size = (u32)strlen(text);
    if (method == ZIP_METHOD_STORED) { memcpy(out, text, size); return size; }
    out[0] = 0x01;   /* BFINAL 1, BTYPE 00 */
    zip_test_put16(out, 1, size);
    zip_test_put16(out, 3, ~size & 0xFFFF);
    memcpy(out + 5, text, size);
    return size + 5;
}

/* Writes the local headers and data, the central directory and the end record. */
internal u32
zip_test_build(u8 *out, const zip_test_member *members, u32 count)
{
    u32 offsets[8], compressed_sizes[8];
    u32 at = 0;
    for (u32 index = 0; index < count; ++index)
    {
        const zip_test_member *member = members + index;
        u32 name_size = (u32)strlen(member->name), size = (u32)strlen(member->text);
        offsets[index] = at;
        at = zip_test_put32(out, at, ZIP_LOCAL_SIGNATURE);
        at = zip_test_put16(out, at, 20);                 /* version needed */
        at = zip_test_put16(out, at, 0);                  /* flags */
        at = zip_test_put16(out, at, member->method);
        at = zip_test_put32(out, at, 0);                  /* time and date */
        at = zip_test_put32(out, at, zip_crc32((const u8 *)member->text, size));
        u8 compressed[256];
        compressed_sizes[index] = zip_test_compress(compressed, member->text, member->method);
        at = zip_test_put32(out, at, compressed_sizes[index]);
        at = zip_test_put32(out, at, size);
        at = zip_test_put16(out, at, name_size);
        at = zip_test_put16(out, at, 0);                  /* extra */
        memcpy(out + at, member->name, name_size); at += name_size;
        memcpy(out + at, compressed, compressed_sizes[index]); at += compressed_sizes[index];
    }
    u32 directory = at;
    for (u32 index = 0; index < count; ++index)
    {
        const zip_test_member *member = members + index;
        u32 name_size = (u32)strlen(member->name), size = (u32)strlen(member->text);
        at = zip_test_put32(out, at, ZIP_CENTRAL_SIGNATURE);
        at = zip_test_put16(out, at, 20);                 /* version made by */
        at = zip_test_put16(out, at, 20);                 /* version needed */
        at = zip_test_put16(out, at, 0);                  /* flags */
        at = zip_test_put16(out, at, member->method);
        at = zip_test_put32(out, at, 0);                  /* time and date */
        at = zip_test_put32(out, at, zip_crc32((const u8 *)member->text, size));
        at = zip_test_put32(out, at, compressed_sizes[index]);
        at = zip_test_put32(out, at, size);
        at = zip_test_put16(out, at, name_size);
        at = zip_test_put16(out, at, 0);                  /* extra */
        at = zip_test_put16(out, at, 0);                  /* comment */
        at = zip_test_put16(out, at, 0);                  /* disk */
        at = zip_test_put16(out, at, 0);                  /* internal attributes */
        at = zip_test_put32(out, at, 0);                  /* external attributes */
        at = zip_test_put32(out, at, offsets[index]);
        memcpy(out + at, member->name, name_size); at += name_size;
    }
    u32 directory_size = at - directory;
    at = zip_test_put32(out, at, ZIP_END_SIGNATURE);
    at = zip_test_put32(out, at, 0);                      /* disk numbers */
    at = zip_test_put16(out, at, count);
    at = zip_test_put16(out, at, count);
    at = zip_test_put32(out, at, directory_size);
    at = zip_test_put32(out, at, directory);
    at = zip_test_put16(out, at, 0);                      /* comment */
    return at;
}

internal void
unit_zip_tests(test_counts *counts)
{
    memory_arena arena = test_arena_create(Megabytes(1));
    TEST_CHECK(counts, zip_crc32((const u8 *)"hello", 5) == 0x3610A686u, "CRC-32 of hello");

    const zip_test_member members[3] = {
        { "a.txt",     "hello",          ZIP_METHOD_STORED },
        { "dir/",      "",               ZIP_METHOD_STORED },
        { "dir/b.txt", "deflate member", ZIP_METHOD_DEFLATE },
    };
    u8 file[1024];
    u32 size = zip_test_build(file, members, 3);
    zip_archive *archive = PushStruct(&arena, zip_archive);
    TEST_CHECK(counts, zip_open(archive, file, size) && archive->error == 0, "a valid archive opens");
    TEST_CHECK(counts, archive->member_count == 3, "the directory has three members");
    TEST_CHECK(counts, archive->members[2].name_size == 9 && memcmp(archive->members[2].name, "dir/b.txt", 9) == 0, "the name of a member");

    zip_contents stored = zip_extract(archive, 0, &arena);
    TEST_CHECK(counts, stored.data && stored.size == 5 && memcmp(stored.data, "hello", 5) == 0, "a stored member extracts");
    zip_contents deflated = zip_extract(archive, 2, &arena);
    TEST_CHECK(counts, deflated.data && deflated.size == 14 && memcmp(deflated.data, "deflate member", 14) == 0, "a deflate member extracts");
    zip_contents folder = zip_extract(archive, 1, &arena);
    TEST_CHECK(counts, folder.data && folder.size == 0, "a folder extracts as no bytes");
    TEST_CHECK(counts, zip_extract(archive, 3, &arena).error != 0, "an index past the members is refused");

    /* A changed byte of the data fails the CRC-32. */
    u8 broken[1024];
    memcpy(broken, file, size);
    broken[30 + 5] ^= 1;   /* the first byte of "hello", after the local header and the name */
    zip_archive *changed = PushStruct(&arena, zip_archive);
    zip_open(changed, broken, size);
    zip_contents bad = zip_extract(changed, 0, &arena);
    TEST_CHECK(counts, bad.data == 0 && bad.error != 0, "a changed byte fails the CRC-32");

    /* refusals of the directory */
    TEST_CHECK(counts, !zip_open(changed, file, size - 1) && changed->error != 0, "a cut archive has no end record");
    TEST_CHECK(counts, !zip_open(changed, (const u8 *)"not a zip file at all, no", 25), "a text file is not an archive");
    memcpy(broken, file, size);
    broken[size - ZIP_END_RECORD_SIZE + ZIP_END_DIRECTORY_OFFSET] += 1;
    TEST_CHECK(counts, !zip_open(changed, broken, size), "a moved central directory is refused");

    /* raw deflate through inflate.h: a stored block of three bytes */
    const u8 raw[] = { 0x01, 3, 0, 0xFC, 0xFF, 'a', 'b', 'c' };
    u8 out[8];
    inflate_result inflated = raw_inflate(&arena, raw, sizeof(raw), out, sizeof(out));
    TEST_CHECK(counts, inflated.status == INFLATE_OK && inflated.bytes_written == 3 && memcmp(out, "abc", 3) == 0, "raw_inflate reads a block with no zlib header");
    test_arena_destroy(&arena);
}
