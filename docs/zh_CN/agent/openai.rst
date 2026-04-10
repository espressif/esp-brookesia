.. _agent-openai-sec-00:

OpenAI
======

:link_to_translation:`en:[English]`

- 组件注册表： `espressif/brookesia_agent_openai <https://components.espressif.com/components/espressif/brookesia_agent_openai>`_
- 辅助头文件： ``#include "brookesia/agent_helper/openai.hpp"``
- 辅助类： ``esp_brookesia::agent::helper::Openai``

.. _agent-openai-sec-01:

概述
----

`brookesia_agent_openai` 是基于 ESP-Brookesia Agent Manager 框架实现的 OpenAI 智能体。

.. _agent-openai-sec-02:

功能特性
--------

- **OpenAI Realtime API 集成**：通过 WebRTC 数据通道与 OpenAI 平台进行实时通信，支持语音对话和文本交互。
- **点对点通信**：基于 ``esp_peer`` 实现点对点（P2P）通信，支持信号通道和数据通道。
- **音频编解码**：内置音频编解码支持，默认使用 OPUS 格式，支持 16kHz 采样率和 24kbps 比特率。
- **数据通道消息处理**：支持多种数据通道消息类型，包括音频流、文本流、函数调用、会话管理等。
- **实时交互**：支持实时音频输入输出，提供低延迟的语音交互体验。
- **持久化存储**：可选搭配 `brookesia_service_nvs` 服务持久化保存配置信息。

.. include-build-file:: contract_guides/agent/openai.inc
