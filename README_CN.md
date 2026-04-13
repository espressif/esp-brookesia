<div align="center">
    <img src="docs/_static/brookesia_logo.png" alt="logo" width="800">
</div>

# ESP-Brookesia

* [English Version](./README.md)

## 概述

ESP-Brookesia 是一个面向 AIoT 设备的人机交互开发框架，旨在简化应用程序开发和 AI 能力集成的流程。它以 ESP-IDF 为基础，通过组件化架构为开发者提供从硬件抽象、系统服务到 AI 智能体的全栈支持，加速 HMI 与 AI 应用产品的开发与上市。

> [!NOTE]
> "[Brookesia](https://en.wikipedia.org/wiki/Brookesia)" 是一种变色龙属的物种，擅长于伪装和适应环境，这与 ESP-Brookesia 的目标紧密相关。该框架旨在提供一种灵活、可扩展的解决方案，能够适应各种不同的硬件设备和应用需求，就像 Brookesia 变色龙那样，具有高度的适应性和灵活性。

ESP-Brookesia 的主要特性包括：

- **原生 ESP-IDF 支持**：基于 C/C++ 开发，深度集成 ESP-IDF 开发体系与 ESP Registry 组件注册表，充分利用乐鑫开源组件生态；
- **可扩展的硬件抽象**：定义统一的硬件接口（音频、显示、触摸、存储等），并提供板级适配层，便于快速移植到不同硬件平台；
- **丰富的系统服务**：提供 Wi-Fi 连接、音视频处理等开箱即用的系统级服务，采用 Manager + Helper 架构实现解耦与扩展，为 Agent CLI 提供支持；
- **多 LLM 后端集成**：内置对 OpenAI、Coze、小智等主流 AI 平台的适配，提供统一的智能体管理与生命周期控制；
- **MCP 协议支持**：通过 Function Calling / MCP 协议将设备服务能力暴露给大语言模型，实现 LLM 与系统服务的统一通信；
- **AI 表达能力**：支持表情集、动画集等可视化 AI 表达，为拟人化交互提供丰富的视觉反馈。

ESP-Brookesia 的功能框架如下图所示，自底向上由 **环境与依赖**、**服务与框架** 以及 **应用层** 三个层级组成：

<div align="center">
    <img src="docs/_static/framework_overview.svg" alt="framework_overview" width="800">
</div>
<br>

- **Utils 工具集**：为上层模块提供通用基础能力，包含日志系统、状态机、任务调度器、插件管理、性能分析器等核心工具，以及将服务能力桥接到 MCP 引擎的 MCP 工具集；
- **HAL 硬件抽象**：定义统一的硬件访问接口并提供板级适配实现，支持音频、显示、触摸、LED、存储等标准化硬件接口；
- **General Service 通用服务**：提供 Wi-Fi、Audio、Video、NVS、SNTP 等系统级基础服务，均基于 Manager + Helper 架构，支持本地调用与 RPC 远程通信；
- **AI Agent 智能体框架**：提供 AI 智能体的统一管理框架，内置对 Coze、OpenAI、小智等主流 AI 平台的适配，通过 Function Calling / MCP 协议实现 LLM 与系统服务的双向通信；
- **AI Expression 智能体表达**：提供 AI 交互场景下的可视化表达能力，包括表情集与动画控制。

## 文档中心

- 中文：https://docs.espressif.com/projects/esp-brookesia/zh_CN
- English: https://docs.espressif.com/projects/esp-brookesia/en

## 版本说明

ESP-Brookesia 自 `v0.7` 版本起采用组件化管理，建议项目通过组件注册表获取所需组件。各组件独立迭代，但均具有相同的 `major.minor` 版本号，并且依赖相同的 ESP-IDF 版本。`release` 分支仅维护历史大版本，`master` 分支持续集成新特性。

| ESP-Brookesia | 依赖的 ESP-IDF | 主要变更 | 支持状态 |
| :-----------: | :------------: | :------: | :------: |
| master (v0.7) | >= v5.5, < 6.0 | 支持组件管理器 | 新功能开发分支 |
| release/v0.6 | >= v5.3, <= 5.5 | 预览支持系统框架，提供 ESP-VoCat 固件工程 | 停止维护 |

## 快速参考

### 硬件准备

您可以选择任意 ESP 系列开发板使用 ESP-Brookesia，或者参考 [esp_board_manager](https://github.com/espressif/esp-gmf/blob/main/packages/esp_board_manager) 支持的开发板快速开始。各系列芯片规格请见 [ESP 产品选型工具](https://products.espressif.com/)。

ESP 系列 SoC 采用先进工艺制程，提供业界领先的射频性能、低功耗特性和稳定可靠性，支持 Wi-Fi、蓝牙、LCD、摄像头、USB 等丰富外设，适用于 AIoT、智能家居、可穿戴设备等多种应用场景。

### 从 ESP 组件注册表获取组件

推荐通过 [ESP Component Registry](https://components.espressif.com/) 获取 ESP-Brookesia 组件。

以 `brookesia_service_wifi` 组件为例，可通过以下方式添加依赖：

**方式一：使用命令行**

```bash
idf.py add-dependency "espressif/brookesia_service_wifi"
```

**方式二：修改配置文件**

在工程目录下创建或修改 `idf_component.yml` 文件：

```yaml
dependencies:
   espressif/brookesia_service_wifi: "*"
```

更多组件管理器使用方法请参考 [ESP Registry Docs](https://docs.espressif.com/projects/idf-component-manager/en/latest/)。

ESP-Brookesia 中注册的组件如下：

<center>

#### 工具组件 (Utils)

| 组件 | 描述 | 版本 |
| --- | --- | --- |
| [brookesia_lib_utils](https://components.espressif.com/components/espressif/brookesia_lib_utils) | 核心工具库，提供任务调度器、线程配置、性能分析器、日志系统、状态机、插件系统等基础能力 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_lib_utils/badge.svg)](https://components.espressif.com/components/espressif/brookesia_lib_utils) |
| [brookesia_mcp_utils](https://components.espressif.com/components/espressif/brookesia_mcp_utils) | MCP 工具桥接库，将 Brookesia 服务函数或自定义回调注册为 MCP 工具，供大语言模型调用 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_mcp_utils/badge.svg)](https://components.espressif.com/components/espressif/brookesia_mcp_utils) |

#### 硬件抽象组件 (HAL)

| 组件 | 描述 | 版本 |
| --- | --- | --- |
| [brookesia_hal_adaptor](https://components.espressif.com/components/espressif/brookesia_hal_adaptor) | HAL 板级适配组件，针对具体开发板提供音频与 LCD 设备的接口实现 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_hal_adaptor/badge.svg)](https://components.espressif.com/components/espressif/brookesia_hal_adaptor) |
| [brookesia_hal_boards](https://components.espressif.com/components/espressif/brookesia_hal_boards) | HAL 开发板配置组件，提供各开发板的外设拓扑、引脚与驱动参数 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_hal_boards/badge.svg)](https://components.espressif.com/components/espressif/brookesia_hal_boards) |
| [brookesia_hal_interface](https://components.espressif.com/components/espressif/brookesia_hal_interface) | HAL 基础组件，定义统一的设备/接口模型、生命周期管理及硬件接口（音频、显示、触摸、存储等）抽象 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_hal_interface/badge.svg)](https://components.espressif.com/components/espressif/brookesia_hal_interface) |

#### 服务组件 (Service)

| 组件 | 描述 | 版本 |
| --- | --- | --- |
| [brookesia_service_audio](https://components.espressif.com/components/espressif/brookesia_service_audio) | 音频服务，提供音频播放、编解码、音量控制及播放状态管理 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_audio/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_audio) |
| [brookesia_service_custom](https://components.espressif.com/components/espressif/brookesia_service_custom) | 自定义服务扩展模板，用于基于 Brookesia 服务框架开发自定义服务 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_custom/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_custom) |
| [brookesia_service_helper](https://components.espressif.com/components/espressif/brookesia_service_helper) | 服务助手库，提供类型安全的定义、模式以及统一的服务调用接口（基于 CRTP 模式） | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_helper/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_helper) |
| [brookesia_service_manager](https://components.espressif.com/components/espressif/brookesia_service_manager) | 服务管理器，提供基于插件的服务生命周期管理、双通信模式（本地调用与 TCP RPC）、事件发布/订阅机制 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_manager/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_manager) |
| [brookesia_service_nvs](https://components.espressif.com/components/espressif/brookesia_service_nvs) | NVS 非易失性存储服务，提供基于命名空间的键值对存储管理 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_nvs/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_nvs) |
| [brookesia_service_sntp](https://components.espressif.com/components/espressif/brookesia_service_sntp) | SNTP 网络时间服务，提供 NTP 服务器管理、时区设置和自动时间同步 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_sntp/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_sntp) |
| [brookesia_service_video](https://components.espressif.com/components/espressif/brookesia_service_video) | 视频服务，支持本地采集编码（H.264、MJPEG、RGB/YUV）及压缩流解码到像素格式 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_video/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_video) |
| [brookesia_service_wifi](https://components.espressif.com/components/espressif/brookesia_service_wifi) | Wi-Fi 服务，提供状态机管理、AP 扫描、连接管理和 SoftAP 功能 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_wifi/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_wifi) |

#### AI 智能体组件 (Agent)

| 组件 | 描述 | 版本 |
| --- | --- | --- |
| [brookesia_agent_coze](https://components.espressif.com/components/espressif/brookesia_agent_coze) | Coze 智能体适配，提供 Coze API 集成、WebSocket 通信及智能体管理 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_agent_coze/badge.svg)](https://components.espressif.com/components/espressif/brookesia_agent_coze) |
| [brookesia_agent_helper](https://components.espressif.com/components/espressif/brookesia_agent_helper) | 智能体助手库，提供类型安全的定义、模式及统一的智能体调用接口 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_agent_helper/badge.svg)](https://components.espressif.com/components/espressif/brookesia_agent_helper) |
| [brookesia_agent_manager](https://components.espressif.com/components/espressif/brookesia_agent_manager) | 智能体管理器，提供 AI 智能体生命周期管理、状态机控制及统一操作接口 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_agent_manager/badge.svg)](https://components.espressif.com/components/espressif/brookesia_agent_manager) |
| [brookesia_agent_openai](https://components.espressif.com/components/espressif/brookesia_agent_openai) | OpenAI 智能体适配，提供 OpenAI API 集成、端到端通信及智能体管理 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_agent_openai/badge.svg)](https://components.espressif.com/components/espressif/brookesia_agent_openai) |
| [brookesia_agent_xiaozhi](https://components.espressif.com/components/espressif/brookesia_agent_xiaozhi) | 小智智能体适配，提供小智 API 集成 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_agent_xiaozhi/badge.svg)](https://components.espressif.com/components/espressif/brookesia_agent_xiaozhi) |

#### AI 表达组件 (Expression)

| 组件 | 描述 | 版本 |
| --- | --- | --- |
| [brookesia_expression_emote](https://components.espressif.com/components/espressif/brookesia_expression_emote) | AI 表情集与动画管理，提供表情集、动画集和事件消息集，为拟人化交互提供丰富的视觉反馈 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_expression_emote/badge.svg)](https://components.espressif.com/components/espressif/brookesia_expression_emote) |

</center>

### 从 ESP-Brookesia 仓库获取代码

如果您希望为 ESP-Brookesia 贡献代码，或基于仓库中的示例进行二次开发，可通过以下指令获取 `master` 分支代码：

```bash
git clone --recursive https://github.com/espressif/esp-brookesia
```

### 构建和烧录示例

ESP-Brookesia 提供了多个示例工程，位于 `examples/` 目录下。以下是通用构建步骤：

1. 确保已完成 [ESP-IDF 开发环境搭建](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/index.html)（要求 ESP-IDF >= v5.5）；

2. 选择目标芯片或开发板：

   - **仅依赖芯片外设**（如 [examples/service/wifi](./examples/service/wifi)）：

     ```bash
     idf.py set-target <target>
     ```

   - **依赖特定开发板外设**（如 [examples/service/console](./examples/service/console)）：

     ```bash
     idf.py gen-bmgr-config -b <board>
     idf.py set-target <target>
     ```

3. 配置工程（可选）：

   ```bash
   idf.py menuconfig
   ```

4. 编译并烧录到开发板：

   ```bash
   idf.py build
   idf.py -p <PORT> flash
   ```

5. 通过串口监视输出：

   ```bash
   idf.py -p <PORT> monitor
   ```

更多示例请见 [examples](./examples) 目录，具体使用方法请参考各示例目录下的 README 文件。

## 其它参考资源

- 最新版文档：https://docs.espressif.com/projects/esp-brookesia/zh_CN ，由本仓库 [docs](./docs) 目录构建；
- ESP-IDF 编程指南：https://docs.espressif.com/projects/esp-idf/zh_CN ；
- 可在 [ESP Component Registry](https://components.espressif.com/) 中找到 ESP-Brookesia 的各组件；
- 可前往 [esp32.com](https://www.esp32.com/) 论坛提问，挖掘社区资源；
- 如果您在使用中发现了错误或者需要新的功能，请先查看 [GitHub Issues](https://github.com/espressif/esp-brookesia/issues)，确保该问题不会被重复提交；
