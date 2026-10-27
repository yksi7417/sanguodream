#ifndef TEST_FILES_H
#define TEST_FILES_H
/* File helpers for the test executables and the save_dump tool: read a whole
   file into an arena, and find the spec fixtures next to the build folder.
   The game DLL never uses this file; it reads files through the platform.

   The helpers build on Windows and on POSIX. The web build of the tests
   runs under node with the real file system (docs/WEB.md). There, the
   folder of the executable is the current folder, so run the tests from
   build/. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef Yield    /* winbase.h defines an empty Yield() macro; the ImGui test context has a Yield method */
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#endif
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

/* The folder of the running executable, without the last separator. On
   POSIX it is the current folder. */
internal void
test_exe_dir(char *out, u32 out_size)
{
#ifdef _WIN32
    DWORD length = GetModuleFileNameA(0, out, out_size);
    for (DWORD index = length; index > 0; --index)
    {
        if (out[index - 1] == '\\') { out[index - 1] = 0; return; }
    }
#else
    if (!getcwd(out, out_size)) snprintf(out, out_size, ".");
#endif
}

/* Makes one folder. Nothing happens if the folder exists. */
internal void
test_make_folder(const char *path)
{
#ifdef _WIN32
    CreateDirectoryA(path, 0);
#else
    mkdir(path, 0777);
#endif
}

/* Copies a whole file. False if the copy failed. */
internal b32
test_copy_file(const char *source, const char *target)
{
#ifdef _WIN32
    return CopyFileA(source, target, FALSE) != 0;
#else
    FILE *from = fopen(source, "rb");
    if (!from) return false;
    FILE *to = fopen(target, "wb");
    if (!to) { fclose(from); return false; }
    char buffer[4096];
    b32 ok = true;
    for (size_t read; (read = fread(buffer, 1, sizeof(buffer), from)) > 0;)
    {
        if (fwrite(buffer, 1, read, to) != read) { ok = false; break; }
    }
    fclose(from);
    fclose(to);
    return ok;
#endif
}

/* One entry of a folder. */
struct test_folder_entry
{
    char name[MAX_PATH];
    b32  is_folder;
};

internal int
test_folder_entry_compare(const void *a, const void *b)
{
    return strcmp(((const test_folder_entry *)a)->name, ((const test_folder_entry *)b)->name);
}

/* Lists the entries of a folder, without "." and "..", sorted by name, so
   that the order is the same on each platform. Returns the count. */
internal u32
test_list_folder(const char *folder, test_folder_entry *out, u32 capacity)
{
    u32 count = 0;
#ifdef _WIN32
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s/*", folder);
    WIN32_FIND_DATAA found;
    HANDLE search = FindFirstFileA(pattern, &found);
    if (search == INVALID_HANDLE_VALUE) return 0;
    do
    {
        if (found.cFileName[0] == '.' || count >= capacity) continue;
        snprintf(out[count].name, sizeof(out[count].name), "%s", found.cFileName);
        out[count].is_folder = (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        count++;
    } while (FindNextFileA(search, &found));
    FindClose(search);
#else
    DIR *search = opendir(folder);
    if (!search) return 0;
    for (struct dirent *found; (found = readdir(search)) != 0;)
    {
        if (found->d_name[0] == '.' || count >= capacity) continue;
        snprintf(out[count].name, sizeof(out[count].name), "%s", found->d_name);
        char path[MAX_PATH * 2];
        snprintf(path, sizeof(path), "%s/%s", folder, found->d_name);
        struct stat info;
        out[count].is_folder = stat(path, &info) == 0 && S_ISDIR(info.st_mode);
        count++;
    }
    closedir(search);
#endif
    qsort(out, count, sizeof(test_folder_entry), test_folder_entry_compare);
    return count;
}

/* <exe dir>/../spec-src/spec/fixtures/<relative>. The executables live in build/. */
internal void
test_fixture_path(char *out, u32 out_size, const char *relative)
{
    char exe_dir[MAX_PATH];
    test_exe_dir(exe_dir, MAX_PATH);
    snprintf(out, out_size, "%s/../spec-src/spec/fixtures/%s", exe_dir, relative);
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
        snprintf(resolved, MAX_PATH, "%s/%s", exe_dir, path);
    }
    test_file file = test_read_file(resolved, arena);
    read_file_result result;
    result.size     = file.size;
    result.contents = file.contents;
    return result;
}

/* As the reader: a relative path is relative to the folder of the test
   executable. The folder of the file is made if it does not exist. */
internal PLATFORM_WRITE_ENTIRE_FILE(test_platform_write_entire_file)
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
        snprintf(resolved, MAX_PATH, "%s/%s", exe_dir, path);
    }
    char folder[MAX_PATH];
    snprintf(folder, MAX_PATH, "%s", resolved);
    char *last = 0;   /* the game writes '/' and the exe folder has '\' */
    for (char *at = folder; *at; ++at) if (*at == '\\' || *at == '/') last = at;
    if (last) { *last = 0; test_make_folder(folder); }
    FILE *file = fopen(resolved, "wb");
    if (!file) return false;
    b32 ok = fwrite(data, 1, (size_t)size, file) == (size_t)size;
    fclose(file);
    return ok;
}

/* M16: makes <exe dir>/<name>, a small install folder for the tests of the
   install of the player. It copies the two scenario files of
   spec-src/spec/fixtures/scenarios into media/scenario, the folder of
   formats/binary-identity.md. It also writes a short file
   media/msg/S11MSG00.s11. The engine only finds that file. The spec has no
   layout of the message files. Writes the absolute path of the folder to
   out. Returns false if a copy failed. */
internal b32
test_make_install_folder(const char *name, char *out, u32 out_size)
{
    char exe_dir[MAX_PATH];
    test_exe_dir(exe_dir, MAX_PATH);
    snprintf(out, out_size, "%s/%s", exe_dir, name);

    char folder[MAX_PATH];
    test_make_folder(out);
    snprintf(folder, MAX_PATH, "%s/media", out);
    test_make_folder(folder);
    snprintf(folder, MAX_PATH, "%s/media/scenario", out);
    test_make_folder(folder);
    snprintf(folder, MAX_PATH, "%s/media/msg", out);
    test_make_folder(folder);

    static const char *files[] = { "Scenario.s11", "SCEN002.s11" };
    for (u32 index = 0; index < ArrayCount(files); ++index)
    {
        char source[MAX_PATH], target[MAX_PATH], relative[64];
        snprintf(relative, sizeof(relative), "scenarios/%s", files[index]);
        test_fixture_path(source, MAX_PATH, relative);
        snprintf(target, MAX_PATH, "%s/media/scenario/%s", out, files[index]);
        if (!test_copy_file(source, target)) return false;
    }

    char message[MAX_PATH];
    snprintf(message, MAX_PATH, "%s/media/msg/S11MSG00.s11", out);
    FILE *file = fopen(message, "wb");
    if (!file) return false;
    fputs("not a message table", file);
    fclose(file);
    return true;
}

#endif /* TEST_FILES_H */
