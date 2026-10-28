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
- The download is approximately 40 MB (2026-10-27): index.data is 38.3 MB
  and index.wasm is 1.7 MB. The six 3D models are 27.5 MB of it
  (docs/ASSETS.md, "Base models"):
  - the big city: 12.5 MB
  - the farm: 3.3 MB
  - the pass: 3.1 MB
  - the city: 3.0 MB
  - the port: 2.9 MB
  - the market: 2.7 MB
- The big city has approximately 355,000 triangles. Its 30k export in the
  Drive folder makes the download approximately 11 MB smaller.

## The fly.io site
The fly.io app sanguodream serves the web game at https://sanguodream.fly.dev.
Only the members of the house can play. The pack holds data from the
original game, so the site must not be public.

A member is a user who did these two steps:
1. The user signs in with Google, Apple or Facebook.
2. The user types the house password one time. The server keeps the
   membership in the record of the user.

| File | Job |
|---|---|
| web/server.py | The server, with aiohttp. It sends the game files to members, with the .gz copy for a browser that accepts gzip. It also gives the save api. |
| web/auth.py | The sign-in with Google, Apple and Facebook, the state cookie and the session cookie. |
| web/pages.py | The sign-in page and the house password page. |
| web/storage.py | The records of the users and the saves, in an S3 bucket or in a local folder. |
| web/broadcast.py | The YouTube broadcast, an option. |
| web/requirements.txt | The Python packages of the server, with exact versions. |
| Dockerfile | The image: Python 3.12, ffmpeg, the server and the four files of build/web. It does not compile the game. |
| .dockerignore | It lets only the server files and the web build into the context of the builder. |
| fly.toml | The app, the region iad, port 8080 and the health check /healthz. The machine stops when nobody uses it. |
| deploy_web.bat | It runs build_web.bat, then `flyctl deploy --remote-only --ha=false`. |

The secrets of the site. Set each one with
`flyctl secrets set NAME=value -a sanguodream`. Never write a secret in a
file of this repository.

| Secret | Job |
|---|---|
| SESSION_SECRET | The key of the cookies: a random text of 64 hex digits. The server does not start without it. |
| HOUSE_PASSWORD | The shared password of the house. The server also reads the old name SITE_PASSWORD. |
| GOOGLE_CLIENT_ID, GOOGLE_CLIENT_SECRET | The OAuth client of Google. |
| FACEBOOK_APP_ID, FACEBOOK_APP_SECRET | The app of Facebook Login. |
| APPLE_CLIENT_ID, APPLE_TEAM_ID, APPLE_KEY_ID, APPLE_PRIVATE_KEY | Sign in with Apple: the Services ID, the team, the key and the text of the .p8 file. |
| BUCKET_NAME, AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY, AWS_ENDPOINT_URL_S3, AWS_REGION | The bucket of the saves. `flyctl storage create -a sanguodream` sets them. |
| YOUTUBE_STREAM_KEY | The stream key of the YouTube channel. Without it, the page shows no Live button. |
| ADMIN_EMAILS | The email addresses of the admins, with commas between them. An admin sees the members page. |

The sign-in page shows only the providers that have their secrets. Each
provider needs this return address:
`https://sanguodream.fly.dev/auth/<google|apple|facebook>/callback`.

The rules of the site:
- The session cookie lasts 30 days. The state cookie of a sign-in lasts
  10 minutes.
- After a wrong house password, the server waits 1 second. After 10 wrong
  passwords in one hour, the server refuses the user for the rest of the
  hour.
- A form post, an upload and the broadcast must come from a page of the
  site. The server refuses a request from another origin.
- The "dev" provider signs in with a name and no password. It is for the
  tests on a local computer. The server turns it on only if AUTH_DEV_LOGIN
  is 1 and FLY_APP_NAME is empty. Thus it is never on at fly.io.
- The builder of fly.io makes the image. Docker is not necessary on this
  computer.
- There is no deploy workflow on GitHub. A build there needs the font
  subset of approximately 25 minutes and the private spec submodule.

To deploy, run `deploy_web.bat`, then open https://sanguodream.fly.dev.

### The members page
An admin opens https://sanguodream.fly.dev/admin. The page shows each user
who signed in, with these columns:
- the name, the provider and the email
- the role: admin, member, or "no house password"
- the number of saves on the server
- the time of the first sign-in and the time of the last sign-in

The server finds an admin through the email of the sign-in:
- The email must be in ADMIN_EMAILS. The server ignores the case of the
  letters.
- The provider must say that it verified the email. Google and Apple say
  it. Facebook does not say it, so a Facebook user is never an admin.
- The server keeps the email and the verified flag at each sign-in. A
  user who signed in before this page existed must sign in again one time.
- A change to ADMIN_EMAILS starts a new release of the machine. The new
  list applies at the next request.
- A "Hide My Email" address of Apple is not the real address of the user.
  To make that user an admin, put the relay address in ADMIN_EMAILS.

Another member gets 403 from the page.

To use the site without a public address, run
`flyctl proxy 8080 -a sanguodream` and open http://localhost:8080.

### The saves on the server
The server keeps the saves of each member in the bucket, at
`saves/<uid>/<name>`. Thus the saves stay after a new deploy, and a member
can play on more than one computer. Without BUCKET_NAME, the server uses
the folder in STORAGE_DIR.

| Request | Job |
|---|---|
| GET /api/me | The user: uid, name, provider, and can_broadcast. |
| GET /api/saves | The list of the saves: name, size, modified. |
| GET /api/saves/<name> | The bytes of a save. |
| PUT /api/saves/<name> | It writes a save. The limit is 16 MB. |

web/shell.html keeps IndexedDB as a copy in the browser:
- Before the start of the game, the page sends the saves that did not
  upload before. Then it reads the saves of the server. Then it removes
  the other saves of the browser, because another user of the same
  computer can have saves there.
- After the game writes a save, the page sends it to the server. If the
  upload fails, the page tries again at the next start.
- Without the save api, for example with `python -m http.server`, only
  IndexedDB keeps the saves.

### The YouTube broadcast
The broadcast is an option. It is on only if the secret YOUTUBE_STREAM_KEY has a value.
1. A member clicks the Live button in the lower right corner of the page.
2. The page records the canvas with MediaRecorder: H.264 in WebM if the
   browser can, else VP9 or VP8. It sends a chunk each second through the
   WebSocket /api/broadcast.
3. The server pipes the chunks into ffmpeg. ffmpeg encodes 720 lines at
   24 frames each second with libx264. It adds a silent audio track and
   sends the stream to the RTMP ingest of YouTube.
4. A second click on the button stops the broadcast.

Only one broadcast runs at a time. YouTube keeps each broadcast as a video
of the channel, so a broadcast becomes content to share. To check the
relay without YouTube, set YOUTUBE_INGEST to a local path: ffmpeg then
writes the stream to the file `<YOUTUBE_INGEST><YOUTUBE_STREAM_KEY>`.

### The browser tests
tests/web has four Playwright smoke tests (smoke.spec.js):
- a user signs in, joins the house and sees the title menu
- the title menu loads a save from the server
- the city list moves the camera between cities
- a unit moves beside an enemy base and attacks it

admin.spec.js has two tests of the members page. The "dev" provider gives
the email `<name>@dev.test`, and the config sets ADMIN_EMAILS to
tester@dev.test:
- the admin "Tester" sees the table
- the member "Guest" gets 403

To run them:
1. Run `build_web.bat test-game`. It writes build/webtest: the game with
   the test hooks of code/wasm_test_hooks.h.
2. In tests/web, run `npm install` and
   `node node_modules/@playwright/test/cli.js install chromium` one time.
3. In tests/web, run `npm test`.

The config starts web/server.py with the "dev" provider and a storage
folder in build/webtest-storage. Before the tests, global-setup.js puts
tests/fixtures/saves/slot_01.hxs in the saves of the test user.

The test hooks:
- The game draws ImGui on a canvas. The ImGui item hooks record the rect
  of each item with a stable id ("label###id"). The test asks for the
  rect and clicks its center.
- wasm_test_hex_screen gives the screen point of a hex.
- wasm_test_battle_setup sends a unit from a city of the player and puts
  it two hexes from an enemy base with 1 durability. The test then moves
  and attacks through the real UI.
- Only the test build has the hooks. The game of the site has none.

On Windows the tests draw with D3D11 through the GPU. Elsewhere, and with
HEXDOM_SOFTWARE_GL=1, they draw with SwiftShader at approximately 5
frames each second.

tests/web/.npmrc sets `location=project`, because the npm config of this
computer installs each package as a global package.

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
