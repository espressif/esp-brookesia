# ESP-Brookesia HAL Boards

* [中文版本](./README_CN.md)

## Overview

`brookesia_hal_boards` is the board configuration collection for ESP-Brookesia. It uses YAML files to describe the peripheral topology and device parameters for each supported board, allowing `brookesia_hal_adaptor` to initialize hardware at runtime without board-specific hardcoding.

For more information, see the [ESP-Brookesia Programming Guide](https://docs.espressif.com/projects/esp-brookesia/en/latest/hal/boards/index.html).

### ESP-Mosaico Board Revisions

Select the configuration that matches the CoreBoard revision; board selection is explicit and does not use automatic eFuse detection.

| Board configuration | Adaptation |
| --- | --- |
| `esp_mosaico_v1_0` | Existing V1.0 configuration; see the [V1.0 adaptation guide](https://docs.espressif.com/projects/esp-brookesia/en/latest/hal/boards/espressif.html#hal-boards-espressif-mosaico) |
| `esp_mosaico_v1_2` | Initial V1.2 adaptation for existing HAL and service capabilities; **partially validated on hardware**, see the [V1.2 adaptation guide](https://docs.espressif.com/projects/esp-brookesia/en/latest/hal/boards/espressif.html#hal-boards-espressif-mosaico-v1-2) for the verified scope and remaining checks |

V1.2 separates onboard and expansion I2C buses, updates LCD clock/reset, removes the dedicated codec power GPIO while retaining the power-settling delay, and uses the CST9220 touch driver. NAND FATFS and optional camera integration are retained, but NAND is not yet an application installation location in System Core. IMU and motor services remain outside this adaptation. Both configurations use the existing Board Manager 0.5.15 dependency line.

The V1.2 TinyUSB CDC console supports menu interaction. Automatic download caused the current sample to disconnect from USB without re-enumerating, so it is disabled by default (`CONFIG_BSP_USB_AUTO_DOWNLOAD=n`), and `CONFIG_ESPTOOLPY_BEFORE_NORESET=y` prevents another reset after manual ROM entry. Enter ROM download mode manually with BOOT before flashing; update both settings in an existing `sdkconfig`. Display/touch initialization, brightness control, audio initialization/deinitialization, battery queries, and LittleFS/NVS have been exercised. NAND file operations and unmount/remount tests passed after formatting, as did 50 SC101IOT start/stop and frame-capture cycles. System Super completed startup; desktop display, basic touch interaction, and brightness-slider dragging were manually confirmed on the current sample. My Device information and nearby Wi-Fi access-point scanning were also confirmed; runtime logs show successful network time synchronization. Audio quality, actual camera frame rate and image quality, and NAND file persistence across power loss still require validation.

V1.2 configures the mbedTLS/PSA SHA/AES used by HTTP/TLS to run in software (`CONFIG_MBEDTLS_HARDWARE_SHA=n`, `CONFIG_MBEDTLS_HARDWARE_AES=n`) for LCD, NAND, camera, and HTTPS coexistence; update both options in an existing V1.2 `sdkconfig`. Hardware tracing confirmed that HTTPS activity retained the remaining AXI GDMA RX channel and blocked camera reopening. These settings retain TLS and signature verification and may increase CPU work; they do not disable all direct hardware crypto use. The user manually confirmed normal Camera application preview with the fixed firmware. A repeat after an actual application download and installation has not been separately recorded.

### Mosaico Source Layout

Each revision directory under `boards/espressif/` contains its own `setup_device.c`, `board_devices.cpp`, three board YAML files, `sdkconfig.defaults.board`, and `SOURCE.md`. `setup_device.c` selects the touch factory and passes the revision's LCD clock GPIO to the shared panel factory. `board_devices.cpp` registers Board Manager devices and converts generated configuration into explicit shared parameters. The shared audio implementation exposes separate power-enable and readiness APIs, selected by the V1.0 and V1.2 registration code respectively.

Both revisions select the existing `brookesia_hal_custom`, `esp_lcd_co5300`, and `esp_mosaico_usb_console` components from `components/mosaico/`. Shared device lifecycles live in `brookesia_hal_custom/src/board/`, with registration interfaces under `include/brookesia/hal_custom/board/`; neither board depends on the other revision's sources. See the [shared source provenance](components/mosaico/SOURCE.md) and [Mosaico HAL Custom README](components/mosaico/brookesia_hal_custom/README.md).

The hardware observations above were recorded before this source-layout refactor. They do not establish build or hardware validation of the reorganized layout.

## How to Use

### Environment Requirements

Please refer to the following documentation:

- [ESP-Brookesia Programming Guide - Versioning](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia Programming Guide - Development Environment Setup](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-dev-environment)

### Add to Your Project

Please refer to [ESP-Brookesia Programming Guide - How to Obtain and Use Components](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-component-usage).
