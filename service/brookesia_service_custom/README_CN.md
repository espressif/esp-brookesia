# ESP-Brookesia Custom Service

* [English Version](./README.md)

## 概述

`brookesia_service_custom` 为 ESP-Brookesia 生态系统提供即用的 **CustomService**，支持用户自定义 function 和 event 的注册与调用，无需创建独立的服务组件。

**典型使用场景：**

- **轻量级功能**：对于无需封装为完整 Brookesia 组件的功能（如 LED 控制、PWM、GPIO 翻转、简单传感器等），可通过 CustomService 的接口进行封装和调用。
- **快速原型**：将应用逻辑快速暴露为可调用的 function 或 event，支持本地调用或远程 RPC 访问。
- **可扩展性**：在不修改服务框架的前提下，为应用添加自定义能力。

**主要特性：**

- **动态注册**：运行时通过 `register_function()` 和 `register_event()` 注册 function 和 event
- **固定 Handler 签名**：Handler 接收 `FunctionParameterMap`，返回 `FunctionResult`；支持 lambda、`std::function`、自由函数、仿函数、`std::bind`
- **事件发布/订阅**：完整的事件生命周期：注册、发布、订阅、注销
- **ServiceManager 集成**：与 ServiceManager 配合，支持本地调用和远程 RPC
- **可选 Worker**：可配置任务调度器，实现线程安全执行

## 目录

- [ESP-Brookesia Custom Service](#esp-brookesia-custom-service)
  - [概述](#概述)
  - [目录](#目录)
  - [Handler 签名](#handler-签名)
  - [重要说明](#重要说明)
  - [开发环境要求](#开发环境要求)
  - [添加到工程](#添加到工程)
  - [快速开始](#快速开始)

## Handler 签名

Function 的 handler 具有固定签名：

```cpp
FunctionResult(const FunctionParameterMap &args)
```

- **输入**：`args` 为参数名到 `FunctionValue` 的映射（支持 `bool`、`double`、`std::string`、`boost::json::object`、`boost::json::array`）
- **输出**：`FunctionResult`，包含 `success`、可选 `data`、可选 `error_message`

可使用 lambda、`std::function`、自由函数、仿函数或 `std::bind` 作为 handler，只要签名一致即可。

注册 function 或 event 时，CustomService 会在内部保存一份 schema 和 handler 的副本。注册完成后，再修改原始 schema 对象或 handler 变量，不会影响已经注册好的 function 或 event。

## 重要说明

> [!WARNING]
> **变量生命周期**：Handler 常为 lambda 表达式。请确保所捕获的变量在 function 的整个生命周期内保持有效。避免捕获可能在 function 注销前就已析构的栈上对象。若需跨作用域使用，建议使用值捕获。

> [!NOTE]
> **复制 Handler 对象并不会延长被引用对象的生命周期**：CustomService 会保存 handler 对象本身的副本，但如果 handler 通过引用方式捕获了外部变量，那么这些被引用对象仍然需要由用户保证其生命周期。也就是说，注册会让 handler 对象持续存在，但不会自动让被引用的数据持续存在。

> [!NOTE]
> **保持 Handler 简洁**：CustomService 适用于轻量级操作。不建议在 handler 中执行过于复杂或耗时的逻辑。对于重计算或阻塞 I/O，建议实现独立的服务组件并配合任务调度。

## 开发环境要求

使用本库前，请确保已安装以下 SDK 开发环境：

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> SDK 的安装方法请参阅 [ESP-IDF 编程指南 - 安装](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

## 添加到工程

`brookesia_service_custom` 依赖 `brookesia_service_manager`。可通过以下方式添加到工程：

1. **使用命令行**

   在工程目录下运行以下命令：

   ```bash
   idf.py add-dependency "espressif/brookesia_service_custom"
   ```

2. **修改配置文件**

   在工程目录下创建或修改 *idf_component.yml* 文件：

   ```yaml
   dependencies:
     espressif/brookesia_service_custom: "*"
   ```

详细说明请参阅 [Espressif 文档 - IDF 组件管理器](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/tools/idf-component-manager.html)。

## 快速开始

```cpp
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_custom/service_custom.hpp"

using namespace esp_brookesia::service;

void app_main()
{
    auto &service_manager = ServiceManager::get_instance();
    auto &custom_service = CustomService::get_instance();

    // 1. 启动服务管理器
    if (!service_manager.start()) {
        BROOKESIA_LOGE("Failed to start service manager");
        return;
    }

    // 2. 绑定 CustomService（启动服务）
    auto binding = service_manager.bind(CustomServiceName);
    if (!binding.is_valid()) {
        BROOKESIA_LOGE("Failed to bind CustomService");
        return;
    }

    // 3. 注册 function（handler 接收 FunctionParameterMap，返回 FunctionResult）
    custom_service.register_function(
        FunctionSchema{
            .name = "SetLED",
            .description = "设置 LED 状态",
            .parameters = {
                {.name = "On", .type = FunctionValueType::Boolean},
            },
        },
        [](const FunctionParameterMap &args) -> FunctionResult {
            bool on = std::get<bool>(args.at("On"));
            // 在此设置 LED 状态（如 gpio_set_level(...)）
            return FunctionResult{.success = true};
        }
    );

    // 4. 调用 function（参数以 boost::json::object 传入）
    auto result = custom_service.call_function_sync(
        "SetLED",
        boost::json::object{{"On", true}},
        1000
    );

    if (result.success) {
        BROOKESIA_LOGI("LED 设置成功");
    }

    // 5. 使用完毕后解绑
    binding.release();
}
```
