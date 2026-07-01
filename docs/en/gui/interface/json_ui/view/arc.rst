.. _gui-interface-json-ui-view-arc-sec-00:

Arc
====================

:link_to_translation:`zh_CN:[中文]`

Overview
--------------------

``arc`` represents an arc-shaped numeric node.

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

- ``partStyles.indicator``: the indication arc for the current value; configurable ``arcColor``, ``arcGradientColor``, ``arcWidth``, ``arcRounded``
- ``partStyles.knob``: the drag handle

``arc`` has no native gradient arc; when ``arcGradientColor`` is present, the LVGL backend approximates a two-color gradient with piecewise drawing.

Example
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
