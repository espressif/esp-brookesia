.. _gui-interface-json-ui-styling-props-line-sec-00:

Lineprops
====================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-styling-props-line-sec-01:

Overview
--------------------

``lineProps`` applies to ``line``.

.. _gui-interface-json_ui-styling-props-line-sec-02:

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../../view/line`

.. _gui-interface-json_ui-styling-props-line-sec-03:

Field Table
----------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - Binding
     - UI Effect / Limits
   * - ``points``
     - ``Point[]``
     - ``[]``
     - ``lineProps.points``
     - the set of line points; at least two points are needed to form a visible line

.. _gui-interface-json_ui-styling-props-line-sec-04:

Point Field Table
----------------------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - Description
   * - ``x``
     - integer px
     - ``0``
     - the point's X coordinate
   * - ``y``
     - integer px
     - ``0``
     - the point's Y coordinate

.. _gui-interface-json_ui-styling-props-line-sec-05:

Example
--------------------

.. code-block:: json

   "lineProps": {
       "points": [
           { "x": 0, "y": 0 },
           { "x": 120, "y": 0 }
       ]
   }
