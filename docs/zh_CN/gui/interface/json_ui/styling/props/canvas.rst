.. _gui-interface-json-ui-styling-props-canvas-sec-00:

canvasProps
======================

:link_to_translation:`en:[English]`

概览
--------------------

``canvasProps`` 适用于 ``canvas``。

相关文档
--------------------

- :doc:`index`
- :doc:`../../view/canvas`

字段表
--------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - Binding
     - UI 影响 / 限制
   * - ``commands``
     - ``CanvasCommand[]``
     - ``[]``
     - ``canvasProps.commands``
     - 声明式绘制命令列表

CanvasCommand 字段表
----------------------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - 说明
   * - ``type``
     - string
     - ``""``
     - 命令类型
   * - ``x``
     - integer px
     - ``0``
     - 起点或矩形 X 坐标
   * - ``y``
     - integer px
     - ``0``
     - 起点或矩形 Y 坐标
   * - ``width``
     - integer px
     - ``0``
     - 宽度
   * - ``height``
     - integer px
     - ``0``
     - 高度
   * - ``color``
     - string, ``"#RRGGBB"``
     - ``""``
     - 命令颜色

CanvasCommand type
------------------------------------

当前公开命令类型：

.. list-table::
   :header-rows: 1

   * - 值
     - 含义
     - 使用字段
   * - ``fill``
     - 用 ``color`` 填充整个 canvas 背景
     - ``color``
   * - ``pixel``
     - 在 ``(x, y)`` 设置一个像素
     - ``x``、``y``、``color``

``width`` / ``height`` 当前会被 parser 接收，但命令执行暂未使用。未列出的 ``type`` 会被忽略。

示例
--------------------

.. code-block:: json

   "canvasProps": {
       "commands": [
           {
               "type": "fill",
               "color": "#3b82f6"
           },
           {
               "type": "pixel",
               "x": 12,
               "y": 8,
               "width": 0,
               "height": 0,
               "color": "#ffffff"
           }
       ]
   }
