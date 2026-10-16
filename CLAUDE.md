# hexdom

This document says how to work in the hexdom repository. The engine is a
grand-strategy engine in C++, written from scratch from the behavioral spec
in spec-src/spec/. This repository has never seen the original binary or a
decompilation of it.

## Rules
- Implement from spec-src/spec/ only. Never read spec-src/raw/,
  spec-src/tools/, C:\src\san11-analysis directly, a game folder, a save
  folder under Documents\KOEI, or Ghidra material. A hook enforces this
  rule (.claude/hooks/wall.js).
- If the spec is ambiguous or has no answer, append a question to
  spec-src/spec/QUESTIONS.md and stop. Never guess "how Koei probably did
  it."
- spec-src/spec/fixtures/*.tsv are the acceptance tests. Each mechanic
  starts with tests from those vectors, and those tests fail first
  (tdd-workflow). A fixture file with no handler in
  tests/feature/fixture_tests.cpp is a test that fails.
- The sim core (code/sim/) is headless and deterministic: no Windows
  headers, no I/O, no ImGui, and a seeded PRNG in the state. Commands go in,
  events come out.
- The UI drives the backend only through engine_api (code/engine/engine.h).
  See .claude/rules/testing-pyramid.md for the test layers and for the
  place where a new test goes.
- Reviewers flag each sign of a transliteration: single-letter locals, magic
  offsets, structs that mirror a binary layout, decompiler-style names.
- Style: see .claude/rules/handmade.md. It overrides the generic C++
  guidance from the ECC rules under .claude/rules/ecc/.

## Documents
- docs/ROADMAP.md            layers, content packs, rule hooks, milestones
- docs/tutorials/            one tutorial per module, each tied to its tests;
                             start at 00-reading-order.md

## Build and run
- build.bat         builds build\hexdom.dll, build\win32_hexdom.exe and the
                    three test exes (calls vcvars64 itself)
- build.bat dll     rebuilds only the DLL, for hot reload while the window
                    is open (you cannot relink the exe while it runs)
- run_tests.bat     unit_tests.exe -> feature_tests.exe -> ui_tests.exe,
                    stops at the first layer that fails; --e2e adds the
                    window smoke run
- build\feature_tests.exe [fixtures-dir]   default spec-src\spec\fixtures
- build\ui_tests.exe [filter]              for example "ui", "e2e",
                                           "ui/advance"
- build\win32_hexdom.exe [--frames N]      the game window; --frames exits
                    after N frames with exit code 0; log in build\hexdom.log
- Hot reload: leave win32_hexdom.exe open, edit, then run build.bat dll.

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
- spec-src/              submodule: the analysis repo (this repository uses
                         only spec/)
