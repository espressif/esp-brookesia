# ESP-Brookesia Device Service

* [中文版本](./README_CN.md)

## Overview

`brookesia_service_device` is an application-facing device control service for ESP-Brookesia. It provides a unified service interface for application code to access initialized HAL capabilities, mainly for control operations and status queries.

It is not a hardware driver. HAL adaptor and board components provide low-level interfaces such as display backlight, audio player, storage file systems, board information, and power battery. Device Service discovers these HAL interfaces and exposes them as Service functions and events through `esp_brookesia::service::helper::Device`.

Typical capabilities include:

- Get HAL capability snapshots and board information.
- Set/query display backlight brightness and on/off state.
- Set/query audio player volume and mute state.
- Query mounted storage file systems.
- Query battery information, state, and charge configuration.
- Control battery charging when supported by the underlying HAL.
- Persist selected application-level state, such as volume, mute, and brightness, when `brookesia_service_nvs` is available.

For more information, see the [ESP-Brookesia Programming Guide](https://docs.espressif.com/projects/esp-brookesia/en/latest/service/device.html).

## How to Use

### Environment Requirements

Please refer to the following documentation:

- [ESP-Brookesia Programming Guide - Versioning](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia Programming Guide - Development Environment Setup](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-dev-environment)

### Add to Your Project

Please refer to [ESP-Brookesia Programming Guide - How to Obtain and Use Components](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-component-usage).

### Application Flow

1. Initialize the required HAL devices, for example by using `esp_brookesia::hal::init_all_devices()`.
2. Start `esp_brookesia::service::ServiceManager`.
3. Use `esp_brookesia::service::helper::Device` to call functions or subscribe to events.
4. Query capabilities before using optional features, because each board may expose different HAL interfaces.
