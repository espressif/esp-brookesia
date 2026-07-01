.. _gui-interface-json-ui-styling-props-dropdown-sec-00:

Dropdownprops
==========================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-styling-props-dropdown-sec-01:

Overview
--------------------

``dropdownProps`` applies to ``dropdown``.

.. _gui-interface-json_ui-styling-props-dropdown-sec-02:

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../../view/dropdown`

.. _gui-interface-json_ui-styling-props-dropdown-sec-03:

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

.. _gui-interface-json_ui-styling-props-dropdown-sec-04:

Example
--------------------

.. code-block:: json

   "dropdownProps": {
       "options": ["Light", "Dark"],
       "selectedIndex": 0
   }
