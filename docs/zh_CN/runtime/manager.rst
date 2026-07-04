.. _runtime-manager-sec-00:

运行时管理器
==============

:link_to_translation:`en:[English]`

.. rubric:: 概述

``brookesia_runtime_manager`` 是加载运行时应用 manifest、原生桥接和应用生命周期回调的宿主框架。

.. rubric:: 核心职责

- 定义运行时后端注册和应用上下文归属。
- 提供 context attach、detach、finish、print 等原生宿主桥接函数。
- 把 System Core 运行时应用包连接到具体后端。

.. rubric:: 集成位置

该组件是 ESP-Brookesia 发布组件清单中的独立组件，可通过 ESP-IDF 组件依赖集成，并与同层框架组件按需组合。

.. rubric:: API 参考

.. include-build-file:: inc/runtime.inc
