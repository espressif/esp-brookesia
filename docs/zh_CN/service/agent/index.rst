.. _agent-index-sec-00:

AI 智能体组件
=============

:link_to_translation:`en:[English]`

本分类包含 ESP-Brookesia 智能体框架组件的说明内容。ESP-Brookesia 智能体框架由智能体框架层和具体智能体层组成，各组件的层级关系如下：

.. only:: html

   .. raw:: html
      :file: ../../../_static/mermaid/zh_CN/service/agent/index/diagram.html

.. only:: latex

   .. image:: ../../../_static/mermaid/zh_CN/service/agent/index/diagram.png
      :width: 100%

- ``brookesia_agent_manager``：智能体框架核心，负责智能体插件注册、状态机生命周期控制，并集成音频与时间同步服务
- ``brookesia_service_helper``：统一的服务辅助组件，当前也承载智能体 helper 合约
- **具体智能体**：基于 ``agent_manager`` 实现的特定 AI 平台接入，注册到框架后可被上层统一管理和调用

.. _agent-index-sec-01:

智能体框架
-------------

.. toctree::
   :maxdepth: 1

   智能体管理器 <manager/index>

.. _agent-index-sec-02:

智能体
----------

.. toctree::
   :maxdepth: 1

   Coze <coze>
   OpenAI <openai>
   小智 <xiaozhi>
