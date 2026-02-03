| 支持的芯片 | ESP32-S3 | ESP32-P4 |
| :--------: | :------: | :------: |
|            |    ✅    |    ✅    |

# 服务控制台示例

[English Version](./README.md)

本示例演示了如何通过控制台运行和测试 ESP-Brookesia 的服务框架功能，提供了一个交互式命令行界面，支持历史命令记录。

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
    - [基本命令](#基本命令)
    - [服务文档](#服务文档)
    - [调试命令](#调试命令)
    - [RPC 命令](#rpc-命令)
  - [🔍 故障排除](#-故障排除)
    - [VSCode 编译失败](#vscode-编译失败)
    - [命令无法识别](#命令无法识别)
    - [服务未找到](#服务未找到)
    - [RPC 连接失败](#rpc-连接失败)
  - [💬 技术支持与反馈](#-技术支持与反馈)

## ✨ 功能特性

- 🎯 **服务管理**：查看、调用、订阅各类系统服务
- 🤖 **AI Agent**：支持 Xiaozhi、Coze、OpenAI 等多种 Agent 的共存以及运行时切换
- 🎵 **音频播放**：播放本地和网络音频资源
- 😊 **表情显示**：支持表情和动画显示
- 🔧 **调试工具**：内存、线程、时间分析器
- 🌐 **RPC 支持**：远程服务调用和事件订阅

## 🚩 快速入门

### 硬件要求

**基础功能**：搭载 `ESP32-S3` 或 `ESP32-P4` 芯片且 `Flash >= 8MB` 的开发板

**完整功能**（Audio、Emote、Agent）：需配合 `esp_board_manager` 组件，支持以下开发板：

| 开发板 | 链接 |
|--------|------|
| EchoEar V1.0/V1.2 | [文档](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s3/echoear/index.html) |
| ESP32-S3-BOX-3 | [GitHub](https://github.com/espressif/esp-box) |
| ESP32-S3-Korvo-2 | [文档](https://docs.espressif.com/projects/esp-adf/zh_CN/latest/design-guide/dev-boards/user-guide-esp32-s3-korvo-2.html) |
| ESP32-P4-Function-EV-Board | [文档](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32p4/esp32-p4-function-ev-board/index.html) |

### 开发环境

- ESP-IDF `v5.5.2` TAG（推荐）或 `release/v5.5` 分支

> [!WARNING]
> 不推荐使用 VSCode 扩展插件安装 ESP-IDF 环境，可能导致编译失败。请参照 [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html) 设置开发环境。

## 🔨 获取固件

### 方式一：在线烧录

使用 ESP Launchpad 可直接在浏览器中烧录预编译固件，无需搭建开发环境：

<a href="https://espressif.github.io/esp-launchpad/?flashConfigURL=https://espressif.github.io/esp-brookesia">
    <img alt="Try it with ESP Launchpad" src="https://dl.espressif.com/AE/esp-dev-kits/new_launchpad.png" width="316" height="100">
</a>

> [!NOTE]
> 在线烧录仅支持 [预设开发板](#硬件要求)，其他开发板请使用 [方式二](#方式二从源码编译) 编译。

### 方式二：从源码编译

**1. 选择开发板**

使用支持的开发板：

```bash
idf.py gen-bmgr-config -b <board> -c ./boards
```

可选的 `<board>` 值包括：

- `echoear_core_board_v1_0`
- `echoear_core_board_v1_2`
- `esp_box_3`
- `esp32_p4_function_ev`
- `esp32_s3_korvo2_v3`

使用其他开发板：

```bash
idf.py set-target <target>
```

> [!NOTE]
> 如需添加自定义开发板，请参考 [esp_board_manager 组件文档](https://github.com/espressif/esp-gmf/blob/main/packages/esp_board_manager/README_CN.md)。

**2. 配置选项**

运行 `idf.py menuconfig`，在 `Example Configuration` 菜单中配置：

| 选项 | 说明 |
|------|------|
| **Console** | 控制台选项（历史记录存储、命令行长度等） |
| **Services** | 启用/禁用服务（NVS、SNTP、WiFi、Audio） |
| **Agents** | AI Agent 配置（Xiaozhi、Coze、OpenAI、唤醒词） |
| **Expressions** | 表情显示功能配置 |

**3. 编译烧录**

```bash
idf.py -p PORT build flash monitor
```

按 `Ctrl-]` 退出串口监视。

## 🚀 快速体验

固件烧录成功后，您可以参考 [快速入门教程](./docs/tutorial_cn.md) 开始体验服务控制台示例。

## 📖 命令参考

### 基本命令

| 命令 | 说明 | 示例 |
|------|------|------|
| `svc_list` | 列出所有服务 | `svc_list` |
| `svc_funcs <服务>` | 列出服务函数 | `svc_funcs Wifi` |
| `svc_events <服务>` | 列出服务事件 | `svc_events Wifi` |
| `svc_call <服务> <函数> [参数]` | 调用服务函数 | `svc_call Wifi GetConnectedAps` |
| `svc_subscribe <服务> <事件>` | 订阅事件 | `svc_subscribe Wifi GeneralEventHappened` |
| `svc_unsubscribe <服务> <事件>` | 取消订阅 | `svc_unsubscribe Wifi GeneralEventHappened` |
| `svc_stop <服务>` | 停止服务绑定 | `svc_stop Wifi` |

### 服务文档

| 服务 | 文档 | 说明 |
|------|------|------|
| 💾 NVS | [cmd_nvs_cn.md](./docs/cmd_nvs_cn.md) | 非易失性存储 |
| 📶 WiFi | [cmd_wifi_cn.md](./docs/cmd_wifi_cn.md) | WiFi 连接管理 |
| 🕐 SNTP | [cmd_sntp_cn.md](./docs/cmd_sntp_cn.md) | 网络时间同步 |
| 🎵 Audio (*) | [cmd_audio_cn.md](./docs/cmd_audio_cn.md) | 音频播放控制 |
| 🤖 Agent (*) | [cmd_agent_cn.md](./docs/cmd_agent_cn.md) | AI 代理服务 |
| 😊 Emote (*) | [cmd_emote_cn.md](./docs/cmd_emote_cn.md) | 表情显示 |

> [!NOTE]
> (*) 标记的服务需要支持的开发板才能正常使用。

### 调试命令

| 命令 | 说明 | 示例 |
|------|------|------|
| `debug_mem` | 打印内存分析器信息 | `debug_mem` |
| `debug_thread [-p <sort>] [-s <sort>] [-d <ms>]` | 打印线程分析器信息 | `debug_thread -p core -s cpu -d 1000` |
| `debug_time_report` | 打印时间分析器报告 | `debug_time_report` |
| `debug_time_clear` | 清除时间分析器数据 | `debug_time_clear` |

> [!TIP]
> 详细的调试命令说明请参考 [调试命令](./docs/cmd_debug_cn.md)。

### RPC 命令

| 命令 | 说明 | 示例 |
|------|------|------|
| `svc_rpc_server <action> [-p <port>] [-s <services>]` | 管理 RPC 服务器 | `svc_rpc_server start` |
| `svc_rpc_call <host> <srv> <func> [params] [-p <port>] [-t <timeout>]` | 调用远程服务函数 | `svc_rpc_call 192.168.1.100 Wifi TriggerScanStart` |
| `svc_rpc_subscribe <host> <srv> <event> [-p <port>] [-t <timeout>]` | 订阅远程服务事件 | `svc_rpc_subscribe 192.168.1.100 Wifi ScanApInfosUpdated` |
| `svc_rpc_unsubscribe <host> <srv> <event> [-p <port>] [-t <timeout>]` | 取消订阅远程服务事件 | `svc_rpc_unsubscribe 192.168.1.100 Wifi ScanApInfosUpdated` |

> [!TIP]
> 详细的 RPC 命令说明请参考 [RPC 命令](./docs/cmd_rpc_cn.md)。

## 🔍 故障排除

### VSCode 编译失败

使用命令行安装 ESP-IDF，参考 [开发环境](#开发环境)。

### 命令无法识别

确保已正确编译烧录示例，并检查串口连接是否正常。

### 服务未找到

运行 `svc_list` 命令查看所有可用服务，确保所需服务已在 menuconfig 中启用。

### RPC 连接失败

确保设备已连接到同一 WiFi 网络。

## 💬 技术支持与反馈

请通过以下渠道进行反馈：

- 有关技术问题，请访问 [esp32.com](https://esp32.com/viewforum.php?f=52&sid=86e7d3b29eae6d591c965ec885874da6) 论坛
- 有关功能请求或错误报告，请创建新的 [GitHub 问题](https://github.com/espressif/esp-brookesia/issues)

我们会尽快回复。
