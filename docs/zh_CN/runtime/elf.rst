.. _runtime-elf-sec-00:

ELF 运行时
================

:link_to_translation:`en:[English]`

.. rubric:: 概述

``brookesia_runtime_elf`` 在目标平台支持时加载原生 ELF 运行时应用。

.. rubric:: 核心职责

- 实现 Runtime Manager 后端接口。
- 通过包元数据定位可执行产物。
- 在统一应用生命周期合约后运行原生代码。

.. rubric:: 集成位置

该组件是 ESP-Brookesia 发布组件清单中的独立组件，可通过 ESP-IDF 组件依赖集成，并与同层框架组件按需组合。

.. rubric:: API 参考

.. include-build-file:: inc/runtime/brookesia_runtime_elf/include/brookesia/runtime_elf/backend.inc
