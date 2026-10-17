# 05 Engine and engine_api: the line

Files: code/engine/engine.h, code/engine/engine.cpp. Tests:
tests/feature/engine_tests.cpp (24 checks).

## What it is for
This tutorial covers the engine and its api. `engine_api` is the only way a
front end touches the game. It is a struct of plain function pointers over
an opaque `engine`. The UI holds one and calls `api->end_turn(api->engine)`;
it never sees `sim_state`. Tests hold one too: feature tests over the real
engine, UI tests over a double that records calls. The api is a table of C
function pointers, not methods. That is what makes the double trivial and
makes hot reload safe (the game rebuilds the table after a reload).

## Read it in this order
1. engine.h: the typedef per function, then `engine_api`. Queries come
   first, commands last. The comment at the top is the design rule: a new
   mechanic grows this table and nothing else in the UI contract.
2. `engine_config` and `engine_default_config`: seed, ledger capacity, and
   the size of the placeholder map. With `use_pack_map` (M10) the engine
   loads map\hexes.tsv of the pack and takes its size (tutorial 22).
3. engine.cpp: `engine` owns the sim, a ledger, and a scratch arena.
   `engine_run_command` is the one path for commands into the sim: it
   resets scratch, makes a sink, applies, appends to the ledger.
4. Each `engine_api_*` function is a few lines. `path_cost` shows the
   pattern for a function that needs work memory. Reset scratch, call the
   sim module, return a value, and never return a pointer into scratch.
5. `engine_make_api` fills the table. If you add a function and forget this
   line, the "api table is complete" feature test fails.

## The tests, line by line
tests/feature/engine_tests.cpp drives the real engine only through the api:
- "api table is complete": every pointer non-null.
- Map: size from config, default costs, off-map impassable, straight path
  cost equals distance. A wall built with `set_cell_cost` leaves no path, a
  gate reopens it, "map edits do not touch turn or ledger".
- Turns and ledger: "end_turn advances to turn 1", "ledgers one event",
  "ledgered event names its cause", "advance_turns(12) reaches turn 13",
  "reports and ledgers every event".
- Capacity: "ledger stops at its capacity" while "turns keep advancing".
- "ledger_clear empties the ledger only".
- "memory stats are sane".
- "command ids keep increasing".
- "same seed and commands give the same ledger": determinism, observed
  from outside.

Note `engine_fixture_init`: the engine keeps pointers to the arenas of the
fixture. The test therefore initialises the fixture in place and never
returns it by value. That bug cost one afternoon, and the comment is there
so that it costs no more.

## Exercises
- Add `api->seed()` as a query and a feature test that two engines built
  from `engine_default_config` report the same seed.
- Add a command that fails (for example `set_cell_cost` off the map). Decide
  whether a failure is an event in the ledger. Write the test that pins
  your decision.
