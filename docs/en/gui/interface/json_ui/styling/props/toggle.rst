.. _gui-interface-json-ui-styling-props-toggle-sec-00:

Toggleprops
======================

:link_to_translation:`zh_CN:[中文]`

Overview
--------------------

``toggleProps`` applies to ``switch`` and ``checkbox``.

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../../view/switch`
- :doc:`../../view/checkbox`

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

Example
--------------------

.. code-block:: json

   "toggleProps": {
       "checked": true
   }
