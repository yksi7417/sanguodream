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

SHA-256 of the files in the repository:
- NotoSansCJKtc-Regular.otf: dce08bd4fd91aa8aa76ed8fea4b694c2dfb8550f67871e326843212ddbeb88b4
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
1. Latin, Latin-1 and Vietnamese.
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
```
Then compare the SHA-256 values with the values above.

## Unicode data (data/unicode)
| File | Source | Licence |
|---|---|---|
| BIG5.TXT | unicode.org, Public/MAPPINGS/OBSOLETE/EASTASIA/OTHER | Unicode License v3, LICENSE-UNICODE.txt |

misc/make_big5_table.py reads BIG5.TXT and writes code/text/big5_table.h.
The game does not read BIG5.TXT at run time. See data/unicode/README.md.

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

## Base models (data/placeholder/models)
A base kind can have a 3D model in place of its art card. The card faces
the camera, so it turns when the player turns the view. A model stays
fixed on the map. If the pack has the model of a kind, the renderer draws
the model. If not, it draws the card, and without the card the
placeholder mesh.

| Item | Value |
|---|---|
| Files | base_city, base_big_city, base_gate and base_port for the bases. facility_market, facility_market_lv2, facility_market_lv3 and facility_farm for the shapes of the facilities. Each model has two files: <name>.hxm and <name>.png. |
| Mesh | the HXM1 format of the engine: positions, normals and uv. The larger side of the footprint is one unit. The ground is at y = 0. |
| Texture | the base colour of the model, RGB, 1024 pixels at most. The renderer makes the mip levels at the start. |
| Source | a binary glTF file (GLB). misc/glb_import.py converts it. provenance.tsv in the folder has one row for each model. |
| Licence | the licence of the source of each model, in provenance.tsv. CC-BY-4.0.txt gives the credit and the licence of the Meshy models. |
| Models now | Eight models from Meshy, CC BY 4.0. The table below gives each model. |

| Model | Kind | Source GLB | Triangles | Mesh file | Licence |
|---|---|---|---|---|---|
| base_big_city | the big city | Meshy_AI_Walled_Imperial_City_1026143943_texture.glb | 354,640 | 10.9 MB | CC BY 4.0, credit "Meshy" |
| base_city | the city | city-30k.glb | 30,100 | 1.2 MB | CC BY 4.0, credit "Meshy" |
| facility_farm | the farm shape: the farm, the military farm | farm-lv1-game-ready.glb | 39,730 | 1.8 MB | CC BY 4.0, credit "Meshy" |
| base_gate | the pass | gate-30k.glb | 32,164 | 1.4 MB | CC BY 4.0, credit "Meshy" |
| base_port | the port | port-30k.glb | 29,712 | 1.2 MB | CC BY 4.0, credit "Meshy" |
| facility_market | the market shape at level 1: the market, the great market, the black market, the fish market | market-lv1-30k.glb | 30,168 | 1.1 MB | CC BY 4.0, credit "Meshy" |
| facility_market_lv2 | the market shape at level 2 | market-lv2-30k.glb | 29,488 | 1.2 MB | CC BY 4.0, credit "Meshy" |
| facility_market_lv3 | the market shape at level 3 | market-lv3-game-ready.glb | 38,060 | 1.6 MB | CC BY 4.0, credit "Meshy" |

The owner of the repository made the city, the pass, the port, the farm
and the three markets on 2026-10-27. The GLB files came from the shared Google Drive folder "Three
Kingdoms Animations — Map Art", subfolder 07-3D-Models. Each texture is
1024 x 1024 pixels.

The model of a facility replaces the placeholder shape of its kind
(render_mesh_model_files in code/render/render.h). Each facility kind with
that shape shows the model. The model keeps the scale of the shape, so it
covers the same part of the slot hex. A build in progress shows the model
under the scaffold.

A facility has a level from 1 to 3. The scene gives the level to the
renderer (render_push_facility). render_mesh_model_files has a column for
each level. A level without a model uses the model of the next lower
level. Now only the market has a model for the levels 2 and 3.

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

The scene scales the model to the width of the card of its kind
(render_base_model_instance in code/render/render.h). A city model is 5
world units wide. The model stands at the centre of the base. The pixel
shader model_ps shades the texture with the light of the scene. A flat
red pixel of the texture takes the colour of the owner, as the red mask
of the card does. On the model of a facility, a flat red pixel takes the
colour of the facility kind (ui_facility_look.cpp).

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

This file of the folder is not in the game yet (2026-10-27):
- big-city-v2-rectangular-30k.glb: base_big_city already has a model.
  Give --replace base_big_city to use it. It has 30,140 triangles in
  place of 354,640, and it makes the download 11 MB smaller.

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
