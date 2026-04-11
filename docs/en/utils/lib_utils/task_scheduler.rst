.. _utils-lib_utils-task_scheduler-sec-00:

Task Scheduler
==============

:link_to_translation:`zh_CN:[中文]`

Public header: ``#include "brookesia/lib_utils/task_scheduler.hpp"``

.. _utils-lib_utils-task_scheduler-sec-01:

Overview
--------

`task_scheduler` is a Boost.Asio–based scheduler supporting immediate, delayed, and periodic tasks,
with grouped serial/parallel execution.

.. _utils-lib_utils-task_scheduler-sec-02:

Features
--------

- One-shot, delayed, periodic, and batch scheduling
- Task groups and serial execution control
- Pause, resume, cancel, and wait for completion
- Statistics and pre/post callbacks

.. _utils-lib_utils-task_scheduler-sec-03:

API reference
---------------

.. include-build-file:: inc/task_scheduler.inc
