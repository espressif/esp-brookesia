.. _system-core-sec-00:

System Core
======================

:link_to_translation:`zh_CN:[中文]`

``brookesia_system_core`` provides the common core capabilities required by ESP-Brookesia product systems. It coordinates native apps, runtime apps, app-owned GUI documents, storage, timers, service access, and permission boundaries.

Overview
--------------------

System Core is the orchestration layer of the Brookesia system framework. It wires together the GUI Runtime, Runtime Manager, Service Manager, and TaskScheduler, and exposes a stable app integration model.

It handles two app kinds uniformly: native apps integrate by subclassing ``IApp`` in C++, while runtime apps integrate through an unpacked package and scripted convention functions. Both kinds operate only on their own GUI document and never touch ``DocumentId``, layers, or mount targets directly.

Core Capabilities
--------------------

- Native app install, start, stop, and lifecycle callbacks.
- Runtime app package scanning, loading, and ``brookesia_app.*`` convention calls.
- App-owned GUI document load, mount, event subscription, and cleanup.
- The ``SystemCore``, ``SystemGui``, and ``SystemTimer`` base services.
- Automatic registration and unregistration of native app GUI image/font resources.
- Basic permission tiers based on ``AppKind``.

Mainline Model
--------------------

A regular app never touches ``gui::DocumentId``, layers, or mount targets; it operates only on its own GUI document, and regular screens always mount to ``AppDefault``. A derived ``System`` and built-in native system apps can use the more complete system capabilities through system-only APIs, for example to manage the shell, status bar, and overlay.

Documentation
--------------------

- :doc:`architecture`: overall runtime behavior, initialization flow, and source modules.
- :doc:`configuration`: build configuration, macros, and runtime storage layout.
- :doc:`app_model`: app kinds, native versus runtime differences, and lifecycle.
- :doc:`gui`: app-owned GUI and system-only GUI.
- :doc:`resources_permissions`: resource registration and permission boundaries.
- :doc:`services`: system service overview and call boundaries.
- :doc:`app_package`: runtime app package format and scanning rules.
- :doc:`examples`: minimal integration examples.
- :doc:`troubleshooting`: common problem diagnosis.
- :doc:`api`: public API reference.

.. toctree::
   :maxdepth: 1
   :titlesonly:
   :hidden:

   Architecture <architecture>
   Configuration <configuration>
   App Model <app_model>
   GUI Management <gui>
   Resources and Permissions <resources_permissions>
   System Services <services>
   App Package <app_package>
   Examples <examples>
   Troubleshooting <troubleshooting>
   API Reference <api>
