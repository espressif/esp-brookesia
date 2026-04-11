.. _agent-coze-sec-00:

Coze
====

:link_to_translation:`en:[English]`

- 组件注册表： `espressif/brookesia_agent_coze <https://components.espressif.com/components/espressif/brookesia_agent_coze>`_
- 辅助头文件： ``#include "brookesia/agent_helper/coze.hpp"``
- 辅助类： ``esp_brookesia::agent::helper::Coze``

.. _agent-coze-sec-01:

概述
----

`brookesia_agent_coze` 是基于 ESP-Brookesia Agent Manager 框架实现的 Coze 智能体。

.. _agent-coze-sec-02:

功能特性
--------

- **Coze API 集成**：通过 WebSocket 与 Coze 平台进行实时通信，支持语音对话和文本交互。
- **OAuth2 认证**：支持 Coze 平台的 OAuth2 认证机制，自动获取和管理访问令牌。
- **多机器人支持**：支持配置多个机器人，可以动态切换当前激活的机器人。
- **音频编解码**：内置音频编解码支持，默认使用 G711A 格式，支持 16kHz 采样率。
- **表情支持**：支持接收和显示 Coze 平台的表情事件。
- **事件处理**：支持 Coze 平台事件（如余额不足等）的监听和处理。
- **持久化存储**：可选搭配 `brookesia_service_nvs` 服务持久化保存鉴权信息和机器人等信息。

.. include-build-file:: contract_guides/agent/coze.inc
