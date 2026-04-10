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
  - [🔨 获取固件](#-获取固件)
    - [方式一：在线烧录](#方式一在线烧录)
    - [方式二：从源码编译](#方式二从源码编译)
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

搭载 `ESP32-S3`、`ESP32-P4` 或 `ESP32-C5` 芯片，`Flash >= 8MB, PSRAM >= 4MB`，并具备显示屏、触摸屏、麦克风和扬声器的开发板。

本示例通过 `esp_board_manager` 组件管理硬件，官方支持以下开发板：

- ESP-VoCat V1.0
- ESP-VoCat V1.2
- ESP32-S3-BOX-3
- ESP32-S3-Korvo-2 V3
- ESP32-P4-Function-EV-Board
- ESP-SensairShuttle

### 开发环境

- ESP-IDF `v5.5.2` TAG（推荐）或 `release/v5.5` 分支

> [!WARNING]
> 不推荐使用 VSCode 扩展插件安装 ESP-IDF 环境，可能导致编译失败。请参照 [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html) 设置开发环境。

## 🔨 获取固件

### 方式一：在线烧录

使用 ESP Launchpad 可直接在浏览器中烧录预编译固件，无需搭建开发环境：

<a href="https://espressif.github.io/esp-brookesia/index.html">
    <img alt="Try it with ESP Launchpad" src="https://dl.espressif.com/AE/esp-dev-kits/new_launchpad.png" width="316" height="100">
</a>

烧录完成后，使用 [MobaXterm](https://mobaxterm.mobatek.net/) 等串口工具连接设备（波特率 `115200`，Flow Control `None`）即可开始交互。

> [!NOTE]
> 在线烧录仅支持 [预设开发板](#硬件要求)，其他开发板请使用 [方式二](#方式二从源码编译) 编译。

### 方式二：从源码编译

**1. 选择开发板**

使用支持的开发板：

```bash
idf.py gen-bmgr-config -b <board>
```

可选的 `<board>` 值包括：

- `esp_vocat_board_v1_0`
- `esp_vocat_board_v1_2`
- `esp_box_3`
- `esp32_p4_function_ev`
- `esp32_s3_korvo2_v3`
- `esp_sensair_shuttle`

使用其他开发板：

```bash
idf.py set-target <target>
```

> [!NOTE]
> 如需添加自定义开发板，请参考 [esp_board_manager 组件文档](https://github.com/espressif/esp-gmf/blob/main/packages/esp_board_manager/README_CN.md)。

**2. 配置选项**

运行 `idf.py menuconfig`，在 `Example Configuration` 菜单中配置：

| 选项 | 说明 | 默认值 |
|------|------|--------|
| **Console → Store history** | 将命令历史持久化存储到 Flash | 开启 |
| **Console → Max command line length** | 最大命令行长度（字节） | 1024 |

**3. 编译烧录**

```bash
idf.py -p PORT build flash monitor
```

按 `Ctrl-]` 退出串口监视。

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
