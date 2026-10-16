#ifndef BASE_H
#define BASE_H
/* Base types, macros and memory arenas. Shared by every translation unit.
   Nothing here depends on Windows, the CRT beyond <stdint.h>/<string.h>, or ImGui. */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef s32      b32;
typedef float    f32;
typedef double   f64;

#define internal        static
#define global_variable static
#define local_persist   static

#define U32_MAX 0xFFFFFFFFu
#define U64_MAX 0xFFFFFFFFFFFFFFFFull

#define ArrayCount(a) (sizeof(a) / sizeof((a)[0]))
#define Kilobytes(n) ((u64)(n) * 1024ull)
#define Megabytes(n) (Kilobytes(n) * 1024ull)
#define Gigabytes(n) (Megabytes(n) * 1024ull)
#define Terabytes(n) (Gigabytes(n) * 1024ull)

#define Minimum(a, b) ((a) < (b) ? (a) : (b))
#define Maximum(a, b) ((a) > (b) ? (a) : (b))

#if HEXDOM_SLOW
#  if defined(_MSC_VER)
#    define Assert(x) do { if (!(x)) { __debugbreak(); } } while (0)
#  else
#    define Assert(x) do { if (!(x)) { *(volatile int *)0 = 0; } } while (0)
#  endif
#else
#  define Assert(x)
#endif
#define InvalidCodePath Assert(!"InvalidCodePath")

/* ---- Memory arenas ------------------------------------------------------ */

struct memory_arena
{
    u8  *base;
    u64  size;
    u64  used;
};

internal void
arena_init(memory_arena *arena, void *base, u64 size)
{
    arena->base = (u8 *)base;
    arena->size = size;
    arena->used = 0;
}

internal u64
arena_align_forward(u64 value, u64 alignment)
{
    u64 mask = alignment - 1;
    return (value + mask) & ~mask;
}

internal void *
arena_push_size(memory_arena *arena, u64 size, u64 alignment = 16)
{
    u64 start = arena_align_forward((u64)(arena->base + arena->used), alignment) - (u64)arena->base;
    Assert(start + size <= arena->size);
    void *result = arena->base + start;
    arena->used = start + size;
    memset(result, 0, (size_t)size);
    return result;
}

#define PushStruct(arena, type)       (type *)arena_push_size((arena), sizeof(type))
#define PushArray(arena, count, type) (type *)arena_push_size((arena), (u64)(count) * sizeof(type))

internal void
arena_reset(memory_arena *arena)
{
    arena->used = 0;
}

/* Carve a child arena out of a parent. The child's memory is permanently
   reserved in the parent. */
internal memory_arena
arena_sub(memory_arena *parent, u64 size)
{
    memory_arena result;
    arena_init(&result, arena_push_size(parent, size), size);
    return result;
}

#endif /* BASE_H */
