.. _app-files-sec-00:

Files App
=========

:link_to_translation:`zh_CN:[中文]`

.. rubric:: Overview

``brookesia_app_files`` is the built-in file browser app for System Core packages.

.. rubric:: Core Responsibilities

- Browses mounted storage file systems.
- Displays directories and files through GUI resources.
- Uses Storage service and System Core app lifecycle hooks.

.. rubric:: Integration Position

This component is an independent ESP-Brookesia release component. It can be integrated through ESP-IDF component dependencies and combined with peer framework components as needed.

.. rubric:: API Reference

.. include-build-file:: inc/files_app.inc
