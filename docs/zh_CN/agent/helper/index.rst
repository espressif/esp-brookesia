.. _agent-helper-index-sec-00:

AI 智能体辅助
=============

:link_to_translation:`en:[English]`

- 组件注册表： `espressif/brookesia_agent_helper <https://components.espressif.com/components/espressif/brookesia_agent_helper>`_
- 公共头文件： ``#include "brookesia/agent_helper.hpp"``

.. _agent-helper-index-sec-01:

概述
----

`brookesia_agent_helper` 是 ESP-Brookesia 智能体开发辅助库，基于 C++20 Concepts 和 CRTP（Curiously Recurring Template Pattern）模式，为智能体开发者和使用者提供类型安全的定义、Schema 和统一调用接口。

.. _agent-helper-index-sec-02:

特性
----

- 类型安全定义：提供强类型枚举和结构体类型定义，确保编译时类型检查。
- Schema 定义：提供标准化的函数与事件 Schema，覆盖名称、参数类型与描述等元数据。
- 统一调用接口：提供类型安全的同步与异步函数调用接口，自动处理类型转换与错误处理。
- 事件订阅接口：提供类型安全的事件订阅接口，支持事件处理回调。

.. _agent-helper-index-sec-03:

模块介绍
--------

.. toctree::
   :maxdepth: 1

   智能体管理器 <manager>
   Coze <coze>
   OpenAI <openai>
   小智 <xiaozhi>
