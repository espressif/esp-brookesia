.. _runtime-js-sec-00:

JS 运行时
==============

:link_to_translation:`en:[English]`

.. rubric:: 概述

``brookesia_runtime_js`` 通过 Brookesia 原生桥接运行 JavaScript 应用。

.. rubric:: 核心职责

- 把原生模块注册为 JavaScript 全局对象。
- 支持同步和异步服务桥接函数。
- 在运行时应用包中使用 ES module 入口文件。

.. rubric:: 集成位置

该组件是 ESP-Brookesia 发布组件清单中的独立组件，可通过 ESP-IDF 组件依赖集成，并与同层框架组件按需组合。

.. rubric:: API 参考

.. include-build-file:: inc/runtime/brookesia_runtime_js/include/brookesia/runtime_js/backend.inc
