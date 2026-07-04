.. _gui-interface-json-ui-styling-props-table-sec-00:

Tableprops
====================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-styling-props-table-sec-01:

Overview
--------------------

``tableProps`` applies to ``table``.

.. _gui-interface-json_ui-styling-props-table-sec-02:

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../../view/table`

.. _gui-interface-json_ui-styling-props-table-sec-03:

Field Table
----------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - Binding
     - UI Effect / Limits
   * - ``rows``
     - integer
     - ``0``
     - ``tableProps.rows``
     - number of table rows
   * - ``columns``
     - integer
     - ``0``
     - ``tableProps.columns``
     - number of table columns
   * - ``cells``
     - ``TableCell[]``
     - ``[]``
     - ``tableProps.cells``
     - cell text

.. _gui-interface-json_ui-styling-props-table-sec-04:

Tablecell Field Table
------------------------------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - Description
   * - ``row``
     - integer
     - ``0``
     - cell row index
   * - ``column``
     - integer
     - ``0``
     - cell column index
   * - ``text``
     - string
     - ``""``
     - cell text

.. _gui-interface-json_ui-styling-props-table-sec-05:

Example
--------------------

.. code-block:: json

   "tableProps": {
       "rows": 2,
       "columns": 2,
       "cells": [
           {
               "row": 0,
               "column": 1,
               "text": "Value"
           }
       ]
   }
