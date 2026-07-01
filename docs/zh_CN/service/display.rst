.. _service-display-sec-00:

Display 服务
======================

:link_to_translation:`en:[English]`

- 辅助头文件： ``#include "brookesia/service_helper/media/display.hpp"``
- 辅助类： ``esp_brookesia::service::helper::Display``

.. rubric:: 概述

``brookesia_service_display`` 管理显示输出、显示源、活动源仲裁、帧提交、触摸手势和背光控制。

.. rubric:: 核心职责

- 注册显示源，并把帧路由到活动物理输出。
- 提供输出/源发现和活动源选择服务函数。
- 暴露触摸手势事件和绑定输出的背光操作。

.. rubric:: 集成位置

该组件是 ESP-Brookesia 发布组件清单中的独立组件，可通过 ESP-IDF 组件依赖集成，并与同层框架组件按需组合。

.. include-build-file:: contract_guides/service/display.inc
