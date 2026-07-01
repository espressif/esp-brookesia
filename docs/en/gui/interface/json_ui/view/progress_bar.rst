.. _gui-interface-json-ui-view-progress-bar-sec-00:

Progressbar
======================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-view-progress_bar-sec-01:

Overview
--------------------

``progressBar`` represents a progress bar node.

.. _gui-interface-json_ui-view-progress_bar-sec-02:

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../styling/props/range`
- :doc:`../styling/style`

.. _gui-interface-json_ui-view-progress_bar-sec-03:

Exclusive Props
------------------------------

- ``rangeProps``, see details :doc:`../styling/props/range`

.. _gui-interface-json_ui-view-progress_bar-sec-04:

Common Events
--------------------------

- ``valueChanged``

.. _gui-interface-json_ui-view-progress_bar-sec-05:

Part Style
--------------------

- ``partStyles.indicator``: the filled progress; supports a two-color background gradient

.. _gui-interface-json_ui-view-progress_bar-sec-06:

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
