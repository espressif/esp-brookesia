.. _gui-interface-json-ui-styling-props-common-sec-00:

commonProps
======================

:link_to_translation:`en:[English]`

.. _gui-interface-json_ui-styling-props-common-sec-01:

概览
--------------------

``commonProps`` 只保留全体节点都通用的状态字段。

节点外框尺寸不属于 ``commonProps``。``width``、``height``、``aspectRatio`` 等 size/box 约束统一写在
:doc:`placement <../placement>` 中；``commonProps.zoom`` / ``pivotX`` / ``pivotY`` 只负责已经布局后的 transform。

.. _gui-interface-json_ui-styling-props-common-sec-02:

相关文档
--------------------

- :doc:`index`
- :doc:`../../view/index`
- :doc:`../placement`

.. _gui-interface-json_ui-styling-props-common-sec-03:

字段表
--------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - Binding
     - UI 影响 / 限制
   * - ``hidden``
     - bool
     - ``false``
     - ``commonProps.hidden``
     - 控制节点隐藏状态；show/hide 动画基于该字段变化触发
   * - ``disabled``
     - bool
     - ``false``
     - ``commonProps.disabled``
     - 控制禁用状态
   * - ``clickable``
     - bool
     - 按控件类型 baseline
     - ``commonProps.clickable``
     - 控制是否接收点击类事件
   * - ``scrollable``
     - bool
     - 按控件类型 baseline
     - ``commonProps.scrollable``
     - 控制是否允许滚动
   * - ``pressLock``
     - bool
     - ``true``
     - ``commonProps.pressLock``
     - 控制按住后滑出节点范围时，是否继续锁定该节点为 pressed target
   * - ``angle``
     - integer 度
     - ``0``
     - ``commonProps.angle``
     - 节点通用旋转角度
   * - ``zoom``
     - integer
     - ``256``
     - ``commonProps.zoom``
     - 节点通用缩放值；``256`` 表示 1x
   * - ``pivotX``
     - integer px 或 percent string
     - ``0``
     - ``commonProps.pivotX``
     - 通用旋转/缩放 pivot 的 X 坐标；如 ``"50%"`` 表示节点宽度的一半
   * - ``pivotY``
     - integer px 或 percent string
     - ``0``
     - ``commonProps.pivotY``
     - 通用旋转/缩放 pivot 的 Y 坐标；如 ``"50%"`` 表示节点高度的一半

.. _gui-interface-json_ui-styling-props-common-sec-04:

Baseline 默认值
------------------------

``clickable`` 的默认值来自内建 baseline：

- ``label`` 默认 ``false``
- 其它常规节点，如 ``screen``、``container``、``button``、``image`` 等，默认 ``true``

``scrollable`` 的默认值也来自内建 baseline：

- ``screen``、``container`` 默认 ``true``
- 其它常规节点，如 ``label``、``button``、``image`` 等，默认 ``false``

.. _gui-interface-json_ui-styling-props-common-sec-05:

示例
--------------------

.. code-block:: json

   "commonProps": {
       "hidden": true,
       "disabled": false,
       "clickable": true,
       "scrollable": false,
       "pressLock": true,
       "angle": 15,
       "zoom": 256,
       "pivotX": "50%",
       "pivotY": "50%"
   }

``scrollable: true`` 会要求 backend 启用滚动能力；``scrollable: false`` 会要求 backend 禁用滚动能力。

``pressLock: true`` 表示按住后即使指针移出节点范围，backend 也尽量保持该节点作为当前 pressed target。
如果控件需要在滑出范围时收到 ``pressLost`` 并取消后续有效点击，应设置 ``pressLock: false``。

``pivotX`` / ``pivotY`` 支持裸整数 px 或百分比字符串。百分比按节点当前尺寸换算：``pivotX`` 使用节点宽度，``pivotY`` 使用节点高度。
