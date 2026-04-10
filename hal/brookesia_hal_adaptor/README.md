# ESP-Brookesia HAL Adaptor

* [中文版本](./README_CN.md)

## Overview

`brookesia_hal_adaptor` is the board-level HAL adaptor for ESP-Brookesia. It builds on the device/interface model from `brookesia_hal_interface`, initializes real peripherals through `esp_board_manager`, and registers **audio**, **display**, and **storage** capabilities in the global HAL table so upper layers can discover and use them by name.

## Table of Contents

- [ESP-Brookesia HAL Adaptor](#esp-brookesia-hal-adaptor)
  - [Overview](#overview)
  - [Table of Contents](#table-of-contents)
  - [Features](#features)
  - [Development Environment Requirements](#development-environment-requirements)
  - [Add to Your Project](#add-to-your-project)

## Features

This component exposes three board-level devices. Each is registered as a singleton and, after initialization, publishes its interfaces to the global table:

| Device class (logical name) | Registered interface implementations |
|-------------------------------|--------------------------------------|
| `AudioDevice` (`"Audio"`) | Codec playback (`CODEC_PLAYER_IMPL_NAME`), codec recording (`CODEC_RECORDER_IMPL_NAME`) |
| `DisplayDevice` (`"Display"`) | LEDC backlight (`LEDC_BACKLIGHT_IMPL_NAME`), LCD panel (`LCD_PANEL_IMPL_NAME`), LCD touch (`LCD_TOUCH_IMPL_NAME`) |
| `StorageDevice` (`"Storage"`) | General file system (`GENERAL_FS_IMPL_NAME`), including SPIFFS and SD card (FATFS) |

Sub-interfaces can be enabled or disabled independently under **ESP-Brookesia: Hal Adaptor Configurations** in `menuconfig`; some options depend on board capabilities (`ESP_BOARD_DEV_*`). Default parameters (volume range, recording format, backlight brightness range, and so on) can also be adjusted in `menuconfig` and are mapped to compile-time macros in `macro_configs.h` for the implementations.

To override default capability parameters before initialization, call `set_codec_player_info`, `set_codec_recorder_info`, or `set_ledc_backlight_info` on the corresponding device singleton. Calls made after initialization usually have no effect.

> [!NOTE]
> Application code only needs to depend on the abstract types from `brookesia_hal_interface` and resolve capabilities by full interface name; it does not need to include private headers from this component.

## Development Environment Requirements

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> For SDK installation, see the [ESP-IDF Programming Guide — Installation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).

## Add to Your Project

`brookesia_hal_adaptor` is published on the [Espressif Component Registry](https://components.espressif.com/). You can add it in either of the following ways:

1. **Command line**

   ```bash
   idf.py add-dependency "espressif/brookesia_hal_adaptor"
   ```

2. **idf_component.yml**

   ```yaml
   dependencies:
     espressif/brookesia_hal_adaptor: "*"
   ```

For details, see the Espressif documentation on the [IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).
