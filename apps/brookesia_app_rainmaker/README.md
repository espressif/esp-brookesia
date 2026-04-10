# brookesia_app_rainmaker

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

English | [中文](README_CN.md)

## Overview

**brookesia_app_rainmaker** is an [ESP-Brookesia](https://github.com/espressif/esp-brookesia) phone application that provides an **ESP RainMaker**-style smart home UI on LVGL: login, home, devices, scenes, schedules, automations, and user settings. It registers as a Brookesia `App` plugin and shares backend logic with optional **PC host builds** under [`pc/`](pc/) for UI development and testing without flashing hardware.

- **License:** [Apache-2.0](https://www.apache.org/licenses/LICENSE-2.0) (see SPDX headers in source files).
- **Typical target:** ESP32-S3 / ESP32-P4 products using the phone system (e.g. `products/phone/phone_s3_box_3`, `products/phone/phone_p4_function_ev_board`).

## RainMaker prerequisites and limitations

### Before you use this app

1. Use **RainMaker Home** or the legacy **ESP RainMaker** mobile app to provision standard **light / switch / fan** (and similar) RainMaker devices and bind them to your RainMaker account.
2. Sign in to this app with the **same account** to see those devices in the list; control behavior is broadly similar to RainMaker Home.

### Firmware for standard devices

Reference firmware for standard lights, switches, fans, etc. is available from the **[Tryout Center](https://evaluation.rainmaker.espressif.com/#tryout-center)** for flashing.

### Companion apps

| App | Notes |
|-----|-------|
| **RainMaker Home** | [Download / getting started](https://docs.rainmaker.espressif.com/docs/dev/phone-app/home-app/home-app-get-started/#download-esp-rainmaker-home) |
| **ESP RainMaker (legacy, Android)** | [Google Play](https://play.google.com/store/apps/details?id=com.espressif.rainmaker) |
| **ESP RainMaker (legacy, iOS)** | Use a **non–China mainland** Apple ID, then install from the [App Store](https://apps.apple.com/app/esp-rainmaker/id1497491540). |

### Current limitations

1. **Device state is not pushed to the UI automatically.** Without an equivalent of the phone apps’ notification path on the device side, list and detail views require a **manual refresh** to reflect remote changes; future work may integrate RainMaker Controller–style behavior for automatic updates.
2. **In-app device provisioning is not supported yet.** Continue to use RainMaker Home or legacy ESP RainMaker for Wi-Fi setup; a future release may add provisioning from this app.

## User guide

[User Guide](USER_GUIDE_EN.md)

## Features

- RainMaker-oriented flows: device list, standard light / switch / fan details, scenes and schedules.
- Internationalization hooks (`ui_i18n`) and Chinese font assets under `ui/fonts/`.
- Backend helpers under `backend/` (device list, groups, standard device models) used by firmware and PC stubs.

## Dependencies

Declared in [`idf_component.yml`](idf_component.yml):

| Component | Role |
|-----------|------|
| `brookesia_core` | Brookesia phone framework (override path in manifest). |
| `espressif/rmaker_user_api` | RainMaker user API headers / integration. |

Other dependencies are pulled by the **product** firmware (LVGL, Wi-Fi, RainMaker stack, etc.).

## Integrate into a firmware project

1. Add this component to your product’s `main/idf_component.yml`, for example:

   ```yaml
   dependencies:
     brookesia_app_rainmaker:
       version: "*"
       override_path: "../../../../apps/brookesia_app_rainmaker"
   ```

   Adjust `override_path` relative to your `main` folder.

2. Ensure `brookesia_core` and RainMaker-related components match what the phone product already uses.

3. From the product directory (e.g. `products/phone/phone_s3_box_3`):

   ```bash
   idf.py set-target esp32s3
   idf.py build flash monitor
   ```

The app is registered by name **`ESP RainMaker`** via `ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR` in [`esp_brookesia_app_rainmaker.cpp`](esp_brookesia_app_rainmaker.cpp).

## Directory layout

| Path | Description |
|------|-------------|
| `esp_brookesia_app_rainmaker.cpp` | App entry: `RainmakerDemo`, LVGL UI init/deinit. |
| `ui/` | LVGL screens, images, fonts, i18n, gestures. |
| `backend/` | RainMaker backend glue shared with PC (see `CMakeLists.txt` excludes `pc/`). |
| `pc/` | SDL2 + LVGL host simulator, smoke/layout/visual tests — see [`pc/README.md`](pc/README.md). |

## CMake notes

[`CMakeLists.txt`](CMakeLists.txt) globs `*.c` / `*.cpp` and **excludes** `pc/**` so simulator sources are not built on device. After adding new UI `.c` files, run a clean rebuild if the new file is not picked up (`idf.py fullclean` then `build`).

## Resources

- [ESP-IDF Copyright header guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/contribute/copyright-guide.html)
- [ESP RainMaker](https://github.com/espressif/esp-rainmaker)

## License

SPDX-License-Identifier: Apache-2.0

Copyright 2026 Espressif Systems (Shanghai) CO LTD. See individual file headers for details.
