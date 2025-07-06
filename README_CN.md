![logo](./docs/_static/logo.png)

# ESP-Brookesia

* [English Version](./README.md)

## 概述

ESP-Brookesia 是一个面向物联网设备的人机交互开发框架，旨在简化用户 UI 设计和应用程序开发的流程，它支持高效的开发工具与平台，加速客户 HMI 应用产品的开发与上市。

> [!NOTE]
> "[Brookesia](https://zh.wikipedia.org/wiki/%E8%AE%8A%E8%89%B2%E9%BE%8D%E5%B1%AC)" 是一种变色龙属的物种，擅长于伪装和适应环境，这与 ESP-Brookesia 的目标紧密相关。该框架旨在提供一种灵活、可扩展的 UI 解决方案，能够适应各种不同的设备、屏幕大小和应用需求，就像 Brookesia 变色龙那样，具有高度的适应性和灵活性。

ESP-Brookesia 的主要特性包括：

- 基于 C/C++ 开发，原生支持 ESP-IDF 开发体系，充分利用乐鑫开源组件生态。
- 提供丰富的功能组件，支持 AI 应用场景的开发。
- 采用 app 的应用管理方式，实现多个 app 的 UI 隔离与共存，使用户专注于各自 app 内的 UI 实现。

ESP-Brookesia 的功能框图如下，主要由 `HAL`、`Middle` 和 `Application` 三部分组成：

<div align="center">
    <img src="docs/_static/block_diagram.png" alt ="block_diagram" width="800">
</div>
<br>

- **HAL**：使用 ESP-IDF 提供的硬件抽象层，提供对底层硬件的访问和控制。
- **Middle**：作为连接应用程序与底层硬件的桥梁，通过 `Function Components` 向下对接硬件抽象层，同时通过 `System Services` 向上为应用程序提供标准化的接口，实现系统资源的解耦与隔离。
- **Application**：通过 `AI Framework` 提供 AI 应用场景支持，包括 `HMI`（单屏和双屏的拟人化交互设计）、`Agent`（兼容豆包、小智等主流 LLM 模型） 和 `Protocol`（MCP 协议实现 LLM 与系统服务统一通信）。通过 `System Framework` 提供各种面向产品（移动设备、音箱、机器人等）的系统和应用（设置、AI 助手、应用商店等）支持。
