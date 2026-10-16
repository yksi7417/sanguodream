# 10 Demo recorder: one recorded walkthrough for each milestone

Files: tests/demo/demo_player.cpp, tests/demo/demo_tests.cpp,
code/win32_window.h, misc/gif_encode.cpp, tests/ui/hexdom_te_imconfig.h.
Tests: the two "demo" tests in tests/demo/demo_tests.cpp, run by
`run_tests.bat --demo`.

## What it is for
This tutorial covers the demo recorder. The recorder plays one scripted
walkthrough for each milestone in a real window and records it to a video.
Each walkthrough also checks the result of each step. Thus the same script
gives a demo of the milestone and an end-to-end smoke test.

The rule for this layer is in .claude/rules/testing-pyramid.md, layer 5:
one demo test for each milestone, recorded, with an IM_CHECK after each
step.

## The parts
- demo_player.exe opens a window of 1280 x 720 pixels with D3D11. It uses
  code/win32_window.h, the same window code as win32_hexdom.exe.
- The player compiles the engine and the UI into the exe. It does not load
  hexdom.dll, so it always runs the game code of its own build.
- The player links imgui_te.lib: Dear ImGui, the Dear ImGui Test Engine and
  the Win32 and DX11 backends. hexdom_te_imconfig.h sets
  `IMGUI_TEST_ENGINE_ENABLE_CAPTURE` to 1, so the lib contains the capture
  tool and the stb image writer.
- demo_tests.cpp registers the tests of category "demo". The player runs
  them at `ImGuiTestRunSpeed_Cinematic`. At this speed, the mouse moves at
  the speed of a person. The test engine also stops for a short time after
  each action.

## How a frame becomes a video
1. The test engine calls the `GuiFunc` of the test. `demo_gui` draws the
   real panels over the real engine.
2. `win32_render_frame` renders the frame into the back buffer.
3. `ImGuiTestEngine_PostSwap` runs the capture tool. The tool calls
   `demo_capture_screen`, the `ScreenCaptureFunc` of the player.
4. `win32_read_back_buffer` copies the back buffer into a staging texture.
   Then it copies the rows into the RGBA buffer of the capture tool.
5. The capture tool writes the RGBA frame to the stdin of the encoder.
6. `win32_present_frame` shows the frame.

Caution: step 3 must come before step 6. The swap chain uses the DISCARD
swap effect, so the back buffer has no defined contents after a present.

## The encoder
The player looks for ffmpeg.exe in this order:
1. The path after `--ffmpeg`.
2. The folders on the PATH.
3. %LOCALAPPDATA%\Microsoft\WinGet\Links\ffmpeg.exe.
4. %LOCALAPPDATA%\Microsoft\WinGet\Packages\Gyan.FFmpeg*\ffmpeg-*\bin.

The last two steps find a winget install from a shell that was open before
the install. If the player finds ffmpeg, it records an MP4. If not, it
starts build\gif_encode.exe and records a GIF. The option `--no-ffmpeg`
selects the GIF encoder on a computer that has ffmpeg.

gif_encode.exe reads raw RGBA frames from stdin. It uses a fixed palette of
256 colors: a 6 x 6 x 6 color cube and 40 grays. For each frame it writes
only the rectangle that changed. It merges identical frames into one frame
with a longer delay. The GIF has fewer colors than the MP4. Thus it is a
fallback, not the usual output.

## The walkthroughs
All panels start closed. Each walkthrough opens its panels through the
Windows menu. The ini text `demo_layout_ini` in demo_player.cpp gives each
panel its place, so that no panel covers another panel.

"demo/m0_skeleton" shows milestone M0:
1. It opens the Simulation, Hex map, Event ledger and Memory panels.
2. It clicks two cells on the hex map and checks the path cost.
3. It advances 1 turn, then 12 turns two times, and checks turn 25.
4. It checks 25 events in the ledger and scrolls the ledger.

"demo/m1_loader" shows the loader half of milestone M1:
1. It opens the Simulation, Officers and Cities panels.
2. It loads the sample save and checks 852 or more officers and 42 cities.
3. It loads the city names and checks the name of city 36.
4. It scrolls the Officers table and the Cities table to the last row and
   back to the first row.

After `CaptureEndVideo`, each walkthrough writes one PNG for each panel
that it showed. The video and the PNG files go to build\demos:

| File | Contents |
|---|---|
| m0_skeleton.mp4 | the M0 walkthrough (m0_skeleton.gif without ffmpeg) |
| m0_skeleton_simulation.png, _hex_map, _event_ledger, _memory | one M0 panel each |
| m1_loader.mp4 | the M1 walkthrough (m1_loader.gif without ffmpeg) |
| m1_loader_simulation.png, _officers, _cities | one M1 panel each |

Each walkthrough deletes its old files before it records. It checks that
each new file exists and is not empty.

## The helpers of the script
- `demo_open_panel` checks the menu item and checks that ImGui draws the
  window of the panel.
- `demo_click_cell` finds the "cells" button inside the "map_canvas" child
  window. It uses `hex_to_pixel` with the same origin as `ui_draw_map`. Then
  it moves the mouse to the cell and clicks.
- `demo_scroll_table` finds the child window of a table, for example with
  `ctx->WindowInfo("//Officers/officers")`. It moves the scroll bar to the
  bottom and to the top, and checks the scroll position each time.
- `demo_begin_video`, `demo_end_video` and `demo_screenshot_panel` set the
  output file of the capture tool and check the result.

## Run the walkthroughs
1. Run `build.bat`. It builds demo_player.exe and gif_encode.exe.
2. Run `run_tests.bat --demo`. Do not use the mouse or the keyboard on the
   window while it runs.
3. Make sure that the last line is "ALL PASS".
4. Open build\demos\m0_skeleton.mp4 and build\demos\m1_loader.mp4.

To run one walkthrough, run `build\demo_player.exe m1_loader`. To see the
names of all walkthroughs, run `build\demo_player.exe --list`.

`run_tests.bat --demo` runs only the walkthroughs. It does not run the
unit, feature and ui layers, and it does not build. One walkthrough takes
approximately 25 seconds.

## Add the walkthrough of a new milestone
1. In demo_tests.cpp, write one function `demo_<milestone>`. Start it with
   `demo_begin_video` and end it with `demo_end_video`.
2. Put one IM_CHECK after each step. Check the result through `engine_api`
   or `ui_state`.
3. After `demo_end_video`, call `demo_screenshot_panel` one time for each
   panel that the walkthrough shows.
4. Register the function in `register_demo_tests` with the category "demo".
5. If the walkthrough shows a new panel, add the place of the panel to
   `demo_layout_ini`.
6. Run `run_tests.bat --demo` and watch the new video.

If an old milestone gets a new feature, add steps to its walkthrough. Do
not add a second demo test for that milestone.

## Exercises
- Run `build\demo_player.exe m0_skeleton --no-ffmpeg`. Compare the size of
  m0_skeleton.gif with the size of m0_skeleton.mp4.
- Change one check in `demo_m0_skeleton` so that it fails, for example the
  turn after the first click. Run `run_tests.bat --demo` and read the log.
  Then undo the change.
- Add a `--screenshot file.png` option to win32_hexdom.exe. Use
  `win32_read_back_buffer` and write the PNG in the exe, not in the DLL.
