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
content pack. The owner of this repository added the archive on
2026-10-18.

| Item | Value |
|---|---|
| Contents | 124 JPEG files: 120 numbered pictures and 4 extras ("EXTRA - ...") |
| Names | `Three Kingdoms Portraits/<faction>/<number> <English name> <Chinese name>.jpg` |
| Size | 1792 x 2240 or 1856 x 2304 pixels each, 115 MB in all |
| Source | generated pictures, not art of the original game |
| Licence | not recorded yet |

The source facts come from a check on 2026-10-18:
- Five files have a C2PA manifest from Google. The manifest marks them as
  "trainedAlgorithmicMedia", that is, made by an image generator.
- The other files have no metadata. They have the same size and style.
- Each picture shows a bust in a painted style at high resolution. No
  picture is a portrait from the original game.

Before a release, record the licence of the pictures here. The terms of
the image generator give that licence.

The archive is larger than 20 MB, so git does not hold it. .gitignore
lists it. To use the portraits, copy the archive to
data\civil_war\Portraits.zip. If the file is absent, the UI shows the
first character of the name in place of a portrait.

### How the game uses the archive
1. At start, the platform layer maps the archive into memory
   (code/win32_portraits.h). It reads the central directory with
   code/render/zip.h.
2. code/render/portraits.h makes a list of the pictures from the member
   names: the number and the Chinese name.
3. The UI picks the picture of an officer (ui_officer_portrait):
   - the picture with the Chinese name of the officer, if there is one;
   - else the numbered picture at (officer id modulo 120), in the order of
     the numbers. The same id always gets the same picture.
4. When the UI first asks for a picture, the platform decodes it with
   code/render/jpeg.h at one eighth of its size, 224 x 280 pixels. It
   decodes one picture in each frame at most. The texture stays until the
   game closes.

The unit plates and the officer view of the Court panel show the
portraits.

## Text of the content pack (data/placeholder)
The project wrote these files. They contain no text from the original game.
- text/locales.tsv, text/zh-TW.tsv and text/en.tsv: the UI strings.
- names/en.tsv: historical names in pinyin, keyed by their Big5 bytes.
