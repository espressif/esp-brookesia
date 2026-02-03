# ESP-Brookesia Service Helper

* [中文版本](./README_CN.md)

## Overview

`brookesia_service_helper` is an ESP-Brookesia service development helper library based on C++20 Concepts and CRTP (Curiously Recurring Template Pattern) that provides type-safe definitions, schemas, and unified calling interfaces for service developers and users to build and use services that comply with the ESP-Brookesia service framework specifications.

This library mainly provides:

- **Type-Safe Definitions**: Provides strongly-typed enums (`FunctionId`, `EventId`) and struct type definitions to ensure compile-time type checking
- **Schema Definitions**: Provides standardized function schemas (`FunctionSchema`) and event schemas (`EventSchema`), including complete metadata such as function names, parameter types, parameter descriptions, return value types, etc.
- **Unified Calling Interfaces**: Provides type-safe synchronous and asynchronous function calling interfaces (`call_function_sync()`, `call_function_async()`), automatically handling type conversion and error handling
- **Event Subscription Interface**: Provides type-safe event subscription interface (`subscribe_event()`), supporting type-safe event handling callbacks
- **Type-Safe Return Values**: Uses `std::expected<T, std::string>` to provide type-safe return values, automatically handling success and error cases

## Table of Contents

- [ESP-Brookesia Service Helper](#esp-brookesia-service-helper)
  - [Overview](#overview)
  - [Table of Contents](#table-of-contents)
  - [Features](#features)
    - [Helper Classes](#helper-classes)
    - [Type Safety](#type-safety)
    - [Standardized Interfaces](#standardized-interfaces)
    - [Advanced Features](#advanced-features)
  - [Adding to Project](#adding-to-project)

## Features

### Helper Classes

`brookesia_service_helper` currently provides the following helper classes:

| Helper Class | Header File | Description |
|--------------|-------------|-------------|
| Audio | [audio.hpp](./include/brookesia/service_helper/audio.hpp) | Audio service helper class, providing function definitions for audio playback, codec, volume control, AFE events, etc. |
| NVS | [nvs.hpp](./include/brookesia/service_helper/nvs.hpp) | NVS service helper class, providing function definitions for key-value storage, data management, etc. |
| SNTP | [sntp.hpp](./include/brookesia/service_helper/sntp.hpp) | SNTP service helper class, providing function definitions for time synchronization, timezone settings, etc. |
| Wifi | [wifi.hpp](./include/brookesia/service_helper/wifi.hpp) | WiFi service helper class, providing function definitions for WiFi connection, scanning, state management, etc. |
| Emote | [emote.hpp](./include/brookesia/service_helper/expression/emote.hpp) | Emote service helper class, providing function definitions for emote display, animation control, QR code display, etc. |

### Type Safety

Based on C++20 Concepts and CRTP (Curiously Recurring Template Pattern), provides compile-time type safety guarantees:

- **Strongly-Typed Enums**: Uses enum types such as `FunctionId`, `EventId` instead of strings to avoid runtime errors
- **Compile-Time Type Checking**: Ensures type correctness through `static_assert` and Concepts, catching type errors at compile time
- **Type Deduction**: Template functions automatically deduce return types, supporting `std::expected<T, std::string>` type-safe return values
- **IDE Intelligent Hints**: Strongly-typed definitions provide complete IDE auto-completion and type hints
- **Code Maintainability**: Unified type definitions facilitate refactoring and maintenance, with type changes detected at compile time

### Standardized Interfaces

Through the `Base` base class and standardized Schema definitions, provides unified interface specifications:

- **Function Schema**: Each function has a complete `FunctionSchema` definition, including function name, parameter types, parameter descriptions, return value types, etc.
- **Event Schema**: Each event has a complete `EventSchema` definition, including event name, event item types, and descriptions
- **Synchronous/Asynchronous Calls**: Provides unified calling interfaces `call_function_sync()` and `call_function_async()`
- **Event Subscription**: Provides unified event subscription interface `subscribe_event()`, supporting type-safe event handling
- **Interface Consistency**: All helper classes inherit from `Base`, following the same interface specifications to ensure consistent usage
- **Runtime Validation**: Automatically validates Schema when calling functions and events, ensuring correct parameter types and counts

### Advanced Features

- **`ConvertibleToFunctionValue` Concept**: Provides type-safe parameter conversion, ensuring passed parameters can be correctly converted to the types required by functions
- **`Timeout` Tag Type**: Supports specifying timeout in function calls, format: `call_function_sync<FunctionId>(..., Timeout{5000})`
- **Event Callback Parameter Detection**: Provides Type Traits for detecting event callback function parameter types, ensuring correct callback signatures

## Adding to Project

`brookesia_service_helper` has been uploaded to the [Espressif Component Registry](https://components.espressif.com/). You can add it to your project in the following ways:

1. **Using Command Line**

   Run the following command in your project directory:

   ```bash
   idf.py add-dependency "espressif/brookesia_service_helper"
   ```

2. **Modify Configuration File**

   Create or modify the *idf_component.yml* file in your project directory:

   ```yaml
   dependencies:
     espressif/brookesia_service_helper: "*"
   ```

For detailed instructions, please refer to [Espressif Documentation - IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).
