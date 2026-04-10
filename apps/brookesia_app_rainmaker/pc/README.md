# RainMaker UI — PC simulator (SDL2 + LVGL)

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

English | [中文](README_CN.md)

## Overview

This directory builds a **desktop simulator** for the RainMaker phone UI: **SDL2** display, **LVGL 8**, shared UI sources from [`../ui`](../ui) and backend sources from [`../backend`](../backend), with **host stubs** (`rainmaker_*_pc.c`) instead of ESP-IDF drivers.

Use it to iterate on layouts, i18n, and navigation **without flashing** an ESP32. Optional **libcurl** enables closer RainMaker transport behavior (`RM_PC_HAVE_CURL`).

- **License:** Apache-2.0 (see SPDX in `CMakeLists.txt` and sources).
- **Requirements:** CMake **3.16+**, C99, **SDL2**, **ESP-IDF `cJSON`**, LVGL tree, **`app_rmaker_user_api.h`** (Component Manager / `managed_components`).

## Prerequisites

### 1. ESP-IDF (for `cJSON.c` only)

The build expects **`IDF_PATH`** so it can compile ESP-IDF’s `components/json/cJSON/cJSON.c`:

```bash
export IDF_PATH=/path/to/esp-idf
```

If `IDF_PATH` is unset, pass the file explicitly:

```bash
cmake -B build -S . -DCJSON_SRC=/path/to/esp-idf/components/json/cJSON/cJSON.c
```

### 2. LVGL (`lvgl__lvgl`)

Prefer fetching managed components once from a phone product (same LVGL version as firmware):

```bash
cd /path/to/esp-brookesia/products/phone/phone_s3_box_3
idf.py reconfigure
```

Then CMake will auto-detect LVGL under, for example:

- `products/phone/phone_s3_box_3/managed_components/lvgl__lvgl`
- or repo-root `managed_components/lvgl__lvgl`

Override manually if needed:

```bash
export LVGL_ROOT=/absolute/path/to/lvgl__lvgl
# or
cmake -B build -S . -DLVGL_ROOT=/absolute/path/to/lvgl__lvgl
```

### 3. RainMaker User API headers

`app_rmaker_user_api.h` must exist under Component Manager output, e.g.:

`.../managed_components/espressif__rmaker_user_api/include`

Run `idf.py reconfigure` in a product that depends on `espressif/rmaker_user_api`, or set:

```bash
cmake -B build -S . -DRMAKER_USER_API_INC=/path/to/include
```

### 4. System packages

| OS | Packages (examples) |
|----|---------------------|
| **Ubuntu / Debian** | `sudo apt install cmake build-essential libsdl2-dev libcurl4-openssl-dev` |
| **macOS (Homebrew)** | `brew install cmake sdl2 curl` |
| **Windows** | Install CMake, SDL2 development files, and optional curl; use a path without spaces when possible. |

**SDL2** is required. **libcurl** is optional; if not found, the project still configures but without `RM_PC_HAVE_CURL` (see `CMakeLists.txt`).

### 5. Python (optional, for visual diff)

For [`visual_test_compare.py`](visual_test_compare.py):

```bash
pip3 install Pillow
```

## Configure and build

From **this `pc/` directory**:

```bash
export IDF_PATH=/path/to/esp-idf
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Release build:

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Run the simulator

Main executable: **`rainmaker_ui_pc`**

```bash
./build/rainmaker_ui_pc
```

Resolution (CLI or environment):

```bash
./build/rainmaker_ui_pc --width 1280 --height 720
./build/rainmaker_ui_pc -w 1280 -H 720
export RM_PC_LCD_HOR=1024 RM_PC_LCD_VER=600
./build/rainmaker_ui_pc
```

## Other targets

| Target | Purpose |
|--------|---------|
| `rainmaker_ui_pc_smoke` | Headless / minimal smoke run (used by CTest). |
| `rainmaker_ui_layout_test` | Layout geometry checks (Select Devices row, etc.). |
| `rainmaker_ui_visual_test` | Captures screenshots for visual regression (requires LVGL snapshot support in `lv_conf.h`). |
| `http_smoke` | Built only when **CURL** is found — simple HTTP smoke test. |

## Tests (CTest)

```bash
cd build
ctest --output-on-failure
# or from pc/ after configure:
ctest --test-dir build
```

Tests set `RM_PC_LCD_HOR` / `RM_PC_LCD_VER` for multiple resolutions where applicable.

## Visual regression (optional)

See **[VISUAL_TEST_README.md](VISUAL_TEST_README.md)** for baselines, `visual_test_compare.py`, and HTML reports. Short flow:

```bash
./build/rainmaker_ui_visual_test --create-baseline   # first time
./build/rainmaker_ui_visual_test -o visual_test_output
python3 visual_test_compare.py visual_test_baseline visual_test_output
```

Helper script: [`run_visual_tests.sh`](run_visual_tests.sh).

## Troubleshooting

| Symptom | What to check |
|---------|----------------|
| `LVGL not found` | Set `LVGL_ROOT` or run `idf.py` in `phone_s3_box_3` to populate `managed_components`. |
| `cJSON.c` not found | Export `IDF_PATH` or pass `-DCJSON_SRC=...`. |
| `app_rmaker_user_api.h` not found | Run `idf.py reconfigure` in a product with `rmaker_user_api`, or `-DRMAKER_USER_API_INC=...`. |
| SDL window fails | Display / SDL2 install; on Linux ensure `libsdl2-dev`. |

## Related documentation

- [App-level README (English)](../README.md) / [中文](../README_CN.md)
- [ESP-IDF Get Started](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)

## License

SPDX-License-Identifier: Apache-2.0

Copyright 2026 Espressif Systems (Shanghai) CO LTD.
