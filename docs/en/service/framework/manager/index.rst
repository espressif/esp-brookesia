.. _service-manager-index-sec-00:

Service Manager
=================

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_service_manager <https://components.espressif.com/components/espressif/brookesia_service_manager>`_
- Public header: ``#include "brookesia/service_manager.hpp"``

.. _service-manager-index-sec-01:

Overview
--------

`brookesia_service_manager` is the core service framework for lifecycle management, function and event registration, local calls, and event subscriptions.

.. _service-manager-index-sec-02:

Features
--------

- **Lifecycle**: Centralized init, start, stop, and deinit.
- **Local calls**: In-process function calls without network transport.
- **Unified model**: Function definitions, registries, events, and subscriptions.
- **Thread safety**: Local runners and schedulers for safe execution.
- **Decoupling**: Apps call through a stable API without depending on provider details.

.. _service-manager-index-sec-03:

Communication Architecture
----------------------------

Apps bind services through `ServiceManager`, then use `ServiceBase` and service helpers to access function and event registries with minimal overhead.

.. only:: html

   .. raw:: html
      :file: ../../../../_static/mermaid/en/service/manager/index/diagram_local.html

.. only:: latex

   .. image:: ../../../../_static/mermaid/en/service/manager/index/diagram_local.png
      :width: 100%

.. _service-manager-index-sec-04:

Modules
-------

.. _service-manager-index-sec-05:

Service Runtime
~~~~~~~~~~~~~~~

Lifecycle, dispatch, binding, and local execution built on ``ServiceBase``, ``ServiceManager``, and ``LocalTestRunner``.

:doc:`API reference </service/framework/manager/service>`

.. _service-manager-index-sec-06:

Function System
~~~~~~~~~~~~~~~

Defines callable interfaces, validates parameters, and dispatches to handlers through the function model and registry.

:doc:`API reference </service/framework/manager/function>`

.. _service-manager-index-sec-07:

Event System
~~~~~~~~~~~~

Event definitions, validation, and dispatch to local subscribers.

:doc:`API reference </service/framework/manager/event>`

.. _service-manager-index-sec-08:

Common
~~~~~~

Shared types and macros for the service manager.

:doc:`API reference </service/framework/manager/common>`

.. toctree::
   :hidden:

   Service runtime <service>
   Function system <function>
   Event system <event>
   Common <common>
