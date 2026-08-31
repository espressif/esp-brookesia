# ESP-Brookesia Service Helper

* [中文版本](./README_CN.md)

## Overview

`brookesia_service_helper` is the typed helper contract layer for Brookesia service functions and events.

For more information, see the [ESP-Brookesia Programming Guide](https://docs.espressif.com/projects/esp-brookesia/en/latest/service/framework/helper/index.html).

## Stable RPC Names

The value returned by each helper's `get_name()` is a stable, case-sensitive public RPC identifier. Applications must use this exact value for service calls and the `services[].name` field in `manifest.json`. Changing an existing RPC name is a breaking API change and requires an explicit compatibility or migration plan; do not rename it during ordinary refactoring.

## DataFlow Helper

The `DataFlow` helper provides typed control-plane access to provider discovery, operation ownership, lifecycle, and source routing. Native frame and audio-buffer access remains available through the typed C++ operation interfaces.

## How to Use

### Environment Requirements

Please refer to the following documentation:

- [ESP-Brookesia Programming Guide - Versioning](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia Programming Guide - Development Environment Setup](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-dev-environment)

### Add to Your Project

Please refer to [ESP-Brookesia Programming Guide - How to Obtain and Use Components](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-component-usage).
