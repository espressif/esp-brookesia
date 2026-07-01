.. _gui-interface-json-ui-view-slider-sec-00:

Slider
====================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-view-slider-sec-01:

Overview
--------------------

``slider`` represents a draggable numeric node.

.. _gui-interface-json_ui-view-slider-sec-02:

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../styling/props/range`
- :doc:`../styling/style`

.. _gui-interface-json_ui-view-slider-sec-03:

Exclusive Props
------------------------------

- ``rangeProps``, see details :doc:`../styling/props/range`

.. _gui-interface-json_ui-view-slider-sec-04:

Common Events
--------------------------

- ``valueChanged``
- ``pressed``
- ``released``

.. _gui-interface-json_ui-view-slider-sec-05:

Part Style
--------------------

- ``partStyles.indicator``: the selected track; configurable ``bgColor``, ``bgGradientColor``, ``bgGradientDirection``
- ``partStyles.knob``: the drag handle; configurable normal style and ``stateStyles.pressed``

.. _gui-interface-json_ui-view-slider-sec-06:

Example
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
