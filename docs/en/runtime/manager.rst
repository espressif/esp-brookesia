.. _runtime-manager-sec-00:

Runtime Manager
===============

:link_to_translation:`zh_CN:[中文]`

.. rubric:: Overview

``brookesia_runtime_manager`` is the host-side framework for loading runtime app manifests, native bridges, and app lifecycle callbacks.

.. rubric:: Core Responsibilities

- Defines runtime backend registration and app context ownership.
- Provides native host bridge functions such as context attach, detach, finish, and print.
- Connects System Core runtime packages to concrete backends.

.. rubric:: Integration Position

This component is an independent ESP-Brookesia release component. It can be integrated through ESP-IDF component dependencies and combined with peer framework components as needed.

.. rubric:: API Reference

.. include-build-file:: inc/runtime.inc
