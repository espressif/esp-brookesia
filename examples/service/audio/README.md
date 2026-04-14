# Audio Service Example

[中文版本](./README_CN.md)

This example demonstrates how to use the audio service (`brookesia_service_audio`) in the ESP-Brookesia framework. It covers audio file playback, playback control, volume management, encoder-decoder loopback testing, and AFE (Audio Front-End) voice activity detection and wake-word recognition.

## 📑 Table of Contents

- [Audio Service Example](#audio-service-example)
  - [📑 Table of Contents](#-table-of-contents)
  - [✨ Features](#-features)
  - [🚩 Getting Started](#-getting-started)
    - [Hardware Requirements](#hardware-requirements)
    - [Development Environment](#development-environment)
  - [🔨 How to Use](#-how-to-use)
  - [🚀 Runtime Overview](#-runtime-overview)
    - [Audio File Playback](#audio-file-playback)
    - [Playback Control](#playback-control)
    - [Volume Management](#volume-management)
    - [Encoder-Decoder Loopback Test](#encoder-decoder-loopback-test)
    - [AFE Audio Front-End](#afe-audio-front-end)
  - [🔍 Troubleshooting](#-troubleshooting)
  - [💬 Technical Support and Feedback](#-technical-support-and-feedback)

## ✨ Features

- 🎵 **Audio File Playback**: Supports playback of single or multiple URLs (local SPIFFS or network sources), with configurable loop count, queue append, or interruption of current playback
- ⏯️ **Playback Control**: Supports pause, resume, and stop operations, with real-time playback state updates through event subscription
- 🔊 **Volume Management**: Supports runtime get/set of playback volume (0-100), with automatic NVS persistence
- 🎙️ **Encoder-Decoder Loopback Test**: Starts the encoder to record microphone input, then plays it back through the decoder; supports PCM, OPUS, and G711A
- 🧠 **AFE Audio Front-End**: Integrates VAD (voice activity detection) and WakeNet (wake-word recognition), with real-time voice and wake events delivered through event subscription

## 🚩 Getting Started

### Hardware Requirements

This example uses the [brookesia_hal_boards](https://components.espressif.com/components/espressif/brookesia_hal_boards) component to manage hardware and supports the following development boards:

- ESP-VoCat V1.0
- ESP-VoCat V1.2
- ESP32-S3-BOX-3
- ESP32-S3-Korvo-2 V3
- ESP32-P4-Function-EV-Board
- ESP-SensairShuttle

### Development Environment

Please refer to the following documentation:

- [ESP-Brookesia Programming Guide - Versioning](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia Programming Guide - Development Environment Setup](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-dev-environment)

## 🔨 How to Use

Please refer to [ESP-Brookesia Programming Guide - How to Use Example Projects](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-example-projects).

## 🚀 Runtime Overview

After the example is flashed, the program automatically runs the following demo scenarios in sequence. All output is printed through the serial monitor.

### Audio File Playback

The example includes several MP3 files in the SPIFFS partition and demonstrates two playback modes:

**Single-file playback**: Plays three files in sequence. The second playback uses `interrupt: false` to append to the queue, and the third playback uses the default interrupt mode:

```text
[Demo: Play Single URL]
  Play 1.mp3
  Play 2.mp3 (append, loop 5 times)
  Play 3.mp3 (interrupt the previous playback)
```

**Multi-file playback**: Merges multiple URLs into a single playlist, with support for delayed start, looping, and timeout:

```text
[Demo: Play Multiple URLs]
  Playlist [4.mp3, non_exist.mp3] (non-existent files are skipped automatically)
  Playlist [5.mp3, 6.mp3] (interrupt after 2s delay, loop 2 times)
```

In both cases, playback state is printed in real time through event subscription (`PlayStateChanged` event).

### Playback Control

Demonstrates pause, resume, and stop operations during playback:

```text
[Demo: Play Control]
  Start playing 7.mp3 (loop 10 times)
  Pause after 3s
  Resume after 3s
  Stop after 3s
```

### Volume Management

Demonstrates reading, modifying, and verifying volume:

```text
[Demo: Volume Control]
  Read current volume
  Set volume to 90 and verify it takes effect
  Play 8.mp3 and 9.mp3
  Restore the original volume and verify it
```

> [!NOTE]
> Volume changes are automatically persisted through NVS and remain after power cycles.

### Encoder-Decoder Loopback Test

Tests PCM, OPUS, and G711A (not on ESP32-C5) in sequence. For each format, the flow is:

```text
[Demo: Encode -> Decode (Format: <format>)]
  Start encoder and record microphone input for 10 seconds (please speak into the microphone)
  Stop encoder and print recorded byte count
  Start decoder and play back the recorded audio in chunks
  Stop decoder
```

> [!NOTE]
> If no sound is captured during recording, the decode playback step is skipped.

### AFE Audio Front-End

Demonstrates VAD and wake-word detection:

```text
[Demo: AFE]
  Configure AFE (enable VAD + default WakeNet parameters)
  Start encoder and enable AFE processing
  Listen for AFE events for 120 seconds (please say the wake word and generate some noise)
  Stop encoder
```

During execution, each triggered VAD or wake-word event is printed to the serial console.

## 🔍 Troubleshooting

**No audio output**

- Check the speaker connection and volume setting; make sure the volume is greater than 0.
- Confirm that the SPIFFS partition has been flashed successfully (`idf.py flash` should include both the partition table and partition contents).

**No sound during encoder-decoder loopback**

- Make sure the microphone is working correctly, and speak or make noise during recording.
- Check the serial log for `"recorded X bytes"`; if it is 0, recording did not succeed.

**No AFE events are triggered**

- Confirm that the wake-word model has been flashed successfully into the `model` partition.
- Speak the full wake word close to the microphone and avoid excessive background noise.

**Build fails in VSCode**

Install ESP-IDF using the command line. See [Development Environment](#development-environment).

## 💬 Technical Support and Feedback

Please use the following channels for feedback:

- For technical questions, visit the [esp32.com](https://esp32.com/viewforum.php?f=52&sid=86e7d3b29eae6d591c965ec885874da6) forum
- For feature requests or bug reports, create a new [GitHub Issue](https://github.com/espressif/esp-brookesia/issues)

We will respond as soon as possible.
