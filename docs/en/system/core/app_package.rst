.. _system-core-app_package-sec-00:

App Package
====================

:link_to_translation:`zh_CN:[中文]`

This page describes the structure of a runtime app package, its manifest fields, the resource descriptor, and the scan and install rules.

.. _system-core-app_package-sec-01:

Overview
--------------------

An app package uses ``manifest.json`` to describe package metadata and the runtime entry; the ``profile.json`` under the runtime resource directory is the system_core resource descriptor, whose ``root`` points to the real JSON UI document and whose startup flow is written in ``screen_flows[]``. It has two usage forms:

- ``.bpk``: a ZIP container, unpacked at runtime by ``system::core::unpack_app_package_to()``.
- unpacked package: deploy ``manifest.json + app/ + res/`` directly, scanned and installed by ``System::init()``.

A package can use either the LVGL or the GSP GUI backend. See ``gui`` under Manifest Fields and the Resource Descriptor section for how a GSP package differs.

To run a ``.bpk``, first call the system_core package API (optionally ``verify_app_package_release()``, then ``unpack_app_package_to()``) to obtain an ``AppManifest``, then convert it with ``make_runtime_app_config()`` into a ``runtime::AppConfig``.

.. _system-core-app_package-sec-02:

Minimal Package Structure
-------------------------

.. code-block:: text

   brookesia.app.demo/
     manifest.json
     app/
       app.lua
     res/
       profile.json
       root.json

Example ``manifest.json``:

.. code-block:: json

   {
     "package": {
       "id": "brookesia.app.demo",
       "name": {"en": "Demo", "zh_CN": "演示"},
       "version": "0.1.0",
       "visible": true,
       "systems": ["core", "super"]
     },
     "runtime": {
       "type": "Lua",
       "entry": "app/app.lua",
       "resource_dir": "res",
       "arguments": []
     },
     "services": [
       {
         "name": "Storage",
         "version": "0.8.1"
       }
     ]
   }

.. _system-core-app_package-sec-03:

Manifest Fields
--------------------

The ``package`` object:

.. list-table::
   :header-rows: 1

   * - Field
     - Description
   * - ``id``
     - Unique app manifest id, required
   * - ``name``
     - Localized display name object; ``AppManifest.name`` falls back to ``en`` / first name / id
   * - ``version``
     - App version, required
   * - ``visible``
     - Whether it appears in the launcher, defaults to ``true``
   * - ``systems``
     - Supported system type list, no restriction by default

The ``runtime`` object:

.. list-table::
   :header-rows: 1

   * - Field
     - Description
   * - ``type``
     - Runtime backend type, for example ``Lua`` or ``JavaScript``, required
   * - ``entry``
     - Relative path of the entry script, required
   * - ``resource_dir``
     - Relative path of the app resource directory
   * - ``arguments``
     - String array of runtime startup arguments

The optional top-level ``gui`` object selects the GUI backend. It is independent of the runtime in ``runtime.type``, so a JavaScript app can use either GUI backend:

.. list-table::
   :header-rows: 1

   * - Field
     - Description
   * - ``backend``
     - ``lvgl`` (default) or ``gsp``. Leave ``gui`` out for an existing JSON UI package. A GSP package sets ``"backend": "gsp"``

A package installs only on firmware that uses the same GUI backend. A GSP package cannot be installed on LVGL firmware, and a JSON UI package cannot be installed on GSP firmware.

The optional top-level ``services`` array declares the service RPCs required by a runtime app:

.. list-table::
   :header-rows: 1

   * - Field
     - Description
   * - ``name``
     - Stable, case-sensitive RPC name, for example ``Storage``; use the exact value returned by the service helper's ``get_name()``
   * - ``version``
     - Minimum compatible service version in numeric ``MAJOR.MINOR.PATCH`` format

A local service version is compatible only when its major and minor versions equal the declared values and its patch version is greater than or equal to the declared patch. For example, local versions ``0.8.2`` and ``0.8.3`` satisfy a requirement for ``0.8.1``, while ``0.9.0`` does not. All declared services are required dependencies.

The ``component`` field may be present as metadata for external tools, but system_core silently ignores it: the field is not parsed, validated, stored, reported, or used for matching. Core service matching uses only ``name`` and ``version``.

When installing a ``.bpk`` or starting an app, system_core checks every declared service for registration and version compatibility. An undeclared service cannot be discovered before the app tries to use it, so the HostBridge also enforces declarations at call time. ``service_available()`` returns ``false`` for an undeclared service, and other named service operations return an error without invoking the service.

The exact RPC names ``SystemCore``, ``SystemGui``, and ``SystemTimer`` are implicitly allowed and do not need declarations. Matching is case-sensitive; variants such as ``systemcore`` are not implicitly allowed. A missing ``services`` field or an empty array means that a runtime app can call only these three implicit system RPCs.

This enforcement applies to runtime apps that access services through the HostBridge. Native apps are trusted firmware code and their direct C++ service access is not restricted by the manifest.

A ``.bpk`` is unpacked and validated under ``apps/.installing/`` before anything installed is touched. When it replaces an installed app, the old directory is moved to ``previous/`` in that staging folder, its ``cache``, ``data`` and ``files`` are carried into the new copy, and the old directory is deleted only after the new version registers and the update is committed: a temporary ``committing`` file is written and then renamed to ``committed``. If registration fails, or the commit fails and ``committed`` does not exist, the new copy is moved aside and the old version is moved back and registered again. At boot, a staging folder with ``previous/`` but no ``committed`` file is treated as an unfinished update and the old version is restored the same way; ``previous/`` is removed only when ``committed`` exists and the new copy is in place. If any of these files cannot be checked or moved, the staging folder is kept untouched. Other leftover staging folders are removed, and an installed app that fails to load is skipped with a warning instead of stopping system start. Uninstalling removes the app directory, including its data.

.. _system-core-app_package-sec-04:

Resource Descriptor
--------------------

``<runtime.resource_dir>/profile.json`` is the system_core resource descriptor, not a JSON UI document:

.. code-block:: json

   {
     "icon_id": "demo",
     "root": "root.json",
     "screen_flows": [
       {"screen_flow": "main", "layer": "AppDefault", "mount_mode": "Replace", "z_order": 0}
     ]
   }

``icon_id`` locates the launcher icon within the package; ``root`` points to a standard JSON UI document; ``screen_flows[]`` specifies the startup flow. A regular app main screen should use ``AppDefault``, and ``AppTop`` is only for an app's own header, floating layer, or toolbar; a runtime app can only use ``Replace`` with ``z_order`` within ``0..100``. ``root.json`` is a standard JSON UI document and should reference a ``screenFlow`` asset.

A GSP package uses a different ``profile.json``. ``root`` names a compiled ``.gspb`` bundle, and pages are GSP scenes inside it, so ``screen_flows`` and ``preload_dom`` are not used:

.. code-block:: json

   {
     "gui_backend": "gsp",
     "root": "app.gspb"
   }

``manifest.gui.backend`` must also be ``gsp``. ``examples/app/gsp_js_device_control`` is a complete GSP package.

A bundle that holds a copy of its scenes for each system theme can name the first scene for each theme, so the app does not flash the wrong theme while its JavaScript starts. A theme that is not listed starts on scene 0:

.. code-block:: json

   {
     "gui_backend": "gsp",
     "root": "app.gspb",
     "start_scenes": {"light": 0, "dark": 12}
   }

JavaScript updates the page with ``brookesia.gsp_set_text(name, text)``, ``brookesia.gsp_set_value(name, value)``, ``brookesia.gsp_set_property(name, property, value)`` (a number, a boolean, or a ``#RRGGBB`` color, for the properties that the scene declares such as ``visible``, ``enabled``, ``color``, or a clock hand angle) and ``brookesia.gsp_show_page(scene)``. A burst of updates waits briefly when the GSP command queue is full.

.. _system-core-app_package-sec-05:

Scanning and Install
--------------------

When ``install_package_apps = true``, ``System::init()`` scans the ``apps`` directories of internal and external storage in the layout and loads each first-level directory that contains ``manifest.json``, sorted by directory path. If the same manifest id appears under multiple volumes, internal and preferred external take priority, and later duplicates are skipped with a warning.

During install, the runtime root ``icon_id`` is used to find a matching ``imageSet`` descriptor in the resource directory; once found, it is registered as a runtime-global image resource and fills ``AppManifest.icon_path`` so the launcher can show the icon.

.. _system-core-app_package-sec-06:

Safety Constraints
--------------------

- ``runtime.entry`` and ``runtime.resource_dir`` must be safe relative paths, not absolute and not containing ``..``.
- A package whose ``package.systems`` does not match the current system type is skipped.
- A missing or wrongly typed ``package.id``, ``package.version``, ``runtime.type``, or ``runtime.entry`` fails manifest parsing.
- An LVGL runtime GUI startup flow must be written in ``screen_flows[]`` of ``profile.json``.
- A GSP package must set both ``manifest.gui.backend`` and ``profile.json`` ``gui_backend`` to ``gsp``. A damaged bundle, or a backend that does not match the firmware, is rejected at install and leaves an already installed version in place.

.. _system-core-app_package-sec-07:

Packaging Tool
--------------------

The repository provides ``tools/app_pack.py`` to pack an unpacked package into a ``.bpk`` and to validate ``manifest.json`` fields. When changing the package specification, keep the schema, tool validation, system_core parser, and example resources consistent.
