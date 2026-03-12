# ESP-Brookesia Custom Service

* [中文版本](./README_CN.md)

## Overview

`brookesia_service_custom` provides a ready-to-use **CustomService** for the ESP-Brookesia ecosystem, enabling user-defined function and event registration and invocation without creating a dedicated service component.

**Typical Use Cases:**

- **Lightweight Features**: For small features that do not warrant a full Brookesia component (e.g., LED control, PWM, GPIO toggling, simple sensors), expose and call them through CustomService's interfaces.
- **Rapid Prototyping**: Quickly expose application logic as callable functions or events for local or remote (RPC) access.
- **Extensibility**: Add custom capabilities to your application without modifying the service framework.

**Key Features:**

- **Dynamic Registration**: Register functions and events at runtime via `register_function()` and `register_event()`
- **Fixed Handler Signature**: Handler receives `FunctionParameterMap` and returns `FunctionResult`; supports lambda, `std::function`, free function, functor, and `std::bind`
- **Event Publish/Subscribe**: Full event lifecycle: register, publish, subscribe, and unregister
- **ServiceManager Integration**: Works with ServiceManager for local calls and remote RPC
- **Optional Worker**: Configurable task scheduler for thread-safe execution

## Table of Contents

- [ESP-Brookesia Custom Service](#esp-brookesia-custom-service)
  - [Overview](#overview)
  - [Table of Contents](#table-of-contents)
  - [Handler Signature](#handler-signature)
  - [Important Notes](#important-notes)
  - [Development Environment Requirements](#development-environment-requirements)
  - [Adding to Project](#adding-to-project)
  - [Quick Start](#quick-start)

## Handler Signature

The function handler has a fixed signature:

```cpp
FunctionResult(const FunctionParameterMap &args)
```

- **Input**: `args` is a map of parameter name to `FunctionValue` (supports `bool`, `double`, `std::string`, `boost::json::object`, `boost::json::array`)
- **Output**: `FunctionResult` with `success`, optional `data`, and optional `error_message`

You can use lambda, `std::function`, free function, functor, or `std::bind` as the handler, as long as the signature matches.

When registering a function or event, CustomService stores its own internal copy of the schema and handler. After registration, changing the original schema object or handler variable will not affect the already registered function or event.

## Important Notes

> [!WARNING]
> **Variable Lifecycle**: Handlers are often lambda expressions. Ensure that any captured variables remain valid for the entire lifetime of the registered function. Avoid capturing stack-allocated objects that may go out of scope before the function is unregistered. Prefer value capture for values that must outlive the current scope.

> [!NOTE]
> **Copying the Handler Object Does Not Extend Captured References**: CustomService stores a copy of the handler object itself, but if the handler captures external variables by reference, those referenced objects still need to remain alive. In other words, registration makes the handler object persistent, but it does not automatically make referenced data persistent.

> [!NOTE]
> **Keep Handlers Simple**: CustomService is designed for lightweight operations. Avoid executing overly complex or long-running logic in handlers. For heavy computation or blocking I/O, consider implementing a dedicated service component with proper task scheduling.

## Development Environment Requirements

Before using this library, ensure you have installed the following SDK development environment:

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> For SDK installation instructions, please refer to [ESP-IDF Programming Guide - Installation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

## Adding to Project

`brookesia_service_custom` depends on `brookesia_service_manager`. Add it to your project as follows:

1. **Using Command Line**

   Run the following command in your project directory:

   ```bash
   idf.py add-dependency "espressif/brookesia_service_custom"
   ```

2. **Modify Configuration File**

   Create or modify the *idf_component.yml* file in your project directory:

   ```yaml
   dependencies:
     espressif/brookesia_service_custom: "*"
   ```

For detailed instructions, please refer to [Espressif Documentation - IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## Quick Start

```cpp
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_custom/service_custom.hpp"

using namespace esp_brookesia::service;

void app_main()
{
    auto &service_manager = ServiceManager::get_instance();
    auto &custom_service = CustomService::get_instance();

    // 1. Start service manager
    if (!service_manager.start()) {
        BROOKESIA_LOGE("Failed to start service manager");
        return;
    }

    // 2. Bind CustomService (starts the service)
    auto binding = service_manager.bind(CustomServiceName);
    if (!binding.is_valid()) {
        BROOKESIA_LOGE("Failed to bind CustomService");
        return;
    }

    // 3. Register a function (handler receives FunctionParameterMap, returns FunctionResult)
    custom_service.register_function(
        FunctionSchema{
            .name = "SetLED",
            .description = "Set LED state",
            .parameters = {
                {.name = "On", .type = FunctionValueType::Boolean},
            },
        },
        [](const FunctionParameterMap &args) -> FunctionResult {
            bool on = std::get<bool>(args.at("On"));
            // Set LED state here (e.g., gpio_set_level(...))
            return FunctionResult{.success = true};
        }
    );

    // 4. Call the function (pass parameters as boost::json::object)
    auto result = custom_service.call_function_sync(
        "SetLED",
        boost::json::object{{"On", true}},
        1000
    );

    if (result.success) {
        BROOKESIA_LOGI("LED set successfully");
    }

    // 5. Unbind when done
    binding.release();
}
```
