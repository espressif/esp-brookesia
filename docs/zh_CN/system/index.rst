.. _system-index-sec-00:

系统组件
====================

:link_to_translation:`en:[English]`

System 组件位于服务、GUI、运行时、HAL 和内置应用之上，提供产品级系统框架。

.. only:: html

   .. raw:: html
      :file: ../../_static/mermaid/zh_CN/system/index/diagram.html

.. only:: latex

   .. image:: ../../_static/mermaid/zh_CN/system/index/diagram.png
      :width: 100%

.. rubric:: 组件职责

- ``brookesia_system_core`` 提供应用生命周期、运行时应用、GUI 访问、存储、定时器、服务和权限等通用核心能力。
- ``brookesia_system_super`` 在 System Core 之上提供面向矩形触摸屏手持设备的标准系统。

.. toctree::
   :maxdepth: 1

   System Core <core/index>
   System Super <super/index>
