.. _service-manager-index-sec-00:

Service Manager
=================

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_service_manager <https://components.espressif.com/components/espressif/brookesia_service_manager>`_
- Public header: ``#include "brookesia/service_manager.hpp"``

.. _service-manager-index-sec-01:

Overview
--------

`brookesia_service_manager` is the core service framework: lifecycle management, function and event registration, local calls, and remote RPC.

.. _service-manager-index-sec-02:

Features
--------

- **Lifecycle**: Centralized init, start, stop, and deinit.
- **Dual mode**: High-performance local calls and TCP/JSON RPC.
- **Unified model**: Function definitions, registries, events, and subscriptions.
- **Thread safety**: Local runners and schedulers for safe execution.
- **Decoupling**: Apps call through a stable API without depending on provider details.

.. _service-manager-index-sec-03:

Communication Architecture
----------------------------

`brookesia_service_manager` supports **local** and **remote RPC** modes.

.. _service-manager-index-sec-04:

Local Mode
~~~~~~~~~~

After `ServiceManager` binds a service, the app uses `ServiceBase` to access function and event registries with minimal overhead.

.. only:: html

   .. raw:: html
      :file: ../../../_static/mermaid/en/service/manager/index/diagram_local.html

.. only:: latex

   .. image:: ../../../_static/mermaid/en/service/manager/index/diagram_local.png
      :width: 100%

.. _service-manager-index-sec-05:

Remote RPC Mode
~~~~~~~~~~~~~~~

Clients use TCP sockets and JSON to reach remote services—suitable for cross-device or cross-language use.

.. only:: html

   .. raw:: html
      :file: ../../../_static/mermaid/en/service/manager/index/diagram_rpc.html

.. only:: latex

   .. image:: ../../../_static/mermaid/en/service/manager/index/diagram_rpc.png
      :width: 100%

.. _service-manager-index-sec-06:

Local vs Remote RPC
~~~~~~~~~~~~~~~~~~~

.. list-table::
   :widths: 18 41 41
   :header-rows: 1

   * - Item
     - Local (`ServiceBase`)
     - Remote RPC (`rpc::Client`)
   * - Deployment
     - Same device
     - Cross-device
   * - Transport
     - Direct calls
     - TCP + JSON
   * - Latency
     - Low (ms)
     - Higher (network)
   * - Performance
     - No serialization
     - Serialize/deserialize
   * - Call rate
     - High frequency
     - Medium/low frequency
   * - Thread safety
     - Async scheduling & guards
     - Network isolation
   * - Languages
     - C++
     - Language-agnostic
   * - Network
     - Not required
     - LAN or routable network
   * - Typical use
     - In-device collaboration
     - Cross-device / cross-language calls

.. _service-manager-index-sec-07:

Modules
-------

.. _service-manager-index-sec-08:

Service Runtime
~~~~~~~~~~~~~~~

Lifecycle, dispatch, binding, and RPC integration—built on ``ServiceBase``, ``ServiceManager``, and ``LocalTestRunner``.

:doc:`API reference </service/manager/service>`

.. _service-manager-index-sec-09:

Function System
~~~~~~~~~~~~~~~

Defines callable interfaces, validates parameters, and dispatches to handlers—function model and registry.

:doc:`API reference </service/manager/function>`

.. _service-manager-index-sec-10:

Event System
~~~~~~~~~~~~

Event definitions, validation, and dispatch to local subscribers or RPC subscribers—definitions, registry, and dispatcher.

:doc:`API reference </service/manager/event>`

.. _service-manager-index-sec-11:

RPC
~~~

TCP + JSON exposure of functions and events: protocol, data link, bridging, client, and server.

:doc:`API reference </service/manager/rpc>`

.. _service-manager-index-sec-12:

Common
~~~~~~

Shared types and macros for the service manager.

:doc:`API reference </service/manager/common>`

.. toctree::
   :hidden:

   Service runtime <service>
   Function system <function>
   Event system <event>
   RPC <rpc>
   Common <common>
