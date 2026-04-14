.. _index-sec-00:

ESP-Brookesia 编程指南
========================================

:link_to_translation:`en:[English]`

.. image:: ../_static/brookesia_logo.png
   :alt: ESP-Brookesia Logo
   :width: 800px
   :align: center

|

.. list-table::
   :widths: 33 33 34

   * - |快速开始|_
     - |工具组件|_
     - |硬件抽象组件|_
   * - `快速开始`_
     - `工具组件`_
     - `硬件抽象组件`_
   * - |服务组件|_
     - |AI智能体组件|_
     - |AI表达组件|_
   * - `服务组件`_
     - `AI智能体组件`_
     - `AI表达组件`_

.. |快速开始| image:: ../_static/index/getting_started.png
.. _快速开始: getting_started.html
.. _index-nav-getting-started: getting_started.html

.. |工具组件| image:: ../_static/index/utils.png
.. _工具组件: utils/index.html
.. _index-nav-utils: utils/index.html

.. |硬件抽象组件| image:: ../_static/index/hal.png
.. _硬件抽象组件: hal/index.html
.. _index-nav-hal: hal/index.html

.. |服务组件| image:: ../_static/index/service.png
.. _服务组件: service/index.html
.. _index-nav-service: service/index.html

.. |AI智能体组件| image:: ../_static/index/agent.png
.. _AI智能体组件: agent/index.html
.. _index-nav-agent: agent/index.html

.. |AI表达组件| image:: ../_static/index/expression.png
.. _AI表达组件: expression/index.html
.. _index-nav-expression: expression/index.html

.. _index-sec-01:

.. rubric:: 概述

ESP-Brookesia 是一个面向 AIoT 设备的人机交互开发框架，旨在简化应用程序开发和 AI 能力集成的流程。它以 ESP-IDF 为基础，通过组件化架构为开发者提供从硬件抽象、系统服务到 AI 智能体的全栈支持，加速 HMI 与 AI 应用产品的开发与上市。

.. NOTE::
   "Brookesia" 是一种变色龙属的物种，擅长于伪装和适应环境，这与 ESP-Brookesia 的目标紧密相关。该框架旨在提供一种灵活、可扩展的解决方案，能够适应各种不同的硬件设备和应用需求，就像 Brookesia 变色龙那样，具有高度的适应性和灵活性。

ESP-Brookesia 的主要特性包括：

- **原生 ESP-IDF 支持**：基于 C/C++ 开发，深度集成 ESP-IDF 开发体系与 ESP Registry 组件注册表，充分利用乐鑫开源组件生态。
- **可扩展的硬件抽象**：定义统一的硬件接口（音频、显示、触摸、存储等），并提供板级适配层，便于快速移植到不同硬件平台。
- **丰富的系统服务**：提供 Wi-Fi 连接、音视频处理等开箱即用的系统级服务，采用 Manager + Helper 架构实现解耦与扩展，为 Agent CLI 提供支持。
- **多 LLM 后端集成**：内置对 OpenAI、Coze、小智等主流 AI 平台的适配，提供统一的智能体管理与生命周期控制。
- **MCP 协议支持**：通过 Function Calling / MCP 协议将设备服务能力暴露给大语言模型，实现 LLM 与系统服务的统一通信。
- **AI 表达能力**：支持表情集、动画集等可视化 AI 表达，为拟人化交互提供丰富的视觉反馈。

.. _index-sec-02:

.. rubric:: 功能架构

ESP-Brookesia 采用分层架构设计，自底向上由 **环境与依赖**、**服务与框架** 以及 **应用层** 三个层级组成，如下图所示：

.. image:: ../_static/framework_overview.svg
   :alt: ESP-Brookesia 框架总览
   :width: 800px
   :align: center

|

.. _index-sec-03:

.. rubric:: 环境与依赖

框架的运行基础。``ESP-IDF`` 开发环境提供编译工具链、实时操作系统及外设驱动等底层支撑；``ESP Registry`` 组件注册表统一管理框架各组件及其第三方依赖的分发与版本迭代。

.. _index-sec-04:

.. rubric:: 服务与框架

框架的核心层，向下对接环境与依赖，向上为应用程序和 AI 智能体提供标准化的服务接口，涵盖基础工具、硬件抽象、系统服务、AI 智能体及表达等模块。

- **Utils 工具集**：为上层模块提供通用基础能力。其中 ``General Utils`` 包含日志系统、错误检查、状态机、任务调度器、插件管理、内存/线程/时间分析器等核心工具； ``MCP Utils`` 作为 ESP-Brookesia 服务体系与 MCP 引擎之间的桥梁，将已注册的服务函数暴露为标准 MCP 工具，实现大语言模型对设备能力的调用。
- **HAL 硬件抽象**：定义统一的硬件访问接口并提供板级适配实现。其中 ``Interface`` 定义音频播放/录制、显示面板/触摸、状态 LED、存储文件系统等标准化硬件接口； ``Adaptor`` 针对具体开发板提供接口实现，完成硬件资源的初始化与映射。 ``Boards`` 提供板级 YAML 配置，描述各开发板的外设拓扑、引脚与驱动参数。
- **General Service 通用服务**：提供系统级基础服务，包括 ``Wi-Fi`` 连接管理、``Audio`` 音频采集与播放、``Video`` 视频编解码、``NVS`` 非易失性存储、``SNTP`` 网络时间同步，以及 ``Custom`` 自定义服务扩展机制。所有服务均基于 Manager + Helper 架构，支持本地调用与 RPC 远程通信。
- **AI Agent 智能体框架**：提供 AI 智能体的统一管理框架，内置对 ``Coze``、``OpenAI``、``小智`` 等主流 AI 平台的适配。通过 ``Function Calling / MCP`` 协议实现大语言模型与系统服务的双向通信，使 LLM 能够感知和调用设备的各项能力。
- **AI Expression 智能体表达**：提供 AI 交互场景下的可视化表达能力，包括 ``Emote`` 表情集与动画控制，为拟人化交互提供丰富的视觉反馈。
- **System 系统框架** *(规划中)*：面向不同产品形态（移动设备、音箱、机器人等）提供 GUI、系统管理与应用框架支持。
- **Runtime 运行时** *(规划中)*：提供 WebAssembly、Python、Lua 等脚本运行时支持，实现应用的动态加载与执行。

.. _index-sec-05:

.. rubric:: 应用层

基于上述各层构建的最终产品与工程：

- ``General Projects 通用工程``：面向产品的通用工程模板，集成框架各组件，可直接用于产品开发。
- ``System Apps 系统应用`` *(规划中)*：面向产品的系统级应用集合，包括设置、AI 助手、应用商店等，可按需裁剪和集成。

.. toctree::
   :hidden:

   快速开始 <getting_started>
   工具组件 <utils/index>
   硬件抽象组件 <hal/index>
   服务组件 <service/index>
   AI 智能体组件 <agent/index>
   AI 表达组件 <expression/index>
