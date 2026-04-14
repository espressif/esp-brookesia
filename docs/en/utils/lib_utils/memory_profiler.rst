.. _utils-lib_utils-memory_profiler-sec-00:

Memory Profiler
=================

:link_to_translation:`zh_CN:[中文]`

Public header: ``#include "brookesia/lib_utils/memory_profiler.hpp"``

.. _utils-lib_utils-memory_profiler-sec-01:

Overview
--------

`memory_profiler` samples heap usage, tracks peaks, and supports threshold alerts for runtime memory
monitoring.

.. _utils-lib_utils-memory_profiler-sec-02:

Features
--------

- Current heap, historical peak, and statistics
- Periodic profiling and manual snapshots
- Threshold callbacks when limits are exceeded
- Optional integration with `TaskScheduler` for periodic sampling

.. _utils-lib_utils-memory_profiler-sec-03:

API Reference
---------------

.. include-build-file:: inc/memory_profiler.inc
