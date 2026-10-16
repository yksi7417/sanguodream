# 01 Base: types, macros, arenas

File: code/base.h. Tests: tests/unit/base_tests.cpp (13 checks).

## What it is for
Everything else needs fixed-width integer types, a handful of macros and a
way to get memory without a heap. base.h is that, and nothing more. It has
no dependencies beyond three C headers.

## Read it in this order
1. The typedefs: `u8..u64`, `s8..s64`, `f32`, `f64`, `b32`. Code uses these
   names, never `int` or `unsigned`, so sizes are visible at every use.
2. `internal`, `global_variable`, `local_persist`: three spellings of
   `static` that say which of its three meanings is meant.
3. `Assert`: on in HEXDOM_SLOW builds (build.bat sets it), compiles to
   nothing otherwise. It breaks into the debugger instead of printing.
4. `memory_arena`: a base pointer, a size and a cursor. `arena_push_size`
   aligns the cursor (16 bytes by default), checks the bound, zeroes the
   memory and hands it out. There is no free; `arena_reset` rewinds the
   whole arena. `arena_sub` carves a child arena out of a parent.
5. `PushStruct` and `PushArray`: the two macros everybody uses.

## Why arenas
The platform layer allocates one block at startup (see tutorial 07) and the
game carves it into a permanent arena (lives for the session), a transient
arena (scratch that survives a frame) and a frame arena (reset every frame).
Allocation is a pointer bump, freeing is a reset, and because the DLL owns no
heap memory, the DLL can be unloaded and reloaded while the game keeps
running.

## The tests, line by line
Open tests/unit/base_tests.cpp beside base.h:
- "align ..." checks `arena_align_forward` on the edge cases 0, 1, exact
  and non-power-of-two alignments.
- "pushes are 16-byte aligned" and "pushes do not overlap" are the two
  properties every later module relies on.
- "pushed memory is zeroed": the test dirties the first allocation, then
  checks a later one is clean. Zeroed memory is why structs can be used
  without constructors.
- "arena_sub carves a child" and "child lies inside parent" pin the
  sub-arena contract the engine uses for its scratch space.
- "explicit alignment honored": the optional alignment argument.

## Exercises
- Add a check that pushing more than the arena holds asserts in a slow build
  (hint: you cannot catch an Assert in a test, so decide what the right
  contract is first; today it is "caller's bug").
- Add `arena_push_copy` for a string and a test for it. Keep it in base.h.
