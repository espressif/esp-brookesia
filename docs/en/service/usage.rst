.. _service-usage-sec-00:

Usage
=====

:link_to_translation:`zh_CN:[中文]`

This guide explains how to integrate the ESP-Brookesia **service framework** in an ESP-IDF project, using the **Wi-Fi service** as an example. A full, buildable sample is in ``examples/service/wifi``.

In a typical project, using a component directly means adding a dependency, including headers, and calling its APIs. As features grow, coupling between modules and components increases; removing a component then requires cleaning up headers and calls as well as the dependency, which raises maintenance cost.

ESP-Brookesia abstracts this behind two unified surfaces: **functions** and **events**. In many cases you only need ``brookesia_service_manager`` and ``brookesia_service_helper`` to reach service features through the Helper; you can also check at runtime whether a service is available. If the concrete service component is not linked, Helper usage usually still compiles, which helps with trimming and maintenance.

The framework also relies on mechanisms such as the :doc:`Task Scheduler <../utils/lib_utils/task_scheduler>` for asynchronous execution on the service side, reducing blocking on the caller. Exposed APIs are designed with thread safety in mind so application code does less ad-hoc synchronization.

The sections below follow this order: **dependencies → initialization and binding → calling functions → subscribing to events → event monitors**.

.. _service-usage-sec-01:

Adding component dependencies
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Declare dependencies in ``idf_component.yml`` at the project root or in a component directory. For the Wi-Fi service:

.. code-block:: yaml

   dependencies:
     espressif/brookesia_service_wifi: "*"
     # espressif/brookesia_service_nvs: "*" # optional, if you need the NVS service

If your project does not link a concrete service implementation and you only need to satisfy “Helper code compiles”, you can depend on ``brookesia_service_helper`` alone:

.. code-block:: yaml

   dependencies:
     espressif/brookesia_service_helper: "*"

For how to obtain components and version constraints, see :ref:`Obtaining and using components <getting-started-component-usage>`.

.. _service-usage-sec-02:

Headers, namespace, and type aliases
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   #include "brookesia/lib_utils.hpp"
   #include "brookesia/service_manager.hpp"
   // Helper header: pick the Helper that matches your service component
   #include "brookesia/service_helper/wifi.hpp"

   // Brookesia data types live under the esp_brookesia namespace
   using namespace esp_brookesia;
   // Type aliases keep examples short
   // Service-related types are under the service namespace
   using WifiHelper = service::helper::Wifi;

.. _service-usage-sec-03:

Starting the service manager
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Before calling any service API, initialize the global **singleton** ``ServiceManager`` by calling ``start()``.

.. code-block:: cpp

   auto &service_manager = service::ServiceManager::get_instance();
   service_manager.start();

.. _service-usage-sec-04:

Starting / stopping a service
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   // Before binding, you can check whether the Helper’s service is linked into the build
   if (!WifiHelper::is_available()) {
      // Service unavailable: omitting the service component does not break compilation, but this check fails
      return;
   }

   // Bind to the service: starts the service and its dependencies while `binding` is alive;
   // when `binding` goes out of scope it is destroyed and the service stops.
   // To keep the service running longer, store `binding` outside the stack (e.g. static or heap).
   auto binding = service_manager.bind(WifiHelper::get_name().data());
   if (!binding.is_valid()) {
      // Failed to start the service
      return;
   }

.. _service-usage-sec-05:

Calling service functions
~~~~~~~~~~~~~~~~~~~~~~~~~

Before calling a function, confirm **parameter types, order, and return value** from the Helper contract or headers, for example:

- :ref:`Wi-Fi service function interface reference <helper-contract-service-wifi-functions>`
- `Wi-Fi Helper header source <https://github.com/espressif/esp-brookesia/blob/master/service/brookesia_service_helper/include/brookesia/service_helper/wifi.hpp>`_

.. note::

   - **Description**: Function behavior, return information, etc. If there is no **return value** section, the function has no return value.
   - **Parameter list**: Parameter names, types, descriptions, and defaults.
      - **Types**: Six kinds are supported: ``String``, ``Number``, ``Boolean``, ``Object``, ``Array``, ``RawBuffer``. Mapping to call-time C++ types:
         - ``String``: ``std::string``
         - ``Number``: ``double`` / ``int`` / *any arithmetic type*
         - ``Boolean``: ``bool``
         - ``Object``: ``boost::json::object`` (often produced by serializing a ``struct``)
         - ``Array``: ``boost::json::array`` (often produced by serializing ``std::vector``)
         - ``RawBuffer``: ``service::RawBuffer``
      - **Description**:
         - For ``String`` with enumerated values in the description, you pass enums via serialization.
         - For ``Object`` / ``Array``, the description may include examples.
      - **Default**: Shown when the parameter is **optional**.
   - **Execution requirement**: Whether the function requires the Task Scheduler.
      - **Required**: Must run **after the service has started**; **sync/async** calls go through the scheduler.
      - **Not required**: May run **before** the service starts; only **sync** calls are allowed. Typical cases:
         - The function must run before startup.
         - The parameter list contains ``RawBuffer``.
         - The implementation is already thread-safe (locks, etc.), so the scheduler is not required.

Use static Helper methods for **synchronous** or **asynchronous** calls. **Synchronous** calls are described first.

.. _service-usage-sec-06:

Synchronous calls
^^^^^^^^^^^^^^^^^

The call blocks until the function finishes or times out.

.. code-block:: cpp

   auto result = WifiHelper::call_function_sync<ReturnType>(WifiHelper::FunctionId::FunctionName [, parameters...] [, timeout]);
   if (!result) {
      // Call failed, log error
      BROOKESIA_LOGE("Failed: %1%", result.error());
   } else {
      // Call succeeded
      // If the function returns a value, use `result.value()`
      auto &value = result.value(); // `value` has type `ReturnType`
      // ...
   }

.. note::

   - ``parameters`` must match the schema in type, count, and order; omit them if the function has no parameters.
   - If the return type is ``void``, omit the ``<ReturnType>`` template argument.
   - ``timeout`` is optional; if omitted, the default is ``BROOKESIA_SERVICE_MANAGER_DEFAULT_CALL_FUNCTION_TIMEOUT_MS``.
   - See :cpp:func:`esp_brookesia::service::helper::Base::call_function_sync`.

.. _service-usage-sec-07:

Example: multiple parameters
""""""""""""""""""""""""""""

``SetConnectAp`` — interface summary and sample code:

- Function: ``SetConnectAp``
- Parameters:
   - ``SSID``: ``String``
   - ``Password``: ``String``
- Return type: ``void``

.. code-block:: cpp

   // Pass parameters in schema order; do not skip required parameters
   auto set_connect_ap_result = WifiHelper::call_function_sync(
      WifiHelper::FunctionId::SetConnectAp, "ssid1", "password1", service::helper::Timeout(100));
   if (!set_connect_ap_result) {
      // Call failed, log error
      BROOKESIA_LOGE("Failed: %1%", set_connect_ap_result.error());
   }

.. _service-usage-sec-08:

Example: serialized parameters
""""""""""""""""""""""""""""""

``TriggerGeneralAction`` — interface summary and sample code:

- Function: ``TriggerGeneralAction``
- Parameters:
   - ``Action``: ``String`` (serialize from ``WifiHelper::GeneralAction``)
- Return type: ``void``

.. code-block:: cpp

   auto start_result = WifiHelper::call_function_sync(
      WifiHelper::FunctionId::TriggerGeneralAction,
      BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Start));
   if (!start_result) {
      // Call failed, log error
      BROOKESIA_LOGE("Failed: %1%", start_result.error());
   }

``SetScanParams`` — interface summary and sample code:

- Function: ``SetScanParams``
- Parameters:
   - ``Param``: ``Object`` (serialize from ``WifiHelper::ScanParams``)
- Return type: ``void``

.. code-block:: cpp

   WifiHelper::ScanParams scan_params{
      .ap_count = 10,
      .interval_ms = 10000,
      .timeout_ms = 60000,
   };
   auto set_scan_params_result = WifiHelper::call_function_sync(
      WifiHelper::FunctionId::SetScanParams, BROOKESIA_DESCRIBE_TO_JSON(scan_params).as_object());
   if (!set_scan_params_result) {
      // Call failed, log error
      BROOKESIA_LOGE("Failed: %1%", set_scan_params_result.error());
   }

.. _service-usage-sec-09:

Example: parsing return values
""""""""""""""""""""""""""""""

``GetConnectedAps`` — interface summary and sample code:

- Function: ``GetConnectedAps``
- Parameters: none
- Return type: ``boost::json::array`` (deserialize to ``std::vector<WifiHelper::ConnectApInfo>``)

.. code-block:: cpp

   auto get_connected_aps_result =
      WifiHelper::call_function_sync<boost::json::array>(WifiHelper::FunctionId::GetConnectedAps);
   if (!get_connected_aps_result) {
      // Call failed, log error
      BROOKESIA_LOGE("Failed to get connected APs: %1%", get_connected_aps_result.error());
   }

   // Parse return value
   std::vector<WifiHelper::ConnectApInfo> infos;
   auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(get_connected_aps_result.value(), infos);
   if (!parse_result) {
      // Parse failed, log error
      BROOKESIA_LOGE("Failed to parse connected APs: %1%", get_connected_aps_result.error());
   } else {
      // Parse succeeded, log result
      BROOKESIA_LOGI("Connected APs: %1%", infos);
   }

.. _service-usage-sec-10:

Asynchronous calls
^^^^^^^^^^^^^^^^^^

The call **submits** work and returns immediately without blocking the caller. If you pass a result handler, the framework invokes it **asynchronously** when the function completes.

.. code-block:: cpp

   auto on_function_handler = [](service::FunctionResult &&result) {
      // Process return value
      if (!result.success) {
         // Execution failed, log error
         BROOKESIA_LOGE("Failed: %1%", result.error_message);
      } else {
         // Execution succeeded
         // If there is a return value:
         // auto &value = result.get_data<ReturnType>();
         // ...
      }
   };
   auto result = WifiHelper::call_function_async(WifiHelper::FunctionId::FunctionName [, parameters...] [, on_function_handler]);
   if (!result) {
      // Call failed
   }

.. note::

   - ``parameters`` must match the schema; omit if the function has no parameters.
   - ``on_function_handler`` is optional; if omitted, results are ignored.
   - Serial async calls to the same service run **in submission order** inside the service:

      .. code-block:: cpp

         Helper::call_function_async(Helper::FunctionId::Function1);
         Helper::call_function_async(Helper::FunctionId::Function2);
         Helper::call_function_async(Helper::FunctionId::Function3);
         // Internal execution order: Function1 -> Function2 -> Function3

   - See :cpp:func:`esp_brookesia::service::helper::Base::call_function_async`.

.. _service-usage-sec-11:

Example
"""""""

``GetConnectedAps`` — interface summary and sample code:

- Function: ``GetConnectedAps``
- Parameters: none
- Return type: ``boost::json::array`` (same as ``std::vector<WifiHelper::ConnectApInfo>`` when serialized)

.. code-block:: cpp

   auto on_get_connected_aps_handler = [](service::FunctionResult &&result) {
      if (!result.success) {
         // Execution failed, log error
         BROOKESIA_LOGE("Failed to get connected APs: %1%", result.error_message);
         return;
      }
      // Execution succeeded, parse return value
      std::vector<WifiHelper::ConnectApInfo> infos;
      auto &data = result.get_data<boost::json::array>();
      auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(data, infos);
      if (!parse_result) {
         // Parse failed, log error
         BROOKESIA_LOGE("Failed to parse connected APs: %1%", result);
         return;
      }
      // Parse succeeded, log result
      BROOKESIA_LOGI("Connected APs: %1%", infos);
   };
   auto get_connected_aps_result =
      WifiHelper::call_function_async(WifiHelper::FunctionId::GetConnectedAps, on_get_connected_aps_handler);
   if (!get_connected_aps_result) {
      // Call failed
      return;
   }

.. _service-usage-sec-12:

Subscribing to service events
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Confirm **item names, types, and order** from the Helper contract or headers, for example:

- :ref:`Wi-Fi service event interface reference <helper-contract-service-wifi-events>`
- `Wi-Fi Helper header source <https://github.com/espressif/esp-brookesia/blob/master/service/brookesia_service_helper/include/brookesia/service_helper/wifi.hpp>`_

.. note::

   - **Description**: Event behavior and return information. If there is no **return value** section, the event carries no return payload in that sense.
   - **Item list**: Item names, types, descriptions, and defaults.
      - **Types**: Same six kinds as for functions; mapping is the same:
         - ``String``: ``std::string``
         - ``Number``: ``double`` / ``int`` / *any arithmetic type*
         - ``Boolean``: ``bool``
         - ``Object``: ``boost::json::object`` (often produced by serializing a ``struct``)
         - ``Array``: ``boost::json::array`` (often produced by serializing ``std::vector``)
         - ``RawBuffer``: ``service::RawBuffer``
      - **Description**:
         - ``String`` with enumerated values deserializes to enum-like data.
         - ``Object`` / ``Array`` may include examples.
   - **Execution requirement**: Whether the event requires the Task Scheduler.
      - **Required**: Must be published **after** the service has started.
      - **Not required**: May be published **before** startup. Typical cases:
         - The event must fire before startup.
         - The item list contains ``RawBuffer``.

.. _service-usage-sec-13:

Subscribing to events
^^^^^^^^^^^^^^^^^^^^^

Use ``subscribe_event()`` to register a callback; when the event fires, callbacks run **serially** in subscription order.

.. code-block:: cpp

   auto on_event_handler = [](const std::string &event_name[, items...]) {
      // Handle the event
   };
   // `conn` is the subscription; destruction **unsubscribes** (RAII).
   // To keep the subscription alive, store `conn` outside the stack (e.g. static or heap).
   auto conn = WifiHelper::subscribe_event(WifiHelper::EventId::EventName, on_event_handler);
   if (!conn.connected()) {
      // Subscribe failed
   }
   // You can also unsubscribe manually
   conn.disconnect();

.. note::

   - Call ``subscribe_event()`` only **after** ``ServiceManager::start()``.
   - ``items`` must match the event schema; omit if the event has no items.
   - There is no hard limit on subscriptions; all callbacks for an event run in subscription order.
   - See :cpp:func:`esp_brookesia::service::helper::Base::subscribe_event`.

.. _service-usage-sec-14:

Example
"""""""

``GeneralEventHappened`` — interface summary and sample code:

- Event: ``GeneralEventHappened``
- Items:
   - ``Event``: ``String`` (deserialize to ``WifiHelper::GeneralEvent``)
   - ``IsUnexpected``: ``Boolean``

.. code-block:: cpp

   auto on_general_event_happened_handler = [](const std::string &event_name, const std::string &event,
                                               bool is_unexpected) {
      // Event fired, parse items
      WifiHelper::GeneralEvent general_event;
      auto parse_result = BROOKESIA_DESCRIBE_STR_TO_ENUM(event, general_event);
      if (!parse_result) {
         // Parse failed, log error
         BROOKESIA_LOGE("Failed to parse general event: %1%", event);
         return;
      }
      // Parse succeeded, log result
      BROOKESIA_LOGI("General event: %1%", general_event);
   };
   // Unsubscribe automatically when the connection is destroyed (RAII)
   auto general_event_happened_connection = WifiHelper::subscribe_event(
      WifiHelper::EventId::GeneralEventHappened, on_general_event_happened_handler);
   if (!general_event_happened_connection.connected()) {
      // Subscribe failed
      return;
   }

   // Trigger Start action
   WifiHelper::call_function_async(WifiHelper::FunctionId::TriggerGeneralAction,
                                   BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Start));

   // Wait for the event (example uses sleep; use a proper sync primitive in production)
   boost::this_thread::sleep_for(boost::chrono::seconds(5));

``ScanApInfosUpdated`` — interface summary and sample code:

- Event: ``ScanApInfosUpdated``
- Items:
   - ``ApInfos``: ``Array`` (deserialize to ``std::vector<WifiHelper::ScanApInfo>``)

.. code-block:: cpp

   auto on_scan_ap_infos_updated_handler = [](const std::string &event_name, const boost::json::array &ap_infos) {
      // Event fired, parse items
      std::vector<WifiHelper::ScanApInfo> scanned_aps;
      auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(ap_infos, scanned_aps);
      if (!parse_result) {
         // Parse failed, log error
         BROOKESIA_LOGE("Failed to parse scan AP infos: %1%", ap_infos);
         return;
      }
      // Parse succeeded, log result
      BROOKESIA_LOGI("Scanned APs: %1%", scanned_aps);
   };
   auto scan_ap_infos_updated_connection =
      WifiHelper::subscribe_event(WifiHelper::EventId::ScanApInfosUpdated, on_scan_ap_infos_updated_handler);
   if (!scan_ap_infos_updated_connection.connected()) {
      // Subscribe failed
      return;
   }

   WifiHelper::call_function_async(WifiHelper::FunctionId::TriggerScanStart);

   boost::this_thread::sleep_for(boost::chrono::seconds(10));

.. _service-usage-sec-15:

Event monitor
^^^^^^^^^^^^^

``EventMonitor`` adds **blocking wait** on top of **asynchronous events**: after triggering an API, you can wait for a matching event or any occurrence to drive control flow.

.. code-block:: cpp

   // Create and start the monitor
   WifiHelper::EventMonitor<WifiHelper::EventId::EventName> event_monitor;
   event_monitor.start();

   // Call the API that raises this event (omitted)

   // Wait for any occurrence
   auto got_any_event = event_monitor.wait_for_any(timeout_ms);
   if (!got_any_event) {
      // Timeout, event did not fire
      return;
   }

   // Or wait for an event whose items match
   auto got_event = event_monitor.wait_for(std::vector<service::EventItem>{items...}, timeout_ms);
   if (!got_event) {
      // Timeout, items did not match
      return;
   }

   event_monitor.stop();

.. note::

   - ``items`` must match the event definition in type, count, and order.
   - See :cpp:class:`esp_brookesia::service::helper::EventMonitor`.

.. _service-usage-sec-16:

Example: wait for specific event items
""""""""""""""""""""""""""""""""""""""

``GeneralEventHappened`` — monitor usage:

- Event: ``GeneralEventHappened``
- Items:
   - ``Event``: ``String`` (serialize from ``WifiHelper::GeneralEvent``)
   - ``IsUnexpected``: ``Boolean``

.. code-block:: cpp

   WifiHelper::EventMonitor<WifiHelper::EventId::GeneralEventHappened> general_event_monitor;
   BROOKESIA_CHECK_FALSE_EXIT(general_event_monitor.start(), "Failed to start general event monitor");

   WifiHelper::call_function_async(WifiHelper::FunctionId::TriggerGeneralAction,
                                   BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Start));

   auto got_event = general_event_monitor.wait_for(
      std::vector<service::EventItem>{BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Started), false}, 5000);
   if (!got_event) {
      // Timeout, event did not fire
      return;
   }

.. _service-usage-sec-17:

Example: latest received event items
""""""""""""""""""""""""""""""""""""

``ScanApInfosUpdated`` — monitor usage:

- Event: ``ScanApInfosUpdated``
- Items:
   - ``ApInfos``: ``Array`` (deserialize to ``std::vector<WifiHelper::ScanApInfo>``)

.. code-block:: cpp

   WifiHelper::EventMonitor<WifiHelper::EventId::ScanApInfosUpdated> scan_ap_infos_updated_monitor;
   BROOKESIA_CHECK_FALSE_EXIT(scan_ap_infos_updated_monitor.start(), "Failed to start scan AP infos updated monitor");

   WifiHelper::call_function_async(WifiHelper::FunctionId::TriggerScanStart);

   auto got_event = scan_ap_infos_updated_monitor.wait_for_any(10000);
   if (!got_event) {
      // Timeout, event did not fire
      return;
   }

   auto last_items = scan_ap_infos_updated_monitor.get_last<boost::json::array>();
   if (!last_items.has_value()) {
      return;
   }
   const auto &ap_infos = std::get<0>(last_items.value());
   std::vector<WifiHelper::ScanApInfo> scanned_aps;
   auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(ap_infos, scanned_aps);
   if (!parse_result) {
      BROOKESIA_LOGE("Failed to parse scan AP infos: %1%", ap_infos);
      return;
   }
   BROOKESIA_LOGI("Scanned APs: %1%", scanned_aps);
