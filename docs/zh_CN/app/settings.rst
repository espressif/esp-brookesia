.. _app-settings-sec-00:

设置应用
==========

:link_to_translation:`en:[English]`

.. rubric:: 概述

``brookesia_app_settings`` 为基于 System Super 的产品提供设备与系统设置页面。

.. rubric:: 核心职责

- 按主页、Wi-Fi、声音、显示、语言、时区等页面组织设置。
- 通过服务辅助接口读取 Wi-Fi、设备、音频、显示和存储状态。
- 从应用包 i18n 资源加载本地化名称和提示。

.. rubric:: 集成位置

该组件是 ESP-Brookesia 发布组件清单中的独立组件，可通过 ESP-IDF 组件依赖集成，并与同层框架组件按需组合。

.. rubric:: API 参考

.. include-build-file:: inc/settings_app.inc
