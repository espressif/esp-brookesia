.. _utils-lib_utils-time_profiler-sec-00:

Time Profiler
===============

:link_to_translation:`zh_CN:[中文]`

Public header: ``#include "brookesia/lib_utils/time_profiler.hpp"``

.. _utils-lib_utils-time_profiler-sec-01:

Overview
--------

`time_profiler` provides hierarchical timing for scopes and events to locate bottlenecks and hot
paths.

.. _utils-lib_utils-time_profiler-sec-02:

Features
--------

- Scope and event timing models
- Hierarchical reports with customizable output
- Per-thread sampling
- Convenience macros for instrumentation

.. warning::

   On **ESP32-P4** with ``CONFIG_SPIRAM_XIP_FROM_PSRAM=y``,
   ``time_profiler`` may crash the system.

.. _utils-lib_utils-time_profiler-sec-03:

API reference
---------------

.. include-build-file:: inc/time_profiler.inc
