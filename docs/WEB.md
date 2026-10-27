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

## The web game (Option A)
To build and run it:
1. Make the web font one time: `C:\tools\hexdom-venv\Scripts\python -I misc\subset_font.py`.
2. Run `build_web.bat`. It writes build/web/index.html, index.js,
   index.wasm and index.data.
3. Serve build/web with a static server, for example
   `python -m http.server 8090 --directory build/web`, and open
   http://localhost:8090.

The parts of the web platform layer:

| File | Job |
|---|---|
| code/wasm_hexdom.cpp | The platform layer. It includes hexdom.cpp (HEXDOM_STATIC_GAME). The browser calls wasm_frame for each frame. |
| code/webgl2_renderer.h | The scene: the terrain, the grid, the meshes, the 3D models of the bases and the billboards. |
| code/wasm_portraits.h | The default portraits and the generated faces as WebGL2 textures. |
| code/wasm_imconfig.h | 32-bit ImGui indices. The draw list of the main map has more than 65536 vertices. |
| web/shell.html | The page: a canvas that fills the window. |

The rules of the web build:
- The current folder is /build, and index.data holds /data and the three
  scenario files of the title menu. Thus the paths of the game are the
  same as on Windows.
- The saves are in IndexedDB, mounted at /build/saves. The platform writes
  IndexedDB after each save and every 600 frames.
- The renderer sends the matrix of camera_view_projection without a
  change. The shaders use a row_major block and move the depth from the
  range of D3D to the range of WebGL2. The CPU pick stays the same.
- ImGui uses its GLFW backend on the GLFW of Emscripten, and its OpenGL3
  backend. Each frame, the GLFW window takes the CSS size of the canvas.

The status on 2026-10-27:
- The title menu, the scenario list, the ruler list, the settings and the
  main map work in the browser pane. The 3D model of the big city shows at
  陳留 and 濮陽. The wheel zooms the map.
- These checks are open:
  - a portrait on the screen
  - a save, a reload of the page, and a load
  - the loss of the WebGL2 context
  - the parity test of the 3D scene against the Windows build
- The download is approximately 26 MB. The model of the big city is 12 MB
  of it.

## The fly.io site
The fly.io app sanguodream serves the web game at https://sanguodream.fly.dev.
A password protects the site. The pack holds data from the original game,
so the site must not be public.

| File | Job |
|---|---|
| web/server.py | The static server. It asks for HTTP basic auth and sends the .gz copy of a file to a browser that accepts gzip. |
| Dockerfile | The image: Python 3.12, web/server.py and the four files of build/web. It does not compile the game. |
| .dockerignore | It lets only these five files into the context of the builder. |
| fly.toml | The app, the region iad and port 8080. The machine stops when nobody uses it. |
| deploy_web.bat | It runs build_web.bat, then `flyctl deploy --remote-only --ha=false`. |

The rules of the site:
- The password is the fly secret SITE_PASSWORD. The user name is the
  secret SITE_USER, and the default is "hexdom". Never write the password
  in a file of this repository.
- If SITE_PASSWORD is empty, the server does not start.
- After a wrong password, the server waits 1 second.
- web/shell.html gives index.wasm and index.data an address without the
  password. Thus an address of the form `user:password@host` also works.
- The builder of fly.io makes the image. Docker is not necessary on this
  computer.
- There is no deploy workflow on GitHub. A build there needs the font
  subset of approximately 25 minutes and the private spec submodule.

To deploy:
1. Set the password one time:
   `flyctl secrets set SITE_PASSWORD=<password> -a sanguodream`.
2. Run `deploy_web.bat`.
3. Open https://sanguodream.fly.dev and type the user name and the
   password.

To use the site without a public address, run
`flyctl proxy 8080 -a sanguodream` and open http://localhost:8080.

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
