.. _gui-interface-json-ui-styling-props-label-sec-00:

labelProps
====================

:link_to_translation:`en:[English]`

.. _gui-interface-json_ui-styling-props-label-sec-01:

概览
--------------------

``labelProps`` 适用于 ``label`` 和 ``checkbox``。``checkbox`` 的文案也通过 ``labelProps.text`` 表达，不额外引入 ``checkboxProps.text``。

.. _gui-interface-json_ui-styling-props-label-sec-02:

相关文档
--------------------

- :doc:`index`
- :doc:`../../view/label`

.. _gui-interface-json_ui-styling-props-label-sec-03:

字段表
--------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - Binding
     - UI 影响 / 限制
   * - ``text``
     - string
     - ``""``
     - ``labelProps.text``
     - 显示文本；可使用 ``${constant.*}`` 常量引用

.. _gui-interface-json_ui-styling-props-label-sec-04:

示例
--------------------

.. code-block:: json

   "labelProps": {
       "text": "${constant.text.aboutTitle}"
   }
