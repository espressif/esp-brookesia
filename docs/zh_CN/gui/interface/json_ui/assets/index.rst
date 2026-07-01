.. _gui-interface-json-ui-assets-index-sec-00:

资源
============================

:link_to_translation:`en:[English]`

概览
--------------------

本文档负责 asset 与 Runtime 资源的入口说明。公开 JSON 使用 ``camelCase``。

本文档不负责 ``view`` 内部字段细节，也不负责具体 ``layout`` / ``placement`` / ``style`` / ``props``。这些内容请查看 :doc:`../view/index`、:doc:`../styling/layout`、:doc:`../styling/placement`、:doc:`../styling/style`、:doc:`../styling/props/index`。

相关文档
--------------------

- :doc:`../index`
- :doc:`../document/root`
- :doc:`../view/index`
- :doc:`../runtime`

资源类型
--------------------

``root.json.assets`` 和 ``variants[].assets`` 可以引用路径字符串，也可以内嵌 asset object。路径相对当前 ``root.json`` 所在目录解析，内嵌 object 中的相对路径也相对当前 ``root.json`` 所在目录解析。

Document 内正式 asset 负责组成当前 document：

.. list-table::
   :header-rows: 1

   * - 类型
     - 文档
     - 说明
   * - ``constant``
     - :doc:`constant`
     - 常量树，供 ``${constant.<path>}`` 引用
   * - ``imageSet``
     - :doc:`image`
     - 当前 document 的图片资源，供 ``imageProps.src`` 引用
   * - ``viewScreen``
     - :doc:`view_screen`
     - 可挂载页面根节点
   * - ``viewTemplate``
     - :doc:`view_template`
     - 可复用模板根节点
   * - ``interactionTemplate``
     - :doc:`interaction_template`
     - 可复用交互事件、动画和状态样式
   * - ``styleSet``
     - :doc:`theme`
     - document 内命名样式集合，供 ``styleRefs`` 引用
   * - ``screenFlow``
     - :doc:`../interaction/screen_flow`
     - 同一 document 内一组 screen 的切换状态机

Runtime 全局资源由 Runtime API 注册或加载：

.. list-table::
   :header-rows: 1

   * - 类型
     - 文档
     - 说明
   * - ``fontSet``
     - :doc:`font`
     - 全局字体资源集合，供 ``style.font`` 引用
   * - ``imageSet``
     - :doc:`image`
     - 全局图片资源集合，供 ``imageProps.src`` 引用
   * - ``theme``
     - :doc:`theme`
     - 全局主题覆盖层，供 ``set_theme(...)`` 选择

资源边界
--------------------

- ``constant``、``imageSet``、``viewScreen``、``viewTemplate``、``interactionTemplate``、``styleSet``、``screenFlow`` 是 document asset 的正式类型。
- 普通 view 节点用 ``type`` 表示控件类型。
- 图片、字体资源统一使用 ``imageSet`` / ``fontSet``，即使只有一个资源。
- ``fontSet`` 和 ``theme`` 不是 document ``assets`` 的标准类型，应通过 Runtime 全局 API 注册或加载。

子文档
--------------------

- :doc:`constant`
- :doc:`view_screen`
- :doc:`view_template`
- :doc:`interaction_template`
- :doc:`font`
- :doc:`image`
- :doc:`theme`

.. toctree::
   :maxdepth: 1

   constant
   font
   image
   theme
   view_screen
   view_template
   interaction_template
