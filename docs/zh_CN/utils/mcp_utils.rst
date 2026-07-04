.. _utils-mcp-utils-sec-00:

MCP Utils
=========

:link_to_translation:`en:[English]`

.. rubric:: 概述

``brookesia_mcp_utils`` 将 Brookesia 服务函数和自定义回调桥接为 ESP MCP 工具。

.. rubric:: 核心职责

- 把服务函数注册为可调用的 MCP 工具。
- 根据服务 schema 元数据构建工具描述和处理函数。
- 让 AI 智能体通过 MCP 兼容工具调用设备能力。

.. rubric:: 集成位置

该组件是 ESP-Brookesia 发布组件清单中的独立组件，可通过 ESP-IDF 组件依赖集成，并与同层框架组件按需组合。

