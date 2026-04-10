.. _utils-lib_utils-thread_profiler-sec-00:

线程分析器
==============

:link_to_translation:`en:[English]`

公共头文件： ``#include "brookesia/lib_utils/thread_profiler.hpp"``

.. _utils-lib_utils-thread_profiler-sec-01:

概述
----

`thread_profiler` 提供线程（任务）运行状态与 CPU 使用情况的采样与统计能力，
便于分析系统负载与异常任务。

.. _utils-lib_utils-thread_profiler-sec-02:

特性
----

- 采样任务状态、CPU 占用与栈等信息
- 支持周期性 profiling 与手动快照
- 提供排序、过滤与阈值检测能力
- 可与 `TaskScheduler` 联动进行定时采样

.. _utils-lib_utils-thread_profiler-sec-03:

API 参考
------------

.. include-build-file:: inc/thread_profiler.inc
