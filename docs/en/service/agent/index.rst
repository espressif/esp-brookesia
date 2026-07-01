.. _agent-index-sec-00:

AI Agent Components
===================

:link_to_translation:`zh_CN:[中文]`

This section documents ESP-Brookesia agent framework components. The framework consists of an agent framework layer and a concrete-agents layer. Their component hierarchy is shown below:

.. only:: html

   .. raw:: html
      :file: ../../../_static/mermaid/en/service/agent/index/diagram.html

.. only:: latex

   .. image:: ../../../_static/mermaid/en/service/agent/index/diagram.png
      :width: 100%

- ``brookesia_agent_manager``: The agent framework core, responsible for plugin registration, state machine lifecycle control, and integration of audio and time-sync services.
- ``brookesia_service_helper``: The shared helper component that now owns the agent helper contracts.
- **Agents**: Concrete AI platform integrations implemented on top of ``agent_manager``, registered into the framework and managed uniformly by upper layers.

.. _agent-index-sec-01:

AI Agent Framework
==================

.. toctree::
   :maxdepth: 1

   Agent Manager <manager/index>

.. _agent-index-sec-02:

Agents
------

.. toctree::
   :maxdepth: 1

   Coze <coze>
   OpenAI <openai>
   XiaoZhi <xiaozhi>
