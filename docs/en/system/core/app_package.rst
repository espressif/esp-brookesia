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
- The runtime GUI startup flow must be written in ``screen_flows[]`` of ``profile.json``.

.. _system-core-app_package-sec-07:

Packaging Tool
--------------------

The repository provides ``tools/app_pack.py`` to pack an unpacked package into a ``.bpk`` and to validate ``manifest.json`` fields. When changing the package specification, keep the schema, tool validation, system_core parser, and example resources consistent.
