.. _utils-lib_utils-describe_helpers-sec-00:

Describe Helpers
================

:link_to_translation:`zh_CN:[中文]`

Public header: ``#include "brookesia/lib_utils/describe_helpers.hpp"``

.. _utils-lib_utils-describe_helpers-sec-01:

Overview
--------

`describe_helpers` uses Boost.Describe and Boost.JSON for object description plus serialization
and deserialization, reducing boilerplate for structs, enums, and complex types.

.. _utils-lib_utils-describe_helpers-sec-02:

Features
--------

- Reflection for enums and struct members
- JSON encode/decode for strings, numbers, bools, containers, optionals, etc.
- Handles `variant`, `optional`, `map`, `vector`, and related composites
- Debug-friendly description output

.. _utils-lib_utils-describe_helpers-sec-03:

API reference
---------------

.. include-build-file:: inc/describe_helpers.inc
