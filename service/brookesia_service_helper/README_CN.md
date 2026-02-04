# ESP-Brookesia Service Helper

* [English Version](./README.md)

## 概述

`brookesia_service_helper` 是 ESP-Brookesia 服务开发辅助库，基于 C++20 Concepts 和 CRTP（Curiously Recurring Template Pattern）模式，为服务开发者和使用者提供类型安全的定义、Schema 和统一调用接口，用于构建和使用符合 ESP-Brookesia 服务框架规范的服务。

本库主要提供：

- **类型安全定义**：提供强类型枚举（`FunctionId`、`EventId`）和结构体类型定义，确保编译时类型检查
- **Schema 定义**：提供标准化的函数 Schema（`FunctionSchema`）和事件 Schema（`EventSchema`），包括函数名、参数类型、参数描述、返回值类型等完整元数据
- **统一调用接口**：提供类型安全的同步和异步函数调用接口（`call_function_sync()`、`call_function_async()`），自动处理类型转换和错误处理
- **事件订阅接口**：提供类型安全的事件订阅接口（`subscribe_event()`），支持类型安全的事件处理回调
- **类型安全返回值**：使用 `std::expected<T, std::string>` 提供类型安全的返回值，自动处理成功和错误情况

## 目录

- [ESP-Brookesia Service Helper](#esp-brookesia-service-helper)
  - [概述](#概述)
  - [目录](#目录)
  - [功能特性](#功能特性)
    - [辅助类](#辅助类)
    - [类型安全](#类型安全)
    - [标准化接口](#标准化接口)
    - [高级特性](#高级特性)
  - [添加到工程](#添加到工程)

## 功能特性

### 辅助类

`brookesia_service_helper` 目前提供以下辅助类：

| 辅助类 | 头文件 | 说明 |
|--------|--------|------|
| Audio | [audio.hpp](./include/brookesia/service_helper/audio.hpp) | 音频服务辅助类，提供音频播放、编解码、音量控制、AFE 事件等功能定义 |
| NVS | [nvs.hpp](./include/brookesia/service_helper/nvs.hpp) | NVS 服务辅助类，提供键值对存储、数据管理等功能定义 |
| SNTP | [sntp.hpp](./include/brookesia/service_helper/sntp.hpp) | SNTP 服务辅助类，提供时间同步、时区设置等功能定义 |
| Wifi | [wifi.hpp](./include/brookesia/service_helper/wifi.hpp) | WiFi 服务辅助类，提供 WiFi 连接、扫描、状态管理等功能定义 |
| Emote | [emote.hpp](./include/brookesia/service_helper/expression/emote.hpp) | 表情服务辅助类，提供表情显示、动画控制、二维码显示等功能定义 |

### 类型安全

基于 C++20 Concepts 和 CRTP（Curiously Recurring Template Pattern）模式，提供编译时类型安全保障：

- **强类型枚举**：使用 `FunctionId`、`EventId` 等枚举类型替代字符串，避免运行时错误
- **编译时类型检查**：通过 `static_assert` 和 Concepts 确保类型正确性，在编译期捕获类型错误
- **类型推导**：模板函数自动推导返回类型，支持 `std::expected<T, std::string>` 类型安全返回值
- **IDE 智能提示**：强类型定义提供完整的 IDE 自动补全和类型提示
- **代码可维护性**：统一的类型定义便于重构和维护，类型变更会在编译期被检测

### 标准化接口

通过 `Base` 基类和标准化 Schema 定义，提供统一的接口规范：

- **函数 Schema**：每个函数都有完整的 `FunctionSchema` 定义，包括函数名、参数类型、参数描述、返回值类型等
- **事件 Schema**：每个事件都有完整的 `EventSchema` 定义，包括事件名、事件项类型和描述
- **同步/异步调用**：提供 `call_function_sync()` 和 `call_function_async()` 统一调用接口
- **事件订阅**：提供 `subscribe_event()` 统一事件订阅接口，支持类型安全的事件处理
- **接口一致性**：所有辅助类继承自 `Base`，遵循相同的接口规范，确保使用方式一致
- **运行时验证**：函数和事件调用时自动验证 Schema，确保参数类型和数量正确

### 高级特性

- **`ConvertibleToFunctionValue` Concept**：提供类型安全的参数转换，确保传入参数可正确转换为函数所需类型
- **`Timeout` Tag 类型**：支持在函数调用中指定超时时间，格式为 `call_function_sync<FunctionId>(..., Timeout{5000})`
- **事件回调参数检测**：提供 Type Traits 用于检测事件回调函数的参数类型，确保回调签名正确

## 添加到工程

`brookesia_service_helper` 已上传到 [Espressif 组件库](https://components.espressif.com/)，您可以通过以下方式将其添加到工程中：

1. **使用命令行**

   在工程目录下运行以下命令：

   ```bash
   idf.py add-dependency "espressif/brookesia_service_helper"
   ```

2. **修改配置文件**

   在工程目录下创建或修改 *idf_component.yml* 文件：

   ```yaml
   dependencies:
     espressif/brookesia_service_helper: "*"
   ```

详细说明请参阅 [Espressif 文档 - IDF 组件管理器](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/tools/idf-component-manager.html)。
