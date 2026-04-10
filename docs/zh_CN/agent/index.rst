.. _agent-index-sec-00:

AI 智能体组件
=============

:link_to_translation:`en:[English]`

本分类包含 ESP-Brookesia 智能体框架组件的说明内容。ESP-Brookesia 智能体框架由智能体框架层和具体智能体层组成，各组件的层级关系如下：

.. only:: html

   .. mermaid::

      flowchart TD
          App["App / 用户代码"]
          Helper["**brookesia_agent_helper**<br/>· CRTP 类型安全辅助基类<br/>· 智能体函数 / 事件 Schema 定义<br/>· call_function / subscribe_event"]
          Manager["**brookesia_agent_manager**<br/>· 智能体插件生命周期管理<br/>· 状态机控制（激活 / 启动 / 休眠 / 停止）<br/>· 集成音频与 SNTP 服务"]
          Agents["**具体智能体（基于 agent_manager 实现）**<br/>Coze · OpenAI · 小智（XiaoZhi）"]

          App -->|"调用函数 / 订阅事件"| Helper
          Helper -->|"构建在"| Manager
          Agents -->|"注册为智能体插件"| Manager

.. only:: latex

   .. image:: ../../_static/agent/index_diagram_cn.svg
      :width: 100%

- ``brookesia_agent_manager``：智能体框架核心，负责智能体插件注册、状态机生命周期控制，并集成音频与时间同步服务
- ``brookesia_agent_helper``：基于 CRTP 的类型安全辅助层，简化智能体的函数/事件定义与调用方式，构建在 ``service_helper`` 之上
- **具体智能体**：基于 ``agent_manager`` 实现的特定 AI 平台接入，注册到框架后可被上层统一管理和调用

.. _agent-index-sec-01:

智能体框架
-------------

.. toctree::
   :maxdepth: 1

   智能体管理器 <manager/index>
   智能体辅助 <helper/index>

.. _agent-index-sec-02:

智能体
----------

.. toctree::
   :maxdepth: 1

   Coze <coze>
   OpenAI <openai>
   小智 <xiaozhi>
