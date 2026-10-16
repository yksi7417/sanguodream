#ifndef TEST_FILES_H
#define TEST_FILES_H
/* File helpers for the test executables and the save_dump tool: read a whole
   file into an arena, and find the spec fixtures next to the build folder.
   The game DLL never uses this file; it reads files through the platform. */

#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef Yield    /* winbase.h defines an empty Yield() macro; the ImGui test context has a Yield method */
#include "base.h"
#include "hexdom.h"

struct test_file
{
    u8  *contents;   /* zero when the file could not be read */
    u64  size;
};

internal test_file
test_read_file(const char *path, memory_arena *arena)
{
    test_file result = {};
    FILE *file = fopen(path, "rb");
    if (!file) return result;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size >= 0 && (u64)size + 1 <= arena->size - arena->used)
    {
        u8 *buffer = (u8 *)arena_push_size(arena, (u64)size + 1);
        size_t read = fread(buffer, 1, (size_t)size, file);
        if (read == (size_t)size)
        {
            result.contents = buffer;
            result.size     = (u64)size;
        }
    }
    fclose(file);
    return result;
}

/* The folder of the running executable, without the last backslash. */
internal void
test_exe_dir(char *out, u32 out_size)
{
    DWORD length = GetModuleFileNameA(0, out, out_size);
    for (DWORD index = length; index > 0; --index)
    {
        if (out[index - 1] == '\\') { out[index - 1] = 0; return; }
    }
}

/* <exe dir>\..\spec-src\spec\fixtures\<relative>. The executables live in build\. */
internal void
test_fixture_path(char *out, u32 out_size, const char *relative)
{
    char exe_dir[MAX_PATH];
    test_exe_dir(exe_dir, MAX_PATH);
    snprintf(out, out_size, "%s\\..\\spec-src\\spec\\fixtures\\%s", exe_dir, relative);
}

/* The platform's file reader for engines in the tests: a relative path is
   relative to the folder of the executable, as in win32_hexdom.cpp. */
internal PLATFORM_READ_ENTIRE_FILE(test_platform_read_entire_file)
{
    char resolved[MAX_PATH];
    b32 is_absolute = path[0] == '\\' || path[0] == '/' || (path[0] && path[1] == ':');
    if (is_absolute)
    {
        snprintf(resolved, MAX_PATH, "%s", path);
    }
    else
    {
        char exe_dir[MAX_PATH];
        test_exe_dir(exe_dir, MAX_PATH);
        snprintf(resolved, MAX_PATH, "%s\\%s", exe_dir, path);
    }
    test_file file = test_read_file(resolved, arena);
    read_file_result result;
    result.size     = file.size;
    result.contents = file.contents;
    return result;
}

#endif /* TEST_FILES_H */
