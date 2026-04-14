.. _utils-lib_utils-function_guard-sec-00:

Function Guard
==============

:link_to_translation:`zh_CN:[中文]`

Public header: ``#include "brookesia/lib_utils/function_guard.hpp"``

.. _utils-lib_utils-function_guard-sec-01:

Overview
--------

`FunctionGuard` is an RAII guard that runs cleanup at scope exit to simplify resource release
and rollback.

.. _utils-lib_utils-function_guard-sec-02:

Features
--------

- Automatic callback on scope exit; optional `release` to cancel
- Move semantics for transferring guards
- Catches `std::exception` during cleanup to avoid breaking teardown

.. _utils-lib_utils-function_guard-sec-03:

API Reference
---------------

.. include-build-file:: inc/function_guard.inc
