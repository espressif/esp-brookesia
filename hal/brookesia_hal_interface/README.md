# ESP-Brookesia HAL Interface

* [中文版本](./README_CN.md)

## Overview

`brookesia_hal_interface` is the hardware abstraction foundation for the ESP-Brookesia ecosystem. It defines a common `Device` and `Interface` model so board-specific implementations can be registered once and consumed through stable, typed APIs.

This component mainly provides:

- **Device Lifecycle Model**: Common `probe()`, `check_initialized()`, `init()`, and `deinit()` flows through `Device`.
- **Plugin-based Registry**: Devices can be registered through the Brookesia plugin system and initialized in batches.
- **Typed Lookup Helpers**: Supports `get_device()`, `get_interface<T>()`, `get_interfaces<T>()`, and `get_first_interface<T>()`.
- **Reusable Interface Definitions**: Common audio, display, storage, and status LED interfaces are exposed under `include/brookesia/hal_interface/interfaces/`.
- **Regression Tests**: Includes Unity and pytest coverage for registry, interface lookup, and public API behavior.

## Table of Contents

- [ESP-Brookesia HAL Interface](#esp-brookesia-hal-interface)
  - [Overview](#overview)
  - [Table of Contents](#table-of-contents)
  - [Features](#features)
    - [Device and Registry Model](#device-and-registry-model)
    - [Built-in Interface Catalog](#built-in-interface-catalog)
    - [Test Coverage](#test-coverage)
  - [Development Environment Requirements](#development-environment-requirements)
  - [Adding to Project](#adding-to-project)
  - [Quick Start](#quick-start)

## Features

### Device and Registry Model

- **`Device`** defines the common HAL lifecycle and typed query entry.
- **`DeviceImpl<Derived>`** provides a CRTP helper for mapping one device to one or more interfaces.
- **`Device::init_device_from_registry()`** initializes all registered devices in one call.
- **`get_device()` / `get_interface<T>()` / `get_interfaces<T>()` / `get_first_interface<T>()`** provide the common lookup path used by upper-layer services.

### Built-in Interface Catalog

| Header | Interface | Description |
|--------|-----------|-------------|
| `status_led.hpp` | `StatusLedIface` | Status LED blink control with predefined blink types. |
| `storage_fs.hpp` | `StorageFsIface` | File-system related access such as mount path and filesystem type. |
| `display_panel.hpp` | `DisplayPanelIface` | Display resolution, bitmap flush, callback registration, and backlight control. |
| `display_touch.hpp` | `DisplayTouchIface` | Display touch device handle access. |
| `audio_player.hpp` | `AudioPlayerIface` | Playback handle and player configuration access. |
| `audio_recorder.hpp` | `AudioRecorderIface` | Recorder handle and recorder configuration access. |

### Test Coverage

The `test_apps` directory includes Unity and pytest coverage for:

- FNV-1a interface hash helpers
- registry initialization and interface filtering
- `DeviceImpl` interface query behavior
- status LED, audio, and display interface accessors
- `esp32s3` and `esp32p4` pytest targets

## Development Environment Requirements

Before using this component, please ensure the following SDK development environment is installed:

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> For SDK installation instructions, please refer to [ESP-IDF Programming Guide - Installation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

## Adding to Project

`brookesia_hal_interface` has been uploaded to the [Espressif Component Registry](https://components.espressif.com/). You can add it to your project in the following ways:

1. **Using Command Line**

   Run the following command in your project directory:

   ```bash
   idf.py add-dependency "espressif/brookesia_hal_interface"
   ```

2. **Modify Configuration File**

   Create or modify the *idf_component.yml* file in your project directory:

   ```yaml
   dependencies:
     espressif/brookesia_hal_interface: "*"
   ```

For detailed instructions, please refer to [Espressif Documentation - IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## Quick Start

```cpp
#include "brookesia/hal_interface.hpp"

using namespace esp_brookesia::hal;

class DemoLedDevice : public DeviceImpl<DemoLedDevice>, public StatusLedIface {
public:
    DemoLedDevice(): DeviceImpl<DemoLedDevice>("demo_led") {}

    bool probe() override { return true; }
    bool check_initialized() const override { return true; }
    bool init() override { return true; }
    bool deinit() override { return true; }
    void start_blink(BlinkType type) override { (void)type; }
    void stop_blink(BlinkType type) override { (void)type; }

private:
    void *query_interface(uint64_t id) override
    {
        return build_table<StatusLedIface>(id);
    }
};

BROOKESIA_PLUGIN_REGISTER(Device, DemoLedDevice, "demo_led");
```
