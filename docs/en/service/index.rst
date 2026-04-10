.. _service-index-sec-00:

Service Components
==================

:link_to_translation:`zh_CN:[中文]`

This section documents the ESP-Brookesia service framework components. The framework consists of a service framework layer and a general-services layer. Their component hierarchy is shown below:

.. only:: html

   .. mermaid::

      flowchart TD
          App["App / User Code"]
          Helper["**brookesia_service_helper**<br/>· CRTP type-safe helper base class<br/>· Function / event schema definitions<br/>· call_function / subscribe_event"]
          Manager["**brookesia_service_manager**<br/>· Service plugin lifecycle management<br/>· Thread-safe local calls & TCP RPC remote calls<br/>· Function registry & event registry"]
          Services["**General Services (built on service_helper)**<br/>NVS · SNTP · Wi-Fi · Audio · Video · Custom"]

          App -->|"call function / subscribe event"| Helper
          Helper -->|"built on"| Manager
          Services -->|"register as plugins"| Manager

.. only:: latex

   .. image:: ../../_static/service/index_diagram_en.svg
      :width: 100%

- ``brookesia_service_manager``: The service framework core, responsible for plugin registration, function routing, and event dispatching in both local and RPC communication modes.
- ``brookesia_service_helper``: A CRTP-based type-safe helper layer that simplifies service function/event definition and invocation.
- **General Services**: Concrete services implemented on top of ``service_helper``, registered into ``service_manager`` and discoverable by name from upper layers.

.. _service-index-sec-01:

Service framework
-----------------

.. toctree::
   :maxdepth: 1

   Service Manager <manager/index>
   Service Helper <helper/index>

.. _service-index-sec-02:

General services
----------------

.. toctree::
   :maxdepth: 1

   NVS <nvs>
   SNTP <sntp>
   Wi-Fi <wifi>
   Audio <audio>
   Video <video>
   Custom Service <custom>

.. _service-index-sec-03:

Development guide
-----------------

.. toctree::
   :maxdepth: 1

   Application development guide <usage>
