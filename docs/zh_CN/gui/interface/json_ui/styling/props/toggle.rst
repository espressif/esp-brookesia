.. _gui-interface-json-ui-styling-props-toggle-sec-00:

toggleProps
======================

:link_to_translation:`en:[English]`

概览
--------------------

``toggleProps`` 适用于 ``switch`` 和 ``checkbox``。

相关文档
--------------------

- :doc:`index`
- :doc:`../../view/switch`
- :doc:`../../view/checkbox`

字段表
--------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - Binding
     - UI 影响 / 限制
   * - ``checked``
     - bool
     - ``false``
     - ``toggleProps.checked``
     - 当前开关或勾选状态

示例
--------------------

.. code-block:: json

   "toggleProps": {
       "checked": true
   }
