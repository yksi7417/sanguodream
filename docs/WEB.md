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
| web/shared_game.js | The script of a shared game: the lobby and the socket of a room. The page loads it before the game. |

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
- The download is approximately 100 MB (2026-10-29): index.data is 98.5 MB
  and index.wasm is 1.7 MB. The 25 3D models are 78.7 MB of it
  (docs/ASSETS.md, "Base models"):
  - the smithy at level 3: 4.0 MB
  - the bronze sparrow tower: 3.9 MB
  - the camp: 3.8 MB
  - the smithy at level 2: 3.7 MB
  - the tally office: 3.7 MB
  - the stable at level 3: 3.7 MB
  - the smithy at level 1: 3.6 MB
  - the stable at level 2: 3.6 MB
  - the stable at level 1: 3.5 MB
  - the barracks at level 3: 3.4 MB
  - the granary: 3.4 MB
  - the farm at level 1: 3.3 MB
  - the farm at level 2: 3.3 MB
  - the market at level 3: 3.3 MB
  - the shipyard: 3.3 MB
  - the pass: 3.1 MB
  - the city: 3.0 MB
  - the port: 2.9 MB
  - the big city: 2.8 MB
  - the market at level 2: 2.8 MB
  - the farm at level 3: 2.8 MB
  - the barracks at level 1: 2.8 MB
  - the arrow tower: 2.8 MB
  - the market at level 1: 2.7 MB
  - the barracks at level 2: 2.4 MB
- The 30k model of the big city made the download approximately 10 MB
  smaller.

## The fly.io site
The fly.io app sanguodream serves the web game at https://sanguodream.fly.dev.
Any person with an account can sign up and play. The owner decided this on
2026-11-01. A visitor must sign in to get the game files.

A member is a user who signed in. A user signs in with one of these:
- Google, Apple, Facebook or Discord.
- A link in an email ("The sign-in with an email link").

The site has no passwords. The server stores no password of a user.

| File | Job |
|---|---|
| web/server.py | The server, with aiohttp. It sends the game files to members, with the .gz copy for a browser that accepts gzip. It also gives the save api. |
| web/auth.py | The sign-in with Google, Apple, Facebook and Discord, the state cookie and the session cookie. |
| web/email_link.py | The sign-in with an email link: the tokens, the limits, the email and the accounts of the addresses. |
| web/pages.py | The sign-in page, the pages of the email link and the members page. |
| web/storage.py | The records of the users and the saves, in an S3 bucket or in a local folder. |
| web/broadcast.py | The YouTube broadcast, an option. |
| web/game_files.py | index.wasm and index.data in the bucket, and their presigned addresses for the members |
| web/sw.js, web/boot.html | The service worker and its boot page: the splash while the machine starts ("The first second of a visit") |
| web/http_common.py | The keys of the application, the session of the user and the check of the origin, for server.py and games_api.py |
| web/games_api.py | The routes of the shared games: the lobby, the start image, the snapshots, the invite link and the socket |
| web/rooms.py | The rooms of the shared games: the seats, the order of the steps, the checksums and the log in the bucket (docs/MULTIPLAYER.md) |
| web/room_state.py | The format of a step, the limits and the checks of one room, for rooms.py |
| web/room_socket.py | The messages of the socket of a room and the loops of the flush and of the timers, for rooms.py |
| web/requirements.txt | The Python packages of the server, with exact versions. |
| Dockerfile | The image: Python 3.12, ffmpeg, the server and the five files of build/web. It does not compile the game. |
| .dockerignore | It lets only the server files and the web build into the context of the builder. |
| fly.toml | The app, the region iad, port 8080 and the health check /healthz. The machine stops when nobody uses it. |
| deploy_web.bat | It runs build_web.bat, then `flyctl deploy --remote-only --ha=false`. |

The secrets of the site. Set each one with
`flyctl secrets set NAME=value -a sanguodream`. Never write a secret in a
file of this repository.

| Secret | Job |
|---|---|
| SESSION_SECRET | The key of the cookies: a random text of 64 hex digits. The server does not start without it. |
| RESEND_API_KEY, EMAIL_FROM | The key of Resend and the sender of the emails, for example `hexdom <sign-in@example.com>`. Without them, the sign-in page has no email form. |
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

### The first second of a visit
A visitor sees the splash art and an icon that moves, as early as possible. The
machine stops after approximately 8 minutes without a visitor. On 2026-11-01
the log showed a start of 15 s: the VM needed 0.7 s, and the server needed
the rest. Without a change, the browser shows nothing in that time.

These parts make the wait short or visible:

| Part | Job |
|---|---|
| The build id in a file | The Dockerfile runs web/game_files.py, and that script writes /srv/web/build_id. The start of the server reads this file. It does not read 223 MB for a hash. |
| The start time in the log | The log has the line "server.py: the imports took ...". Read it after a cold start to find the next slow part. |
| The first paint | web/shell.html paints the splash first, and then makes the WebGL2 context of the device check. A timer also starts the work, because a hidden tab paints nothing. |
| The loading icon | A CSS animation of a transform moves the icon. The browser moves it on another thread, so the icon also moves while the page is busy with the game files. |
| The service worker | web/sw.js shows web/boot.html if the server does not give the page in 500 ms. |

The service worker runs after the first visit to the site. A first visit
during a cold start still shows nothing until the server answers. The flow of
a later visit has these steps:
1. The worker asks the server for `/` or `/login` and starts a timer of 500 ms.
2. If the page comes first, the browser gets that page without a change.
3. Else, or after an error or a status of 500 or more, the browser gets the boot page.
4. The boot page shows the same picture and icon. It asks /healthz until the server answers.
5. The boot page tells the worker that the server is awake, and opens the address again.
   For the next 30 s, the worker gives each answer of the server, also a slow answer.

The rules of the worker:
- The worker keeps only web/boot.html and web/splash.jpg. It never keeps a page of a member.
- The server still decides each page, the sign-in and each redirect.
- The worker reads only `/`, `/login` and the picture. It does not touch a sign-in callback, a form or the api.
- The worker removes only the old caches with the name "hexdom-boot-". The game files in "hexdom-game" stay.
- After a change to web/boot.html, change the number of BOOT_CACHE in web/sw.js.
- The page of a Discord Activity does not start the worker.

The sign-in page and the pages of the email link show the splash art. The
server sends web/splash.jpg, web/boot.html and web/sw.js without a session.
These three files hold no data of the game. The sign-in panel continues the
card of the art and hides the empty bar. The fractions in web/pages.py come
from the card in misc/make_splash.py. After a change to the card, change the fractions.

### The sign-in with an email link
The email form is the fourth option of the sign-in page. The flow has
these steps:
1. The user types an address.
2. The server makes a random token and keeps only the SHA-256 of the token,
   at `signin/<hash>.json`. The server sends the link through Resend.
3. The link opens a page with a button. A GET of the link does not use the
   token, because a mail scanner opens links.
4. The button posts the token. The server deletes the record of the token
   and gives the session cookie. The cookie is HttpOnly and SameSite=Lax,
   and it is Secure on the https site.

The rules of the link:
- A link works one time, for 15 minutes.
- The tokens stay in the bucket, because the machine stops when nobody uses
  it. The server deletes old tokens at most one time each hour.
- An address gets at most 5 emails each hour. After that limit, the reply
  does not change, and the server sends no email.
- An IP can send at most 20 requests each hour. After that limit, the server
  answers 429. The IP comes from the header Fly-Client-IP on fly.io.
- The limits stay in the memory of the machine, so a restart clears them.
- The reply is the same for a known address and a new address. It also does
  not change if Resend refuses the email.
- The log gets no token and no address. The access log is off. The log has
  the name of the user, and the default name is the part of the address
  before the "@". The other players of a room also see the name.
- The page of the link has `Referrer-Policy: same-origin` and
  `Cache-Control: no-store`.

### The accounts of the addresses
An account keeps its uid, and the saves and the seats use the uid.
`emails/<hash of the address>.json` gives the uid of the account of an address.

The first start of the new server does a one-time migration:
- The server reads each record in `users/`.
- A record goes into the index if the provider verified its email, or if
  the user typed the old house password. The old `member` field shows the second case.
- If several records have one address, the record with the last sign-in goes
  into the index. The server copies the saves of the other records into it.
  It overwrites no save, and the old copies stay.
- `emails/index-done.json` marks the end of the migration.

After the migration:
- A user who signs in with the same address gets the old account and the saves.
- A provider that verified the address finds the account of that address. This
  also applies to a user who came with an email link first.
- An email that nobody verified finds no account, and it goes into no index.
  Facebook does not verify. Another person can type any email at Facebook.
- An account with no email stays in the bucket with its saves. Its user
  signs in with the same provider as before. An email link makes a second,
  new account for that user.
- An Apple "Hide My Email" account has the relay address. The user keeps
  the account through Sign in with Apple.

### The setup of Resend
1. Make an account at https://resend.com.
2. In Domains, add a domain that you own. You cannot use fly.dev, because
   you cannot add DNS records to it.
3. Add the DNS records that Resend shows, at the DNS provider of the domain.
4. Wait until the domain shows "Verified".
5. In the settings of the domain, turn off the option that counts the clicks. It
   changes the link and sends the token through another site.
6. In API Keys, make a key with the permission "Sending access". Limit the key to the domain.
7. Set the two secrets. Replace the values:
   `flyctl secrets set RESEND_API_KEY=re_xxx "EMAIL_FROM=hexdom <sign-in@yourdomain>" -a sanguodream`
8. Run `deploy_web.bat`.
9. After the deploy, remove the old secrets. The old server cannot start without them, so do not do this first:
   `flyctl secrets unset HOUSE_PASSWORD SITE_PASSWORD -a sanguodream`

Open the sign-in page and ask for a link. The result is an email in less
than one minute. If no email comes, `flyctl logs -a sanguodream` shows
"email: the server sent no sign-in link" and the status of Resend.

### The members page
An admin opens https://sanguodream.fly.dev/admin. The page shows each user
who signed in, with these columns:
- the name, the provider and the email
- the role: admin or member
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
| GET /api/me | The user: uid, name, provider, admin, and can_broadcast. |
| POST /api/client-error | The page sends the reason when the game stops. The server writes it to the log with the browser name. |
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

### The user badge and the sign-out
The page shows a badge with the name of the user in the upper right
corner. A click on the badge opens a menu:
- the provider of the sign-in
- "Members", for an admin only
- "Sign out"

A web page cannot close its window. Thus 結束遊戲 of the game signs the
user out on the web:
1. The game sets quit_requested of game_memory for one frame.
2. wasm_hexdom.cpp calls Module.hexdomQuit of the page.
3. The page asks the user. If the user agrees, the page signs out and
   opens the sign-in page.

The user loses the progress after the last save. 返回開頭畫面 in the 設定 menu
of the HUD goes back to the title without a sign-out.

### The requirements of the device
The page checks the device before it downloads the game:
- The browser must have WebAssembly and WebGL2.
- The device must have a mouse. A phone with only a touch screen fails
  this check.

If a check fails, the page shows the requirements and does not load the
game. The user can click "Try anyway". If the game stops later, for
example when the memory is not sufficient, the page shows the same
requirements with the reason. The page also sends the reason to
/api/client-error. To read the reasons, run
`flyctl logs -a sanguodream` and find the lines that start with "client:".

The game script is in a template element, so it does not run by itself.
After the checks, the page loads it.

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

### The game files in the bucket
The two big files of the game come from the bucket of the saves, not from
the machine. Tigris has no fee for the traffic out (tigrisdata.com/pricing,
2026-10). Thus the download costs nothing at fly.io. Only members get the
files. web/game_files.py does the work.

The files in the bucket are index.wasm and index.data, approximately 46 MB
together. index.html and index.js stay on the server. index.html needs the
sign-in, so a person without a session never sees the page.

#### The start of the server
1. The server reads the build id from the file build_id. The Dockerfile
   writes this file. Without the file, the server computes the id. The
   build id is the first 16 hex digits of the SHA-256 of index.wasm and
   index.data.
2. The server puts the gzip copy of each file in the bucket at
   `builds/<build id>/<name>`, if the bucket does not have it. Each object
   has its `Content-Type`, `Content-Encoding: gzip` and the Cache-Control
   `private, max-age=31536000, immutable`.
3. The server sets the CORS rule of the bucket. The rule lets the pages
   of the site read the bucket with GET and HEAD.
4. The server keeps the last five builds in the bucket and removes the
   older builds. A page that started before a deploy keeps its files.

The server does these steps in the background, so the health check
answers without a wait. Until the steps end, and after an error, the server
sends the files itself, as before. The log shows the result:
`game_files: the members get build <id> from the bucket`.

Tigris can answer 408 to one request. On 2026-10-28, the first deploy got
408 for a HeadObject after the upload of index.wasm. Thus the server tries
the steps four times, after waits of 10, 30 and 90 seconds. Each try
starts again from step 2 and skips the files that the bucket already has.

The upload uses the keys of the bucket that the server already has. Thus
a deploy needs no key on the computer of the developer, and
deploy_web.bat does not change. The image still holds the two files. The
server needs them for the upload and for the fallback.

#### Only members get the files
1. The bucket stays private. It has no public address. A request
   without a signature gets 403.
2. The page asks the server for the addresses: GET /api/game-files. The
   server calls require_member, as for each api. A person who is not a
   member gets 401 and no address.
3. For a member, the server gives a presigned address for each file. The
   address holds a signature from the keys of the server and an end time
   of 10 minutes. The signature covers the key of one file.
4. The browser downloads each file directly from the bucket. A download
   that starts before the end time continues after it.

A member can give an address to another person. The address then works
until its end time. A member can also copy the files from the browser.
Thus the bucket gives the same protection as the server.

#### The cache in the browser
A presigned address changes at each request. Thus the HTTP cache of the
browser does not keep the files between two visits. web/shell.html keeps
them in the Cache Storage of the browser, under the build id:
1. If the Cache Storage has the files of the build id, the page uses
   them and downloads nothing.
2. Else, the page downloads the files and shows the percent. It puts the
   files in the Cache Storage and removes the files of the other builds.
3. If the file from the bucket fails or has the wrong size, the page
   downloads the copy of the server.
4. The page gives the bytes to the game through Module.instantiateWasm
   and Module.getPreloadedPackage.

If the bucket sends the gzip bytes without `Content-Encoding`, the page
unpacks them with DecompressionStream. Without the api, for example with
`python -m http.server`, the game downloads its files itself.

The answer of /api/game-files:

| Field | Contents |
|---|---|
| build | the build id |
| source | "bucket" or "server" |
| expires | the end time of the addresses, or null for the files of the server |
| files | for index.wasm and index.data: url, fallback (the file of the server) and size |

To check the bucket after a deploy, run
`flyctl logs -a sanguodream` and find the lines that start with
"game_files:". In the browser, the console shows
`platform: the game files of build <id> come from the bucket`.

### The browser tests
tests/web has seven Playwright smoke tests (smoke.spec.js):
- a user signs in and sees the title menu
- the title menu loads a save from the server
- the city list moves the camera between cities
- a unit moves beside an enemy base and attacks it
- the system menu goes back to the title
- the user badge shows the name and signs the user out
- the quit button of the title asks, then signs the user out

email_link.spec.js signs in with an email link. The config sets
EMAIL_OUTBOX, so the server writes the link to a file in place of an email.
The server ignores EMAIL_OUTBOX on fly.io.

device.spec.js emulates a phone. The phone sees the requirements page,
and the page does not download the game.

admin.spec.js has two tests of the members page. The "dev" provider gives
the email `<name>@dev.test`, and the config sets ADMIN_EMAILS to
tester@dev.test:
- the admin "Tester" sees the table
- the member "Guest" gets 403

game_files.spec.js has three tests of the game files:
- a request without a member session to /api/game-files gets 401
- a member gets the build id and the addresses of the server
- a second load of the page takes the files from the Cache Storage and
  downloads nothing

boot.spec.js has five tests of the first second of a visit:
- the sign-in page shows the splash art, and the panel hides the empty bar
- the page shows the splash and the icon that moves, before the game files
- the page paints before the check of the device
- the worker shows the boot page while the site does not answer, and then opens the real page
- the worker does not touch a sign-in or a form

rooms.spec.js has three tests of the rooms of the shared games
(docs/MULTIPLAYER.md, MP2):
- the socket of a room refuses a request without a member session (401)
- the socket refuses a page of another origin (403)
- a member makes a room, takes a seat, starts the game and gets the
  welcome on the socket

multiplayer.spec.js has one test of a game together (docs/MULTIPLAYER.md,
MP3). Two browser contexts sign in as "Tester" and "Guest". The host
clicks "Play online", and the guest opens the invite link. Each takes a
seat, and the host starts the game. Each player ends a turn of a faction.
After each step, the two pages have the same steps and the same checksum.

desync.spec.js has one test of a desync (docs/MULTIPLAYER.md, MP6). The page
that does not act sends one false checksum. The test checks that the page loads
the good state, that the checksums agree, and that the game goes on. It needs
build/referee.js.

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
| Renderer scratch | 64 | 15.0 |

The frame block holds the terrain mesh of the 200 x 200 map. The renderer
scratch holds the file of one model while the renderer loads it. The peak
of the renderer scratch is from a game on 2026-10-27, after the 30k model
of the big city. Measure the peak again after a
long game before you set the sizes of the web build. The target for the
web build is less than 512 MB.
