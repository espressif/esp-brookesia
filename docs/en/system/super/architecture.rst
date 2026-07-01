.. _system-super-architecture-sec-00:

Architecture
====================

:link_to_translation:`zh_CN:[中文]`

``esp_brookesia::system::super::System`` derives from ``esp_brookesia::system::core::System`` and provides a Super-style system shell on top of the core.

Initialization
--------------------

``super::System::init()`` eventually calls ``init(Config{})``, and ``init(Config config)``:

1. Sets ``config.core_config.system_type`` to ``BROOKESIA_SYSTEM_SUPER_SYSTEM_TYPE``.
2. Checks that the caller provides a GUI backend through ``config.core_config.gui_backend``.
3. Calls ``core::System::init()``.

Super does not depend on a specific GUI backend directly; the product project selects the backend (for example the LVGL backend) and passes it via ``core_config.gui_backend``.

System Type
--------------------

Super uses ``BROOKESIA_SYSTEM_SUPER_SYSTEM_TYPE``, currently ``"super"``. ``on_init()`` also calls ``set_system_type("super")`` again to ensure service queries return the Super type.

Extension Points
--------------------

The core extension points Super overrides:

.. list-table::
   :header-rows: 1

   * - Function
     - Super Behavior
   * - ``on_init()``
     - Set the system type and install the built-in shell app
   * - ``on_start()``
     - Start the Shell app; the core starts background/content/overlay flows per the Shell manifest
   * - ``on_stop()``
     - Mark stopping and stop the Shell-managed UI
   * - ``on_deinit()``
     - Clean up the shell app id
   * - ``on_app_installed()``
     - Log that a launcher app is available
   * - ``on_app_started()``
     - Log the foreground app and switch Shell background / app background by the active app
   * - ``on_app_stopped()``
     - Restore the shell after a regular app stops

Source Modules
--------------------

The implementation of ``system_super`` is split by responsibility:

.. list-table::
   :header-rows: 1

   * - File
     - Responsibility
   * - ``src/system.cpp``
     - The thin ``System::init()`` entry
   * - ``src/system_lifecycle.cpp``
     - Core extension points and Shell app install/restore
   * - ``src/shell_overlay.cpp``
     - The overlay binding, status, Wi-Fi, clock, and bottom swipe-up exit of ``ShellApp``
   * - ``src/system_navigation.cpp``
     - Shell page restore and active app close
   * - ``src/shell_app.cpp``
     - ``ShellApp``, the Shell multi-page, and the dynamic App Launcher
   * - ``src/private/system_constants.hpp``
     - Internal path, action, and animation constants
   * - ``src/private/shell_app.hpp``
     - Internal ``ShellApp`` declaration

Built-In Components
--------------------

Super currently has three built-in GUI parts:

- Shell app: a native ``IApp`` with manifest id ``brookesia.system.super.shell``, hosting the Home, App Launcher, and Notifications content pages.
- background: the desktop background screen inside the Shell main document, mounted by the ``background`` flow to ``SystemBottom``.
- overlay: the system overlay screen inside the Shell main document, mounted by the ``overlay`` flow to ``AppTop + Stack + z_order 101``, responsible for the top status bar, gesture indicator, and floating layers such as the app modal.
