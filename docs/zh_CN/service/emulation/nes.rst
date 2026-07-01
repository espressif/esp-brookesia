.. _service-nes-sec-00:

NES 服务
==============

:link_to_translation:`en:[English]`

- 辅助头文件： ``#include "brookesia/service_helper/emulation/nes.hpp"``
- 辅助类： ``esp_brookesia::service::helper::Nes``

.. rubric:: 概述

``brookesia_emulation_nes`` 为 NES 模拟器运行时暴露服务控制面。

.. rubric:: 核心职责

- 控制 ROM 加载、启动、暂停、停止、存档和手柄状态。
- 通过 Display 服务源输出画面，而不是通过 RPC 传输帧。
- 在服务组件内部维护 nofrendo 后端代码。

.. rubric:: 集成位置

该组件是 ESP-Brookesia 发布组件清单中的独立组件，可通过 ESP-IDF 组件依赖集成，并与同层框架组件按需组合。

.. include-build-file:: contract_guides/service/nes.inc
