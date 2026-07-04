.. _service-index-sec-00:

服务组件
==========

:link_to_translation:`en:[English]`

本分类说明 ESP-Brookesia 服务框架以及可发布的 service 族组件。

.. only:: html

   .. raw:: html
      :file: ../../_static/mermaid/zh_CN/service/index/diagram.html

.. only:: latex

   .. image:: ../../_static/mermaid/zh_CN/service/index/diagram.png
      :width: 100%

.. rubric:: 组件职责

- Service Manager 负责插件生命周期、函数路由、事件分发和本地调用。
- Service Helper 提供类型安全 schema 和辅助调用。
- service 族按 framework、network、media、system-service、agent、expression 和 emulation 组件组织。

.. rubric:: 组件类别

.. toctree::
   :maxdepth: 1

   服务框架 <framework/index>
   网络服务 <network/index>
   媒体服务 <media/index>
   系统服务 <system/index>
   AI 智能体 <agent/index>
   AI 表达 <expression/index>
   模拟器服务 <emulation/index>
