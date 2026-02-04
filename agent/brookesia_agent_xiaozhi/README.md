# ESP-Brookesia Xiaozhi Agent

* [中文版本](./README_CN.md)

## Overview

`brookesia_agent_xiaozhi` is a XiaoZhi AI agent implementation for the ESP-Brookesia ecosystem, providing:

- **XiaoZhi Platform Integration**: Communication with XiaoZhi AI platform based on `esp_xiaozhi` SDK
- **Real-time Voice Interaction**: OPUS audio codec support at 16kHz sample rate and 24kbps bitrate
- **Rich Event Support**: Speaking/listening status changes, agent/user text, emote events
- **Manual Listening Control**: Manual start/stop listening for Manual chat mode
- **Interrupt Speaking**: Support for interrupting agent speech
- **Unified Lifecycle Management**: Unified agent lifecycle management based on `brookesia_agent_manager` framework

## Features

### Supported General Functions

| Function | Description |
|----------|-------------|
| `InterruptSpeaking` | Interrupt agent speaking |

### Supported General Events

| Event | Description |
|-------|-------------|
| `SpeakingStatusChanged` | Speaking status changed |
| `ListeningStatusChanged` | Listening status changed |
| `AgentSpeakingTextGot` | Agent speaking text received |
| `UserSpeakingTextGot` | User speaking text received |
| `EmoteGot` | Emote event |

## How to Use

### Development Environment Requirements

Before using this library, please ensure the following SDK development environment is installed:

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

### Adding to Project

`brookesia_agent_xiaozhi` has been uploaded to the [Espressif Component Registry](https://components.espressif.com/). You can add it to your project in the following ways:

1. **Using Command Line**

   Run the following command in your project directory:

   ```bash
   idf.py add-dependency "espressif/brookesia_agent_xiaozhi"
   ```

2. **Modify Configuration File**

   Create or modify the *idf_component.yml* file in your project directory:

   ```yaml
   dependencies:
     espressif/brookesia_agent_xiaozhi: "*"
   ```

### Kconfig Options

| Option | Description | Default |
|--------|-------------|---------|
| `BROOKESIA_AGENT_XIAOZHI_ENABLE_AUTO_REGISTER` | Enable automatic plugin registration | `y` |
| `BROOKESIA_AGENT_XIAOZHI_ENABLE_DEBUG_LOG` | Enable debug logs | `n` |
| `BROOKESIA_AGENT_XIAOZHI_GET_HTTP_INFO_INTERVAL_MS` | Activation check interval (ms) | `30000` |
| `BROOKESIA_AGENT_XIAOZHI_WAKE_WORD` | Wake word | `"Hi,ESP"` |
