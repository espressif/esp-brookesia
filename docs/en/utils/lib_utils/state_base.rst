.. _utils-lib_utils-state_base-sec-00:

State Base Class
================

:link_to_translation:`zh_CN:[中文]`

Public header: ``#include "brookesia/lib_utils/state_base.hpp"``

.. _utils-lib_utils-state_base-sec-01:

Overview
--------

`state_base` defines the base state abstraction for state machines with enter, exit, and update
callbacks, plus timeouts and update intervals.

.. _utils-lib_utils-state_base-sec-02:

Features
--------

- Standard lifecycle: `on_enter` / `on_exit` / `on_update`
- Timeout actions and update intervals
- Easy to subclass for custom states

.. _utils-lib_utils-state_base-sec-03:

API Reference
---------------

.. include-build-file:: inc/state_base.inc
