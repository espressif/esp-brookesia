.. _utils-lib_utils-log-sec-00:

日志系统
============

:link_to_translation:`en:[English]`

公共头文件： ``#include "brookesia/lib_utils/log.hpp"``

.. _utils-lib_utils-log-sec-01:

概述
----

`log` 提供统一的日志接口与格式化能力，可在 ESP_LOG 与标准输出之间切换，
并支持基于 `std::source_location` 的上下文信息。

.. _utils-lib_utils-log-sec-02:

特性
----

- 支持多级别日志输出（Trace/Debug/Info/Warn/Error）
- 统一的格式化接口，兼容 `boost::format` 风格占位符
- 自动提取函数名、文件名等上下文，便于定位问题
- 提供 Trace Guard 用于自动记录函数进入/退出

.. _utils-lib_utils-log-sec-03:

API 参考
------------

.. include-build-file:: inc/log.inc
