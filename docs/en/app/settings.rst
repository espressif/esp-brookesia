.. _app-settings-sec-00:

Settings App
============

:link_to_translation:`zh_CN:[中文]`

.. rubric:: Overview

``brookesia_app_settings`` provides device and system settings screens for products built with System Super.

.. rubric:: Core Responsibilities

- Organizes settings by screens such as home, Wi-Fi, sound, display, language, time zone, and more.
- Uses service helpers for Wi-Fi, device, audio, display, and storage state.
- Loads localized names and messages from app package i18n resources.

.. rubric:: Integration Position

This component is an independent ESP-Brookesia release component. It can be integrated through ESP-IDF component dependencies and combined with peer framework components as needed.

.. rubric:: API Reference

.. include-build-file:: inc/settings_app.inc
