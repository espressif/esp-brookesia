.. _gui-interface-json-ui-styling-props-toggle-sec-00:

Toggleprops
======================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-styling-props-toggle-sec-01:

Overview
--------------------

``toggleProps`` applies to ``switch`` and ``checkbox``.

.. _gui-interface-json_ui-styling-props-toggle-sec-02:

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../../view/switch`
- :doc:`../../view/checkbox`

.. _gui-interface-json_ui-styling-props-toggle-sec-03:

Field Table
----------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - Binding
     - UI Effect / Limits
   * - ``checked``
     - bool
     - ``false``
     - ``toggleProps.checked``
     - current switch or checked state

.. _gui-interface-json_ui-styling-props-toggle-sec-04:

Example
--------------------

.. code-block:: json

   "toggleProps": {
       "checked": true
   }
