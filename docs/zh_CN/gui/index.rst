.. _gui-index-sec-00:

GUI 组件
====================

:link_to_translation:`en:[English]`

GUI 组件定义 ESP-Brookesia 应用使用的后端无关文档模型，以及将解析后模型渲染到 LVGL 的后端实现。

.. only:: html

   .. raw:: html
      :file: ../../_static/mermaid/zh_CN/gui/index/diagram.html

.. only:: latex

   .. image:: ../../_static/mermaid/zh_CN/gui/index/diagram.png
      :width: 100%

.. rubric:: 组件职责

- ``brookesia_gui_interface`` 定义 JSON UI 文档、运行时资源、绑定、action 和后端契约。
- ``brookesia_gui_lvgl`` 将解析后的 GUI 文档映射为 LVGL 对象，并提供图片打包辅助能力。
- System 组件通常组合使用这两层，使应用能够加载 UI 资源而不直接依赖 LVGL 细节。

.. toctree::
   :maxdepth: 1
   :titlesonly:

   GUI 接口 <interface/json_ui/index>
   LVGL 后端 <lvgl/index>
