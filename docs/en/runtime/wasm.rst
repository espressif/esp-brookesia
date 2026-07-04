.. _runtime-wasm-sec-00:

WASM Runtime
============

:link_to_translation:`zh_CN:[中文]`

.. rubric:: Overview

``brookesia_runtime_wasm`` executes WebAssembly runtime apps behind the common runtime backend interface.

.. rubric:: Core Responsibilities

- Implements runtime loading for WASM artifacts.
- Connects WebAssembly code to native host bridge functions.
- Targets sandboxed or portable runtime-app deployment.

.. rubric:: Integration Position

This component is an independent ESP-Brookesia release component. It can be integrated through ESP-IDF component dependencies and combined with peer framework components as needed.

.. rubric:: API Reference

.. include-build-file:: inc/runtime/brookesia_runtime_wasm/include/brookesia/runtime_wasm/backend.inc
