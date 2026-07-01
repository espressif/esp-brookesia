.. _gui-interface-json-ui-view-arc-sec-00:

arc
====================

:link_to_translation:`en:[English]`

概览
--------------------

``arc`` 表示弧形数值节点。

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
- ``pressed``
- ``released``

Part 样式
--------------------

- ``partStyles.indicator``：当前值对应的指示弧。可配置 ``arcColor``、``arcGradientColor``、``arcWidth``、``arcRounded``
- ``partStyles.knob``：拖动手柄

``arc`` 没有原生渐变弧线；LVGL backend 在 ``arcGradientColor`` 存在时使用分段绘制近似两色渐变。

示例
--------------------

.. code-block:: json

   "partStyles": {
       "indicator": {
           "arcColor": "#22c55e",
           "arcGradientColor": "#3b82f6",
           "arcWidth": "10dp",
           "arcRounded": true,
           "arcGradientSegments": 48
       }
   }
