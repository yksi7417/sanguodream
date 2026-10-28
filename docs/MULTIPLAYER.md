# Multiplayer

This document gives the plan for games that more than one player plays
together on the fly.io site. It also records the facts of the current
site that the plan starts from. Nothing in this plan exists in code yet.

## The current site: one copy of the game for each browser
Each browser runs its own copy of the game. Two browsers never share a
game, also with the same sign-in. These facts of the code show it:
- code/wasm_hexdom.cpp includes hexdom.cpp. Thus the sim, the engine and
  the UI run inside the page. The browser calls GameUpdateAndRender for
  each frame, and no state goes to the server.
- web/server.py sends four static files to a member. Its api reads and
  writes save files at `saves/<uid>/<name>` (web/storage.py). The server
  has no game state and no game rules.
- The only WebSocket is /api/broadcast (web/broadcast.py). It carries
  video from one page to YouTube, in one direction.
- web/shell.html reads the saves of the user before the start. After each
  save, it sends the file to the server.

Thus two users play two separate games. One user in two browsers also
plays two separate games. Those two copies share the list of saves. If
the two copies write the same slot, the last upload replaces the first
upload.

## The goal
A member can start a new game with friends from the site:
1. The host makes a game and gets an invite link and a room code.
2. Each friend opens the link and chooses a faction.
3. The host starts the game. Each player then commands only the faction
   of the player. The AI commands the other factions.
4. The game stays on the server. The players can stop and continue it on
   another day.

## The architecture: a relay of commands with a check of the state
The plan uses a lockstep of commands, not a sim on the server. Each
browser keeps its own sim. The server keeps the one order of the steps
and sends each step to each player.

### The facts that make this choice possible
- The sim is deterministic. It has a seeded PRNG in sim_state, and it
  uses only integer types. The same start state and the same steps
  give the same state on each computer.
- The game already has a record of steps (docs/REPLAY.md). A step is a
  command through engine_run, a change of the settings, or a change of
  the human factions. The record has a checksum of the state after each
  step.
- The game is turn-based by faction. In each turn, only one faction acts
  at a time (sim_whose_turn). The AI factions act inside the end_turn
  step of a human player (code/sim/turn.cpp). Thus only one player sends
  steps at a time, and the steps of two players never collide.
- The sim refuses a command that breaks a rule, for example
  SIM_REFUSAL_NOT_YOUR_TURN. A refused command is no step. Each copy of
  the sim makes the same decision.

### The comparison of the two options
| Point | Lockstep of commands (the choice) | Sim on the server |
|---|---|---|
| The work on the server | A relay in Python, with rooms and a log | A headless build of the engine for each game on the server |
| The memory of the machine | Small. 512 MB is sufficient for many rooms. | Approximately 100 MB for each running game |
| The traffic | Small. A step is less than 1 KB. | The server must send the change of the state after each step |
| The feel of a command | The acting player sees the result at once | Each command waits for the server |
| Hidden information | Each browser has the full state | The server can hide the state of the other factions |
| A desync | Possible after a bug. The checksums find it. | Not possible |
| The fit with the code | The record and the checksum of docs/REPLAY.md exist | A new protocol for the state |

The lockstep gives the most value for the least work. Its two weak
points are the desync and the hidden information. The plan answers the
desync with the checksums (see "The check of the state"). A sim on the
server comes later as a referee, only when the server must decide (MP6).

### The author of a step
The engine of the player who acts is the author of each step of that
player:
1. The player gives a command in the UI. The UI calls engine_api as
   before.
2. The engine runs the command directly, as before. The UI gets the
   engine_command_result in the same call.
3. If the command makes a step, the engine puts the step in a queue for
   the network.
4. The platform layer sends the step to the server.
5. The server gives the step the next number and stores it.
6. The server sends the step to each other player.
7. Each other engine applies the step and compares its checksum.

The UI contract does not change. A front end still issues a command and
reads the result in the same call. Only the seat that acts can send a command. Thus
the local result is the result for all players.

The server is the author of a small set of steps. Two examples are the
end of a faction turn after a timeout and an AI takeover. These steps
are set_faction_human and end_turn. Each engine applies them as it
applies a step of a player.

### The parts of the system
| Part | File | Job |
|---|---|---|
| The step codec | code/sim/sim_wire.cpp (new) | Writes a step to bytes and reads it back. It has no I/O. |
| The multiplayer mode of the engine | code/engine/engine_multiplayer.cpp (new) | The seats, the queue of outgoing steps, the apply of a remote step, the lock of the setters |
| The net bridge of the platform | code/wasm_hexdom.cpp, web/shell.html | The WebSocket in the page. It moves the bytes between the socket and the game. |
| The panels | code/ui/ui_multiplayer.cpp (new) | The banner of the acting player, the timer, the seats, the chat. They call engine_api only. |
| The room server | web/rooms.py (new) | The rooms, the seats, the log of steps, the timers, the checksums |
| The lobby pages | web/pages.py, web/lobby.html (new) | Make a game, the invite link, the choice of factions, the list of games |
| The storage | web/storage.py | The keys of the shared games in the bucket |

The sim does not change for MP1 to MP5. The codec in code/sim reads and
writes plain bytes, as save_format.cpp does.

## Turns, seats and the start of a game together

### The turn order
The game keeps the turn order of the original. In each turn, the
factions act one after the other. When a human faction acts, the other
players watch. This mode is "hot-seat over the network". The local
hot-seat already exists: set_faction_human can mark more than one
faction as human.

The plan does not have simultaneous turns in the first milestones. They
need a change of the rules in code/sim. With them, two factions can
command the same city or the same hex at the same time. The rules of the
original have no answer for that. The question is open for the user
(Q-5).

A player who waits can do these things:
- read the map, the cities, the officers and the log;
- see each command of the player who acts when it arrives;
- write in the chat;
- open the replay panel.

A player who waits cannot give a command. The engine refuses the command
with a new reason, ENGINE_REFUSAL_NOT_YOUR_SEAT.

### Two modes of a game
| Mode | Time of a faction turn | Use |
|---|---|---|
| Live | 2 to 10 minutes, the host sets it | All players are online at the same time |
| Correspondence | 12 to 72 hours, the host sets it | Each player acts on a day that suits the player |

The room server keeps the deadline of each faction turn in the room
record. Thus a timer stays correct after a stop of the machine. The
server checks the deadlines at each start and at each request.

### The steps to start a new game together
1. The host clicks "New online game" on the site.
2. The host chooses the scenario, the mode, the time of a turn and the
   number of seats.
3. The game of the host loads the scenario. It then sends the list of
   factions to the server: the force id, the name and the colour of each.
4. The server makes a room. It gives the room an id, an invite link
   `https://sanguodream.fly.dev/r/<code>` and a room code of six
   letters. The code uses no letter that looks like a digit.
5. A friend opens the link. If the friend is not a member, the sign-in
   and the house password come first. The server then sends the friend
   back to the room. auth.new_state already keeps a return path.
6. Each player clicks a free faction to take its seat. The host can also
   set a seat to the AI, or remove a player from a seat.
7. Each player clicks "Ready". The host clicks "Start" after all players
   are ready.
8. The server picks a random seed for the game.
9. The game of the host makes the start image: the scenario, the seed,
   the human factions and the settings. It sends the image to the server.
10. Each player loads the start image. Each engine sends the checksum of
    the start state. If all checksums agree, turn 1 starts.

A member can also enter a room code on the title menu of the game. A
list on the site shows the open rooms of the house and the games of the
member. The list shows "Your turn" for each game where the faction of the
member acts.

## Absent players and the AI takeover
The room server knows the connection of each seat. These rules apply:
- A player who loses the connection has 60 seconds to connect again. The
  timer of the faction turn does not stop in that time.
- If the timer of a faction turn ends, the server sends the step end_turn
  for that faction. The commands of that faction in that turn stay.
- If a player misses two faction turns in a row, the server sends the
  step set_faction_human with the value false. The AI then commands that
  faction.
- When the player connects again, the server sends set_faction_human
  with the value true. The step takes effect before the next faction
  turn of that faction.
- The host can choose "Pause when a player is absent" in place of the AI
  takeover. The timers then stop until all players connect.
- A player can leave a game. The seat then goes to the AI for the rest
  of the game, or to a new player that the host invites.

The engine must let the AI finish a faction turn of a faction that is no
longer human. A feature test of MP4 checks this.

## The saves of a shared game
A shared game has one copy on the server. No player owns it. The bucket
holds these keys:

| Key | Contents |
|---|---|
| `games/<gid>/room.json` | The room: the name, the host, the scenario, the pack, the build id, the seats, the mode, the deadlines, the state of the room, the number of the last step |
| `games/<gid>/start.hxs` | The start image of the game, in the format of a save slot |
| `games/<gid>/snap/<step>.hxs` | A snapshot: a save file with its record (docs/REPLAY.md) |
| `games/<gid>/log/<first>-<last>.bin` | A part of the log of steps, after the last snapshot |
| `members/<uid>/games/<gid>` | An empty file. It lists the games of a member without a scan of all rooms. |

The rules of the saves:
- The server keeps the log of steps in memory. It writes a part of the log
  to the bucket at each end_turn step and at least each 30 seconds.
- At the end of each game turn, the author of the last step sends a
  snapshot. The server accepts it only if its checksum is the checksum
  that all players agreed on for that step. The server keeps the last
  five snapshots.
- A player who joins or connects again gets the last snapshot and the
  log after it. The engine loads the snapshot, applies the log and
  compares the checksum.
- The save menu of the game writes a snapshot in a shared game. The load
  menu is off in a shared game.
- The local saves of a member (`saves/<uid>/`) do not change. A shared
  game never writes a local slot.

### A new release of the game
The record of an old build can give another state in a new build
(docs/REPLAY.md). Thus each room records the build id: the hash of
index.wasm. These rules apply after a deploy:
1. A client with another build id cannot send a step to a game that runs.
2. The server starts the room again from its last snapshot. The new build
   loads the snapshot and continues. The log before the snapshot does not
   run again.
3. If the save format of the snapshot changed, the room stops with a
   message. The tests of the save format already guard against this
   change.

## The check of the state
Each engine sends the checksum of its state after each step. The
checksum is engine_game_replay_checksum, the checksum of the record of
docs/REPLAY.md. The server compares the checksums of all players for each
step.

If one checksum differs, the game has a desync. The server then does
these steps:
1. The server stops the game for all players and shows a message.
2. The server asks each player to send the state image after the step.
   The server keeps these images at `games/<gid>/desync/<step>-<uid>.hxs`
   for the developer.
3. With three players or more, the majority of the checksums wins. The
   other players load the last good snapshot and the log again.
4. With two players, the server cannot know the correct state. Before
   MP6, the state of the author of the step wins. After MP6, the referee
   on the server decides (see "The referee").
5. The game continues from the step after the desync.

A desync is a bug in the determinism or a sign of a cheat. Each desync
gets a play report and a feature test that plays the stored steps.

These rules keep the copies the same:
- All players of a room run the same build and the same content pack.
- In a shared game, the engine refuses each setter outside the steps,
  for example set_city_gold and the editor. A setter changes the state
  without a step (docs/REPLAY.md, "edited").
- The debug panels that change the state are off in a shared game.
- Only the host can change the settings, and each change is a step.

## Sign-in and protection against a cheat
The plan uses the sign-in of the site (web/auth.py) without a change.
- The WebSocket of a room needs a member, as /api/broadcast does. The
  server checks the session cookie and the origin of the page.
- The server knows the uid of each socket. It takes a step from a socket
  only if the seat of that uid is the faction that acts now.
- The server takes a step only if its number is the next number. A copy
  of an old step does nothing.
- Each player engine applies each step with the rules of the sim. A
  forged command that breaks a rule is a refused command on each
  computer. Thus a cheat can change only the copy of the cheater. The
  checksums then find the desync.
- A limit applies to each socket: 64 KB for each message and 20 steps
  each second. The server closes a socket that goes past a limit.
- The chat text goes through the same cleanup as the names of the users.
  The page shows chat text as text, never as HTML.

The lockstep cannot hide information. Each browser has the full state of
the game. A player with the developer tools can read the cities and the
armies of the other factions. For the house of friends, the plan accepts
this limit. A sim on the server can remove it later (Q-3).

## The referee
The referee is a headless build of the engine for the server. It is a
later part of the plan (MP6), not a part of the first game together.
- `build_web.bat referee` builds it with the same toolchain as the tests
  of `run_tests.bat --web`. Node runs it.
- server.py starts it as a child process, as broadcast.py starts ffmpeg.
  The referee reads steps on stdin and writes checksums on stdout.
- The referee runs only when the server needs a decision: a desync with
  two players, or a check of a snapshot.
- After the run, the process stops. Thus the memory of the machine stays
  small.

## The transport
The transport is a WebSocket at `/api/games/<gid>/socket`. aiohttp
already serves the WebSocket of the broadcast.

| Message | From | Contents |
|---|---|---|
| hello | client | the game id, the build id, the number of the last step that the client has |
| welcome | server | the seat, the room, and the steps that the client does not have |
| step | client or server | the number of the step and the bytes of the step |
| checksum | client | the number of the step and the checksum after it |
| presence | server | the seats that have a connection |
| timer | server | the faction that acts and its deadline |
| chat | client or server | the uid, the name and the text |
| desync | server | the step and the request for the state image |
| error | server | the reason, for example "not your seat" or "old build" |

The rules of the transport:
- Control messages are JSON. A step is a binary message with a header
  of 8 bytes and the bytes of the codec.
- The step codec writes each field of sim_command in a fixed order, as
  little-endian integers. It does not copy the struct. Thus a later
  Windows client can use the same bytes as the web client.
- aiohttp sends a ping each 20 seconds. The proxy of fly.io closes an
  idle connection after 60 seconds.
- After a lost connection, the page tries again after 1, 2, 4 and 8
  seconds, then each 15 seconds. The hello message gives the last step,
  so the client gets only the steps after it.
- The start image and the snapshots go through HTTP, not through the
  socket: `PUT /api/games/<gid>/snap/<step>` and
  `GET /api/games/<gid>/snap/latest`. The limit is MAX_SAVE_BYTES.

## The host on fly.io
The app sanguodream serves the rooms. The plan needs these changes:
- The rooms are in the memory of one machine. deploy_web.bat already
  deploys with `--ha=false`, so the app has one machine. The plan keeps
  this rule. A second machine needs a route for each room, for example
  the fly-replay header.
- fly.toml keeps `auto_stop_machines = 'stop'`. An open WebSocket keeps
  the machine on. When no socket is open, the machine can stop. The
  deadlines in room.json then wait for the next start.
- A correspondence game needs no machine between two faction turns. The
  server applies the expired deadlines of a room when the next player
  connects.
- The VM keeps 512 MB for MP1 to MP5. The referee of MP6 can need 1 GB.
- The Dockerfile copies the new files of the server: rooms.py and
  lobby.html. The referee of MP6 also needs Node in the image.
- A deploy stops each socket. Each page connects again by itself. The
  log in the bucket is at most 30 seconds old, and the clients have the
  newer steps. The first client to connect sends those steps again.

## Other options for the relay
A managed service can carry the messages in place of the relay. This
section compares the options by the work that stays for the owner. The
relay of this plan has four jobs:
1. The order. The relay gives each step one number. Each player applies
   the steps in that order.
2. The seat check. Only the seat that acts can send a step.
3. The log. A player who joins later gets the steps after the last
   snapshot.
4. The timers. After a deadline, the relay sends end_turn and
   set_faction_human.

The order is the hard job. Two senders can send at the same moment, for
example the player who acts and the timer of the deadline. A broker
without one order can then deliver the two steps in two orders to two
players. The two engines then have a desync.

| Option | The order | The seat check | The log | The timers | What stays on fly.io | The cost (2026-10) |
|---|---|---|---|---|---|---|
| The own relay in server.py (the choice) | Yes: one room in one process | Yes | The bucket | Yes | The site as now, with one more module | The current machine |
| EMQX Serverless: MQTT over WebSocket | Only for each topic. No number for each step. | A fixed ACL for each topic. It does not know whose turn it is. | A retained message keeps only the last message of a topic | No | The sign-in, the saves, the files, the accounts of the broker, the timers and the log | Free for 1 million session minutes, 1 GB and 1 million rule actions each month, with up to 1000 connections. With a spend limit of 0, the deployment stops at the limit. |
| Cloudflare Durable Objects | Yes: one object for each room, with one thread | Yes, in our code | The SQLite store of the object | Yes: the alarms | The sign-in and the saves, or nothing after a full move | A free plan with daily limits. A socket that sleeps costs no time. |
| Ably, Pusher or PubNub | For each channel, without a check | The capabilities of a token, fixed | A history with limits | No | The sign-in, the saves, the timers and the checks | Free plans with limits on the messages |
| Supabase Realtime with Postgres | Yes: a unique key on the number of the step | The rules of the rows | A table | pg_cron | The sign-in and the saves | A free plan. A free project pauses after a week without use. |
| WebRTC between the browsers, with the browser of the host as the relay | Yes | Yes | Only while the host is online | Yes | A server for the signals | No relay traffic. No correspondence game. Discord has no WebRTC in an Activity. |

The facts of EMQX Serverless:
- The EMQX Cloud documents say that a Serverless deployment has no JWT
  authentication. Thus the fly.io server must make a user name and a
  password in the broker for each player, through the api of EMQX.
- The broker gives no number to a step and keeps no deadline. Thus the
  fly.io server keeps the order, the timers and the log.
- EMQX then removes only the socket code of rooms.py. It adds a second
  system, a second set of secrets and a quota that can stop a game.

The decision for MP2: the plan keeps the own relay. The site already has
the sign-in, the bucket and a WebSocket. The relay adds one module, not
one machine. fly.io keeps the TLS, the restarts and the health check.
The relay has no database and no second process.

These rules keep a later move open:
- web/rooms.py holds the rules of a room without aiohttp. Its tests do
  not need a socket. A port to a Durable Object is then a small job.
- The engine also checks the seat of each remote step. step_apply
  refuses a step from a seat that does not act. Thus the copies stay the
  same on a broker without a seat check.
- Durable Objects are the best fit for a later move (Q-12). Each room is
  one object with one thread, a store and alarms.

## Discord as the social layer
Discord can give the voice, the video, the chat and the invites. It
cannot be the relay. These facts come from the Discord documents for
Activities:
- An Activity is a web page in a voice channel of Discord. The players
  talk, see video and write in Discord while they play.
- The Embedded App SDK gives the identity of the user and the
  participants of the channel. It gives no network for the game.
- Discord sends each request of an Activity through its proxy. In the
  developer portal, the owner maps a path prefix to a site, for example
  /api to sanguodream.fly.dev. The proxy also carries a WebSocket.
- A developer forum reports that an Activity cannot use WebRTC.

Thus the relay stays on fly.io, and Discord reaches it through the
proxy. A bot that posts the steps as messages of a channel is not a
relay. Discord limits the rate of the messages of a bot, and each step
then shows in the channel.

The plan for Discord (MP8):
1. Make a Discord app with an Activity. Install it only in the Discord
   server of the house. Do not list it in the App Directory, because the
   data of the original must not be public.
2. Add "discord" as a provider to web/auth.py. In an Activity, the SDK
   gives a code. The server changes the code for the identity of the
   Discord user.
3. In an Activity, the page sends a signed token in the hello message in
   place of the session cookie. The token has the format of the cookies
   and the key SESSION_SECRET.
4. The voice channel is the lobby. The Activity makes one room for the
   channel, and each participant takes a seat.
5. Inside Discord, the voice and the chat of Discord replace the chat of
   MP7. The site outside Discord keeps its own chat.
6. The house password stays. A Discord user who is not a member types
   it one time.

## The content and the licence
The code of hexdom is clean-room. The data of some packs is not:
- The placeholder pack and the install reader of M16 use data of the
  original game. This data has the scenarios, the officers, the names
  and the map.
- Koei Tecmo owns this data. The name 三國志 is a trademark.
- Thus the site stays for members only (docs/WEB.md). A public site, a
  public Activity or a sale needs a game without the data of the
  original.
- The start image and the snapshots of a shared game hold the state of
  the game. With the data of the original, they hold that data. Thus a
  public server must not keep them for a game with that data.

This document is not legal advice. Ask a lawyer for intellectual property
before a public release.

The path to a public game:
1. Make an original pack for the Three Kingdoms. It has its own officers,
   its own values, its own portraits (docs/art/GEMINI-PROMPTS.md) and a
   map from real geography (docs/MAPS.md). The history of the period and
   the old text of the novel 三國演義 are in the public domain. A modern
   translation is not.
2. Give the game a name that is not 三國志.
3. Keep the install reader of M16 for a player who owns the original.
   The data then stays on the computer of that player, in a game for one
   player.
4. Check the licence of the Dear ImGui Test Engine before a commercial
   use (CLAUDE.md). Only the test builds use it.

## The cost of the site and a free game
The relay does not run the sim, so the server has little work. The main
cost is the download of the game. A new player downloads approximately
36 MB (docs/WEB.md). The table gives estimates from public price pages
of 2026-10. Check them before a decision.

| Item | Estimate |
|---|---|
| The fly.io machine, a shared CPU with 512 MB | A few US dollars each month |
| The traffic out of fly.io in North America and Europe | Approximately 0.02 USD for each GB |
| 10,000 new players in a month, with the files from fly.io | Approximately 360 GB, or approximately 7 USD |
| The same download from the bucket, Tigris or R2 | No cost for the traffic |
| The saves and the logs of the shared games | Small. Tigris gives 5 GB each month without cost. |

Thus the game can stay free for a long time. docs/WEB.md, "The game
files in the bucket", gives the plan that moves the download out of the
machine.

If the cost grows, these ways keep the game free and fair:
- donations, with a badge for a supporter;
- items for the look only, for example flags, frames of portraits and
  skins of units. An item never gives power in the game. US-042 of the
  wish list is a joke;
- paid content packs, for example new eras, maps and campaigns. The base
  game stays free;
- a paid desktop version with its own content, for example on Steam;
- purchases inside a Discord Activity. Check the terms and the share of
  Discord first.

Each way needs a game without the data of the original. See "The
content and the licence".

## The milestones
Each milestone ends with its tests green in `run_tests.bat`,
`run_tests.bat --web` and `npm test` in tests/web. The C++ tests follow
.claude/rules/testing-pyramid.md.

### MP1 The stream of steps in the engine
The engine can give its steps to another engine and apply the steps of
another engine. No network exists yet.
- The step codec in code/sim/sim_wire.cpp.
- engine_api gets these functions: multiplayer_begin (the seats and the
  local seat), steps_outgoing, step_apply, state_checksum and set_seed.
- The new refusal ENGINE_REFUSAL_NOT_YOUR_SEAT. The lock of the setters
  in a shared game.

The tests:
- Unit (tests/unit/wire_tests.cpp): a round trip of each kind of
  sim_command through the codec. A short buffer gives an error and no
  step.
- Feature (tests/feature/multiplayer_tests.cpp): two engines in one test
  run. Engine A plays a scenario through engine_api. Engine B applies
  the steps of A. The checksums agree after each step.
- Feature: a refused command puts no step in the queue for the network. A command of a seat
  that does not act gets ENGINE_REFUSAL_NOT_YOUR_SEAT. A setter in a
  shared game changes nothing.
- Feature: a record of 30 turns gives one fixed final checksum. The same
  test runs native and under node. Thus Windows and the browser agree.
- UI (tests/ui/ui_tests.cpp): when the local seat does not act, the city
  panel issues no command (recorder_count_commands is 0).

### MP2 The room server and the transport
The server keeps rooms, seats and the log, and it relays steps. A
scripted client takes the place of the game.
- web/rooms.py: the room, the seats, the order of the steps, the
  comparison of the checksums.
- The WebSocket `/api/games/<gid>/socket` and the messages of the table.
- The keys of storage.py for `games/` and `members/`.

The tests:
- The pyramid has no layer for the Python server yet. MP2 adds
  tests/server/rooms_tests.py, with unittest and FolderStorage. Add a
  line for this layer to .claude/rules/testing-pyramid.md in the same
  commit.
- Server: a step from the wrong seat gets an error. A step with an old
  number does nothing. Two different checksums for one step give a
  desync message.
- Server: a stop and a start of the server keeps the room and the log.
- Browser (tests/web/rooms.spec.js): a socket without a member session
  gets 401. A socket from another origin gets 403.

### MP3 Start a new game together
Two members start one game from an invite link and play it.
- The lobby page, the invite link, the room code, the choice of
  factions, Ready and Start.
- The net bridge in wasm_hexdom.cpp and shell.html.
- The panels: the banner "<name> acts", the seats with their colours,
  the end of the wait.

The tests:
- UI: the banner shows the name of the player who acts. The Start button
  of a guest issues no command.
- Browser (tests/web/multiplayer.spec.js): two browser contexts with two
  "dev" users. The host makes a room, and the guest joins through the
  link. The guest takes a faction, and the host starts. The host moves a
  unit, and the guest sees the move. The test hook gives the checksum of
  each page, and the two checksums agree.
- Demo: "demo/mp3_game_together". The window plays a recorded stream of
  steps from a second seat. It shows the banner and the moves of the
  other player.

### MP4 Timers, absence and the AI takeover
- The deadlines of the faction turns, the two modes, the pause option.
- The connection again after a loss, with the steps after the last step.
- The server steps end_turn and set_faction_human.

The tests:
- Feature: set_faction_human false in the faction turn of a faction lets
  the AI finish that faction turn. The checksums of two engines agree.
- Server: a deadline in the past at the start of the server gives one
  end_turn step, not two.
- Browser: the guest closes the page. After the timer, the AI commands
  the faction of the guest. The guest opens the page again and gets the
  faction back.

### MP5 Shared saves and the list of games
- The snapshots, the resume from a snapshot and the log.
- The list "My games" with "Your turn", and the room code on the title
  menu.
- The rules for a new release (the build id).

The tests:
- Feature: a snapshot plus the log after it gives the checksum of the
  live game.
- Server: a snapshot with a wrong checksum gets an error. The server
  keeps five snapshots.
- Browser: two players stop a game. On the next day of the test clock,
  both players open "My games" and continue the game from the same step.

### MP6 The recovery from a desync and the referee
- The stop, the state images, the majority, the load of the good state.
- The headless referee and its use for two players.

The tests:
- Feature: an engine with one changed city gives another checksum at
  the next step.
- Server: a desync with three players gives the majority state to the
  third player.
- Server: the referee replays a stored log and gives the same checksum
  as the engines.

### MP7 The social part
- The chat in the game (US-040 of docs/requirements/community-wishlist.md).
- Spectators. A spectator gets the steps and gives no command. The
  replay panel already shows a shared record (docs/REPLAY.md,
  "Multiplayer").
- The notice "Your turn" for a correspondence game. The way to send it
  is open (Q-6).
- The rules of a room (US-039), for example a limit on alliances.

The tests:
- UI: the chat panel sends the text through engine_api or the platform,
  and it issues no game command.
- Browser: a spectator sees the moves and cannot take a seat while the
  game runs.

### MP8 The Discord Activity
- The Discord app, its Activity and the map of the proxy to the site.
- The provider "discord" in web/auth.py and the signed token of the
  hello message.
- One room for each voice channel.

The tests:
- Server: the provider "discord" with a fake token endpoint gives the
  record of the user. A hello with a bad token gets 401.
- Browser: a page with a token in place of the cookie joins a room and
  gets the steps.
- A browser test cannot run inside Discord. Thus a member checks the
  Activity in a test server of Discord and writes the result in the play
  reports.

## The open questions for the user
| Id | Question | The default of the plan |
|---|---|---|
| Q-1 | How many players can share one game? | 2 to 8 human seats |
| Q-2 | Which mode comes first, live or correspondence? | Live in MP3, correspondence in MP4 |
| Q-3 | Can a member who reads the browser memory see the hidden state of other factions? | Yes, for the house of friends |
| Q-4 | When a player is absent, does the AI take the faction, or does the game pause? | The AI, after two missed faction turns. The host can choose the pause. |
| Q-5 | Do you want simultaneous turns, with a change of the rules of the original? | No. The factions act in the order of the original. |
| Q-6 | How does a player learn that it is the turn of the player? Email needs a mail service and a secret. | A badge on the site only |
| Q-7 | Can the players load an old snapshot of a shared game? | Only if all players agree in the room |
| Q-8 | A duel or a debate between two human officers: do both players play the duel, or does the AI play the side of the other faction? | The AI plays the side of the other faction |
| Q-9 | Can the Windows build join an online game? | Not before MP7. The codec already permits it. |
| Q-10 | Can each member of the house see and join each open room, or only a player with the link? | Each member sees the open rooms |
| Q-11 | Can a spectator watch a game, and does the host decide? | Yes, and the host decides |
| Q-12 | Do you want to move the relay to Cloudflare Durable Objects later? | No. The relay stays on fly.io until the machine is not sufficient. |
| Q-13 | Do you want the Discord Activity, and in which Discord server? | Yes, in MP8, only in the Discord server of the house |
| Q-14 | Do you want a public release? It needs an original Three Kingdoms pack and a new name. | No. The site stays for members until a lawyer agrees. |
| Q-15 | If the cost grows, which way to pay for the site do you prefer? | Donations first |
