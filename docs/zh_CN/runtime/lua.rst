.. _runtime-lua-sec-00:

Lua 运行时
================

:link_to_translation:`en:[English]`

.. rubric:: 概述

``brookesia_runtime_lua`` 通过共享的 Runtime Manager 合约运行 Lua 应用。

.. rubric:: 核心职责

- 为运行时应用包提供 Lua 后端。
- 向脚本代码暴露宿主桥接函数。
- 与其他运行时后端共享包发现和生命周期处理。

.. rubric:: 集成位置

该组件是 ESP-Brookesia 发布组件清单中的独立组件，可通过 ESP-IDF 组件依赖集成，并与同层框架组件按需组合。

.. rubric:: API 参考

.. include-build-file:: inc/runtime/brookesia_runtime_lua/include/brookesia/runtime_lua/backend.inc
