# UI Chinese bitmap font (`lv_font_ui_zh_16`)

`lv_font_ui_zh_16.c` is generated with [lv_font_conv](https://github.com/lvgl/lv_font_conv) from **Noto Sans SC** (SIL Open Font License 1.1). Source OTF:

- https://github.com/googlefonts/noto-cjk/tree/main/Sans/SubsetOTF/SC

The glyph set is **ASCII 0x20–0x7F** plus every unique non-ASCII character collected from `ui_i18n.c` (`s_strings_zh`). After changing Chinese strings, regenerate so new characters are included.

## Regenerate

1. Install Node.js (for `npx`).
2. Download `NotoSansSC-Regular.otf` into this directory, or run `gen_ui_zh_font.sh` (downloads if missing).
3. From this directory: `./gen_ui_zh_font.sh`

This overwrites `lv_font_ui_zh_16.c` and `ui_zh_symbols.txt`.

## Files

| File | Purpose |
|------|---------|
| `lv_font_ui_zh_16.c` | LVGL font; linked by app CMake glob |
| `ui_zh_symbols.txt` | CJK chars passed to lv_font_conv |
| `gen_ui_zh_font.sh` | Download font + run converter |

Large `NotoSansSC-Regular.otf` is gitignored; the generated `.c` is what firmware needs.
