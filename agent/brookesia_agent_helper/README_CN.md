# ESP-Brookesia Agent Helper

* [English Version](./README.md)

## 概述

`brookesia_agent_helper` 是 ESP-Brookesia 智能体开发辅助库，基于 C++20 Concepts 和 CRTP（Curiously Recurring Template Pattern）模式，为智能体开发者和使用者提供类型安全的定义、Schema 和统一调用接口。

本库主要提供：

- **类型安全定义**：提供强类型枚举（`FunctionId`、`EventId`）和结构体类型定义，确保编译时类型检查
- **Schema 定义**：提供标准化的函数 Schema（`FunctionSchema`）和事件 Schema（`EventSchema`），包括函数名、参数类型、参数描述等完整元数据
- **统一调用接口**：提供类型安全的同步和异步函数调用接口，自动处理类型转换和错误处理
- **事件订阅接口**：提供类型安全的事件订阅接口，支持类型安全的事件处理回调

## 功能特性

### 辅助类

`brookesia_agent_helper` 目前提供以下辅助类：

- [Manager](./include/brookesia/agent_helper/manager.hpp) - 智能体管理器辅助类，提供智能体生命周期管理、状态机控制、通用函数和事件定义
- [Coze](./include/brookesia/agent_helper/coze.hpp) - Coze 智能体辅助类，提供 Coze API 集成、授权信息、机器人配置等功能定义
- [OpenAI](./include/brookesia/agent_helper/openai.hpp) - OpenAI 智能体辅助类，提供 OpenAI API 集成、模型配置等功能定义
- [Xiaozhi](./include/brookesia/agent_helper/xiaozhi.hpp) - 小智智能体辅助类，提供小智平台集成、OTA 配置等功能定义

### 类型安全

基于 C++20 Concepts 和 CRTP 模式，提供编译时类型安全保障：

- **强类型枚举**：使用 `FunctionId`、`EventId` 等枚举类型替代字符串，避免运行时错误
- **编译时类型检查**：通过 `static_assert` 和 Concepts 确保类型正确性
- **类型推导**：模板函数自动推导返回类型，支持 `std::expected<T, std::string>` 类型安全返回值

### 标准化接口

通过继承 `brookesia_service_helper` 的 `Base` 基类，提供统一的接口规范：

- **函数 Schema**：每个函数都有完整的 `FunctionSchema` 定义
- **事件 Schema**：每个事件都有完整的 `EventSchema` 定义
- **同步/异步调用**：提供 `call_function_sync()` 和 `call_function_async()` 统一调用接口
- **事件订阅**：提供 `subscribe_event()` 统一事件订阅接口

## 添加到工程

`brookesia_agent_helper` 已上传到 [Espressif 组件库](https://components.espressif.com/)，您可以通过以下方式将其添加到工程中：

1. **使用命令行**

   在工程目录下运行以下命令：

   ```bash
   idf.py add-dependency "espressif/brookesia_agent_helper"
   ```

2. **修改配置文件**

   在工程目录下创建或修改 *idf_component.yml* 文件：

   ```yaml
   dependencies:
     espressif/brookesia_agent_helper: "*"
   ```
