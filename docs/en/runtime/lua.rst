.. _runtime-lua-sec-00:

Lua Runtime
===========

:link_to_translation:`zh_CN:[中文]`

.. rubric:: Overview

``brookesia_runtime_lua`` runs Lua runtime apps through the shared Runtime Manager contract.

.. rubric:: Core Responsibilities

- Provides a Lua backend for runtime app packages.
- Exposes host bridge functions to script code.
- Shares package discovery and lifecycle handling with other runtime backends.

.. rubric:: Integration Position

This component is an independent ESP-Brookesia release component. It can be integrated through ESP-IDF component dependencies and combined with peer framework components as needed.

.. rubric:: API Reference

.. include-build-file:: inc/runtime/brookesia_runtime_lua/include/brookesia/runtime_lua/backend.inc
