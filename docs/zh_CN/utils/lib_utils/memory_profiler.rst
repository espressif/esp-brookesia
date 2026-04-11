.. _utils-lib_utils-memory_profiler-sec-00:

内存分析器
==============

:link_to_translation:`en:[English]`

公共头文件： ``#include "brookesia/lib_utils/memory_profiler.hpp"``

.. _utils-lib_utils-memory_profiler-sec-01:

概述
----

`memory_profiler` 提供堆内存使用情况的采样、统计与阈值检测能力，
便于运行时监控内存趋势与异常波动。

.. _utils-lib_utils-memory_profiler-sec-02:

特性
----

- 采样当前堆内存、历史峰值与统计信息
- 支持周期性 profiling 与手动快照
- 阈值检测与信号回调（超过阈值自动通知）
- 可与 `TaskScheduler` 联动进行定时采样

.. _utils-lib_utils-memory_profiler-sec-03:

API 参考
------------

.. include-build-file:: inc/memory_profiler.inc
