# Device Service Example

[中文版本](./README_CN.md)

This example demonstrates how to use the device service (`brookesia_service_device`) in the ESP-Brookesia framework. The service is the application-layer gateway to HAL-backed device capabilities, covering board capability discovery, device control, status queries, and event validation.

## 📑 Table of Contents

- [Device Service Example](#device-service-example)
  - [📑 Table of Contents](#-table-of-contents)
  - [✨ Features](#-features)
  - [🚩 Getting Started](#-getting-started)
    - [Hardware Requirements](#hardware-requirements)
    - [Development Environment](#development-environment)
  - [🔨 How to Use](#-how-to-use)
  - [🚀 Runtime Overview](#-runtime-overview)
    - [Capability Discovery](#capability-discovery)
    - [Board Information](#board-information)
    - [Display Control](#display-control)
    - [Audio Control](#audio-control)
    - [Storage Query](#storage-query)
    - [Battery State and Charge Control](#battery-state-and-charge-control)
    - [Reset Data](#reset-data)
  - [🔍 Troubleshooting](#-troubleshooting)
  - [💬 Technical Support and Feedback](#-technical-support-and-feedback)

## ✨ Features

- 🧩 **Board-adaptive capability discovery**: Uses `GetCapabilities` to query the HAL interfaces supported by the current board and only runs supported demos
- 🧾 **Board information**: Reads board name, chip, version, description, and manufacturer when `BoardInfo` is available
- 💡 **Display control**: Reads and sets backlight brightness and on/off state when `DisplayBacklight` is available
- 🖼️ **Display panel validation**: Draws pixel-bit color bars directly through HAL when `DisplayPanel` is available
- 🔊 **Audio control**: Reads and sets audio player volume and mute state when `AudioCodecPlayer` is available, then plays a PCM file from SPIFFS
- 💾 **Storage query**: Lists mounted file systems when `StorageFs` is available
- 🔋 **Battery state**: Queries battery information, runtime state, and charge configuration when `Power:Battery` is available
- 📡 **Event validation**: Uses `DeviceHelper::EventMonitor` to wait for and inspect display, audio, and battery events triggered by control operations
- 🧹 **State reset**: Calls `ResetData` to clear persisted device service state, then verifies restored display and audio state where supported

## 🚩 Getting Started

### Hardware Requirements

This example manages hardware through the [brookesia_hal_boards](https://components.espressif.com/components/espressif/brookesia_hal_boards) component, and supports development boards that meet the following requirements:

- Flash >= 16MB
- A board-manager configuration is generated for the selected board
- Support one or more of the following HAL interfaces:

  - `BoardInfo`
  - `DisplayBacklight`
  - `DisplayPanel`
  - `AudioCodecPlayer`
  - `StorageFs`
  - `Power:Battery`

The example automatically skips unsupported features. Please refer to the [ESP-Brookesia Programming Guide - Supported Boards](https://docs.espressif.com/projects/esp-brookesia/en/latest/hal/boards/index.html#hal-boards-sec-02) for a list of supported development boards.

### Development Environment

Please refer to the following documentation:

- [ESP-Brookesia Programming Guide - Versioning](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia Programming Guide - Development Environment Setup](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-dev-environment)

## 🔨 How to Use

Please refer to [ESP-Brookesia Programming Guide - How to Use Example Projects](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-example-projects).

> [!NOTE]
> The example uses `EXAMPLE_TEST_ACTION_DELAY_MS` to control the delay between test actions. The default value is `2000` ms. Override this macro in the build configuration if you need a slower or faster demo pace.

## 🚀 Runtime Overview

After flashing, the program initializes all HAL devices, starts `ServiceManager`, binds `NVSHelper` and `DeviceHelper`, and then runs the following demos in sequence. All output is printed through the serial monitor.

### Capability Discovery

The example first calls `GetCapabilities` to query the HAL interfaces registered by the current board and prints a feature matrix:

```text
[Demo: Capabilities]
  Raw HAL capabilities reported by the current board
  Device helper feature matrix
```

All subsequent demos are gated by this capability matrix. Unsupported features print a `skip` log instead of failing the example.

### Board Information

When `BoardInfo` is available, the example calls `GetBoardInfo` and prints static board metadata:

```text
[Demo: Board Info]
  name / chip / version / description / manufacturer
```

### Display Control

When `DisplayBacklight` is available, the example demonstrates brightness and on/off control:

```text
[Demo: Display Controls]
  Read current brightness and on/off state
  Turn the backlight on first if it is currently off
  Draw pixel-bit color bars according to pixel_bits if DisplayPanel is available
  Change brightness and wait for the BrightnessChanged event through EventMonitor
  Turn the backlight off and wait for the OnOffChanged(false) event
  Turn the backlight on again and wait for the OnOffChanged(true) event
  Read back the state for verification
```

### Audio Control

When `AudioCodecPlayer` is available, the example demonstrates volume and mute control. It reads `/spiffs/audio.pcm` from the SPIFFS partition and opens the player as `16 kHz / 16-bit / mono`:

```text
[Demo: Audio Controls]
  Read current volume and mute state
  Unmute first if the player is currently muted
  Change volume and wait for the VolumeChanged event through EventMonitor
  Play audio.pcm
  Mute and wait for the MuteChanged(true) event
  Play audio.pcm while muted
  Unmute and wait for the MuteChanged(false) event
  Play audio.pcm again
```

> [!NOTE]
> `main/CMakeLists.txt` uses `spiffs_create_partition_image(spiffs_data ../spiffs FLASH_IN_PROJECT)` to package `spiffs/audio.pcm` into the SPIFFS partition.

### Storage Query

When `StorageFs` is available, the example calls `GetStorageFileSystems` and prints all mounted file systems, including file system type, medium type, and mount point.

### Battery State and Charge Control

When `Power:Battery` is available, the example demonstrates battery queries and event monitoring:

```text
[Demo: Power Battery]
  Query PowerBatteryInfo
  Query PowerBatteryState
  Query PowerBatteryChargeConfig if ChargeConfig is supported
  Refresh charging enabled state if both ChargerControl and ChargeConfig are supported
  Wait for battery state or charge config events through EventMonitor
```

If the current board does not support charge configuration or charger control, the related steps are skipped and the example continues.

### Reset Data

Finally, the example calls `ResetData` to reset the persisted device service state:

```text
[Demo: Reset Data]
  Call ResetData
  If display backlight is available, read and print restored brightness and on/off state
  If audio player is available, read and print restored volume and mute state
```

## 🔍 Troubleshooting

**Some demos are skipped**

- This is expected. The example uses the `GetCapabilities` result to skip interfaces that are not supported by the current board.
- Check the board configuration and HAL adaptor Kconfig options if an expected interface is missing.

**No audio output**

- Confirm that the SPIFFS partition has been flashed successfully (`idf.py flash` should include both the partition table and the `spiffs_data` partition).
- Confirm that the current board supports `AudioCodecPlayer`, and that the amplifier/speaker hardware works correctly.
- Check the serial log to confirm that `/spiffs/audio.pcm` was read successfully.

**No display change**

- Confirm that the current board supports `DisplayBacklight` and/or `DisplayPanel`.
- If only backlight is supported but panel drawing is not, the example only demonstrates brightness and on/off control.

**Battery information is unavailable**

- Confirm that the current board supports `Power:Battery`.
- `ChargeConfig` and `ChargerControl` are optional abilities. Related steps are skipped automatically when unsupported.

**Build fails in VSCode**

Install ESP-IDF using the command line. See [Development Environment](#development-environment).

## 💬 Technical Support and Feedback

Please use the following channels for feedback:

- For technical questions, visit the [esp32.com](https://esp32.com/viewforum.php?f=52&sid=86e7d3b29eae6d591c965ec885874da6) forum
- For feature requests or bug reports, create a new [GitHub Issue](https://github.com/espressif/esp-brookesia/issues)

We will respond as soon as possible.
