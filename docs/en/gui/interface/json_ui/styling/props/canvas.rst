.. _gui-interface-json-ui-styling-props-canvas-sec-00:

Canvasprops
======================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-styling-props-canvas-sec-01:

Overview
--------------------

``canvasProps`` applies to ``canvas``.

.. _gui-interface-json_ui-styling-props-canvas-sec-02:

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../../view/canvas`

.. _gui-interface-json_ui-styling-props-canvas-sec-03:

Field Table
----------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - Binding
     - UI Effect / Limits
   * - ``commands``
     - ``CanvasCommand[]``
     - ``[]``
     - ``canvasProps.commands``
     - declarative drawing command list

.. _gui-interface-json_ui-styling-props-canvas-sec-04:

Canvascommand Field Table
--------------------------------------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - Description
   * - ``type``
     - string
     - ``""``
     - command type
   * - ``x``
     - integer px
     - ``0``
     - start point or rectangle X coordinate
   * - ``y``
     - integer px
     - ``0``
     - start point or rectangle Y coordinate
   * - ``width``
     - integer px
     - ``0``
     - width
   * - ``height``
     - integer px
     - ``0``
     - height
   * - ``color``
     - string, ``"#RRGGBB"``
     - ``""``
     - command color

.. _gui-interface-json_ui-styling-props-canvas-sec-05:

Canvascommand Type
------------------------------------

Currently exposed command types:

.. list-table::
   :header-rows: 1

   * - Value
     - Meaning
     - Fields used
   * - ``fill``
     - fill the entire canvas background with ``color``
     - ``color``
   * - ``pixel``
     - set a pixel at ``(x, y)``
     - ``x``, ``y``, ``color``

``width`` / ``height`` are currently accepted by the parser but not yet used during command execution. A ``type`` that is not listed is ignored.

.. _gui-interface-json_ui-styling-props-canvas-sec-06:

Example
--------------------

.. code-block:: json

   "canvasProps": {
       "commands": [
           {
               "type": "fill",
               "color": "#3b82f6"
           },
           {
               "type": "pixel",
               "x": 12,
               "y": 8,
               "width": 0,
               "height": 0,
               "color": "#ffffff"
           }
       ]
   }
