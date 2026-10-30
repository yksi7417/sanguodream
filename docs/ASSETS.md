# Assets and their licences

This document lists the files under data/ that other people made, and the
rules for a new asset. The engine and the content packs use only assets with
an open licence. No asset comes from the original game.

## The rules for an asset
- Use an asset only if its licence lets you copy and distribute it.
- Keep the licence text next to the asset.
- Add a row for the asset to ui_credits in code/ui/ui_credits.cpp. The
  Credits window of the Settings dialog shows each row. Add its text ids
  to each file under data/placeholder/text.
- Check in an asset only if it is smaller than 20 MB and its licence allows
  it. Otherwise, add a download step to a script under misc/ and write the
  step here.
- Do not copy text, images, sound or fonts from the original game. Read
  the player's own install at run time if the game needs them (ROADMAP.md,
  M9).

## Fonts (data/fonts)
The platform layer loads the fonts at start (code/ui/ui_fonts.h). If the
CJK font is not in data/fonts, the game uses the default ImGui font. That
font shows Latin only, and the log gives the name of the absent file.

| File | Covers | Source | Licence | Size |
|---|---|---|---|---|
| NotoSansCJKtc-Regular.otf | Latin, Vietnamese, CJK ideographs, kana, Hangul, CJK punctuation | https://github.com/notofonts/noto-cjk, Sans/OTF/TraditionalChinese | SIL Open Font License 1.1, OFL-NotoSansCJK.txt | 16.4 MB |
| NotoSansThai-Regular.ttf | Thai | https://github.com/notofonts/notofonts.github.io, fonts/NotoSansThai/hinted/ttf | SIL Open Font License 1.1, OFL-NotoSansThai.txt | 37 KB |
| NotoSans-Regular.ttf | Latin Extended-A and B, spacing modifier letters, combining marks, Latin Extended Additional (E3: "ł", "ř", "ṣ") | https://github.com/notofonts/notofonts.github.io, fonts/NotoSans/hinted/ttf | SIL Open Font License 1.1, OFL-NotoSans.txt | 622 KB |

The CJK font lacks six letters of the native names of docs/EUROPE.md, E3. These
letters are U+0142, U+0159, U+02BF, U+1E25, U+1E63 and U+1E93. The loader merges
NotoSans-Regular.ttf for the Latin Extended ranges, so that each letter has a glyph.

SHA-256 of the files in the repository:
- NotoSansCJKtc-Regular.otf: dce08bd4fd91aa8aa76ed8fea4b694c2dfb8550f67871e326843212ddbeb88b4
- NotoSans-Regular.ttf: 478c558ea716033cd60c03438f628dfa75694dcf6b5f6d505a2f05fd2b4f3823
- NotoSansThai-Regular.ttf: 61cf814eec46b294d6ea4401ac295d0cecd5207bd2331dcc5a15e7301d30ee44

### Fonts that qualify
A replacement font must have an open licence and outlines that
stb_truetype can read (TrueType outlines or CFF outlines). These fonts
qualify:
- Noto Sans CJK TC, SC, JP or KR (SIL OFL 1.1). Each one covers all four
  scripts. The letters of the name select the preferred glyph shapes.
- Source Han Sans (SIL OFL 1.1). It is the same design as Noto Sans CJK.
- Noto Sans Thai and Noto Sans (SIL OFL 1.1), for Thai and for Latin.

These fonts do not qualify:
- A font with only a variable outline. ImGui 1.91 reads static outlines.
- A font of the original game, or a font that comes with Windows. Their
  licences do not let you distribute them.

### How the loader selects the glyphs
ImGui 1.91 puts each glyph into one texture before the first frame. The
loader does not add all of the 65,000 glyphs of the font. It adds:
1. Latin, Latin-1, Vietnamese and Latin Extended.
2. Thai, kana, CJK punctuation and the full-width forms.
3. Each code point of the Big5 table. These are the characters of the
   zh-TW names.
4. Each character of the text files and the names files of each locale in
   data/<pack>/text/locales.tsv. A Korean file adds its Hangul.

The result is approximately 14,500 code points. The log of the game window
gives the number. If you add a locale, the loader adds its characters at
the next start. You do not change code.

### Download the fonts again
If data/fonts does not have the font files, get them again:
```
curl -L -o data\fonts\NotoSansCJKtc-Regular.otf https://github.com/notofonts/noto-cjk/raw/main/Sans/OTF/TraditionalChinese/NotoSansCJKtc-Regular.otf
curl -L -o data\fonts\NotoSansThai-Regular.ttf https://github.com/notofonts/notofonts.github.io/raw/main/fonts/NotoSansThai/hinted/ttf/NotoSansThai-Regular.ttf
curl -L -o data\fonts\NotoSans-Regular.ttf https://github.com/notofonts/notofonts.github.io/raw/main/fonts/NotoSans/hinted/ttf/NotoSans-Regular.ttf
```
Then compare the SHA-256 values with the values above.

## Unicode data (data/unicode)
| File | Source | Licence |
|---|---|---|
| BIG5.TXT | unicode.org, Public/MAPPINGS/OBSOLETE/EASTASIA/OTHER | Unicode License v3, LICENSE-UNICODE.txt |

misc/make_big5_table.py reads BIG5.TXT and writes code/text/big5_table.h.
The game does not read BIG5.TXT at run time. See data/unicode/README.md.

## Map data (data/medieval_europe/map)
misc/make_world_map.py makes the map of a pack from these sources. It
downloads them to build/geo_cache, out of git. The map files of the pack
contain only values that the tool derived from them.

| Source | Licence | What the tool uses |
|---|---|---|
| NOAA ETOPO 2022, 60 arc-second surface elevation, doi:10.25921/fd45-gt74 | U.S. Government work, public domain | The height of each hex |
| Natural Earth 1:50m: land, lakes, rivers, geography regions | Public domain | The coasts, the lakes, the rivers, the deserts and the deltas |

The positions of the cities in map/source/sites.tsv are facts. Wikidata
(CC0) has the same values. docs/MAPS.md lists the other sources that a
commercial game can use, and the sources that it must not use.

## Default portraits (data/civil_war/Portraits.zip)
The game shows these pictures as the portraits of the officers in each
content pack. The owner of this repository added the first 124 pictures
on 2026-10-18 and 159 more on 2026-10-23.

| Item | Value |
|---|---|
| Contents | 279 JPEG files, numbered 1 to 279, one for each officer |
| Names | `Three Kingdoms Portraits/<faction>/<number> <English name> <Chinese name>.jpg` |
| Size | 224 x 280 pixels each, baseline JPEG, quality 85, 4:2:0; 3.4 MB in all |
| Source | generated pictures, not art of the original game |
| Licence | not recorded yet |

The sources are two archives of 1792 x 2240 pictures (a few are 1856 x
2304), 115 MB and 149 MB. The game shows a portrait at 6 text lines at
most, approximately 130 pixels tall. A 224 x 280 picture is more than two
times that size. misc/make_portraits.py makes the archive of the pack
from the sources. It fits each picture to 224 x 280 and writes a baseline
JPEG at quality 85 with 4:2:0 chroma. It stores the files in the zip
without deflate, because a JPEG does not compress. The re-encode drops
the metadata of the sources. The four extras of the
first archive have numbered pictures in the second, so the pack holds the
numbered ones only. Two officers share the name 馬忠, one of Shu and one
of Wu. Both have a picture, and the game shows the first for both.

The source facts come from a check on 2026-10-18:
- Five source files have a C2PA manifest from Google. The manifest marks
  them as "trainedAlgorithmicMedia", that is, made by an image generator.
- The other files have no metadata. They have the same size and style.
- Each picture shows a bust in a painted style. No picture is a portrait
  from the original game.

Before a release, record the licence of the pictures here. The terms of
the image generator give that licence.

The archive is smaller than 20 MB, so git holds it. If the file is
absent, every officer gets a generated face (the next section).

### How the game uses the archive
1. At start, the platform layer maps the archive into memory
   (code/win32_portraits.h). It reads the central directory with
   code/render/zip.h.
2. code/render/portraits.h makes a list of the pictures from the member
   names: the number and the Chinese name.
3. The UI picks the picture of an officer (ui_officer_portrait): the
   picture with the Chinese name of the officer. An officer without one
   gets a generated face, not the picture of another officer.
4. When the UI first asks for a picture, the platform decodes it with
   code/render/jpeg.h at full size, 224 x 280 pixels. A picture of an
   older archive, 1792 pixels wide, decodes at one eighth to the same
   size. The platform decodes one picture in each frame at most. The
   texture stays until the game closes.

The unit plates, the officer lists and the officer view of the Court
panel show the portraits.

## Generated faces (code/render/faces.h)
An officer without a picture of its name gets a face that the engine
draws on the spot. The face is not an asset. The code makes it, so it
has no licence of its own. The user asked for it on 2026-10-22. The face
must be fast to make and must show an emotion. Before, a minor officer
got the picture of another officer.

| Item | Value |
|---|---|
| Size | 48 x 60 pixels, the 4:5 ratio of the default portraits |
| Traits | head shape (3), skin tone (4), hair tone (3 and grey), hair style (4), eyes (3), beard (3), background (4) |
| Emotions | calm, happy, angry, sad, surprised |
| Inputs | the officer id, the sex, the age and the personality from engine_api |

How the face comes to the screen:
1. The UI reads the sex, the age and the personality of the officer
   (officer_detail). face_describe hashes the officer id with a salt for
   each trait and picks the traits. The same id always gives the same
   face. A woman gets long hair and no beard. An officer under 25 gets no
   beard. An officer of 50 or more gets grey hair and lines under the
   eyes. The personality tints the emotion of one officer in two: bold
   smiles, reckless glares, careful and calm are calm.
2. face_key_pack folds the description into one u32. The UI passes the
   officer id and the key to the platform (platform_api.face_texture).
3. The platform paints the face (face_paint) and makes a texture
   (code/win32_faces.h). It keeps the texture by officer id until the
   game closes. If the key of an officer changes, for example when the
   officer turns 50, the platform paints the face again. It paints at
   most 64 faces in one frame.

The hash is its own function (face_hash). The faces never touch the PRNG
of the sim. tests/unit/faces_tests.cpp checks the generator, and
tests/ui/ui_tests.cpp checks that the UI asks for a face when no picture
has the name of the officer.

## Unit sprites (data/placeholder/sprites)
The scene draws one picture on the hex of each unit. The picture shows
the class of the unit, so the player finds the hex of the unit under its
plate. The renderer loads the art of a class when the pack has it. Else
it loads the placeholder picture of the class. Without both, it draws a
figure of a head over a body.

### The art (data/placeholder/sprites/art)
| Item | Value |
|---|---|
| Files | sword.png, pike.png, halberd.png, crossbow.png, cavalry.png, ram.png, wood_beast.png, tower.png, catapult.png, boat.png, tower_ship.png, battle_ship.png, transport.png, banner.png |
| Size | 256 x 256 pixels, RGBA. The front view of the model sheet, with its feet on the bottom edge. |
| Source | The owner of the repository made the model sheets with the prompts of docs/art/GEMINI-PROMPTS.md and shared them on 2026-10-24 (the Drive folder 01-Units). misc/art_import.py cuts the front view of each sheet. provenance.tsv in the folder has one row for each file. |
| Licence | the licence of this repository; the images are original work of the owner, made with an image model |

The alpha channel carries the mask of the faction colour. A pixel with the
alpha 255 keeps its paint. A pixel with the alpha 160 is the flat red of
the sheet (MASK-RED). The renderer paints it in the colour of the
faction, shaded by its brightness. The pixel shader billboard_ps of
code/d3d11_renderer.h does it. The source sheets stay in art-src/units,
out of git. To import them again, run
`python -X utf8 misc\art_import.py` from the root of the repository.
banner.png waits for the crowds of docs/GRAPHICS.md, section 5.1.

### The placeholder pictures (data/placeholder/sprites)
| Item | Value |
|---|---|
| Files | unit.png, sword.png, pike.png, halberd.png, crossbow.png, cavalry.png, siege.png, boat.png, ship.png, transport.png |
| Size | 64 x 64 pixels, RGBA, a light figure with a dark outline on a transparent background. The figure has the alpha 160, so it takes the colour of the faction. |
| Source | misc/make_unit_sprites.py draws them with Pillow. They are project work, not art of the original game. |
| Licence | the licence of this repository |

The tables render_sprite_art_files and render_sprite_files in
code/render/render.h name the files of each render_sprite. The UI picks
the sprite of a unit from the key of its class (ui_unit_sprite in
code/ui/ui_war.cpp). The four siege weapons share the placeholder
siege.png, and 樓船 and 鬥艦 share ship.png. Each has its own art.

To draw the placeholder pictures again, run
`python -X utf8 misc\make_unit_sprites.py` from the root of the repository.

## Base art (data/placeholder/sprites/art)
The scene draws one picture on each city, each pass and each port. The
picture shows the kind of the base. A big city has its own picture.
Without the picture, the renderer draws the placeholder mesh of the base.

| Item | Value |
|---|---|
| Files | base_city.png, base_big_city.png, base_gate.png, base_port.png |
| Size | 384 pixels wide, RGBA, with the aspect of the view: 299, 368, 301 and 424 pixels high. The three-quarter top-down view of the concept sheet, with the front corner of the walls on the bottom edge. |
| Source | The owner of the repository made the concept sheets with the prompts of docs/art/GEMINI-PROMPTS.md, section 5. The owner shared them on 2026-10-25 as the archive 02-Bases (the folders City, BigCity, Gate and Port). misc/art_import.py cuts the left view of each sheet. provenance.tsv in the folder has one row for each file. |
| Licence | the licence of this repository; the images are original work of the owner, made with an image model |

The alpha channel carries the mask of the faction colour, as for the
units. The red banners of a base take the colour of its owner.

The scene names the art of each base (ui_city_art and ui_base_art in
code/ui/ui_scene.cpp). render_push_base keeps the placeholder mesh in the
command and adds the sprite of the art. The renderer loads the picture
at the start. If the picture is present, the renderer draws it in place
of the mesh, on a card that faces the camera. The bottom edge of the card
stands at the front edge of the base. render_base_art_sizes in
code/render/render.h gives the width of each kind:

| Kind | Width in world units | Covers |
|---|---|---|
| city, big city | 5.0 | the flower of 7 city hexes |
| pass | 2.4 | one hex and the hills at its sides |
| port | 2.2 | one hex |

The art has no damaged or broken state yet. The facilities of a city
keep their placeholder meshes, because the archive has no art for them.
The source sheets stay in art-src/bases, out of git. To import them again,
run `python -X utf8 misc\art_import.py art-src\bases` from the root of
the repository.

## The splash art (data/placeholder/sprites/art/splash.png)
The game shows this picture at the start, before the title menu. The
picture goes after 3 seconds, or after a click or a key
(code/ui/ui_splash.cpp).

| Item | Value |
|---|---|
| File | splash.png |
| Size | 1280 x 720 pixels, 8-bit RGB, 1.7 MB |
| Source | The owner of the repository made the picture with an image model. The owner shared it on 2026-10-29 in the shared Google Drive folder "Three Kingdoms Animations — Map Art" as sanguo-dream-splash.png. The source is 2048 x 1152 pixels and 4.0 MB. provenance.tsv in the folder has its row. |
| Licence | the licence of this repository; the picture is original work of the owner, made with an image model |

The source file stays in art-src/splash, out of git. Its SHA-256 is
d483963318295bea9475ad38911a2d25cca72fe8bf72cee56bb6a095c3d8bddf.
The pack keeps a smaller copy, because each visit to the web game
downloads the pack. To make the copy again, scale the source to
1280 x 720 pixels with a Lanczos filter. Save it as an 8-bit RGB PNG
without interlace, because code/render/png.h decodes only that format.

The platform loads the picture one time after the renderer starts
(HEXDOM_SPLASH_FILE in code/hexdom.h). It gives the texture to the UI
through platform_api.splash_picture. If the file is absent, the splash
shows a dark screen with the hint.

## Coats of arms (data/medieval_europe/arms)
The flag of a faction in the game of 1200 shows its coat of arms
(docs/EUROPE.md, E5).

| Item | Value |
|---|---|
| Files | 24 PNG files, one for each force key, for example france.png |
| Size | 256 x 256 pixels, 8-bit RGBA with a transparent background, 1.3 MB in all |
| Source | The owner of the repository made the pictures with Google Gemini. The owner composited Welf and Hungary and edited Denmark. The owner shared them on 2026-10-29 in a shared Google Drive folder at 1024 x 1024 pixels. sources.tsv in the folder gives the Gemini link of each picture. |
| Licence | generated content, under the Google Terms of Service. No picture is a file of Wikimedia Commons, so no CC BY-SA credit applies (Q-E3). |

To import new files, put them in a folder with the force key as the name.
Then run misc/arms_import.py with that folder. The tool makes the copies of
256 x 256 pixels and writes arms/arms.tsv of each pack of 1200. The test
unit_arms_file_tests decodes each file of these tables.

The platform loads a file when the UI first asks for it
(PLATFORM_DATA_PICTURE in code/hexdom.h). The path of the file is under the
data folder, and text_arms_file_is_allowed must accept it. If the file is
absent, the flag shows the family name of the ruler.

## Base models (data/placeholder/models)
A base kind can have a 3D model in place of its art card. The card faces
the camera, so it turns when the player turns the view. A model stays
fixed on the map. If the pack has the model of a kind, the renderer draws
the model. If not, it draws the card, and without the card the
placeholder mesh.

| Item | Value |
|---|---|
| Files | base_city, base_big_city, base_gate and base_port for the bases. facility_market, facility_market_lv2, facility_market_lv3, facility_farm, facility_farm_lv2, facility_farm_lv3, facility_barracks, facility_barracks_lv2, facility_barracks_lv3, facility_smithy, facility_smithy_lv2, facility_smithy_lv3, facility_stable, facility_stable_lv2, facility_stable_lv3, facility_shipyard, facility_bronze_sparrow_tower, facility_granary and facility_tally_office for the shapes of the facilities. facility_camp and facility_arrow_tower for the shapes of the field buildings. Each model has two files: <name>.hxm and <name>.png. |
| Mesh | the HXM1 format of the engine: positions, normals and uv. The larger side of the footprint is one unit. The ground is at y = 0. |
| Texture | the base colour of the model, RGB, 1024 pixels at most. The renderer makes the mip levels at the start. |
| Source | a binary glTF file (GLB). misc/glb_import.py converts it. provenance.tsv in the folder has one row for each model. |
| Licence | the licence of the source of each model, in provenance.tsv. CC-BY-4.0.txt gives the credit and the licence of the Meshy models. |
| Models now | 25 models from Meshy, CC BY 4.0. The table below gives each model. |

| Model | Kind | Source GLB | Triangles | Mesh file | Licence |
|---|---|---|---|---|---|
| base_big_city | the big city | big-city-v2-rectangular-30k.glb | 30,140 | 1.2 MB | CC BY 4.0, credit "Meshy" |
| base_city | the city | city-30k.glb | 30,100 | 1.2 MB | CC BY 4.0, credit "Meshy" |
| facility_arrow_tower | the watchtower shape of the field buildings: 箭樓, 連弩樓, 投石台 | arrow-tower-30k.glb | 29,098 | 1.5 MB | CC BY 4.0, credit "Meshy" |
| facility_barracks | the hall shape at level 1: the barracks, the military office, the drill ground | barracks-lv1-30k.glb | 30,468 | 1.2 MB | CC BY 4.0, credit "Meshy" |
| facility_barracks_lv2 | the hall shape at level 2 | barracks-lv2-30k.glb | 29,382 | 1.1 MB | CC BY 4.0, credit "Meshy" |
| facility_barracks_lv3 | the hall shape at level 3 | barracks-lv3-30k.glb | 28,971 | 1.8 MB | CC BY 4.0, credit "Meshy" |
| facility_bronze_sparrow_tower | the tower shape at level 1: the bronze sparrow tower | bronze-sparrow-terrace-30k.glb | 29,801 | 2.2 MB | CC BY 4.0, credit "Meshy" |
| facility_camp | the camp shape of the field buildings: 陣, 砦, 城塞 | camp-30k.glb | 30,598 | 2.1 MB | CC BY 4.0, credit "Meshy" |
| facility_farm | the farm shape at level 1: the farm, the military farm | farm-lv1-game-ready.glb | 39,730 | 1.8 MB | CC BY 4.0, credit "Meshy" |
| facility_farm_lv2 | the farm shape at level 2 | farm-lv2-30k.glb | 33,288 | 1.5 MB | CC BY 4.0, credit "Meshy" |
| facility_farm_lv3 | the farm shape at level 3 | farm-lv3-30k.glb | 28,238 | 1.2 MB | CC BY 4.0, credit "Meshy" |
| facility_granary | the silo shape at level 1: the granary, the mint | granary-30k.glb | 30,858 | 2.2 MB | CC BY 4.0, credit "Meshy" |
| base_gate | the pass | gate-30k.glb | 32,164 | 1.4 MB | CC BY 4.0, credit "Meshy" |
| base_port | the port | port-30k.glb | 29,712 | 1.2 MB | CC BY 4.0, credit "Meshy" |
| facility_market | the market shape at level 1: the market, the great market, the black market, the fish market | market-lv1-30k.glb | 30,168 | 1.1 MB | CC BY 4.0, credit "Meshy" |
| facility_market_lv2 | the market shape at level 2 | market-lv2-30k.glb | 29,488 | 1.2 MB | CC BY 4.0, credit "Meshy" |
| facility_market_lv3 | the market shape at level 3 | market-lv3-game-ready.glb | 38,060 | 1.6 MB | CC BY 4.0, credit "Meshy" |
| facility_shipyard | the dock shape at level 1: the shipyard | shipyard-30k.glb | 30,134 | 1.6 MB | CC BY 4.0, credit "Meshy" |
| facility_smithy | the forge shape at level 1: the smithy, the workshop | smithy-lv1-30k.glb | 29,914 | 1.9 MB | CC BY 4.0, credit "Meshy" |
| facility_smithy_lv2 | the forge shape at level 2 | smithy-lv2-30k.glb | 30,384 | 1.9 MB | CC BY 4.0, credit "Meshy" |
| facility_smithy_lv3 | the forge shape at level 3 | smithy-lv3-30k.glb | 30,448 | 2.3 MB | CC BY 4.0, credit "Meshy" |
| facility_stable | the shed shape at level 1: the stable | stable-lv1-30k.glb | 30,085 | 2.0 MB | CC BY 4.0, credit "Meshy" |
| facility_stable_lv2 | the shed shape at level 2 | stable-lv2-30k.glb | 30,186 | 2.1 MB | CC BY 4.0, credit "Meshy" |
| facility_stable_lv3 | the shed shape at level 3 | stable-lv3-30k.glb | 30,311 | 2.2 MB | CC BY 4.0, credit "Meshy" |
| facility_tally_office | the office shape at level 1: the tally office, the personnel office, the plot office, the diplomacy office | tally-office-30k.glb | 29,763 | 2.3 MB | CC BY 4.0, credit "Meshy" |

The owner of the repository made the first eight models on 2026-10-27.
The models of the barracks and of the farm at levels 2 and 3 came on
2026-10-28. The models of the smithy, the stable, the shipyard and the
barracks at level 3 came later on 2026-10-28. The model of the bronze
sparrow tower came in the evening of 2026-10-28. The models of the
granary and of the tally office came on 2026-10-29. The models of the
camp and of the arrow tower came later on 2026-10-29. The GLB
files came from the shared Google Drive folder "Three Kingdoms Animations
— Map Art", subfolder 07-3D-Models. Each texture is 1024 x 1024 pixels.
The first model of the big city had 354,640 triangles.
big-city-v2-rectangular-30k.glb replaced it on 2026-10-27, and the
download became 11 MB smaller.

The model of a facility replaces the placeholder shape of its kind
(render_mesh_model_files in code/render/render.h). Each facility kind with
that shape shows the model. The model keeps the scale of the shape, so it
covers the same part of the slot hex. A build in progress shows the model
under the scaffold.

A facility has a level from 1 to 3. The scene gives the level to the
renderer (render_push_facility). render_mesh_model_files has a column for
each level. A level without a model uses the model of the next lower
level. The market, the farm, the hall, the forge and the shed shapes have
a model for each level. The dock, the tower, the silo and the office
shapes have a model for level 1 only, so the levels 2 and 3 use it.
A field building has no level. The camp and the watchtower shapes use
the model of level 1. The red banners of a model show the colour of the
owner.

A model with many triangles costs time on each frame. The map has many
facilities, so the renderer uses two rules:
- The renderer draws a model only if the sphere that holds the model is
  in the view (render_model_in_view).
- If the camera is farther than 50 units (RENDER_SHAPE_MODEL_MAX_DISTANCE),
  each facility shows its placeholder shape. The models of the bases stay.

On 2026-10-27 the wide view of the map took 326 ms for each frame with
the market model and no rules. With the rules it takes 33 ms, the same
time as without the facility models. A near view with three markets
takes 22 ms. A new model must have 40,000 triangles or less. Meshy can
export a model with a lower count of triangles, for example city-30k.glb.
The market of the first import had 354,934 triangles. market-lv1-30k.glb
replaced it on the same day.

The scene scales the model to the width of its kind
(render_base_model_widths and render_base_model_instance in
code/render/render.h). A city model is 5 world units wide. The model
stands at the centre of the base. The pixel
shader model_ps shades the texture with the light of the scene. A flat
red pixel of the texture takes the colour of the owner, as the red mask
of the card does. On the model of a facility, a flat red pixel takes the
colour of the facility kind (ui_facility_look.cpp).

The scene turns the models of the ports and the passes (ui_scene.cpp):
- The pier and the boats of the port model are on its local +Z side.
  ui_port_yaw turns that side to the neighbour hex of water with the most
  water beside it. Water is a terrain that a ship can enter.
- The passage of the gate model runs along its local Z axis. ui_gate_yaw
  turns it along the road through the pass.

The player asked on 2026-10-28 for a pass that spans three hexes. The
model must be a long wall with a gate tower and a door on each face. It
has no town inside. The wall stands across the road, on the hex of the pass and the cliff on
each side. RENDER_GATE_WALL_WIDTH gives that width, 3 times the distance
between two hex centres. The base_gate model of today is a square fort,
so it keeps the width of its art (2.4 units). When the wall model comes,
do these steps:
1. Import it as base_gate with the wall along the local X axis.
2. Set the gate row of render_base_model_widths to RENDER_GATE_WALL_WIDTH.

To import the new models of the Drive folder, run misc/drive_models.py
from the root of the repository. The script needs no sign-in, because the
owner shares the folder by link. It does these steps:
- It finds the model name of each GLB from the start of its file name:
  big-city, city, gate, port, market-lv1, market-lv2, market-lv3 and
  farm-lv1.
- It skips a GLB that provenance.tsv names as a source, and a GLB with
  more than 40,000 triangles.
- It keeps a model that the pack has, unless you give --replace NAME.
- It downloads each new GLB one time into art-src/models/drive, out of
  git, and imports it with misc/glb_import.py.
- It writes the row of the model in provenance.tsv.

    python -X utf8 -E misc\drive_models.py --list
    python -X utf8 -E misc\drive_models.py
    python -X utf8 -E misc\drive_models.py --replace facility_market

--list prints the decision for each file and changes nothing. After an
import, add the model to the table above, run build.bat and
run_tests.bat, and look at the model in the game.

On 2026-10-27 the game has a model from each name of the folder. The
other GLBs of the folder are larger exports of the same models. Each of
them has more than 40,000 triangles.

To import one GLB by hand, put it in art-src/models, out of git. Then run
this command from the root of the repository. The name is one of the
names in the "Files" row.

    python -X utf8 misc\glb_import.py art-src\models\<file>.glb base_big_city

If the front of the model faces the wrong way, add `--yaw 90`, or
another angle in degrees. The script refuses a GLB with a wrong length in
its header. A text transfer of a binary file causes this error.

## Text of the content pack (data/placeholder)
The project wrote these files. They contain no text from the original game.
- text/locales.tsv, text/zh-TW.tsv and text/en.tsv: the UI strings.
- names/en.tsv: historical names in pinyin, keyed by their Big5 bytes.
