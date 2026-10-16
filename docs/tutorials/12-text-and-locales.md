# 12 Text and locales: Big5 names, UI strings and fonts

Files: code/text/text.h, code/text/text.cpp, code/text/big5_table.h,
misc/make_big5_table.py, code/ui/ui_fonts.h, code/ui/ui_settings.cpp,
data/placeholder/text/, data/placeholder/names/, data/fonts/.
Tests: tests/unit/text_tests.cpp, the name and locale tests in
tests/ui/ui_tests.cpp.

## What it is for
This tutorial covers the text module. The module turns the Big5 names of
the save into UTF-8 for the panels. It also gives the panels their strings
in the language that the player selects. The first locale is zh-TW. The
design accepts Korean, Japanese, Thai, Vietnamese and English as data
files, with no change to the panels.

The text module is a part of the front end, as code/ui is. The engine
keeps each name as the bytes of the save (tutorial 09). The module has no
Windows headers, no CRT file calls and no ImGui. It reads files only
through the platform reader that the caller gives it.

## Big5 to Unicode
The spec does not name the Big5 variant of the names (QUESTIONS.md,
Q-013). The module uses the Unicode table data/unicode/BIG5.TXT. The
licence of that file lets the repository keep a copy (data/unicode/README.md).

misc/make_big5_table.py writes code/text/big5_table.h from BIG5.TXT. The
header has a table in two levels:
- `big5_row_of_lead[lead - 0x81]` gives the row of a lead byte, or 0.
- `big5_rows[row - 1][trail index]` gives the code point, or 0.

The trail index is 0..62 for the trail bytes 0x40..0x7E, and 63..156 for
the trail bytes 0xA1..0xFE. The table has 88 rows and 13,703 pairs. It
uses approximately 28 KB.

`text_big5_pair_to_code_point` adds the user-defined area with a formula.
The table does not hold these rows:

| Lead bytes | Code points |
|---|---|
| 0xFA..0xFE | U+E000..U+E310 |
| 0x8E..0xA0 | U+E311..U+EEB7 |
| 0x81..0x8D | U+EEB8..U+F6B0 |

These code points are in the Private Use Area. Each pair has its own code
point, and `text_big5_from_private_use` gives the pair back. Thus a name
with an unknown character still has a unique form in UTF-8. Q-013 names
three officers with the bytes FA40 and FA45. They decode to U+E000 and
U+E005. The font has no glyph for these code points, so ImGui shows its
fallback glyph.

`text_decode_big5_into` decodes a name field:
1. It stops at the first NUL byte or at the end of the field.
2. A byte below 0x80 is ASCII.
3. A lead byte with a valid trail byte is one pair.
4. If the next byte is not a trail byte, the lead byte gives U+FFFD. The
   decoder then reads the next byte as a new character.
5. The bytes 0x80 and 0xFF give U+FFFD.
6. It writes only whole characters into the buffer, and a NUL at the end.

`text_utf8_encode` writes one code point as UTF-8. A surrogate or a value
above U+10FFFF gives U+FFFD.

## Make the table again
Do this procedure only if BIG5.TXT or the generator changes.
1. Open a shell at the root of the repository.
2. Run `python -I misc/make_big5_table.py`.
3. If data/unicode/BIG5.TXT is not there, add `--download`.
4. Run `build.bat`, then `build\unit_tests.exe`.

The script prints "88 rows, 13703 pairs with a code point". The line `PASS 104/104
unit/text` shows that the new table agrees with the tests.

## The locale layer
A content pack has three kinds of TSV file. The first row of each file is
the header. A row that starts with "#" is a comment. A third column is a
note for translators, and the module ignores it.

| File | Columns | Contents |
|---|---|---|
| data/<pack>/text/locales.tsv | code, name | each locale, and its name in its own language |
| data/<pack>/text/<locale>.tsv | id, text | the UI strings |
| data/<pack>/names/<locale>.tsv | key, text | names, keyed by the Big5 bytes of one name field as upper-case hex |

In the text column, `\n`, `\t` and `\\` are a new line, a tab and a
backslash. If an id occurs two times, the first row wins.

`text_catalog` holds the tables of one locale. `text_catalog_set_locale`
resets the arena of the catalog and reads the files again. A locale code
has 1 to 15 characters from A-Z, a-z, 0-9, "-" and "_". Thus a code from a
file cannot point at another folder.

`text_get(catalog, id)` looks for the id in three places:
1. text/<locale>.tsv of the active locale.
2. text/<fallback locale>.tsv. The game uses en.
3. If neither file has the id, it returns the id. The catalog counts these
   misses in `stats.missing_strings`.

`text_name(catalog, bytes, size, out, out_size)` looks for a name in two
places:
1. names/<locale>.tsv, with the key of the bytes. For example, the key of
   B1E4 00 00 is "B1E4".
2. If the file has no row, it decodes the bytes. This is the zh-TW form.

Each name field has its own key: the family name, the given name and the
city name. One row thus serves each officer with that family name.

## Where the catalog lives
- code/hexdom.cpp makes the catalog in the permanent block, with an arena
  of 4 MB. It loads zh-TW before the first frame.
- `ui_state.text` points at the catalog. A DLL reload keeps both, because
  both are in the permanent block.
- The Settings panel writes the chosen code into `ui_state.pending_locale`.
  `ui_draw` loads it at the start of the next frame, before a panel asks
  for a string.

The locale is a setting of the front end. The Language combo changes the
catalog and commands nothing in the engine.

## Labels with a stable id
A localized label changes with the locale. ImGui makes the id of a window
or an item from its label. Thus the code adds "###" and a stable id to each
localized label, through `ui_label`. For example, the Officers window has
the label "武將###Officers" in zh-TW and "Officers###Officers" in en. ImGui
uses only the part after "###" for the id.

The tests and the demo use the stable id: `ctx->SetRef("###Officers")`,
`ctx->MenuCheck("###Windows/###Settings")`. The demo layout uses
`[Window][###Officers]`.

## Fonts
code/ui/ui_fonts.h loads data/fonts/NotoSansCJKtc-Regular.otf and merges
data/fonts/NotoSansThai-Regular.ttf. win32_hexdom.exe and demo_player.exe
call `ui_fonts_load` before the first frame. ImGui 1.91 puts each glyph in
one texture, so the loader adds only the glyphs that the game can show.
docs/ASSETS.md gives the list, the licences and the fonts that qualify.

If the CJK font is not there, the game uses the default ImGui font. The
log of the window then has a line that starts with "fonts:". The
headless ui tests do not load fonts.

## Add a locale
This procedure adds Korean. Other locales are the same.
1. Add the row `ko	한국어` to data/placeholder/text/locales.tsv.
2. Copy data/placeholder/text/en.tsv to ko.tsv.
3. Translate the text column of ko.tsv. Keep the id column.
4. If you have Korean names, write data/placeholder/names/ko.tsv.
5. Run `build\ui_tests.exe ui/each_locale_has_each_ui_string`.
6. Start build\win32_hexdom.exe. Open Settings from the Windows menu.

The ui test prints PASS when ko.tsv has each id. The Language combo shows
"한국어". The font loader reads ko.tsv at start, so the Hangul shows.

## Add a UI string
1. Write the id and the text in data/placeholder/text/en.tsv.
2. Write the same id in each other text file.
3. In the panel, call `text_get(ui->text, "the.id")`. For a window, a menu
   item or a column, call `ui_label` with a stable id.
4. Run `build\ui_tests.exe ui/each_locale_has_each_ui_string`.

The test fails if a locale file does not have the id.

## The tests, line by line
- text_tests.cpp, the Big5 table: pairs from BIG5.TXT, for example A440 is
  U+4E00 and B1E4 is U+66F9. B35C has the trail byte 0x5C. The pairs that
  BIG5.TXT maps to U+FFFD, the lead byte C8 and the pair F9FE give U+FFFD.
  Each block of the user-defined area has a test for its first and last
  pair. Invalid trail bytes give U+FFFD.
- text_tests.cpp, the round trip: each of the 5,809 pairs of the
  user-defined area decodes and comes back as the same pair.
- text_tests.cpp, UTF-8: the limits of each length, a surrogate and a value
  above U+10FFFF.
- text_tests.cpp, decode: a name with NUL bytes at the end, a lead byte without a
  trail byte, a small buffer and a full arena.
- text_tests.cpp, tables: a BOM, CRLF, comments, escapes, the note column,
  a repeated id, bad rows and a wrong header.
- text_tests.cpp, the catalog: an absent id, the fallback locale, a locale
  without a file, refused codes and the name lookups. An in-memory reader
  gives the files.
- ui_tests.cpp: the Officers and Cities panels read each name through
  engine_api and show it through `text_name`. They command nothing. The
  Language combo changes the locale and commands nothing. Each locale file
  has each id that the panels use.
- ui_tests.cpp, the e2e test: each officer and city name of sample.s11 and
  Scenario.s11 decodes without U+FFFD.

## Limits
- BIG5.TXT has no row for the lead byte C8 and no pairs after F9D5. The
  ETEN extension uses these bytes. They decode to U+FFFD. The real files
  do not use them.
- The Private Use Area has no glyphs. A row in names/zh-TW.tsv can give a
  name its correct characters after the spec answers Q-013.
- save_dump.exe `--utf8-names` still uses the Windows code page 950.

## Exercises
- Add ja.tsv with the strings in Japanese. Then select it in Settings.
- Add a `--locale` option to win32_hexdom.exe and send it to the DLL
  through game_memory.
- Change save_dump.exe to decode names through the text module.

The result you can check: `run_tests.bat --e2e` prints ALL PASS, and the
Officers panel shows the names in Chinese characters.
