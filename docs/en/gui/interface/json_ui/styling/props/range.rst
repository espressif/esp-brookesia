.. _gui-interface-json-ui-styling-props-range-sec-00:

Rangeprops
====================

:link_to_translation:`zh_CN:[中文]`

Overview
--------------------

``rangeProps`` applies to ``slider``, ``progressBar``, and ``arc``.

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../../view/slider`
- :doc:`../../view/progress_bar`
- :doc:`../../view/arc`

Field Table
----------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - Binding
     - UI Effect / Limits
   * - ``value``
     - integer
     - ``0``
     - ``rangeProps.value``
     - current value
   * - ``min``
     - integer
     - ``0``
     - ``rangeProps.min``
     - minimum value
   * - ``max``
     - integer
     - ``100``
     - ``rangeProps.max``
     - maximum value
   * - ``step``
     - integer
     - ``1``
     - ``rangeProps.step``
     - step value; currently used mainly for protocol expression and event logic

Example
--------------------

.. code-block:: json

   "rangeProps": {
       "value": 45,
       "min": 0,
       "max": 100,
       "step": 5
   }
