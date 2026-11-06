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

## The findings of match 1 and the changes to the AI
Match 1 of 2026-11-06 ran 150 turns of the sample. No city changed its owner after turn 58. A run of the same save with the AI in each seat, without the browser, showed four causes:
- The AI took only pikes for a sortie. A city without pikes sent no army.
- The AI conscripted more soldiers than the grain of the city fed. Those soldiers left the city.
- The patrols and the conscriptions took all the action points. The sortie had the lowest priority.
- Most large armies stood in cities with no enemy city within 30 hexes.

The AI now has these rules. Q-185 says that all of them are assumptions. Each pack has them in rules.tsv and ai_priorities.tsv:
| Rule or row | Value | Effect |
|---|---|---|
| sortie, priority | 44 | The soldiers above the threshold give urgency, full at two times the threshold |
| ai.sortie_min_soldiers | 2000 | The AI takes the land weapon with the largest stock. With fewer weapons it sends swords |
| ai.sortie_soldiers | 15000 | The soldiers of one AI unit. It was 5000 |
| ai.grain_turns | 12 | No conscription in a city with grain for fewer turns |
| buy_grain | 58 | The city buys grain from the merchant with the gold above ai.merchant_keep_gold (1000) |
| reinforce | 45 | A city with no target sends its soldiers above ai.sortie_reserve to the nearest own city on a front |
| conscript_rich | 35 | A city with more than 10000 gold conscripts |
| ai.weak_city_bonus | 60 | A unit prefers a city with fewer soldiers than the unit |
| defend | - | A city sends defenders only while the enemy units near it outnumber its own units |
| take_free | - | A city sends no unit while an own unit stands next to it |

The headless run of 150 turns gave these results on three seeds:
| Seed | Captures before | Last capture before | Captures after | Last capture after |
|---|---|---|---|---|
| 1 | 10 | turn 40 | 20 | turn 129 |
| 2 | 10 | turn 62 | 14 | turn 138 |
| 3 | 11 | turn 66 | 14 | turn 125 |

The gold of the AI still grows. The patrols still take the most action points.

## The limits
- The driver plays the guest by hand only for its first turns. It ends each turn without a command. After that, the AI plays the faction.
- The spectator follows each action with a pause. In a game of the AI alone, its captions fall behind the live game.
- The hook reads the full state. Only the test build has it.
