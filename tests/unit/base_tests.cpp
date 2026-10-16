/* Unit tests for code/base.h: alignment and arenas. */

internal void
unit_base_tests(test_counts *counts)
{
    TEST_CHECK(counts, arena_align_forward(0, 16) == 0, "align 0");
    TEST_CHECK(counts, arena_align_forward(1, 16) == 16, "align 1 up to 16");
    TEST_CHECK(counts, arena_align_forward(16, 16) == 16, "align exact stays");
    TEST_CHECK(counts, arena_align_forward(17, 8) == 24, "align 17 up to 24");

    u8 buffer[512];
    memory_arena arena;
    arena_init(&arena, buffer, sizeof(buffer));
    TEST_CHECK(counts, arena.used == 0 && arena.size == sizeof(buffer), "arena_init");

    void *first  = arena_push_size(&arena, 10);
    void *second = arena_push_size(&arena, 10);
    TEST_CHECK(counts, first != 0 && second != 0, "pushes return memory");
    TEST_CHECK(counts, ((u64)second & 15) == 0, "pushes are 16-byte aligned");
    TEST_CHECK(counts, (u8 *)second >= (u8 *)first + 10, "pushes do not overlap");

    memset(first, 0xFF, 10);
    u32 *zeroed = PushArray(&arena, 4, u32);
    TEST_CHECK(counts, zeroed[0] == 0 && zeroed[3] == 0, "pushed memory is zeroed");

    memory_arena child = arena_sub(&arena, 64);
    TEST_CHECK(counts, child.size == 64 && child.used == 0, "arena_sub carves a child");
    TEST_CHECK(counts, child.base >= arena.base && child.base + 64 <= arena.base + arena.used, "child lies inside parent");

    arena_reset(&arena);
    TEST_CHECK(counts, arena.used == 0, "arena_reset");

    u32 *aligned_4 = (u32 *)arena_push_size(&arena, 4, 4);
    u32 *aligned_64 = (u32 *)arena_push_size(&arena, 4, 64);
    TEST_CHECK(counts, ((u64)aligned_4 & 3) == 0 && ((u64)aligned_64 & 63) == 0, "explicit alignment honored");
}
