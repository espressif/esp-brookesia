.. _runtime-elf-sec-00:

ELF Runtime
===========

:link_to_translation:`zh_CN:[中文]`

.. rubric:: Overview

``brookesia_runtime_elf`` loads native ELF runtime applications when the target platform supports that deployment model.

.. rubric:: Core Responsibilities

- Implements the Runtime Manager backend interface.
- Uses package metadata to locate executable artifacts.
- Runs native code behind the common app lifecycle contract.

.. rubric:: Integration Position

This component is an independent ESP-Brookesia release component. It can be integrated through ESP-IDF component dependencies and combined with peer framework components as needed.

.. rubric:: API Reference

.. include-build-file:: inc/runtime/brookesia_runtime_elf/include/brookesia/runtime_elf/backend.inc
