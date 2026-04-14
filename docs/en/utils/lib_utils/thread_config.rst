.. _utils-lib_utils-thread_config-sec-00:

Thread Configuration
====================

:link_to_translation:`zh_CN:[中文]`

Public header: ``#include "brookesia/lib_utils/thread_config.hpp"``

.. _utils-lib_utils-thread_config-sec-01:

Overview
--------

`thread_config` centralizes thread runtime parameters and uses RAII guards to apply and restore
settings in a scope.

.. _utils-lib_utils-thread_config-sec-02:

Features
--------

- `ThreadConfig` for name, priority, stack, core affinity
- Read defaults or current thread settings
- `ThreadConfigGuard` restores previous config on scope exit
- Convenience macros for configuration and queries

.. _utils-lib_utils-thread_config-sec-03:

API Reference
---------------

.. include-build-file:: inc/thread_config.inc
