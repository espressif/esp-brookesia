.. _agent-xiaozhi-sec-00:

小智
====

:link_to_translation:`en:[English]`

- 组件注册表： `espressif/brookesia_agent_xiaozhi <https://components.espressif.com/components/espressif/brookesia_agent_xiaozhi>`_
- 辅助头文件： ``#include "brookesia/agent_helper/xiaozhi.hpp"``
- 辅助类： ``esp_brookesia::agent::helper::XiaoZhi``

.. _agent-xiaozhi-sec-01:

概述
----

`brookesia_agent_xiaozhi` 是为 ESP-Brookesia 生态系统提供的小智 AI 智能体实现。

.. _agent-xiaozhi-sec-02:

功能特性
--------

- **小智平台集成**：基于 ``esp_xiaozhi`` SDK 实现与小智 AI 平台的通信。
- **实时语音交互**：支持 OPUS 音频编解码，16kHz 采样率，24kbps 比特率。
- **丰富的事件支持**：支持说话/监听状态变化、智能体/用户文本、表情等事件。
- **手动监听控制**：支持手动开始/停止监听，适用于 Manual 对话模式。
- **中断说话**：支持中断智能体说话功能。
- **统一生命周期管理**：基于 ``brookesia_agent_manager`` 框架的统一智能体生命周期管理。

.. include-build-file:: contract_guides/agent/xiaozhi.inc
