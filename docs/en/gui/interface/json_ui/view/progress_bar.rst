.. _gui-interface-json-ui-view-progress-bar-sec-00:

Progressbar
======================

:link_to_translation:`zh_CN:[中文]`

Overview
--------------------

``progressBar`` represents a progress bar node.

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

Part Style
--------------------

- ``partStyles.indicator``: the filled progress; supports a two-color background gradient

Example
--------------------

.. code-block:: json

   "partStyles": {
       "indicator": {
           "bgColor": "#22c55e",
           "bgGradientColor": "#3b82f6",
           "bgGradientDirection": "horizontal"
       }
   }
