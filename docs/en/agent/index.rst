.. _agent-index-sec-00:

AI Agent Components
===================

:link_to_translation:`zh_CN:[中文]`

This section documents ESP-Brookesia agent framework components. The framework consists of an agent framework layer and a concrete-agents layer. Their component hierarchy is shown below:

.. only:: html

   .. mermaid::

      flowchart TD
          App["App / User Code"]
          Helper["**brookesia_agent_helper**<br/>· CRTP type-safe helper base class<br/>· Agent function / event schema definitions<br/>· call_function / subscribe_event"]
          Manager["**brookesia_agent_manager**<br/>· Agent plugin lifecycle management<br/>· State machine control (Activate / Start / Sleep / Stop)<br/>· Integrates audio & SNTP services"]
          Agents["**Agents (built on agent_manager)**<br/>Coze · OpenAI · XiaoZhi"]

          App -->|"call function / subscribe event"| Helper
          Helper -->|"built on"| Manager
          Agents -->|"register as agent plugins"| Manager

.. only:: latex

   .. image:: ../../_static/agent/index_diagram_en.png
      :width: 100%

- ``brookesia_agent_manager``: The agent framework core, responsible for plugin registration, state machine lifecycle control, and integration of audio and time-sync services.
- ``brookesia_agent_helper``: A CRTP-based type-safe helper layer that simplifies agent function/event definition and invocation, built on top of ``service_helper``.
- **Agents**: Concrete AI platform integrations implemented on top of ``agent_manager``, registered into the framework and managed uniformly by upper layers.

.. _agent-index-sec-01:

AI Agent Framework
==================

.. toctree::
   :maxdepth: 1

   Agent Manager <manager/index>
   Agent Helper <helper/index>

.. _agent-index-sec-02:

Agents
------

.. toctree::
   :maxdepth: 1

   Coze <coze>
   OpenAI <openai>
   XiaoZhi <xiaozhi>
