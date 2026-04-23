#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail
cd "$(dirname "$0")"
FONT_OTF="${FONT_OTF:-NotoSansSC-Regular.otf}"
FONT_URL="${FONT_URL:-https://github.com/googlefonts/noto-cjk/raw/main/Sans/SubsetOTF/SC/NotoSansSC-Regular.otf}"

if [[ ! -f "$FONT_OTF" ]]; then
  echo "Downloading $FONT_OTF ..."
  curl -fsSL -o "$FONT_OTF" "$FONT_URL"
fi

python3 << 'PY'
import pathlib, re
p = (pathlib.Path("..") / "ui_i18n.c").resolve()
lines = p.read_text(encoding="utf-8").splitlines()
start = next(i for i, l in enumerate(lines) if "s_strings_zh[UI_STR_COUNT]" in l)
end = next(i for i in range(start + 1, len(lines)) if lines[i].strip() == "};" and i > start + 10)
block = "\n".join(lines[start : end + 1])
block = re.sub(r"/\*.*?\*/", "", block, flags=re.S)
chars = {c for c in block if ord(c) > 127}
syms = "".join(sorted(chars, key=ord))
pathlib.Path("ui_zh_symbols.txt").write_text(syms, encoding="utf-8")
print("Unique non-ASCII chars:", len(syms))
PY

SYMS=$(python3 -c "print(open('ui_zh_symbols.txt', encoding='utf-8').read())")
npx --yes lv_font_conv@1.5.2 \
  --font "$FONT_OTF" \
  --size 16 \
  --bpp 4 \
  --format lvgl \
  -r 0x20-0x7F \
  --symbols "$SYMS" \
  --no-compress \
  -o lv_font_ui_zh_16.c

echo "Wrote lv_font_ui_zh_16.c"
