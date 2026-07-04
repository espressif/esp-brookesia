.. _service-http-sec-00:

HTTP 服务
================

:link_to_translation:`en:[English]`

- 辅助头文件： ``#include "brookesia/service_helper/network/http.hpp"``
- 辅助类： ``esp_brookesia::service::helper::Http``

.. rubric:: 概述

``brookesia_service_http`` 以 Brookesia 服务函数和事件提供 HTTP/HTTPS 客户端请求能力。

.. rubric:: 核心职责

- 把 ESP-IDF HTTP client 行为封装在服务 schema 后。
- 支持请求提交、取消、内存响应、文件下载、进度和完成事件。
- 供应用和智能体获取包索引、下载、云配置和网络资源。

.. rubric:: 集成位置

该组件是 ESP-Brookesia 发布组件清单中的独立组件，可通过 ESP-IDF 组件依赖集成，并与同层框架组件按需组合。

.. include-build-file:: contract_guides/service/http.inc
