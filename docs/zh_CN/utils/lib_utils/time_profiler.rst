.. _utils-lib_utils-time_profiler-sec-00:

时间分析器
==============

:link_to_translation:`en:[English]`

公共头文件： ``#include "brookesia/lib_utils/time_profiler.hpp"``

.. _utils-lib_utils-time_profiler-sec-01:

概述
----

`time_profiler` 提供层级化的时间分析能力，可记录作用域与事件耗时，
帮助定位性能瓶颈与热点路径。

.. _utils-lib_utils-time_profiler-sec-02:

特性
----

- 支持作用域与事件两类计时模型
- 生成层级化统计报告，支持自定义输出格式
- 支持线程内的独立采样与统计
- 提供便捷宏快速接入分析点

.. warning::

   当 `ESP32-P4` 开启 ``CONFIG_SPIRAM_XIP_FROM_PSRAM=y`` 时，
   ``time_profiler`` 功能可能导致系统崩溃。

.. _utils-lib_utils-time_profiler-sec-03:

API 参考
------------

.. include-build-file:: inc/time_profiler.inc
