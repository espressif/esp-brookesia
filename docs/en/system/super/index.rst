.. _system-super-sec-00:

System Super
========================

:link_to_translation:`zh_CN:[中文]`

``brookesia_system_super`` is a standard system built on System Core for handheld devices with rectangular touch screens. It provides a default shell, launcher, overlay, status bar, app switching, and built-in resource flow.

Overview
--------------------

System Super derives from ``core::System`` and uses the core lifecycle extension points to install the built-in Shell app, mount the overlay, and restore the Shell after a regular app stops. It does not reimplement the app lifecycle or runtime app package loading; it reuses System Core capabilities and focuses on the pages, navigation, and resources of the system shell.

.. note::

   The Super name originates from the simplified form of `Brookesia Superciliaris <https://en.wikipedia.org/wiki/Brookesia_superciliaris>`_, a representative species of the Brookesia genus. It fits this system as the standard and complete ESP-Brookesia system for rectangular touch devices.

Relationship to System Core
---------------------------

- The product project provides the GUI backend and device services through ``core_config.gui_backend``.
- Super manages the Shell and overlay resources, while the app lifecycle, GUI document loading, and runtime app package scanning are still handled by System Core.
- A customized product should keep user app screens in the regular app layer and keep the system UI in the Shell-managed layers.

Key Capabilities
--------------------

- The built-in ``ShellApp``: the Home Dashboard, App Launcher, and Notifications pages.
- The system overlay: the status bar, the bottom swipe-up gesture indicator to exit an app, and floating layers such as keyboard, message dialog, and app modal.
- The background flow for the desktop background and app background.
- Light/dark Runtime themes and an on-demand system keyboard.
- The startup overlay at boot and the app launch transition animation.

Documentation
--------------------

- :doc:`architecture`: the derivation and extension points.
- :doc:`shell`: the Shell app, pages, launcher, and theme.
- :doc:`overlay`: the status bar, gesture indicator, and system floating layers.
- :doc:`app_flow`: the flow to start the Shell and open or exit an app.
- :doc:`assets`: the Shell resource structure, screens, flows, and templates.
- :doc:`configuration`: macros, Kconfig, and resource paths.
- :doc:`extension`: the boundaries for customizing the Shell, overlay, and resources.
- :doc:`troubleshooting`: common problem diagnosis.
- :doc:`api`: public API reference.

.. toctree::
   :maxdepth: 1
   :titlesonly:
   :hidden:

   Architecture <architecture>
   Shell <shell>
   Overlay <overlay>
   App Flow <app_flow>
   Assets <assets>
   Configuration <configuration>
   Extension <extension>
   Troubleshooting <troubleshooting>
   API Reference <api>
