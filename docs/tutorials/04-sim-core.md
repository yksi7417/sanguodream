# 04 Sim core: turns, commands, events, PRNG

Files: code/sim/sim.h, code/sim/sim.cpp. Tests: tests/unit/sim_tests.cpp
(14 checks).

## What it is for
This tutorial covers the sim core. The simulation is a pure state machine:
`sim_state` in, a `sim_command` applied, `sim_event`s out. It never touches
a file, a window or a clock. The only randomness is a seeded generator in
the state. Two sims with the same seed and the same commands are identical
forever. That determinism is what makes the spec fixtures possible, and it
will make multiplayer and replays cheap later.

## Read it in this order
1. `sim_event`: kind, turn, subject entity, delta, cause (the id of the
   command that produced it). Every change to a measurable quantity will be
   an event. The engine stores the events in the ledger and the UI shows
   them. This is how a save-file diff from the original game maps onto
   expectations.
2. `sim_event_sink`: a fixed array that the caller provides. When the sink
   is full, `sim_emit` drops events instead of an overflow.
3. `sim_command`: a kind plus an id from `sim_make_command`. Ids count up
   from 1 and are the cause that the events carry.
4. `sim_state`: PRNG state, turn, next command id, the entity manager, the
   hex map. Component tables will come here, one per mechanic.
5. `sim_apply`: the switch over command kinds. `END_TURN` calls
   `sim_advance_turn` and then stamps the cause on each event it emitted.
6. `sim_random_u32` (xorshift64*) and `sim_random_below` (unbiased range).

## Where mechanics will go
`sim_advance_turn` has a comment that names the fixed system order:
TurnScheduler, CommandResolution, Production, Movement, Combat, Diplomacy,
AIPlanning, EventTriggers, LedgerApply. Each becomes a function that takes
`sim_state*` and a sink. The order is data (an array), so the tests can run
a subset.

## The tests, line by line
- "fresh sim starts at turn 0": and the first command id is 1.
- "prng deterministic for equal seeds" / "differs for different seeds".
- "random_below stays in range" / "is roughly uniform": 7000 draws into 7
  buckets, each expected approximately 1000.
- "seed 0 is replaced": xorshift must never start at zero, or it stays zero.
- "command ids count up from 1".
- "end_turn advances the turn", "emits turn_advanced", "event carries turn
  and causing command": the whole command/event contract in three checks.
- "none command does nothing".
- "a full sink drops events instead of overflowing".

## Exercises
- Add a `SIM_COMMAND_SET_SEED` for tests and the event that it emits (or
  argue why it emits none).
- Make the system order an array of function pointers and a test that runs
  only the first two systems.
