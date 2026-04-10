.. _agent-manager-index-sec-00:

AI 智能体管理器
===============

:link_to_translation:`en:[English]`

- 组件注册表： `espressif/brookesia_agent_manager <https://components.espressif.com/components/espressif/brookesia_agent_manager>`_
- 辅助头文件： ``#include "brookesia/agent_helper/manager.hpp"``
- 辅助类： ``esp_brookesia::agent::helper::Manager``

.. _agent-manager-index-sec-01:

概述
----

`brookesia_agent_manager` 是 ESP-Brookesia 智能体管理框架核心组件，提供：

- **统一的智能体生命周期管理**：通过插件机制，集中式管理智能体的初始化、激活、启动、停止、休眠和唤醒，支持智能体之间的动态切换。
- **状态机管理**：基于状态机自动管理智能体的状态转换，确保状态转换的正确性和一致性。
- **智能体操作控制**：支持智能体的暂停/恢复、中断说话、状态查询等操作，提供完整的智能体控制能力。
- **对话模式支持**：支持实时对话（RealTime）和手动对话（Manual）两种模式，手动模式下支持手动开始/停止监听。
- **事件驱动架构**：支持通用操作事件、状态变化事件、说话/监听状态事件、文本交互事件和表情事件，实现智能体与应用之间的解耦通信。
- **功能扩展支持**：支持函数调用、文本处理、中断说话、表情等扩展功能，通过智能体属性配置灵活启用。
- **AFE 事件处理**：可选启用 AFE（音频前端）事件处理，自动响应唤醒开始/结束事件。
- **服务集成**：基于服务框架，提供统一的服务接口，集成音频服务和 SNTP 服务。
- **持久化存储**：可选搭配 `brookesia_service_nvs` 服务持久化保存智能体激活状态和对话模式等信息。

.. _agent-manager-index-sec-02:

状态机架构
----------

`brookesia_agent_manager` 使用状态机管理智能体的生命周期，确保状态转换的正确性和一致性。

.. _agent-manager-index-sec-03:

状态机图
~~~~~~~~

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

状态说明
~~~~~~~~

.. list-table::
   :widths: 22 16 62
   :header-rows: 1

   * - 状态
     - 类型
     - 说明
   * - **TimeSyncing**
     - 瞬态
     - 正在同步时间，等待时间同步完成事件
   * - **Ready**
     - 稳定
     - 就绪状态，时间已同步（或跳过），等待激活命令
   * - **Activating**
     - 瞬态
     - 正在激活智能体，等待激活完成事件
   * - **Activated**
     - 稳定
     - 智能体已激活，等待启动命令
   * - **Starting**
     - 瞬态
     - 正在启动智能体，等待启动完成事件
   * - **Started**
     - 稳定
     - 智能体已启动，可以接收音频输入和输出
   * - **Sleeping**
     - 瞬态
     - 正在休眠智能体，等待休眠完成事件
   * - **Slept**
     - 稳定
     - 智能体已休眠，可以唤醒或停止
   * - **WakingUp**
     - 瞬态
     - 正在唤醒智能体，等待唤醒完成事件
   * - **Stopping**
     - 瞬态
     - 正在停止智能体，等待停止完成事件

.. _agent-manager-index-sec-05:

API 参考
--------

.. _agent-manager-index-sec-06:

智能体基类
~~~~~~~~~~~

公共头文件： ``#include "brookesia/agent_manager/base.hpp"``

.. include-build-file:: inc/agent/brookesia_agent_manager/include/brookesia/agent_manager/base.inc

.. _agent-manager-index-sec-07:

智能体管理器
~~~~~~~~~~~~~

公共头文件： ``#include "brookesia/agent_manager/manager.hpp"``

.. include-build-file:: inc/agent/brookesia_agent_manager/include/brookesia/agent_manager/manager.inc
