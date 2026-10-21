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
| 19 | Court: officers, posts, personnel, diplomacy, duel and debate | code/sim/officers.cpp, diplomacy.cpp, duel.cpp, debate.cpp, code/engine/engine_court.cpp, code/ui/ui_court.cpp, data/placeholder/skills.tsv, posts.tsv, ranks.tsv, items.tsv | tests/unit/court_tests.cpp, tests/feature/court_tests.cpp, tests/ui/ui_tests.cpp |
| 20 | Rivals: the AI, corps, council, the Han emperor, events and bases | code/sim/ai.cpp, rivals.cpp, events.cpp, bases.cpp, code/engine/engine_rivals.cpp, code/ui/ui_rivals.cpp, data/placeholder/bases.tsv, events.tsv, ai_priorities.tsv | tests/unit/rivals_tests.cpp, tests/feature/rivals_tests.cpp, tests/ui/ui_tests.cpp |
| 21 | Playable: save and load, the start, new officers, PK training, history and the game screens | code/engine/engine_saves.cpp, engine_playable.cpp, code/sim/pk.cpp, history.cpp, code/ui/ui_game.cpp, data/placeholder/ability_chart.tsv, ancient_officers.tsv, history.tsv, portraits.tsv, scenarios.tsv, battles.tsv, data/civil_war/ | tests/unit/playable_tests.cpp, tests/feature/playable_tests.cpp, tests/ui/ui_tests.cpp |
| 22 | Map: the real continent, its terrain rules and its sites | code/sim/map.cpp, code/sim/hex.h, code/engine/engine_map.cpp, code/ui/ui_map.cpp, misc/make_map_pack.py, data/placeholder/map/, terrain.tsv, movement_costs.tsv | tests/unit/map_tests.cpp, tests/feature/map_tests.cpp, tests/feature/fixture_tests.cpp, tests/ui/ui_tests.cpp |
| 23 | Save map: the state of a save on the real map, the frame of a city or a unit, the hex info and the keys | code/sim/save_map.cpp, save_format, code/render/render.h, code/ui/ui_locate.cpp, ui_scene.cpp, data/placeholder/save_types.tsv | tests/unit/save_format_tests.cpp, render_tests.cpp, tests/feature/save_map_tests.cpp, tests/ui/ui_tests.cpp |
| 25 | Game screens: the HUD, the list and the dialogs on demand | code/ui/ui_hud.cpp, code/ui/ui_context.cpp, code/ui/ui_game.cpp | tests/ui/ui_tests.cpp, tests/demo/demo_tests.cpp |
| 24 | Main map: the round mini map, the unit plates and the turn | code/ui/ui_minimap.cpp, code/ui/ui_locate.cpp, code/render/render.h, code/engine/engine_march.cpp | tests/unit/render_tests.cpp, tests/feature/march_tests.cpp, save_map_tests.cpp, tests/ui/ui_tests.cpp |
| 26 | Replay: the save pairs of the original, the load in the middle of a turn and the Fidelity panel | code/engine/engine_replay.cpp, code/sim/sim.cpp (sim_resume_save), save_format (the per-force tables), code/ui/ui_fidelity.cpp, misc/make_pair_index.py, data/placeholder/pairs.tsv | tests/unit/save_format_tests.cpp, tests/feature/replay_tests.cpp, tests/ui/ui_tests.cpp, tests/demo/demo_tests.cpp |
| 27 | Base menu: the base command menu of the game mode and a turn from the game screens | code/ui/ui_base_menu.cpp, code/ui/ui_city.cpp | tests/ui/ui_tests.cpp, tests/demo/demo_tests.cpp |
| 28 | AI view: the goals of the AI units, the record of an AI turn and the AI panel | code/sim/ai.cpp, code/engine/engine_rivals.cpp, code/ui/ui_ai.cpp, data/placeholder/ai_priorities.tsv, rules.tsv | tests/feature/ai_tests.cpp, rivals_tests.cpp, tests/ui/ui_tests.cpp, tests/demo/demo_tests.cpp |
| 29 | Content source: the install of the player, the source of each asset and the Content panel | code/engine/engine_content.cpp, code/ui/ui_content.cpp, code/ui/ui_court.cpp | tests/feature/content_source_tests.cpp, tests/ui/ui_tests.cpp, tests/demo/demo_tests.cpp |
| 30 | First hour: the title rider, the load transition, the turns of the AI after 進行 and the turn messages | code/ui/ui_feel.cpp, code/ui/ui_game.cpp, code/ui/ui_hud.cpp | tests/ui/ui_tests.cpp, tests/demo/demo_tests.cpp |
| 31 | Feedback: the text provider, the message log, the speech bubble and the values | code/engine/engine_report.cpp, code/text/text.cpp, code/ui/ui_report.cpp, data/placeholder/event_tones.tsv | tests/unit/text_tests.cpp, tests/feature/report_tests.cpp, tests/ui/ui_tests.cpp, tests/demo/demo_tests.cpp |

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
