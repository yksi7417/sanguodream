# The web build

This document gives the plan and the status of the WebAssembly build of
hexdom. The web build is a second platform layer next to win32_hexdom.cpp.
The game code does not change for it.

## The plan
1. Phase 0 makes the game code portable. The Windows build stays as it is.
2. Option A adds the web platform layer:
   - code/wasm_hexdom.cpp: Emscripten, the main loop of the browser, the
     files of the pack in MEMFS, and the saves in IDBFS.
   - code/webgl2_renderer.h: a port of d3d11_renderer.h to WebGL2. It
     draws the same render_commands, with the same 3D scene.
   - build_web.bat and web/shell.html.
3. A commit that adds a render feature changes d3d11_renderer.h and
   webgl2_renderer.h together.

## Phase 0 status

| Step | Status |
|---|---|
| The game paths use '/' | Done. The save header check accepts '\' and '/' (engine_same_path), so the old saves still load. |
| HEXDOM_EXPORT and HEXDOM_STATIC_GAME (hexdom.h) | Done. The web build includes hexdom.cpp in its platform unit. |
| The fonts, the models and the textures load through platform_api | Done. ui_fonts_load and d3d11_renderer_init get the file reader of the platform. render_mesh_file_parse reads the mesh files for each renderer. |
| The peak use of each memory block | Done. The window writes it to build/hexdom.log when it closes. |
| tests/test_files.h without windows.h | Done. The helpers have a Windows half and a POSIX half. test_list_folder replaces FindFirstFile in the feature tests. |
| The unit tests and the feature tests under emcc and node | Done. build_web.bat builds build/unit_tests.js and build/feature_tests.js. `run_tests.bat --web` runs them. Each suite gives the same counts as on Windows. |
| A subset of the CJK font for the web | Done. misc/subset_font.py writes build/web/fonts/NotoSansCJKtc-Regular.otf. The Windows build keeps the full font. |

## The tools
- The Emscripten SDK is in C:\tools\emsdk. To install it, clone
  github.com/emscripten-core/emsdk, then run `emsdk install latest` and
  `emsdk activate latest`. build_web.bat calls emsdk_env.bat itself. To
  use another folder, set EMSDK.
- fontTools is in the virtual environment C:\tools\hexdom-venv, because
  `python -I` does not read the packages of the user. To make it, run
  `python -m venv C:\tools\hexdom-venv`, then
  `C:\tools\hexdom-venv\Scripts\python -m pip install fonttools`.
- To make the font, run
  `C:\tools\hexdom-venv\Scripts\python -I misc\subset_font.py` from the
  root of the repository. The script takes approximately 25 minutes. The
  subset is 3.4 MB in place of 16.4 MB, with 14,212 code points. ImGui
  builds the same atlas from it as from the full font: 14,300 glyphs.

## The web build of the tests
- build_web.bat compiles with the warnings of build.bat, and with
  -Werror. It also turns off one clang check that MSVC does not have: a u8
  that the code compares with a capacity of 256.
- The tests use NODERAWFS, so they read the real files of the repository.
  On POSIX, test_exe_dir gives the current folder. Thus
  `run_tests.bat --web` runs them in build/, as on Windows.

## The memory budget
The window reserves these blocks. The peak is from the title screen with
the real map (`win32_hexdom.exe --frames 120`, 2026-10-27).

| Block | Size (MB) | Peak (MB) |
|---|---|---|
| Permanent | 256 | 16.4 |
| Transient | 256 | 4.0 |
| Frame | 160 | 67.0 |
| Renderer scratch | 64 | 35.5 |

The frame block holds the terrain mesh of the 200 x 200 map. The renderer
scratch holds the model of the big city. Measure the peak again after a
long game before you set the sizes of the web build. The target for the
web build is less than 512 MB.
