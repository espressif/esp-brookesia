# ESP-Brookesia Coze Agent

* [中文版本](./README_CN.md)

## Overview

`brookesia_agent_coze` is a Coze agent implementation based on the ESP-Brookesia Agent Manager framework, providing:

- **Coze API Integration**: Real-time communication with the Coze platform via WebSocket, supporting voice conversations and text interactions.
- **OAuth2 Authentication**: Supports Coze platform's OAuth2 authentication mechanism with automatic access token acquisition and management.
- **Multiple Robot Support**: Supports configuring multiple robots with dynamic switching of the currently active robot.
- **Audio Codec**: Built-in audio codec support, defaulting to G711A format with 16kHz sample rate.
- **Emote Support**: Supports receiving and displaying emote events from the Coze platform.
- **Event Handling**: Supports listening and handling Coze platform events (such as insufficient credits balance, etc.).
- **Persistent Storage**: Optionally integrates with `brookesia_service_nvs` service for persistent storage of authentication information and robot configurations.

## Table of Contents

- [ESP-Brookesia Coze Agent](#esp-brookesia-coze-agent)
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

`brookesia_agent_coze` has been uploaded to the [Espressif Component Registry](https://components.espressif.com/). You can add it to your project in the following ways:

1. **Using Command Line**

   Run the following command in your project directory:

   ```bash
   idf.py add-dependency "espressif/brookesia_agent_coze"
   ```

2. **Modify Configuration File**

   Create or modify the *idf_component.yml* file in your project directory:

   ```yaml
   dependencies:
     espressif/brookesia_agent_coze: "*"
   ```

For detailed instructions, please refer to [Espressif Documentation - IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).
