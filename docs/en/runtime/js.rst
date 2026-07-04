.. _runtime-js-sec-00:

JS Runtime
==========

:link_to_translation:`zh_CN:[中文]`

.. rubric:: Overview

``brookesia_runtime_js`` runs JavaScript runtime apps through the Brookesia native bridge.

.. rubric:: Core Responsibilities

- Registers native modules as JavaScript globals.
- Supports synchronous and asynchronous service bridge functions.
- Uses ES module app entry files in packaged runtime apps.

.. rubric:: Integration Position

This component is an independent ESP-Brookesia release component. It can be integrated through ESP-IDF component dependencies and combined with peer framework components as needed.

.. rubric:: API Reference

.. include-build-file:: inc/runtime/brookesia_runtime_js/include/brookesia/runtime_js/backend.inc
