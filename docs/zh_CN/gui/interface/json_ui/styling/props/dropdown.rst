.. _gui-interface-json-ui-styling-props-dropdown-sec-00:

dropdownProps
==========================

:link_to_translation:`en:[English]`

概览
--------------------

``dropdownProps`` 适用于 ``dropdown``。

相关文档
--------------------

- :doc:`index`
- :doc:`../../view/dropdown`

字段表
--------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - Binding
     - UI 影响 / 限制
   * - ``options``
     - string[]
     - ``[]``
     - ``dropdownProps.options``
     - 下拉选项；binding 字符串需能解析为 string array
   * - ``selectedIndex``
     - integer
     - ``0``
     - ``dropdownProps.selectedIndex``
     - 当前选中项索引

示例
--------------------

.. code-block:: json

   "dropdownProps": {
       "options": ["Light", "Dark"],
       "selectedIndex": 0
   }
