.. _utils-lib_utils-thread_profiler-sec-00:

Thread Profiler
=================

:link_to_translation:`zh_CN:[中文]`

Public header: ``#include "brookesia/lib_utils/thread_profiler.hpp"``

.. _utils-lib_utils-thread_profiler-sec-01:

Overview
--------

`thread_profiler` samples task and thread state and CPU usage,
helping analyze system load and hot tasks.

.. _utils-lib_utils-thread_profiler-sec-02:

Features
--------

- Task status, CPU usage, stack info
- Periodic profiling and manual snapshots
- Sorting, filtering, and thresholds
- Optional `TaskScheduler` integration

.. _utils-lib_utils-thread_profiler-sec-03:

API reference
---------------

.. include-build-file:: inc/thread_profiler.inc
