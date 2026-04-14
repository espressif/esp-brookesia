.. _service-custom-sec-00:

Service Customization
=====================

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_service_custom <https://components.espressif.com/components/espressif/brookesia_service_custom>`_
- Public header: ``#include "brookesia/service_custom.hpp"``

.. _service-custom-sec-01:

Overview
--------

`brookesia_service_custom` ships a ready-to-use **CustomService** so you can register custom functions and events without a separate Brookesia component.

.. _service-custom-sec-02:

Typical Uses
^^^^^^^^^^^^

- **Lightweight features**: LEDs, PWM, GPIO toggles, simple sensors—wrapped as CustomService calls.
- **Prototypes**: Expose logic as functions or events for local or RPC access.
- **Extensibility**: Add capabilities without changing the core service framework.

.. _service-custom-sec-03:

Features
--------

- **Dynamic registration**: `register_function()` / `register_event()` at runtime.
- **Fixed handler shape**: `FunctionParameterMap` in, `FunctionResult` out; lambdas, `std::function`, free functions, functors, `std::bind`.
- **Events**: Full publish/subscribe lifecycle.
- **ServiceManager**: Local calls and remote RPC.
- **Optional worker**: Task scheduler for thread-safe execution.

.. _service-custom-sec-04:

API Reference
-------------

.. include-build-file:: inc/service/brookesia_service_custom/include/brookesia/service_custom/service_custom.inc
