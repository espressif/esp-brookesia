# ESP-Brookesia Library Utils

* [English Version](./README.md)

## 概述

`brookesia_lib_utils` 是 ESP-Brookesia 框架的核心工具库，提供了一套完整的实用工具集，包括 `任务调度器`、`线程配置`、`性能分析工具`、`日志系统`、`状态机`、`插件系统`以及各种`辅助工具`。

## 目录

- [ESP-Brookesia Library Utils](#esp-brookesia-library-utils)
  - [概述](#概述)
  - [目录](#目录)
  - [特性](#特性)
  - [如何使用](#如何使用)
    - [开发环境要求](#开发环境要求)
    - [添加到工程](#添加到工程)
  - [工具类介绍](#工具类介绍)

## 特性

- **任务调度器 (Task Scheduler)**：基于 Boost.Asio 的异步任务调度系统，支持多线程任务调度，支持立即任务、延迟任务以及周期任务执行
- **线程配置 (Thread Configuration)**：RAII 风格的线程配置管理，支持设置线程名称、优先级、栈大小、栈位置、CPU 核心绑定等
- **性能分析工具 (Profilers)**：
  - 内存分析器：监控和分析内存使用情况，支持自动阈值检测和信号发送
  - 线程分析器：监控线程状态和性能，支持自动阈值检测和信号发送
  - 时间分析器：测量代码执行时间和性能瓶颈
- **日志系统 (Logging)**：支持 ESP_LOG 和标准 printf 两种输出方式，支持 boost::format 格式化输出
- **状态机 (State Machine)**：提供状态基类和状态机实现，便于管理复杂的状态转换逻辑
- **插件系统 (Plugin)**：支持模块化开发的插件机制
- **辅助工具 (Helpers)**：
  - 检查宏：提供包含 false、nullptr、异常以及数据范围检查的宏，支持自定义错误处理代码块
  - 函数守卫：RAII 风格的函数调用守卫，辅助管理函数调用顺序和资源释放
  - 对象描述工具：通过静态反射机制提供对象描述信息，支持枚举、结构体、JSON 以及部分标准类型（字符串、整数、浮点数、布尔值、指针、容器等）的序列化和反序列化

## 如何使用

### 开发环境要求

使用本库前，请确保已安装以下 SDK 开发环境：

- [ESP-IDF](https://github.com/espressif/esp-idf): `>=5.5,<6`

> [!NOTE]
> SDK 的安装方法请参阅 [ESP-IDF 编程指南 - 安装](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)

### 添加到工程

`brookesia_lib_utils` 已上传到 [Espressif 组件库](https://components.espressif.com/)，您可以通过以下方式将其添加到工程中：

1. **使用命令行**

    在工程目录下运行以下命令：

   ```bash
   idf.py add-dependency "espressif/brookesia_lib_utils"
   ```

2. **修改配置文件**

   在工程目录下创建或修改 *idf_component.yml* 文件：

   ```yaml
   dependencies:
     espressif/brookesia_lib_utils: "*"
   ```

详细说明请参阅 [Espressif 文档 - IDF 组件管理器](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/tools/idf-component-manager.html)。

## 工具类介绍

|                                                              头文件                                                              |           功能说明           |                                                 测试用例                                                 |
| -------------------------------------------------------------------------------------------------------------------------------- | ---------------------------- | -------------------------------------------------------------------------------------------------------- |
| [check.hpp](include/brookesia/lib_utils/check.hpp)                                                                               | 提供断言和条件检查功能       | [test_apps/check](test_apps/check)                                                                       |
| [describe_helpers.hpp](include/brookesia/lib_utils/describe_helpers.hpp)                                                         | 提供对象描述和调试信息       | [test_apps/describe_helpers](test_apps/describe_helpers)                                                 |
| [function_guard.hpp](include/brookesia/lib_utils/function_guard.hpp)                                                             | 提供 RAII 风格的函数守卫功能 | [test_apps/function_guard](test_apps/function_guard)                                                     |
| [log.hpp](include/brookesia/lib_utils/log.hpp)                                                                                   | 提供日志系统                 | [test_apps/log](test_apps/log)                                                                           |
| [memory_profiler.hpp](include/brookesia/lib_utils/memory_profiler.hpp)                                                           | 提供内存分析功能             | [test_apps/memory_profiler](test_apps/memory_profiler)                                                   |
| [plugin.hpp](include/brookesia/lib_utils/plugin.hpp)                                                                             | 提供插件系统                 | [test_apps/plugin](test_apps/plugin)                                                                     |
| [state_base.hpp](include/brookesia/lib_utils/state_base.hpp), [state_machine.hpp](include/brookesia/lib_utils/state_machine.hpp) | 提供状态基类和状态机         | [test_apps/state_base](test_apps/state_machine), [test_apps/state_machine](test_apps/state_machine/main) |
| [task_scheduler.hpp](include/brookesia/lib_utils/task_scheduler.hpp)                                                             | 提供任务调度器               | [test_apps/task_scheduler](test_apps/task_scheduler)                                                     |
| [thread_config.hpp](include/brookesia/lib_utils/thread_config.hpp)                                                               | 提供线程配置功能             | [test_apps/thread_config](test_apps/thread_config)                                                       |
| [thread_profiler.hpp](include/brookesia/lib_utils/thread_profiler.hpp)                                                           | 提供线程分析功能             | [test_apps/thread_profiler](test_apps/thread_profiler)                                                   |
| [time_profiler.hpp](include/brookesia/lib_utils/time_profiler.hpp)                                                               | 提供时间分析功能             | [test_apps/time_profiler](test_apps/time_profiler)                                                       |
