# hexdom

A from-scratch grand-strategy engine in C++ implemented from the behavioral
spec in spec-src/spec/. This repo has never seen the original binary or any
decompilation.

## Rules
- Implement from spec-src/spec/ only. Never read spec-src/raw/,
  spec-src/tools/, C:\src\san11-analysis directly, any game folder, any save
  folder under Documents\KOEI, or any Ghidra material. A hook enforces this
  (.claude/hooks/wall.js).
- Spec ambiguous or missing: append a question to spec-src/spec/QUESTIONS.md
  and stop. Never guess "how Koei probably did it."
- spec-src/spec/fixtures/*.tsv are the acceptance tests. Every mechanic lands
  with failing tests from those vectors first (tdd-workflow). A fixture file
  with no handler in tests/feature/fixture_tests.cpp is a failing test.
- The sim core (code/sim/) is headless and deterministic: no Windows headers,
  no I/O, no ImGui, seeded PRNG in state. Commands in, events out.
- The UI drives the backend only through engine_api (code/engine/engine.h).
  See .claude/rules/testing-pyramid.md for the test layers and where a new
  test goes.
- Reviewers flag anything transliterated: single-letter locals, magic
  offsets, structs mirroring a binary layout, decompiler-style names.
- Style: see .claude/rules/handmade.md. It overrides any generic C++ guidance
  from the ECC rules under .claude/rules/ecc/.

## Documents
- docs/ROADMAP.md            layers, content packs, rule hooks, milestones
- docs/tutorials/            one tutorial per module, each tied to its tests;
                             start at 00-reading-order.md

## Build and run
- build.bat         builds build\hexdom.dll, build\win32_hexdom.exe and the
                    three test exes (calls vcvars64 itself)
- build.bat dll     rebuilds only the DLL, for hot reload while the window
                    is open (the running exe cannot be relinked)
- run_tests.bat     unit_tests.exe -> feature_tests.exe -> ui_tests.exe,
                    stops at the first failing layer; --e2e adds the window
                    smoke run
- build\feature_tests.exe [fixtures-dir]   default spec-src\spec\fixtures
- build\ui_tests.exe [filter]              e.g. "ui", "e2e", "ui/advance"
- build\win32_hexdom.exe [--frames N]      the game window; --frames exits
                    after N frames with exit code 0; log in build\hexdom.log
- Hot reload: keep win32_hexdom.exe running, edit, run build.bat dll.

## Layout
- code/base.h            types, macros, memory arenas
- code/hexdom.h          the DLL contract (game_memory, platform_api, input)
- code/hexdom.cpp        game DLL, unity build of ecs/ sim/ engine/ ui/
- code/win32_hexdom.cpp  platform layer: window, D3D11, ImGui backends,
                         memory block, hot reload
- code/ecs/              sparse-set component tables
- code/sim/              headless simulation core (rules)
- code/engine/           the backend: owns the sim and the event ledger,
                         exposes engine_api
- code/ui/               ImGui panels; call engine_api only
- tests/test_framework.h one check macro, counters, heap arenas
- tests/unit/            per-module unit tests
- tests/feature/         engine_api behaviour tests and the fixture runner
- tests/ui/              imgui_test_engine tests, engine_recorder.h double,
                         hexdom_te_imconfig.h (test-only ImGui config)
- third_party/imgui      Dear ImGui (submodule, pinned)
- third_party/imgui_test_engine   Dear ImGui Test Engine (submodule, pinned;
                         free license for individuals, check before any
                         commercial use)
- spec-src/              submodule: the analysis repo (only spec/ is used)
