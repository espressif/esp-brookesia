.. _service-helper-base-sec-00:

Base
====

:link_to_translation:`zh_CN:[中文]`

- Public header: ``#include "brookesia/service_helper/base.hpp"``

.. _service-helper-base-sec-01:

Overview
--------

`esp_brookesia::service::helper::Base<Derived>` is the CRTP base for Service Helpers, providing unified function calls, event subscriptions, and schema validation.

.. _service-helper-base-sec-02:

Features
--------

- **CRTP contract**: `DerivedMeta` requires `FunctionId` / `EventId`, `get_name`, `get_*_schemas`, etc.
- **Calls**: `call_function_sync` / `call_function_async` with packing and parsing.
- **Events**: Type-safe `subscribe_event` overloads.
- **Availability**: `is_available`, `get_function_schema`, `get_event_schema`.
- **Timeouts**: Optional `Timeout(ms)` on sync calls.
- **Macros**: `BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_*` for handler boilerplate.

.. _service-helper-base-sec-03:

Usage
-----

Derive a helper with function/event enums and schemas:

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

API Reference
-------------

.. include-build-file:: inc/service/brookesia_service_helper/include/brookesia/service_helper/base.inc
