# 服务控制台示例

[English Version](./README.md)

本示例演示了如何通过串口控制台运行和测试 ESP-Brookesia 的服务框架。它提供了一个交互式 CLI（Command Line Interface）界面，支持历史命令记录，可直接通过命令行管理和调用各类服务、订阅事件、进行远程 RPC 调用以及查看运行时性能数据。

## 📑 目录

- [服务控制台示例](#服务控制台示例)
  - [📑 目录](#-目录)
  - [✨ 功能特性](#-功能特性)
  - [🚩 快速入门](#-快速入门)
    - [硬件要求](#硬件要求)
    - [开发环境](#开发环境)
  - [🔨 如何使用](#-如何使用)
  - [🚀 快速体验](#-快速体验)
  - [📖 命令参考](#-命令参考)
    - [服务命令](#服务命令)
    - [调试命令](#调试命令)
    - [RPC 命令](#rpc-命令)
  - [🔍 故障排除](#-故障排除)
  - [💬 技术支持与反馈](#-技术支持与反馈)

## ✨ 功能特性

- 🎯 **服务管理**：通过命令行列出、调用各类服务函数，以及订阅/取消订阅服务事件
- 🌐 **RPC 远程调用**：启动 RPC 服务器，跨设备调用服务函数或订阅远程事件
- 📊 **运行时分析**：内置内存与线程性能分析器，支持按需打印或定期自动输出
- 📝 **历史命令记录**：命令历史持久化存储于 Flash，重启后仍可回溯（可配置）

## 🚩 快速入门

### 硬件要求

本示例通过 [brookesia_hal_boards](https://components.espressif.com/components/espressif/brookesia_hal_boards) 组件管理硬件，支持以下开发板：

- ESP-VoCat V1.0
- ESP-VoCat V1.2
- ESP32-S3-BOX-3
- ESP32-S3-Korvo-2 V3
- ESP32-P4-Function-EV-Board
- ESP-SensairShuttle

### 开发环境

请参考以下文档：

- [ESP-Brookesia 编程指南 - 版本说明](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia 编程指南 - 开发环境搭建](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-dev-environment)

## 🔨 如何使用

<a href="https://espressif.github.io/esp-brookesia/index.html">
  <img alt="Try it with ESP Launchpad" src="https://dl.espressif.com/AE/esp-dev-kits/new_launchpad.png" width="400">
</a>

请参考 [ESP-Brookesia 编程指南 - 如何使用示例工程](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-example-projects)。

## 🚀 快速体验

固件烧录成功后，您可以参考 [快速入门教程](./docs/tutorial_cn.md) 开始体验服务控制台示例。

## 📖 命令参考

### 服务命令

| 命令 | 说明 | 示例 |
|------|------|------|
| `svc_list` | 列出所有已注册服务 | `svc_list` |
| `svc_funcs <服务>` | 列出服务的所有函数 | `svc_funcs Wifi` |
| `svc_events <服务>` | 列出服务的所有事件 | `svc_events Wifi` |
| `svc_call <服务> <函数> [JSON参数]` | 调用服务函数 | `svc_call Wifi GetConnectedAps` |
| `svc_subscribe <服务> <事件>` | 订阅服务事件 | `svc_subscribe Wifi ScanApInfosUpdated` |
| `svc_unsubscribe <服务> <事件>` | 取消订阅服务事件 | `svc_unsubscribe Wifi ScanApInfosUpdated` |
| `svc_stop <服务>` | 解除服务绑定 | `svc_stop Wifi` |

> [!WARNING]
`svc_call` 的 `JSON参数` 中不能存在空格。例如：`svc_call Wifi TriggerGeneralAction { "Action": "Start" }` 是错误的，正确的格式是：`svc_call Wifi TriggerGeneralAction {"Action":"Start"}`。

### 调试命令

| 命令 | 说明 | 示例 |
|------|------|------|
| `debug_mem` | 打印当前内存使用情况 | `debug_mem` |
| `debug_thread [-p <排序>] [-s <排序>] [-d <ms>]` | 打印线程运行状态（可指定排序方式和采样间隔） | `debug_thread -p core -s cpu -d 1000` |
| `debug_time_report` | 打印时间分析器统计报告 | `debug_time_report` |
| `debug_time_clear` | 清除时间分析器历史数据 | `debug_time_clear` |

> [!TIP]
> 详细说明请参考 [调试命令文档](./docs/cmd_debug_cn.md)。

### RPC 命令

RPC 功能允许通过 WiFi 在多个设备之间互相调用服务函数或订阅事件。

| 命令 | 说明 | 示例 |
|------|------|------|
| `svc_rpc_server <action> [-p <端口>] [-s <服务列表>]` | 启动/停止本机 RPC 服务器 | `svc_rpc_server start` |
| `svc_rpc_call <host> <服务> <函数> [参数] [-p <端口>] [-t <超时>]` | 调用远端设备的服务函数 | `svc_rpc_call 192.168.1.100 Wifi TriggerScanStart` |
| `svc_rpc_subscribe <host> <服务> <事件> [-p <端口>] [-t <超时>]` | 订阅远端设备的服务事件 | `svc_rpc_subscribe 192.168.1.100 Wifi ScanApInfosUpdated` |
| `svc_rpc_unsubscribe <host> <服务> <事件> [-p <端口>] [-t <超时>]` | 取消订阅远端设备的服务事件 | `svc_rpc_unsubscribe 192.168.1.100 Wifi ScanApInfosUpdated` |

> [!TIP]
> 详细说明请参考 [RPC 命令文档](./docs/cmd_rpc_cn.md)。

## 🔍 故障排除

**VSCode 编译失败**

使用命令行安装 ESP-IDF，参考 [开发环境](#开发环境)。

**命令无法识别**

确认固件已正确烧录，并检查串口连接是否正常。输入 `help` 查看当前已注册的全部命令。

**服务未找到**

运行 `svc_list` 查看已注册服务。若服务不在列表中，请在 menuconfig 中确认对应服务已启用。

**RPC 连接失败**

确认两端设备已连接到同一 WiFi 网络，且目标设备已通过 `svc_rpc_server start` 启动了 RPC 服务器。

## 💬 技术支持与反馈

请通过以下渠道进行反馈：

- 有关技术问题，请访问 [esp32.com](https://esp32.com/viewforum.php?f=52&sid=86e7d3b29eae6d591c965ec885874da6) 论坛
- 有关功能请求或错误报告，请创建新的 [GitHub 问题](https://github.com/espressif/esp-brookesia/issues)

我们会尽快回复。
