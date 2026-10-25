# Gemini prompts for the art of hexdom

This document gives the prompts that make the art of docs/GRAPHICS.md
with Gemini. Each prompt is in a code block. Copy the blocks into Gemini
in the order that section 1 gives.

## 1. How to use the prompts
- Use the image model of Gemini, for example Gemini 3 Pro Image ("Nano
  Banana Pro") or a newer one. AI Studio and the API give images without
  a visible mark. Use Veo only for the video prompts of section 11.
- Make the assets in this order:
  1. The style bible (section 3).
  2. The model sheet of each unit (section 4.1).
  3. The animation strips (section 4.2). Attach the style bible and the
     model sheet of the unit to each strip prompt.
  4. The concept sheets of the bases and the kit textures (sections 5
     and 6).
  5. The facilities, the terrain, the effects and the icons (sections 7
     to 10).
- Build each prompt from the blocks of section 2. Put STYLE first, then
  the subject, then the background block, the layout block and NEGATIVE.
- Do not attach an image or a video of the original game. Do not name the
  original game, its studio or its series in a prompt. These rules keep
  the art a clean-room work.
- If a strip shows a different soldier, make the strip again. Do not
  change the model sheet.
- Write a row in data/<pack>/art/provenance.tsv for each image that you
  keep: file, prompt id, model, date, reference images.
- Run misc/art_import.py on the image. It removes the background, cuts
  the frames and finds the red mask (docs/GRAPHICS.md, section 6).

| Asset | Aspect ratio | Background block |
|---|---|---|
| Style bible | 16:9 | none |
| Model sheet | 1:1 | BG-GREEN |
| Strip of 2 to 4 frames | 16:9 | BG-GREEN |
| Strip of 6 to 8 frames | 21:9 | BG-GREEN or BG-BLACK |
| Concept sheet of a base | 16:9 | BG-GREEN |
| Facility | 1:1 | BG-GREEN |
| Seamless texture or terrain tile | 1:1 | none |
| Fire, sparks, lightning | 21:9 | BG-BLACK |
| Smoke, dust, splash | 21:9 | BG-BLACK (the importer uses the brightness as alpha) |

## 2. Shared blocks

STYLE: paste first in every prompt.
```
Game art asset for a turn-based strategy game set in late Eastern Han China (around 190 to 220 AD). Hand-painted look: gouache with ink-wash linework on warm paper tones, crisp readable silhouettes, slightly stylised proportions (heads a little large, about 1:5), muted earth palette of ochre, umber, slate blue, pine green and lacquer black. Soft light from the upper left. Camera: three-quarter top-down view looking down at about 50 degrees, orthographic, no perspective distortion. Historically grounded Han dynasty equipment and architecture, based on Han tomb figurines, pottery tower models and stone reliefs: lamellar armour, quilted tunics, ji halberds, crossbows with bronze trigger boxes, long spears, round-topped shields, two-wheeled carts, hip-and-gable tiled roofs with upturned eaves, rammed-earth walls, paired que gate towers. Original design.
```

MASK-RED: paste in every prompt of a unit, a banner or a base.
```
Paint the whole uniform of every soldier (tunic, sleeves, trousers and the cloth under the armour), and every banner, pennant, sash, helmet plume and shield face, in one flat saturated red, hex #D01818, with no shading inside those areas. Use that red nowhere else in the image. Skin, wood, leather, armour plates and bronze must stay clearly brown, grey or ochre, never red.
```

BG-GREEN: for every sprite, sheet and facility.
```
Background: one flat pure chroma-key green, hex #00FF00, edge to edge. No gradient, no floor, no ground plane, no cast shadow, no vignette. Nothing in the subject uses green close to #00FF00; foliage must be dark pine green or olive.
```

BG-BLACK: for fire, sparks, smoke, dust, splash and magic.
```
Background: pure black, hex #000000, edge to edge, no gradient. The effect is the only bright thing in the image.
```

LAYOUT-SHEET: for a model sheet with two views.
```
Layout: two views of the same subject side by side, same size, same height, feet on one shared baseline, a wide empty gap between them, nothing touching the image edge. Left view: three-quarter front, the subject faces toward the lower-left corner of the image. Right view: three-quarter back, the subject faces toward the upper-left corner of the image.
```

LAYOUT-STRIP: for an animation strip. Replace N with the frame count.
```
Layout: one single horizontal row of N frames of the same subject, evenly spaced, each frame the same size, feet on one shared baseline, a wide empty gap between frames, nothing touching the image edge. The frames are consecutive poses of one short animation, read from left to right. The subject faces toward the lower-left corner of the image in every frame.
```

LAYOUT-STRIP-BACK: use instead of LAYOUT-STRIP for the second view of a strip.
```
Layout: one single horizontal row of N frames of the same subject, evenly spaced, each frame the same size, feet on one shared baseline, a wide empty gap between frames, nothing touching the image edge. The frames are consecutive poses of one short animation, read from left to right. The subject faces away from the viewer, toward the upper-left corner of the image, in every frame.
```

NEGATIVE: paste last in every prompt.
```
Do not include: text, letters, Chinese characters, numbers, logos, signatures, watermarks, frames, borders, UI elements, photographic realism, 3D render look, anime style, modern objects, gunpowder weapons, European plate armour. Do not imitate any existing video game.
```

## 3. The style bible
```
[STYLE]
A single key-art illustration that defines the look of the whole game. A wide river valley in late Eastern Han China seen from the three-quarter top-down view. On the left, a walled city of rammed earth with corner watchtowers, a two-storey gate tower and tiled roofs; red banners on the walls. In the middle, on a hexagonal pattern of fields faintly visible in the ground, three small squads of soldiers: a block of five spearmen with a banner bearer, a line of four crossbowmen, a wedge of four horsemen with a banner. On the right, a river with a timber pier and one moored river boat, and a mountain pass with a fortified gate wall between two cliffs. Pine forest, terraced fields, a dirt main road. Calm weather, soft late-afternoon light from the upper left. The soldiers are small compared with the city, as on a strategy map.
[MASK-RED]
[NEGATIVE]
```

## 4. Units

### 4.1 Model sheets
Make one model sheet for each unit. Each prompt is STYLE, then the unit
block, then SMALL, MASK-RED, BG-GREEN, LAYOUT-SHEET and NEGATIVE.

A unit on the map is a crowd of up to 80 figures. Each figure is 20 to
30 pixels tall on the screen. Thus each foot soldier and each rider
needs a simple, bold shape. Paste this block after the unit block:

SMALL:
```
The figure must stay readable when it is shrunk to 24 pixels tall: a bold simple silhouette, large clear shapes, the weapon slightly oversized and clearly visible, no fine detail, few lines. The weapon tells the class at a glance: a long spear, a halberd with a side blade, a crossbow, or a horse.
```

Sword (劍兵):
```
Subject: one Han dynasty light infantryman with no long weapon: a straight iron ring-pommel sword in the right hand, a small round-topped wooden shield on the left arm, a quilted knee-length tunic belted at the waist, wrapped leggings, cloth shoes, a soft cloth cap. Plain, a little shabby: a soldier without equipment.
```

Pike (槍兵):
```
Subject: one Han dynasty spearman: a long spear about twice his height with a leaf-shaped iron head and a dark wooden shaft, held upright in both hands; a lamellar vest of small lacquered plates over a quilted tunic; a rounded iron helmet with a short plume; a sash at the waist; wrapped leggings; cloth boots.
```

Halberd (戟兵):
```
Subject: one Han dynasty halberdier: a ji halberd taller than himself, with a straight spear point and a side blade at a right angle, held across the body in both hands; heavy lamellar armour down to the thighs with shoulder guards; a rounded iron helmet with neck flaps and a plume; a sash; broad stance; the toughest-looking of the foot soldiers.
```

Crossbow (弩兵):
```
Subject: one Han dynasty crossbowman: a wooden crossbow with a bronze trigger box held at the hip, a quiver of short bolts on the right hip, a light lamellar vest over a quilted tunic, a cloth head wrap with a small plume, a sash, wrapped leggings. Lean build, alert pose.
```

Cavalry (騎兵):
```
Subject: one Han dynasty cavalryman on a stocky steppe horse: the rider wears a lamellar coat, a rounded iron helmet with a plume and a sash, and holds a long ji halberd raised; a saddle with high front and back, a saddle cloth, simple bridle; the horse is bay brown with a dark mane, tail tied up, mid-stride. Rider and horse together fit one sprite.
```

Banner bearer:
```
Subject: one Han dynasty standard bearer on foot: a tall bamboo pole with a long vertical rectangular banner hanging from a crossbar at the top; the banner is plain and empty, no symbol, no character, with a short fringe at the bottom edge; light lamellar vest, rounded helmet with a plume, sash, both hands on the pole.
```

Ram (衝車):
```
Subject: one Han dynasty battering ram cart: a heavy wooden frame on four solid wooden wheels, a pitched roof of planks covered with wet hides, under the roof a log ram with an iron cap hanging on ropes, two soldiers in quilted tunics visible inside pushing. A small pennant on the roof.
```

Wood beast (木獸):
```
Subject: one Han dynasty "wooden beast" siege engine: a wheeled wooden war machine shaped like a crouching tiger with a carved, painted wooden head and an open mouth that holds a bronze flame nozzle, plank armour on the body, four wooden wheels, two soldiers behind it working a bellows. A small pennant on its back.
```

Tower (井闌):
```
Subject: one Han dynasty mobile siege tower: a tall square wooden tower on four big wheels, three storeys of timber frame with plank walls and arrow slits, an open fighting platform with a parapet at the top where two archers stand, ladders inside, a pennant on a pole at the top corner.
```

Catapult (投石):
```
Subject: one Han dynasty traction trebuchet: a wooden frame on a wheeled base, a long throwing arm with a sling and a basket at the short end, pull ropes hanging from the short end, a pile of round stones beside it, two soldiers at the ropes. A small pennant on the frame.
```

Transport (輸送隊):
```
Subject: a small supply column: one two-wheeled ox cart loaded with grain sacks and wooden boxes, an ox in front, one porter with a shoulder pole and two baskets, one guard with a spear. A small pennant on the cart.
```

Boat (走舸):
```
Subject: one small Han dynasty fast river boat seen down to the water line only (no water drawn): a narrow wooden hull, six oars on each side, three soldiers with crossbows sitting in the middle, a small pennant at the stern. The hull ends cleanly at the water line.
```

Tower ship (樓船):
```
Subject: one Han dynasty tower ship seen down to the water line only (no water drawn): a broad wooden hull with two storeys of deck houses, plank walls with arrow slits, a tall mast with a rectangular mat sail, banners on the top deck, archers on the upper deck. The hull ends cleanly at the water line.
```

Battle ship (鬥艦):
```
Subject: one Han dynasty heavy warship seen down to the water line only (no water drawn): a long reinforced wooden hull with an iron-shod ram at the bow, a high bulwark with shields, one deck house, a small traction catapult on the foredeck, a mast with a mat sail, banners. The hull ends cleanly at the water line.
```

### 4.2 Animation strips
Each strip prompt is STYLE, then this line, then the action block, then
MASK-RED, BG-GREEN, LAYOUT-STRIP with N, and NEGATIVE.
```
Using the attached model sheet, draw exactly the same character with the same equipment, colours and proportions.
```
Make each strip two times: with LAYOUT-STRIP and with LAYOUT-STRIP-BACK.
The engine mirrors the two views for the other two directions. The
engine draws the projectiles, the flames and the dust. Thus the strips
show only the bodies.

Idle, 2 frames, all foot units, the banner bearer and cavalry:
```
Action: idle. Frame 1: standing ready, weight on both feet. Frame 2: the same pose with a slight breath, shoulders a little higher, weapon tip moved a few pixels. Cavalry: the horse shifts its weight and lifts its head a little in frame 2.
```

Walk, 4 frames, all foot units and the banner bearer:
```
Action: walk cycle at marching pace, weapon carried upright. Frame 1: left foot forward, heel down. Frame 2: passing pose, legs together, body slightly higher. Frame 3: right foot forward, heel down. Frame 4: passing pose again. The banner bearer keeps the pole upright and the banner sways a little.
```

Trot, 4 frames, cavalry:
```
Action: horse trot. The four trot phases, diagonal leg pairs moving together, rider upright, halberd held upright.
```

Gallop, 4 frames, cavalry:
```
Action: full gallop. The four gallop phases including one frame with all hooves off the ground, mane and tail flying, rider leaning forward with the halberd levelled. Do not draw dust.
```

Melee attack, 4 frames, sword, pike and halberd:
```
Action: melee attack toward the lower-left. Frame 1: wind-up, weapon drawn back, body twisted. Frame 2: lunge, front foot far forward, weapon fully extended toward the lower-left. Frame 3: follow-through. Frame 4: recover to the ready pose. Sword: overhead cut with the shield forward. Pike: straight thrust. Halberd: chopping swing with the side blade.
```

Melee, 3 frames, crossbow:
```
Action: close defence. Frame 1: crossbow raised in both hands like a club. Frame 2: swing down toward the lower-left. Frame 3: recover, crossbow back at the hip.
```

Thrust, 4 frames, pike (for 突刺, 螺旋突刺, 二段突刺):
```
Action: a powerful two-handed spear thrust. Frame 1: deep crouch, spear pulled back along the body. Frame 2: explosive step forward, spear half extended. Frame 3: full extension, body in a straight line from back foot to spear point, sash and plume flying forward. Frame 4: spear twisted at full extension, ready to pull back.
```

Sweep, 4 frames, halberd (for 橫掃):
```
Action: a wide horizontal sweep of the ji halberd from right to left across the front. Frame 1: halberd held far to the right, body coiled. Frame 2: blade passing in front of the body at waist height. Frame 3: blade far to the left, body fully turned. Frame 4: recover, halberd back across the body.
```

Spin, 4 frames, halberd (for 旋風):
```
Action: a full spinning attack, the halberdier turns a complete circle with the halberd held out at arm's length. Frame 1: facing lower-left, halberd extended to the right. Frame 2: back turned, halberd behind. Frame 3: facing right, halberd to the front. Frame 4: back to facing lower-left, halberd extended to the left. Armour skirts and plume flare outward.
```

Hook, 3 frames, halberd (for 熊手):
```
Action: hooking pull. Frame 1: halberd reached far forward and low, side blade hooked down. Frame 2: both hands pulling back hard, body leaning back. Frame 3: halberd pulled close to the chest, feet braced.
```

Shoot, 4 frames, crossbow:
```
Action: crossbow shot. Frame 1: kneeling, foot in the stirrup of the crossbow, pulling the string back. Frame 2: placing a bolt. Frame 3: aiming from the shoulder toward the lower-left, the crossbow raised slightly upward for a long arc. Frame 4: the instant after release, the crossbow kicks up a little. Do not draw the bolt in flight.
```

Volley upward, 3 frames, crossbow (for 亂射):
```
Action: shooting high into the sky. Frame 1: kneeling, crossbow raised steeply at 60 degrees. Frame 2: release, the crossbow kicks up. Frame 3: lowering the crossbow to reload. Do not draw the bolts.
```

Charge, 3 frames, cavalry (for 突擊, 突破, 突進):
```
Action: cavalry charge impact. Frame 1: horse at full gallop, rider leaning far forward, halberd levelled like a lance. Frame 2: horse rearing at the moment of impact, halberd thrust forward. Frame 3: horse landing, rider pulling the halberd back.
```

Hit, 2 frames, all foot units and cavalry:
```
Action: taking a hit. Frame 1: recoiling backward, head down, weapon pushed aside, shield or arm raised. Frame 2: staggering, one foot stepping back, off balance. No blood.
```

Fall, 3 frames, all foot units and cavalry:
```
Action: defeated, falling. Frame 1: knees buckling, weapon slipping from the hands. Frame 2: on one knee, body bent. Frame 3: lying on the ground on the side, weapon beside the body. Cavalry: the horse stumbles and the rider falls off in frame 3. No blood, no gore.
```

Banner drop, 3 frames, banner bearer:
```
Action: the banner falls. Frame 1: the bearer staggers, the pole tilts. Frame 2: the pole at 45 degrees, the banner flapping. Frame 3: the pole on the ground, the banner lying flat, the bearer kneeling.
```

Ram, use the frame count of each line:
```
Roll, 2 frames: wheels in two rotation phases. Swing, 3 frames: frame 1 log pulled back on the ropes; frame 2 log swung fully forward, iron cap out of the front; frame 3 log rebounding. Broken, 1 frame: roof collapsed, one wheel off, timbers splintered, no soldiers.
```

Wood beast:
```
Roll, 2 frames: wheels in two rotation phases. Flame, 3 frames: frame 1 mouth opening; frame 2 mouth wide open with the bronze nozzle glowing orange; frame 3 mouth closing. Do not draw the flame jet. Broken, 1 frame: head cracked, body tilted, timbers splintered.
```

Tower:
```
Roll, 2 frames: wheels in two rotation phases, the tower slightly swaying. Shoot, 3 frames: frame 1 archers on the top draw; frame 2 release upward; frame 3 reload. Broken, 1 frame: top platform collapsed, tower leaning.
```

Catapult:
```
Roll, 2 frames: wheels in two rotation phases. Throw, 4 frames: frame 1 arm cocked low with the stone in the sling; frame 2 soldiers pulling the ropes, arm rising; frame 3 arm vertical, sling whipping; frame 4 arm past vertical, sling empty. Do not draw the stone in flight. Broken, 1 frame: arm snapped, frame tilted.
```

Boat:
```
Row, 2 frames: frame 1 oars forward; frame 2 oars back. Shoot, 2 frames: crossbows aimed; crossbows released. Sink, 3 frames: hull tilting; half below the water line; only the stern above the water line.
```

Tower ship and battle ship:
```
Sail, 2 frames: sail and banners in two wind phases. Shoot, 2 frames: archers on the upper deck draw; release. Ram, 2 frames: hull pitched forward with the bow down; level again. Throw, battle ship only, 3 frames: catapult on the foredeck cocked; arm rising; arm released. Sink, 3 frames: hull listing to one side; half below the water line; mast and bow only.
```

Transport, walk, 4 frames:
```
Action: the supply column moves. Frames 1 to 4: the walk cycle of the ox, the cart wheels in four rotation phases, the baskets of the porter swinging on the shoulder pole, the guard walking.
```

## 5. Bases
The engine draws the bases as 3D meshes (docs/GRAPHICS.md, section 5.2).
These sheets are the design for the kit of parts and for an
image-to-3D tool. Each prompt is STYLE, the subject, MASK-RED, BG-GREEN,
the sheet layout below and NEGATIVE. Until the meshes come, the scene
shows the left view of each sheet. Name the file base-<kind>.jpg, with
the kind city, big-city, gate or port, and run misc/art_import.py on it.

Sheet layout for a base:
```
Layout: a concept sheet with three views of the same building on one image: on the left a three-quarter top-down view, in the middle a straight front elevation, on the right a straight top-down plan view. Same scale in all three views, wide gaps between them, nothing touching the image edge.
```

City (都市):
```
Subject: a walled Han dynasty county city that fits inside a hexagonal footprint. Rammed-earth walls with a brick facing and crenellations, a square corner watchtower with a tiled hip roof at each corner, one main gate on the front wall with a two-storey gate tower and heavy wooden doors, no moat. Inside: a governor's hall with a large hip-and-gable roof at the centre, tight rows of grey-tiled houses, a few trees, a market square. Red banners on the corner towers and above the gate.
```

Big city (大都市):
```
Subject: a large walled Han dynasty provincial capital that fits inside a hexagonal footprint, larger and richer than a county city. Paired que towers in front of the main gate, taller walls with a brick facing and crenellations, six corner and wall towers, an inner palace compound with its own wall and a large two-tiered hall roof, a drum tower, rows of houses and workshops, a broad main street from the gate to the palace. Red banners on every tower.
```

Gate (關所):
```
Subject: a Han dynasty mountain pass fortress. A thick wall of stone and rammed earth spans a narrow valley between two steep rocky slopes. In the centre, a massive gate house with a two-tiered tiled roof, a deep arched gateway with heavy iron-studded wooden double doors, crenellated walkways on top, two small watchtowers at the ends of the wall where it meets the cliffs. Red banners along the wall.
```

Port (港):
```
Subject: a Han dynasty river port on a riverbank. On land: a short walled compound with a gate, two warehouses with tiled roofs, a watchtower. On the water side: timber piers on stilts, mooring posts, two small river boats tied to the pier, stacks of crates and grain sacks. Do not draw the water; the piers end at the water line. Red banners on the watchtower and the gate.
```

Damaged variant. Attach the concept sheet of the base:
```
Using the attached concept sheet, draw exactly the same building from the three-quarter top-down view only, in a damaged state: cracks in the walls, some crenellations knocked off, a few roof tiles missing, scorch marks, thin smoke from one roof, one banner torn. Same position, same size.
```

Broken variant. Attach the concept sheet of the base:
```
Using the attached concept sheet, draw exactly the same building from the three-quarter top-down view only, in a broken state: two breaches in the walls, a gate door hanging off, collapsed roofs, rubble at the foot of the walls, small fires, banners down. Same position, same size. Keep flames and smoke small; the engine adds the big fire.
```

Gate doors open. Attach the concept sheet of the base:
```
Using the attached concept sheet, draw the same gate house as a front elevation only, with both doors swung fully open inward and the dark gateway visible.
```

## 6. Textures of the kit
Each prompt is STYLE, then this template with one row of the table.
```
A seamless tileable texture, square, viewed straight on, flat even light, no perspective, no shadow from outside the tile, the left edge matches the right edge and the top edge matches the bottom edge. Material: [MATERIAL]. Painted in the same gouache and ink-wash style. No text, no logo.
```

| Texture | MATERIAL |
|---|---|
| Rammed earth | horizontal layers of compacted yellow-brown earth with fine pole holes in rows |
| Brick facing | grey fired bricks in a running bond with thin lime mortar, a little worn |
| Roof tiles | grey semicircular clay roof tiles in vertical rows, round end caps at the bottom |
| Timber | dark weathered wooden planks with visible grain and iron nails |
| Lacquered wood | red-brown lacquered pillars and beams with small black bands |
| White plaster | white lime plaster wall with faint cracks and a dark wooden skirting |
| Stone wall | large irregular grey stone blocks fitted dry, with moss in the joints |
| Palisade | a row of sharpened logs bound with rope, seen from the side |
| Woven mat | woven reed matting for sails and roofs, yellow-tan |
| Paving | square grey stone paving slabs of a courtyard |

## 7. Facilities and other buildings
Each prompt is STYLE, the template below with one row, MASK-RED,
BG-GREEN and NEGATIVE. The engine uses the result as the design of the
mesh, or as an upright card for a small item.
```
Subject: a single small Han dynasty structure that fits on one hexagonal tile, seen from the three-quarter top-down view, centred, with a small patch of bare ground under it shaped like a hexagon: [DESCRIPTION]
```

| Item | DESCRIPTION |
|---|---|
| 市場 Market Lv1 | a few open stalls with cloth awnings, baskets of goods, one small tiled shop |
| 市場 Lv2 and Lv3 | the same market, larger: Lv2 two rows of stalls and two shops; Lv3 a walled market square with a gate and a two-storey shop |
| 農場 Farm Lv1 | a small farmhouse with a thatched roof, a patch of millet fields, a well |
| 農場 Lv2 and Lv3 | Lv2 more fields and a threshing floor; Lv3 a farmstead compound with a granary hut and terraced fields |
| 兵舍 Barracks Lv1 | one long low barrack hall with a tiled roof and a small yard with weapon racks |
| 兵舍 Lv2 and Lv3 | Lv2 two halls and a drill yard; Lv3 a walled compound with three halls and a gate |
| 鍛冶廠 Smithy Lv1 | an open forge shed with a chimney, an anvil, a rack of spears |
| 鍛冶廠 Lv2 and Lv3 | Lv2 two forges and a store; Lv3 a workshop compound with three chimneys |
| 廄舍 Stable Lv1 | a wooden stable with a hay roof, two horses at a rail, a water trough |
| 廄舍 Lv2 and Lv3 | Lv2 a longer stable and a paddock; Lv3 a large stable yard with a fenced paddock and several horses |
| 工房 Workshop | a timber workshop shed with half-built siege wheels and beams outside |
| 造船廠 Shipyard | a slipway at the water line with a half-built boat hull and a crane of poles |
| 造幣廠 Mint | a compact tiled building with a strong door and stacks of coin strings at the door |
| 穀倉 Granary | two round raised granaries on stilts with conical thatched roofs and a ladder |
| 銅雀台 Bronze Sparrow Terrace | a tall three-storey terrace pavilion on a high earthen platform, a bronze bird figure on the roof ridge |
| 符節台 Tally office | a small official hall with a tiled roof and a raised platform with a flag pole |
| 人材府 Personnel office | a small official hall with an open front where scrolls and seats are visible |
| 計略府 Plot office | a closed dark hall with small windows and a lantern at the door |
| 大市場 Great market | a large walled market with many stalls, two gates and a central tower |
| 黑市 Black market | a few shabby huts and covered carts in a narrow alley |
| 外交府 Diplomacy office | an elegant small hall with a carved gate and two ceremonial flag poles |
| 軍事府 Military office | a fortified small hall with a weapon rack and a war drum at the door |
| 鍊兵所 Drill ground | a fenced square of bare earth with straw targets and weapon racks |
| 魚市 Fish market | stalls with fish baskets beside a small pier |
| 軍屯農 Military farm | soldiers' huts beside millet fields, spears stacked by the field |
| 陣 Camp | a low wooden palisade ring with a gate, two tents and a banner |
| 砦 Fort | a stronger palisade ring with a corner watch platform and a gate tower |
| 城塞 Citadel | a small walled earth fort with four corner towers and a gate house |
| 箭樓 Arrow tower | a tall timber watchtower on four legs with a covered platform and arrow slits |
| 連弩樓 Repeating tower | a heavier timber tower with a roofed platform that holds a large repeating crossbow |
| 投石台 Catapult tower | a raised earthen platform with a fixed traction catapult and a pile of stones |
| 太鼓台 Drum tower | an open platform with a roof and a huge war drum on a stand |
| 軍樂台 Music tower | an open platform with a roof, bells on a frame and horns on a rack |
| 石兵八陣 Stone maze | eight tall rough standing stones in a ring with smaller stones between |
| 土壘 Earth wall | a short straight earthen rampart segment with a wooden fence on top |
| 石牆 Stone wall | a short straight dry-stone wall segment with a crenellated top |
| 火種 Fire seed | a low pile of dry brushwood and small clay jars of oil, half hidden in grass |
| 火焰種 and 業火種 | the same pile, larger, with more jars; 業火種 has sealed black jars |
| 火球 Fire ball | a large ball of bundled brushwood bound with rope, on a slope |
| 火焰球 and 業火球 | the same ball, larger and darker with oil stains |
| 火船 Fire ship | a small old boat loaded with brushwood and oil jars, no crew, cut at the water line |
| 遺跡 Ruin | broken stone platforms and a fallen gate pillar, grass growing through |
| 廟 Temple | a small shrine hall with a tiled roof, a stone stele and an incense burner |
| 堤防 Dike | a long earthen embankment segment with stakes on its face |
| 落石 Rockfall | a heap of large fallen boulders below a cliff face |
| Scaffold, small | a frame of bamboo poles tied with rope, one ladder |
| Scaffold, medium and large | the same frame, larger, with planks and a pulley |

## 8. Terrain tiles
Each prompt is STYLE, then this template with one row. The tile is a top
view, so that it can wrap the top of each hex.
```
A seamless tileable ground texture seen from straight above, square, flat even light, no perspective, the left edge matches the right edge and the top edge matches the bottom edge. Ground: [GROUND]. Painted in the same gouache and ink-wash style, low contrast so that units stay readable on top of it. No text, no objects larger than a bush.
```

| Terrain | GROUND |
|---|---|
| 草地 Grass | short green-ochre grass with small wild flowers |
| 土 Earth | dry brown earth with faint cart ruts |
| 砂地 Sand | pale yellow sand with ripples |
| 濕地 Swamp | dark wet mud with reeds and small pools |
| 毒泉 Poison spring | grey-green mud with sickly yellow-green pools and dead reeds |
| 森 Forest | dense pine canopy seen from above, dark green with shadows |
| 川 Stream | a narrow shallow stream over pebbles |
| 河 River | slow brown-green river water with soft ripples |
| 海 Sea | deep blue-grey sea water with soft wave lines |
| 荒地 Waste | cracked dry ground with sparse dry grass and stones |
| 主徑 Main road | a packed earth road with wheel ruts, grass at the edges |
| 棧道 Plank road | wooden plank walkway on beams over a dark drop |
| 渡所 Ford | shallow water over a gravel bar with stepping stones |
| 淺瀨 Shallows | very shallow clear water over sand |
| 岸 Shore | wet sand and pebbles at the water edge |
| 崖 Cliff | grey rock face with cracks and small ledges |
| 都市 City ground | grey stone paving |
| 港 Port ground | wooden decking and paving |
| 關所 Gate ground | worn stone paving with a central gutter |
| 小徑 Small path | a narrow footpath through rough grass and stones |

For each water tile, make a second frame:
```
Using the attached tile, draw exactly the same tile with the ripples moved a little, so that the two tiles loop as a slow two-frame animation.
```

## 9. Effects
Each prompt is STYLE, the subject, BG-BLACK, LAYOUT-STRIP with N and
NEGATIVE. In these prompts, one frame is one step of the effect. The
importer turns the black background into transparency.

Fireworks burst, 8 frames, for the fall of a base:
```
Subject: a festive fireworks burst in the night sky style of old Chinese celebration paintings: a rising spark trail in frame 1, a round burst of gold and red sparks that opens in frames 2 to 5, falling and fading glitter in frames 6 to 8.
```

Flame loop, 8 frames:
```
Subject: a looping campfire-sized flame, painted with bold ink-wash strokes in yellow, orange and deep red, the flame shape changes through 8 frames and frame 8 leads back to frame 1. The base of the flame sits on the shared baseline.
```

Big fire on a building, 8 frames:
```
Subject: a large roaring fire, three tall tongues of flame with sparks rising, painted in yellow, orange and deep red ink-wash strokes, looping through 8 frames.
```

Smoke puff, 8 frames:
```
Subject: a puff of white smoke that grows from a small round cloud to a large soft cloud and fades to thin wisps over 8 frames, painted in soft white ink-wash.
```

Dust cloud, 6 frames:
```
Subject: a low cloud of dust kicked up by feet and hooves, white, growing outward and fading over 6 frames, wider than tall.
```

Impact debris, 6 frames. Use BG-GREEN instead of BG-BLACK:
```
Subject: an impact burst of broken earth, wood splinters and stone chips flying outward from the centre and falling, over 6 frames.
```

Sparks, 4 frames:
```
Subject: a small star-shaped burst of bright yellow-white sparks from clashing iron weapons, expanding and fading over 4 frames.
```

Water splash, 6 frames:
```
Subject: a white water splash rising as a crown and falling back, over 6 frames.
```

Thunderbolt for 落雷, 4 frames:
```
Subject: a vertical lightning bolt from the top of the frame down to the baseline, brilliant white with a pale violet glow: frame 1 thin, frame 2 full, frame 3 branching, frame 4 fading.
```

Dark vortex for 妖術, 8 frames:
```
Subject: a swirling vortex of dark violet ink with pale glowing streaks, spinning and growing over 8 frames, seen from the three-quarter top-down view as a flat spiral on the ground. No symbols.
```

Calm ripple for 鎮靜, 6 frames:
```
Subject: a soft jade-green ring of light that expands outward on the ground and fades, over 6 frames, seen from the three-quarter top-down view as a flat ellipse.
```

Drum rings for 擾亂, 6 frames:
```
Subject: three concentric pale gold sound rings expanding outward from the centre and fading, over 6 frames, flat on the ground in the three-quarter top-down view.
```

Projectiles, one image. Use BG-GREEN, and no layout block:
```
Subject: five separate small objects in one row with wide gaps: a crossbow bolt pointing right; a fire arrow pointing right with a small flame at the head; a round catapult stone; a rolled bamboo scroll tied with red cord; a burning torch. Each object drawn flat from the side, simple and readable at small size.
```

## 10. Icons
One prompt makes all icons. Use STYLE, this subject, BG-GREEN and
NEGATIVE.
```
Subject: a sheet of eight small round badge icons in one row with wide gaps, each a circle with a thin dark ink border and a warm paper fill, each showing one simple symbol in ink and one accent colour: 1 a dizzy spiral (confusion), 2 a folded letter with a broken seal (false report), 3 a flame (fire), 4 an upward arrow with a small banner (morale up), 5 a hammer and a plank (build), 6 a hammer and sparks (repair), 7 a grain sack (supply), 8 a rope tied in a knot (captive). No letters or characters.
```

## 11. Veo prompts
Veo makes short videos. The game does not play these videos as sprites.
Use them for the title screen, for the backgrounds of the cut-ins and as
motion reference for the strips of section 4.2.

Title screen loop:
```
A slow cinematic aerial shot over a late Eastern Han China landscape at dawn, painted animation style of gouache and ink wash: mist over a wide river, a walled city with red banners in the distance, a column of soldiers with spears and banners marching along a dirt road, a mountain pass fortress on the horizon. The camera glides forward slowly. Calm and epic, no text, no people close to the camera. Ends on a wide view that can loop back to the start.
```

Motion reference for a cavalry charge (突擊):
```
Side view, plain grey studio background, painted animation style: a Han dynasty cavalryman in lamellar armour on a bay steppe horse gallops in from the right, levels a long ji halberd and charges into a straw training dummy, the horse rears at the impact, then lands. Locked-off camera, full body always in frame, slow motion, 4 seconds. No text.
```

Motion reference for a halberd spin (旋風):
```
Side view, plain grey studio background, painted animation style: a Han dynasty halberdier in heavy lamellar armour spins a full circle with a ji halberd held at arm's length, armour skirts flaring, then returns to a ready stance. Locked-off camera, full body in frame, 3 seconds. No text.
```

Motion reference for a crossbow volley (亂射):
```
Side view, plain grey studio background, painted animation style: a line of four Han dynasty crossbowmen kneel, raise their crossbows steeply and release together, the bolts arc high out of frame, then they lower the crossbows and reload. Locked-off camera, 4 seconds. No text.
```

Fall of a city, for the cut-in of a capture:
```
Painted animation style of gouache and ink wash: a rammed-earth city gate under siege at dusk. A wooden battering ram cart strikes the gate twice, the gate doors burst open, soldiers with spears pour through, red banners on the wall tower are pulled down and blue banners rise in their place. The camera holds a three-quarter top-down view with a slow push-in. Smoke and small fires. No gore, no text, 8 seconds.
```

Duel background:
```
Painted animation style of gouache and ink wash: an empty battlefield clearing at sunset, trampled grass, broken spears in the ground, drifting dust and embers, two lines of distant soldiers with banners at the left and right edges. Static wide shot with gentle drifting dust, loopable, nobody in the centre, no text, 8 seconds.
```

## 12. The check after each image
Look at each image before the import. Make it again if one of these
items is wrong:
- The figure is the same character as the model sheet.
- The frame count is correct, and each frame stands apart from the next.
- The feet sit on one line.
- Red is only on the uniforms, banners, sashes, plumes and shields.
- A foot soldier stays readable when you shrink the image to 24 pixels.
- The background is one flat colour.
- The image has no text, no mark and no frame.
