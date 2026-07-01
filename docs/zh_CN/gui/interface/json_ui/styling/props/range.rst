.. _gui-interface-json-ui-styling-props-range-sec-00:

rangeProps
====================

:link_to_translation:`en:[English]`

概览
--------------------

``rangeProps`` 适用于 ``slider``、``progressBar`` 和 ``arc``。

相关文档
--------------------

- :doc:`index`
- :doc:`../../view/slider`
- :doc:`../../view/progress_bar`
- :doc:`../../view/arc`

字段表
--------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - Binding
     - UI 影响 / 限制
   * - ``value``
     - integer
     - ``0``
     - ``rangeProps.value``
     - 当前值
   * - ``min``
     - integer
     - ``0``
     - ``rangeProps.min``
     - 最小值
   * - ``max``
     - integer
     - ``100``
     - ``rangeProps.max``
     - 最大值
   * - ``step``
     - integer
     - ``1``
     - ``rangeProps.step``
     - 步进值；当前主要供协议表达和事件逻辑使用

示例
--------------------

.. code-block:: json

   "rangeProps": {
       "value": 45,
       "min": 0,
       "max": 100,
       "step": 5
   }
