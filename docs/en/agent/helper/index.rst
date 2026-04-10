.. _agent-helper-index-sec-00:

Agent Helper
============

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_agent_helper <https://components.espressif.com/components/espressif/brookesia_agent_helper>`_
- Public header: ``#include "brookesia/agent_helper.hpp"``

.. _agent-helper-index-sec-01:

Overview
--------

`brookesia_agent_helper` uses C++20 concepts and CRTP to provide type-safe schemas and a unified calling surface for agents.

.. _agent-helper-index-sec-02:

Features
--------

- **Type-safe definitions**: Strong enums and structs with compile-time checks.
- **Schemas**: Standard function and event metadata (names, parameters, descriptions).
- **Calls**: Type-safe sync/async calls with conversion and error handling.
- **Events**: Type-safe subscriptions and callbacks.

.. _agent-helper-index-sec-03:

Modules
-------

.. toctree::
   :maxdepth: 1

   Manager <manager>
   Coze <coze>
   OpenAI <openai>
   XiaoZhi <xiaozhi>
