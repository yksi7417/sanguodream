# 07 Platform: window, memory block, hot reload

Files: code/win32_hexdom.cpp, code/win32_window.h, code/hexdom.h,
code/hexdom.cpp. Test:
`run_tests.bat --e2e` (window smoke run) and the hot-reload loop by hand.

## What it is for
This tutorial covers the platform layer. The platform layer is the only
code that talks to Windows and the GPU. It creates the window and a D3D11
device, owns the ImGui context and backends, allocates one memory block,
and loads the game DLL. Every frame it calls one exported function,
`GameUpdateAndRender`, and renders what ImGui produced.

The window, the D3D11 device and the swap chain are in code/win32_window.h.
demo_player.exe includes the same file (tutorial 10), so the demo window
and the game window use the same code.

## The DLL contract (hexdom.h)
- `game_memory`: permanent and transient blocks plus `platform_api`, a
  table of function pointers that the DLL can call (read a file, log, and
  the ImGui allocators).
- `game_input`: frame time, frame index, and `dll_reloaded`, true for one
  frame after a hot reload.
- The DLL owns no state outside `game_memory`. That is why the platform can
  swap it while the game runs.

## Hot reload, step by step (win32_hexdom.cpp)
1. `build.bat` writes `lock.tmp` while it compiles and deletes it after.
2. Each frame the platform compares the write time of `hexdom.dll` with the
   time of the one it loaded. If the time changed and no lock file exists,
   the platform does three things. It unloads the old module. It copies the
   DLL to `hexdom_temp.dll`. It loads the copy with `LoadLibrary`, because
   the copy is what lets the compiler overwrite the original.
3. The next call gets `dll_reloaded = true`. In hexdom.cpp that rebuilds
   `engine_api`, because its function pointers pointed into the old image.
4. ImGui survives because its context and allocations belong to the exe.
   The DLL binds to them on every call with `SetCurrentContext` and
   `SetAllocatorFunctions`.

Try it: run `build\win32_hexdom.exe`, change a string in ui.cpp, run
`build.bat dll`. The reload counter in the menu bar goes up and the turn
count stays.

## Memory (the single block)
One `VirtualAlloc` of permanent + transient + frame sizes, at a fixed
address in internal builds, so pointers are stable from run to run.
hexdom.cpp puts `game_state` at the start of the permanent block and carves
the arenas after it.

## `--frames N`
The exe exits cleanly after N frames and writes `build\hexdom.log`. That is
how the smoke run and an automated check can "launch the game" with no
human to close the window. `run_tests.bat --e2e` greps the log for
"clean exit".

## Exercises
- Add a `--screenshot file.png` option. `win32_read_back_buffer` in
  code/win32_window.h reads the back buffer of the swap chain. Keep the PNG
  encoder out of the DLL. Tutorial 10 shows how demo_player.exe uses the
  same function.
- Record `dt_seconds` into a ring buffer in `game_state` and draw a frame
  time graph in the Memory panel.
