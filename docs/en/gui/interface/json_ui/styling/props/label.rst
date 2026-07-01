.. _gui-interface-json-ui-styling-props-label-sec-00:

Labelprops
====================

:link_to_translation:`zh_CN:[中文]`

Overview
--------------------

``labelProps`` applies to ``label`` and ``checkbox``. ``checkbox`` text is also expressed via ``labelProps.text``; no separate ``checkboxProps.text`` is introduced.

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../../view/label`

Field Table
----------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - Binding
     - UI Effect / Limits
   * - ``text``
     - string
     - ``""``
     - ``labelProps.text``
     - display text; supports ``${constant.*}`` constant references

Example
--------------------

.. code-block:: json

   "labelProps": {
       "text": "${constant.text.aboutTitle}"
   }
