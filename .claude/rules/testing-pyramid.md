# Testing pyramid (overrides the generic ECC testing rules)

This rule says where each test goes and what each layer checks. There is
no GoogleTest, no CMake and no coverage tool. The framework is
tests/test_framework.h and the Dear ImGui Test Engine. run_tests.bat runs
the layers from the bottom to the top and stops at the first failure.

## The line between UI and engine
- code/engine/engine.h defines engine_api: a table of function pointers over
  an opaque engine. It is the ONLY way a front end touches the game.
- code/ui/ can include engine.h, imgui.h and code/text/text.h, and nothing
  from code/sim/ or code/ecs/ directly. A reviewer flags UI code that
  reaches past the api. The text module is a part of the front end: it
  gives strings and names, and it never touches the engine.
- A new mechanic grows the api (queries and commands) and nothing else in
  the UI contract. Add to engine_api, implement in engine.cpp, then build
  the UI.

## Layers (where a test goes)
1. tests/unit/ (unit_tests.exe): functions and structs of one module in
   isolation: base.h, ecs/, sim/, text/. Fast, no files, no engine, no
   ImGui.
   One file per module, one function per module: unit_<module>_tests.
2. tests/feature/ (feature_tests.exe): behaviour of the backend through
   engine_api with the real engine.
   - engine_tests.cpp: what a front end observes after each command.
   - fixture_tests.cpp: every spec-src/spec/fixtures/*.tsv, one handler per
     mechanic, run through engine_api. A fixture with no handler FAILS.
3. tests/ui/ (ui_tests.exe, headless, imgui_test_engine): the real panels,
   driven against tests/ui/engine_recorder.h. The recorder is a double of
   engine_api that records each call. Each test asserts the API CALLS that
   a user action produced, and that the action commanded nothing else
   (recorder_count_commands). UI tests never assert on game state. That is
   the job of the feature layer.
4. End to end, kept minimal: the "e2e" category in ui_tests.exe and
   `run_tests.bat --e2e`. The "e2e" category puts the real engine behind
   the real panels, with a handful of tests at most. `run_tests.bat --e2e`
   runs the window for 120 frames and checks its log.
5. Demo walkthroughs: the "demo" category in tests/demo/demo_tests.cpp,
   run by demo_player.exe, `run_tests.bat --demo-fast` and
   `run_tests.bat --demo`. Each milestone has
   one cinematic walkthrough, "demo/<milestone>", for example
   "demo/m1_loader". It is the recorded demo of the milestone and also an
   end-to-end smoke test.
   - Keep one demo test for each milestone. When you complete a milestone,
     add its walkthrough. Do not add a second demo test to an old milestone.
   - The walkthrough runs the real engine behind the real panels in a real
     window, at ImGuiTestRunSpeed_Cinematic.
   - Put `ctx->CaptureBeginVideo` before the first step and
     `ctx->CaptureEndVideo` after the last step. The player writes
     build\demos\<milestone>.mp4 (a GIF if the computer has no ffmpeg) and
     one PNG for each panel that the walkthrough shows.
   - Put an IM_CHECK after each step, so that the walkthrough fails when
     the UI or the engine breaks.
   - `run_tests.bat --demo` builds nothing. It fails if a demo test fails,
     or if build\demos does not have the video and the PNG files of a test.
   - `run_tests.bat --demo-fast` runs `demo_player.exe --fast`. The fast
     mode runs the same steps and checks at ImGuiTestRunSpeed_Fast. It
     uses no vsync and a fixed frame time of 1/60 s. It records no video
     and no PNG, and the helpers that record return success.
   - A walkthrough gives the same result in the two modes. Fast mode
     skips SleepStandard, and the mouse jumps to its target. The timers
     of the UI run four times as fast.
   - If a step waits for the UI, it waits for a condition with a frame
     limit. Examples are an animation, a timer and the AI round. It does
     not wait for a time.
   - Use the fast mode in each quality gate. Before a commit, run
     `run_tests.bat`, then `run_tests.bat --demo-fast`. The full fast
     run takes approximately 90 s.
   - Use the demo mode only to make the video and the PNG files of a
     milestone. Run `run_tests.bat --demo` when you complete a milestone,
     or when the user asks for a new video. The full demo run takes
     approximately 20 minutes.

6. tests/server/ (Python unittest, `run_tests.bat --server`): the server
   of the site in web/, with FolderStorage in a temporary folder and a
   fake socket. rooms_tests.py checks the rooms of the shared games
   (docs/MULTIPLAYER.md). It checks the seats, the order of the steps,
   the checksums and the log after a stop of the server. A test of the real
   site in a browser goes to tests/web (Playwright, `npm test`).
   referee_tests.py runs the referee (build\referee.js) under node on the
   fixture of the feature tests of the web build. Run `build_web.bat tests`,
   `run_tests.bat --web` and `build_web.bat referee` first.

## Rules of thumb
- A bug found in the UI gets a ui test (expected API call). If the engine
  was also wrong, it also gets a feature test. Never a new e2e test, and
  never a new demo test.
- Feature tests set up state only through engine_api. If that is not
  possible, the api lacks a query or command: add it.
- Every test exe prints PASS/FAIL lines and "RESULT PASS|FAIL" and returns a
  nonzero exit code on failure.
- Test executables can use the CRT and Windows headers; code/sim and code/ecs
  cannot.
