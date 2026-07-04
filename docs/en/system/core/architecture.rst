.. _system-core-architecture-sec-00:

Architecture
====================

:link_to_translation:`zh_CN:[中文]`

This page describes the main objects, startup flow, and internal collaboration of ``brookesia_system_core``.

.. _system-core-architecture-sec-01:

Entry Object
--------------------

The core entry is ``esp_brookesia::system::core::System``, which exposes:

- System lifecycle: ``init()``, ``start()``, ``stop()``, ``deinit()``.
- App management: ``install_app()``, ``install_runtime_app()``, ``start_app()``, ``stop_app()``, ``uninstall_app()``.
- App-owned GUI operations: ``gui_set_text()``, ``gui_trigger_screen_flow()``, and so on.
- Timer operations: ``timer_start_periodic()``, ``timer_start_delayed()``, ``timer_stop()``.
- Runtime caller owner lookup: ``get_current_runtime_app_owner()``.

Internal state lives in ``System::Impl``, and the public header exposes only the stable interface. The internal records and shared scheduling helpers of ``System::Impl`` live in ``src/private/system/impl.hpp``, while behavior is implemented per module under the ``system/``, ``app/``, ``service/``, and ``runtime/`` directories.

.. _system-core-architecture-sec-02:

Main Flow
--------------------

``System::init()`` creates and connects the base runtime environment:

1. Set ``system_type`` and ``gui::Environment``.
2. Create and start ``lib_utils::TaskScheduler`` for internal app, GUI, and timer scheduling.
3. If ``gui_backend`` is configured, create ``gui::Runtime``.
4. Create ``SystemHostBridge`` and ``runtime::Runtime``.
5. Initialize ``service::ServiceManager``.
6. Register and bind ``SystemCore``, ``SystemGui``, ``SystemTimer``.
7. Call the derived-class extension point ``on_init()``.
8. Install registered native apps and package apps according to configuration.

``System::start()`` calls ``on_start()`` and enters the started state; ``System::stop()`` calls ``on_stop()`` and clears the started state; ``System::deinit()`` stops running apps and cleans up the runtime, GUI, timers, services, and app records.

``System::process_timers()`` is currently retained as a compatibility entry; timers are triggered automatically by the internal scheduler and dispatched to apps.

If ``System::Config::enable_gui_live_preview`` is on, the core registers live preview after a file-backed GUI document loads, and starts a periodic poll in the ``SystemGui`` task group. The poll and document reload still pass through the GUI runtime gate and backend thread guard, and the poll state is canceled on ``deinit()`` or document unload.

.. _system-core-architecture-sec-03:

Task Scheduling
--------------------

``system_core`` uses one shared ``lib_utils::TaskScheduler`` configured with several serial task groups:

.. list-table::
   :header-rows: 1

   * - Group
     - Purpose
   * - ``SystemGui``
     - GUI document load/mount/unload, regular binding flush, lifecycle-related GUI operations
   * - ``SystemGuiInput``
     - GUI event dispatch, input-triggered ``ExecuteBatch``, image switching, and runtime animations
   * - ``SystemApp``
     - App lifecycle and regular timer callbacks
   * - ``SystemAppInput``
     - The ``on_action()`` callbacks for GUI actions
   * - ``SystemTimer``
     - Periodic/delayed timer trigger source

``SystemGui`` and ``SystemGuiInput`` share one GUI runtime gate and backend thread guard to avoid concurrent access to ``gui::Runtime`` and the LVGL backend; ``SystemApp`` and ``SystemAppInput`` share one app callback gate to avoid native or runtime apps being entered concurrently by timers and actions.

GUI actions do not run app logic directly inside the LVGL event callback stack. For lightweight actions without payload, ``gui::Runtime`` uses a fast path to trigger the action subscription directly, and the subscription only posts to ``SystemAppInput``; for events such as ``ValueChanged`` that need to sync GUI runtime state, the backend event still enters ``SystemGuiInput`` first, and then the action subscription posts to ``SystemAppInput``.

.. _system-core-architecture-sec-04:

Impl Record Model
--------------------

``Impl::AppRecord`` keeps the runtime state of each app:

- ``AppInfo info``.
- The native app instance and ``AppContext``.
- The app-owned GUI ``document_id``.
- The runtime internal app id.
- The action connection list.
- Registered image/font resource ids.
- The map from timer id to timer name.
- The currently mounted screen path.
- Flags for whether the runtime and GUI are loaded.

The core uses ``AppRecord`` to handle native and runtime app differences uniformly.

.. _system-core-architecture-sec-05:

Source Modules
--------------------

The implementation of ``system_core`` is split by function:

.. list-table::
   :header-rows: 1

   * - Source File
     - Responsibility
   * - ``system/core.cpp``
     - ``System`` construction/destruction and default virtual callbacks
   * - ``system/lifecycle.cpp``
     - The ``init/start/stop/deinit/process_timers`` main flow
   * - ``system/task.cpp``
     - ``TaskScheduler`` groups, GUI/app gates, synchronous/asynchronous task helpers
   * - ``system/gui.cpp``
     - App-owned GUI, system-only GUI, action subscription, binding merge
   * - ``system/gui_access.cpp``
     - The system-only GUI facade used by a derived ``System``
   * - ``system/resources.cpp``
     - Automatic registration and unregistration of native app image/font resources
   * - ``system/timer.cpp``
     - Timer create, stop, cancel, pending merge, and ``on_timer`` dispatch
   * - ``app/app.cpp``
     - Base implementation of the app-facing runtime wrapper
   * - ``app/manager.cpp``
     - App install, start, stop, pause, resume, and query
   * - ``app/package.cpp``
     - ``.bpk`` unpacking, package manifest parsing, ``AppManifest`` / ``AppConfig`` mapping
   * - ``app/package_scan.cpp``
     - Scanning the ``apps`` directory of internal/external storage and filtering duplicate manifest ids
   * - ``system/storage.cpp``
     - Internal/external storage layout, app/private/public directories, and restricted file operations
   * - ``runtime/runtime.cpp``
     - Runtime initialization, load/unload, ``brookesia_app.*`` calls, and owner mapping
   * - ``runtime/host_bridge.cpp``
     - The ``SystemHostBridge`` native module and runtime app service bridge
   * - ``service/*.cpp``
     - ``SystemCore``, ``SystemGui``, ``SystemTimer`` schema, handlers, and service helpers

.. _system-core-architecture-sec-06:

Public Include Modules
----------------------

``include/brookesia/system_core`` keeps the top-level compatibility headers and also provides fine-grained per-module headers:

.. list-table::
   :header-rows: 1

   * - Entry
     - Responsibility
   * - ``app.hpp``
     - Common umbrella for native app authors, aggregating app types, context, runtime wrapper, and ``IApp``
   * - ``system.hpp``
     - Common umbrella for a derived ``System``, aggregating batch, the system-only GUI facade, and ``System``
   * - ``service_api.hpp``
     - Umbrella for service helpers and service classes
   * - ``package.hpp``
     - Umbrella for package unpacking, manifest reading, and ``runtime::AppConfig`` mapping
   * - ``app/*.hpp``
     - App types, ``AppGuiRuntime``, ``AppTimerRuntime``, ``SystemApi``, ``AppContext``, ``IApp``, package API
   * - ``system/*.hpp``
     - ``GuiBatch*``, ``SystemGuiAccess``, storage layout, ``System``
   * - ``service/*.hpp``
     - ``SystemCore``, ``SystemGui``, ``SystemTimer`` helpers and service classes

In-repo implementation files prefer including the fine-grained headers; external code can keep including the top-level compatibility headers.

.. _system-core-architecture-sec-07:

Module Collaboration
--------------------

.. list-table::
   :header-rows: 1

   * - Module
     - Responsibility in System Core
   * - ``gui::Runtime``
     - Load JSON UI documents, register runtime image/font, mount screens, find and update views
   * - ``runtime::Runtime``
     - Load scripted apps and call ``brookesia_app.*`` lifecycle functions
   * - ``SystemHostBridge``
     - The private host bridge of system_core that lets managed runtime apps call system services through a native module
   * - ``service::ServiceManager``
     - Hosts ``SystemCore``, ``SystemGui``, ``SystemTimer``
   * - ``lib_utils::TaskScheduler``
     - Implements internal GUI, app, and timer scheduling
   * - ``IAppProvider`` registry
     - Provides native app providers linked into the project at compile time

.. _system-core-architecture-sec-08:

Extension Points
--------------------

A derived system can override:

- ``on_init()``
- ``on_start()``
- ``on_stop()``
- ``on_deinit()``
- ``on_app_installed()``
- ``on_app_started()``
- ``on_app_stopped()``

``system_super`` uses these extension points to install the shell app, mount the overlay, and restore the shell after a regular app stops.
