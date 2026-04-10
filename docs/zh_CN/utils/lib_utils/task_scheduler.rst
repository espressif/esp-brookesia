.. _utils-lib_utils-task_scheduler-sec-00:

任务调度器
==============

:link_to_translation:`en:[English]`

公共头文件： ``#include "brookesia/lib_utils/task_scheduler.hpp"``

.. _utils-lib_utils-task_scheduler-sec-01:

概述
----

`task_scheduler` 提供基于 Boost.Asio 的任务调度器，支持即时、延迟、周期任务
以及分组串行/并行执行。

.. _utils-lib_utils-task_scheduler-sec-02:

特性
----

- 支持一次性、延迟、周期与批量任务调度
- 任务分组与串行执行控制
- 支持暂停、恢复、取消与等待任务完成
- 提供统计信息与执行前后回调

.. _utils-lib_utils-task_scheduler-sec-03:

API 参考
------------

.. include-build-file:: inc/task_scheduler.inc
