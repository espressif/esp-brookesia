# AI Chatbot Example

[中文版本](./README_CN.md)

This example demonstrates how to build a complete AI voice chatbot based on the ESP-Brookesia framework. It supports wake-word detection, multi-AI-agent integration with runtime switching, animated expression display, gesture navigation, and hardware control via MCP tools.

## 📑 Table of Contents

- [AI Chatbot Example](#ai-chatbot-example)
  - [📑 Table of Contents](#-table-of-contents)
  - [✨ Features](#-features)
  - [🚩 Getting Started](#-getting-started)
    - [Hardware Requirements](#hardware-requirements)
    - [Development Environment](#development-environment)
  - [🔨 Get the Firmware](#-get-the-firmware)
    - [Option 1: Flash Online](#option-1-flash-online)
    - [Option 2: Build from Source](#option-2-build-from-source)
  - [🚀 Quick Start](#-quick-start)
    - [First Boot — Wi-Fi Provisioning](#first-boot--wi-fi-provisioning)
    - [Voice Interaction](#voice-interaction)
    - [Settings Screen](#settings-screen)
  - [🔍 Troubleshooting](#-troubleshooting)
  - [💬 Technical Support and Feedback](#-technical-support-and-feedback)

## ✨ Features

- 🤖 **Multiple AI Agents**: Built-in support for XiaoZhi (default), Coze, and OpenAI agents with runtime switching
- 🎙️ **Voice Wake-up and Interaction**: Integrated AFE (Audio Front-End) engine with VAD and wake-word detection for hands-free conversation
- 😊 **Animated Expressions**: Real-time expression animations reflecting AI states such as speaking, thinking, and idle
- 🖐️ **Gesture Navigation**: Swipe gestures to switch between the expression main screen and the settings screen
- 🛠️ **MCP Hardware Control Tools**: The XiaoZhi agent can directly control display brightness (get/set/on/off) and audio volume (get/set/mute) via MCP (Model Context Protocol) tools
- 📶 **Wi-Fi Provisioning**: Automatically enters SoftAP provisioning mode on first boot; credentials are saved and reconnected automatically on subsequent boots
- 💾 **Persistent Settings**: User preferences such as brightness are saved to NVS and retained across power cycles
- 📊 **Runtime Profiling**: Built-in memory and thread profilers for development and debugging

## 🚩 Getting Started

### Hardware Requirements

A development board equipped with an `ESP32-S3`, `ESP32-P4`, or `ESP32-C5` chip, `Flash ≥ 8 MB, PSRAM ≥ 4 MB`, and featuring a display, touchscreen, microphone, and speaker.

This example uses the `esp_board_manager` component for hardware management. The following boards are officially supported:

- ESP-VoCat V1.0
- ESP-VoCat V1.2
- ESP32-S3-BOX-3
- ESP32-S3-Korvo-2 V3
- ESP32-P4-Function-EV-Board

### Development Environment

- ESP-IDF `v5.5.2` tag (recommended) or the `release/v5.5` branch

> [!WARNING]
> Installing ESP-IDF via the VSCode extension plugin is not recommended, as it may cause build failures. Please follow the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) to set up the development environment.

## 🔨 Get the Firmware

### Option 1: Flash Online

Use ESP Launchpad to flash the pre-built firmware directly in your browser, with no development environment required:

<a href="https://espressif.github.io/esp-brookesia/index.html">
    <img alt="Try it with ESP Launchpad" src="https://dl.espressif.com/AE/esp-dev-kits/new_launchpad.png" width="316" height="100">
</a>

> [!NOTE]
> Online flashing only supports the [officially supported boards](#hardware-requirements). For other boards, please use [Option 2](#option-2-build-from-source).

### Option 2: Build from Source

**1. Select a Board**

For a supported board:

```bash
idf.py gen-bmgr-config -b <board>
```

Available `<board>` values:

- `esp_vocat_board_v1_0`
- `esp_vocat_board_v1_2`
- `esp_box_3`
- `esp32_p4_function_ev`
- `esp32_s3_korvo2_v3`

> [!NOTE]
> To add a custom board, refer to the [esp_board_manager component documentation](https://github.com/espressif/esp-gmf/blob/main/packages/esp_board_manager/README.md).

**2. Configure AI Agents**

Run `idf.py menuconfig` and configure agents under the `Example Configuration` menu:

| Option | Description |
|--------|-------------|
| **Enable Xiaozhi agent** | Enable the XiaoZhi agent (enabled by default) |
| **Enable Coze agent** | Enable the Coze agent; requires App ID, public key, private key, and bot configuration |
| **Enable OpenAI agent** | Enable the OpenAI agent; requires an API key and model name |

> [!NOTE]
> Multiple agents can be enabled simultaneously and switched at runtime from the settings screen.

**3. Build and Flash**

```bash
idf.py -p PORT build flash monitor
```

Press `Ctrl-]` to exit the serial monitor.

## 🚀 Quick Start

### First Boot — Wi-Fi Provisioning

On power-up, the device checks for saved Wi-Fi credentials:

- **Not configured**: The device opens a SoftAP hotspot. Connect to it with your phone and enter the SSID and password of your home Wi-Fi on the provisioning page to complete the setup.
- **Already configured**: The device connects to the saved Wi-Fi automatically, and the AI agent (default XiaoZhi Agent) starts once the connection is established.

### Voice Interaction

After the AI agent starts, you can begin interacting by voice. When the AI responds, corresponding animated expressions will be shown on the screen in real time; once the conversation ends, the device automatically returns to standby mode.

During a conversation, the XiaoZhi Agent can invoke MCP tools to control the device, for example:

- "Turn up the volume a bit"
- "What is the current volume?"
- "Mute"
- "Unmute"
- "Set the screen brightness to 50%"
- "Turn off the screen"
- "Turn on the screen"

After a period of time without detecting voice, the AI agent will automatically enter sleep mode. At this time, say the wake word (default "Hi,ESP") to wake it up and start a conversation.

### Settings Screen

Swipe from anywhere on the **expression main screen** to open the **settings screen**:

| Setting | Description |
|---------|-------------|
| **Wi-Fi Status** | Shows the connected Wi-Fi SSID and connection state |
| **Brightness** | Drag the slider to adjust screen brightness (0–100%); saved automatically on power-off |
| **Volume** | Drag the slider to adjust playback volume (0–100%) |
| **Agent Switch** | Drop-down to select the active agent (XiaoZhi / Coze / OpenAI) |
| **Factory Reset** | Clears all saved settings; the device re-enters provisioning mode after reboot |

Swipe from the edge of the **settings screen** to return to the **expression main screen**.

## 🔍 Troubleshooting

**Device does not enter provisioning mode**

Ensure there is no stale provisioning data in Flash. Tap "Factory Reset" on the settings screen and reboot.

**AI does not respond**

- Check whether Wi-Fi is connected (view status on the settings screen).
- Verify that the corresponding agent is enabled in menuconfig and that credentials such as API Key / Bot ID are correctly filled in.
- Check the serial log to confirm whether the agent service started successfully.

**Wake word not recognized**

- Confirm the microphone is working and that the volume is not too low.
- Check that the wake-word model has been successfully flashed to the `model` partition.

**Build failure (VSCode)**

Install ESP-IDF via the command line. Refer to [Development Environment](#development-environment).

## 💬 Technical Support and Feedback

Please use the following channels for feedback:

- For technical questions, visit the [esp32.com](https://esp32.com/viewforum.php?f=52&sid=86e7d3b29eae6d591c965ec885874da6) forum
- For feature requests or bug reports, open a new [GitHub Issue](https://github.com/espressif/esp-brookesia/issues)

We will get back to you as soon as possible.
