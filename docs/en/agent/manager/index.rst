.. _agent-manager-index-sec-00:

Agent Manager
===============

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_agent_manager <https://components.espressif.com/components/espressif/brookesia_agent_manager>`_
- Helper header: ``#include "brookesia/agent_helper/manager.hpp"``
- Helper class: ``esp_brookesia::agent::helper::Manager``

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
- **Services**: Audio and SNTP integration.
- **Persistence**: Optional `brookesia_service_nvs` for active agent and mode.

.. _agent-manager-index-sec-02:

State machine
-------------

The manager uses a state machine for agent lifecycle.

.. _agent-manager-index-sec-03:

State diagram
~~~~~~~~~~~~~

.. only:: html


   .. mermaid::

      stateDiagram-v2
        direction TB

        %% State Styles
        classDef Stable fill:#DEFFF8,stroke:#46EDC8,color:#378E7A,font-weight:bold;
        classDef Transient fill:#FFEFDB,stroke:#FBB35A,color:#8F632D,font-weight:bold;

        %% Initial State
        [*] --> TimeSyncing
        [*] --> Ready: Bypass

        %% =====================
        %% TimeSyncing (Optional)
        %% =====================
        state TimeSyncing {
          do_time_sync() --> poll_until_time_synced()
        }

        TimeSyncing --> Ready: Success

        %% =====================
        %% Ready
        %% =====================
        Ready --> Activating: Activate

        %% =====================
        %% Activating
        %% =====================
        state Activating {
          do_activate() --> poll_until_activated()
        }

        Activating --> Activated: Success
        Activating --> Stopping: Stop
        Activating --> Ready: Failure / Timeout

        %% =====================
        %% Activated
        %% =====================
        Activated --> Starting: Start
        Activated --> Stopping: Stop

        %% =====================
        %% Starting
        %% =====================
        state Starting {
          do_start() --> poll_until_started()
        }

        Starting --> Started: Success
        Starting --> Stopping: Stop
        Starting --> Ready: Failure / Timeout

        %% =====================
        %% Started
        %% =====================
        Started --> Sleeping: Sleep
        Started --> Stopping: Stop

        %% =====================
        %% Sleeping
        %% =====================
        state Sleeping {
          do_Sleep() --> poll_until_Slept()
        }

        Sleeping --> Slept: Success
        Sleeping --> Stopping: Stop
        Sleeping --> Ready: Failure / Timeout

        %% =====================
        %% Slept
        %% =====================
        Slept --> WakingUp: WakeUp
        Slept --> Stopping: Stop

        %% =====================
        %% WakingUp
        %% =====================
        state WakingUp {
          do_wakeup() --> poll_until_awake()
        }

        WakingUp --> Started: Success
        WakingUp --> Stopping: Stop
        WakingUp --> Ready: Failure / Timeout

        %% =====================
        %% Stopping
        %% =====================
        state Stopping {
          do_stop() --> poll_until_stopped()
        }

        Stopping --> Ready: Success / Failure / Timeout

        class Ready Stable
        class Activated Stable
        class Started Stable
        class Slept Stable
        class TimeSyncing Transient
        class Starting Transient
        class Activating Transient
        class Sleeping Transient
        class WakingUp Transient
        class Stopping Transient

.. only:: latex

   .. image:: ../../../_static/agent/manager_diagram.svg
      :width: 100%

.. _agent-manager-index-sec-04:

States
~~~~~~

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

.. _agent-manager-index-sec-05:

API reference
-------------

.. _agent-manager-index-sec-06:

Agent base
~~~~~~~~~~

Public header: ``#include "brookesia/agent_manager/base.hpp"``

.. include-build-file:: inc/agent/brookesia_agent_manager/include/brookesia/agent_manager/base.inc

.. _agent-manager-index-sec-07:

Agent manager
~~~~~~~~~~~~~

Public header: ``#include "brookesia/agent_manager/manager.hpp"``

.. include-build-file:: inc/agent/brookesia_agent_manager/include/brookesia/agent_manager/manager.inc
