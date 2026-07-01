.. _gui-interface-json-ui-view-arc-sec-00:

Arc
====================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-view-arc-sec-01:

Overview
--------------------

``arc`` represents an arc-shaped numeric node.

.. _gui-interface-json_ui-view-arc-sec-02:

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../styling/props/range`
- :doc:`../styling/style`

.. _gui-interface-json_ui-view-arc-sec-03:

Exclusive Props
------------------------------

- ``rangeProps``, see details :doc:`../styling/props/range`

.. _gui-interface-json_ui-view-arc-sec-04:

Common Events
--------------------------

- ``valueChanged``
- ``pressed``
- ``released``

.. _gui-interface-json_ui-view-arc-sec-05:

Part Style
--------------------

- ``partStyles.indicator``: the indication arc for the current value; configurable ``arcColor``, ``arcGradientColor``, ``arcWidth``, ``arcRounded``
- ``partStyles.knob``: the drag handle

``arc`` has no native gradient arc; when ``arcGradientColor`` is present, the LVGL backend approximates a two-color gradient with piecewise drawing.

.. _gui-interface-json_ui-view-arc-sec-06:

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
