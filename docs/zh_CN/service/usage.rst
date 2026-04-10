.. _service-usage-sec-00:

使用说明
========

:link_to_translation:`en:[English]`

本文说明如何在 ESP-IDF 工程中集成 ESP-Brookesia **服务框架**，并以 **Wi-Fi 服务** 为例给出典型调用方式；完整可编译示例见 ``examples/service/wifi``。

在常规工程中，直接使用某个组件时，一般需要添加依赖、包含头文件并调用其接口。功能增多后，模块与组件之间的耦合会变强；若要移除某一组件，除删除依赖外，还需清理相关头文件与调用，维护成本较高。

ESP-Brookesia 将上述关系抽象为统一的 **函数** 与 **事件** 两类接口。多数场景下只需依赖 ``brookesia_service_manager`` 与 ``brookesia_service_helper``，即可通过 Helper 访问各服务功能；也可在运行时检查某服务是否可用，未链接具体服务组件时通常不会因 Helper 调用本身而产生编译错误，便于裁剪与维护。

此外，框架借助 :doc:`任务调度器 <../utils/lib_utils/task_scheduler>` 等机制支持服务侧异步执行，减轻对调用线程的阻塞；对外暴露的接口在设计上考虑线程安全，降低业务侧自行处理并发同步的负担。

下文按 **依赖配置 → 初始化与绑定 → 调用函数 → 订阅事件 → 事件监听器** 的顺序说明操作要点。

.. _service-usage-sec-01:

添加组件依赖
~~~~~~~~~~~~

在工程根目录或组件目录的 ``idf_component.yml`` 中声明依赖。以 Wi-Fi 服务为例：

.. code-block:: yaml

   dependencies:
     espressif/brookesia_service_wifi: "*"
     # espressif/brookesia_service_nvs: "*" # 可选，如果需要使用 NVS 服务

若工程不链接具体服务实现，仅需满足「能编译通过 Helper 相关代码」一类约束时，可只添加 ``brookesia_service_helper``：

.. code-block:: yaml

   dependencies:
     espressif/brookesia_service_helper: "*"

组件的获取与版本约束请参考 :ref:`如何获取和使用组件 <getting-started-component-usage>`。

.. _service-usage-sec-02:

头文件、命名空间与类型别名
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   #include "brookesia/lib_utils.hpp"
   #include "brookesia/service_manager.hpp"
   // Helper 头文件，需要根据具体服务组件选择对应的 Helper 头文件
   #include "brookesia/service_helper/wifi.hpp"

   // 所有 Brookesia 组件中的数据类型均在 esp_brookesia 命名空间下
   using namespace esp_brookesia;
   // 使用类型别名简化代码
   // 服务相关的类型则位于 service 命名空间下
   using WifiHelper = service::helper::Wifi;

.. _service-usage-sec-03:

启动服务管理器
~~~~~~~~~~~~~~

在调用任何服务接口前，应对全局 **单例** ``ServiceManager`` 调用 ``start()`` 完成初始化。

.. code-block:: cpp

   auto &service_manager = service::ServiceManager::get_instance();
   service_manager.start();

.. _service-usage-sec-04:

启动/停止服务
~~~~~~~~~~~~~

.. code-block:: cpp

   // 在绑定前可检查 Helper 对应的服务是否已链接到工程
   if (!WifiHelper::is_available()) {
      // 服务不可用：未添加对应服务组件时不会因此产生编译错误，但此处检查会失败
      return;
   }

   // 获取服务绑定，绑定期间会拉起该服务及其依赖；
   // 在 `binding` 对象存活期间服务保持运行，离开作用域后解绑（析构），服务停止。
   // 如果需要长期持有服务，可以将 `binding` 对象存储到非栈区域中。
   auto binding = service_manager.bind(WifiHelper::get_name().data());
   if (!binding.is_valid()) {
      // 服务启动失败
      return;
   }

.. _service-usage-sec-05:

调用服务函数
~~~~~~~~~~~~

调用前请确认目标函数的 **参数类型、顺序与返回值**，可在对应 Helper 的契约文档或头文件中查阅，例如：

- :ref:`Wi-Fi 服务函数接口说明文档 <helper-contract-service-wifi-functions>`
- `Wi-Fi 服务辅助头文件源码 <https://github.com/espressif/esp-brookesia/blob/master/service/brookesia_service_helper/include/brookesia/service_helper/wifi.hpp>`_

.. note::

   - **描述**：包含函数的功能描述、返回值信息等。若无 **返回值** 相关内容，则表示函数无返回值。
   - **参数列表**：包含函数参数的名称、类型、描述和默认值。
      - **类型**：总共支持 ``String``、 ``Number``、 ``Boolean``、 ``Object``、 ``Array``、 ``RawBuffer`` 六种，它们与实际调用时传入的参数类型关系如下：
         - ``String``： ``std::string``
         - ``Number``： ``double/int/<任意算术类型>``
         - ``Boolean``： ``bool``
         - ``Object``： ``boost::json::object`` （通常可以利用 ``struct`` 序列化生成）
         - ``Array``： ``boost::json::array`` （通常可以利用 ``std::vector`` 序列化生成）
         - ``RawBuffer``： ``service::RawBuffer``
      - **描述**：
         - 当参数类型为 ``String`` 且描述中提供候选值时，表示参数可以通过序列化将枚举类型传入。
         - 当参数类型为 ``Object/Array`` 时，描述会提供示例。
      - **默认值**：当参数标记为 **可选** 时，会提供默认值。
   - **执行要求**：表示该函数是否需要调度器（Task Scheduler）支持。
      - 若标记为 **需要**，则表示该函数必须在 **服务启动后** 再执行，此时支持通过调度器进行 **同步/异步调用**。
      - 若标记为 **不需要**，则表示该函数可以在 **服务启动前** 执行，此时仅支持 **同步调用**。这种情况通常出现在以下场景：
         - 该函数需要在服务启动前执行。
         - 该函数的参数列表中存在 ``RawBuffer`` 类型。
         - 该函数的底层实现已经通过锁或其他机制保证了线程安全，因此无需通过调度器进行调用。

通过 Helper 的静态方法可发起 **同步调用** 或 **异步调用**。先说明 **同步调用**：

.. _service-usage-sec-06:

同步调用
^^^^^^^^

此接口会阻塞等待函数执行完成，并返回结果；若超时则返回错误。

.. code-block:: cpp

   auto result = WifiHelper::call_function_sync<返回类型>(WifiHelper::FunctionId::函数名 [, 参数列表...] [, 超时时间]);
   if (!result) {
      // 调用失败，打印错误信息
      BROOKESIA_LOGE("Failed: %1%", result.error());
   } else {
      // 调用成功
      // 若函数有返回值，则可以通过 `result.value()` 获取
      auto &value = result.value(); // `value` 类型为 `返回类型`
      // ...
   }

.. note::

   - ``参数列表`` 的类型、数量和顺序必须与函数定义中的参数列表一致，若函数无参数，则省略。
   - 若 ``返回类型`` 为 ``void``，则 ``<返回类型>`` 模板参数可以省略。
   - ``超时时间`` 为可选参数，若省略则使用默认值 ``BROOKESIA_SERVICE_MANAGER_DEFAULT_CALL_FUNCTION_TIMEOUT_MS``。
   - 具体接口说明见 :cpp:func:`esp_brookesia::service::helper::Base::call_function_sync`。

.. _service-usage-sec-07:

示例：多参数输入
""""""""""""""""""

以 ``SetConnectAp`` 为例，接口要点与示例代码如下：

- 函数名： ``SetConnectAp``
- 参数列表：
   - ``SSID``：类型为 ``String``
   - ``Password``：类型为 ``String``
- 返回值类型： ``void``

.. code-block:: cpp

   // 严格按照参数列表的顺序传入参数，不可使用默认值
   auto set_connect_ap_result = WifiHelper::call_function_sync(
      WifiHelper::FunctionId::SetConnectAp, "ssid1", "password1", service::helper::Timeout(100));
   if (!set_connect_ap_result) {
      // 调用失败，打印错误信息
      BROOKESIA_LOGE("Failed: %1%", set_connect_ap_result.error());
   }

.. _service-usage-sec-08:

示例：序列化生成参数
""""""""""""""""""""

以 ``TriggerGeneralAction`` 为例，接口要点与示例代码如下：

- 函数名：``TriggerGeneralAction``
- 参数列表：
   - ``Action``：类型为 ``String`` （可由 ``WifiHelper::GeneralAction`` 类型数据序列化生成）
- 返回值类型： ``void``

.. code-block:: cpp

   auto start_result = WifiHelper::call_function_sync(
      WifiHelper::FunctionId::TriggerGeneralAction,
      BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Start));
   if (!start_result) {
      // 调用失败，打印错误信息
      BROOKESIA_LOGE("Failed: %1%", start_result.error());
   }

以 ``SetScanParams`` 为例，接口要点与示例代码如下：

- 函数名： ``SetScanParams``
- 参数列表：
   - ``Param``：类型为 ``Object`` （可由 ``WifiHelper::ScanParams`` 类型数据序列化生成）
- 返回值类型： ``void``

.. code-block:: cpp

   WifiHelper::ScanParams scan_params{
      .ap_count = 10,
      .interval_ms = 10000,
      .timeout_ms = 60000,
   };
   auto set_scan_params_result = WifiHelper::call_function_sync(
      WifiHelper::FunctionId::SetScanParams, BROOKESIA_DESCRIBE_TO_JSON(scan_params).as_object());
   if (!set_scan_params_result) {
      // 调用失败，打印错误信息
      BROOKESIA_LOGE("Failed: %1%", set_scan_params_result.error());
   }

.. _service-usage-sec-09:

示例：返回值解析
""""""""""""""""""

以 ``GetConnectedAps`` 为例，接口要点与示例代码如下：

- 函数名： ``GetConnectedAps``
- 参数列表：无
- 返回值类型： ``boost::json::array`` （可被反序列化为 ``std::vector<WifiHelper::ConnectApInfo>`` 类型数据）

.. code-block:: cpp

   auto get_connected_aps_result =
      WifiHelper::call_function_sync<boost::json::array>(WifiHelper::FunctionId::GetConnectedAps);
   if (!get_connected_aps_result) {
      // 调用失败，打印错误信息
      BROOKESIA_LOGE("Failed to get connected APs: %1%", get_connected_aps_result.error());
   }

   // 解析返回值
   std::vector<WifiHelper::ConnectApInfo> infos;
   auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(get_connected_aps_result.value(), infos);
   if (!parse_result) {
      // 解析失败，打印错误信息
      BROOKESIA_LOGE("Failed to parse connected APs: %1%", get_connected_aps_result.error());
   } else {
      // 解析成功，打印结果
      BROOKESIA_LOGI("Connected APs: %1%", infos);
   }

.. _service-usage-sec-10:

异步调用
^^^^^^^^

异步调用在 **提交** 调用后立即返回，不阻塞当前线程；若传入结果回调，则在函数执行完成后由框架在合适时机 **异步** 回调。

.. code-block:: cpp

   auto on_function_handler = [](service::FunctionResult &&result) {
      // 处理返回值
      if (!result.success) {
         // 执行失败，打印错误信息
         BROOKESIA_LOGE("Failed: %1%", result.error_message);
      } else {
         // 执行成功
         // 若函数有返回值
         // auto &value = result.get_data<返回类型>();
         // ...
      }
   };
   auto result = WifiHelper::call_function_async(WifiHelper::FunctionId::函数名 [, 参数列表...] [, on_function_handler]);
   if (!result) {
      // 调用失败
   }

.. note::

   - ``参数列表`` 的类型、数量和顺序必须与函数定义中的参数列表一致，若函数无参数，则省略。
   - ``on_function_handler`` 为可选参数，若省略则不处理返回值。
   - 如果程序对同一个服务串行执行多个异步调用，则这些调用的实际执行顺序与调用顺序一致。如：

      .. code-block:: cpp

         Helper::call_function_async(Helper::FunctionId::Function1);
         Helper::call_function_async(Helper::FunctionId::Function2);
         Helper::call_function_async(Helper::FunctionId::Function3);
         // 服务内部实际执行顺序为：Function1 -> Function2 -> Function3

   - 具体接口说明见 :cpp:func:`esp_brookesia::service::helper::Base::call_function_async`。

.. _service-usage-sec-11:

示例
""""""""

以 ``GetConnectedAps`` 为例，接口要点与示例代码如下：

- 函数名： ``GetConnectedAps``
- 参数列表：无
- 返回值类型： ``boost::json::array`` （可由 ``std::vector<WifiHelper::ConnectApInfo>`` 类型数据序列化生成）

.. code-block:: cpp

   auto on_get_connected_aps_handler = [](service::FunctionResult &&result) {
      if (!result.success) {
         // 执行失败，打印错误信息
         BROOKESIA_LOGE("Failed to get connected APs: %1%", result.error_message);
         return;
      }
      // 执行成功，解析返回值
      std::vector<WifiHelper::ConnectApInfo> infos;
      auto &data = result.get_data<boost::json::array>();
      auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(data, infos);
      if (!parse_result) {
         // 解析失败，打印错误信息
         BROOKESIA_LOGE("Failed to parse connected APs: %1%", result);
         return;
      }
      // 解析成功，打印结果
      BROOKESIA_LOGI("Connected APs: %1%", infos);
   };
   auto get_connected_aps_result =
      WifiHelper::call_function_async(WifiHelper::FunctionId::GetConnectedAps, on_get_connected_aps_handler);
   if (!get_connected_aps_result) {
      // 调用失败
      return;
   }

.. _service-usage-sec-12:

订阅服务事件
~~~~~~~~~~~~

订阅前请确认事件的 **条目名称、类型与顺序**，可在 Helper 文档或头文件中查阅，例如：

- :ref:`Wi-Fi 服务事件接口说明文档 <helper-contract-service-wifi-events>`
- `Wi-Fi 服务辅助头文件源码 <https://github.com/espressif/esp-brookesia/blob/master/service/brookesia_service_helper/include/brookesia/service_helper/wifi.hpp>`_

.. note::

   - **描述**：包含事件的功能描述、返回值信息等。若无 **返回值** 相关内容，则表示事件无返回值。
   - **条目列表**：包含事件条目的名称、类型、描述和默认值。
      - **类型**：总共支持 ``String``、 ``Number``、 ``Boolean``、 ``Object``、 ``Array``、 ``RawBuffer`` 六种，它们与实际调用时传入的参数类型关系如下：
         - ``String``： ``std::string``
         - ``Number``： ``double/int/<任意算术类型>``
         - ``Boolean``： ``bool``
         - ``Object``： ``boost::json::object``（通常可以利用 ``struct`` 序列化生成）
         - ``Array``： ``boost::json::array``（通常可以利用 ``std::vector`` 序列化生成）
         - ``RawBuffer``： ``service::RawBuffer``
      - **描述**：
         - 当条目类型为 ``String`` 且描述中提供候选值时，表示条目可以被反序列化为枚举类型数据。
         - 当条目类型为 ``Object/Array`` 时，描述会提供示例。
   - **执行要求**：表示该事件是否需要调度器（Task Scheduler）支持。
      - 若标记为 **需要**，则表示该事件必须在 **服务启动后** 再发布。
      - 若标记为 **不需要**，则表示该事件可以在 **服务启动前** 发布。这种情况通常出现在以下场景：
         - 该事件需要在服务启动前执行。
         - 该事件的条目列表中存在 ``RawBuffer`` 类型。

.. _service-usage-sec-13:

订阅事件
^^^^^^^^

使用 ``subscribe_event()`` 注册回调，在事件触发时按订阅顺序串行调用：

.. code-block:: cpp

   auto on_event_handler = [](const std::string &event_name[, 条目列表...]) {
      // 处理事件
   };
   // 返回值 `conn` 表示订阅连接；析构时会 **自动取消订阅**（RAII）。
   // 若需长期保持订阅，请将连接对象保存在非栈上（如静态或堆上）。
   auto conn = WifiHelper::subscribe_event(WifiHelper::EventId::事件名, on_event_handler);
   if (!conn.connected()) {
      // 订阅失败
   }
   // 除此之外，也可以手动取消订阅
   conn.disconnect();

.. note::

   - ``subscribe_event()`` 须在 ``ServiceManager::start()`` 之后调用。
   - ``条目列表`` 的类型、数量和顺序必须与事件定义中的条目列表一致；若事件无条目，则省略。
   - 服务事件的订阅数量没有代码限制，某一事件触发时，所有订阅该事件的回调函数都会按照订阅顺序被串行调用。
   - 具体接口说明见 :cpp:func:`esp_brookesia::service::helper::Base::subscribe_event`。

.. _service-usage-sec-14:

示例
"""""""

以 ``GeneralEventHappened`` 为例，接口要点与示例代码如下：

- 事件名： ``GeneralEventHappened``
- 条目列表：
   - ``Event``：类型为 ``String``（可被反序列化为 ``WifiHelper::GeneralEvent`` 类型数据）
   - ``IsUnexpected``：类型为 ``Boolean``

.. code-block:: cpp

   auto on_general_event_happened_handler = [](const std::string &event_name, const std::string &event,
                                               bool is_unexpected) {
      // 事件触发，解析条目
      WifiHelper::GeneralEvent general_event;
      auto parse_result = BROOKESIA_DESCRIBE_STR_TO_ENUM(event, general_event);
      if (!parse_result) {
         // 解析失败，打印错误信息
         BROOKESIA_LOGE("Failed to parse general event: %1%", event);
         return;
      }
      // 解析成功，打印结果
      BROOKESIA_LOGI("General event: %1%", general_event);
   };
   // 连接析构时自动取消订阅（RAII）
   auto general_event_happened_connection = WifiHelper::subscribe_event(
      WifiHelper::EventId::GeneralEventHappened, on_general_event_happened_handler);
   if (!general_event_happened_connection.connected()) {
      // 订阅失败
      return;
   }

   // 触发 Start 动作
   WifiHelper::call_function_async(WifiHelper::FunctionId::TriggerGeneralAction,
                                   BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Start));

   // 等待事件触发（示例用延时，实际工程请用同步原语等）
   boost::this_thread::sleep_for(boost::chrono::seconds(5));

以 ``ScanApInfosUpdated`` 为例，接口要点与示例代码如下：

- 事件名： ``ScanApInfosUpdated``
- 条目列表：
   - ``ApInfos``：类型为 ``Array``（可被反序列化为 ``std::vector<WifiHelper::ScanApInfo>`` 类型数据）

.. code-block:: cpp

   auto on_scan_ap_infos_updated_handler = [](const std::string &event_name, const boost::json::array &ap_infos) {
      // 事件触发，解析条目
      std::vector<WifiHelper::ScanApInfo> scanned_aps;
      auto parse_result = BROOKESIA_DESCRIBE_FROM_JSON(ap_infos, scanned_aps);
      if (!parse_result) {
         // 解析失败，打印错误信息
         BROOKESIA_LOGE("Failed to parse scan AP infos: %1%", ap_infos);
         return;
      }
      // 解析成功，打印结果
      BROOKESIA_LOGI("Scanned APs: %1%", scanned_aps);
   };
   auto scan_ap_infos_updated_connection =
      WifiHelper::subscribe_event(WifiHelper::EventId::ScanApInfosUpdated, on_scan_ap_infos_updated_handler);
   if (!scan_ap_infos_updated_connection.connected()) {
      // 订阅失败
      return;
   }

   WifiHelper::call_function_async(WifiHelper::FunctionId::TriggerScanStart);

   boost::this_thread::sleep_for(boost::chrono::seconds(10));

.. _service-usage-sec-15:

事件监听器
^^^^^^^^^^

``EventMonitor`` 用于在 **异步事件** 之上提供 **同步等待** 语义：可在调用某接口后阻塞等待指定事件或任意事件，便于编排流程。

.. code-block:: cpp

   // 创建并启动监听器
   WifiHelper::EventMonitor<WifiHelper::EventId::事件名> event_monitor;
   event_monitor.start();

   // 调用会触发该事件的接口，略

   // 等待任意一次事件
   auto got_any_event = event_monitor.wait_for_any(超时时间);
   if (!got_any_event) {
      // 超时，事件未触发
      return;
   }

   // 或等待条目匹配的事件
   auto got_event = event_monitor.wait_for(std::vector<service::EventItem>{[条目列表...]}, 超时时间);
   if (!got_event) {
      // 超时，未匹配到指定条目
      return;
   }

   event_monitor.stop();

.. note::

   - ``条目列表`` 的类型、数量和顺序必须与事件定义中的条目列表一致。
   - 具体接口说明见 :cpp:class:`esp_brookesia::service::helper::EventMonitor`。

.. _service-usage-sec-16:

示例：等待特定条目列表的事件触发
""""""""""""""""""""""""""""""""""

以 ``GeneralEventHappened`` 为例，监听器用法如下：

- 事件名： ``GeneralEventHappened``
- 条目列表：
   - ``Event``：类型为 ``String``（可由 ``WifiHelper::GeneralEvent`` 类型数据序列化生成）
   - ``IsUnexpected``：类型为 ``Boolean``

.. code-block:: cpp

   WifiHelper::EventMonitor<WifiHelper::EventId::GeneralEventHappened> general_event_monitor;
   BROOKESIA_CHECK_FALSE_EXIT(general_event_monitor.start(), "Failed to start general event monitor");

   WifiHelper::call_function_async(WifiHelper::FunctionId::TriggerGeneralAction,
                                   BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Start));

   auto got_event = general_event_monitor.wait_for(
      std::vector<service::EventItem>{BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralEvent::Started), false}, 5000);
   if (!got_event) {
      // 超时，事件未触发
      return;
   }

.. _service-usage-sec-17:

示例：获取最新接收的事件条目列表
""""""""""""""""""""""""""""""""""""""

以 ``ScanApInfosUpdated`` 为例，监听器用法如下：

- 事件名： ``ScanApInfosUpdated``
- 条目列表：
   - ``ApInfos``：类型为 ``Array``（可被反序列化为 ``std::vector<WifiHelper::ScanApInfo>`` 类型数据）

.. code-block:: cpp

   WifiHelper::EventMonitor<WifiHelper::EventId::ScanApInfosUpdated> scan_ap_infos_updated_monitor;
   BROOKESIA_CHECK_FALSE_EXIT(scan_ap_infos_updated_monitor.start(), "Failed to start scan AP infos updated monitor");

   WifiHelper::call_function_async(WifiHelper::FunctionId::TriggerScanStart);

   auto got_event = scan_ap_infos_updated_monitor.wait_for_any(10000);
   if (!got_event) {
      // 超时，事件未触发
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
