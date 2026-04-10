.. _expression-emote-sec-00:

表情
====

:link_to_translation:`en:[English]`

- 组件注册表： `espressif/brookesia_expression_emote <https://components.espressif.com/components/espressif/brookesia_expression_emote>`_
- 辅助头文件： ``#include "brookesia/service_helper/expression/emote.hpp"``
- 辅助类： ``esp_brookesia::service::helper::ExpressionEmote``

.. _expression-emote-sec-01:

概述
----

`brookesia_expression_emote` 是 ESP-Brookesia 表情管理组件，基于 ESP-Brookesia 服务框架实现，提供：

- **资源管理**：支持加载和管理表情/动画资源，灵活配置资源。
- **表情控制**：支持设置和管理表情符号（emoji），用于表达不同的情感状态。
- **动画控制**：支持动画播放控制，包括等待动画帧完成、停止动画等操作。
- **二维码**：支持设置和隐藏二维码，实现二维码的显示和隐藏。
- **事件消息**：支持设置事件消息，实现事件消息的显示和隐藏。

.. include-build-file:: contract_guides/service/emote.inc
