#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H
/* The whole test framework: a pass/fail counter, one check macro, and an
   arena from the heap. Shared by the unit, feature and ui suites. */

#include <stdio.h>
#include <stdlib.h>
#include "base.h"

struct test_counts
{
    u32 passed;
    u32 failed;
};

#define TEST_CHECK(counts, condition, name)                                          \
    do {                                                                             \
        if (condition) { (counts)->passed++; }                                       \
        else { (counts)->failed++; printf("  FAIL %s (%s:%d)\n", name, __FILE__, __LINE__); } \
    } while (0)

internal memory_arena
test_arena_create(u64 size)
{
    memory_arena arena;
    void *block = malloc((size_t)size);
    if (!block) { printf("out of memory\n"); exit(2); }
    arena_init(&arena, block, size);
    return arena;
}

internal void
test_arena_destroy(memory_arena *arena)
{
    free(arena->base);
    arena->base = 0;
    arena->size = arena->used = 0;
}

/* Prints "PASS n/n suite" or "FAIL k/n suite"; returns 1 on failure. */
internal int
test_report(const char *suite, test_counts *counts)
{
    printf("%s %u/%u %s\n", counts->failed ? "FAIL" : "PASS",
           counts->passed, counts->passed + counts->failed, suite);
    return counts->failed ? 1 : 0;
}

#endif /* TEST_FRAMEWORK_H */
