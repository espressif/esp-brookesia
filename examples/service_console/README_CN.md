# 服务控制台示例

[English Version](./README.md)

本示例演示了如何通过控制台运行和测试 ESP-Brookesia 的服务框架功能。

本示例提供了一个交互式命令行界面，允许您通过串口控制台与各种系统服务进行交互。目前支持的服务列表请参考 [调用服务函数](#调用服务函数) 章节。

本示例可作为开发和调试 ESP-Brookesia 服务的实用工具。

## 📑 目录

- [服务控制台示例](#服务控制台示例)
  - [📑 目录](#-目录)
  - [🚀 快速入门](#-快速入门)
    - [准备工作](#准备工作)
    - [ESP-IDF 要求](#esp-idf-要求)
  - [🔨 如何使用示例](#-如何使用示例)
    - [设置目标](#设置目标)
      - [对于支持的开发板](#对于支持的开发板)
      - [对于其他开发板](#对于其他开发板)
    - [配置](#配置)
    - [编译和烧录示例](#编译和烧录示例)
  - [📖 命令说明](#-命令说明)
    - [基本命令](#基本命令)
      - [列出所有服务](#列出所有服务)
      - [列出服务函数和事件](#列出服务函数和事件)
      - [订阅/取消订阅服务事件](#订阅取消订阅服务事件)
      - [停止服务绑定](#停止服务绑定)
    - [调用服务函数](#调用服务函数)
    - [调试命令](#调试命令)
      - [内存分析器](#内存分析器)
      - [线程分析器](#线程分析器)
      - [时间分析器](#时间分析器)
    - [RPC 命令](#rpc-命令)
  - [🔍 故障排除](#-故障排除)
    - [常见问题](#常见问题)
  - [💬 技术支持与反馈](#-技术支持与反馈)

## 🚀 快速入门

### 准备工作

本示例的基础功能支持搭载 `ESP32-S3` 或 `ESP32-P4` 芯片且 `Flash >= 8MB` 的开发板。

对于需要使用外部设备的功能（如 Audio、Emote、Agent 等），需要配合 `esp_board_manager` 组件使用。默认支持的开发板包括：

- [EchoEar](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s3/echoear/index.html)
- [ESP32-S3-BOX-3](https://github.com/espressif/esp-box)
- [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32p4/esp32-p4-function-ev-board/index.html)

示例通过串口控制台提供交互式命令行界面。

### ESP-IDF 要求

本示例支持以下 ESP-IDF 版本：

- ESP-IDF TAG `v5.5.2` 或 `release/v5.5` 最新版本

> [!WARNING]
> **请注意**：不推荐使用 VSCode 的扩展插件安装 ESP-IDF 环境和编译本示例，可能会导致编译失败。

请参照 [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html) 设置开发环境。**强烈推荐** 通过 [编译第一个工程](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#id8) 来熟悉 ESP-IDF，并确保环境配置正确。

## 🔨 如何使用示例

### 设置目标

#### 对于支持的开发板

如果使用 `esp_board_manager` 组件支持的开发板，请运行以下命令进行选择：

```bash
idf.py gen-bmgr-config -b <board> -c ./boards
```

其中：

- `<board>` 为开发板名称
- `./boards` 为开发板配置文件目录

默认支持的开发板名称如下：

- `echoear_core_board_v1_2`
- `esp_box_3`
- `esp32_p4_function_ev`

> [!NOTE]
> - 关于如何添加自定义开发板，请参考 [esp_board_manager 组件文档](https://github.com/espressif/esp-gmf/blob/main/packages/esp_board_manager/README_CN.md) 了解更多信息。
> - 本示例默认使用 [idf_ext.py](./idf_ext.py) 文件对 `idf.py gen-bmgr-config` 命令进行扩展，用户无需手动下载 `esp_board_manager` 组件和设置 `IDF_EXTRA_ACTIONS_PATH` 环境变量。

#### 对于其他开发板

如果使用其他开发板，请运行以下命令选择目标芯片：

```bash
idf.py set-target <target>
```

### 配置

运行 `idf.py menuconfig` 可以在 `Example Configuration` 菜单中配置以下选项：

- **Console Configuration**：配置控制台相关选项，如是否在 Flash 中存储命令历史、最大命令行长度等
- **Board Manager**：用于表示启用了 `esp_board_manager` 组件支持的开发板。该选项应在配置目标时自动启用，请勿手动配置
- **Services**：启用或禁用特定服务
- **Agents**：配置 Agent 信息，包括 API 密钥、Bot 配置等

### 编译和烧录示例

编译项目并将其烧录到开发板上，运行监视工具可查看串行端口输出（将 `PORT` 替换为所用开发板的串行端口名）：

```bash
idf.py -p PORT build flash monitor
```

输入 `Ctrl-]` 可退出串口监视。

有关配置和使用 ESP-IDF 编译项目的完整步骤，请参阅 [ESP-IDF 快速入门指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html)。

## 📖 命令说明

### 基本命令

本示例提供了管理所有注册服务的基础命令。通过这些命令，您可以查看可用服务、调用服务函数、订阅服务事件等。

#### 列出所有服务

查看所有已注册的服务：

```bash
svc_list
```

#### 列出服务函数和事件

查看指定服务的所有函数：

```bash
svc_funcs <服务名>
```

查看指定服务的所有事件：

```bash
svc_events <服务名>
```

#### 订阅/取消订阅服务事件

订阅服务事件（例如 WiFi 扫描结果更新）：

```bash
svc_subscribe <服务名> <事件名>
```

取消订阅服务事件：

```bash
svc_unsubscribe <服务名> <事件名>
```

#### 停止服务绑定

停止指定服务的绑定：

```bash
svc_stop <服务名>
```

### 调用服务函数

使用 `svc_call` 命令调用服务函数：

```bash
svc_call <服务名> <函数名> [JSON 参数]
```

JSON 参数为 JSON 格式。例如：

```bash
svc_call NVS Set {"KeyValuePairs":{"key1":"value1"}}
```

有关各服务的详细函数说明，请参考对应的服务文档：

- 💾 [NVS 服务](./docs/cmd_nvs_cn.md)：非易失性存储服务
- 📶 [WiFi 服务](./docs/cmd_wifi_cn.md)：WiFi 连接和管理服务
- 🕐 [SNTP 服务](./docs/cmd_sntp_cn.md)：网络时间同步服务
- 🎵 [音频服务](./docs/cmd_audio_cn.md) (*)：音频播放控制服务
- 🤖 [Agent 服务](./docs/cmd_agent_cn.md) (*)：AI 代理服务（Coze、OpenAI）
- 😊 [Emote 服务](./docs/cmd_emote_cn.md) (*)：表情服务

> [!NOTE]
> (*) 标记的服务默认仅在支持的开发板上可以正常使用，请参考 [准备工作](#准备工作) 章节了解默认支持的开发板列表。

### 调试命令

本示例提供了调试命令，用于分析和监控系统性能：

#### 内存分析器

打印内存分析器信息：

```bash
debug_mem
```

#### 线程分析器

打印线程分析器信息：

```bash
debug_thread
```

使用自定义排序和采样持续时间：

```bash
# 设置主排序方式（none|core，默认：core）
debug_thread -p core

# 设置次排序方式（cpu|priority|stack|name，默认：cpu）
debug_thread -s cpu

# 设置采样持续时间（毫秒，默认：1000）
debug_thread -d 2000

# 组合使用
debug_thread -p core -s cpu -d 2000
```

#### 时间分析器

打印时间分析器报告：

```bash
debug_time_report
```

清除所有时间分析器数据：

```bash
debug_time_clear
```

> [!NOTE]
> 时间分析器数据需要在代码中显式调用接口（如 `BROOKESIA_TIME_PROFILER_SCOPE()`、`BROOKESIA_TIME_PROFILER_START_EVENT()`、`BROOKESIA_TIME_PROFILER_END_EVENT()`）来采集。

### RPC 命令

本示例还提供了 RPC 命令，允许您通过网络调用远程设备上的服务函数，并订阅远程服务事件。

有关 RPC 命令的详细说明，请参考 [RPC 命令](./docs/cmd_rpc_cn.md) 文档。

## 🔍 故障排除

### 常见问题

- **Windows 系统下编译失败**：请参考 [ESP-IDF 要求](#esp-idf-要求) 章节确保环境配置正确。
- **命令无法识别**：确保已正确编译和烧录示例，并且串口连接正常。
- **服务未找到**：使用 `svc_list` 命令查看所有可用服务，确保所需服务已启用。
- **RPC 连接失败**：确保设备已连接到同一 WiFi 网络。

## 💬 技术支持与反馈

请通过以下渠道进行反馈：

- 有关技术问题，请访问 [esp32.com](https://esp32.com/viewforum.php?f=52&sid=86e7d3b29eae6d591c965ec885874da6) 论坛
- 有关功能请求或错误报告，请创建新的 [GitHub 问题](https://github.com/espressif/esp-brookesia/issues)

我们会尽快回复。
