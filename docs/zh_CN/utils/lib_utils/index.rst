.. _utils-lib_utils-index-sec-00:

通用工具类
==============

:link_to_translation:`en:[English]`

- 组件注册表： `espressif/brookesia_lib_utils <https://components.espressif.com/components/espressif/brookesia_lib_utils>`_
- 公共头文件： ``#include "brookesia/lib_utils.hpp"``

.. _utils-lib_utils-index-sec-01:

概述
----

`brookesia_lib_utils` 是 ESP-Brookesia 框架的通用工具库，提供任务调度、线程配置、性能分析、日志、状态机、插件系统以及通用辅助工具等能力。

.. _utils-lib_utils-index-sec-02:

特性
----

- 线程配置：RAII 风格线程配置，支持名称、优先级、栈大小、核心绑定等
- 任务调度器：基于 Boost.Asio 的异步任务调度，支持立即、延迟与周期任务
- 日志系统：支持 ESP_LOG 与 printf 输出，并支持格式化
- 状态机与插件系统：支持复杂状态流转和模块化扩展
- 辅助工具：覆盖错误检查、函数守卫、对象描述与序列化等能力
- 性能分析工具：提供内存、线程、时间维度分析能力

.. _utils-lib_utils-index-sec-03:

核心工具
------------

.. toctree::
   :maxdepth: 1

   线程配置 <thread_config>
   任务调度器 <task_scheduler>
   状态基类 <state_base>
   状态机 <state_machine>
   插件系统 <plugin>

.. _utils-lib_utils-index-sec-04:

辅助工具
------------

.. toctree::
   :maxdepth: 1

   日志系统 <log>
   错误检查 <check>
   函数守卫 <function_guard>
   描述辅助 <describe_helpers>

.. _utils-lib_utils-index-sec-05:

调试工具
------------

.. toctree::
   :maxdepth: 1

   内存分析器 <memory_profiler>
   线程分析器 <thread_profiler>
   时间分析器 <time_profiler>
