# Settings GSP fallback font

`DroidSansFallback-CommonChinese.ttf` is a subset of Google's Droid Sans Fallback font. It contains all GB2312 Han characters, Big5 level-1 characters and punctuation, and U+3000–U+30FF. This covers common simplified and traditional Chinese SSIDs and Japanese kana. It does not cover every Unicode character or every Big5 level-2 character. GSP keeps the scene's compiled DejaVu Sans glyph packs for ASCII and other declared characters; FreeType renders missing glyphs from this font.

The source font is Apache-2.0 licensed. Its distribution copyright notice is reproduced in `DroidSansFallback.LICENSE`. The source file used here was `/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf` from the Debian `fonts-droid-fallback` package, SHA-256 `acb6440a713d880a13a21b468ba7cd43f5a2b2934972e51be791c880730777b8`.

To regenerate with Python 3 and `fonttools==4.65.0`:

```sh
python3 -m pip install fonttools==4.65.0
python3 make_common_chinese_font.py /usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf DroidSansFallback-CommonChinese.ttf
sha256sum DroidSansFallback-CommonChinese.ttf
```

The expected subset hash is `20d73fe8c4b098aa4b0fb35ae46a37547f84fde8c75f09dfab92093e8b6e4211` (1,355,696 bytes). The bundled `ssid_latin_greek_cyrillic.txt` is a UTF-8 glyph corpus for GSP's compiled DejaVu Sans packs. It contains printable glyphs in U+00A0–U+00FF, U+0370–U+03FF and U+0400–U+04FF that are present in the scene's DejaVu Sans source, so Wi-Fi SSIDs and connection text can also show accented Latin, Greek, and Cyrillic names. Its SHA-256 is `1e86fd97e8d7e9c2904a79582db0539dc8b6ac869b7fc7f8192584573874093a`.
