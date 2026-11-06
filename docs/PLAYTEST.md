# The play test of a shared game

This document gives the steps to record a shared game with two players and the AI factions. The record shows how the game develops, and the summary gives the data for a review of the fun.

## The parts
| Part | File | Job |
|---|---|---|
| The room server | web/server.py | Runs the rooms on the local site with the test login |
| The host | a browser that you control | Makes the room, takes a seat, and plays by hand |
| The driver | tests/web/playtest/match.js | Plays the second seat, records a spectator as video, and writes the logs |
| The hook | code/wasm_test_hooks.h, `wasm_test_factions` | Gives the date and the cities, troops, gold, food and units of each faction |
| The summary | tests/web/playtest/analyze.py | Reads the logs and writes summary.json |

## The procedure
1. Build the test game: `build_web.bat test-game`. Build the referee: `build_web.bat referee`.
2. Start the local site on port 8096 with `AUTH_DEV_LOGIN=1`, as in docs/TRY-MULTIPLAYER.md, part B1.
3. In your browser, open `http://127.0.0.1:8096/auth/dev/start?name=Host`.
4. Click `New game`, choose two rulers, click `OK`, and click `Play online`.
5. Take the seat of the first ruler. Copy the invite link.
6. Run the driver. Set `GUEST_FORCE` to the force id of the second ruler:

```powershell
$env:GUEST_FORCE = "10"; node tests/web/playtest/match.js http://127.0.0.1:8096/r/CODE build/playtest/match1
```

7. When the driver says that the host can start, click `Start the game`.
8. Play your turns. When you want a long game, click `AI plays for me`.
9. The driver stops at `MAX_TURNS` (default 150). To stop it before, make the file `STOP` in the out folder.
10. Run the summary:

```powershell
python -X utf8 -I tests/web/playtest/analyze.py build/playtest/match1
```

## The output
| File | Contents |
|---|---|
| turns.jsonl | One row for each change of the acting faction: the date, the steps, the checksums of two pages, and each faction |
| captions.jsonl | Each caption of "Follow the action" on the page of the spectator |
| driver.jsonl | The actions of the driver |
| console.jsonl | The platform lines and the errors of the two pages |
| shots/ | One picture of the spectator for each turn |
| video-Rival/, video-Camera/ | The video of the guest and of the spectator (WebM) |
| summary.json | The series of each faction, the eliminations, the pace and the kinds of the captions |

## The limits
- The driver plays the guest by hand only for its first turns. It ends each turn without a command. After that, the AI plays the faction.
- The spectator follows each action with a pause. In a game of the AI alone, its captions fall behind the live game.
- The hook reads the full state. Only the test build has it.
