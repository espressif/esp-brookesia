.. _gui-interface-json-ui-view-slider-sec-00:

Slider
====================

:link_to_translation:`zh_CN:[中文]`

Overview
--------------------

``slider`` represents a draggable numeric node.

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../styling/props/range`
- :doc:`../styling/style`

Exclusive Props
------------------------------

- ``rangeProps``, see details :doc:`../styling/props/range`

Common Events
--------------------------

- ``valueChanged``
- ``pressed``
- ``released``

Part Style
--------------------

- ``partStyles.indicator``: the selected track; configurable ``bgColor``, ``bgGradientColor``, ``bgGradientDirection``
- ``partStyles.knob``: the drag handle; configurable normal style and ``stateStyles.pressed``

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
