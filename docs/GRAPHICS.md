# Graphics and animation plan

This document is the plan for the art and the animation of the map. It
covers the units, the cities, the gates, the ports and the facilities. It
also covers the animation of each action. The actions are a move hex by
hex, an attack, each tactic, each plot, fire, a siege and a capture.
docs/art/GEMINI-PROMPTS.md has the prompts that make the art with Gemini.
ROADMAP.md lists the six milestones G1 to G6 of this plan.

## 1. The engine today
- The scene is 3D. code/render/terrain_mesh.h builds one hex prism for
  each cell. The orbit camera turns freely and tilts from 30 to 75
  degrees (code/render/render.h).
- The platform layer has one mesh and one sprite (code/d3d11_renderer.h).
  Mesh 0 is settlement.hxm or a placeholder house. Sprite 0 is unit.png or
  a placeholder figure of 64 x 64 pixels.
- code/ui/ui_scene.cpp pushes one house for each base and one billboard
  for each unit. A faction colour tints both.
- A unit jumps from its old hex to its new hex. Nothing moves between
  two frames of the sim.
- The sim is instant and deterministic. Commands go in and events come
  out. The event ledger keeps every event.

## 2. Goals and limits
- The player sees each action of the sim as a short scene on the map. A
  unit walks hex by hex. Archers shoot arrows. Cavalry runs into the
  enemy. A gate breaks and the new banners go up.
- The sim does not change for the animation. The animation is a
  replay of the events of one command. The UI plays it.
- The rules of the clean room apply to the art too:
  - Do not give Gemini an image, a video or a name of the original game.
    Do not write "in the style of" a game or a studio in a prompt.
  - Describe the look from history: Han dynasty armour, weapons, tomb
    figures, pottery tower models and stone reliefs.
  - The choreography in section 8 comes from the rules of the spec. It
    does not copy the scenes of the original.
- Every asset has a fallback. Without the art pack, the game draws the
  placeholders of today and the procedural effects of G1.
- The player can turn the animation off, make it fast, or skip one scene.

## 3. Art direction
One style bible fixes the look before any other asset. The first prompt
of docs/art/GEMINI-PROMPTS.md makes it. Each later prompt attaches it as
the reference image.

| Topic | Choice |
|---|---|
| Period | Late Eastern Han, 184 to 220 AD |
| Paint | Gouache and ink wash on warm paper tones; crisp silhouettes |
| Palette | Ochre, umber, slate blue, pine green, lacquer black; one accent |
| Light | Soft, from the upper left; no hard cast shadows in the sprite |
| View | Three-quarter top-down, about 50 degrees, orthographic |
| Faction colour | Banners, sashes, plumes and shield faces in one flat red (#D01818); the importer turns that red into a tint mask |
| Scale | One hex is one world unit; a foot soldier is 0.30 units tall; a city fills its hexes |
| Text | None in any image; the engine draws all text |

## 4. The show system

### 4.1 The idea
The sim runs a command without delay and writes events. A new UI module, the
director (code/ui/ui_show.cpp), reads the events of that command. It
turns them into clips on a timeline. The scene draws the clips. When the
last clip ends, the scene shows the state of the sim again.

### 4.2 The data that the director needs
Some events do not carry sufficient data for a scene. G1 adds the fields or
the event kinds in this table. The sim computes the data already. It
only writes more of it.

| Need | Event today | Change in G1 |
|---|---|---|
| Each hex of a move path | SIM_EVENT_UNIT_MOVED (points only) | New SIM_EVENT_UNIT_STEP for each hex: unit, cell from, cell to |
| The attacker, the target and the action | SIM_EVENT_UNIT_DAMAGED (target only) | New SIM_EVENT_ATTACK: attacker, target (unit, base or building), action (melee, arrow, tactic id, plot id), success |
| The tactic and its hexes | SIM_EVENT_TACTIC_USED (unit, success) | The tactic id and the target cell |
| The plot and its hexes | SIM_EVENT_PLOT_USED (unit, success) | The plot id and the target cell |
| A push or a pull | SIM_EVENT_UNIT_PUSHED (hexes) | Cell from and cell to |
| A tower shot at turn start | none | New SIM_EVENT_FACILITY_SHOT: building, target unit |
| A sortie from a base | SIM_EVENT_UNIT_SORTIED | The cell of the first hex |
| A retreat | SIM_EVENT_UNIT_RETREATED | The cell where the unit stood |

The order of the events in the ledger is the order of the scene. The
director never reads the sim state of a later time.

### 4.3 Clips
A clip has a kind, a start time, a duration, actors and cells. Clips of
one event run in sequence. Some clips run in parallel, for example the
arrows of a volley.

| Clip | What it shows |
|---|---|
| step | The unit moves one hex. The figures walk. The height follows the terrain. |
| face | The unit turns to a hex direction. |
| strike | The figures go 0.3 hex forward, clash, and come back. Sparks at the contact. |
| volley | 6 to 12 arrows fly on arcs from the shooters to the target hex. |
| bolt | One fast arrow flies flat through every hex on the line. |
| stone | A stone flies on a high arc. Dust and debris at the impact. |
| charge | Cavalry runs into the target hex and comes back, or stays (see section 8). |
| pass | The unit runs through the target to the far hex. |
| slide | The target slides N hexes with dust. Used for a push and a pull. |
| hit | The target flashes white, the figures stagger, and the loss in soldiers rises in red. |
| fall | Figures fall one by one. The banner drops. The unit fades. |
| fire | A flame loop starts on a hex and stays until the fire ends. |
| burst | One effect plays once on a hex: smoke, splash, lightning, swirl. |
| icon | A state icon appears over a unit: confusion, false report. |
| number | A number rises and fades: soldiers, durability, morale. |
| flag | The banners of a base go down and the banners of the new owner go up. |
| camera | The camera moves to the scene when it is off screen. |
| cut-in | A message window with a portrait, as in ui/unit-commands.md. |

### 4.4 The shown state
While a scene plays, the scene draws a shown copy of each actor. The
shown copy has its own position, direction, soldier count and figures.
The other units and bases come from the sim. When the scene ends, the
shown copies go away. A skip jumps to the end. Because the sim already
holds the end state, a skip can never break a rule.

### 4.5 Speed, skip and the turns of the AI
| Setting | Values | Default |
|---|---|---|
| show_speed | off, fast (3x), normal | normal |
| show_step_seconds | 0.05 to 0.60 s for one hex of a foot unit; the other times scale with it | 0.22 |
| show_ai | all, near own bases, none | near own bases |
| show_camera_follow | on, off | off |

The Settings panel shows each of these settings, and the settings file
keeps them. The original always plays the move at one speed, and its
camera stays where the player left it (spec ui/main-map.md, "Scenes on
the map"). Thus the defaults copy the original. The other values are
extras of hexdom.

- A right click or the Space key skips the scene that plays.
- The player cannot give a command while a scene plays.
- Off makes every clip zero time. The numbers and the cut-ins still show.
- The effects use their own random numbers in the UI. The sim random
  numbers stay untouched, and a replay of the sim stays the same.

### 4.6 The timing table
data/placeholder/show_timing.tsv keeps every duration. A designer tunes
the times without a build. The project assumes the values below. The real times
of the original are an open question (section 10).

| Clip | Normal speed |
|---|---|
| step, foot | 0.22 s for each hex |
| step, cavalry | 0.16 s for each hex |
| step, siege and transport | 0.30 s for each hex |
| step, ship | 0.25 s for each hex |
| strike | 0.60 s |
| volley | 0.70 s |
| stone | 0.90 s |
| charge or pass | 0.50 s |
| slide | 0.25 s for each hex |
| hit with number | 0.50 s |
| fall | 0.80 s |
| flag | 1.20 s |
| cut-in | until a click, or 2.0 s on the turns of the AI |

## 5. The renderer
The platform layer gets these parts. Each one keeps the line between
the DLL and the platform: the DLL pushes commands, the platform draws.

| Part | Today | After |
|---|---|---|
| Sprite table | 8 sprites of 64 px | Atlas pages of 2048 x 2048, each sprite a rectangle in a page |
| Billboard | sprite id, size, tint | Also: frame rectangle, mirror flag, tint mask colour, alpha, blend mode |
| Facing | none | Two drawn facings (front three-quarter, back three-quarter) and their mirrors give four facings |
| Faction colour | Tint of the whole sprite | The mask of the red areas takes the faction colour; the rest keeps its paint |
| Banner | none | A blank banner sprite and the first character of the commander, from the CJK font |
| Mesh table | 8 meshes | 64 meshes, each with a vertex mask for the faction colour |
| Durability state | none | Three meshes or overlays for each base: whole, damaged, broken |
| Particles | none | Billboards with additive or alpha blend: fire, smoke, dust, sparks, splash |
| Projectiles | none | Billboards that turn along the velocity: arrow, fire arrow, stone |
| Blob shadow | none | A dark ellipse under each figure, so that units sit on the ground |
| Numbers and icons | ImGui plates | ImGui overlay at the world-to-screen point of the actor |

### 5.1 How a unit looks
- A unit is a dense crowd of small figures that fills its hex, as in the
  original (spec ui/main-map.md, "Scenes on the map"). More soldiers
  give more figures. A unit of 8000 soldiers shows approximately 60 to
  70 figures.
- The figure count is an assumption: one figure for each 120 soldiers,
  at least 4 and at most 80. One banner pole in the faction colour
  stands in each unit.
- The whole uniform of each figure takes the faction colour. The class
  shows in the figures: spears, halberds, crossbows or horses.
- The figures stand in rows that fill the hex, with a small random
  offset. Siege weapons and ships are one large sprite.
- 54 units of 80 figures need more than 4096 billboards. The renderer
  draws the crowds with instanced quads and raises the limit to 16384.
- The direction of a unit comes from its last step. The renderer compares
  that direction with the yaw of the camera and picks one of four views.
- Each figure gets a small random offset and a random first frame. Thus
  the figures of a squad do not step together.

### 5.2 How a base looks
- A base is a set of 3D meshes. A sprite shows one side only, and the
  camera turns freely.
- G4 builds a kit of parts in code: wall segment, corner tower, gate
  house, hip-and-gable roof, house block, pier, palisade, platform.
  misc/make_meshes.py writes the parts as .hxm files.
- Gemini makes the concept sheet of each base and the textures of the
  kit. Gemini does not make 3D meshes.
- Path B for later: an image-to-3D tool turns a concept sheet into a
  mesh. misc/obj_to_hxm.py converts the result. The licence of the tool
  goes into ASSETS.md.
- Small props stand as upright cards: trees, carts, traps, stone pillars.

## 6. The asset pipeline
1. Make the image with the prompt of docs/art/GEMINI-PROMPTS.md. Attach
   the style bible and the model sheet of the class.
2. Save the image under art-src/<group>/<name>.png. art-src/ stays out
   of git, because the source images are large.
3. Run misc/art_import.py. It does these steps:
   - It cuts each frame from the strip by the gaps between figures.
   - It removes the green background (#00FF00), or turns luminance into
     alpha for smoke and dust, or keeps black for additive fire.
   - It finds the red mask (#D01818) and writes it to a second channel.
   - It puts the feet of every frame on one baseline and centres them.
   - It scales each frame to its target size and packs the atlas pages.
   - It writes data/<pack>/art/atlas_N.png and data/<pack>/art/sprites.tsv.
4. Write the provenance row of each asset in data/<pack>/art/provenance.tsv:
   file, prompt id, model, date, the reference images.
5. Add the licence note to ASSETS.md, as for the portraits.

sprites.tsv has one row for each frame: sprite key, action, view,
frame, page, x, y, w, h, pivot x, pivot y, mask. The engine reads it at
start. A key that the file does not have uses the placeholder.

## 7. The asset list
Each row is one prompt family of docs/art/GEMINI-PROMPTS.md.

### 7.1 Units
| Unit | Model sheet | Strips (frames) |
|---|---|---|
| 劍兵 Sword | yes | idle 2, walk 4, attack 4, hit 2, fall 3 |
| 槍兵 Pike | yes | idle 2, walk 4, attack 4, thrust 4, hit 2, fall 3 |
| 戟兵 Halberd | yes | idle 2, walk 4, attack 4, sweep 4, spin 4, hook 3, hit 2, fall 3 |
| 弩兵 Crossbow | yes | idle 2, walk 4, shoot 4, melee 3, hit 2, fall 3 |
| 騎兵 Cavalry | yes | idle 2, trot 4, gallop 4, charge 3, hit 2, fall 3 |
| Banner bearer | yes | idle 2, walk 4, banner drop 3 |
| 衝車 Ram | yes | roll 2, swing 3, broken 1 |
| 木獸 Wood beast | yes | roll 2, flame 3, broken 1 |
| 井闌 Tower | yes | roll 2, shoot 3, broken 1 |
| 投石 Catapult | yes | roll 2, throw 4, broken 1 |
| 輸送隊 Transport | yes | walk 4 (carts and porters) |
| 走舸 Boat | yes | row 2, shoot 2, sink 3 |
| 樓船 Tower ship | yes | sail 2, shoot 2, ram 2, sink 3 |
| 鬥艦 Battle ship | yes | sail 2, shoot 2, ram 2, throw 3, sink 3 |

Each strip has two facings. The two mirrors make four.

### 7.2 Bases
| Base | Concept sheet | Variants |
|---|---|---|
| 都市 City | front, side, top | whole, damaged, broken |
| 大都市 Big city | front, side, top | whole, damaged, broken |
| 關所 Gate | front, side, top | whole, damaged, broken; doors shut and open |
| 港 Port | front, side, top | whole, damaged, broken |

### 7.3 Facilities and other buildings
| Group | Items |
|---|---|
| City facilities (on the slots of a city) | 市場, 農場, 兵舍, 鍛冶廠, 廄舍 at levels 1 to 3; 工房, 造船廠, 造幣廠, 穀倉, 銅雀台, 符節台, 人材府, 計略府, 大市場, 黑市, 外交府, 軍事府, 鍊兵所, 魚市, 軍屯農 |
| Military facilities | 陣, 砦, 城塞, 箭樓, 連弩樓, 投石台, 太鼓台, 軍樂台, 石兵八陣 |
| Obstacles | 土壘, 石牆 |
| Traps | 火種, 火焰種, 業火種, 火球, 火焰球, 業火球, 火船 |
| Map sites | 遺跡 ruin, 廟 temple, 堤防 dike, 落石 rockfall |
| Build stage | A scaffold of poles and ropes, at three sizes |

### 7.4 Terrain
One seamless tile of 512 x 512 for each of the 20 terrains of
terrain.tsv. The atlas of today keeps the tile index. Water tiles get a
second frame for a slow ripple.

### 7.5 Effects and icons
| Effect | Frames | Blend |
|---|---|---|
| Fireworks burst | 8 | additive |
| Flame loop | 8 | additive |
| Big fire on a building | 8 | additive |
| Smoke puff | 8 | luminance alpha |
| Dust cloud | 6 | luminance alpha |
| Impact debris | 6 | key green |
| Sparks | 4 | additive |
| Water splash | 6 | luminance alpha |
| Lightning bolt | 4 | additive |
| Dark vortex (妖術) | 8 | additive |
| Calm ripple (鎮靜) | 6 | additive |
| Arrow, fire arrow, stone, scroll | 1 each | key green |
| Icons: confusion, false report, fire, morale up, build, repair, supply, captive | 1 each | key green |

## 8. The choreography of each action
Each row gives the clips in order. "Target" is the unit, base or
building that the action hits.

### 8.1 Movement and unit commands
| Action | Scene |
|---|---|
| Move | step for each hex of the path; dust under cavalry; a wake behind ships; the camera follows on the turns of the AI |
| Move with a stop by a zone of control | steps to the stop hex; the unit stops with a short brace |
| Sortie (出陣) | the gate of the base opens; the figures walk out in a column; they form up on the first hex |
| Enter a base | steps to the gate; the column walks in; the gate shuts |
| 待命 Wait | the banner bearer plants the banner; idle frames |
| 退卻 Retreat | the figures turn and walk off; the unit fades; it appears at the home base with a short dust |
| 補給 Supply | carts roll to the other unit and back; number for the soldiers |
| 設置 Build | the figures carry poles to the hex; a scaffold appears; it grows each turn until the building stands |
| 修補 Repair | hammer sparks on the building; number for the durability |
| Soldiers lost on the way | a grey puff on the plank road or a green vapour at the poison spring; red number |
| Desertion for lack of grain | one or two figures walk away and fade; red number |

### 8.2 Attacks
| Action | Scene |
|---|---|
| 攻擊 Melee | face; strike; hit on the target; the target strikes back (counterattack); hit on the attacker |
| Arrow attack | face; shoot frames; volley at 2 hexes; hit; no counterattack at range |
| 齊攻 Joint attack | each attacker strikes in turn, with a 0.15 s overlap; one hit with the sum |
| Critical hit (會心一擊) | the strike flashes gold; a short camera shake; the number is larger |
| Attack on a base | strike or volley at the wall; debris; the durability number |
| Unit destroyed | fall; the banner drops; a captive icon if the sim captures the commander |
| 單挑 Duel | the two commanders face; the duel screen opens (M7) |

### 8.3 Tactics
| Class | Tactic | Scene |
|---|---|---|
| Pike | 突刺 | thrust frames; strike; slide of the target 1 hex; hit |
| Pike | 螺旋突刺 | thrust with a spiral trail; hit; confusion icon on a success |
| Pike | 二段突刺 | two thrusts; slide of the target 2 hexes; hit |
| Halberd | 熊手 | hook frames; the target slides 1 hex toward the unit; hit |
| Halberd | 橫掃 | sweep frames; an arc trail over the three front hexes; hit on each |
| Halberd | 旋風 | spin frames; a ring trail around the hex; hit on all six |
| Crossbow | 火矢 | shoot frames; volley of fire arrows; hit; fire on a success |
| Crossbow | 貫箭 | shoot frames; bolt through each hex of the line; hit on each |
| Crossbow | 亂射 | shoot frames upward; a rain of arrows on seven hexes; hit on each, own units too |
| Cavalry | 突擊 | gallop; charge into the target; the unit moves 1 hex; hit; a duel can follow |
| Cavalry | 突破 | gallop; pass through the target to the far hex; hit |
| Cavalry | 突進 | gallop; charge; slide of the target 2 hexes; hit |
| Ram | 破碎 | roll forward; swing; impact with debris; the durability number |
| Wood beast | 放射 | flame frames; a cone of flame; fire on the target |
| Tower | 火矢 | shoot frames from the top; volley of fire arrows into the base; soldiers number |
| Catapult | 投石 | throw frames; stone on a high arc at 3 hexes; impact; with 霹靂, dust on the six hexes around |
| Ships | 火矢 | volley of fire arrows from the deck |
| Ships | 猛撞 | the ship lunges; the target ship slides 1 hex; splash |
| Battle ship | 投石 | stone from the deck at 3 hexes |
| Any | Failure | the same start; the effect misses; a speech cut-in with the portrait: the officer of the unit says a short line of failure |
| Any | Knock onto a trap or a unit | slide stops at the obstacle; a hit on both; the trap fires (8.5) |

### 8.4 Plots
| Plot | Scene |
|---|---|
| 火計 Fire | a figure runs to the target and throws a torch; fire on a success |
| 滅火 Extinguish | water splash on the own hex and the hexes around; the flames shrink to smoke |
| 偽報 False report | a scroll flies to the target; false report icon; the target turns toward home |
| 擾亂 Confusion | drum rings spread over the target; confusion icon |
| 鎮靜 Calm | a green ripple on the target; the icons fade |
| 伏兵 Ambush | figures burst out of the forest hex; strike; hit; no counterattack |
| 內訌 Infighting | the adjacent enemy units face each other and strike |
| 妖術 Sorcery | a dark vortex over the target and the six hexes around; icons |
| 落雷 Lightning | lightning on seven hexes; hit on each; fire |
| Any failure | the same start; the effect fades; a speech cut-in: the officer of the target says that he saw through the plot |

The project writes its own lines for these cut-ins. It never copies the
text of the original. data/<pack>/text holds a few lines for each kind
of officer, so that each officer sounds a little different.

### 8.5 Fire and traps
| Action | Scene |
|---|---|
| Fire starts | flame loop on the hex; smoke rises |
| Fire each turn | the flame flares; hit on the unit or the building in it |
| Fire ends | the flame shrinks to smoke; the hex darkens for one turn |
| 火種, 火焰種, 業火種 | a burst on the trap; fire on the adjacent hexes; 業火種 also gives confusion icons |
| 火球 and its levels | a fire ball rolls 3 hexes in the direction of the ignition; hit on each unit on the way |
| 火船 Fire ship | the ship flares; fire on the adjacent water hexes; confusion icons |
| A trap of the enemy | visible on the map with its own sprite, as in the original |

### 8.6 Siege and capture
| Action | Scene |
|---|---|
| Blockade at turn start | a short smoke on the base; the soldiers number of the base |
| Facility shot at turn start | the tower faces the target; volley; hit |
| Base hit | debris on the wall; the durability number |
| Damaged state | below 66 % durability: cracks and thin smoke |
| Broken state | below 33 %: breaches in the wall and small fires |
| Base falls | the gate doors burst open; the old banners drop; the unit marches in; flag; fireworks over the base; a title card "XX 攻陷" over the base; then the dialog of the captives |
| Gate (關所) falls | as a city; the camera shows both sides of the pass |
| Port (港) falls | as a city; the moored boats take the new colour |
| Facility destroyed | the building collapses into dust; loot icons (gold, grain, soldiers) fly to the unit |
| Captives | a dialog lists the captured officers; for each one the player selects persuade, imprison, release or execute |

### 8.7 Turn events
| Event | Scene |
|---|---|
| Faction turn starts | the camera goes to the first base of the faction when show_ai allows it |
| Building completed | the scaffold drops away; the new building stands |
| Season change | the terrain tint changes over 1 s (later, not in G1 to G6) |

## 9. Milestones
ROADMAP.md has the short form. Each milestone keeps the four parts:
spec input, API, tests, what you see.

### G1 The show system
- Spec input: the rules of the spec for the order of the events.
  Section 4.6 gives the assumed times.
- API: the new event kinds and fields of section 4.2. engine_api gives
  the show settings and the events of the last command.
- Tests:
  - unit: the director turns an event list into the right clips.
  - unit: the clip durations come from show_timing.tsv.
  - unit: the position along a path at a given time.
  - unit: the four views from the camera yaw.
  - feature: a move writes one UNIT_STEP for each hex of the path.
  - feature: an attack writes ATTACK with the attacker, the target and
    the action.
  - ui: a unit stands between two hexes in the middle of a move.
  - ui: Space ends the scene. The setting off gives zero time.
- What you see: the placeholder figure walks hex by hex. Procedural
  effects show the attacks: lines for arrows, flashes for hits, numbers.

### G2 Units
- Spec input: military-units-and-tactics.md for the classes and ships.
- API: none new. The art pack adds sprites.tsv and the atlas pages.
- Tests:
  - unit: the parser of sprites.tsv and the frame lookup.
  - unit: the figure count from the soldiers.
  - ui: each class draws its sprite key.
  - ui: a pack without art draws the placeholder.
- What you see: squads of painted figures with banners in the faction
  colour. Siege weapons, transports and ships have their own sprites.

### G3 Combat
- Spec input: the tactics table of military-units-and-tactics.md.
- API: none new.
- Tests:
  - unit: one clip list for each tactic of section 8.3.
  - feature: each tactic writes the target cells that its scene needs.
- What you see: every attack and every tactic of section 8.2 and 8.3.

### G4 Bases, buildings and terrain
- Spec input: bases-ports-and-passes.md, cities-and-facilities.md,
  military-facilities-and-traps.md, terrain-and-map.md.
- API: the durability state of a base and a building for the renderer.
- Tests:
  - unit: the state from the durability.
  - unit: the mesh key of each facility type of save_types.tsv.
  - ui: sample.s11 draws every base and building with a mesh.
  - ui: no placeholder house stays on the map of sample.s11.
- What you see: walled cities, gates and ports, the facilities on their
  slots, and textured terrain.

### G5 Siege, plots, fire and traps
- Spec input: siege-and-bases.md, plots-and-states.md,
  military-facilities-and-traps.md.
- API: the flag clip needs the old and the new owner of a base.
- Tests:
  - unit: the clip lists of section 8.4 to 8.6.
  - feature: the fall of a base writes the events in the order of
    section 8.6.
  - e2e: a scripted siege from a save to the capture, at fast speed.
- What you see: a ram breaks a gate, fire spreads, the banners change.

### G6 Polish
- Spec input: ui/screen-flow.md for the cut-ins. The answers to the
  questions of section 10.
- API: sound hooks. Each clip can name a sound key (M17).
- Tests:
  - ui: the AI turn plays at the speed of show_ai.
  - demo g6 is a full battle.
- What you see: the camera follows the AI near the own bases. Each scene
  has its sound key. The times match the answers of the analysis side.

## 10. Questions for the analysis side
The player answered the seven questions of this plan on 2026-10-19.
The spec records the answers in ui/main-map.md, "Scenes on the map".
This plan uses them in sections 4.5, 5.1, 7.5, 8.3, 8.4, 8.5 and 8.6.

| Question | Answer |
|---|---|
| Q-G1 Options for the animation | None. The move always plays at one speed. |
| Q-G2 Time of one step | Approximately 0.2 s for each hex. hexdom makes it a setting. |
| Q-G3 Camera in the AI turn | The camera stays where the player left it. |
| Q-G4 Enemy traps | Visible on the map before they fire. |
| Q-G5 Fall of a base | Fireworks, then the title "XX 攻陷", then the dialog of the captives. |
| Q-G6 Figures and soldiers | A dense crowd. More soldiers give more figures. |
| Q-G7 Failed tactic | A line of the officer. For a plot, a line of the target. |

## 11. Risks
| Risk | Answer |
|---|---|
| Gemini draws a different soldier in each strip | Attach the model sheet to every strip prompt; regenerate the strip, not the sheet |
| Gemini does not keep the frame count or the gaps | The importer finds the figures by their gaps; a human fixes a bad strip in an editor |
| Gemini paints red outside the mask areas | The prompt allows no other red; the importer checks the mask area and reports large areas |
| Gemini output has a visible mark | Use AI Studio or the API; check each image before the import |
| The camera turns, and a sprite shows the wrong side | Four facings from two drawings; meshes for every base |
| A long scene slows the game | Speed fast and off; the skip key; show_ai near own bases |
| Art pages grow past 20 MB | art-src stays out of git; the atlas pages use compressed PNG; a download step if a pack grows too large |
