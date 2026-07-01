.. _gui-interface-json-ui-styling-props-dropdown-sec-00:

Dropdownprops
==========================

:link_to_translation:`zh_CN:[中文]`

Overview
--------------------

``dropdownProps`` applies to ``dropdown``.

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../../view/dropdown`

Field Table
----------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - Binding
     - UI Effect / Limits
   * - ``options``
     - string[]
     - ``[]``
     - ``dropdownProps.options``
     - dropdown options; the binding string must resolve to a string array
   * - ``selectedIndex``
     - integer
     - ``0``
     - ``dropdownProps.selectedIndex``
     - index of the currently selected item

Example
--------------------

.. code-block:: json

   "dropdownProps": {
       "options": ["Light", "Dark"],
       "selectedIndex": 0
   }
