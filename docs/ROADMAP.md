# hexdom roadmap

A layered, data-driven grand-strategy engine. The first content pack is the
Three Kingdoms setting specified by the analysis repo; the mechanics are
written so that a Civil War, Second World War or modern-conflict pack is a
different set of tables and a few plug-in rule functions, not a rewrite.

Read docs/tutorials/00-reading-order.md to follow the code; this file says
what gets built, in what order, and why.

## 1. The layers

Each layer only knows the layers below it. Tests live at the layer they test
(see .claude/rules/testing-pyramid.md).

| Layer | Module | What it is | Status |
|---|---|---|---|
| L0 Platform | code/win32_hexdom.cpp | window, GPU, input, memory block, hot reload | done |
| L1 Base | code/base.h, code/ecs/ | types, arenas, entity ids, component tables | done |
| L2 Space | code/sim/hex.h | hex coordinates, the cost map, shortest paths | done |
| L3 Time | code/sim/sim.h, sim.cpp | turns, commands in, events out, seeded PRNG | skeleton |
| L4 Domain | code/sim/ (one file per subsystem) | territory, settlements, resources, facilities, units, characters | next |
| L5 Conflict | code/sim/ | movement rules, zones of control, combat, siege | later |
| L6 Politics | code/sim/ | diplomacy, loyalty, factions, scripted events | later |
| L7 AI | code/sim/ai_*.cpp | planning over the same commands a player uses | later |
| L8 Front end | code/engine/, code/ui/ | engine_api and ImGui panels (debug now, game UI later) | done for L1-L3 |
| L9 Content | data/<pack>/ | tables that make it a particular game | not started |

The rule for every layer from L4 up: a mechanic is a pure function of
sim_state and a command, parameterised by tables from the content pack, that
emits events. No mechanic reads a hard-coded number; numbers live in tables.

## 2. What makes it pluggable

### 2.1 Content packs (data, not code)
A pack is a folder of tables the engine loads at start. The San11 pack is
generated from spec-src/spec (field tables and formulas); other packs are
hand-written. Every table is a flat TSV with a header row, so the same
loader and the same fixture format serve both tests and content.

| Table | Rows describe | Three Kingdoms | Civil War (1861) | WW2 (1939) |
|---|---|---|---|---|
| terrain.tsv | kinds of cell: move cost per movement class, defence modifier, yield modifiers | plain, forest, hill, mountain, river, road, city | farmland, forest, swamp, mountain, river, railroad | plain, forest, urban, mountain, sea, road, rail |
| map.tsv | one row per cell: terrain id, river/road flags, region id | the 42-city map | eastern theatre | Europe |
| regions.tsv | territory with an owner and a settlement anchor | cities, ports, gates | states / counties | provinces |
| resources.tsv | stockpiled quantities with a unit and a decay rule | gold, food, troops, weapons, horses, ships | money, supplies, manpower, rail capacity | fuel, steel, manpower, munitions |
| facilities.tsv | buildable things: cost, build turns, prerequisite, per-turn yield, capacity, upkeep | farm, market, barracks, smithy, stable, shipyard, walls | depot, foundry, recruiting office, fort, rail junction | factory, refinery, airfield, port, fortification |
| unit_types.tsv | movement class, strength, attack/defence profile, supply use, upkeep | spear, pike, bow, cavalry, siege, navy | infantry, cavalry, artillery | infantry, armour, air, naval |
| characters.tsv | named people with stats, skills, affiliation, loyalty | officers | generals, governors | generals, ministers |
| factions.tsv | sides: ruler, colour, relations, AI personality | warlords | Union, Confederacy | nations |
| tech.tsv | researchable improvements and what they unlock | PK tech tree | rifled muskets, ironclads | radar, doctrine |
| events.tsv | triggers (turn, condition) and effects | historical events | emancipation, elections | lend-lease, winter |
| text.tsv | every player-facing string by id | zh/en | en | en |

### 2.2 Rule hooks (the few things data cannot express)
Some formulas differ in shape between settings, not just in constants. Those
are function pointers in a `ruleset` struct, one per pack, defaulting to the
generic rule:

- development_gain(facility, builder character, gold, season)
- harvest(settlement, season)
- combat_damage(attacker, defender, terrain, tactic)
- morale_change(unit, event)
- loyalty_drift(character, faction, turn)
- supply_reach(settlement, unit)
- ai_priority(faction, candidate command)

A pack ships its ruleset in C++ (code/sim/rules_<pack>.cpp) and nowhere
else; everything else about the pack is tables. The San11 ruleset is written
from spec-src/spec/mechanics with the fixtures as its acceptance tests.

### 2.3 What stays generic
Turn order, the command and event pipeline, the ledger, hex geometry,
pathfinding, zones of control, the ECS, saves, the AI planner skeleton and
every panel of the UI. A new pack never touches these.

### 2.4 What a very different setting still needs
A modern asymmetric conflict (insurgency, population support, air power,
logistics over roads) needs mechanics the Three Kingdoms pack does not:
population attitude per region, non-territorial units, air and sea layers.
The layer model allows adding them as new L4-L6 modules with their own
tables; the point of the ruleset and table split is that adding them does
not disturb the existing packs.

## 3. Milestones

Each milestone names the spec input it needs from the analysis side, the
engine_api additions, the tests at each layer, and what you can see in the
UI when it is done.

### M0 Skeleton (done)
Window, hot-reloaded DLL, deterministic sim core with ledger, ECS, hex map
with A*, engine_api, test pyramid with imgui_test_engine. Tutorials 01-08.

### M1 Formats and the first mechanic (deck Milestone 1)
Spec input: spec/formats/save-file.md, scenario-file.md; mechanics/farm-development.md with fixtures.
- Engine: TSV table loader (code/sim/tables.h) used by both content packs and fixtures; save/scenario loader written from the field tables; settlements and characters as components; `develop` command.
- API: settlement queries (name, owner, resources, facility levels), character queries, `command_develop(settlement, facility, character, gold)`.
- Tests: unit (table parser), feature (fixture rows for farm development pass through the api; loader dumps the sample save to the same JSON the analysis dump script produces), ui (settlement panel issues exactly one develop command per click).
- UI: settlement inspector, character list, develop dialog.
- Exit: Milestone 1 items 3, 5 and 7 of the playbook.

### M2 Turn structure and the command budget
Spec input: mechanics/turn-structure.md (phases, action points, who acts when).
- Engine: phase order as data (TurnScheduler), per-faction action budget, command validation (who may issue what, when), AI phase placeholder that passes.
- API: `whose_turn`, `actions_left`, `can_issue(command)`, `end_phase`.
- Tests: feature tests for every refusal reason; ui test that disabled buttons issue nothing.

### M3 Economy
Spec input: mechanics/{market,upkeep,tax,harvest}.md with a 12-month trace.
- Engine: Production system over facilities.tsv yields and resources.tsv decay; the ledger explains every number.
- Tests: fixture trace of one untouched city for 12 months matches within tolerance; ui ledger filter test.
- UI: ledger filters by settlement and resource; economy summary panel.

### M4 Recruitment and training
Spec input: mechanics/{recruit,train,conscript}.md.
- Engine: unit_types.tsv, troops as a resource with quality; training as a facility yield.
- Tests: fixtures; feature test that recruitment respects facility capacity.

### M5 Movement and the map proper
Spec input: formats/map.md (terrain per cell, roads, rivers), mechanics/movement.md (costs per movement class, ZOC, stacking).
- Engine: terrain.tsv drives hex_map costs per movement class; units as entities with position; `move` command with path validation; zones of control.
- API: `unit_at`, `reachable_cells(unit)`, `command_move(unit, to)`.
- Tests: unit (ZOC geometry), feature (fixtures for move costs), ui (map click sends one move command with the picked path).
- UI: units on the map, movement range overlay.

### M6 Combat and siege
Spec input: mechanics/{combat,tactics,morale,siege}.md with many fixture rows.
- Engine: combat_damage rule hook, morale, siege as facility damage; the San11 ruleset.
- Tests: fixtures dominate here; property tests (damage never negative, dead units leave the map).

### M7 Characters and politics
Spec input: mechanics/{loyalty,recruit-officer,diplomacy,debate,duel}.md.
- Engine: loyalty_drift hook, diplomacy as proposals with acceptance rules, debate and duel as mini-games over character stats.

### M8 Technology, events, AI
Spec input: mechanics/{tech-tree,events,ai-phase-order}.md.
- Engine: tech.tsv unlocks; events.tsv triggers; AI planner using ai_priority over the same commands the player has.
- Tests: AI feature tests are "given this state, the AI issues a command of kind X"; never pixel or timing based.

### M9 Playable
Game UI on top of the debug panels (text.tsv, portraits and music from the
player's own install at runtime, never in the repo), save/load of the
engine's own format, a second content pack (Civil War) as the proof that
the mechanics are generic.

## 4. Playability
The analysis repo's spec/knowledge/playability.md (mined from the video
series) feeds this list; it is what the UI and the AI are measured against,
not a rule set:
- The map is the game: everything a player decides is visible on the hex map
  (unit ranges, supply, threats) without opening a menu.
- Every number has a cause: the ledger is the player's accountant.
- Officers are the hook: assigning the right person to the right job must be
  the dominant decision, so the character panel is first-class.
- The AI must play by the same commands, so a "why did the AI do that" view
  shows its ai_priority scores.
