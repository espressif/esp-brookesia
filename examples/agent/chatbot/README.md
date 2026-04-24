# AI Chatbot Example

[中文版本](./README_CN.md)

This example demonstrates how to build a complete AI voice chatbot based on the ESP-Brookesia framework. It supports wake-word detection, multiple AI agents with runtime switching, expression animations, gesture navigation, and direct hardware control through MCP tools.

## 📑 Table of Contents

- [AI Chatbot Example](#ai-chatbot-example)
  - [📑 Table of Contents](#-table-of-contents)
  - [✨ Features](#-features)
  - [🚩 Getting Started](#-getting-started)
    - [Hardware Requirements](#hardware-requirements)
    - [Development Environment](#development-environment)
  - [🔨 How to Use](#-how-to-use)
  - [🔧 Configure AI Agents](#-configure-ai-agents)
  - [🚀 Quick Tryout](#-quick-tryout)
    - [First Boot and Wi-Fi Provisioning](#first-boot-and-wi-fi-provisioning)
    - [Voice Interaction](#voice-interaction)
    - [Settings Screen](#settings-screen)
  - [🔍 Troubleshooting](#-troubleshooting)
  - [💬 Technical Support and Feedback](#-technical-support-and-feedback)

## ✨ Features

- 🤖 **Multiple AI Agents**: Built-in XiaoZhi (default), Coze, and OpenAI agents, with runtime switching support
- 🎙️ **Voice Wake-Up and Interaction**: Integrates the AFE (Audio Front-End) engine with VAD and wake-word detection for hands-free conversations
- 😊 **Expression Animations**: Displays matching expression animations in real time for states such as speaking, thinking, and waiting
- 🖐️ **Gesture Navigation**: Swipe on the touch screen to switch between the expression main screen and the settings screen
- 🛠️ **MCP Hardware Control Tools**: The XiaoZhi agent can directly control screen brightness (get/set/on/off) and volume (get/set/mute) through MCP (Model Context Protocol) tools
- 📶 **Wi-Fi Provisioning**: On first boot, the device automatically enters SoftAP provisioning mode; after provisioning, credentials are saved and reconnected automatically
- 💾 **Persistent Settings**: User settings such as brightness are saved in NVS and retained across power cycles
- 📊 **Runtime Profiling**: Built-in memory and thread profilers for development and debugging

## 🚩 Getting Started

### Hardware Requirements

This example manages hardware through the [brookesia_hal_boards](https://components.espressif.com/components/espressif/brookesia_hal_boards) component, and supports development boards that meet the following requirements:

- Flash >= 16MB
- PSRAM >= 8MB
- Support Wi-Fi
- Support the following interfaces:

  - `AudioCodecPlayer`
  - `AudioCodecRecorder`
  - `DisplayPanel`
  - `DisplayTouch`
  - `StorageFs`

Please refer to the [ESP-Brookesia Programming Guide - Supported Boards](https://docs.espressif.com/projects/esp-brookesia/en/latest/hal/boards/index.html#hal-boards-sec-02) for a list of supported development boards.

### Development Environment

Please refer to the following documentation:

- [ESP-Brookesia Programming Guide - Versioning](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia Programming Guide - Development Environment Setup](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-dev-environment)

## 🔨 How to Use

<a href="https://espressif.github.io/esp-brookesia/index.html">
  <img alt="Try it with ESP Launchpad" src="https://dl.espressif.com/AE/esp-dev-kits/new_launchpad.png" width="400">
</a>

Please refer to [ESP-Brookesia Programming Guide - How to Use Example Projects](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-example-projects).

## 🔧 Configure AI Agents

Run `idf.py menuconfig` and configure the agents in the `Example Configuration` menu:

| Option | Description |
|------|------|
| **Enable Xiaozhi agent** | Enable the XiaoZhi agent (enabled by default) |
| **Enable Coze agent** | Enable the Coze agent; requires App ID, public key, private key, and bot configuration |
| **Enable OpenAI agent** | Enable the OpenAI agent; requires API Key and model name |

> [!NOTE]
> Multiple agents can be enabled at the same time, and the active agent can be switched at runtime from the settings screen.

## 🚀 Quick Tryout

### First Boot and Wi-Fi Provisioning

When the device powers on, it checks whether Wi-Fi credentials have already been saved:

- **Not configured**: The device automatically starts a SoftAP hotspot. Connect to it with your phone, then enter the SSID and password of your home Wi-Fi network on the provisioning page.
- **Already configured**: The device automatically connects to the saved Wi-Fi network. After the connection succeeds, the AI agent (XiaoZhi by default) starts automatically.

### Voice Interaction

Once the AI agent starts, you can begin talking to it. The default configuration is half-duplex mode, that is, the Agent will not listen to human voice when speaking, but can be interrupted by the wake word (default: `"Hi,ESP"`) to start listening to human voice.

While the Agent is responding, the screen shows matching expression animations. After the conversation ends, the device automatically returns to the standby state.

During conversations, the XiaoZhi agent can proactively call MCP tools to operate the device, for example:

- "Turn the volume up a bit"
- "What is the current volume?"
- "Mute"
- "Unmute"
- "Set the screen brightness to 50%"
- "Turn off the screen"
- "Turn on the screen"

If no human voice is detected for a period of time, the AI agent automatically enters sleep mode. At that point, say the wake word (default: `"Hi,ESP"`) to start a conversation again.

### Settings Screen

On the **expression main screen**, swipe from anywhere on the screen to enter the **settings screen**, which supports the following operations:

| Setting | Description |
|--------|------|
| **WiFi Status** | Shows the current Wi-Fi SSID and connection status |
| **Brightness** | Drag the slider to adjust screen brightness (0-100%); the value is saved after power-off |
| **Volume** | Drag the slider to adjust playback volume (0-100%) |
| **Agent Switch** | Select the active agent from the drop-down list (XiaoZhi / Coze / OpenAI) |
| **Factory Reset** | Clears all saved configuration; after reboot, the device enters provisioning again |

On the **settings screen**, swipe inward from any edge to return to the **expression main screen**.

## 🔍 Troubleshooting

**The device does not enter provisioning mode**

Make sure there is no old provisioning information left in Flash. You can tap "Factory Reset" in the settings screen and then reboot the device.

**The AI does not respond**

- Check whether Wi-Fi is connected on the settings screen.
- Check whether the corresponding agent is enabled in menuconfig, and confirm that API Key, Bot ID, and other credentials are filled in correctly.
- Check the serial log to verify that the agent service started successfully.

**The wake word does not respond**

- Make sure the microphone is working properly and the input volume is not too low.
- Check whether the wake-word model has been flashed successfully into the `model` partition.

**Build fails in VSCode**

Install ESP-IDF using the command line. See [Development Environment](#development-environment).

## 💬 Technical Support and Feedback

Please use the following channels for feedback:

- For technical questions, visit the [esp32.com](https://esp32.com/viewforum.php?f=52&sid=86e7d3b29eae6d591c965ec885874da6) forum
- For feature requests or bug reports, create a new [GitHub Issue](https://github.com/espressif/esp-brookesia/issues)

We will respond as soon as possible.
