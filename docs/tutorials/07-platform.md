# 07 Platform: window, memory block, hot reload

Files: code/win32_hexdom.cpp, code/hexdom.h, code/hexdom.cpp. Test:
`run_tests.bat --e2e` (window smoke run) and the hot-reload loop by hand.

## What it is for
The platform layer is the only code that talks to Windows and the GPU. It
creates the window and a D3D11 device, owns the ImGui context and backends,
allocates one memory block, and loads the game DLL. Every frame it calls one
exported function, `GameUpdateAndRender`, and renders what ImGui produced.

## The DLL contract (hexdom.h)
- `game_memory`: permanent and transient blocks plus `platform_api`, a
  table of function pointers the DLL may call (read a file, log, and the
  ImGui allocators).
- `game_input`: frame time, frame index, and `dll_reloaded`, true for one
  frame after a hot reload.
- The DLL owns no state outside `game_memory`. That is why it can be
  swapped while running.

## Hot reload, step by step (win32_hexdom.cpp)
1. `build.bat` writes `lock.tmp` while compiling and deletes it after.
2. Each frame the platform compares `hexdom.dll`'s write time with the one
   it loaded. If it changed and no lock file exists, it unloads the old
   module, copies the DLL to `hexdom_temp.dll`, and `LoadLibrary`s the copy
   (the copy is what lets the compiler overwrite the original).
3. The next call gets `dll_reloaded = true`. In hexdom.cpp that rebuilds
   `engine_api`, because its function pointers pointed into the old image.
4. ImGui survives because its context and allocations belong to the exe;
   the DLL binds to them every call with `SetCurrentContext` and
   `SetAllocatorFunctions`.

Try it: run `build\win32_hexdom.exe`, change a string in ui.cpp, run
`build.bat dll`. The menu bar's reload counter goes up and the turn count
stays.

## Memory (the single block)
`VirtualAlloc` of permanent + transient + frame sizes, at a fixed address in
internal builds so pointers are stable run to run. hexdom.cpp puts
`game_state` at the start of the permanent block and carves arenas after it.

## `--frames N`
The exe exits cleanly after N frames and writes `build\hexdom.log`. That is
how the smoke run and any automated check can "launch the game" without a
human closing the window. `run_tests.bat --e2e` greps the log for
"clean exit".

## Exercises
- Add a `--screenshot file.png` option using the swap chain's back buffer
  (keep the encoder out of the DLL).
- Record `dt_seconds` into a ring buffer in `game_state` and draw a frame
  time graph in the Memory panel.
