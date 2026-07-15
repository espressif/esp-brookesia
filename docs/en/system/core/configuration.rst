.. _system-core-configuration-sec-00:

Configuration
====================

:link_to_translation:`zh_CN:[中文]`

This page collects the Kconfig options, PC CMake config, macro defaults, and runtime storage configuration used by the current ``brookesia_system_core`` implementation.

.. _system-core-configuration-sec-01:

Kconfig
--------------------

ESP platform configuration lives in ``system/brookesia_system_core/Kconfig``.

.. list-table::
   :header-rows: 1

   * - Option
     - Default
     - Description
   * - ``BROOKESIA_SYSTEM_CORE_RUNTIME_APP_MAX_Z_ORDER``
     - ``100``
     - Maximum ``z_order`` a runtime app manifest may request
   * - ``BROOKESIA_SYSTEM_CORE_RUNTIME_ASYNC_DEBUG_PROBE``
     - ``n``
     - Register the runtime async timeout debug probe
   * - ``BROOKESIA_SYSTEM_CORE_ENABLE_DEBUG_LOG``
     - ``n``
     - Master switch for core component debug log
   * - ``BROOKESIA_SYSTEM_CORE_SYSTEM_ENABLE_DEBUG_LOG``
     - ``y``
     - Enable system module debug log once the master switch is on
   * - ``BROOKESIA_SYSTEM_CORE_SERVICE_ENABLE_DEBUG_LOG``
     - ``y``
     - Enable service module debug log once the master switch is on
   * - ``BROOKESIA_SYSTEM_CORE_WORKER_NAME_PREFIX``
     - ``System``
     - System task scheduler worker thread name prefix
   * - ``BROOKESIA_SYSTEM_CORE_WORKER_PRIORITY``
     - ``10``
     - System task scheduler worker priority
   * - ``BROOKESIA_SYSTEM_CORE_WORKER_STACK_SIZE``
     - ``65536``
     - System task scheduler worker stack size
   * - ``BROOKESIA_SYSTEM_CORE_WORKER_STACK_IN_EXT``
     - ``y if SPIRAM``
     - Whether to place the worker stack in external memory
   * - ``BROOKESIA_SYSTEM_CORE_WORKER_POLL_INTERVAL_MS``
     - ``5``
     - Worker poll interval
   * - ``BROOKESIA_SYSTEM_CORE_WORKER_NUM``
     - ``3``
     - Worker count, range ``1..4``
   * - ``BROOKESIA_SYSTEM_CORE_WORKER_0_CORE_ID``
     - ``0``
     - Worker 0 core affinity; ``-1`` means unspecified
   * - ``BROOKESIA_SYSTEM_CORE_WORKER_1_CORE_ID``
     - ``1``
     - Worker 1 core affinity; ``-1`` means unspecified
   * - ``BROOKESIA_SYSTEM_CORE_WORKER_2_CORE_ID``
     - ``-1``
     - Worker 2 core affinity; ``-1`` means unspecified
   * - ``BROOKESIA_SYSTEM_CORE_WORKER_3_CORE_ID``
     - ``-1``
     - Worker 3 core affinity; ``-1`` means unspecified

System no longer specifies a legacy resource root through Kconfig. At runtime it enumerates storage filesystems from the Device service and builds an Android-like storage layout.

.. _system-core-configuration-sec-02:

Host Platform Config
--------------------

PC platform configuration lives in ``system/brookesia_system_core/cmake/pc_platform.cmake``.

.. list-table::
   :header-rows: 1

   * - CMake Variable
     - Default
     - Description
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_ENABLE_DEBUG_LOG``
     - ``OFF``
     - PC debug log master switch
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_SYSTEM_ENABLE_DEBUG_LOG``
     - ``ON``
     - PC system debug log default
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_SERVICE_ENABLE_DEBUG_LOG``
     - ``ON``
     - PC service debug log default
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_RUNTIME_ASYNC_DEBUG_PROBE``
     - ``OFF``
     - PC runtime async timeout debug probe default
   * - ``BROOKESIA_SYSTEM_CORE_PC_STAGE_INTERNAL_ROOT``
     - ``${CMAKE_BINARY_DIR}/brookesia``
     - PC staged internal storage root
   * - ``BROOKESIA_SYSTEM_CORE_PC_STAGE_APP_ROOT``
     - ``.../brookesia/apps``
     - PC build-time app staging directory
   * - ``BROOKESIA_SYSTEM_CORE_PC_STAGE_SYSTEM_ROOT``
     - ``.../brookesia/system``
     - PC build-time system resource staging directory
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_RUNTIME_APP_MAX_Z_ORDER``
     - ``100``
     - Maximum ``z_order`` a PC runtime app manifest may request
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_WORKER_NAME_PREFIX``
     - ``System``
     - PC worker thread name prefix
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_WORKER_PRIORITY``
     - ``10``
     - PC worker priority
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_WORKER_STACK_SIZE``
     - ``32768``
     - PC worker stack size
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_WORKER_STACK_IN_EXT``
     - ``OFF``
     - Whether the PC worker uses an external memory stack
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_WORKER_POLL_INTERVAL_MS``
     - ``2``
     - PC worker poll interval
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_WORKER_NUM``
     - ``3``
     - PC worker count
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_WORKER_0_CORE_ID``
     - ``0``
     - PC Worker 0 core affinity
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_WORKER_1_CORE_ID``
     - ``1``
     - PC Worker 1 core affinity
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_WORKER_2_CORE_ID``
     - ``-1``
     - PC Worker 2 core affinity
   * - ``BROOKESIA_SYSTEM_CORE_PC_CONFIG_WORKER_3_CORE_ID``
     - ``-1``
     - PC Worker 3 core affinity

The ``PC_STAGE_*`` variables are only for build-time staging. The runtime storage layout is still determined by the Device service or by ``System::Config::storage`` overrides.

.. _system-core-configuration-sec-03:

Macro Configs
--------------------

The public macros live in ``include/brookesia/system_core/macro_configs.h``.

.. list-table::
   :header-rows: 1

   * - Macro
     - Current Value or Source
     - Description
   * - ``BROOKESIA_SYSTEM_CORE_LOG_TAG``
     - ``"SysCore"``
     - Log tag
   * - ``BROOKESIA_SYSTEM_CORE_SERVICE_NAME``
     - ``"SystemCore"``
     - System service name
   * - ``BROOKESIA_SYSTEM_CORE_GUI_SERVICE_NAME``
     - ``"SystemGui"``
     - GUI service name
   * - ``BROOKESIA_SYSTEM_CORE_TIMER_SERVICE_NAME``
     - ``"SystemTimer"``
     - Timer service name
   * - ``BROOKESIA_SYSTEM_CORE_DEFAULT_SYSTEM_TYPE``
     - ``"core"``
     - Default system type
   * - ``BROOKESIA_SYSTEM_CORE_DEFAULT_SCREEN_PATH``
     - ``"/root"``
     - Default app screen path
   * - ``BROOKESIA_SYSTEM_CORE_RUNTIME_ASYNC_DEBUG_PROBE``
     - Kconfig/PC config/default
     - Whether to register the runtime async timeout debug probe
   * - ``BROOKESIA_SYSTEM_CORE_RUNTIME_APP_MAX_Z_ORDER``
     - Kconfig/PC config/default
     - Maximum ``z_order`` a runtime app manifest may request
   * - ``BROOKESIA_SYSTEM_CORE_WORKER_*``
     - Kconfig/PC config/default
     - System task scheduler worker configuration

.. _system-core-configuration-sec-04:

Storage Layout
--------------------

During System init, the Device service is queried for all storage filesystems, producing one internal volume and zero or more external volumes.

The internal auto-selection priority is:

1. Flash + LittleFS
2. Flash + FATFS
3. SDCard + FATFS
4. Other LittleFS
5. Other FATFS

External includes all SDCard storage volumes. ``System::Config::storage`` can override the internal, external, preferred external id, and default install target.

System automatically creates the following fixed directories:

- internal/external: ``apps``, ``music``, ``download``, ``movies``, ``pictures``, ``documents``
- internal only: ``system``
- per app: ``apps/<manifest_id>/cache``, ``apps/<manifest_id>/data``, ``apps/<manifest_id>/files``

The runtime app scan directories are ``apps`` under internal and all external volumes. Duplicate manifest ids are deduplicated in the order internal, preferred external, then other external.

The default runtime app install target is ``Auto``: prefer the preferred external, then the first external, then fall back to internal. A native app can change the default via ``SystemApi::set_default_install_storage()``; System saves it to Storage KV and restores it on the next startup.

.. _system-core-configuration-sec-05:

System::Config
--------------------

``System::Config`` is the runtime configuration entry:

- ``gui_backend``: the GUI backend. No GUI runtime is created when empty.
- ``environment``: the screen environment passed to the JSON UI parser/runtime.
- ``storage``: overrides the internal/external volume, preferred external id, and default install target.
- ``system_type``: defaults to ``"core"``.
- ``start_service_manager``: whether to start ``ServiceManager`` during init.
- ``install_registered_apps``: whether to install apps from the ``IAppProvider`` registry.
- ``install_package_apps``: whether to scan the ``apps`` directory of the storage layout and install runtime apps.
- ``enable_gui_view_debug``: whether to enable the view debug outline after creating the GUI runtime; off by default.
- ``enable_gui_live_preview``: whether to auto-enable live preview for file-backed GUI documents; off by default.
- ``gui_live_preview_options``: options passed to ``gui::Runtime::enable_live_preview()``.
- ``gui_live_preview_poll_interval_ms``: the period of the internal live preview poll task; defaults to ``100ms``.
- ``startup_overlay``: an optional system startup overlay; shown after the GUI runtime is created and before app install and Shell startup, hidden after ``start()`` completes.
- ``app_launch_transition``: an optional app launch transition; shown before app GUI resource registration and document loading, with its origin set by ``start_app(app_id, AppStartOptions{.launch_origin_frame = ...})``.

.. _system-core-configuration-sec-06:

Directory and File Interface
----------------------------

A native app can access the following through ``AppContext::system_service()``:

.. code-block:: cpp

   auto layout = context.system_service().get_storage_layout();
   auto app_dirs = context.system_service().get_app_storage_paths();
   auto public_dirs = context.system_service().get_public_storage_paths();

A runtime app uses the equivalent JSON API through the ``SystemCore`` service. File operations must pass a ``StoragePathKind``, a volume id, and a relative path; absolute paths and ``..`` are rejected and cannot escape into another app's directory.
