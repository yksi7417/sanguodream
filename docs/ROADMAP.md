# hexdom roadmap

This document says what the project builds, in what order, and why. The
hexdom engine is a layered, data-driven grand-strategy engine. The first
content pack is the Three Kingdoms setting that the analysis repo specifies.
The mechanics are generic. A Civil War, Second World War or modern-conflict
pack is a different set of tables and a few plug-in rule functions. It is
not a rewrite.

Read docs/tutorials/00-reading-order.md to follow the code.

## 1. The layers

Each layer knows only the layers below it. Tests live at the layer that they
test (see .claude/rules/testing-pyramid.md).

| Layer | Module | What it is | Status |
|---|---|---|---|
| L0 Platform | code/win32_hexdom.cpp | window, GPU, input, memory block, hot reload | done |
| L1 Base | code/base.h, code/ecs/ | types, arenas, entity ids, component tables | done |
| L2 Space | code/sim/hex.h | hex coordinates, the cost map, shortest paths | done |
| L3 Time | code/sim/sim.h, sim.cpp, calendar.h, turn.cpp | turns, commands in, events out, seeded PRNG; the calendar, the turn order and the action points (tutorial 13) | done for M2 |
| L4 Domain | code/sim/ (one file per subsystem) | territory, settlements, resources, facilities, units, characters | facilities, slots and the develop command (tutorial 14); the production system, durability and the five city commands (tutorial 15); the rest next |
| L5 Conflict | code/sim/ | movement rules, zones of control, combat, siege | later |
| L6 Politics | code/sim/ | diplomacy, loyalty, factions, scripted events | later |
| L7 AI | code/sim/ai_*.cpp | planning over the same commands a player uses | later |
| L8 Front end | code/engine/, code/ui/, code/text/, code/render/, code/d3d11_renderer.h | engine_api, ImGui panels, locales and fonts (tutorial 12), and the 2.5D scene: a terrain mesh from per-cell kind and level, an orbit camera, instanced models and billboards, a pick from pixel to hex (tutorial 11) | done for L1-L3; placeholder art; text module done |
| L9 Content | data/<pack>/ | tables that make it a particular game | text, names, facilities.tsv, cities.tsv and rules.tsv of the placeholder pack |

The rule for every layer from L4 up: a mechanic is a pure function of
sim_state and a command. Tables from the content pack parameterise the
mechanic, and the mechanic emits events. No mechanic reads a hard-coded
number. Numbers live in tables.

## 2. What makes it pluggable

### 2.1 Content packs (data, not code)
A pack is a folder of tables that the engine loads at start. A generator
writes the San11 pack from spec-src/spec (field tables and formulas). People
write the other packs by hand. Every table is a flat TSV with a header row,
so the same loader and the same fixture format serve both tests and content.

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
| text/<locale>.tsv | every player-facing string by id (done: tutorial 12) | zh-TW, en | en | en |
| names/<locale>.tsv | names of people and places by their Big5 bytes (done: tutorial 12) | en; zh-TW decodes the bytes | - | - |

### 2.2 Rule hooks (the few things data cannot express)
Some formulas differ in shape between settings, not only in constants.
Those formulas are function pointers in a `ruleset` struct, one struct per
pack. Each pointer defaults to the generic rule:

- development_gain(facility, builder character, gold, season)
- harvest(settlement, season)
- combat_damage(attacker, defender, terrain, tactic)
- morale_change(unit, event)
- loyalty_drift(character, faction, turn)
- supply_reach(settlement, unit)
- ai_priority(faction, candidate command)

A pack ships its ruleset in C++ (code/sim/rules_<pack>.cpp) and nowhere
else. Everything else in the pack is tables. The clean room writes the San11
ruleset from spec-src/spec/mechanics, with the fixtures as its acceptance
tests.

### 2.3 What stays generic
These parts stay generic, and a new pack never touches them:
- turn order
- the command and event pipeline
- the ledger
- hex geometry
- pathfinding
- zones of control
- the ECS
- saves
- the AI planner skeleton
- every panel of the UI

### 2.4 What a very different setting still needs
A modern asymmetric conflict has insurgency, population support, air power
and logistics over roads. It needs mechanics that the Three Kingdoms pack
does not have: population attitude per region, non-territorial units, and
air and sea layers. The layer model lets you add them as new L4-L6 modules
with their own tables. The split between the ruleset and the tables has one
purpose: the new modules do not disturb the packs that exist.

## 3. Milestones

Each milestone names four things:
- the spec input it needs from the analysis side
- the engine_api additions
- the tests at each layer
- what you can see in the UI at the end of the milestone

### M0 Skeleton (done)
Window, hot-reloaded DLL, deterministic sim core with ledger, ECS, hex map
with A*, engine_api, test pyramid with imgui_test_engine. Tutorials 01-08.

### M1 Formats and the first mechanic (deck Milestone 1)
Status (2026-10-16): done. The loader half and the mechanic half work. The
save and scenario loader reads sample.s11 into officer and
city tables. engine_api has `load_save`, `officer_count`, `officer_at`,
`city_count` and `city_at`. The Officers and Cities panels show the tables.
build\save_dump.exe prints the sample as JSON for comparison with the dump
of the analysis side (tutorial 09).
Develop (2026-10-16, tutorial 14): the spec gave
mechanics/cities-and-facilities.md and city-commands.md instead of
farm-development.md. code/sim/tables.h is the TSV parser of the content
pack and of the fixture runner. data/placeholder/facilities.tsv has the 10
base and the 10 PK facilities, and cities.tsv has the slots of the 42
cities. The `develop` command builds a facility in a slot. The sum of
politics sets the period (an assumed line through the three points of the
manual, Q-142). A mint or a granary gives 1.5 times to the next market or
farm. The governor follows the selection rule without the post (Q-147).
The City panel and the Develop dialog issue one `command_develop` for each
click. No fixture exists yet: the feature tests check the documented
numbers. The slot layout is a placeholder (Q-145). FEATURES.md marks F-020,
F-021, F-022 and F-057 as built.
Scenario fixtures (spec f788503): feature tests load the real SCEN002.s11
and Scenario.s11 through the loaders. They compare SCEN002 with sample.s11
where the spec gives one layout. engine_api has `load_name_index` and
`city_name_at`. The Cities panel shows the Big5 bytes of each city name.
save_dump.exe prints the names with `--name-index`. Open spec questions
from the loader: Q-012, Q-013 and Q-015 to Q-018. The spec closed Q-014.
Cross-wall comparison (2026-10-16): the clean-room dump and the dump of
the analysis side agree on sample.s11. They agree on every field of all 42
cities and of the 659 officers that both list. The clean room lists 194
more officers (empty-status slots with names, and dead officers). Spec
Q-012 decides the correct rule. This is item 3 of the playbook's Milestone 1.
Text and locales (2026-10-16): the Officers and Cities panels show the
names as characters. The text module decodes the Big5 bytes with the
Unicode BIG5.TXT table and maps the user-defined area to the Private Use
Area. Each name of sample.s11 and Scenario.s11 decodes without U+FFFD.
The UI strings come from data/placeholder/text/<locale>.tsv, with zh-TW
first and en as the fallback. A Settings panel selects the locale. The
game window loads Noto Sans CJK TC and Noto Sans Thai from data/fonts
(docs/ASSETS.md, tutorial 12). Q-013 stays open: the spec must name the
Big5 variant and the characters of FA40 and FA45.

Spec input: spec/formats/save-file.md, scenario-file.md; mechanics/farm-development.md with fixtures.
- Engine:
  - a TSV table loader (code/sim/tables.h) that both content packs and fixtures use
  - a save/scenario loader written from the field tables
  - settlements and characters as components
  - the `develop` command
- API: settlement queries (name, owner, resources, facility levels), character queries, `command_develop(settlement, facility, character, gold)`.
- Tests:
  - unit: the table parser
  - feature: fixture rows for farm development pass through the api. The loader dumps the sample save to the same JSON that the analysis dump script produces.
  - ui: the settlement panel issues exactly one develop command per click
- UI: settlement inspector, character list, develop dialog.
- Exit: Milestone 1 items 3, 5 and 7 of the playbook.

### M2 Turn structure and the command budget
Status (2026-10-16): done (tutorial 13). The spec input was
mechanics/calendar-and-turn.md. A turn is 10 days, a month 3 turns. The
ledger names the month, season and year events, the hooks of the
disasters, and the gold and grain income and consumption. The engine
assumes each amount, and the ledger marks it "assumed". The factions act in order:
fewer cities, then fewer officers, then fewer troops. A human faction
ends its turn with `end_turn` ("proceed"). Each AI faction passes. Each
faction has one pool of action points with a cap of 255. The engine assumes
the recovery rule (Q-019, Q-141). `can_issue` gives each refusal reason as
an enum. The demo is "demo/m2_turns". FEATURES.md marks F-002, F-003 and
F-004 as built.
`end_phase` of the plan is `end_turn`, because the spec has no phases
inside a faction turn.

Spec input: mechanics/turn-structure.md (phases, action points, who acts when).
- Engine: phase order as data (TurnScheduler), a per-faction action budget, command validation (who can issue what, and when), and an AI phase placeholder that passes.
- API: `whose_turn`, `actions_left`, `can_issue(command)`, `end_phase`.
- Tests: feature tests for every refusal reason; a ui test that disabled buttons issue nothing.

### M3 Economy
Status (2026-10-16): done (tutorial 15). The spec input was
mechanics/cities-and-facilities.md, calendar-and-turn.md, city-commands.md,
weapons-and-production.md and skills.md. The production system
(code/sim/economy.cpp) gives the gold, the grain, the salaries and the
consumption at the calendar events. It applies the mint and granary bonus,
the PK markets, the merge levels, public order, the income skills and the
caps. The ledger explains each change of the gold and the grain: the sum of
the events of a city is the change of its stock. Facilities recover
durability each turn and the engine destroys a facility at 0. The five city
commands patrol, conscript, train, merchant and produce have the documented
costs and the refusal reasons of M2. Each command gives merit, and all but
the merchant give technique points. rules.tsv has the effects as assumed
rules (Q-151 to Q-160). The UI has a dialog for each command, an
Economy panel and a ledger filter by city and resource. The demo is
"demo/m3_economy". FEATURES.md marks F-005, F-006, F-025, F-026, F-027,
F-029, F-030 and F-031 as built.
The spec has no fixture for the economy yet. Thus the 12-month trace checks
the calendar order, the forecast amounts, the ledger sums and the caps. It
does not compare with a game trace.

Spec input: mechanics/{market,upkeep,tax,harvest}.md with a 12-month trace.
- Engine: a Production system over the facilities.tsv yields and the resources.tsv decay; the ledger explains every number.
- Tests: the fixture trace of one untouched city for 12 months matches within tolerance; a ui ledger filter test.
- UI: ledger filters by settlement and resource; an economy summary panel.

### M4 Growth: techniques, merge, disasters, the end of the game
Status (2026-10-17): done (tutorial 16). The spec input was
mechanics/research-techniques.md, cities-and-facilities.md,
weapons-and-production.md, calendar-and-turn.md and skills.md.
techniques.tsv has the 36 techniques with the documented costs. The
research command keeps up to three officers busy for an assumed period
(Q-161). The effects are flags; M4 applies the morale cap, the facility
recovery, the strong weapons and the order loss. The merge command raises
a facility to level 2 and 3 (Q-163). The PK facilities have the effects of
the commands that exist (Q-164). disasters.tsv gives the locusts, the
plague, the bandits and the good harvest as calendar events with assumed
numbers (Q-162). The engine checks victory and defeat at the start of each
turn (Q-165). The game setting "disasters" turns the rolls off. The demo
is "demo/m4_growth". FEATURES.md marks F-007, F-008, F-023, F-024, F-028
and F-040 to F-043 as built.
The first plan of M4 named recruitment and training. M3 built the
conscription per barracks and the train command. unit_types.tsv and the
quality of troops move to M5 with the unit classes (F-080).

Spec input: mechanics/research-techniques.md, cities-and-facilities.md, calendar-and-turn.md.
- Engine: techniques.tsv and the research command; the merge; disasters.tsv as calendar events; victory and defeat.
- API: `technique_kind_at`, `faction_techniques`, `command_research`, `command_merge`, `city_disasters`, `outcome`, `settings`.
- Tests: feature tests for each refusal, effect and disaster; the random rolls start each disaster only in its months.

### M5 Movement and the map proper
Spec input: formats/map.md (terrain per cell, roads, rivers), mechanics/movement.md (costs per movement class, ZOC, stack rules).
- Engine: terrain.tsv drives the hex_map costs per movement class; units as entities with a position; the `move` command with path validation; zones of control.
- API: `unit_at`, `reachable_cells(unit)`, `command_move(unit, to)`.
- Tests: unit (ZOC geometry), feature (fixtures for move costs), ui (a map click sends one move command with the picked path).
- UI: units on the map, a movement range overlay.

### M6 Combat and siege
Spec input: mechanics/{combat,tactics,morale,siege}.md with many fixture rows.
- Engine: the combat_damage rule hook, morale, siege as facility damage; the San11 ruleset.
- Tests: fixtures dominate here; property tests (damage is never negative, dead units leave the map).

### M7 Characters and politics
Spec input: mechanics/{loyalty,recruit-officer,diplomacy,debate,duel}.md.
- Engine: the loyalty_drift hook, diplomacy as proposals with acceptance rules, debate and duel as mini-games over character stats.

### M8 Technology, events, AI
Spec input: mechanics/{tech-tree,events,ai-phase-order}.md.
- Engine: tech.tsv unlocks; events.tsv triggers; an AI planner that uses ai_priority over the same commands that the player has.
- Tests: AI feature tests are "given this state, the AI issues a command of kind X"; never pixel-based or time-based.

### M9 Playable
This milestone has three parts:
- a game UI on top of the debug panels. The text module and text/<locale>.tsv exist (tutorial 12). Portraits and music come from the player's own install at runtime, never from the repo.
- save and load of the engine's own format
- a second content pack (Civil War) as the proof that the mechanics are generic

## 4. Playability
The analysis repo's spec/knowledge/playability.md (mined from the video
series) feeds this list. This list is the measure of the UI and the AI, not
a rule set:
- The map is the game: everything a player decides is visible on the hex map
  (unit ranges, supply, threats) without a menu.
- Every number has a cause: the ledger is the player's accountant.
- Officers are the hook: the dominant decision must be to put the right
  person in the right job, so the character panel is first-class.
- The AI must play by the same commands, so a "why did the AI do that" view
  shows its ai_priority scores.
