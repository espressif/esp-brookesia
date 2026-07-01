<div align="center">
    <img src="docs/_static/brookesia_logo.png" alt="logo" width="800">
</div>

# ESP-Brookesia

* [English Version](./README.md)

## 概述

ESP-Brookesia 是面向 AIoT 与 HMI 产品开发的全栈式开发平台。它以 ESP-IDF 组件形式组织硬件抽象、服务框架、运行时、GUI、系统框架、应用组件和 AI 能力，使产品可以按需组合并复用平台能力。

> [!NOTE]
> [Brookesia](https://en.wikipedia.org/wiki/Brookesia) 是一种变色龙属，具有体型小、适应性强的特点。ESP-Brookesia 借用这一意象，强调框架能够适配不同硬件形态、不同交互界面和不同产品系统。

## 主架构分层

<div align="center">
    <img src="docs/_static/architecture_diagram_cn.svg" alt="ESP-Brookesia 整体架构" width="900">
</div>
<br>

- **硬件抽象**：屏蔽平台和硬件差异，统一设备访问，支持 ESP 设备与 PC 仿真。
- **服务框架**：统一服务注册、函数调用、事件发布订阅与 MCP / Agent 能力连接。
- **运行时与 GUI**：支撑多运行时应用与 JSON UI 驱动的 GUI 模型。
- **系统框架**：产品级系统框架，承接 GUI、Runtime、服务与应用生命周期。
- **应用生态**：第三方应用供给、上传发布、设备端分发与安装。

## 主架构特性

- **硬件解耦**：HAL 与 PC 仿真降低硬件差异对上层应用和系统的影响。
- **应用开发效率**：Service 化、多运行时和声明式 GUI 提升应用开发与验证效率。
- **产品化与生态化**：系统与应用框架结合应用市场机制，支撑规模化产品交付。
- **AI 辅助开发闭环**：AI Workflow 与 PC Validation 支持开发验证闭环，开发者审查、调整并补充后生成可发布 App。
- **发布与分发链路**：App 可进入 Upload & Publish、App Store 和 Device Runtime 链路，完成发布、发现、下载、安装和运行。

## ESP-Brookesia 版本说明

ESP-Brookesia 自 `v0.7` 版本起采用组件化管理，建议项目通过组件注册表获取所需组件，约定如下：

1. 各组件独立迭代，但均具有相同的 `major.minor` 版本号，并且依赖相同的 ESP-IDF 版本。
2. `release` 分支维护历史稳定版本，仅修复 bug 和安全问题。
3. `master` 分支支持持续集成新特性，可能包含未稳定的新功能。

| ESP-Brookesia | 依赖的 ESP-IDF | 主要变更 | 支持状态 |
| --- | --- | --- | --- |
| master (v0.8) | ESP32-S31: 6.2<br>其他： >= 6.0, <= 6.2 | 组件化发布扩展到 GUI、Runtime、System、App 与 AI 能力；新增 PC 仿真、JSON UI、多运行时和标准 System Super 主线 | 新功能开发分支 |
| release/v0.7 | >= 5.5, <= 6.0 | 支持组件管理器 | 稳定版本，仅修复 bug 和安全问题 |
| release/v0.6 | >= 5.3, <= 5.5 | 预览支持系统框架，提供 ESP-VoCat 固件工程 | 停止维护 |

## 文档中心

- 中文：https://docs.espressif.com/projects/esp-brookesia/zh_CN
- English: https://docs.espressif.com/projects/esp-brookesia/en

快速参考：

- [快速开始](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html)
- [工具组件](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/utils/index.html)
- [硬件组件](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/hal/index.html)
- [服务组件](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/service/index.html)
- [GUI 组件](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/gui/index.html)
- [运行时组件](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/runtime/index.html)
- [系统组件](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/system/index.html)
- [应用组件](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/app/index.html)

## 组件列表

### 工具组件 (Utils)

| 组件 | 描述 | 版本 |
| --- | --- | --- |
| [brookesia_lib_utils](https://components.espressif.com/components/espressif/brookesia_lib_utils) | 核心工具库，提供任务调度器、线程配置、性能分析器、日志系统、状态机、插件系统等基础能力 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_lib_utils/badge.svg)](https://components.espressif.com/components/espressif/brookesia_lib_utils) |
| [brookesia_mcp_utils](https://components.espressif.com/components/espressif/brookesia_mcp_utils) | MCP 工具桥接库，将 Brookesia 服务函数或自定义回调注册为 MCP 工具，供大语言模型调用 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_mcp_utils/badge.svg)](https://components.espressif.com/components/espressif/brookesia_mcp_utils) |

### 硬件组件 (HAL)

| 组件 | 描述 | 版本 |
| --- | --- | --- |
| [brookesia_hal_interface](https://components.espressif.com/components/espressif/brookesia_hal_interface) | HAL 基础组件，定义统一设备/接口模型、生命周期管理及硬件接口抽象 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_hal_interface/badge.svg)](https://components.espressif.com/components/espressif/brookesia_hal_interface) |
| [brookesia_hal_adaptor](https://components.espressif.com/components/espressif/brookesia_hal_adaptor) | ESP 设备板级适配组件，基于 `esp_board_manager` 初始化真实外设并注册 HAL 接口实现 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_hal_adaptor/badge.svg)](https://components.espressif.com/components/espressif/brookesia_hal_adaptor) |
| [brookesia_hal_boards](https://components.espressif.com/components/espressif/brookesia_hal_boards) | ESP 设备板级配置组件，提供开发板外设拓扑、引脚、驱动参数和 Kconfig 默认值 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_hal_boards/badge.svg)](https://components.espressif.com/components/espressif/brookesia_hal_boards) |

> [!NOTE]
> PC 仿真支持由仓库内的 `hal/brookesia_hal_linux` 和 `hal/brookesia_hal_wasm` 提供，用于 Linux host、CI 和 WebAssembly/Web 演示场景。

### 服务组件 (Service)

#### 服务框架

| 组件 | 描述 | 版本 |
| --- | --- | --- |
| [brookesia_service_manager](https://components.espressif.com/components/espressif/brookesia_service_manager) | 服务管理器，提供插件生命周期、函数路由、事件发布订阅、本地调用和 TCP RPC | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_manager/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_manager) |
| [brookesia_service_helper](https://components.espressif.com/components/espressif/brookesia_service_helper) | 服务助手库，提供类型安全 schema、辅助调用和统一服务访问接口 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_helper/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_helper) |
| [brookesia_service_custom](https://components.espressif.com/components/espressif/brookesia_service_custom) | 自定义服务扩展模板，用于基于 Brookesia 服务框架开发产品自定义能力 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_custom/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_custom) |

#### 网络服务

| 组件 | 描述 | 版本 |
| --- | --- | --- |
| [brookesia_service_http](https://components.espressif.com/components/espressif/brookesia_service_http) | HTTP 服务，提供 HTTP/HTTPS 客户端请求、资源限制和请求生命周期事件 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_http/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_http) |
| [brookesia_service_sntp](https://components.espressif.com/components/espressif/brookesia_service_sntp) | SNTP 服务，提供 NTP 服务器管理、时区设置和自动时间同步 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_sntp/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_sntp) |
| [brookesia_service_wifi](https://components.espressif.com/components/espressif/brookesia_service_wifi) | Wi-Fi 服务，提供状态机管理、AP 扫描、连接管理和 SoftAP 功能 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_wifi/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_wifi) |

#### 媒体服务

| 组件 | 描述 | 版本 |
| --- | --- | --- |
| [brookesia_service_audio](https://components.espressif.com/components/espressif/brookesia_service_audio) | 音频服务，提供音频播放、编解码、音量控制及播放状态管理 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_audio/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_audio) |
| [brookesia_service_video](https://components.espressif.com/components/espressif/brookesia_service_video) | 视频服务，支持本地采集编码及压缩流解码到像素格式 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_video/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_video) |
| [brookesia_service_display](https://components.espressif.com/components/espressif/brookesia_service_display) | 显示服务，封装显示输入、手势和显示相关系统能力 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_display/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_display) |

#### 系统服务

| 组件 | 描述 | 版本 |
| --- | --- | --- |
| [brookesia_service_storage](https://components.espressif.com/components/espressif/brookesia_service_storage) | Storage 服务，提供基于命名空间的键值对存储管理 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_storage/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_storage) |
| [brookesia_service_device](https://components.espressif.com/components/espressif/brookesia_service_device) | 设备服务，提供应用层对设备控制、状态查询和协议接口的访问 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_device/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_device) |

#### 智能体与表达服务

| 组件 | 描述 | 版本 |
| --- | --- | --- |
| [brookesia_agent_manager](https://components.espressif.com/components/espressif/brookesia_agent_manager) | 智能体管理器，提供 AI 智能体生命周期、状态机和统一操作接口 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_agent_manager/badge.svg)](https://components.espressif.com/components/espressif/brookesia_agent_manager) |
| [brookesia_agent_coze](https://components.espressif.com/components/espressif/brookesia_agent_coze) | Coze 智能体适配，提供 Coze API 集成、WebSocket 通信及智能体管理 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_agent_coze/badge.svg)](https://components.espressif.com/components/espressif/brookesia_agent_coze) |
| [brookesia_agent_openai](https://components.espressif.com/components/espressif/brookesia_agent_openai) | OpenAI 智能体适配，提供 OpenAI API 集成和智能体管理 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_agent_openai/badge.svg)](https://components.espressif.com/components/espressif/brookesia_agent_openai) |
| [brookesia_agent_xiaozhi](https://components.espressif.com/components/espressif/brookesia_agent_xiaozhi) | 小智智能体适配，提供小智 API 集成 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_agent_xiaozhi/badge.svg)](https://components.espressif.com/components/espressif/brookesia_agent_xiaozhi) |
| [brookesia_expression_emote](https://components.espressif.com/components/espressif/brookesia_expression_emote) | AI 表情集与动画管理，为拟人化交互提供视觉反馈 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_expression_emote/badge.svg)](https://components.espressif.com/components/espressif/brookesia_expression_emote) |

#### 模拟器服务

| 组件 | 描述 | 版本 |
| --- | --- | --- |
| [brookesia_emulation_nes](https://components.espressif.com/components/espressif/brookesia_emulation_nes) | NES 模拟器服务，集成显示、音频、存储和手柄控制 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_emulation_nes/badge.svg)](https://components.espressif.com/components/espressif/brookesia_emulation_nes) |

### 图形库组件（GUI）

| 组件 | 描述 | 版本 |
| --- | --- | --- |
| [brookesia_gui_interface](https://components.espressif.com/components/espressif/brookesia_gui_interface) | GUI 接口层，定义 JSON UI 文档模型、运行时资源、动作绑定和后端契约 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_gui_interface/badge.svg)](https://components.espressif.com/components/espressif/brookesia_gui_interface) |
| [brookesia_gui_lvgl](https://components.espressif.com/components/espressif/brookesia_gui_lvgl) | LVGL 后端，将解析后的 GUI 文档映射为 LVGL 对象并处理事件、动画和图片资源 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_gui_lvgl/badge.svg)](https://components.espressif.com/components/espressif/brookesia_gui_lvgl) |

### 运行时组件（Runtime）

| 组件 | 描述 | 版本 |
| --- | --- | --- |
| [brookesia_runtime_manager](https://components.espressif.com/components/espressif/brookesia_runtime_manager) | 运行时管理器，负责动态应用包、运行时后端注册和应用上下文管理 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_runtime_manager/badge.svg)](https://components.espressif.com/components/espressif/brookesia_runtime_manager) |
| [brookesia_runtime_elf](https://components.espressif.com/components/espressif/brookesia_runtime_elf) | ELF 运行时后端 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_runtime_elf/badge.svg)](https://components.espressif.com/components/espressif/brookesia_runtime_elf) |
| [brookesia_runtime_js](https://components.espressif.com/components/espressif/brookesia_runtime_js) | JavaScript 运行时后端 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_runtime_js/badge.svg)](https://components.espressif.com/components/espressif/brookesia_runtime_js) |
| [brookesia_runtime_lua](https://components.espressif.com/components/espressif/brookesia_runtime_lua) | Lua 运行时后端 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_runtime_lua/badge.svg)](https://components.espressif.com/components/espressif/brookesia_runtime_lua) |
| [brookesia_runtime_wasm](https://components.espressif.com/components/espressif/brookesia_runtime_wasm) | WebAssembly 运行时后端 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_runtime_wasm/badge.svg)](https://components.espressif.com/components/espressif/brookesia_runtime_wasm) |

### 系统与应用组件（System & App）

| 组件 | 描述 | 版本 |
| --- | --- | --- |
| [brookesia_system_core](https://components.espressif.com/components/espressif/brookesia_system_core) | 系统核心框架，统一编排应用生命周期、GUI、Runtime、服务、存储和权限边界 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_system_core/badge.svg)](https://components.espressif.com/components/espressif/brookesia_system_core) |
| [brookesia_system_super](https://components.espressif.com/components/espressif/brookesia_system_super) | 面向矩形触摸设备的标准系统，提供 Shell、launcher、overlay、状态栏和内置资源流程 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_system_super/badge.svg)](https://components.espressif.com/components/espressif/brookesia_system_super) |
| [brookesia_app_files](https://components.espressif.com/components/espressif/brookesia_app_files) | 文件应用，提供设备端文件浏览入口 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_app_files/badge.svg)](https://components.espressif.com/components/espressif/brookesia_app_files) |
| [brookesia_app_settings](https://components.espressif.com/components/espressif/brookesia_app_settings) | 设置应用，提供设备与系统配置入口 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_app_settings/badge.svg)](https://components.espressif.com/components/espressif/brookesia_app_settings) |
| [brookesia_app_store](https://components.espressif.com/components/espressif/brookesia_app_store) | 应用商店组件，提供本地和远端应用包入口 | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_app_store/badge.svg)](https://components.espressif.com/components/espressif/brookesia_app_store) |

## 获取代码

如果您希望为 ESP-Brookesia 贡献代码，或基于仓库中的示例进行二次开发，可通过以下指令获取 `master` 分支代码：

```bash
git clone https://github.com/espressif/esp-brookesia
```

## 贡献说明

当前欢迎开发者进行以下几个方面的代码贡献：

- 修复任意 bug
- 向 [brookesia_hal_boards](./hal/brookesia_hal_boards) 组件添加新的开发板支持

对于新增组件的需求，我们更建议先在 [GitHub Issues](https://github.com/espressif/esp-brookesia/issues) 中发起讨论，便于大家一起确认使用场景、接口边界和整体规划；在需求达成一致后，再推进后续提交会更顺畅。

## 其它参考资源

- 最新版文档：https://docs.espressif.com/projects/esp-brookesia/zh_CN ，由本仓库 [docs](./docs) 目录构建；
- ESP-IDF 编程指南：https://docs.espressif.com/projects/esp-idf/zh_CN ；
- 可在 [ESP Component Registry](https://components.espressif.com/) 中找到 ESP-Brookesia 的各组件；
- 可前往 [esp32.com](https://www.esp32.com/) 论坛提问，挖掘社区资源；
- 如果您在使用中发现了错误或者需要新的功能，请先查看 [GitHub Issues](https://github.com/espressif/esp-brookesia/issues)，确保该问题不会被重复提交；
