.. _gui-interface-json-ui-styling-props-text-input-sec-00:

textInputProps
============================

:link_to_translation:`en:[English]`

.. _gui-interface-json_ui-styling-props-text_input-sec-01:

概览
--------------------

``textInputProps`` 适用于 ``textInput``。

.. _gui-interface-json_ui-styling-props-text_input-sec-02:

相关文档
--------------------

- :doc:`index`
- :doc:`../../view/text_input`

.. _gui-interface-json_ui-styling-props-text_input-sec-03:

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
     - ``textInputProps.text``
     - 当前输入文本
   * - ``placeholder``
     - string
     - ``""``
     - ``textInputProps.placeholder``
     - 占位文本
   * - ``password``
     - bool
     - ``false``
     - ``textInputProps.password``
     - 是否启用密码显示模式
   * - ``multiline``
     - bool
     - ``false``
     - ``textInputProps.multiline``
     - 是否允许多行输入
   * - ``maxLength``
     - integer
     - ``0``
     - ``textInputProps.maxLength``
     - 最大输入长度；``0`` 表示不额外限制

.. _gui-interface-json_ui-styling-props-text_input-sec-04:

示例
--------------------

.. code-block:: json

   "textInputProps": {
       "text": "Hello",
       "placeholder": "Type here",
       "password": false,
       "multiline": false,
       "maxLength": 32
   }
