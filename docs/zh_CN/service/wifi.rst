.. _service-wifi-sec-00:

Wi-Fi
=====

:link_to_translation:`en:[English]`

- 组件注册表： `espressif/brookesia_service_wifi <https://components.espressif.com/components/espressif/brookesia_service_wifi>`_
- 辅助头文件： ``#include "brookesia/service_helper/wifi.hpp"``
- 辅助类： ``esp_brookesia::service::helper::Wifi``

.. _service-wifi-sec-01:

概述
----

`brookesia_service_wifi` 是为 ESP-Brookesia 生态系统提供的 WiFi 连接管理服务，提供：

- **状态机管理**：通过状态机统一管理 WiFi 的初始化、启动、连接等生命周期状态
- **自动重连**：支持自动连接历史 AP，并在断开后自动尝试重连
- **WiFi 扫描**：支持周期性扫描周围 AP，并自动发现可连接的 AP
- **连接管理**：管理目标 AP 和已连接 AP 列表，支持多 AP 历史记录
- **事件通知**：提供丰富的事件通知机制，实时反馈 WiFi 状态变化
- **持久化存储**：可选搭配 `brookesia_service_nvs` 服务持久化保存连接配置和其他参数

.. _service-wifi-sec-02:

功能特性
--------

.. _service-wifi-sec-03:

状态机管理
^^^^^^^^^^

WiFi 服务通过状态机统一管理 WiFi 的生命周期状态，确保状态转换的安全性和一致性。状态机包含 4 个核心状态：

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - 状态
     - 说明
   * - ``Idle``
     - WiFi 未初始化，系统初始状态
   * - ``Inited``
     - WiFi 已初始化，但未启动，可以配置参数
   * - ``Started``
     - WiFi 已启动，正在扫描或等待连接
   * - ``Connected``
     - WiFi 已成功连接到 AP，可以正常通信

状态转换通过触发相应的动作（Action）来实现：

- **正向流程**：Idle → Inited（Init）→ Started（Start）→ Connected（Connect）
- **断开连接**：Connected → Started（Disconnect）
- **停止流程**：Started / Connected → Inited（Stop）
- **反初始化**：Inited → Idle（Deinit）

状态转换图如下：

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

   .. image:: ../../_static/service/wifi_diagram.svg
      :width: 100%

.. _service-wifi-sec-04:

自动重连机制
^^^^^^^^^^^^

- **启动时自动连接**：WiFi 启动后自动尝试连接历史可连接的 AP
- **断开后自动重连**：检测到意外断开后，自动尝试连接历史可连接的 AP
- **扫描发现自动连接**：扫描过程中发现目标 AP 或历史可连接 AP 时，自动触发连接

.. _service-wifi-sec-05:

WiFi 扫描
^^^^^^^^^

- **周期性扫描**：支持配置扫描间隔和超时时间
- **扫描结果通知**：通过事件实时通知扫描到的 AP 信息
- **AP 信息**：包含 SSID、信号强度等级、是否加密等信息

.. _service-wifi-sec-06:

SoftAP 功能
^^^^^^^^^^^

- **参数设置**：支持设置 SoftAP 的 SSID、密码、最大连接数和通道
- **最优通道选择**：如果未设置通道，则会自动扫描附近 AP 并选择最佳通道
- **配网功能**：支持启动 SoftAP 配网功能

.. include-build-file:: contract_guides/service/wifi.inc
