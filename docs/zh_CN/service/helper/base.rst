.. _service-helper-base-sec-00:

基类
====

:link_to_translation:`en:[English]`

- 公共头文件： ``#include "brookesia/service_helper/base.hpp"``

.. _service-helper-base-sec-01:

概述
----

`esp_brookesia::service::helper::Base<Derived>` 是 Service Helper 体系的 CRTP 基类，为各具体 helper 提供统一的函数调用、事件订阅与 schema 校验能力。

.. _service-helper-base-sec-02:

特性
----

- CRTP 契约约束：通过 `DerivedMeta` 要求派生类提供 `FunctionId/EventId/get_name/get_*_schemas` 等标准元信息。
- 统一函数调用：提供 `call_function_sync` 与 `call_function_async`，自动完成参数打包与返回值解析。
- 类型安全事件订阅：提供多种 `subscribe_event` 重载，支持按回调签名自动适配事件参数。
- 服务可用性检查：通过 `is_available`、`get_function_schema`、`get_event_schema` 快速判断和获取能力描述。
- 超时控制：同步调用支持在参数末尾附加 `Timeout(ms)`，用于控制等待时长。
- 回调适配宏：提供 `BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_*`，简化服务函数注册时的参数转换样板代码。

.. _service-helper-base-sec-03:

使用方式
--------

派生 helper 需定义函数/事件枚举和 schema，并继承 `Base<Derived>` 复用通用调用逻辑。

.. code-block:: cpp

   class MyHelper : public esp_brookesia::service::helper::Base<MyHelper> {
   public:
       enum class FunctionId { Ping };
       enum class EventId { Ready };
       static std::string_view get_name();
       static std::vector<FunctionSchema> get_function_schemas();
       static std::vector<EventSchema> get_event_schemas();
   };

.. _service-helper-base-sec-04:

API 参考
--------

.. include-build-file:: inc/service/brookesia_service_helper/include/brookesia/service_helper/base.inc
