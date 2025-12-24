# ESP-Brookesia Library Utils

* [中文版本](./README_CN.md)

## Overview

`brookesia_lib_utils` is the core utility library of the ESP-Brookesia framework, providing a comprehensive set of practical tools including `task scheduler`, `thread configuration`, `performance profilers`, `logging system`, `state machine`, `plugin system`, and various `helper utilities`.

## Table of Contents

- [ESP-Brookesia Library Utils](#esp-brookesia-library-utils)
  - [Overview](#overview)
  - [Table of Contents](#table-of-contents)
  - [Features](#features)
  - [How to Use](#how-to-use)
    - [Development Environment Requirements](#development-environment-requirements)
    - [Adding to Project](#adding-to-project)
  - [Overview of Utility Classes](#overview-of-utility-classes)

## Features

- **Task Scheduler**: An asynchronous task scheduling system based on Boost.Asio, supporting multi-threaded task management, including immediate, delayed, and periodic tasks.
- **Thread Configuration**: RAII-style thread configuration management, supporting thread naming, priority setting, stack size and location configuration, CPU core binding, and more.
- **Profilers**:
  - Memory Profiler: Monitors and analyzes memory usage, supporting automatic threshold detection and signal sending.
  - Thread Profiler: Monitors thread status and performance, supporting automatic threshold detection and signal sending.
  - Time Profiler: Measures code execution time and performance bottlenecks
- **Logging System**: Supports both ESP_LOG and standard printf output, and allows output formatting with boost::format.
- **State Machine**: Provides base classes and implementations for state machines, making it easy to manage complex state transitions.
- **Plugin System**: Enables a modular development plugin mechanism.
- **Helpers**:
  - Check Macros: Provides macros for checking false, nullptr, exceptions, and value ranges, as well as supporting custom error handling code blocks.
  - Function Guard: Provides RAII-style function call guards to help manage function call order and resource releasing.
  - Object Description Tools: Uses static reflection to provide object description, supporting serialization and deserialization of enums, structs, JSON, and certain standard types (string, integer, float, boolean, pointer, array, containers, etc.).

## How to Use

### Development Environment Requirements

Before using this library, please ensure the following SDK development environment is installed:

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> * For SDK installation, please refer to [ESP-IDF Programming Guide - Installation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

### Adding to Project

`brookesia_lib_utils` has been uploaded to the [Espressif Component Registry](https://components.espressif.com/). You can add it to your project in the following ways:

1. **Using Command Line**

    Run the following command in your project directory:

    ```bash
    idf.py add-dependency "espressif/brookesia_lib_utils"
    ```

2. **Modifying Configuration File**

    Create or modify the *idf_component.yml* file in your project directory:

    ```yaml
    dependencies:
      espressif/brookesia_lib_utils: "*"
    ```

For detailed information, please refer to [Espressif Documentation - IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## Overview of Utility Classes

|                                                           Header File                                                            |                  Description                   |                                                Test Cases                                                |
| -------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------- | -------------------------------------------------------------------------------------------------------- |
| [check.hpp](include/brookesia/lib_utils/check.hpp)                                                                               | Provides assertion and condition checking      | [test_apps/check](test_apps/check)                                                                       |
| [describe_helpers.hpp](include/brookesia/lib_utils/describe_helpers.hpp)                                                         | Provides object description and debugging info | [test_apps/describe_helpers](test_apps/describe_helpers)                                                 |
| [function_guard.hpp](include/brookesia/lib_utils/function_guard.hpp)                                                             | Provides RAII-style function guard             | [test_apps/function_guard](test_apps/function_guard)                                                     |
| [log.hpp](include/brookesia/lib_utils/log.hpp)                                                                                   | Provides logging system                        | [test_apps/log](test_apps/log)                                                                           |
| [memory_profiler.hpp](include/brookesia/lib_utils/memory_profiler.hpp)                                                           | Provides memory profiling                      | [test_apps/memory_profiler](test_apps/memory_profiler)                                                   |
| [plugin.hpp](include/brookesia/lib_utils/plugin.hpp)                                                                             | Provides plugin system                         | [test_apps/plugin](test_apps/plugin)                                                                     |
| [state_base.hpp](include/brookesia/lib_utils/state_base.hpp), [state_machine.hpp](include/brookesia/lib_utils/state_machine.hpp) | Provides state base class and state machine    | [test_apps/state_base](test_apps/state_machine), [test_apps/state_machine](test_apps/state_machine/main) |
| [task_scheduler.hpp](include/brookesia/lib_utils/task_scheduler.hpp)                                                             | Provides task scheduler                        | [test_apps/task_scheduler](test_apps/task_scheduler)                                                     |
| [thread_config.hpp](include/brookesia/lib_utils/thread_config.hpp)                                                               | Provides thread configuration                  | [test_apps/thread_config](test_apps/thread_config)                                                       |
| [thread_profiler.hpp](include/brookesia/lib_utils/thread_profiler.hpp)                                                           | Provides thread profiling                      | [test_apps/thread_profiler](test_apps/thread_profiler)                                                   |
| [time_profiler.hpp](include/brookesia/lib_utils/time_profiler.hpp)                                                               | Provides time profiling                        | [test_apps/time_profiler](test_apps/time_profiler)                                                       |
