/* Unit tests for code/ecs: entity ids, the entity manager, component tables. */

struct test_position
{
    s32 q;
    s32 r;
};

internal void
unit_ecs_tests(test_counts *counts)
{
    memory_arena arena = test_arena_create(Megabytes(1));

    entity_id packed = entity_make(12345, 7);
    TEST_CHECK(counts, entity_index(packed) == 12345 && entity_generation(packed) == 7, "entity id packs index and generation");

    entity_manager em;
    em_init(&em, &arena, 64);
    component_table<test_position> positions;
    table_init(&positions, &arena, 64, 8);

    entity_id a = em_create(&em);
    entity_id b = em_create(&em);
    entity_id c = em_create(&em);
    TEST_CHECK(counts, a != ENTITY_NULL && a != b && b != c, "entities are distinct");
    TEST_CHECK(counts, em.alive_count == 3, "alive count");
    TEST_CHECK(counts, em_alive(&em, a) && !em_alive(&em, ENTITY_NULL), "em_alive");

    test_position pa = { 1, 2 }, pb = { 3, 4 }, pc = { 5, 6 };
    table_add(&positions, a, pa);
    table_add(&positions, b, pb);
    table_add(&positions, c, pc);
    TEST_CHECK(counts, positions.count == 3, "table count after add");
    TEST_CHECK(counts, table_has(&positions, b) && table_get(&positions, b)->r == 4, "table_get");

    test_position pa2 = { 7, 8 };
    table_add(&positions, a, pa2);
    TEST_CHECK(counts, positions.count == 3 && table_get(&positions, a)->q == 7, "add overwrites");

    TEST_CHECK(counts, table_remove(&positions, a), "remove returns true");
    TEST_CHECK(counts, positions.count == 2 && table_get(&positions, a) == 0, "removed entity has no component");
    TEST_CHECK(counts, table_get(&positions, c)->q == 5 && table_get(&positions, b)->q == 3, "swap-with-last keeps others");
    TEST_CHECK(counts, !table_remove(&positions, a), "second remove returns false");

    em_destroy(&em, b);
    TEST_CHECK(counts, !em_alive(&em, b), "destroyed entity is dead");
    TEST_CHECK(counts, table_get(&positions, b) != 0, "orphaned component stays until its owner removes it");
    entity_id d = em_create(&em);
    TEST_CHECK(counts, entity_index(d) == entity_index(b) && d != b, "slot reused with new generation");
    TEST_CHECK(counts, table_get(&positions, d) == 0, "new entity in the slot does not see the orphan");
    TEST_CHECK(counts, table_remove(&positions, b), "orphan removable by the old id");
    TEST_CHECK(counts, positions.count == 1 && table_get(&positions, c)->q == 5, "only c remains");

    u32 iterated = 0;
    TableEach(&positions, i) { iterated++; }
    TEST_CHECK(counts, iterated == positions.count, "TableEach visits dense range");

    u32 added = 0;
    for (u32 i = 0; i < 16; ++i)
    {
        entity_id e = em_create(&em);
        test_position p = { (s32)i, 0 };
        if (table_add(&positions, e, p)) added++;
    }
    TEST_CHECK(counts, positions.count == positions.capacity, "table fills to capacity");
    TEST_CHECK(counts, added == positions.capacity - 1, "table_add returns null when full");

    entity_manager tiny;
    em_init(&tiny, &arena, 3);
    entity_id t1 = em_create(&tiny), t2 = em_create(&tiny), t3 = em_create(&tiny);
    TEST_CHECK(counts, t1 && t2 && t3 == ENTITY_NULL, "em_create returns null when full");

    test_arena_destroy(&arena);
}
