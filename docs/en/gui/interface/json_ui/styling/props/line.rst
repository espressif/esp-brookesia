.. _gui-interface-json-ui-styling-props-line-sec-00:

Lineprops
====================

:link_to_translation:`zh_CN:[中文]`

Overview
--------------------

``lineProps`` applies to ``line``.

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../../view/line`

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

Example
--------------------

.. code-block:: json

   "lineProps": {
       "points": [
           { "x": 0, "y": 0 },
           { "x": 120, "y": 0 }
       ]
   }
