# Service Console Example

[中文版本](./README_CN.md)

This example demonstrates how to run and test the ESP-Brookesia service framework via a serial console. It provides an interactive CLI (Command Line Interface) with command history support, allowing you to manage and invoke services, subscribe to events, perform remote RPC calls, and inspect runtime performance data directly from the command line.

## 📑 Table of Contents

- [Service Console Example](#service-console-example)
  - [📑 Table of Contents](#-table-of-contents)
  - [✨ Features](#-features)
  - [🚩 Getting Started](#-getting-started)
    - [Hardware Requirements](#hardware-requirements)
    - [Development Environment](#development-environment)
  - [🔨 Get the Firmware](#-get-the-firmware)
    - [Option 1: Flash Online](#option-1-flash-online)
    - [Option 2: Build from Source](#option-2-build-from-source)
  - [🚀 Quick Start](#-quick-start)
  - [📖 Command Reference](#-command-reference)
    - [Service Commands](#service-commands)
    - [Debug Commands](#debug-commands)
    - [RPC Commands](#rpc-commands)
  - [🔍 Troubleshooting](#-troubleshooting)
  - [💬 Technical Support and Feedback](#-technical-support-and-feedback)

## ✨ Features

- 🎯 **Service Management**: List and call service functions, and subscribe/unsubscribe to service events from the command line
- 🌐 **RPC Remote Calls**: Start an RPC server to call service functions or subscribe to events across devices over Wi-Fi
- 📊 **Runtime Profiling**: Built-in memory and thread profilers with on-demand or periodic output
- 📝 **Command History**: Command history is persisted to Flash and survives reboots (configurable)

## 🚩 Getting Started

### Hardware Requirements

A development board with an `ESP32-S3`, `ESP32-P4`, or `ESP32-C5` chip, `Flash ≥ 8 MB, PSRAM ≥ 4 MB`, and equipped with a display, touchscreen, microphone, and speaker.

This example uses the `esp_board_manager` component for hardware management. The following boards are officially supported:

- ESP-VoCat V1.0
- ESP-VoCat V1.2
- ESP32-S3-BOX-3
- ESP32-S3-Korvo-2 V3
- ESP32-P4-Function-EV-Board
- ESP-SensairShuttle

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

After flashing, connect to the device with a serial tool such as [MobaXterm](https://mobaxterm.mobatek.net/) (baud rate `115200`, Flow Control `None`) to start interacting.

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
- `esp_sensair_shuttle`

For other boards:

```bash
idf.py set-target <target>
```

> [!NOTE]
> To add a custom board, refer to the [esp_board_manager component documentation](https://github.com/espressif/esp-gmf/blob/main/packages/esp_board_manager/README.md).

**2. Configure Options**

Run `idf.py menuconfig` and configure under the `Example Configuration` menu:

| Option | Description | Default |
|--------|-------------|---------|
| **Console → Store history** | Persist command history to Flash | Enabled |
| **Console → Max command line length** | Maximum command line length (bytes) | 1024 |

**3. Build and Flash**

```bash
idf.py -p PORT build flash monitor
```

Press `Ctrl-]` to exit the serial monitor.

## 🚀 Quick Start

After flashing the firmware successfully, refer to the [Quick Start Tutorial](./docs/tutorial_cn.md) to begin exploring the service console example.

## 📖 Command Reference

### Service Commands

| Command | Description | Example |
|---------|-------------|---------|
| `svc_list` | List all registered services | `svc_list` |
| `svc_funcs <service>` | List all functions of a service | `svc_funcs Wifi` |
| `svc_events <service>` | List all events of a service | `svc_events Wifi` |
| `svc_call <service> <function> [JSON params]` | Call a service function | `svc_call Wifi GetConnectedAps` |
| `svc_subscribe <service> <event>` | Subscribe to a service event | `svc_subscribe Wifi ScanApInfosUpdated` |
| `svc_unsubscribe <service> <event>` | Unsubscribe from a service event | `svc_unsubscribe Wifi ScanApInfosUpdated` |
| `svc_stop <service>` | Release the service binding | `svc_stop Wifi` |

> [!WARNING]
> The `JSON params` of `svc_call` cannot contain spaces. For example: `svc_call Wifi TriggerGeneralAction { "Action": "Start" }` is incorrect, the correct format is: `svc_call Wifi TriggerGeneralAction {"Action":"Start"}`.

### Debug Commands

| Command | Description | Example |
|---------|-------------|---------|
| `debug_mem` | Print current memory usage | `debug_mem` |
| `debug_thread [-p <sort>] [-s <sort>] [-d <ms>]` | Print thread status with optional sort order and sampling interval | `debug_thread -p core -s cpu -d 1000` |
| `debug_time_report` | Print time profiler statistics report | `debug_time_report` |
| `debug_time_clear` | Clear time profiler history data | `debug_time_clear` |

> [!TIP]
> For detailed descriptions, refer to the [Debug Command Documentation](./docs/cmd_debug_cn.md).

### RPC Commands

The RPC feature allows calling service functions or subscribing to events across multiple devices over Wi-Fi.

| Command | Description | Example |
|---------|-------------|---------|
| `svc_rpc_server <action> [-p <port>] [-s <services>]` | Start/stop the local RPC server | `svc_rpc_server start` |
| `svc_rpc_call <host> <service> <function> [params] [-p <port>] [-t <timeout>]` | Call a service function on a remote device | `svc_rpc_call 192.168.1.100 Wifi TriggerScanStart` |
| `svc_rpc_subscribe <host> <service> <event> [-p <port>] [-t <timeout>]` | Subscribe to a service event on a remote device | `svc_rpc_subscribe 192.168.1.100 Wifi ScanApInfosUpdated` |
| `svc_rpc_unsubscribe <host> <service> <event> [-p <port>] [-t <timeout>]` | Unsubscribe from a service event on a remote device | `svc_rpc_unsubscribe 192.168.1.100 Wifi ScanApInfosUpdated` |

> [!TIP]
> For detailed descriptions, refer to the [RPC Command Documentation](./docs/cmd_rpc_cn.md).

## 🔍 Troubleshooting

**Build failure (VSCode)**

Install ESP-IDF via the command line. Refer to [Development Environment](#development-environment).

**Command not recognized**

Confirm the firmware has been flashed correctly and check that the serial connection is working. Type `help` to list all currently registered commands.

**Service not found**

Run `svc_list` to view all registered services. If the service is not listed, confirm that it has been enabled in menuconfig.

**RPC connection failure**

Ensure both devices are connected to the same Wi-Fi network and that the target device has started its RPC server with `svc_rpc_server start`.

## 💬 Technical Support and Feedback

Please use the following channels for feedback:

- For technical questions, visit the [esp32.com](https://esp32.com/viewforum.php?f=52&sid=86e7d3b29eae6d591c965ec885874da6) forum
- For feature requests or bug reports, open a new [GitHub Issue](https://github.com/espressif/esp-brookesia/issues)

We will get back to you as soon as possible.
