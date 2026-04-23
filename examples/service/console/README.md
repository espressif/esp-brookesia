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
  - [🔨 How to Use](#-how-to-use)
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

This example manages hardware through the [brookesia_hal_boards](https://components.espressif.com/components/espressif/brookesia_hal_boards) component.

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

## 🚀 Quick Start

After flashing the firmware successfully, refer to the [Quick Start Tutorial](./docs/tutorial.md) to begin exploring the service console example.

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

> [!TIP]
> - The `JSON params` of `svc_call` cannot contain spaces. For example: `svc_call Wifi TriggerGeneralAction { "Action": "Start" }` is incorrect, the correct format is: `svc_call Wifi TriggerGeneralAction {"Action":"Start"}`.
> - For detailed parameter descriptions and usage of service commands, please refer to the **Service Interfaces** sections in each service chapter of the **ESP-Brookesia Programming Guide**. You can also check the [CLI command example](https://docs.espressif.com/projects/esp-brookesia/en/latest/service/wifi.html#helper-contract-service-wifi-main-functions-triggergeneralaction-cli-command) (replace `null` with your actual parameter values).

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
