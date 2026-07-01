.. _system-core-app_model-sec-00:

App Model
====================

:link_to_translation:`zh_CN:[中文]`

This page describes the base app types in System Core, the native and runtime integration paths, and the full lifecycle state transitions.

Base Types
--------------------

.. list-table::
   :header-rows: 1

   * - Type
     - Description
   * - ``AppId``
     - The runtime app id assigned by the core; ``INVALID_APP_ID`` is ``0``
   * - ``TimerId``
     - The app timer id assigned by the core; ``INVALID_TIMER_ID`` is ``0``
   * - ``AppKind``
     - App source, currently ``Native`` or ``Runtime``
   * - ``AppState``
     - App state, currently ``Installed``, ``Starting``, ``Running``, ``Paused``, ``Stopping``, ``Stopped``, ``Error``
   * - ``GuiRootKind``
     - GUI root source, currently ``None``, ``File``, ``JsonString``
   * - ``AppManifest``
     - App install, display, and runtime information
   * - ``AppGuiDescriptor``
     - GUI root, startup screen flows, and GUI resources
   * - ``AppInfo``
     - ``AppId``, ``AppManifest``, state, and last error

App Manifest
--------------------

.. list-table::
   :header-rows: 1

   * - Field
     - Description
   * - ``id``
     - Unique app manifest id
   * - ``name``
     - Current default display name
   * - ``localized_names``
     - Localized display names, keyed by language
   * - ``version``
     - App version, defaults to ``"0.1.0"``
   * - ``kind``
     - ``AppKind::Native`` or ``AppKind::Runtime``
   * - ``visible``
     - Whether it appears in app lists such as the launcher
   * - ``icon_id``
     - Display icon image id; for a runtime package, filled from the runtime root ``icon_id``
   * - ``supported_systems``
     - Supported system type list; an empty array means no restriction
   * - ``icon_path``
     - App icon ``imageSet`` descriptor path; for a runtime package, auto-discovered from the runtime root ``icon_id`` at install
   * - ``runtime_type``
     - Runtime backend type
   * - ``app_path``
     - Runtime package root directory
   * - ``entry``
     - Runtime entry file
   * - ``resource_dir``
     - App resource directory
   * - ``arguments``
     - Runtime startup arguments

Native App Versus Runtime App
-----------------------------

.. list-table::
   :header-rows: 1

   * - Dimension
     - Native App
     - Runtime App
   * - Integration
     - C++ subclass of ``IApp``
     - Unpacked package manifest + scripted convention functions
   * - ``AppKind``
     - ``Native``
     - ``Runtime``
   * - Install source
     - ``install_app()`` or the ``IAppProvider`` registry
     - ``install_runtime_app()`` or app path scanning
   * - Lifecycle
     - C++ virtual functions
     - ``brookesia_app.*`` functions
   * - GUI operations
     - ``AppContext::gui()``
     - ``SystemGui`` service
   * - Timer
     - ``AppContext::timer()``
     - ``SystemTimer`` service
   * - Permission
     - Currently treated as a trusted built-in app
     - Regular app, can only operate its own resources

App Lifecycle
--------------------

Install. Native app: ``install_app()`` reads ``IApp::get_manifest()``, the core forces ``manifest.kind = AppKind::Native``, reads ``get_gui_descriptor()`` and validates the startup screen flow target, allocates an ``AppId`` and creates an ``AppContext``, calls ``IApp::on_install()``, writes the app table, and calls ``on_app_installed()``. Runtime app: ``install_runtime_app()`` receives an ``AppManifest``, the core forces ``manifest.kind = AppKind::Runtime``, reads the resource descriptor from ``<runtime.resource_dir>/profile.json`` and validates ``screen_flows[]``, allocates an ``AppId`` and creates an ``AppContext``, writes the app table, and calls ``on_app_installed()``.

The ``start_app()`` order:

1. Set the state to ``Starting``.
2. A native app auto-registers the resources in the GUI descriptor.
3. Load the app-owned GUI document according to the GUI descriptor.
4. If the descriptor configures ``screen_flows[]``, the core auto-starts the corresponding flow and mounts ``initial`` to the logical layer.
5. A native app calls ``IApp::on_start()``.
6. A runtime app calls ``load_app()``, then ``start_app()``, then ``brookesia_app.on_start()``.
7. On success the state becomes ``Running``, the active app is updated, and ``on_app_started()`` is called.

If resource registration, GUI loading, or a lifecycle step fails, the core unloads the loaded GUI, unregisters the registered resources, and sets the state to ``Error``.

The ``stop_app()`` order:

1. Set the state to ``Stopping``.
2. Cancel all timers of the app and clear pending timers.
3. A native app calls ``IApp::on_stop()``; a runtime app calls ``brookesia_app.on_stop()`` and then stops the runtime app.
4. Disconnect actions, stop the app-owned running screen flow, and unload the GUI document.
5. Unregister app image/font resources.
6. On success the state becomes ``Stopped``; if it was the active app the active app is cleared, and ``on_app_stopped()`` is called.

``uninstall_app()`` first stops a ``Running`` or ``Paused`` app, then calls ``on_uninstall()`` or ``brookesia_app.on_uninstall()``, cancels timers, unloads the runtime, unloads the GUI document, unregisters GUI resources, and removes it from the app table and manifest id index.

``pause_app()`` calls ``on_pause()`` and the state becomes ``Paused``; ``resume_app()`` calls ``on_resume()`` and the state becomes ``Running`` again and updates the active app. The current implementation does not auto-unmount the GUI or stop timers on pause; an app should handle special needs in its callbacks.

Native App Integration
----------------------

A native app integrates by subclassing ``core::IApp`` in C++, usually including the compatibility entry ``brookesia/system_core/app.hpp``. The current ``IApp`` interface:

.. list-table::
   :header-rows: 1

   * - Function
     - Description
   * - ``get_manifest()``
     - Required; returns the app manifest
   * - ``get_gui_descriptor()``
     - Optional; returns the GUI root, startup screen flows, and image/font resources
   * - ``on_install()`` / ``on_uninstall()``
     - Install/uninstall callbacks; succeed by default
   * - ``on_start()`` / ``on_stop()``
     - Start/stop callbacks; succeed by default
   * - ``on_pause()`` / ``on_resume()``
     - Pause/resume callbacks; succeed by default
   * - ``on_action()``
     - Called when a GUI action is dispatched; succeeds by default
   * - ``on_timer()``
     - Called when an app timer fires; succeeds by default

``start_app()`` auto-registers the resources returned by ``get_gui_descriptor()`` and loads the GUI document before ``on_start()``; if ``screen_flows[]`` is configured, it also auto-mounts the ``initial`` screen of that flow. ``on_start()`` is a no-op success by default; a visible UI app usually overrides it to initialize text, bindings, actions, timers, and other business state.

``AppContext`` provides ``app_id()``, ``system_service()``, ``timer()``, and ``gui()``. ``system_service()`` returns ``SystemApi``; a native app is currently treated as a trusted built-in app and can start/stop any app, read the storage layout, access its own ``cache/data/files`` and public directories, and adjust the runtime app default install target.

``IAppProvider`` registers apps linked into the project at compile time: ``get_manifest()`` returns the provider's manifest, and ``create_app()`` returns a native app instance (when empty, it is installed as a runtime manifest). Registration macro:

.. code-block:: cpp

   BROOKESIA_SYSTEM_CORE_APP_PROVIDER_REGISTER_WITH_SYMBOL(ProviderType, name, symbol_name)

``System::install_registered_apps()`` iterates the ``AppProviderRegistry`` and installs the apps the providers supply.

Runtime App Integration
-----------------------

A runtime app integrates through an unpacked package and scripted convention functions and does not include C++ headers. When ``install_package_apps = true``, ``System::init()`` scans the ``apps`` directories under internal and external storage in the layout:

.. code-block:: text

   <storage-root>/apps/
     brookesia.app.calculator/
       manifest.json
       app/
         app.lua
       res/
         profile.json
         root.json

The core scans the first-level directories that contain ``manifest.json`` under each app root and loads them sorted by directory path. ``manifest.json`` is parsed by ``read_unpacked_app_manifest()``, with this mapping:

.. list-table::
   :header-rows: 1

   * - Package Field
     - Maps To
   * - ``package.id``
     - ``AppManifest.id``
   * - ``package.name``
     - ``AppManifest.localized_names``; ``AppManifest.name`` falls back to ``en`` / first name / id
   * - ``package.version``
     - ``AppManifest.version``
   * - ``package.visible``
     - ``AppManifest.visible``, defaults to ``true``
   * - ``package.systems``
     - ``AppManifest.supported_systems``, defaults to no system type restriction
   * - ``runtime.type``
     - ``AppManifest.runtime_type``
   * - ``runtime.entry``
     - ``AppManifest.entry``
   * - ``runtime.resource_dir``
     - ``AppManifest.resource_dir``
   * - ``runtime.arguments``
     - ``AppManifest.arguments``

``runtime.entry`` and ``runtime.resource_dir`` must be safe relative paths, not absolute and not containing ``..``; a package whose ``package.systems`` does not match the current system type is skipped. The script entry must create a global table ``brookesia_app``, and the core calls the convention functions through the runtime:

- ``brookesia_app.on_install()`` / ``brookesia_app.on_uninstall()``
- ``brookesia_app.on_start()`` / ``brookesia_app.on_stop()``
- ``brookesia_app.on_pause()`` / ``brookesia_app.on_resume()``
- ``brookesia_app.on_action(action)``
- ``brookesia_app.on_timer(timer_id, name)``

A missing function is treated as a no-op; a function that returns ``false`` or throws fails the corresponding lifecycle step, and the app may enter ``Error``. The runtime app GUI document comes from the ``root`` of ``<runtime.resource_dir>/profile.json``, and the startup flow is written in ``screen_flows[]``; see :doc:`app_package`.

Related Documents
--------------------

- :doc:`gui`
- :doc:`services`
- :doc:`resources_permissions`
- :doc:`app_package`
