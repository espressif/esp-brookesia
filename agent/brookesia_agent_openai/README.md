# ESP-Brookesia OpenAI Agent

* [中文版本](./README_CN.md)

## Overview

`brookesia_agent_openai` is an OpenAI agent implementation based on the ESP-Brookesia Agent Manager framework, providing:

- **OpenAI Realtime API Integration**: Real-time communication with the OpenAI platform via WebRTC data channels, supporting voice conversations and text interactions.
- **Peer-to-Peer Communication**: Implements peer-to-peer (P2P) communication using `esp_peer`, supporting signaling and data channels.
- **Audio Codec**: Built-in audio codec support, defaulting to OPUS format with 16kHz sample rate and 24kbps bitrate.
- **Data Channel Message Handling**: Supports multiple data channel message types, including audio streams, text streams, function calls, session management, etc.
- **Real-Time Interaction**: Supports real-time audio input/output, providing low-latency voice interaction experience.
- **Persistent Storage**: Optionally integrates with `brookesia_service_nvs` service for persistent storage of configuration information.

## Table of Contents

- [ESP-Brookesia OpenAI Agent](#esp-brookesia-openai-agent)
  - [Overview](#overview)
  - [Table of Contents](#table-of-contents)
  - [How to Use](#how-to-use)
    - [Development Environment Requirements](#development-environment-requirements)
    - [Adding to Project](#adding-to-project)

## How to Use

### Development Environment Requirements

Before using this library, please ensure the following SDK development environment is installed:

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> For SDK installation instructions, please refer to [ESP-IDF Programming Guide - Installation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

### Adding to Project

`brookesia_agent_openai` has been uploaded to the [Espressif Component Registry](https://components.espressif.com/). You can add it to your project in the following ways:

1. **Using Command Line**

   Run the following command in your project directory:

   ```bash
   idf.py add-dependency "espressif/brookesia_agent_openai"
   ```

2. **Modify Configuration File**

   Create or modify the *idf_component.yml* file in your project directory:

   ```yaml
   dependencies:
     espressif/brookesia_agent_openai: "*"
   ```

For detailed instructions, please refer to [Espressif Documentation - IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).
