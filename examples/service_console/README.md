| Supported Chips | ESP32-S3 | ESP32-P4 | ESP32-C5 |
| :-------------: | :------: | :------: | :------: |
|                 |    ✅    |    ✅    |    ✅    |

# Service Console Example

[中文版本](./README_CN.md)

This example demonstrates how to run and test ESP-Brookesia service framework features through a console, providing an interactive command-line interface with command history support.

## 📑 Table of Contents

- [Service Console Example](#service-console-example)
  - [📑 Table of Contents](#-table-of-contents)
  - [✨ Features](#-features)
  - [🚩 Getting Started](#-getting-started)
    - [Hardware Requirements](#hardware-requirements)
    - [Development Environment](#development-environment)
  - [🔨 Get Firmware](#-get-firmware)
    - [Option 1: Online Flashing](#option-1-online-flashing)
    - [Option 2: Build from Source](#option-2-build-from-source)
  - [🚀 Quick Start](#-quick-start)
  - [📖 Command Reference](#-command-reference)
    - [Basic Commands](#basic-commands)
    - [Service Documentation](#service-documentation)
    - [Debug Commands](#debug-commands)
    - [RPC Commands](#rpc-commands)
  - [🔍 Troubleshooting](#-troubleshooting)
    - [VSCode Build Failure](#vscode-build-failure)
    - [Command Not Recognized](#command-not-recognized)
    - [Service Not Found](#service-not-found)
    - [RPC Connection Failed](#rpc-connection-failed)
  - [💬 Technical Support and Feedback](#-technical-support-and-feedback)

## ✨ Features

- 🎯 **Service Management**: View, call, and subscribe to various system services
- 🤖 **AI Agent**: Support for Xiaozhi, Coze, OpenAI and other Agents with runtime switching
- 🎵 **Audio Playback**: Play local and network audio resources
- 😊 **Expression Display**: Support for emoji and animation display
- 🔧 **Debug Tools**: Memory, thread, and time profilers
- 🌐 **RPC Support**: Remote service calls and event subscriptions

## 🚩 Getting Started

### Hardware Requirements

**Basic Features**: Development boards with `ESP32-S3`, `ESP32-P4` or `ESP32-C5` chip and `Flash >= 8MB`

**Full Features** (Audio, Emote, Agent): Requires `esp_board_manager` component, supporting the following boards:

| Board | Link |
|-------|------|
| EchoEar V1.0/V1.2 | [Documentation](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/echoear/index.html) |
| ESP32-S3-BOX-3 | [GitHub](https://github.com/espressif/esp-box) |
| ESP32-S3-Korvo-2 | [Documentation](https://docs.espressif.com/projects/esp-adf/en/latest/design-guide/dev-boards/user-guide-esp32-s3-korvo-2.html) |
| ESP32-P4-Function-EV-Board | [Documentation](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/index.html) |
| ESP32-C5-Sensair-Shuttle | [Documentation](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c5/esp-sensairshuttle/index.html) |

### Development Environment

- ESP-IDF `v5.5.2` TAG (recommended) or `release/v5.5` branch

> [!WARNING]
> Using VSCode extension to install ESP-IDF is not recommended as it may cause build failures. Please follow the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) to set up the development environment.

## 🔨 Get Firmware

### Option 1: Online Flashing

Use ESP Launchpad to flash pre-built firmware directly in your browser, no development environment required:

<a href="https://espressif.github.io/esp-brookesia/index.html">
    <img alt="Try it with ESP Launchpad" src="https://dl.espressif.com/AE/esp-dev-kits/new_launchpad.png" width="316" height="100">
</a>

After flashing, you can connect to the device with a serial tool such as [MobaXterm](https://mobaxterm.mobatek.net/) (baud rate `115200`, Flow Control `None`), and interact with the command line.

> [!NOTE]
> Online flashing only supports [preset boards](#hardware-requirements). For other boards, please use [Option 2](#option-2-build-from-source).

### Option 2: Build from Source

**1. Select Board**

For supported boards:

```bash
idf.py gen-bmgr-config -b <board> -c ./boards
```

Available `<board>` values:

- `echoear_core_board_v1_0`
- `echoear_core_board_v1_2`
- `esp_box_3`
- `esp32_p4_function_ev`
- `esp32_s3_korvo2_v3`
- `esp_sensair_shuttle`

For other boards:

```bash
idf.py set-target <target>
```

> [!NOTE]
> To add custom boards, refer to [esp_board_manager component documentation](https://github.com/espressif/esp-gmf/blob/main/packages/esp_board_manager/README.md).

**2. Configuration Options**

Run `idf.py menuconfig` and configure in the `Example Configuration` menu:

| Option | Description |
|--------|-------------|
| **Console** | Console options (history storage, command line length, etc.) |
| **Services** | Enable/disable services (NVS, SNTP, WiFi, Audio) |
| **Agents** | AI Agent configuration (Xiaozhi, Coze, OpenAI, wake word) |
| **Expressions** | Expression display configuration |

**3. Build and Flash**

```bash
idf.py -p PORT build flash monitor
```

Press `Ctrl-]` to exit the serial monitor.

## 🚀 Quick Start

After firmware flashing, refer to the [Quick Start Tutorial](./docs/tutorial.md) to start experiencing the service console example.

## 📖 Command Reference

### Basic Commands

| Command | Description | Example |
|---------|-------------|---------|
| `svc_list` | List all services | `svc_list` |
| `svc_funcs <service>` | List service functions | `svc_funcs Wifi` |
| `svc_events <service>` | List service events | `svc_events Wifi` |
| `svc_call <service> <func> [params]` | Call service function | `svc_call Wifi GetConnectedAps` |
| `svc_subscribe <service> <event>` | Subscribe to event | `svc_subscribe Wifi GeneralEventHappened` |
| `svc_unsubscribe <service> <event>` | Unsubscribe | `svc_unsubscribe Wifi GeneralEventHappened` |
| `svc_stop <service>` | Stop service binding | `svc_stop Wifi` |

### Service Documentation

| Service | Documentation | Description |
|---------|---------------|-------------|
| 💾 NVS | [cmd_nvs.md](./docs/cmd_nvs.md) | Non-volatile storage |
| 📶 WiFi | [cmd_wifi.md](./docs/cmd_wifi.md) | WiFi connection management |
| 🕐 SNTP | [cmd_sntp.md](./docs/cmd_sntp.md) | Network time synchronization |
| 🎵 Audio (*) | [cmd_audio.md](./docs/cmd_audio.md) | Audio playback control |
| 🤖 Agent (*) | [cmd_agent.md](./docs/cmd_agent.md) | AI agent service |
| 😊 Emote (*) | [cmd_emote.md](./docs/cmd_emote.md) | Expression display |

> [!NOTE]
> Services marked with (*) require supported development boards to function properly.

### Debug Commands

| Command | Description | Example |
|---------|-------------|---------|
| `debug_mem` | Print memory profiler info | `debug_mem` |
| `debug_thread [-p <sort>] [-s <sort>] [-d <ms>]` | Print thread profiler info | `debug_thread -p core -s cpu -d 1000` |
| `debug_time_report` | Print time profiler report | `debug_time_report` |
| `debug_time_clear` | Clear time profiler data | `debug_time_clear` |

> [!TIP]
> For detailed debug command documentation, refer to [Debug Commands](./docs/cmd_debug.md).

### RPC Commands

| Command | Description | Example |
|---------|-------------|---------|
| `svc_rpc_server <action> [-p <port>] [-s <services>]` | Manage RPC server | `svc_rpc_server start` |
| `svc_rpc_call <host> <srv> <func> [params] [-p <port>] [-t <timeout>]` | Call remote service function | `svc_rpc_call 192.168.1.100 Wifi TriggerScanStart` |
| `svc_rpc_subscribe <host> <srv> <event> [-p <port>] [-t <timeout>]` | Subscribe to remote event | `svc_rpc_subscribe 192.168.1.100 Wifi ScanApInfosUpdated` |
| `svc_rpc_unsubscribe <host> <srv> <event> [-p <port>] [-t <timeout>]` | Unsubscribe from remote event | `svc_rpc_unsubscribe 192.168.1.100 Wifi ScanApInfosUpdated` |

> [!TIP]
> For detailed RPC command documentation, refer to [RPC Commands](./docs/cmd_rpc.md).

## 🔍 Troubleshooting

### VSCode Build Failure

Use command line to install ESP-IDF, refer to [Development Environment](#development-environment).

### Command Not Recognized

Ensure the example is properly built and flashed, and check if the serial connection is working.

### Service Not Found

Run `svc_list` command to view all available services, ensure the required service is enabled in menuconfig.

### RPC Connection Failed

Ensure devices are connected to the same WiFi network.

## 💬 Technical Support and Feedback

Please provide feedback through the following channels:

- For technical questions, please visit the [esp32.com](https://esp32.com/viewforum.php?f=52&sid=86e7d3b29eae6d591c965ec885874da6) forum
- For feature requests or bug reports, please create a new [GitHub issue](https://github.com/espressif/esp-brookesia/issues)

We will reply as soon as possible.
