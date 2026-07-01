.. _app-files-sec-00:

文件应用
==========

:link_to_translation:`en:[English]`

.. rubric:: 概述

``brookesia_app_files`` 是 System Core 应用包中的内置文件浏览应用。

.. rubric:: 核心职责

- 浏览已挂载的存储文件系统。
- 通过 GUI 资源展示目录和文件。
- 使用 Storage 服务与 System Core 应用生命周期钩子。

.. rubric:: 集成位置

该组件是 ESP-Brookesia 发布组件清单中的独立组件，可通过 ESP-IDF 组件依赖集成，并与同层框架组件按需组合。

.. rubric:: API 参考

.. include-build-file:: inc/files_app.inc
