# ESP-Brookesia Audio Service

* [中文版本](./README_CN.md)

## Overview

`brookesia_service_audio` is the media service for audio playback, capture, AFE, and volume control.

Playback callers may set `PlayUrlConfig::only_if_idle` with `interrupt=false` to reject a busy player instead of queueing a request. The option defaults to false, preserving existing playback behavior.

Volume and mute controls apply their target state before submitting persistence asynchronously. A successful control result does not confirm that the value was saved across restarts; unavailable storage or failed writes are logged. Unchanged values do not trigger another write.

For more information, see the [ESP-Brookesia Programming Guide](https://docs.espressif.com/projects/esp-brookesia/en/latest/service/audio.html).

## DataFlow Integration

The service registers provider-backed audio playback and capture operations with the Service Manager. Applications can select an available provider through the typed DataFlow contracts or the `DataFlow` helper without binding directly to codec or device implementations.

## How to Use

### Environment Requirements

Please refer to the following documentation:

- [ESP-Brookesia Programming Guide - Versioning](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia Programming Guide - Development Environment Setup](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-dev-environment)

### Add to Your Project

Please refer to [ESP-Brookesia Programming Guide - How to Obtain and Use Components](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-component-usage).
