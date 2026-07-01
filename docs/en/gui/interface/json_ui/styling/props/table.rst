.. _gui-interface-json-ui-styling-props-table-sec-00:

Tableprops
====================

:link_to_translation:`zh_CN:[中文]`

Overview
--------------------

``tableProps`` applies to ``table``.

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../../view/table`

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
