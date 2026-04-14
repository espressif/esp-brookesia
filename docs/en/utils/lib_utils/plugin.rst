.. _utils-lib_utils-plugin-sec-00:

Plugin System
=============

:link_to_translation:`zh_CN:[中文]`

Public header: ``#include "brookesia/lib_utils/plugin.hpp"``

.. _utils-lib_utils-plugin-sec-01:

Overview
--------

`plugin` provides a generic plugin registry and instance management: register, discover, and create plugins by name.

.. _utils-lib_utils-plugin-sec-02:

Features
--------

- Template registration and lookup by name
- Lazy instantiation and singleton registration
- Enumerate, release, and cleanup
- Macros for registration symbols and link visibility

.. _utils-lib_utils-plugin-sec-03:

API Reference
---------------

.. include-build-file:: inc/plugin.inc
