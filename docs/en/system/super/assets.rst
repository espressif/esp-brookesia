.. _system-super-assets-sec-00:

Assets
====================

:link_to_translation:`zh_CN:[中文]`

The main JSON UI resources of system super are concentrated in the Shell directory, organized as ``constants + screens + flows + templates``.

.. _system-super-assets-sec-01:

Overview
--------------------

The Shell resource directory structure:

- ``resource/shell/root.json``: the single Shell app entry, loading constants, generated ``imageSet`` descriptors, screens, flows, and templates.
- ``resource/shell/constants/*.json``: default and resolution-override tokens.
- ``resource/shell/screens/*.json``: the Background, Home, App Launcher, Notifications, Overlay, and keyboard input screens.
- ``resource/shell/flows/*.json``: the ``screenFlow`` assets for background, content pages, and overlay.
- ``resource/shell/templates/*.json``: dynamic instance templates.
- ``resource/themes/*.json``: Runtime theme descriptors, loaded by ShellApp at startup, not written into ``root.json.assets``.
- ``resource/startup/root.json``: the boot screen mounted by system core to ``SystemTop`` before the Shell loads.

The source images live under ``assets/shell/images/**``. At build time, CMake first stages ``resource/`` verbatim to the runtime resource root, then converts the images under ``assets/`` with ``brookesia_gui_lvgl_pack_images()`` into LVGL ``.bin + imageSet index.json``: PNG outputs as ``ARGB8888`` and JPG/JPEG outputs as ``RGB565``, and the generated files are not committed to the source directory.

.. _system-super-assets-sec-02:

Constants Namespaces
--------------------

Constants use ``ui`` as the internal Shell token namespace:

.. list-table::
   :header-rows: 1

   * - Namespace
     - Purpose
   * - ``${constant.ui.color...}``
     - Light semantic colors such as surface, border, accent, status colors, and overlay-specific colors
   * - ``${constant.ui.metric...}``
     - Shared metrics that need explicit reference, such as page title, caption, and card corner radius
   * - ``${constant.ui.content.metric...}``
     - Content page sizes such as the work area, launcher grid, chip, and search
   * - ``${constant.ui.overlay.metric...}``
     - The status bar, gesture indicator, and overlay animation sizes

Common visuals such as the default font, base text color, transparent screen/container background, and default borders of button/text input are provided by ``resource/themes/*.json``; constants are reserved for semantic colors, font sizes, and layout sizes that need explicit reference in specific nodes.

.. _system-super-assets-sec-03:

Runtime Themes
--------------------

A theme descriptor is a global Runtime resource, not a Shell document asset. ShellApp scans all ``*.json`` in the first level of ``<system-root>/super/themes`` at startup, loads them sorted by file name, and sets the default theme to ``shell.light``. The current V1 provides ``light.json`` (``shell.light``) and ``dark.json`` (``shell.dark``). Shell screens reference named styles such as ``shell.card``, ``shell.pageTitle``, and ``shell.navButton`` through ``styleRefs``; an explicit node ``style`` takes precedence. Do not write ``resource/themes/*.json`` into the ``assets`` of ``root.json``; the JSON UI parser rejects the document asset type ``theme``.

.. _system-super-assets-sec-04:

Shell Root
--------------------

The Shell root defines the Shell background screens, content pages, overlay, and launcher button template. Key nodes:

.. list-table::
   :header-rows: 1

   * - Node or Template
     - Path or Id
     - C++ Constant
   * - Desktop background screen
     - ``/background``
     - ``SUPER_BACKGROUND_SCREEN_PATH``
   * - App background screen
     - ``/app_background``
     - JSON screen id
   * - Home Dashboard screen
     - ``/home``
     - ``SUPER_HOME_SCREEN_PATH``
   * - App Launcher screen
     - ``/launcher``
     - ``SUPER_APP_LAUNCHER_SCREEN_PATH``
   * - Notifications screen
     - ``/notifications``
     - ``SUPER_NOTIFICATIONS_SCREEN_PATH``
   * - overlay screen
     - ``/overlay``
     - ``SUPER_OVERLAY_SCREEN_PATH``
   * - keyboard input screen
     - ``/keyboard_input``
     - ``SUPER_KEYBOARD_INPUT_SCREEN_PATH``
   * - status bar
     - ``/overlay/status``
     - ``SUPER_STATUS_BAR_PATH``
   * - status Wi-Fi
     - ``/overlay/status/status_right/wifi_pill``
     - ``SUPER_STATUS_WIFI_PATH``
   * - status clock
     - ``/overlay/status/status_right/clock``
     - ``SUPER_STATUS_CLOCK_PATH``
   * - gesture indicator
     - ``/overlay/gesture_indicator``
     - ``SUPER_GESTURE_INDICATOR_PATH``
   * - summary label
     - ``/launcher/content/summary_badge/summary``
     - ``SUPER_LAUNCHER_SUMMARY_PATH``
   * - launcher grid
     - ``/launcher/content/grid``
     - ``SUPER_LAUNCHER_GRID_PATH``
   * - launcher button template
     - ``launcher_app_button``
     - ``SUPER_LAUNCHER_BUTTON_TEMPLATE_ID``

A dynamic instance uses the instance id prefix ``app_`` and the path prefix ``/launcher/content/grid/app_``. A valid tap on the launcher button root starts an app through the action ``super.launch.app`` (``SUPER_ACTION_LAUNCH_APP``).

.. _system-super-assets-sec-05:

Screen Flows
--------------------

The Shell root references 3 flows: ``flows/background.json`` manages ``/background`` (mounted to ``SystemBottom``); ``flows/shell_pages.json`` manages Home / App Launcher / Notifications (mounted to ``AppDefault``); ``flows/overlay.json`` manages ``/overlay`` (mounted to ``AppTop`` with ``Stack`` and ``z_order = 101``). The state id of ``shell_pages`` uses the top-level screen id directly, and the navigation bar actions serve as flow transition actions:

.. list-table::
   :header-rows: 1

   * - Action
     - Target Screen
   * - ``super.nav.home``
     - ``/home``
   * - ``super.nav.open_launcher``
     - ``/launcher``
   * - ``super.nav.open_notifications``
     - ``/notifications``

The Shell manifest auto-starts the ``shell_pages`` flow, and subsequent page switching only triggers the flow, without manual unmount/mount of content screens.

.. _system-super-assets-sec-06:

Startup Overlay
--------------------

``resource/startup/root.json`` is a standalone lightweight JSON UI root, loaded by ``system_core`` after the GUI runtime is created and before the Shell and app scan, and mounted to ``SystemTop`` through a transient screen to avoid a long blank screen during ESP-side file parsing, resource registration, and Shell initialization. The default screen path is ``/startup``, and the root file path can be changed via Super Kconfig or PC CMake config.

.. _system-super-assets-sec-07:

Background Screens
--------------------

The background flow is the bottom background state machine in the Shell root. ``/background`` is the Shell desktop image background, and ``/app_background`` is the solid background when a regular app is in front. At runtime the Shell manifest mounts the ``background`` flow to ``SystemBottom``: it switches to ``/background`` when the Shell is in front and to ``/app_background`` when a regular app is in front. Content pages still mount to the app layer and the overlay still mounts to the top of the ``AppTop`` stack, so there is no need to repeat the background node in each content screen.

.. _system-super-assets-sec-08:

Keyboard Input
--------------------

Keyboard input is the on-demand transient screen ``/keyboard_input`` in the Shell root, not part of any Shell screenFlow. When an app requests the system keyboard, ShellApp shows it through a transient mount to the top of the ``AppTop`` stack with a higher z-order than the overlay. The screen contains a full-screen mask, a single-line ``textInput``, and a ``keyboard``; ``abc`` / ``ABC`` / ``123`` / ``,.?!``, confirm, and cancel are embedded in the keyboard key layout. The keyboard layout is described by ``keyboardProps.layouts`` for the ``text`` / ``upper`` / ``number`` / ``special`` layouts, mapped by the backend to the LVGL keyboard map, with disallowed mode switch keys disabled by ``allowedModes``. Key actions include ``super.keyboard.text_changed``, ``super.keyboard.submit.*``, ``super.keyboard.cancel.*``, and ``super.keyboard.toggle_password``.

.. _system-super-assets-sec-09:

Modification Notes
--------------------

If you change a JSON node id, template id, screen path, or action string, you must sync the corresponding constant in ``src/private/system_constants.hpp``.
