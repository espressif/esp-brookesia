.. _agent-manager-index-sec-00:

Agent Manager
=============

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_agent_manager <https://components.espressif.com/components/espressif/brookesia_agent_manager>`_
- Helper header: ``#include "brookesia/service_helper/agent/manager.hpp"``
- Helper class: ``esp_brookesia::service::helper::AgentManager``

.. _agent-manager-index-sec-01:

Overview
--------

`brookesia_agent_manager` is the core agent framework:

- **Lifecycle**: Plugin-based init, activate, start, stop, sleep, wake, and dynamic switching.
- **State machine**: Correct, consistent transitions.
- **Control**: Pause/resume, interrupt, status queries.
- **Dialog modes**: RealTime and Manual (manual listen start/stop).
- **Events**: Operations, state, speech/listen, text, emotes—decoupled from apps.
- **Extensions**: Function calling, text, interrupt, emotes via agent properties.
- **AFE (optional)**: Wake begin/end handling.
- **Services**: Audio service and Device time-sync integration.
- **Persistence**: Optional `brookesia_service_storage` for active agent and mode.

.. _agent-manager-index-sec-02:

State Machine Architecture
--------------------------

The manager uses a state machine for agent lifecycle.

.. _agent-manager-index-sec-03:

State Diagram
^^^^^^^^^^^^^

.. only:: html

   .. raw:: html
      :file: ../../../../_static/mermaid/en/service/agent/manager/index/diagram.html

.. only:: latex

   .. image:: ../../../../_static/mermaid/en/service/agent/manager/index/diagram.png
      :width: 100%

.. _agent-manager-index-sec-04:

State Descriptions
^^^^^^^^^^^^^^^^^^

.. list-table::
   :widths: 22 16 62
   :header-rows: 1

   * - State
     - Type
     - Description
   * - **TimeSyncing**
     - Transient
     - Waiting for time sync.
   * - **Ready**
     - Stable
     - Time OK (or skipped); waiting for activate.
   * - **Activating**
     - Transient
     - Activating agent.
   * - **Activated**
     - Stable
     - Activated; waiting for start.
   * - **Starting**
     - Transient
     - Starting agent.
   * - **Started**
     - Stable
     - Running; audio in/out enabled.
   * - **Sleeping**
     - Transient
     - Entering sleep.
   * - **Slept**
     - Stable
     - Asleep; can wake or stop.
   * - **WakingUp**
     - Transient
     - Waking from sleep.
   * - **Stopping**
     - Transient
     - Stopping agent.

.. include-build-file:: contract_guides/agent/manager.inc

.. _agent-manager-index-sec-05:

API Reference
-------------

.. _agent-manager-index-sec-06:

Agent Base Class
^^^^^^^^^^^^^^^^

Public header: ``#include "brookesia/agent_manager/base.hpp"``

.. include-build-file:: inc/service/agent/brookesia_agent_manager/include/brookesia/agent_manager/base.inc

.. _agent-manager-index-sec-07:

Agent Manager
^^^^^^^^^^^^^

Public header: ``#include "brookesia/agent_manager/manager.hpp"``

.. include-build-file:: inc/service/agent/brookesia_agent_manager/include/brookesia/agent_manager/manager.inc
