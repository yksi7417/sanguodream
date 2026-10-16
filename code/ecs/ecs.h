#ifndef ECS_H
#define ECS_H
/* Sparse-set entity component storage, fixed capacity, carved from an arena
   at startup. No allocation after init, no virtuals. Templates here only
   generate typed arrays. */

#include "base.h"

typedef u32 entity_id;   /* index:20 | generation:12. Zero is the null entity. */

#define ENTITY_INDEX_BITS  20
#define ENTITY_INDEX_MASK  ((1u << ENTITY_INDEX_BITS) - 1)
#define ENTITY_NULL        0u

internal u32       entity_index(entity_id e)      { return e & ENTITY_INDEX_MASK; }
internal u32       entity_generation(entity_id e) { return e >> ENTITY_INDEX_BITS; }
internal entity_id entity_make(u32 index, u32 generation)
{
    Assert(index <= ENTITY_INDEX_MASK);
    return (generation << ENTITY_INDEX_BITS) | index;
}

struct entity_manager
{
    u32  capacity;      /* total slots, slot 0 reserved */
    u32  alive_count;
    u32  next_unused;   /* first slot never handed out */
    u16 *generation;    /* per slot */
    u8  *alive;         /* per slot */
    u32 *free_list;     /* recycled slots */
    u32  free_count;
};

template<typename T>
struct component_table
{
    u32        count;
    u32        capacity;
    u32       *sparse;        /* entity index -> dense slot, or U32_MAX */
    entity_id *dense_owner;   /* dense slot  -> entity (with generation) */
    T         *data;          /* dense slot  -> component */
};

/* ---- entity manager ----------------------------------------------------- */

internal void
em_init(entity_manager *em, memory_arena *arena, u32 capacity)
{
    Assert(capacity >= 2 && capacity <= ENTITY_INDEX_MASK);
    em->capacity    = capacity;
    em->alive_count = 0;
    em->next_unused = 1;
    em->generation  = PushArray(arena, capacity, u16);
    em->alive       = PushArray(arena, capacity, u8);
    em->free_list   = PushArray(arena, capacity, u32);
    em->free_count  = 0;
}

internal entity_id
em_create(entity_manager *em)
{
    u32 index;
    if (em->free_count)
    {
        index = em->free_list[--em->free_count];
    }
    else
    {
        if (em->next_unused >= em->capacity) return ENTITY_NULL;   /* full: caller decides */
        index = em->next_unused++;
    }
    em->alive[index] = 1;
    em->alive_count++;
    return entity_make(index, em->generation[index]);
}

internal b32
em_alive(entity_manager *em, entity_id e)
{
    u32 index = entity_index(e);
    return e != ENTITY_NULL && index < em->capacity && em->alive[index] &&
           em->generation[index] == entity_generation(e);
}

/* Destroying bumps the generation so a later entity that reuses the slot never
   matches the old id. Components are not removed here; the owning system
   removes them. Until it does, the old id still finds its orphaned component,
   but the new entity in that slot does not. */
internal void
em_destroy(entity_manager *em, entity_id e)
{
    if (!em_alive(em, e)) return;
    u32 index = entity_index(e);
    em->alive[index] = 0;
    em->generation[index] = (u16)((em->generation[index] + 1) & 0xFFF);
    em->free_list[em->free_count++] = index;
    em->alive_count--;
}

/* ---- component tables --------------------------------------------------- */

template<typename T> internal void
table_init(component_table<T> *t, memory_arena *arena, u32 entity_capacity, u32 component_capacity)
{
    t->count       = 0;
    t->capacity    = component_capacity;
    t->sparse      = PushArray(arena, entity_capacity, u32);
    t->dense_owner = PushArray(arena, component_capacity, entity_id);
    t->data        = PushArray(arena, component_capacity, T);
    for (u32 i = 0; i < entity_capacity; ++i) t->sparse[i] = U32_MAX;
}

template<typename T> internal u32
table_slot(component_table<T> *t, entity_id e)
{
    u32 slot = t->sparse[entity_index(e)];
    if (slot != U32_MAX && t->dense_owner[slot] == e) return slot;
    return U32_MAX;
}

template<typename T> internal T *
table_get(component_table<T> *t, entity_id e)
{
    u32 slot = table_slot(t, e);
    return slot == U32_MAX ? 0 : t->data + slot;
}

template<typename T> internal b32
table_has(component_table<T> *t, entity_id e)
{
    return table_slot(t, e) != U32_MAX;
}

/* Adds or overwrites. Returns the stored component, or null when full. */
template<typename T> internal T *
table_add(component_table<T> *t, entity_id e, T value)
{
    u32 slot = table_slot(t, e);
    if (slot == U32_MAX)
    {
        if (t->count >= t->capacity) return 0;   /* full: caller decides */
        slot = t->count++;
        t->sparse[entity_index(e)] = slot;
        t->dense_owner[slot] = e;
    }
    t->data[slot] = value;
    return t->data + slot;
}

/* Swap-with-last removal, O(1). Dense order is not stable. */
template<typename T> internal b32
table_remove(component_table<T> *t, entity_id e)
{
    u32 slot = table_slot(t, e);
    if (slot == U32_MAX) return false;
    u32 last = t->count - 1;
    if (slot != last)
    {
        entity_id mover = t->dense_owner[last];
        t->data[slot]        = t->data[last];
        t->dense_owner[slot] = mover;
        t->sparse[entity_index(mover)] = slot;
    }
    t->sparse[entity_index(e)] = U32_MAX;
    t->count = last;
    return true;
}

/* Tight loop over the dense arrays: TableEach(&table, i) { use table.data[i], table.dense_owner[i] } */
#define TableEach(table, i) for (u32 i = 0; i < (table)->count; ++i)

#endif /* ECS_H */
