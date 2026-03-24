# ESP-Brookesia HAL Adaptor

* [中文版本](./README_CN.md)

## Overview

`brookesia_hal_adaptor` provides ready-to-use board-side device implementations for `brookesia_hal_interface`. It bridges Brookesia HAL interfaces with peripherals exposed by `esp_board_manager`, so upper layers can consume audio and display capabilities through typed interfaces.

This component currently provides:

- **Audio Device Adaptor**: Registers `AudioDevice`, implementing `AudioPlayerIface` and `AudioRecorderIface`.
- **LCD Display Adaptor**: Registers `LcdDisplay`, implementing `DisplayPanelIface` and `DisplayTouchIface`.
- **Board Integration**: Wraps `audio_dac`, `audio_adc`, `display_lcd`, `lcd_brightness`, and `lcd_touch` devices from `esp_board_manager`.
- **Registry-based Initialization**: Works with `Device::init_device_from_registry()` for unified startup.

## Table of Contents

- [ESP-Brookesia HAL Adaptor](#esp-brookesia-hal-adaptor)
  - [Overview](#overview)
  - [Table of Contents](#table-of-contents)
  - [Features](#features)
    - [Built-in Devices](#built-in-devices)
    - [Board Requirements](#board-requirements)
    - [Typical Usage](#typical-usage)
  - [Development Environment Requirements](#development-environment-requirements)
  - [Adding to Project](#adding-to-project)
  - [Quick Start](#quick-start)

## Features

### Built-in Devices

| Device Name | Interfaces | Backing board-manager devices | Description |
|-------------|------------|-------------------------------|-------------|
| `AudioDevice` | `AudioPlayerIface`, `AudioRecorderIface` | `audio_dac`, `audio_adc` | Exposes codec handles and fills recorder defaults according to the detected board. |
| `LcdDisplay` | `DisplayPanelIface`, `DisplayTouchIface` | `display_lcd`, `lcd_brightness`, `lcd_touch` | Exposes display resolution, bitmap flush, touch handle access, callback registration, and backlight control. |

### Board Requirements

- **Audio adaptor** currently provides recorder defaults for:
  - `esp32_s3_korvo2_v3`
  - `esp_vocat_board_v1_0`
  - `esp_vocat_board_v1_2`
  - `esp_box_3`
  - `esp32_s3_korvo2l_v1`
  - `esp32_p4_function_ev`
  - `esp_sensair_shuttle`
- **Display adaptor** requires the selected board definition to provide `display_lcd`, `lcd_brightness`, and `lcd_touch`.
- If a required board device is missing or the current audio board is unsupported, initialization fails during `Device::init_device_from_registry()`.

### Typical Usage

- `service/brookesia_service_audio` can retrieve `AudioPlayerIface` and `AudioRecorderIface` from the registry instead of touching board drivers directly.
- Display-capable applications can retrieve `DisplayPanelIface` and `DisplayTouchIface` to access resolution, draw callbacks, and touch handles.
- Applications typically initialize all registered devices once during startup.

## Development Environment Requirements

Before using this component, please ensure the following SDK development environment is installed:

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`
- `espressif/esp_board_manager`: `0.5.*`
- `espressif/brookesia_hal_interface`

> [!NOTE]
> For SDK installation instructions, please refer to [ESP-IDF Programming Guide - Installation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

## Adding to Project

`brookesia_hal_adaptor` can be added to your project through the [Espressif Component Registry](https://components.espressif.com/):

1. **Using Command Line**

   Run the following command in your project directory:

   ```bash
   idf.py add-dependency "espressif/brookesia_hal_adaptor"
   ```

2. **Modify Configuration File**

   Create or modify the *idf_component.yml* file in your project directory:

   ```yaml
   dependencies:
     espressif/brookesia_hal_adaptor: "*"
   ```

For detailed instructions, please refer to [Espressif Documentation - IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## Quick Start

```cpp
#include "brookesia/hal_interface.hpp"

using namespace esp_brookesia::hal;

extern "C" void app_main(void)
{
    Device::init_device_from_registry();

    auto *player = get_interface<AudioPlayerIface>("AudioDevice");
    auto *display = get_interface<DisplayPanelIface>("LcdDisplay");

    if (player != nullptr) {
        player->set_volume(75);
    }
    if (display != nullptr) {
        display->set_backlight(100);
    }
}
```
