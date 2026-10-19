# ESP-Brookesia HAL Adaptor

* [中文版本](./README_CN.md)

## Overview

`brookesia_hal_adaptor` is the board-level HAL adaptor for ESP-Brookesia. It builds on the device/interface model from `brookesia_hal_interface`, initializes real peripherals through `esp_board_manager`, and registers audio, display, and storage capabilities in the global HAL table so upper layers can discover and use them by name.

For more information, see the [ESP-Brookesia Programming Guide](https://docs.espressif.com/projects/esp-brookesia/en/latest/hal/adaptor.html).

## How to Use

### Environment Requirements

Please refer to the following documentation:

- [ESP-Brookesia Programming Guide - Versioning](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia Programming Guide - Development Environment Setup](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-dev-environment)

### Add to Your Project

Please refer to [ESP-Brookesia Programming Guide - How to Obtain and Use Components](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-component-usage).

## Audio Processor Recording

`BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_PROCESSOR_RECORDING_IMPL` defaults to enabled when the audio processor is enabled. Disable it for playback-only firmware to omit the high-level `EncoderIface` and `DecoderIface` capabilities and their AFE wake-word code. `PlaybackIface`, `CodecPlayerIface`, and `CodecRecorderIface` remain available. Playback still depends on the `av_processor` component.

## ADC Compatibility Patches

When the SDK no longer defines `SOC_ADC_CHANNEL_NUM`, feature-enabled ADC integrations use the equivalent ADC LL channel count. The Board Manager and codec patches preserve the existing ADC bounds checks; a required patch mismatch stops configuration. See the [patch list](https://docs.espressif.com/projects/esp-brookesia/en/latest/hal/adaptor.html#hal-adaptor-dependency-patches) and [source provenance](SOURCE.md).

## Extending Board Support

A custom device may still hold resources after initialization or release fails. Board adaptors can register cleanup callbacks for HAL to invoke on later release requests. Applications using an existing board do not need to register these callbacks themselves.

When adding a device, see the [cleanup registration interface](include/brookesia/hal_adaptor/board_manager.h) for registration and callback requirements.
