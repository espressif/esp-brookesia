.. _utils-lib_utils-check-sec-00:

Error Checking
==============

:link_to_translation:`zh_CN:[中文]`

Public header: ``#include "brookesia/lib_utils/check.hpp"``

.. _utils-lib_utils-check-sec-01:

Overview
--------

`check` provides unified check macros for invalid parameters, failed expressions, exceptions,
error codes, and out-of-range values.

.. _utils-lib_utils-check-sec-02:

Features
--------

- Covers common cases: ``nullptr``, booleans, `std::exception`, `esp_err_t`, ranges
- Multiple failure modes: code blocks, `return`, `exit`, `goto`
- Integrates with logging for context
- Configurable failure policy (no-op, log, assert)

.. _utils-lib_utils-check-sec-03:

API Reference
---------------

.. include-build-file:: inc/check.inc
