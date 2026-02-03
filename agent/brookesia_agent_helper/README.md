# ESP-Brookesia Agent Helper

* [中文版本](./README_CN.md)

## Overview

`brookesia_agent_helper` is an ESP-Brookesia agent development helper library based on C++20 Concepts and CRTP (Curiously Recurring Template Pattern), providing type-safe definitions, schemas, and unified calling interfaces for agent developers and users.

This library mainly provides:

- **Type-safe definitions**: Provides strongly-typed enumerations (`FunctionId`, `EventId`) and struct type definitions to ensure compile-time type checking
- **Schema definitions**: Provides standardized function schemas (`FunctionSchema`) and event schemas (`EventSchema`), including function names, parameter types, parameter descriptions, and other complete metadata
- **Unified calling interfaces**: Provides type-safe synchronous and asynchronous function calling interfaces with automatic type conversion and error handling
- **Event subscription interfaces**: Provides type-safe event subscription interfaces with type-safe event handling callbacks

## Features

### Helper Classes

`brookesia_agent_helper` currently provides the following helper classes:

- [Manager](./include/brookesia/agent_helper/manager.hpp) - Agent manager helper class, providing agent lifecycle management, state machine control, general functions and event definitions
- [Coze](./include/brookesia/agent_helper/coze.hpp) - Coze agent helper class, providing Coze API integration, authorization info, robot configuration definitions
- [OpenAI](./include/brookesia/agent_helper/openai.hpp) - OpenAI agent helper class, providing OpenAI API integration, model configuration definitions
- [Xiaozhi](./include/brookesia/agent_helper/xiaozhi.hpp) - Xiaozhi agent helper class, providing Xiaozhi platform integration, OTA configuration definitions

### Type Safety

Based on C++20 Concepts and CRTP pattern, providing compile-time type safety guarantees:

- **Strongly-typed enumerations**: Uses enumeration types like `FunctionId`, `EventId` instead of strings to avoid runtime errors
- **Compile-time type checking**: Ensures type correctness through `static_assert` and Concepts
- **Type deduction**: Template functions automatically deduce return types, supporting `std::expected<T, std::string>` type-safe return values

### Standardized Interfaces

By inheriting the `Base` class from `brookesia_service_helper`, provides unified interface specifications:

- **Function Schema**: Each function has a complete `FunctionSchema` definition
- **Event Schema**: Each event has a complete `EventSchema` definition
- **Sync/Async calls**: Provides unified `call_function_sync()` and `call_function_async()` calling interfaces
- **Event subscription**: Provides unified `subscribe_event()` event subscription interface

## Adding to Your Project

`brookesia_agent_helper` has been uploaded to the [Espressif Component Registry](https://components.espressif.com/). You can add it to your project using the following methods:

1. **Using command line**

   Run the following command in your project directory:

   ```bash
   idf.py add-dependency "espressif/brookesia_agent_helper"
   ```

2. **Modifying configuration file**

   Create or modify the *idf_component.yml* file in your project directory:

   ```yaml
   dependencies:
     espressif/brookesia_agent_helper: "*"
   ```
