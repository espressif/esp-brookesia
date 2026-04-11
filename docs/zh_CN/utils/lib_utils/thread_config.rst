.. _utils-lib_utils-thread_config-sec-00:

线程配置
============

:link_to_translation:`en:[English]`

公共头文件： ``#include "brookesia/lib_utils/thread_config.hpp"``

.. _utils-lib_utils-thread_config-sec-01:

概述
----

`thread_config` 提供线程运行参数的统一配置能力，并通过 RAII 守卫在作用域内
自动应用与恢复线程配置。

.. _utils-lib_utils-thread_config-sec-02:

特性
----

- 提供 `ThreadConfig` 结构体，统一管理名称、优先级、栈大小、核心绑定等
- 支持从系统默认配置或当前线程配置读取并应用
- `ThreadConfigGuard` 作用域内自动恢复原配置
- 提供便捷宏用于配置与查询线程参数

.. _utils-lib_utils-thread_config-sec-03:

API 参考
------------

.. include-build-file:: inc/thread_config.inc
