# Testing pyramid (overrides the generic ECC testing rules)

No GoogleTest, no CMake, no coverage tooling. The framework is
tests/test_framework.h and the Dear ImGui Test Engine. run_tests.bat runs the
layers bottom to top and stops at the first failure.

## The line between UI and engine
- code/engine/engine.h defines engine_api: a table of function pointers over
  an opaque engine. It is the ONLY way a front end touches the game.
- code/ui/ may include engine.h and imgui.h, nothing from code/sim/ or
  code/ecs/ directly. A reviewer flags any UI code that reaches past the api.
- A new mechanic grows the api (queries and commands) and nothing else about
  the UI contract. Add to engine_api, implement in engine.cpp, then build UI.

## Layers (where a test goes)
1. tests/unit/ (unit_tests.exe): functions and structs of one module in
   isolation: base.h, ecs/, sim/. Fast, no files, no engine, no ImGui.
   One file per module, one function per module: unit_<module>_tests.
2. tests/feature/ (feature_tests.exe): behaviour of the backend through
   engine_api with the real engine.
   - engine_tests.cpp: what a front end observes after each command.
   - fixture_tests.cpp: every spec-src/spec/fixtures/*.tsv, one handler per
     mechanic, run through engine_api. A fixture with no handler FAILS.
3. tests/ui/ (ui_tests.exe, headless, imgui_test_engine): the real panels
   driven against tests/ui/engine_recorder.h, a recording double of
   engine_api. Each test asserts WHICH API CALLS a user action produced and
   that nothing else was commanded (recorder_count_commands). UI tests never
   assert on game state; that is the feature layer's job.
4. End to end, kept minimal: the "e2e" category in ui_tests.exe (real engine
   behind the real panels; a handful at most) and `run_tests.bat --e2e`,
   which runs the window for 120 frames and checks its log.

## Rules of thumb
- A bug found in the UI gets a ui test (expected API call) and, if the engine
  was also wrong, a feature test. Never a new e2e test.
- Feature tests set up state only through engine_api. If that is not
  possible, the api is missing a query or command: add it.
- Every test exe prints PASS/FAIL lines and "RESULT PASS|FAIL" and returns a
  nonzero exit code on failure.
- Test executables may use the CRT and Windows headers; code/sim and code/ecs
  may not.
