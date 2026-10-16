# Reading order

This document says in what order to read the tutorials. Each tutorial
covers one module: what the module is for, the files to open, and the tests
to run and read. The tests are the executable version of the tutorial.
Every check line in them is a sentence that describes the module.

Build first, then run the pyramid one time, so that the numbers below match:

```
build.bat
run_tests.bat --e2e
```

| # | Tutorial | Code | Tests |
|---|---|---|---|
| 01 | Base: types, macros, arenas | code/base.h | tests/unit/base_tests.cpp |
| 02 | ECS: entity ids and component tables | code/ecs/ecs.h | tests/unit/ecs_tests.cpp |
| 03 | Hex grid: coordinates, map, paths | code/sim/hex.h | tests/unit/hex_tests.cpp |
| 04 | Sim core: turns, commands, events, PRNG | code/sim/sim.h, sim.cpp | tests/unit/sim_tests.cpp |
| 05 | Engine and engine_api: the line | code/engine/ | tests/feature/engine_tests.cpp |
| 06 | UI: panels that only call the api | code/ui/ | tests/ui/ui_tests.cpp |
| 07 | Platform: window, memory block, hot reload | code/win32_hexdom.cpp, code/win32_window.h, code/hexdom.cpp | run_tests.bat --e2e |
| 08 | Adding a mechanic: the TDD loop end to end | tests/feature/fixture_tests.cpp | spec fixtures |
| 09 | Save loader: from a .s11 file to officers and cities | code/sim/inflate.h, save_format, scenario_format | tests/unit/inflate_tests.cpp, save_format_tests.cpp, tests/feature/load_save_tests.cpp |
| 10 | Demo recorder: one recorded walkthrough for each milestone | tests/demo/demo_player.cpp, code/win32_window.h, misc/gif_encode.cpp | tests/demo/demo_tests.cpp, run_tests.bat --demo |

You will see these conventions everywhere:
- `internal` means `static`: the function is private to its translation
  unit. The build is a unity build (one .cpp includes the others), so almost
  everything is `internal`.
- Structs plus free functions. No classes, no virtuals, no exceptions, no STL
  in code/sim and code/ecs.
- Memory comes from arenas that the game carves out of one block. Nothing in
  the game DLL calls malloc after startup.
- `b32` is a 32-bit bool. `u32` and `s32` are the unsigned and signed 32-bit
  integers.
