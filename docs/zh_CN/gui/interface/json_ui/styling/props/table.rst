.. _gui-interface-json-ui-styling-props-table-sec-00:

tableProps
====================

:link_to_translation:`en:[English]`

概览
--------------------

``tableProps`` 适用于 ``table``。

相关文档
--------------------

- :doc:`index`
- :doc:`../../view/table`

字段表
--------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - Binding
     - UI 影响 / 限制
   * - ``rows``
     - integer
     - ``0``
     - ``tableProps.rows``
     - 表格行数
   * - ``columns``
     - integer
     - ``0``
     - ``tableProps.columns``
     - 表格列数
   * - ``cells``
     - ``TableCell[]``
     - ``[]``
     - ``tableProps.cells``
     - 单元格文本

TableCell 字段表
--------------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - 说明
   * - ``row``
     - integer
     - ``0``
     - 单元格行索引
   * - ``column``
     - integer
     - ``0``
     - 单元格列索引
   * - ``text``
     - string
     - ``""``
     - 单元格文本

示例
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
