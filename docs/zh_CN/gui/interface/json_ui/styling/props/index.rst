.. _gui-interface-json-ui-styling-props-index-sec-00:

属性
==========================

:link_to_translation:`en:[English]`

.. _gui-interface-json_ui-styling-props-index-sec-01:

概览
--------------------

本文档负责各控件类型 / 语义分组的 props 域，以及 ``bindings`` 可指向的 props 叶子字段。公开 JSON 使用 ``camelCase``，``bindings`` key 也使用公开 ``camelCase`` 点路径。

本文档不负责 ``layout`` / ``placement`` / ``style`` / ``events`` / ``animations``。这些内容请查看对应文档。

.. _gui-interface-json_ui-styling-props-index-sec-02:

相关文档
--------------------

- :doc:`../index`
- :doc:`../../index`
- :doc:`../../view/index`
- :doc:`../../interaction/bindings`
- :doc:`../placement`

.. _gui-interface-json_ui-styling-props-index-sec-03:

总体规则
--------------------

公开 JSON 使用显式的 props 域：

.. list-table::
   :header-rows: 1

   * - Props 域
     - 适用控件类型
     - 子文档
   * - ``commonProps``
     - 全部控件类型
     - :doc:`common`
   * - ``labelProps``
     - ``label`` / ``checkbox``
     - :doc:`label`
   * - ``imageProps``
     - ``image``
     - :doc:`image`
   * - ``textInputProps``
     - ``textInput``
     - :doc:`text_input`
   * - ``rangeProps``
     - ``slider`` / ``progressBar`` / ``arc``
     - :doc:`range`
   * - ``toggleProps``
     - ``switch`` / ``checkbox``
     - :doc:`toggle`
   * - ``dropdownProps``
     - ``dropdown``
     - :doc:`dropdown`
   * - ``tableProps``
     - ``table``
     - :doc:`table`
   * - ``lineProps``
     - ``line``
     - :doc:`line`
   * - ``keyboardProps``
     - ``keyboard``
     - :doc:`keyboard`
   * - ``canvasProps``
     - ``canvas``
     - :doc:`canvas`
   * - ``frameViewProps``
     - ``frameView``
     - :doc:`frame_view`

.. _gui-interface-json_ui-styling-props-index-sec-04:

子文档
--------------------

- :doc:`common`
- :doc:`label`
- :doc:`image`
- :doc:`text_input`
- :doc:`range`
- :doc:`toggle`
- :doc:`dropdown`
- :doc:`table`
- :doc:`line`
- :doc:`keyboard`
- :doc:`canvas`
- :doc:`frame_view`

.. _gui-interface-json_ui-styling-props-index-sec-05:

Binding 路径
--------------------

``bindings`` 若要绑定 props，必须使用公开 ``camelCase`` 点路径，例如：

.. code-block:: json

   "bindings": {
       "labelProps.text": "status",
       "textInputProps.placeholder": "placeholder",
       "rangeProps.value": "value"
   }

每个子文档的字段表会列出当前字段是否支持 binding。

.. _gui-interface-json_ui-styling-props-index-sec-06:

不使用专属 props 的控件类型
----------------------------------

以下控件类型当前没有专属 props 域，通常只使用 ``commonProps`` 和其他模块字段：

- ``screen``
- ``container``
- ``button``
- ``spinner``

其中：

- ``button`` 若需要文案，继续推荐显式添加子 ``label``
- ``spinner`` 暂无专属 JSON props

.. toctree::
   :maxdepth: 1

   common
   image
   label
   line
   range
   toggle
   dropdown
   text_input
   keyboard
   canvas
   frame_view
   table
