.. _utils-lib_utils-log-sec-00:

Logging
=======

:link_to_translation:`zh_CN:[中文]`

Public header: ``#include "brookesia/lib_utils/log.hpp"``

.. _utils-lib_utils-log-sec-01:

Overview
--------

`log` provides a unified logging API with formatting, switching between ESP_LOG and standard output,
and `std::source_location` context.

.. _utils-lib_utils-log-sec-02:

Features
--------

- Levels: Trace / Debug / Info / Warn / Error
- Unified formatting compatible with `boost::format`-style placeholders
- Automatic function/file context
- Trace Guard for enter/exit logging

.. _utils-lib_utils-log-sec-03:

API reference
---------------

.. include-build-file:: inc/log.inc
