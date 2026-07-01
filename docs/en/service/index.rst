.. _service-index-sec-00:

Service Components
==================

:link_to_translation:`zh_CN:[中文]`

This section documents the ESP-Brookesia service framework and publishable service-family components.

.. only:: html

   .. raw:: html
      :file: ../../_static/mermaid/en/service/index/diagram.html

.. only:: latex

   .. image:: ../../_static/mermaid/en/service/index/diagram.png
      :width: 100%

.. rubric:: Component Responsibilities

- Service Manager handles plugin lifecycle, function routing, event dispatch, and local calls.
- Service Helper provides type-safe schemas and helper calls.
- The service family is organized by framework, network, media, system-service, agent, expression, and emulation components.

.. rubric:: Component Categories

.. toctree::
   :maxdepth: 1

   Framework <framework/index>
   Network <network/index>
   Media <media/index>
   System Services <system/index>
   AI Agents <agent/index>
   AI Expression <expression/index>
   Emulation <emulation/index>
