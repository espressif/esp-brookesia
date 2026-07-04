.. _runtime-wasm-sec-00:

WASM 运行时
==================

:link_to_translation:`en:[English]`

.. rubric:: 概述

``brookesia_runtime_wasm`` 在统一运行时后端接口后执行 WebAssembly 应用。

.. rubric:: 核心职责

- 实现 WASM 产物的运行时加载。
- 把 WebAssembly 代码连接到原生宿主桥接函数。
- 面向沙箱化或可移植运行时应用部署。

.. rubric:: 集成位置

该组件是 ESP-Brookesia 发布组件清单中的独立组件，可通过 ESP-IDF 组件依赖集成，并与同层框架组件按需组合。

.. rubric:: API 参考

.. include-build-file:: inc/runtime/brookesia_runtime_wasm/include/brookesia/runtime_wasm/backend.inc
