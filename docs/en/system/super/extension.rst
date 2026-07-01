.. _system-super-extension-sec-00:

Extension
====================

:link_to_translation:`zh_CN:[中文]`

This page describes the boundaries for customizing system super in the current implementation.

.. _system-super-extension-sec-01:

Replacing the Resource Directory
--------------------------------

The minimal customization is to replace the resource directory, for example the shell resource directory ``BROOKESIA_SYSTEM_SUPER_SHELL_RESOURCE_DIR``. The resources must still keep the screen paths, template ids, and action strings the source depends on, or you must sync the corresponding constants in ``src/private/system_constants.hpp``.

.. _system-super-extension-sec-02:

Extending the Launcher
----------------------

The current launcher logic lives in the built-in ``ShellApp``: the app source is ``owner_.list_apps()``, only apps with ``manifest.visible = true`` are shown, buttons are created dynamically from the ``launcher_app_button`` template, and the tap action is ``super.launch.app``. For visual-only changes, prefer editing ``resource/shell/constants`` and ``resource/shell/screens``; for filtering, sorting, grouping, or icon logic, modify ``ShellApp::populate_launcher()``.

.. _system-super-extension-sec-03:

Adjusting the Overlay
---------------------

The overlay is the ``/overlay`` screen in the Shell root, mounted by the ``overlay`` flow of the Shell manifest to ``AppTop + Stack + z_order 101``. To add system-level buttons or gestures, extend ``resource/shell/screens/overlay.json`` and the action subscriptions in ``ShellApp::mount_overlay()``. A regular app should not modify the overlay through a runtime service.

.. _system-super-extension-sec-04:

Deriving the System
--------------------

For a product-level system shell, you can keep deriving ``super::System`` or derive ``core::System`` directly:

- To keep only the Super Shell and Overlay: derive ``super::System``.
- To fully replace the shell, navigation, or app flow: deriving ``core::System`` is more direct.

Either way, app-owned GUI and runtime app permissions should still follow the rules in the System Core documentation.
