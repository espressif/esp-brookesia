.. _gui-interface-json-ui-styling-props-line-sec-00:

lineProps
====================

:link_to_translation:`en:[English]`

.. _gui-interface-json_ui-styling-props-line-sec-01:

概览
--------------------

``lineProps`` 适用于 ``line``。

.. _gui-interface-json_ui-styling-props-line-sec-02:

相关文档
--------------------

- :doc:`index`
- :doc:`../../view/line`

.. _gui-interface-json_ui-styling-props-line-sec-03:

字段表
--------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - Binding
     - UI 影响 / 限制
   * - ``points``
     - ``Point[]``
     - ``[]``
     - ``lineProps.points``
     - 线段点集；至少需要两个点才能形成可见线段

.. _gui-interface-json_ui-styling-props-line-sec-04:

Point 字段表
--------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - 说明
   * - ``x``
     - integer px
     - ``0``
     - 点的 X 坐标
   * - ``y``
     - integer px
     - ``0``
     - 点的 Y 坐标

.. _gui-interface-json_ui-styling-props-line-sec-05:

示例
--------------------

.. code-block:: json

   "lineProps": {
       "points": [
           { "x": 0, "y": 0 },
           { "x": 120, "y": 0 }
       ]
   }
