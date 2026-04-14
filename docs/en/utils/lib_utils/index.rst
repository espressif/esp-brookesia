.. _utils-lib_utils-index-sec-00:

General Utility Classes
=======================

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_lib_utils <https://components.espressif.com/components/espressif/brookesia_lib_utils>`_
- Public header: ``#include "brookesia/lib_utils.hpp"``

.. _utils-lib_utils-index-sec-01:

Overview
--------

`brookesia_lib_utils` is the general-purpose utility library for ESP-Brookesia: task scheduling, thread configuration, profiling, logging, state machines, plugins, and helpers.

.. _utils-lib_utils-index-sec-02:

Features
--------

- Thread configuration: RAII-style thread settings (name, priority, stack, core affinity)
- Task scheduler: Boost.Asio–based async scheduling (immediate, delayed, periodic)
- Logging: ESP_LOG and printf-style output with formatting
- State machine and plugins: complex flows and modular extension
- Helpers: checks, function guards, describe/serialization
- Profilers: memory, thread, and time analysis

.. _utils-lib_utils-index-sec-03:

Core Utilities
--------------

.. toctree::
   :maxdepth: 1

   Thread Config <thread_config>
   Task Scheduler <task_scheduler>
   State Base <state_base>
   State Machine <state_machine>
   Plugin <plugin>

.. _utils-lib_utils-index-sec-04:

Helpers
-------

.. toctree::
   :maxdepth: 1

   Logging <log>
   Check <check>
   Function Guard <function_guard>
   Describe Helpers <describe_helpers>

.. _utils-lib_utils-index-sec-05:

Debug Tools
-----------

.. toctree::
   :maxdepth: 1

   Memory Profiler <memory_profiler>
   Thread Profiler <thread_profiler>
   Time Profiler <time_profiler>
