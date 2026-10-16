# 02 ECS: entity ids and component tables

File: code/ecs/ecs.h. Tests: tests/unit/ecs_tests.cpp (21 checks).

## What it is for
Game objects (officers, settlements, units) are entities: a 32-bit id. Their
data lives in component tables, one table per kind of data, each a dense
array you can loop over at full speed. Systems are plain functions that loop
over tables. There are no objects with methods.

## Read it in this order
1. `entity_id`: 20 bits of index, 12 bits of generation. The generation is
   what makes a stale id harmless: when a slot is reused, its generation goes
   up, so the old id no longer matches.
2. `entity_manager`: slots, a generation per slot, an alive flag, a free
   list. `em_create` reuses freed slots first. `em_destroy` bumps the
   generation. Slot 0 is reserved so that `ENTITY_NULL` (0) is never valid.
3. `component_table<T>`: a sparse set. `sparse[entity index]` gives the dense
   slot, `dense_owner[slot]` gives the entity back (with its generation),
   `data[slot]` is the component. The template only generates typed arrays;
   there is no inheritance anywhere.
4. `table_add` (adds or overwrites), `table_get` (null if absent or stale),
   `table_remove` (swap with last, so order is not stable), `TableEach`.

## The one subtle rule
Destroying an entity does not remove its components. Each table's owning
system removes them. Until then the old id still finds the orphaned
component, but a new entity that reuses the slot does not, because the
generation differs. The tests spell this out.

## The tests, line by line
- "entity id packs index and generation": the bit layout.
- "entities are distinct", "alive count", "em_alive": the manager basics.
- "table_get", "add overwrites": adding twice is an update, not a duplicate.
- "swap-with-last keeps others": removal moves the last element into the
  hole, and the test checks the moved element is still findable.
- "orphaned component stays until its owner removes it", "slot reused with
  new generation", "new entity in the slot does not see the orphan",
  "orphan removable by the old id": the subtle rule above, in four lines.
- "table fills to capacity" and "table_add returns null when full":
  capacity is fixed at init; running out is a return value, not an assert.
- "em_create returns null when full": same contract for entities.

## Exercises
- Write a `position` component and a system that moves every entity one hex
  east. Test it with TableEach.
- Add `table_remove_all_dead(table, entity_manager)` that sweeps orphans,
  and a test that it leaves live entities untouched.
