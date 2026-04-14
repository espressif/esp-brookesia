.. _service-wifi-sec-00:

Wi-Fi
=====

:link_to_translation:`zh_CN:[中文]`

- Component registry: `espressif/brookesia_service_wifi <https://components.espressif.com/components/espressif/brookesia_service_wifi>`_
- Helper header: ``#include "brookesia/service_helper/wifi.hpp"``
- Helper class: ``esp_brookesia::service::helper::Wifi``

.. _service-wifi-sec-01:

Overview
--------

`brookesia_service_wifi` manages Wi-Fi connectivity:

- **State machine**: Init, start, connect, and related lifecycle states.
- **Auto reconnect**: Reconnect to known APs after drops.
- **Scanning**: Periodic scans and discovery of connectable APs.
- **Connection management**: Target AP and connected-AP lists with history.
- **Events**: Rich notifications for state changes.
- **Persistence**: Optional `brookesia_service_nvs` for credentials and parameters.

.. _service-wifi-sec-02:

Features
--------

.. _service-wifi-sec-03:

State Machine
^^^^^^^^^^^^^

The Wi-Fi service manages lifecycle states in a state machine for safe, consistent transitions.
The state machine has four core states:

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - State
     - Description
   * - ``Idle``
     - Wi-Fi not initialized (initial state).
   * - ``Inited``
     - Initialized but not started; parameters can be configured.
   * - ``Started``
     - Started; scanning or waiting to connect.
   * - ``Connected``
     - Associated with an AP and ready for traffic.

Transitions use **actions**:

- **Forward**: Idle → Inited (Init) → Started (Start) → Connected (Connect)
- **Disconnect**: Connected → Started (Disconnect)
- **Stop**: Started / Connected → Inited (Stop)
- **Deinit**: Inited → Idle (Deinit)

State transition diagram:

.. only:: html


   .. mermaid::

      stateDiagram-v2
        direction TB

        %% State Styles
        classDef Stable fill:#DEFFF8,stroke:#46EDC8,color:#378E7A,font-weight:bold;
        classDef Transient fill:#FFEFDB,stroke:#FBB35A,color:#8F632D,font-weight:bold;

        %% Initial State
        [*] --> Idle

        %% =====================
        %% Idle
        %% =====================
        Idle --> Initing: Init

        %% =====================
        %% Initing
        %% =====================
        state Initing {
          do_init() --> poll_until_inited()
        }

        Initing --> Inited: Success
        Initing --> Idle: Failure
        Initing --> Deiniting: Timeout

        %% =====================
        %% Inited
        %% =====================
        Inited --> Starting: Start
        Inited --> Deiniting: Deinit

        %% =====================
        %% Deiniting
        %% =====================
        state Deiniting {
          do_deinit() --> poll_until_deinited()
        }

        Deiniting --> Idle: Success / Failure / Timeout

        %% =====================
        %% Starting
        %% =====================
        state Starting {
          do_start() --> poll_until_started()
        }

        Starting --> Started: Success
        Starting --> Inited: Failure
        Starting --> Stopping: Timeout

        %% =====================
        %% Started
        %% =====================
        Started --> Connecting: Connect
        Started --> Stopping: Stop

        %% =====================
        %% Connecting
        %% =====================
        state Connecting {
          do_connect() --> poll_until_connected()
        }

        Connecting --> Connected: Success
        Connecting --> Started: Failure
        Connecting --> Disconnecting: Timeout

        %% =====================
        %% Connected
        %% =====================
        Connected --> Disconnecting: Disconnect
        Connected --> Stopping: Stop

        %% =====================
        %% Disconnecting
        %% =====================
        state Disconnecting {
          do_disconnect() --> poll_until_disconnected()
        }

        Disconnecting --> Started: Success / Failure / Timeout

        %% =====================
        %% Stopping
        %% =====================
        state Stopping {
          do_stop() --> poll_until_stopped()
        }

        Stopping --> Inited: Success / Failure / Timeout

        class Idle,Inited,Started,Connected Stable
        class Initing,Starting,Connecting,Disconnecting,Stopping,Deiniting Transient

.. only:: latex

   .. image:: ../../_static/service/wifi_diagram.png
      :width: 100%

.. _service-wifi-sec-04:

Auto Reconnect
^^^^^^^^^^^^^^

- Connect to historical APs after start.
- Reconnect after unexpected disconnects.
- Auto-connect when a target or historical AP appears during scan.

.. _service-wifi-sec-05:

Wi-Fi Scanning
^^^^^^^^^^^^^^

- Configurable interval and timeout.
- Events with SSID, signal level, security, etc.

.. _service-wifi-sec-06:

SoftAP
^^^^^^

- SSID, password, max clients, channel.
- Optional automatic best-channel selection.
- SoftAP provisioning mode.

.. include-build-file:: contract_guides/service/wifi.inc
