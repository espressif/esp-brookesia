.. _system-core-resources_permissions-sec-00:

Resources and Permissions
=========================

:link_to_translation:`zh_CN:[中文]`

This page describes the automatic registration rules for GUI resources and the current permission boundaries of System Core based on ``AppManifest.kind``.

.. _system-core-resources_permissions-sec-01:

GUI Resource Registration
-------------------------

System Core supports two kinds of GUI resources: a native app auto-registers image/font through ``AppGuiDescriptor.resources`` during ``start_app()`` and unregisters them on ``stop_app()``, ``uninstall_app()``, or ``deinit()``; a runtime package auto-discovers and registers the launcher icon image from the runtime root ``icon_id`` during install and unregisters it on ``uninstall_app()`` or ``deinit()``.

.. _system-core-resources_permissions-sec-02:

Resource Declaration
--------------------

A native app can override ``get_gui_descriptor()``, where ``resources`` contains ``std::vector<gui::RuntimeImageResource> images`` and ``std::vector<gui::RuntimeFontResource> fonts``. These resources are registered into the global runtime resource table of ``gui::Runtime`` for ``${image.xxx}`` and ``${font.xxx}`` references in JSON UI.

.. _system-core-resources_permissions-sec-03:

Registration Timing
--------------------

The ``start_app()`` order is: register app GUI resources first, then load the GUI document, then call the app ``on_start()``. This guarantees that JSON UI can resolve the app-provided runtime image/font ids while parsing the document.

If an image ``primary_src`` points to an LVGL ``.bin`` file, ``gui::Runtime`` notifies the backend to preload that file when loading the GUI document. Preloading happens before node creation, so a missing file or invalid header for ``${image.xxx}`` fails app start and enters the error path, instead of surfacing on the first display or binding flush.

On-demand formats such as PNG are not preloaded by default; they enter the backend decoded cache only when ``imageSet.images[].preload: true`` is set or when the app explicitly requests it by image id through ``SystemGui.PreloadImages`` / ``context.gui().preload_images(...)``. A runtime app can only pass image ids visible to its own document, not arbitrary file paths.

.. _system-core-resources_permissions-sec-04:

Unregistration Timing
---------------------

The ``stop_app()`` order is: call the app ``on_stop()`` first, then unload the GUI document, then unregister app GUI resources. ``uninstall_app()`` and ``deinit()`` also cancel timers, unload the GUI/runtime, and unregister resources as a fallback.

Unregistering an image releases the backend preload reference to that resource; if the same path is still referenced by another document or resource, the path cache of the LVGL backend is retained by reference counting.

.. _system-core-resources_permissions-sec-05:

Conflict Policy
--------------------

The current v1 requires app resource ids to be globally unique: an app returning a duplicate image id or font id fails to start; another app already owning the same image/font id also fails to start; on a registration failure, the core rolls back the registered resources and sets the app state to ``Error``. Future resource sharing would require reference counting or a system public resource domain; the current implementation does not share.

.. _system-core-resources_permissions-sec-06:

Runtime Package File Resources
------------------------------

A runtime app prefers the file resources in its package, the ``<runtime.resource_dir>/profile.json`` descriptor, and the JSON UI document referenced by ``root``. A regular runtime app cannot register native LVGL image/font resources or operate the runtime global resource table through a service.

The ``imageSet`` JSON in a runtime package can also reference ``.bin`` files; such files are likewise preloaded during the document load stage. If a PNG entry declares ``preload: true``, it is also decoded and cached during document load.

.. _system-core-resources_permissions-sec-07:

Launcher Icon
--------------------

A runtime package can declare the display icon id ``icon_id`` in ``<runtime.resource_dir>/profile.json``. ``icon_id`` is only an image id; the runtime manifest does not contain the icon descriptor path, so System Core looks in the package resource directory for an ``imageSet`` descriptor JSON whose ``images[]`` contains an ``id`` equal to the runtime root ``icon_id``, for example ``<runtime.resource_dir>/images/*.json`` or ``<app_path>/images/*.json``. Once found, that image is registered as a runtime-global image resource and the internal ``AppManifest.icon_path`` is filled for the launcher; when no descriptor matches, the launcher should use a text or placeholder fallback.

.. _system-core-resources_permissions-sec-08:

Permission Model
--------------------

System Core currently uses ``AppManifest.kind`` to distinguish base permissions.

.. _system-core-resources_permissions-sec-09:

Native App
--------------------

``AppKind::Native`` denotes a C++ ``IApp`` app, treated as a trusted built-in app in the current MVP. A native app can:

- Query system information through ``SystemApi``.
- Start or stop other apps through ``SystemApi::start_app()`` and ``SystemApi::stop_app()``.
- Update the runtime app default install target through ``SystemApi::set_default_install_storage()``.
- Access its own app directories and public directories through the storage API.
- Operate its own document through the app-owned GUI wrapper.
- Load, mount, and unload system-owned documents through the native-only GUI API.
- Create periodic or delayed timers through ``AppTimerRuntime``.

.. _system-core-resources_permissions-sec-10:

Runtime App
--------------------

``AppKind::Runtime`` denotes a scripted runtime app such as Lua or JavaScript, treated as a regular app. A runtime app can call ``SystemCore.GetSystemType``, ``GetEnvironment``, ``GetActiveApp``, ``ListApps``, request to close itself through ``SystemCore.RequestCloseApp``, operate its own GUI document through ``SystemGui``, and manage its own periodic timer through ``SystemTimer``. A runtime app cannot:

- Specify a GUI ``DocumentId``, layer, or mount target.
- Call the load/unload GUI document service.
- Start or stop other apps through a service.
- Change the default install target or access another app's private directory.
- Register native image/font runtime resources.

.. _system-core-resources_permissions-sec-11:

Caller Owner Resolution
-----------------------

When a service handles a runtime call, it queries the current runtime app id through ``runtime::RuntimeFunctionBridge::get_current_app_id()`` and maps it to the core ``AppId``. If no owner is found, the service call fails. This ensures ``SystemGui`` and ``SystemTimer`` operations belong to the calling app.
