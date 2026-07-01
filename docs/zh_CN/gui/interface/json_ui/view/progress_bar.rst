.. _gui-interface-json-ui-view-progress-bar-sec-00:

progressBar
======================

:link_to_translation:`en:[English]`

概览
--------------------

``progressBar`` 表示进度条节点。

相关文档
--------------------

- :doc:`index`
- :doc:`../styling/props/range`
- :doc:`../styling/style`

专属 props
--------------------

- ``rangeProps``，详情见 :doc:`../styling/props/range`

常见事件
--------------------

- ``valueChanged``

Part 样式
--------------------

- ``partStyles.indicator``：已填充进度，可配置两色背景渐变

示例
--------------------

.. code-block:: json

   "partStyles": {
       "indicator": {
           "bgColor": "#22c55e",
           "bgGradientColor": "#3b82f6",
           "bgGradientDirection": "horizontal"
       }
   }
