.. _gui-lvgl-index-sec-00:

LVGL 后端
==============================================

:link_to_translation:`en:[English]`

``brookesia_gui_lvgl`` 是 ``brookesia_gui_interface`` 的 LVGL 后端实现，负责把解析后的 JSON UI 文档模型映射为 LVGL 对象，并提供图片的构建期打包能力。

适用范围
--------------------

- LVGL 后端的工程接入
- PNG 到 LVGL image ``.bin`` 的构建期打包
- ``.bin`` 图片在后端中的预加载与缓存行为

JSON UI 协议本身见 :doc:`../interface/json_ui/index`。

本组文档
--------------------

.. list-table::
   :header-rows: 1

   * - 文档
     - 内容
     - 适用场景
   * - :doc:`backend`
     - JSON UI 解析模型到 LVGL API 的映射、``.bin`` 预加载、后端 pump
     - 排查 LVGL 后端行为
   * - :doc:`image_pack`
     - ``brookesia_gui_lvgl_pack_images()``、LVGLImage.py 解析、Python 依赖
     - 把 PNG 资源打包进 package / SPIFFS

.. toctree::
   :maxdepth: 1
   :titlesonly:

   backend
   image_pack

.. note::

   维护者同步检查：修改 ``cmake/image_pack.cmake`` 时同步 :doc:`image_pack`；修改 ``.bin`` 预加载、缓存策略或 JSON UI 字段映射时同步 :doc:`backend`。
