# Sanguo Dream

Sanguo Dream is a grand-strategy game on a hex map. You rule a faction,
develop your cities, recruit officers and lead armies to unify the land.
The hexdom engine runs the game. The engine is in C++, and it also runs in
the browser through WebAssembly.

## Play now

**[Play in your browser at sanguodream.fly.dev](https://sanguodream.fly.dev)**

1. Open https://sanguodream.fly.dev.
2. Sign in with Google, Apple, Facebook, Discord or a link in an email.
   The site has no passwords.
3. Select a scenario and play.

Notes:
- The first visit after a quiet period can take approximately 15 seconds.
  The server sleeps when nobody plays, and the splash screen shows while it
  starts.
- The first load downloads the game files. Later visits use the cache of
  your browser.
- You need a desktop browser with WebAssembly and WebGL2, and a mouse.
  The game does not work on a phone with only a touch screen.

## Play with friends

The site has shared games for two or more players.

1. Start a game from the lobby.
2. Send the invite link to your friends.
3. Each friend signs in and takes a seat. A friend can also watch as a
   spectator.

If a player leaves, the AI takes that faction. The player can come back
and get it again. You can stop a shared game and continue it on another
day. See [docs/TRY-MULTIPLAYER.md](docs/TRY-MULTIPLAYER.md) for a
step-by-step guide.

## Scenarios

The game has these content packs:

| Pack | Setting |
|---|---|
| han_196 | The Three Kingdoms period of China, from the year 196 |
| medieval_europe, medieval_west, medieval_east | Medieval Europe |
| civil_war | The American Civil War |

Each pack is a folder of tables in [data/](data). The engine has no
setting of its own, so a new pack is data and not new code.

## Build from source

This section is for developers on Windows.

| Command | Result |
|---|---|
| `build.bat` | Builds the Windows game |
| `run_tests.bat` | Runs the tests |
| `build_web.bat` | Builds the WebAssembly game into build/web |
| `deploy_web.bat` | Builds the web game and deploys it to fly.io |

Start with these documents:
- [docs/ROADMAP.md](docs/ROADMAP.md) gives the layers and the milestones.
- [docs/tutorials/00-reading-order.md](docs/tutorials/00-reading-order.md)
  gives the order to read the code.
- [docs/WEB.md](docs/WEB.md) gives the web build and the site.
- [docs/MULTIPLAYER.md](docs/MULTIPLAYER.md) gives the plan of the shared games.
- [docs/ASSETS.md](docs/ASSETS.md) lists the assets and their licences.

## Licence

Copyright (c) 2026 Anthony Si. All rights reserved.

You can play the game on the site above. You must not copy, change,
distribute or host the code, the data or the art without written
permission from the author. See [LICENSE](LICENSE).

Some third-party parts have their own licences. These are the libraries in
[third_party/](third_party), the fonts in [data/fonts/](data/fonts) and
other assets that [docs/ASSETS.md](docs/ASSETS.md) lists.

Sanguo Dream is an original work. Koei Tecmo and other publishers have no
connection to this game, and they do not endorse it.
