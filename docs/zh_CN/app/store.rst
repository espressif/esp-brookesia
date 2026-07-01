.. _app-store-sec-00:

应用商店
==========

:link_to_translation:`en:[English]`

.. rubric:: 概述

``brookesia_app_store`` 管理 System Core 安装环境中的本地和远端应用包。

.. rubric:: 核心职责

- 通过 HTTP 服务获取远端应用索引。
- 下载、安装、移除和列出应用包。
- 通过 Storage 与 System Core 应用管理接口处理包数据。

.. rubric:: 集成位置

该组件是 ESP-Brookesia 发布组件清单中的独立组件，可通过 ESP-IDF 组件依赖集成，并与同层框架组件按需组合。

.. rubric:: API 参考

.. include-build-file:: inc/app_store_app.inc
