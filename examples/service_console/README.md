# Service Console Example

[中文版本](./README_CN.md)

This example demonstrates how to run and test ESP-Brookesia service framework functionality through the console.

This example provides an interactive command-line interface that allows you to interact with various system services through the serial console. For a list of currently supported services, please refer to the [Call Service Functions](#call-service-functions) section.

This example can be used as a practical tool for developing and debugging ESP-Brookesia services.

## 📑 Table of Contents

- [Service Console Example](#service-console-example)
  - [📑 Table of Contents](#-table-of-contents)
  - [🚀 Quick Start](#-quick-start)
    - [Prerequisites](#prerequisites)
    - [ESP-IDF Requirements](#esp-idf-requirements)
  - [🔨 How to Use the Example](#-how-to-use-the-example)
    - [Set Target](#set-target)
      - [For Supported Boards](#for-supported-boards)
      - [For Other Boards](#for-other-boards)
    - [Configuration](#configuration)
    - [Build and Flash the Example](#build-and-flash-the-example)
  - [📖 Command Reference](#-command-reference)
    - [Basic Commands](#basic-commands)
      - [List All Services](#list-all-services)
      - [List Service Functions and Events](#list-service-functions-and-events)
      - [Subscribe/Unsubscribe to Service Events](#subscribeunsubscribe-to-service-events)
      - [Stop Service Binding](#stop-service-binding)
    - [Call Service Functions](#call-service-functions)
    - [Debug Commands](#debug-commands)
      - [Memory Profiler](#memory-profiler)
      - [Thread Profiler](#thread-profiler)
      - [Time Profiler](#time-profiler)
    - [RPC Commands](#rpc-commands)
  - [🔍 Troubleshooting](#-troubleshooting)
    - [Common Issues](#common-issues)
  - [💬 Technical Support and Feedback](#-technical-support-and-feedback)

## 🚀 Quick Start

### Prerequisites

The basic functionality of this example supports development boards with `ESP32-S3` or `ESP32-P4` chips and `Flash >= 8MB`.

For features that require external devices (such as Audio, Emote, Agent, etc.), the `esp_board_manager` component is required. Default supported boards include:

- [EchoEar](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/echoear/index.html)
- [ESP32-S3-BOX-3](https://github.com/espressif/esp-box)
- [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/index.html)

The example provides an interactive command-line interface through the serial console.

### ESP-IDF Requirements

This example supports the following ESP-IDF versions:

- ESP-IDF release/v5.5 latest version

Please refer to the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) to set up your development environment. **It is strongly recommended** to familiarize yourself with ESP-IDF by [building your first project](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#id8) and ensure your environment is configured correctly.

## 🔨 How to Use the Example

### Set Target

#### For Supported Boards

If using a board supported by the `esp_board_manager` component, run the following command to select it:

```bash
idf.py gen-bmgr-config -b <board> -c ./boards
```

Where:

- `<board>` is the board name
- `./boards` is the board configuration file directory

Default supported board names are as follows:

- `echoear_core_board_v1_2`
- `esp_box_3`
- `esp32_p4_function_ev`

> [!NOTE]
> - For information on how to add custom boards, please refer to the [esp_board_manager component documentation](https://github.com/espressif/esp-gmf/blob/main/packages/esp_board_manager/README.md) for more information.
> - This example uses the [idf_ext.py](./idf_ext.py) file by default to extend the `idf.py gen-bmgr-config` command, so users do not need to manually download the `esp_board_manager` component or set the `IDF_EXTRA_ACTIONS_PATH` environment variable.

#### For Other Boards

If using other boards, run the following command to select the target chip:

```bash
idf.py set-target <target>
```

### Configuration

Run `idf.py menuconfig` to configure the following options in the `Example Configuration` menu:

- **Console Configuration**: Configure console-related options, such as whether to store command history in Flash, maximum command line length, etc.
- **Board Manager**: Indicates that a board supported by the `esp_board_manager` component is enabled. This option should be automatically enabled when configuring the target, please do not configure it manually.
- **Services**: Enable or disable specific services
- **Agents**: Configure Agent information, including API keys, Bot configuration, etc.

### Build and Flash the Example

Build the project and flash it to your development board. Run the monitor tool to view serial port output (replace `PORT` with your development board's serial port name):

```bash
idf.py -p PORT build flash monitor
```

Press `Ctrl-]` to exit the serial monitor.

For complete steps on configuring and using ESP-IDF to build projects, please refer to the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).

## 📖 Command Reference

### Basic Commands

This example provides basic commands for managing all registered services. Through these commands, you can view available services, call service functions, subscribe to service events, etc.

#### List All Services

View all registered services:

```bash
svc_list
```

#### List Service Functions and Events

View all functions in a specific service:

```bash
svc_funcs <service_name>
```

View all events in a specific service:

```bash
svc_events <service_name>
```

#### Subscribe/Unsubscribe to Service Events

Subscribe to a service event (e.g., WiFi scan results update):

```bash
svc_subscribe <service_name> <event_name>
```

Unsubscribe from a service event:

```bash
svc_unsubscribe <service_name> <event_name>
```

#### Stop Service Binding

Stop the binding of a specific service:

```bash
svc_stop <service_name>
```

### Call Service Functions

Use the `svc_call` command to call service functions:

```bash
svc_call <service_name> <function_name> [JSON parameters]
```

JSON parameters are in JSON format. For example:

```bash
svc_call NVS Set {"KeyValuePairs":{"key1":"value1"}}
```

For detailed function descriptions of each service, please refer to the corresponding service documentation:

- 💾 [NVS Service](./docs/cmd_nvs.md): Non-volatile storage service
- 📶 [WiFi Service](./docs/cmd_wifi.md): WiFi connection and management service
- 🕐 [SNTP Service](./docs/cmd_sntp.md): Network time synchronization service
- 🎵 [Audio Service](./docs/cmd_audio.md) (*): Audio playback control service
- 🤖 [Agent Service](./docs/cmd_agent.md) (*): AI agent service (Coze, OpenAI)
- 😊 [Emote Service](./docs/cmd_emote.md) (*): Expression service

> [!NOTE]
> Services marked with (*) are only available on supported boards by default. Please refer to the [Prerequisites](#prerequisites) section for the list of default supported boards.

### Debug Commands

This example provides debug commands for analyzing and monitoring system performance:

#### Memory Profiler

Print memory profiler information:

```bash
debug_mem
```

#### Thread Profiler

Print thread profiler information:

```bash
debug_thread
```

Use custom sorting and sampling duration:

```bash
# Set primary sort (none|core, default: core)
debug_thread -p core

# Set secondary sort (cpu|priority|stack|name, default: cpu)
debug_thread -s cpu

# Set sampling duration (milliseconds, default: 1000)
debug_thread -d 2000

# Combine options
debug_thread -p core -s cpu -d 2000
```

#### Time Profiler

Print time profiler report:

```bash
debug_time_report
```

Clear all time profiler data:

```bash
debug_time_clear
```

> [!NOTE]
> Time profiler data requires explicit API calls in code (such as `BROOKESIA_TIME_PROFILER_SCOPE()`, `BROOKESIA_TIME_PROFILER_START_EVENT()`, `BROOKESIA_TIME_PROFILER_END_EVENT()`) to collect.

### RPC Commands

This example also provides RPC commands that allow you to call service functions on remote devices over the network and subscribe to remote service events.

For detailed descriptions of RPC commands, please refer to the [RPC Commands](./docs/cmd_rpc.md) documentation.

## 🔍 Troubleshooting

### Common Issues

- **Command not recognized**: Ensure that the example has been compiled and flashed correctly, and the serial connection is working properly.
- **Service not found**: Use the `svc_list` command to view all available services and ensure the required service is enabled.
- **RPC connection failed**: Ensure that devices are connected to the same WiFi network.

## 💬 Technical Support and Feedback

Please provide feedback through the following channels:

- For technical questions, please visit the [esp32.com](https://esp32.com/viewforum.php?f=52&sid=86e7d3b29eae6d591c965ec885874da6) forum
- For feature requests or bug reports, please create a new [GitHub issue](https://github.com/espressif/esp-brookesia/issues)

We will reply as soon as possible.
