# ESP-Brookesia Audio Service

* [中文版本](./README_CN.md)

## Overview

`brookesia_service_audio` is an audio service for the ESP-Brookesia ecosystem, providing:

- **Audio Playback**: Supports playing audio files from URLs with playback controls such as pause, resume, and stop.
- **Audio Codec**: Supports multiple audio codec formats (PCM, OPUS, G711A) with encoding and decoding capabilities.
- **Volume Control**: Supports volume setting and querying, providing complete volume management capabilities.
- **Playback State Management**: Tracks playback state in real-time (Idle, Playing, Paused) and notifies state changes through events.
- **Encoder Management**: Supports encoder start, stop, and configuration, with configurable encoder read data size.
- **Decoder Management**: Supports decoder start, stop, and data input, with streaming decoding support.
- **HAL Audio Integration**: Uses `AudioCodecPlayerIface` and `AudioCodecRecorderIface` from `brookesia_hal_interface` to obtain player and recorder handles, volume control, and board audio defaults.
- **Persistent Storage**: Optionally works with the `brookesia_service_nvs` service to persistently save volume and other information.

## Table of Contents

- [ESP-Brookesia Audio Service](#esp-brookesia-audio-service)
  - [Overview](#overview)
  - [Table of Contents](#table-of-contents)
  - [Features](#features)
    - [Audio Codec Formats](#audio-codec-formats)
    - [Playback Control](#playback-control)
    - [HAL Audio Integration](#hal-audio-integration)
    - [Encoder Configuration](#encoder-configuration)
    - [Decoder Configuration](#decoder-configuration)
    - [Event Notifications](#event-notifications)
  - [Development Environment Requirements](#development-environment-requirements)
  - [Adding to Project](#adding-to-project)

## Features

### Audio Codec Formats

Audio Service supports the following audio codec formats:

| Format | Encode | Decode | Description |
|--------|--------|--------|-------------|
| **PCM** | ✅ | ✅ | Lossless audio format |
| **OPUS** | ✅ | ✅ | Supports VBR and fixed bitrate configuration |
| **G711A** | ✅ | ✅ | Telephone quality audio format |

### Playback Control

The following playback control operations are supported:

- **Play**: Play audio files from a specified URL
- **Pause**: Pause current playback
- **Resume**: Resume playback from paused state
- **Stop**: Stop current playback

### HAL Audio Integration

- **Typed HAL Access**: Retrieves `AudioCodecPlayerIface` and `AudioCodecRecorderIface` through `get_first_interface<T>()` during service startup.
- **Less Codec Coupling**: Player volume, open, close, and recorder gain setup are routed through HAL interfaces instead of direct service-layer `esp_codec` calls.
- **Board Defaults**: Recorder sample rate, sample bits, channel count, mic layout, and gain settings are taken from HAL-provided board configuration.

### Encoder Configuration

Encoder supports flexible configuration options:

- **Codec Format**: PCM, OPUS, G711A
- **Channels**: 1-4 channels
- **Sample Bits**: 8, 16, 24, 32 bits
- **Sample Rate**: 8000, 16000, 24000, 32000, 44100, 48000 Hz
- **Frame Duration**: Configurable frame duration (milliseconds)
- **OPUS Extra Configuration**: VBR switch, bitrate settings

### Decoder Configuration

Decoder supports the following configuration:

- **Codec Format**: PCM, OPUS, G711A
- **Channels**: 1-4 channels
- **Sample Bits**: 8, 16, 24, 32 bits
- **Sample Rate**: 8000, 16000, 24000, 32000, 44100, 48000 Hz
- **Frame Duration**: Configurable frame duration (milliseconds)

### Event Notifications

Audio Service provides the following event notifications:

- **Playback State Changed**: Triggered when playback state changes (Idle, Playing, Paused)
- **Encoder Event**: Triggered when encoder events occur
- **Encoder Data Ready**: Triggered when encoded data is ready

## Development Environment Requirements

Before using this library, please ensure the following SDK development environment is installed:

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`
- `espressif/brookesia_hal_interface`: `0.7.*`
- `espressif/brookesia_hal_adaptor`: `0.7.*`

> [!NOTE]
> For SDK installation instructions, please refer to [ESP-IDF Programming Guide - Installation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

## Adding to Project

`brookesia_service_audio` has been uploaded to the [Espressif Component Registry](https://components.espressif.com/). You can add it to your project in the following ways:

1. **Using Command Line**

   Run the following command in your project directory:

   ```bash
   idf.py add-dependency "espressif/brookesia_service_audio"
   ```

2. **Modify Configuration File**

   Create or modify the *idf_component.yml* file in your project directory:

   ```yaml
   dependencies:
     espressif/brookesia_service_audio: "*"
   ```

For detailed instructions, please refer to [Espressif Documentation - IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).
