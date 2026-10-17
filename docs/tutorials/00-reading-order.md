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
| 11 | Scene: the 2.5D map, the orbit camera and the pick | code/render/, code/d3d11_renderer.h, code/ui/ui_scene.cpp | tests/unit/render_tests.cpp, terrain_mesh_tests.cpp, png_tests.cpp |
| 12 | Text and locales: Big5 names, UI strings and fonts | code/text/, code/ui/ui_fonts.h, code/ui/ui_settings.cpp, data/placeholder/ | tests/unit/text_tests.cpp, tests/ui/ui_tests.cpp |
| 13 | Calendar and turn: dates, the turn order and action points | code/sim/calendar.h, code/sim/turn.cpp, data/placeholder/rules.tsv | tests/unit/calendar_tests.cpp, turn_tests.cpp, tests/feature/turn_tests.cpp |
| 14 | The develop command: facilities, slots, periods and refusals | code/sim/tables.h, content.h, develop.cpp, code/ui/ui_city.cpp, data/placeholder/facilities.tsv, cities.tsv | tests/unit/content_tests.cpp, develop_tests.cpp, tests/feature/develop_tests.cpp, tests/ui/ui_tests.cpp |
| 15 | Economy: production, durability and the five city commands | code/sim/economy.cpp, city_commands.cpp, code/ui/ui_city_commands.cpp, ui_economy.cpp, data/placeholder/rules.tsv, weapons.tsv | tests/unit/economy_tests.cpp, tests/feature/economy_tests.cpp, city_command_tests.cpp, tests/ui/ui_tests.cpp |
| 16 | Growth: research, merge, disasters and the end of the game | code/sim/research.cpp, merge.cpp, disasters.cpp, outcome.cpp, code/engine/engine_growth.cpp, code/ui/ui_growth.cpp, data/placeholder/techniques.tsv, disasters.tsv | tests/unit/growth_tests.cpp, tests/feature/growth_tests.cpp, tests/ui/ui_tests.cpp |
| 17 | March: terrain, units, the move range and the unit commands | code/sim/units.cpp, code/engine/engine_march.cpp, code/ui/ui_march.cpp, ui_map.cpp, ui_scene.cpp, data/placeholder/terrain.tsv, unit_types.tsv | tests/unit/march_tests.cpp, tests/feature/march_tests.cpp, tests/ui/ui_tests.cpp |
| 18 | Battle: attacks, tactics, plots, fire, buildings and sieges | code/sim/combat.cpp, code/sim/field.cpp, code/engine/engine_combat.cpp, code/ui/ui_combat.cpp, data/placeholder/tactics.tsv, plots.tsv, buildings.tsv, critical_skills.tsv | tests/unit/combat_tests.cpp, tests/feature/combat_tests.cpp, tests/ui/ui_tests.cpp |

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
