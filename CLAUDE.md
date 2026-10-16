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
  with no handler in tests/sim_tests.cpp is a failing test.
- The sim core (code/sim/) is headless and deterministic: no Windows headers,
  no I/O, no ImGui, seeded PRNG in state. Commands in, events out.
- Reviewers flag anything transliterated: single-letter locals, magic
  offsets, structs mirroring a binary layout, decompiler-style names.
- Style: see .claude/rules/handmade.md. It overrides any generic C++ guidance
  from the ECC rules under .claude/rules/ecc/.

## Build and run
- build.bat        builds build\hexdom.dll, build\win32_hexdom.exe and
                   build\sim_tests.exe (calls vcvars64 itself)
- build.bat dll    rebuilds only the DLL, for hot reload while the window
                   is open (the running exe cannot be relinked)
- build\sim_tests.exe [fixtures-dir]   headless acceptance tests; exit code
                   is pass/fail; default dir is spec-src\spec\fixtures
- build\win32_hexdom.exe [--frames N]  the game window; --frames exits after
                   N frames with exit code 0 (for automated smoke runs);
                   log goes to build\hexdom.log
- Hot reload: keep win32_hexdom.exe running, edit, run build.bat.

## Layout
- code/base.h            types, macros, memory arenas
- code/hexdom.h          the DLL contract (game_memory, platform_api, input)
- code/hexdom.cpp        game DLL, unity build of sim/ ecs/ ui/
- code/win32_hexdom.cpp  platform layer: window, D3D11, ImGui backends,
                         memory block, hot reload
- code/sim/              headless simulation core
- code/ecs/              sparse-set component tables
- code/ui/               ImGui panels (debug UI)
- tests/sim_tests.cpp    fixture runner and self-tests
- spec-src/              submodule: the analysis repo (only spec/ is used)
