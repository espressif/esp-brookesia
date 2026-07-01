.. _gui-interface-json-ui-view-slider-sec-00:

slider
====================

:link_to_translation:`en:[English]`

.. _gui-interface-json_ui-view-slider-sec-01:

概览
--------------------

``slider`` 表示可拖动数值节点。

.. _gui-interface-json_ui-view-slider-sec-02:

相关文档
--------------------

- :doc:`index`
- :doc:`../styling/props/range`
- :doc:`../styling/style`

.. _gui-interface-json_ui-view-slider-sec-03:

专属 props
--------------------

- ``rangeProps``，详情见 :doc:`../styling/props/range`

.. _gui-interface-json_ui-view-slider-sec-04:

常见事件
--------------------

- ``valueChanged``
- ``pressed``
- ``released``

.. _gui-interface-json_ui-view-slider-sec-05:

Part 样式
--------------------

- ``partStyles.indicator``：已选中轨道，可配置 ``bgColor``、``bgGradientColor``、``bgGradientDirection``
- ``partStyles.knob``：拖动手柄，可配置普通 style 和 ``stateStyles.pressed``

.. _gui-interface-json_ui-view-slider-sec-06:

示例
--------------------

.. code-block:: json

   "partStyles": {
       "indicator": {
           "bgColor": "#2563eb",
           "bgGradientColor": "#38bdf8",
           "bgGradientDirection": "horizontal"
       },
       "knob": {
           "style": {"bgColor": "#ffffff", "radius": "12dp"},
           "stateStyles": {"pressed": {"bgColor": "#dbeafe"}}
       }
   }
