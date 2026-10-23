# Assets and their licences

This document lists the files under data/ that other people made, and the
rules for a new asset. The engine and the content packs use only assets with
an open licence. No asset comes from the original game.

## The rules for an asset
- Use an asset only if its licence lets you copy and distribute it.
- Keep the licence text next to the asset.
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
plate. The pictures are placeholders until the art of docs/GRAPHICS.md
exists.

| Item | Value |
|---|---|
| Files | unit.png, sword.png, pike.png, halberd.png, crossbow.png, cavalry.png, siege.png, boat.png, ship.png, transport.png |
| Size | 64 x 64 pixels, RGBA, a light figure with a dark outline on a transparent background |
| Source | misc/make_unit_sprites.py draws them with Pillow. They are project work, not art of the original game. |
| Licence | the licence of this repository |

The table render_sprite_files in code/render/render.h names the file of
each render_sprite. The renderer loads the files at start and multiplies
each picture by the colour of the faction. If a file is absent, the
renderer uses the figure of a head over a body. The UI picks the sprite of a unit from the
key of its class (ui_unit_sprite in code/ui/ui_war.cpp). The four siege
weapons share siege.png. 樓船 and 鬥艦 share ship.png.

To draw the pictures again, run `python -X utf8 misc\make_unit_sprites.py`
from the root of the repository.

## Text of the content pack (data/placeholder)
The project wrote these files. They contain no text from the original game.
- text/locales.tsv, text/zh-TW.tsv and text/en.tsv: the UI strings.
- names/en.tsv: historical names in pinyin, keyed by their Big5 bytes.
