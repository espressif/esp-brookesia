.. _gui-interface-json-ui-styling-props-frame-view-sec-00:

frameViewProps
============================

:link_to_translation:`en:[English]`

概览
--------------------

``frameViewProps`` 适用于 ``frameView``，用于配置离屏帧缓冲的输出注册方式与像素格式。

相关文档
--------------------

- :doc:`index`
- :doc:`../../view/frame_view`

字段表
--------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - Binding
     - UI 影响 / 限制
   * - ``autoRegisterOutput``
     - bool
     - ``true``
     - 否
     - 是否在创建时自动把帧缓冲注册为命名输出
   * - ``outputName``
     - string
     - ``""``
     - 否
     - 注册输出时使用的名称；为空时由 backend 决定默认名称
   * - ``colorFormat``
     - string enum，``RGB565`` / ``RGB888``
     - ``"RGB565"``
     - 否
     - 帧缓冲像素格式

示例
--------------------

.. code-block:: json

   "frameViewProps": {
       "autoRegisterOutput": true,
       "outputName": "camera_preview",
       "colorFormat": "RGB888"
   }
