.. _app-store-sec-00:

App Store
=========

:link_to_translation:`zh_CN:[中文]`

.. rubric:: Overview

``brookesia_app_store`` manages local and remote app packages for System Core installations.

.. rubric:: Core Responsibilities

- Fetches remote app indexes through HTTP service.
- Downloads, installs, removes, and lists app packages.
- Uses Storage and System Core app-management APIs for package data.

.. rubric:: Integration Position

This component is an independent ESP-Brookesia release component. It can be integrated through ESP-IDF component dependencies and combined with peer framework components as needed.

.. rubric:: API Reference

.. include-build-file:: inc/app_store_app.inc
