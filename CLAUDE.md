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
- Take each draw of the PRNG in its own statement. C++ does not fix the
  order of the arguments of a call. MSVC and clang then give two
  different games, and a shared game of Windows and the web goes out of
  step (docs/MULTIPLAYER.md, "The status of MP1").
- The UI drives the backend only through engine_api (code/engine/engine.h).
  See .claude/rules/testing-pyramid.md for the test layers and for the
  place where a new test goes.
- Reviewers flag each sign of a transliteration: single-letter locals, magic
  offsets, structs that mirror a binary layout, decompiler-style names.
- Style: see .claude/rules/handmade.md. It overrides the generic C++
  guidance from the ECC rules under .claude/rules/ecc/.
- Documents, commit messages and comments are Simplified Technical
  English (.claude/rules/ste-writing.md). Write it right the first time,
  so that no check-and-fix loop is necessary:
  - Keep each sentence under 20 words. Put one fact in each sentence.
  - Do not join clauses with a semicolon, a colon, or a parenthesis.
    Make a second sentence.
  - Do not write "which", "could", "would", "should", "may", "once",
    "ensure", "e.g.", "etc." or "via". Use "that", "can", "must",
    "after", "make sure", "for example", a list, and "through".
  - Say who does the action: "The loader reads", not "is read".
  - Do not use a verb in -ing.
  - Before a commit, run `python -X utf8 -I misc\ste_check.py -v FILE`
    one time. The `-X utf8` flag stops a crash on Chinese text in the
    report.

## Documents
- docs/ROADMAP.md            layers, content packs, rule hooks, milestones
- docs/tutorials/            one tutorial per module, each tied to its tests;
                             start at 00-reading-order.md
- docs/ASSETS.md             fonts and other assets, with their licences
- docs/GRAPHICS.md           the plan for the art and the animation (G1-G6)
- docs/WEB.md                the WebAssembly build: the plan, the status of
                             phase 0 and the memory budget
- docs/REPLAY.md             the replay of a game, step by step: the record,
                             the checkpoints and the format in the save file
- docs/art/GEMINI-PROMPTS.md the prompts that make the art with Gemini
- docs/MULTIPLAYER.md        the plan of the shared games: the relay of the
                             steps, the rooms and the milestones MP1-MP8
- docs/MAPS.md               maps from real geography: the scale, the
                             licences of the data, the tool and the plan
- docs/EUROPE.md             the game of 1200: English names by key, the
                             lives, the arms, the church and the story

## Build and run
- build.bat         builds build\hexdom.dll, build\win32_hexdom.exe, the
                    three test exes, build\demo_player.exe,
                    build\gif_encode.exe and build\save_dump.exe (calls
                    vcvars64 itself)
- build.bat dll     rebuilds only the DLL, for hot reload while the window
                    is open (you cannot relink the exe while it runs)
- run_tests.bat     unit_tests.exe -> feature_tests.exe -> ui_tests.exe,
                    stops at the first layer that fails; --e2e adds the
                    window smoke run. With --demo, it runs only the
                    demo walkthroughs and records them to build\demos.
                    With --demo-fast, it runs the same walkthroughs and
                    checks in the fast mode. It records no video.
                    Use --demo-fast in each quality gate before a
                    commit. Use --demo only to make the video of a
                    milestone (.claude/rules/testing-pyramid.md).
                    With --web, it runs only the tests of build_web.bat
                    under node. With --server, it runs only the tests of
                    the room server in tests\server, with Python
- build_web.bat     builds the web game into build\web (index.html, .js,
                    .wasm, .data) with the Emscripten SDK in C:\tools\emsdk.
                    "build_web.bat tests" builds the unit tests and the
                    feature tests as WebAssembly (docs/WEB.md)
- build_web.bat referee     builds build\referee.js, the referee of the shared
                    games for node (docs/MULTIPLAYER.md, MP6).
                    "run_tests.bat --server" tests it
- build_web.bat test-game   builds the web game with the test hooks into
                    build\webtest; then "npm test" in tests\web runs the
                    Playwright smoke tests (docs/WEB.md, "The browser tests")
- deploy_web.bat    builds the web game and deploys it to the fly.io app
                    sanguodream, for the members of the house (docs/WEB.md,
                    "The fly.io site")
- build\feature_tests.exe [fixtures-dir]   default spec-src\spec\fixtures
- build\ui_tests.exe [filter]              for example "ui", "e2e",
                                           "ui/advance"
- build\demo_player.exe [test-name] [--list] [--fast] [--no-ffmpeg]
                    plays the "demo" tests at cinematic speed. It
                    records an MP4 (ffmpeg) or a GIF, and a PNG of each
                    panel.
                    With --fast, it plays them at ImGuiTestRunSpeed_Fast
                    with no vsync, and it records nothing. The checks
                    are the same in the two modes
- build\save_dump.exe <file.s11> [--all-slots] [--utf8-names]
                    [--name-index <Scenario.s11>]   prints a save as JSON
                    with the clean-room loader; with --name-index, each
                    city also has its name bytes
- build\win32_hexdom.exe [--frames N] [--pack NAME]   the game window.
                    With --frames, it exits after N frames with exit
                    code 0. With --pack, it loads the rules and the map
                    of data\NAME. The log is build\hexdom.log
- Hot reload: leave win32_hexdom.exe open, edit, then run build.bat dll.

## Layout
- code/base.h            types, macros, memory arenas
- code/hexdom.h          the DLL contract (game_memory, platform_api, input)
- code/version.h         HEXDOM_VERSION changes by hand at a release.
                         misc/build_number.bat gives the build number and
                         the commit. The title and the help window show
                         them.
- code/hexdom.cpp        game DLL, unity build of ecs/ sim/ engine/ ui/
- code/win32_hexdom.cpp  platform layer: window, D3D11, ImGui backends,
                         fonts, memory block, hot reload
- code/win32_window.h    the window and the D3D11 swap chain, for
                         win32_hexdom.exe and demo_player.exe
- code/win32_portraits.h the default portraits: maps the archive, makes a
                         texture for each picture on demand (docs/ASSETS.md)
- code/wasm_hexdom.cpp   the web platform layer: Emscripten, WebGL2, ImGui
                         GLFW and OpenGL3 backends, saves in IndexedDB; it
                         includes hexdom.cpp (docs/WEB.md)
- code/webgl2_renderer.h the WebGL2 port of d3d11_renderer.h. A change to
                         one renderer goes into the other in the same commit
- code/wasm_portraits.h  the portraits and the generated faces of the web
- code/wasm_imconfig.h   the ImGui settings of the web build
- web/shell.html         the page of the web build. It loads
                         web/shared_game.js, the lobby and the socket of a
                         shared game
- web/server.py          the server of the fly.io site (Dockerfile,
                         fly.toml). web/http_common.py has the parts that
                         its routes share, and web/games_api.py has the
                         routes of the shared games. web/auth.py signs in the users, and
                         web/storage.py keeps the saves in S3.
                         web/game_files.py serves index.wasm and
                         index.data from the bucket to members.
                         web/rooms.py keeps the rooms of the shared games
                         and relays their steps (docs/MULTIPLAYER.md).
                         web/room_state.py has the checks of one room, and
                         web/room_socket.py has the messages of its socket.
                         web/broadcast.py relays the optional YouTube
                         broadcast
- code/wasm_referee.cpp  the referee of the shared games: a headless engine
                         for node. web/referee.py starts it (docs/MULTIPLAYER.md)
- code/wasm_test_hooks.h the test hooks of "build_web.bat test-game" for
                         the Playwright smoke tests in tests/web
- code/render/           render commands and camera; png.h, zip.h and
                         jpeg.h decoders; portraits.h, the picture of an
                         officer
- code/ecs/              sparse-set component tables
- code/sim/              headless simulation core (rules)
- code/engine/           the backend: owns the sim and the event ledger,
                         exposes engine_api
- code/text/             front end text: Big5 to UTF-8 (a script writes
                         big5_table.h), UI strings, names and lives for each
                         locale
- code/ui/               ImGui panels; call engine_api only; ui_fonts.h
                         loads the fonts for the executables
- tests/test_framework.h one check macro, counters, heap arenas
- tests/test_files.h     file reads and fixture paths for the test exes
                         and save_dump
- misc/save_dump.cpp     the save_dump tool; misc/ste_check.py checks STE
- misc/make_big5_table.py  writes code/text/big5_table.h from
                         data/unicode/BIG5.TXT
- misc/gif_encode.cpp    the GIF encoder of demo_player.exe without ffmpeg
- misc/make_map_pack.py  copies the map fixtures of the spec into the pack
                         (run it after a spec update; tutorial 22)
- misc/make_world_map.py  makes the map, cities.tsv and bases.tsv of a pack
                         from real geography: NOAA ETOPO heights and
                         Natural Earth vectors (docs/MAPS.md)
- misc/make_scenario.py  checks a scenario of a pack (TSV files). It writes
                         the Big5 names of its officers (docs/MAPS.md).
                         It also writes names/en.tsv and lore/links.tsv of
                         the pack (docs/EUROPE.md)
- misc/make_pair_index.py  writes data/placeholder/pairs.tsv from the save
                         pairs of the spec (run it after a spec update;
                         tutorial 26)
- misc/glb_import.py     converts a GLB into the 3D model of a base kind
                         or of a facility shape,
                         data/<pack>/models/<name>.hxm and .png
                         (docs/ASSETS.md, "Base models")
- misc/drive_models.py   imports the new GLBs of the shared Drive folder
                         07-3D-Models with glb_import.py; --list shows
                         the decision for each file (docs/ASSETS.md)
- misc/subset_font.py    writes the subset of the CJK font for the web
                         build to build/web/fonts (docs/WEB.md)
- misc/make_portraits.py  makes data/civil_war/Portraits.zip, the 224 x 280
                         portraits, from the source archives (docs/ASSETS.md)
- data/placeholder/      the content pack: text/<locale>.tsv (UI strings),
                         names/<locale>.tsv (names by Big5 hex)
- data/placeholder/map/  the real map (M10): hexes.tsv and bases.tsv of
                         spec-src/spec/fixtures/map; misc/make_map_pack.py
                         copies them and writes movement_costs.tsv
- data/civil_war/        the second content pack (M9): the same engine code
                         with other data; see its README.md
- data/medieval_europe/  the prototype map from real geography: Europe and
                         North Africa in 1200, with its own scenario in
                         scenarios/y1200; see its README.md
- data/medieval_west/    the west map of the game of 1200 (E8): 42 cities
                         and its own scenario; see its README.md
- data/medieval_east/    the east map of the game of 1200 (E8): 42 cities,
                         with Antioch, and its own scenario; see its README.md
- data/fonts/, data/unicode/   open-licence assets; see docs/ASSETS.md
- tests/unit/            per-module unit tests
- tests/feature/         engine_api behaviour tests and the fixture runner
- tests/ui/              imgui_test_engine tests, engine_recorder.h double,
                         hexdom_te_imconfig.h (test-only ImGui config)
- tests/server/          the tests of the Python server of the site
                         (run_tests.bat --server)
- tests/demo/            demo_player.cpp and demo_tests.cpp: one recorded
                         walkthrough for each milestone
- third_party/imgui      Dear ImGui (submodule, pinned)
- third_party/imgui_test_engine   Dear ImGui Test Engine (submodule, pinned;
                         free license for individuals, check before any
                         commercial use)
- spec-src/              submodule: the analysis repo (this repository uses
                         only spec/)
