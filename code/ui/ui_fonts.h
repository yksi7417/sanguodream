#ifndef UI_FONTS_H
#define UI_FONTS_H
/* The fonts of the front end. The executables that own the ImGui context
   (win32_hexdom.exe and demo_player.exe) call ui_fonts_load one time,
   after they create the context and before the first frame. The DLL does
   not call it: ImGui builds the atlas in the executable.

   ImGui 1.91 puts each glyph in one texture when it builds the atlas, so
   the loader adds only the glyphs that the game can show:
   - Latin and Latin-1, Vietnamese, Thai, kana and CJK punctuation
   - each code point of the Big5 table (the zh-TW names)
   - each character of text/<locale>.tsv and names/<locale>.tsv of each
     locale in text/locales.tsv (for example the Hangul of a Korean file)

   The fonts are in data/fonts/ (docs/ASSETS.md). If the CJK font is not
   there, the loader uses the default ImGui font, which has Latin only. */

#include <stdio.h>
#include "imgui.h"
#include "imgui_internal.h"     /* ImFileLoadToMemory */
#include "text/text.h"

#define UI_FONT_CJK_FILE   "NotoSansCJKtc-Regular.otf"   /* Latin, CJK, kana and Hangul */
#define UI_FONT_THAI_FILE  "NotoSansThai-Regular.ttf"    /* merged into the CJK font */
#define UI_FONT_PIXEL_SIZE 16.0f
#define UI_FONT_PATH_SIZE  512

struct ui_font_result
{
    b32  loaded_cjk;
    b32  loaded_thai;
    u32  glyph_count;          /* code points that the atlas asks the font for */
    char message[256];         /* one line for the log of the platform layer */
};

/* ImGui keeps the pointer to the glyph ranges until it builds the atlas,
   so the ranges live here and not on the stack. */
global_variable ImVector<ImWchar> ui_font_glyph_ranges;

global_variable const ImWchar ui_font_extra_ranges[] =
{
    0x2000, 0x206F,   /* general punctuation */
    0x3000, 0x30FF,   /* CJK symbols and punctuation, hiragana, katakana */
    0x31F0, 0x31FF,   /* katakana phonetic extensions */
    0xFF00, 0xFFEF,   /* half-width and full-width forms */
    0,
};

global_variable const ImWchar ui_font_thai_ranges[] =
{
    0x0E00, 0x0E7F,   /* Thai */
    0,
};

internal b32
ui_fonts_file_exists(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    fclose(file);
    return true;
}

/* Adds each character of a UTF-8 file. A missing file adds nothing. */
internal void
ui_fonts_add_file_text(ImFontGlyphRangesBuilder *builder, const char *path)
{
    size_t size = 0;
    void *contents = ImFileLoadToMemory(path, "rb", &size, 1);
    if (!contents) return;
    builder->AddText((const char *)contents, (const char *)contents + size);
    IM_FREE(contents);
}

/* Adds the text files and the names files of each locale of the pack. */
internal void
ui_fonts_add_pack_text(ImFontGlyphRangesBuilder *builder, const char *data_dir, const char *pack)
{
    char path[UI_FONT_PATH_SIZE];
    snprintf(path, sizeof(path), "%s\\%s\\text\\locales.tsv", data_dir, pack);
    size_t size = 0;
    void *contents = ImFileLoadToMemory(path, "rb", &size, 1);
    if (!contents) return;
    builder->AddText((const char *)contents, (const char *)contents + size);

    u64 arena_size = Kilobytes(64) + 4 * (u64)size;
    void *arena_memory = IM_ALLOC((size_t)arena_size);
    memory_arena arena;
    arena_init(&arena, arena_memory, arena_size);
    text_table locales = text_table_parse((const char *)contents, size, "code", &arena);
    for (u32 row = 0; row < locales.row_count; ++row)
    {
        if (!text_is_locale_code(locales.row_keys[row])) continue;
        snprintf(path, sizeof(path), "%s\\%s\\text\\%s.tsv", data_dir, pack, locales.row_keys[row]);
        ui_fonts_add_file_text(builder, path);
        snprintf(path, sizeof(path), "%s\\%s\\names\\%s.tsv", data_dir, pack, locales.row_keys[row]);
        ui_fonts_add_file_text(builder, path);
    }
    IM_FREE(arena_memory);
    IM_FREE(contents);
}

/* Loads the fonts from <data_dir>\fonts into the atlas of the current
   ImGui context. data_dir is a full path, for example
   "C:\src\hexdom\build\..\data". */
internal ui_font_result
ui_fonts_load(const char *data_dir, const char *pack)
{
    ui_font_result result = {};
    ImGuiIO &io = ImGui::GetIO();
    char cjk_path[UI_FONT_PATH_SIZE];
    char thai_path[UI_FONT_PATH_SIZE];
    snprintf(cjk_path, sizeof(cjk_path), "%s\\fonts\\%s", data_dir, UI_FONT_CJK_FILE);
    snprintf(thai_path, sizeof(thai_path), "%s\\fonts\\%s", data_dir, UI_FONT_THAI_FILE);

    if (!ui_fonts_file_exists(cjk_path))
    {
        io.Fonts->AddFontDefault();
        snprintf(result.message, sizeof(result.message),
                 "fonts: %s not found; the default font shows Latin only (see docs\\ASSETS.md)", UI_FONT_CJK_FILE);
        return result;
    }

    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
    builder.AddRanges(io.Fonts->GetGlyphRangesVietnamese());
    builder.AddRanges(ui_font_extra_ranges);
    builder.AddRanges(ui_font_thai_ranges);
    for (u32 row = 0; row < BIG5_ROW_COUNT; ++row)
    {
        for (u32 index = 0; index < BIG5_TRAIL_COUNT; ++index)
        {
            if (big5_rows[row][index]) builder.AddChar((ImWchar)big5_rows[row][index]);
        }
    }
    ui_fonts_add_pack_text(&builder, data_dir, pack);
    ui_font_glyph_ranges.clear();
    builder.BuildRanges(&ui_font_glyph_ranges);
    for (int index = 0; index + 1 < ui_font_glyph_ranges.Size; index += 2)
    {
        result.glyph_count += (u32)(ui_font_glyph_ranges[index + 1] - ui_font_glyph_ranges[index] + 1);
    }

    /* One sample for each pixel: CJK glyphs are dense, and the atlas stays small. */
    ImFontConfig config;
    config.OversampleH = 1;
    config.OversampleV = 1;
    config.PixelSnapH  = true;
    result.loaded_cjk = io.Fonts->AddFontFromFileTTF(cjk_path, UI_FONT_PIXEL_SIZE, &config, ui_font_glyph_ranges.Data) != 0;
    if (!result.loaded_cjk)
    {
        io.Fonts->AddFontDefault();
        snprintf(result.message, sizeof(result.message), "fonts: %s did not load; the default font shows Latin only", cjk_path);
        return result;
    }
    if (ui_fonts_file_exists(thai_path))
    {
        config.MergeMode = true;
        result.loaded_thai = io.Fonts->AddFontFromFileTTF(thai_path, UI_FONT_PIXEL_SIZE, &config, ui_font_thai_ranges) != 0;
    }
    snprintf(result.message, sizeof(result.message), "fonts: %s%s, %u code points",
             UI_FONT_CJK_FILE, result.loaded_thai ? " + " UI_FONT_THAI_FILE : "", result.glyph_count);
    return result;
}

#endif /* UI_FONTS_H */
