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
Status (2026-10-17): done (tutorial 17). The spec input was
mechanics/terrain-and-map.md, military-units-and-tactics.md,
city-commands.md and ui/unit-commands.md. terrain.tsv has the 16 kinds
with an assumed cost for each movement class (Q-167). unit_types.tsv has
the 12 classes and the transport unit with the documented grades and the
observed movement. The unit values use an assumed formula (Q-170). Sortie
and transport take officers, soldiers, weapons, gold and grain out of a
city. A unit moves within its range, stops in a zone of control, loses
soldiers on a plank road or a poison spring, and eats grain. Wait,
retreat (assumed: in the same turn, Q-171) and supply complete the orders of M5.
The placeholder map comes from terrain.tsv with roads between the
cities; M10 replaces it with the real map. The Hex map panel, the scene and
the mini map show the units and the cities in the faction colours. The
demo is "demo/m5_march". FEATURES.md marks F-072 to F-076, F-078, F-080
and F-081 as built.

Spec input: formats/map.md (terrain per cell, roads, rivers), mechanics/movement.md (costs per movement class, ZOC, stack rules).
- Engine: terrain.tsv drives the hex_map costs per movement class; units as entities with a position; the `move` command with path validation; zones of control.
- API: `unit_at`, `reachable_cells(unit)`, `command_move(unit, to)`.
- Tests: unit (ZOC geometry), feature (fixtures for move costs), ui (a map click sends one move command with the picked path).
- UI: units on the map, a movement range overlay.

### M6 Combat and siege
Status (2026-10-17): done (tutorial 18). The spec input was
mechanics/military-units-and-tactics.md, plots-and-states.md,
military-facilities-and-traps.md, siege-and-bases.md and
terrain-and-map.md. tactics.tsv, plots.tsv, buildings.tsv and
critical_skills.tsv hold the documented rules. Every damage goes through
the combat_damage rule hook with an assumed default (Q-172 to Q-176).
Attacks, joint attacks, tactics, plots, fire, buildings, traps, natural
traps, sieges and the capture of officers work on the placeholder map. A
land unit on water is a boat. The demo is "demo/m6_battle". FEATURES.md
marks F-082 to F-093 as built. Ports and passes (F-032) wait for M8.

Spec input: mechanics/{combat,tactics,morale,siege}.md with many fixture rows.
- Engine: the combat_damage rule hook, morale, siege as facility damage; the San11 ruleset.
- Tests: fixtures dominate here; property tests (damage is never negative, dead units leave the map).

### M7 Characters and politics
Status (2026-10-17): done (tutorial 19). The spec input was
mechanics/officers-and-relations.md, factions-corps-and-posts.md,
city-commands.md, skills.md, diplomacy.md, duel.md, debate.md and
ui/duel.md. skills.tsv holds the 100 skills and sets the skill rules;
posts.tsv, ranks.tsv and items.tsv hold the documented tables. Loyalty
goes through the loyalty_drift rule hook. The personnel and ruler
commands, diplomacy, the plots of factions, the duel and the debate use
assumed numbers (Q-177 to Q-181). Allies and ceasefires are no enemies.
The demo is "demo/m7_court". FEATURES.md marks F-051 to F-056, F-058,
F-059, F-100, F-101 and F-110 to F-113 as built.

Spec input: mechanics/{loyalty,recruit-officer,diplomacy,debate,duel}.md.
- Engine: the loyalty_drift hook, diplomacy as proposals with acceptance rules, debate and duel as mini-games over character stats.

### M8 Technology, events, AI
Status (2026-10-17): done (tutorial 20). The spec input was knowledge/ai.md,
factions-corps-and-posts.md, city-commands.md, calendar-and-turn.md,
events.md, bases-ports-and-passes.md and siege-and-bases.md. bases.tsv,
events.tsv and ai_priorities.tsv hold the tables. The computer factions
and the delegated corps use the commands of the player through the
planner. The council and the adviser show its proposals. The engine query
ai_plan lets a test check the command that the AI issues for a state. The
engine uses assumed numbers for the AI, the corps, the emperor, the events
and the bases (Q-183 to Q-186). The demo is "demo/m8_rivals". FEATURES.md marks
F-032, F-120 to F-123 and F-130 as built.

Spec input: mechanics/{tech-tree,events,ai-phase-order}.md.
- Engine: tech.tsv unlocks; events.tsv triggers; an AI planner that uses ai_priority over the same commands that the player has.
- Tests: AI feature tests are "given this state, the AI issues a command of kind X"; never pixel-based or time-based.

### M9 Playable
Status (2026-10-17): done (tutorial 21). The engine saves and loads its own
format in 32 slots. The start has the settings of game-setup-and-modes.md,
the scenario list, the ruler list and new officers. The PK ability chart,
the 40 historical events of history.tsv, the three endings and the
portrait stage are in. The game screens sit on top of the debug panels.
The editor and the battle mode are stubs. data/civil_war/ is the second
content pack. The demo is "demo/m9_playable". FEATURES.md marks F-009,
F-060, F-061, F-131 to F-135 and F-140 to F-145 as built. F-077 waited for
Q-034; M10 builds it.

This milestone has three parts:
- a game UI on top of the debug panels. The text module and text/<locale>.tsv exist (tutorial 12). Portraits and music come from the player's own install at runtime, never from the repo.
- save and load of the engine's own format
- a second content pack (Civil War) as the proof that the mechanics are generic

### M10 The real map
Status (2026-10-17): done (tutorial 22). The spec input was
formats/map-data.md, mechanics/terrain-and-map.md and spec/fixtures/map/
(spec c6e197a, Q-034 resolved). The game window plays on the real
continent of 200 x 200 hexes. The tests of M0 to M9 keep the placeholder
map as their board. The demo is "demo/m10_map". The analysis side can mark
F-077 and F-147 to F-151 as built.

Spec input: formats/map-data.md, mechanics/terrain-and-map.md, fixtures/map/*.tsv.
- Engine:
  - hex.h uses the grid of the spec: flat-top hexes in odd-q offset. Column
    x runs west to east, row y north to south; odd columns sit half a hex
    south.
  - data/placeholder/map/ holds hexes.tsv and bases.tsv of the spec.
    misc/make_map_pack.py copies them and writes movement_costs.tsv.
  - code/sim/map.cpp loads the terrain, the region, the trap, the flags and
    the height of each hex, and the hex of each base. The rules of the load
    turn the 28 side hexes of the gates to cliff. They give the 50 slots of
    the port regions to the city of the port (F-151).
  - Each city stands on its flower of 7 hexes. Its slots are the 開發地 of
    the map, and the mint and the granary use the real slot neighbours.
    Each port and gate stands on its hex.
  - terrain.tsv has the 20 terrains with the ids of the spec, "enterable",
    "burnable" and "buildable". movement_costs.tsv has the 12 armaments x
    20 terrains. A unit type pays the row of its armament on land and the
    row of its ship on water. A hex with a fire costs 4 times (F-148).
  - A fire counts 0 to 7 turns and stays only on a burnable terrain (F-149).
  - A military facility stands only on 草地, 土, 砂地 and 主徑; a fire ship
    only on 河 and 海 (F-147).
  - F-150, the sites:
    - the 4 dikes flood their fixed zones;
    - a used rockfall comes back with a chance of 10 percent;
    - bandits use a bandit site of their city;
    - the game picks 6 + 6 event sites for the temples and the ruins.
- API: `map_info`, `hex_at`, `armament_count`, `armament_at`, `move_cost`,
  `bandit_sites` and the test command `start_fire`; `slot_neighbours` in
  `facility_at`; `enterable` and `burnable` in `terrain_kind_at`;
  `engine_config.use_pack_map`.
- Tests:
  - unit: the loader with the counts of the spec. These are 40,000 hexes,
    42, 10 and 35 bases, 294 city hexes and 591 slots. Also the gate
    cliffs, the slots and their neighbours, the sites, the flood zones, the
    errors, the cost table, and the neighbour table of map-data.md;
  - feature: movement on real terrain, fire decay, build permission, the
    rockfall, the bandit sites and the event sites. Also AI turns and a save
    on the real map, and each row of the seven map fixtures;
  - ui: the Hex map draws from one terrain view; a click on a city frames
    its slots and commands nothing;
  - demo: "demo/m10_map".
- UI: the Hex map panel draws the visible hexes in the tile colours. It
  shows the data of the picked hex and frames the slots of a clicked city.
  The scene shows the continent and the same slots.
- Open points for the analysis side (see the M10 report):
  - the masks of 關所 and 港 in facility-types.tsv;
  - the one ruin of events.md against the 6 + 6 event sites;
  - the entry of a unit into an own port or gate;
  - the land armament of a ship unit and the movement of the transport;
  - the sortie hex of a city flower.

### M11 The saved game on the real map
Status (2026-10-17): done (tutorial 23). The spec input was
formats/save-file.md, fixtures/map/facility-types.tsv and the fixtures of
play session 1 (spec 9aa9a7c), and ui/main-map.md (spec 02a4bad). The save gives the building array at
0x0008A3 and the troop unit array at 0x086B71. A loaded save puts its state on the map,
and the player can find each city and unit. The demo is
"demo/m11_save_map".

- Engine:
  - save_format reads the 16384 building records and the 54 troop unit
    records. save_dump prints them with --buildings and --units.
  - data/placeholder/save_types.tsv maps each facility type of a save to
    a row of the pack. A row is a facility with its level, a building, a
    natural trap, a temple or a ruin.
  - code/sim/save_map.cpp puts each facility on its slot with its level,
    and each military building and trap on its hex. It gives the
    durability to each base and natural trap. Fix (2026-10-18, spec
    9632e15): each gate and port has its own record at 0x08428D. Its owner
    (+9) and its soldiers (+10) come from there, not from building +2.
  - The units that officers ride in go on the map with their leader,
    deputies and troops. Fix (2026-10-18, spec 7fff546): the hex comes
    from record +23 and +25. The first free hex near the home base is only
    a fallback. The unit array has 56 units, past 0x08803B (Q-216). Fix
    (2026-10-18, spec e60a738): the record also gives the morale, the gold,
    the grain, the 混亂, the land class and the ship class. On water the
    unit moves and fights as its ship. The unit eats its grain. Fix (spec
    819d36f): the list of 11 entries at +41 gives the cargo of the six
    transports and the ship of a battle unit (Q-217). 混亂 ends by a chance
    each turn from 智力, and a small unit can panic (assumed, Q-218).
  - The per-hex block of the save gives the fire of each hex and the 12
    event sites (16 = temple, assumed; Q-205). The fix also does this part
    of M14.
  - grain.soldiers is 40, the rate of fixtures/grain-consumption.tsv.
- API: `hex_at` also gives the facility and its level, the building and
  the unit of a hex.
- UI:
  - A row of the Cities panel, the city combo or a row of the new Units
    panel selects a city or a unit. The camera frames it. The Hex
    map panel scrolls to it.
  - The scene draws a plate over each city, gate, port and unit. The
    plate shows the name, the soldiers and a thin bar. Near the camera,
    each slot of a city shows a glyph of its facility and level, or a ring
    when it is free. The unit under the mouse shows its commander.
  - The status bar at the bottom centre shows the hex under the mouse
    (ui/main-map.md). It gives the terrain and the base that owns it, and
    the coordinates x,y. It also gives the kinds that the player can place.
  - The grid lines show only on terrain that a unit can enter.
  - The mini map shows the city names, a white frame of the main view
    and a compass ring 北東南西 that turns with the camera.
  - A click in the scene opens the Hex info box. It shows the terrain,
    the height, the region by name and the slot. It also shows the
    facility and its level, the building, the unit and the sites.
  - W and S move the view up and down the screen, A and D to the sides,
    at every yaw. Q and Left turn the view clockwise on the screen, E and
    Right counter-clockwise, as the original does (ui/main-map.md). Up and
    Down tilt; the clamp keeps the yaw.
  - The City panel counts its facilities by kind and level.
- Tests:
  - unit: the building and unit records; the screen directions of the
    keys at nine headings; the tilt clamp;
  - feature: 柴桑 of sample.s11 has 12 of 12 slots in use. They hold:
    - 5 markets of level 3 next to a mint;
    - a fish market, a drill ground and a military farm;
    - barracks and a smithy of level 3, and a shipyard. Also 53 units, 30
    buildings, the bases; handlers for the three fixtures of session 1;
  - ui: the keys, a city row and a unit row frame, the Hex info box, the
    status bar, the facility counts.
- Open points for the analysis side:
  - Q-211: the hex, the class and the grain of a unit;
  - Q-212: building +2 and the empty record;
  - Q-213: 軍屯農 in the check of 柴桑.

### M12 The main map of the original
Status (2026-10-18): done (tutorial 24). The spec input was ui/main-map.md
(spec 02a4bad): "Mini map", "Map labels" and "Camera". M12 closes the
three items that M11 left open. The demo is "demo/m12_main_map".

- UI:
  - The mini map is a round map of the whole continent in a compass ring.
    The map and the ring turn with the camera. Each city is a square in
    the colour of its owner; the roads (主徑) are lines. A white frame
    shows the ground of the main view. A click or a drag on the disc moves
    the camera there. The code moved to code/ui/ui_minimap.cpp.
  - Each unit has a plate: a portrait, the unit name, a status mark (未,
    完 or 委), a supply icon 糧 and the soldiers. The clean room has no
    officer pictures. A square in the faction colour with the first
    character of the commander's name stands in for the portrait.
  - One press of Q or Left turned the view 60 degrees clockwise. The
    user then chose a smooth turn (2026-10-18). Q, E and the arrows now
    turn the view while the key is down, 90 degrees each second.
- API: `unit_at` also gives grain_turns, the turns that the grain lasts.
- Tests:
  - unit: the mini map axes at nine yaws;
  - feature: grain_turns of a new unit and of a unit of the save;
  - ui: the keys turn the view; a click on the mini map moves the
    camera; the unit plates read the commander and the corps.
- Open points for the analysis side:
  - Q-214: the angle, the time and the repeat delay of one press of Q;
  - Q-215: what 委 and the supply icon show.

### The goal of M13 to M17
M0 to M12 built the rules from the manual and the real map from the
game files. The goal from here is a game that is fun to play. Three
things stand between the engine and that goal:
- the rules the clean room assumed (Q-141 to Q-216) are not verified
  against the original, so the game can feel wrong in ways no test sees;
- a turn of play still needs the debug panels; the game screens of the
  original are not all there;
- the AI does not yet give a player a reason to come back.

Each milestone below keeps the four parts: spec input, api, tests, what
you see. The analysis side runs the experiments of
spec/experiments/ab-testing-plan.md in step with M13 and M15.

### M13 Fidelity: replay the original's saves
Status (2026-10-19): done (tutorial 26). The spec input was the three
save pairs of spec/fixtures/saves/pairs (conscript/001, develop/001,
economy-turn/001; spec 84714de), saves/README.md and
experiments/ab-testing-plan.md. The demo is "demo/m13_replay".

- Engine:
  - Fix: the city record has the troops at +16 and the gold at +20 (the
    spec correction of 2026-10-17). The first pair showed the swap.
  - save_format reads the two per-force tables. Table A at 0x082E49 gives
    the ruler and the action points. Table B at 0x0807A9 gives the
    technique points and the research with its turns.
  - sim_resume_save loads a save in the middle of the player's turn. No
    AI acts. The action points, the technique points and the acted flags
    come from the save.
  - code/engine/engine_replay.cpp runs a pair: it loads the second save
    and the first save, runs the command with the AI on, and compares
    5884 fields. Each difference names its rule and its Q-id.
  - misc/make_pair_index.py writes data/placeholder/pairs.tsv from the
    pair folders of the spec.
- API: `replay_pair(before, command, after)`, `replay_pair_count`,
  `replay_pair_at` and `replay_diff_at`.
- Tests:
  - unit: the two per-force tables and the record of force 46;
  - feature: a pair folder without a row or a handler fails. Each pair
    replays. The confirmed differences of each pair stay at or below the
    measured count: conscript 2, develop 1, economy turn 334;
  - ui: the Replay button of a row sends one replay_pair;
  - demo: "demo/m13_replay".
- UI: the Fidelity panel of the debug menu lists the pairs, replays one,
  and shows each field that differs with the rule and the Q-id.
- The gold, the merit, the acted flags and the officers of the develop
  and conscript commands replay with zero differences. The replays do not
  reach the goal of zero confirmed differences. The spec has no rule for
  the fields that differ. The spec also has no movement pair.
- Open points for the analysis side (not yet in QUESTIONS.md: an
  unattended run does not commit to the spec repository):
  - Q-153: one 徵兵 in 江陵 by 3 officers gave 3439 soldiers. The clean
    room gives 3570.
  - Q-152 and Q-154: the same 徵兵 took 10 public order, and city +84 fell
    by 9. The clean room takes 5 public order. The loader does not
    decode city +84.
  - Q-158: the 徵兵 gave 5 technique points, and the 開發 gave none at the
    order. The clean room gives 10 for each.
  - The merit of 開發: the original gives no merit at the order, but 徵兵
    gives 50 at the order. When does the officer of 開發 get the merit?
  - pair.json does not name the officers and the slot of a command. The
    index takes the officers from diff.json, and the engine takes the
    first free slot.
  - economy-turn/001 runs the AI of 13 factions. The differences are
    mostly AI choices (M15). A 進行 on the first day of a month can show
    the income (Q-151).
  - No pair for movement yet (Q-167 to Q-171).
  - Q-202: the record of force 46 of table A ends inside the city array.

The clean room loads a save of the original, runs one command, and must
reach the state of the next save of the original. This is the A/B test
at the level of one action.
- Spec input: save pairs in spec/fixtures/saves/pairs/<rule>/ (before,
  after, the command, the values on screen), one folder per resolved
  question. The analysis side makes them with its driver.
- API: `replay_pair(before, command, after)` in a test build only: it
  loads `before`, runs `command`, loads `after` with the save loader
  and diffs the two states field by field. A diff report names each
  field that differs, with the rule that owns it.
- Tests: feature: one replay test per pair folder, generated from the
  folder list; a pair with no handler fails. The unit of measure is the
  count of fields that differ. The milestone is done when the pairs of
  the economy, the city commands and movement replay with zero
  differences in the fields that the spec marks verified.
- What you see: a "Fidelity" panel that lists the pairs, the fields that
  differ and the Q-id of each; the demo m13 replays one pair.

### M14 A turn without the debug panels
Status (2026-10-19): done (tutorials 25 and 27). The spec input was
ui/city-commands.md, ui/unit-commands.md and ui/main-map.md, and the
per-hex block of formats/save-file.md. The demo is
"demo/m14_game_screens".

- UI:
  - In the game mode the City dialog shows the base command menu: 都市,
    軍事, 人材, 外交, 計略 and 君主. The first three open the submenus of
    city-commands.md. Each item opens the dialog of its command.
  - 移動 and 召喚 are grey: the engine has no command for them.
  - The debug layout keeps the flat buttons of the City panel.
- API: hex_at gives the fire turns and the event site of a hex. This is
  the `hex_state_at` of the plan. The M11 fix loads the per-hex block.
- Tests:
  - ui: one test for each of the 19 items of the base command menu;
  - feature: the fire of 周泰's hex and the 12 event sites of sample.s11
    (save_map_tests.cpp, M11 fix);
  - e2e: a turn of 柴桑 from sample.s11 with the debug panels hidden;
  - demo: "demo/m14_game_screens" runs 巡察 through 都市 and ends the turn.
- Open points for the analysis side:
  - 移動 and 召喚: mechanics/city-commands.md gives 20 action points and
    "turns: yes", but no travel time;
  - the submenus of 外交, 計略 and 君主 (not observed);
  - the port menu without 都市: the clean room has no port dialog yet.

Every command of one turn runs from the game screens, as in the
original's city, unit and map menus. The per-hex state of a save loads
too.
- Spec input: ui/city-commands.md, ui/unit-commands.md and
  ui/main-map.md (context menus and messages). Also formats/save-file.md
  for the per-hex block (fire turns, temple and ruin sites) and for the
  unit class, grain and morale (Q-211 when answered).
- API: the commands that the game screens still lack; `hex_state_at`
  gives the fire turns and the event site of a hex.
- Tests: ui: one test per menu item that asserts the api command it
  sends; feature: fire and the 12 event sites load from sample.s11 (周泰's
  hex burns); e2e: a scripted turn from the save with the debug panels
  hidden.
- What you see: the player plays a turn with the game screens only. The
  option `show_debug_panels` hides the ImGui tools. The demo m14 plays
  one turn of 柴桑 from the save.
- Status (2026-10-18): the main screen works (tutorial 25, the demo
  "demo/m14_game_screens"). The game mode shows only the HUD with the list
  at the top left and the mini map at the bottom right. Every other window
  opens on demand next to its entity and closes again. The menu items of
  the unit and city commands that still lack an api stay open work.

### M15 An opponent worth playing
Status (2026-10-19): done (tutorial 28). The spec input was
knowledge/ai.md and the replay of economy-turn/001 (M13). The spec has no
year log yet, so M15 has no statistics test. The demo is
"demo/m15_ai".

- Engine:
  - Each AI unit scores four goals and takes the best score:
    - defend an own city;
    - take a free city;
    - attack a city of an enemy;
    - go home when its grain does not last.
  - A unit in contact fights and stays.
  - ai_priorities.tsv has two new rows that send a unit out of a city.
    The row "defend" acts on an enemy unit near the city. The row
    "take_free" acts on a free city within ai.target_hexes.
  - ai.target_hexes is 30, because no city of the real map has a
    neighbour within 12 hexes.
  - ai.attack_after_months stops each sortie but defence in the first 5
    months of a scenario (knowledge/ai.md). The replay of
    economy-turn/001 keeps its 334 confirmed differences.
  - The sim keeps a record of the scored options of the last AI turn of
    each faction. The clean room assumes all goals, scores and rules
    (Q-185).
- API: `ai_explain(faction)`; the test command `set_unit_grain`.
- Tests:
  - feature: the record of ai_explain after an AI turn. A free city falls
    within 6 turns, but not in the first 5 months. A city under attack
    sends a unit that hits the enemy. A unit with grain for one turn goes
    home without desertion. The planner tests of M8 expect 12 rows;
  - ui: the AI panel reads the record of its faction, and a row frames
    its unit or its city. Neither commands anything;
  - demo: "demo/m15_ai" on the real map.
- UI: the AI panel of the debug menu lists the options with a mark on
  each chosen one. The scene draws a line from each unit of the faction
  to its goal.
- Open points for the analysis side (not yet in QUESTIONS.md: an
  unattended run does not commit to the spec repository):
  - a year log of the original: the saves of 12 months from one start,
    with the soldiers, the gold and the cities of each faction;
  - the month of the first AI attack when the player does nothing (the
    claim gives 5 to 6 months);
  - does the original AI take a free city, and how fast;
  - the distance of an enemy unit that makes an AI city send a unit;
  - does an AI unit go home for grain (the naval claim says that AI
    ships starve);
  - the order of the AI actions in a turn and the choice of the targets
    (Q-113, Q-140, Q-185).

The AI factions of the original act on the map with a visible purpose:
they build, recruit, march and take cities. The clean room measures
its AI against the original over a year of play.
- Spec input: knowledge/ai.md claims turned into rules by the analysis
  side (Q-ids on the order of AI actions, the targets it picks, when it
  attacks); a "year log" of the original: the saves of 12 months from
  one start, for the distribution tests.
- API: `ai_explain(faction)` gives the scored options of the last AI
  turn, for the "why did the AI do that" view of section 4.
- Tests: feature: the AI takes a free city within N turns, defends a
  city under attack, does not starve its units; a statistics test runs
  12 months from a save 20 times and compares the soldiers, the gold
  and the city count of each faction with the year log of the original
  (ranges, not exact values, because the random numbers differ).
- What you see: the AI view on the map; the demo m15 shows one AI
  faction taking a city.

### M16 The content of the player's own copy
Status (2026-10-19): done (tutorial 29). The spec input was
formats/scenario-file.md and formats/binary-identity.md. The spec has no
layout of the message, portrait and music files ("Asset formats are out of
scope"). Thus M16 reads the names and the scenarios of the install, and it
reports the other assets. The demo is "demo/m16_content".

- Engine:
  - set_install_folder reads media\scenario of the install. Scenario.s11
    loads as the city names, with one command. SCEN000..013.s11 give the
    scenario list with the number, the start date and the title.
  - The engine refuses a folder without these files, and the refusal
    changes nothing. An empty folder clears the install.
  - The engine counts the message files S11MSG00..03.s11. It does not
    decode them.
- API: `content_source(asset)` gives the source of the city names, the
  scenarios, the messages, the portraits and the music. The other new
  functions are `set_install_folder`, `install_folder`,
  `install_scenario_count` and `install_scenario_at`.
- Tests:
  - feature: the sources without an install. A test install has the two
    scenario files of the spec. It gives the names and one scenario, and
    a game starts from that scenario. Clear gives the pack again;
  - ui: Set and Clear of the Content panel send one set_install_folder
    each. Start sends one load_scenario. The officer view has one portrait
    frame for each source and commands nothing;
  - demo: "demo/m16_content".
- UI: the Content panel of the debug menu shows the source of each asset
  and the scenarios of the install. The officer view of the Court panel
  frames the portrait and names its source. No source gives a picture
  yet, so the frame shows the first character of the name.
- Not done in this run: the generated portraits of
  data/civil_war/Portraits.zip are on the branch feat/default-portraits.
  The unattended run did not get the permission to merge that branch. The
  owner of the repository decides the merge.
- Open points for the analysis side (not yet in QUESTIONS.md: an
  unattended run does not commit to the spec repository):
  - the layout of the message tables media\msg\S11MSG00..03.s11: the
    keys, the character set and the order. The feature test "the loader
    reads the message file of the install" waits for it;
  - the layout of the portraits in media\face, and the map from the
    portrait number of the officer record (+53) to a picture;
  - the files of the music, their format and the event of each track;
  - the map from SCEN000..013 to the scenario keys of scenarios.tsv
    (Q-189 covers the PK scenarios).

The game reads the names, the messages, the portraits and the music
from the player's own install at run time, and ships generated
portraits as the fallback pack.
- Spec input: formats/ for the message and portrait files of the
  original (the analysis side documents the layout, not the art);
  docs/ASSETS.md for the generated portraits and their prompt.
- API: `content_source` reports which pack gave each asset.
- Tests: feature: the loader reads the message file of the install and
  the fallback pack gives the same keys; ui: the officer panel shows a
  portrait from each source.
- What you see: the real faces and the real messages when the player
  points the game at the install; the generated faces otherwise.

### M17 Feel: a first hour that holds
Status (2026-10-19): done (tutorial 30). The spec input was
ui/title-and-load.md, ui/screen-flow.md and ui/main-map.md. The spec has
no capture of a first hour. It gives no frame times, no order of the
messages and no list of the reports. Thus M17 builds the times and the
windows that the spec observed. The demo is "demo/m17_first_hour".

- UI (code/ui/ui_feel.cpp):
  - the rider of the title: after a click, only the chosen button stays.
    The screen opens after 1 s, and a second click does nothing;
  - the load transition: a dark screen with no controls for 4 s after a
    load;
  - the message window at the bottom centre: at the start of a turn of
    a player, the period heading and the reports of the turn. A left
    click advances. The list panel hides while the window shows;
  - after 進行, the UI ends the turns of the AI factions until a player
    acts. Before M17 the game mode stopped at the turn of the first AI
    faction.
- API: none new. The two flags ui.feel.original_timing and
  ui.feel.turn_messages are off in ui_init. The game exe turns them on.
  Thus the tests and the demos of M0 to M16 do not change.
- Tests:
  - ui: the rider, the load transition, the order of the messages and
    the click that advances, and the messages after 進行;
  - e2e: the first hour from the title through 群雄割據 and seven turn
    starts, with a frame limit on each wait;
  - demo: "demo/m17_first_hour". The player marches from the capital to
    the nearest rival city. The first battle comes in the eighteenth turn.
- Open points for the analysis side (not yet in QUESTIONS.md: an
  unattended run does not commit to the spec repository):
  - a capture of a first hour from 重新開始新遊戲: the frame times of
    each screen and the order of the messages of the first turns. The ui
    test "the message order of the first turn matches the capture" waits
    for it. ui/screen-flow.md lists this screen as item 12 to record;
  - the list of the reports at a turn start, and the speaker of each
    report. The clean room shows the events of the player and the
    attacks on the cities of the player, and the ruler speaks;
  - the AI turn on the screen after 進行 (item 3 of the screens to
    record): camera jumps and messages. The clean room ends the AI turns
    without a show;
  - the sound hooks: the spec has no sound events. The keys wait for G6;
  - 遊戲教學, the tutorial scenario of the title: the spec has no data
    for it. The button stays grey;
  - the month of the first AI attack. Under the assumed numbers of M15
    (ai_priorities.tsv, troops_above 15000) the AI does not march in the
    first ten months of 群雄割據. knowledge/ai.md claims 5 to 6 months.

The first hour of a new game feels like the original: speed, messages,
sound hooks, and the tutorial of the first turns.
- Spec input: ui/screen-flow.md, knowledge/playability.md, the capture
  of a first hour of the original (frame timings, message order).
- API: none new; the engine exposes timing hooks for the UI.
- Tests: ui: the message order of the first turn matches the capture;
  e2e: the first hour script runs at cinematic speed without a stall;
  the demo m17 is that first hour.
- What you see: a new game from a scenario start, played with the game
  screens, from the title to the first war.

### The goal of M18 to M22
On 2026-10-20 a session played the original and hexdom side by side
and wrote spec/ui/clone-gap-report.md (in spec-src). The report lists
the gaps of the screens, the flows and the feedback, with a severity.
M18 to M22 close the gaps of severity A, one group each. The art of
track G stays separate. The order puts the feedback of a command
first, because every later test needs it.

### M18 Feedback of a command
Status (2026-10-20): done (tutorial 31, demo "demo/m18_feedback"). The
engine gives event_report. The pack table event_tones.tsv gives the
tone, the sign rule, the report flag and the log flag of each kind. The
text module has the provider interface and the template provider. The
game mode has the log, the speech bubble and the values. The turn start
shows only the reports, with the speaker of each one. The words of the
original are not a goal (DECISIONS.md). Open point for the analysis
side: the spec has no list of the kinds that the original logs and
reports at a turn start. The pack table assumes one.

Closes report sections 1 (log panel), 4 (result of 決定) and 7 (own
orders, reports).
- Spec input: ui/main-map.md (bottom-left panel, speech bubble,
  float of a value, message window), ui/clone-gap-report.md section 7.
  Open: the list of the reports at a turn start and their speaker, and
  the text of each log line for the five city commands.
- Decision of the player (2026-10-20): a text provider writes the
  words of each message. The provider is a plug-in of the front end.
  The first provider writes plain lines from templates. A later
  provider can use a small language model. Thus each event must carry
  the facts that a provider needs. These facts are the kind of the
  event, the entities, the speaker, the personality of the speaker,
  the tone and the sign. The sign is good news or bad news for the
  player. The
  tone and the sign are more important than the exact words.
- API: the engine gives each event its kind, its entities, a speaker
  and a sign. A table in the content pack gives the tone of each kind
  of event. The officer data gives the personality. Q-177 asks where
  the save keeps it. Until Q-177 closes, the pack gives a default.
- UI: a provider interface with one function that turns an event into
  a line. The template provider is the default. The message log at
  the bottom left with period headings and one line per event, names
  in a highlight colour. A speech bubble with a portrait slot over the
  base after a command. A value that floats over the base.
- Tests: unit: the template provider gives one line for each kind of
  event, and no line is empty. ui: one log line per city command. The
  bubble and the float appear after 決定. A second provider in the
  test replaces the default and its lines show. Demo:
  "demo/m18_feedback".
- What you see: 徵兵 in 長安 gives a bubble, a float and a log line,
  as in the original.

### M19 The city as a scene
Status (2026-10-20): in progress. Done:
- the merge of feat/default-portraits. ui_draw_officer_portrait shows
  the picture of the archive, else the frame with the first character;
- the proposal of the most suitable officers. 巡察, 徵兵, 訓練, 商人,
  生產兵裝, 開發 and 研究技巧 rank the free officers by the ability of
  the command (ui_propose_officers in code/ui/ui_city.cpp);
- one step for the sample: "Load sample save" also loads the city names
  (the request of the player of 2026-10-20).
Open: the portraits on the unit plates and in the officer list, the
city panel, the framed dialogs, the officer selection list and the
placement mode.

Closes report sections 3, 4 (dialog, proposal, officer list) and 5.
- Requests of the player (2026-10-20):
  - Each command dialog picks the three most suitable officers when it
    opens. The player does not set them by hand. The ability of the
    first column of the officer list sets the order. These abilities
    are 政治 for 開發, 統率 for 巡察, 武力 for 訓練, and 魅力 for 徵兵
    with 名聲 first. The source is
    spec/experiments/session-1-results-2026-10-20.md. The original
    also proposes three officers. The player can still change them.
  - Each officer shows a portrait: on the plate of a unit, in the
    officer selection list, in the dialog slots and in the bubble. Today
    the portrait is a letter in a frame, because no source gives a
    picture. The branch feat/default-portraits (worktree
    C:\src\hexdom-portraits, commit b6d8c94 of 2026-10-18) shows the
    default portraits of data/civil_war/Portraits.zip. Merge it first.
    The zip stays out of git (.git/info/exclude).
- Spec input: ui/city-commands.md (base panel, command dialogs,
  officer selection lists, placement mode). Open: the columns of the
  first tab of 選擇執行武將 for each command, and the proposal rule of
  the three officers.
- API: a query for the proposal of a command (the officers the engine
  picks). A query for the free slots of a city for the placement.
- UI: the city panel of the original with its 25 fields, at the left
  or right edge. The six-button menu next to the city. The command
  dialogs as framed dialogs at fixed positions on a dark map, with
  three portrait slots and skill tags. The officer selection list with
  its tabs, columns, check boxes, sort and the 一併 buttons. The 開發
  grid with the built counts and the placement mode with the top-right
  panel. The HUD panels hide while the menu or a dialog is open.
- Tests: ui: each dialog opens with the proposal filled. The list
  checks and sorts. 決定 is grey with no officer. The placement takes
  the clicked slot. Demo: "demo/m19_city_scene".
- What you see: a 徵兵 and a 開發 from the city panel look like the
  original, without the art.

### M20 The turn of the others
Closes report section 7 (AI turns).
- Spec input: ui/clone-gap-report.md section 7 (the frame times of
  one AI round), ui/main-map.md "Scenes on the map". Open: a capture
  of a full AI round with the times of each step (screen-flow.md,
  item 3).
- API: the engine already gives the events of each AI faction. The
  show system of G1 plays them.
- Decision of the player (2026-10-20): the show is off by default.
  The player must not click through the events of the other factions.
  Today hexdom shows 32 messages after 進行, and each needs a click.
  That goes away. The message log of M18 keeps every event, and the
  player looks back through it. A setting turns the show on.
- UI, setting off (the default): after 進行, the next turn of the
  player starts with no message to click. The log shows the date of
  the period and the events of each faction. A filter of the log shows
  the events of one faction or only the events near the player.
- UI, setting on: the HUD shows each AI faction in turn with its 技巧
  and no 行動力. Each command of an AI faction shows as a float over
  the base or the unit, with a short pause. A unit moves hex by hex.
  The camera stays where the player left it. No event of the show
  needs a click.
- Tests: ui: with the default setting, 進行 opens no message window
  and the log holds the events of the round. With the show on, the
  HUD names each AI faction in order, and the floats appear in the
  order of the events. Demo: "demo/m20_ai_round".
- What you see: one 進行 with no clicks, then the log of the round.

### M21 Information
Closes report sections 6 (情報, 地圖) and 1 (hover).
- Spec input: ui/information-screens.md, ui/main-map.md (hover
  panels, information menu). Open: the screens that the spec marks
  "not observed".
- API: table queries for officers, cities, ports, factions, units and
  items with a sort key.
- UI: the information menu as a column of 14 buttons. The table
  windows with their tabs, the detail panels, and 返回. The political
  map 中地圖 with its control column. The hover panels of a city and a
  unit next to the object. The 情報 window of the debug layout stays
  in the debug layout.
- Tests: ui: each menu item opens its screen. A header click sorts. A
  row click opens the detail. The hover panel shows the fields of the
  spec. Demo: "demo/m21_information".
- What you see: the fourteen screens and the political map.

### M22 Fit and finish
Closes the B and C rows of the report. The player confirmed the
removals below on 2026-10-20.
- Spec input: ui/clone-gap-report.md, ui/screen-flow.md (global
  rules).
- UI: the HUD panels hide while a menu or a dialog is open. The unit
  menu opens on the second click or after a move. The first click
  selects and shows the panel. The map menu loses 此處情報. The system
  menu has the six items of the original. 除錯面板 moves to F12 only.
  The status bar shows 不可 in red on a city hex. The help button "?"
  exists and opens a short help window. The mini map has no close
  button in the game mode.
- Tests: ui: the global rules of screen-flow.md as one test each.
- What you see: the flows match screen-flow.md step by step.

### Track G: graphics and animation (G1 to G6)
docs/GRAPHICS.md is the full plan. docs/art/GEMINI-PROMPTS.md has the
prompts for the art. The sim stays instant. A director in the UI replays
the events of each command as a short scene on the map. A skip or the
setting "off" shows the end state without delay.
- G1 The show system: new events for each step of a move and for each
  attack, the director, the timeline and the settings. The placeholder
  figure walks hex by hex. Procedural effects show the attacks.
- G2 Units: painted squads of figures in four views, banners in the
  faction colour, siege weapons, transports and ships. The importer
  misc/art_import.py and the atlas pages.
- G3 Combat: melee, arrows, counterattacks, critical hits, and a scene
  for each tactic of mechanics/military-units-and-tactics.md.
- G4 Bases, buildings and terrain: 3D meshes for cities, gates and
  ports in three states of damage, the facilities, and terrain textures.
- G5 Siege, plots, fire and traps: the fall of a base with the change
  of banners, every plot, fire that spreads, and the traps.
- G6 Polish: the camera on the turns of the AI, the sound keys, and the
  times from the answers of the analysis side.

The track starts after M17. R1 ships with each G milestone that has its
status "done". G1 to G3 are the minimum for a playtest.

### Follow-up A1: a pluggable AI
The user wants rules for the computer player that a later version can
replace and improve (2026-10-19). Today data drives the AI, but its logic
is fixed code. ai_priorities.tsv has a closed set of conditions, and
code/sim/ai.cpp holds the planner. The ruleset hook ai_priority of
section 2.2 does not exist yet.
- Engine: an AI planner is a set of function pointers in the ruleset of
  a pack. The planner of M15 becomes the default planner. A setting or
  a pack selects another planner. Each planner gives only the commands
  of a player, and ai_explain works for each planner.
- Rules from the answers of the player (spec QUESTIONS.md, Q-185):
  - the first attack comes after 6 months, not 5;
  - a city sends a unit out when an enemy unit enters the area of its
    faction, not within 12 hexes;
  - the AI takes a free city only for a purpose: spare officers and
    soldiers, developed front cities, or a rival near the city;
  - the AI attacks the nearest enemy city, and only with an advantage in
    strength, or when the target lacks soldiers, officers or grain;
  - a unit goes home when its grain is low. This rule of M15 stays.
- Tests: feature tests for each rule above. The feature test of M15
  that expects a free city to fall within 6 turns changes to match.
- What you see: the AI view of M15 names the planner and the reason for
  each choice.

### Release R1 Playtest build
After M17: a zip with the exe, the DLL and the fallback pack, a one-page
start guide, and the 32 save slots. Two players play the same scenario
in the original and in hexdom for one hour each, with the log of
section 5 of the analysis plan.

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
