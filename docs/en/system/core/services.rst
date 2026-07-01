.. _system-core-services-sec-00:

System Services
====================

:link_to_translation:`zh_CN:[中文]`

System Core currently registers three base services: ``SystemCore``, ``SystemGui``, and ``SystemTimer``. Runtime apps call these services through the private ``SystemHostBridge`` of System Core, and the service side resolves the owner app from the current runtime caller, which keeps GUI and timer operations confined to the calling app.

Service Names
--------------------

.. list-table::
   :header-rows: 1

   * - Macro
     - Service Name
   * - ``BROOKESIA_SYSTEM_CORE_SERVICE_NAME``
     - ``SystemCore``
   * - ``BROOKESIA_SYSTEM_CORE_GUI_SERVICE_NAME``
     - ``SystemGui``
   * - ``BROOKESIA_SYSTEM_CORE_TIMER_SERVICE_NAME``
     - ``SystemTimer``

Permission Boundaries
---------------------

A runtime app can query system information, operate only its own GUI document, manage only its own timers, and cannot start or stop other apps through a service. A native app calls ``SystemApi``, ``AppGuiRuntime``, and ``AppTimerRuntime`` directly through ``AppContext``, and is currently treated as a trusted built-in app.

Core Service
--------------------

``SystemCore`` is the base system service, with the fixed service name ``SystemCore``.

.. list-table::
   :header-rows: 1

   * - Function
     - Description
   * - ``GetSystemType``
     - Return the current system type, for example ``"core"`` or ``"super"``
   * - ``GetEnvironment``
     - Return the JSON object of the current ``gui::Environment``
   * - ``GetActiveApp``
     - Return the current active app info; empty when none
   * - ``ListApps``
     - Return the installed app list
   * - ``RequestCloseApp``
     - Request to close an app
   * - ``StartApp`` / ``StopApp``
     - Start or stop the specified app
   * - ``ShowLoading`` / ``HideLoading``
     - Show or hide the calling app's loading overlay
   * - ``ShowKeyboard`` / ``HideKeyboard``
     - Show or cancel the calling app's system keyboard request

Events include ``AppChanged`` (app state change) and ``KeyboardClosed`` (keyboard input completed or canceled, containing ``RequestId``, ``AppId``, ``Confirmed``, ``Text``). A regular runtime app may call the query functions; ``RequestCloseApp``, ``ShowLoading`` / ``HideLoading``, and ``ShowKeyboard`` / ``HideKeyboard`` act only on itself, and ``StartApp`` and ``StopApp`` are rejected; a native app should use ``SystemApi`` through ``AppContext::system_service()``.

A runtime app can call ``ShowKeyboard`` to request the system keyboard; ``Options`` supports ``title``, ``placeholder``, ``initial_text``, ``max_length``, ``password``, ``mode`` (``text`` / ``upper`` / ``number`` / ``special``), and ``allowedModes``. If ``mode`` is not in ``allowedModes``, the Shell uses the first item of ``allowedModes``; when unset, all four layouts are allowed by default. After the user confirms or cancels, ``KeyboardClosed`` returns ``RequestId``, ``AppId``, ``Confirmed``, and ``Text``.

``GetEnvironment`` returns ``width_px``, ``height_px``, ``density``, ``font_scale``, ``language``, ``theme_id``. ``GetActiveApp`` and ``ListApps`` are based on ``AppInfo``, containing ``app_id``, ``manifest``, ``state``, ``last_error``.

GUI Service
--------------------

``SystemGui`` is the restricted service a runtime app uses to operate its own GUI document, with the fixed service name ``SystemGui``.

.. list-table::
   :header-rows: 1

   * - Function
     - Parameters
     - Description
   * - ``SetBinding`` / ``SetBindings``
     - ``Path``/``Key``/``Value`` or ``Updates``
     - Set or batch-set binding values
   * - ``GetBinding``
     - ``Path``, ``Key``
     - Read a binding value
   * - ``GetConstant``
     - ``Path``
     - Read a constant value of the calling app GUI document
   * - ``SetText`` / ``SetValue`` / ``SetChecked``
     - ``Path`` and its value
     - Update a label, numeric control, or switch/checkbox
   * - ``CreateView`` / ``DestroyView``
     - ``TemplateId``/``ParentPath``/``InstanceId`` or ``Path``
     - Create or destroy a dynamic view from a template
   * - ``SubscribeAction``
     - ``Action``
     - Subscribe to a GUI action, forwarded by the core to the app lifecycle
   * - ``TriggerScreenFlow`` / ``GetScreenFlowState``
     - ``ScreenFlow``, ``Action``
     - Trigger a screen flow transition or query the current state
   * - ``GetViewFrame``
     - ``Path``
     - Query the current frame of a view
   * - ``SetViewSrc``
     - ``Path``, ``Src``
     - Switch the image resource id of an image view
   * - ``StartViewAnimation`` / ``StopAnimation``
     - ``Path``/``Animation`` or ``AnimationId``
     - Start or stop a view runtime animation
   * - ``ExecuteBatch``
     - ``Commands``
     - Execute GUI commands in order within one SystemGui task

All ``SystemGui`` functions implicitly act on the calling runtime app's GUI document, so the API has no ``DocumentId``, layer, mount target, ``LoadFile`` / ``LoadJson`` / ``Unload``, or view debug / live preview interfaces. The ``GetConstant`` ``Path`` uses the dotted path part of ``${constant.<path>}``, and the return value keeps any JSON type in a ``{"Value": ...}`` envelope.

To reduce input latency, the core splits GUI operations into two kinds:

.. list-table::
   :header-rows: 1

   * - Kind
     - Functions
     - Scheduling
   * - High-frequency regular updates
     - ``SetBinding``, ``SetBindings``
     - Merged into the regular binding flush, last value wins
   * - Input-sensitive updates
     - ``ExecuteBatch``, ``SetViewSrc``, ``StartViewAnimation``, ``StopAnimation``
     - Enter ``SystemGuiInput``, prioritized over the regular binding flush

Most mutating functions are asynchronous: a success return means the request was accepted, not that LVGL finished drawing; query functions return after synchronously entering the GUI task. ``ExecuteBatch`` is for multi-step updates that need strict ordering and low scheduling overhead, such as "stop the old animation, set bindings, then start a new animation from the current position":

.. code-block:: json

   {
       "Commands": [
           {"Name": "StopAnimation", "Params": {"AnimationId": 12}},
           {"Name": "SetBindings", "Params": {"Updates": [{"Path": "/screen/bird", "Key": "angle", "Value": "-25"}]}},
           {"Name": "StartViewAnimation", "Params": {"Path": "/screen/bird", "Animation": {"property": "Y", "fromMode": "Current", "toMode": "Relative", "to": -48, "duration": 200}}}
       ]
   }

v1 supports the ``SetBindings``, ``SetViewSrc``, ``StopAnimation``, and ``StartViewAnimation`` commands, executed in array order, stopping the rest on failure, with per-command results in ``Results``.

A runtime app must declare the startup flow in ``screen_flows[]`` of ``profile.json``, and the core auto-starts it before ``on_start()``. ``TriggerScreenFlow(ScreenFlow, Action)`` switches screens by the current state and transition action, with an exact ``from[]`` transition prioritized over an any-source transition; if ``transitions`` is empty, the document still mounts ``initial`` but ``TriggerScreenFlow`` returns an error. A runtime app can only use the ``Replace`` mount mode with ``z_order`` within ``0..100``, and cannot specify a ``DocumentId``, layer, or another app's flow.

``SubscribeAction(action)`` subscribes to a GUI action in the current app document; on trigger, a native app enters ``IApp::on_action()`` and a runtime app enters ``brookesia_app.on_action()``. An action does not run directly inside the LVGL callback stack: the event enters ``SystemGuiInput`` first, then posts to ``SystemAppInput``, sharing the app callback gate with regular timer callbacks.

Timer Service
--------------------

``SystemTimer`` provides app-owned timers for runtime apps, with the fixed service name ``SystemTimer``.

.. list-table::
   :header-rows: 1

   * - Function
     - Parameters
     - Description
   * - ``StartPeriodic``
     - ``Name``, ``IntervalMs``
     - Create a periodic timer, return ``TimerId``
   * - ``StartDelayed``
     - ``Name``, ``DelayMs``
     - Create a one-shot delayed timer, return ``TimerId``
   * - ``Stop``
     - ``TimerId``
     - Stop a timer owned by the calling app

``IntervalMs`` / ``DelayMs`` must be greater than ``0``. After a timer fires, a runtime app receives ``brookesia_app.on_timer(timer_id, name)``; pending timers are not dispatched while the app is not ``Running``. When the same periodic timer is already in the pending queue, the core merges subsequent ticks to avoid unbounded backlog when a script or GUI refresh is slower than the timer period; delayed timers are not merged.

A native app uses ``AppTimerRuntime`` through ``AppContext::timer()`` with ``start_periodic()``, ``start_delayed()``, and ``stop()``. The core auto-cancels app timers on ``stop_app()``, ``uninstall_app()``, and ``deinit()``, and removes undispatched events from the pending queue.

Related Documents
--------------------

- :doc:`gui`
- :doc:`resources_permissions`
- :doc:`app_model`
