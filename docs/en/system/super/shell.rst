.. _system-super-shell-sec-00:

Shell
====================

:link_to_translation:`zh_CN:[中文]`

The Shell is the built-in native app of ``system_super``, with the class name ``ShellApp``. It is the app container for the built-in system UI and contains three pages: Home Dashboard, App Launcher, and Notifications.

Overview
--------------------

The status bar and the bottom swipe-up gesture indicator to exit an app are also managed by ShellApp, but they mount to the top of the ``GuiLayer::Top`` stack with ``z_order = 101`` and are not covered by a regular app's replace screen.

The Shell also owns a background flow, mounted from the same Shell document to ``GuiLayer::Bottom``. When the Shell is in front it shows the ``/background`` image background, and when a regular app is in front it shows the ``/app_background`` solid background.

Manifest
--------------------

The current key fields of the Shell manifest:

.. list-table::
   :header-rows: 1

   * - Field
     - Value
   * - ``id``
     - ``brookesia.system.super.shell``
   * - ``name``
     - ``Shell``
   * - ``kind``
     - ``AppKind::Native``
   * - ``visible``
     - ``false``
   * - ``icon_id``
     - ``super``
   * - ``resource_dir``
     - ``BROOKESIA_SYSTEM_SUPER_RESOURCE_DIR``
   * - ``get_gui_descriptor().root``
     - ``shell/root.json``
   * - ``get_gui_descriptor().screen_flows``
     - ``background -> SystemBottom``, ``shell_pages -> AppDefault``, ``overlay -> AppTop + Stack + z_order 101``, ``keyboard -> AppTop + Stack + z_order 102``

``visible = false`` ensures the Shell itself does not appear in the launcher app list.

Pages
--------------------

The Shell app loads ``shell/root.json`` once, and that root aggregates 2 background screens and 3 content screens:

.. list-table::
   :header-rows: 1

   * - Page
     - Screen Path
   * - Desktop Background
     - ``/background``
   * - App Background
     - ``/app_background``
   * - Home Dashboard
     - ``/home``
   * - App Launcher
     - ``/launcher``
   * - Notifications
     - ``/notifications``

``/background`` and ``/app_background`` are mounted by the ``background`` flow to ``SystemBottom`` and switch exclusively with the foreground app state. The 3 content screens are managed by the ``shell_pages`` flow, with the state id using the screen id directly: the core auto-starts that flow to ``AppDefault`` before the Shell ``on_start()``, and navigation hotkeys trigger flow actions for exclusive switching. When a regular app is the active app, its default ``AppDefault`` screen naturally replaces the Shell page; when returning to the Shell, the system first closes the regular app and then lets the resident Shell trigger the target page transition.

Overlay
--------------------

The Shell root contains the ``/overlay`` screen. The ``overlay`` flow is mounted by the Shell manifest to ``AppTop`` with the ``Stack`` mount mode and ``z_order = 101`` to stay above the regular app top screen. ShellApp only updates bindings, subscribes to Display/Wi-Fi events, and runs animations. See :doc:`overlay` for the overlay nodes and behavior.

App Launcher
--------------------

When the App Launcher page starts, it shows ``/launcher`` and calls ``populate_launcher()`` to generate launcher buttons. ``populate_launcher()`` iterates ``owner_.list_apps()``:

- Skip apps with ``manifest.visible = false``.
- Create instances under ``/launcher/content/grid`` based on the ``launcher_app_button`` template.
- The instance id has the form ``app_<app_id>``.
- Set the instance label to the app name.
- If ``manifest.icon_id`` and ``manifest.icon_path`` are available, set the instance image icon to the global image resource registered under ``manifest.id``.
- When a real image icon is shown, hide the fallback first character; without a real image icon, show the first letter or digit of the app name as a fallback.
- Build the mapping from instance id to ``AppId``.
- Set ``/launcher/content/summary_badge/summary`` to the launchable app count.

Launch Action
--------------------

The launcher button root uses the single action ``super.launch.app``. The inner icon/label child nodes are not clickable, to avoid stealing the root button's press/click event; clicking the button blank area, the icon image, the fallback first character, or the app name all resolve the instance id by event path and then call ``owner_.start_app(app_id)``. The button root also plays a local press/release animation through JSON UI event effects. If a press triggers ``pressLost``, the subsequent ``clicked`` does not dispatch ``super.launch.app``, so sliding off the control does not accidentally launch an app.

Theme
--------------------

The Shell UI manages the light/dark styles through Runtime themes: ``resource/themes/light.json`` (theme id ``shell.light``) and ``resource/themes/dark.json`` (theme id ``shell.dark``). The Shell root loads ``constants/default.json``, and loads ``constants/1024x600.json`` for the matching resolution. These ``ui`` tokens only handle layout, sizing, and a few semantic colors; common visuals such as cards, titles, captions, the navigation bar, and the status bar are provided by named styles in the theme and referenced through ``styleRefs`` on JSON nodes.

The Shell also auto-loads all ``*.json`` Runtime themes in the first level of ``BROOKESIA_SYSTEM_SUPER_RESOURCE_DIR/themes`` at startup and sets the default theme to ``shell.light``. Themes are not written into ``shell/root.json.assets``; they are registered into the Runtime through a C++ native-only GUI API. The global light/dark switch is triggered by the Display page of the Settings app, and on switching the Runtime recomposes the style of loaded nodes and the Shell background updates through the theme variant.
