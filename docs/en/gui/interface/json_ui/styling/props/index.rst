.. _gui-interface-json-ui-styling-props-index-sec-00:

Props
==========================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-styling-props-index-sec-01:

Overview
--------------------

This page covers the props fields of each control type / semantic group, and the props leaf fields that ``bindings`` can target. Public JSON uses ``camelCase``; ``bindings`` keys also use public ``camelCase`` dotted paths.

This page does not cover ``layout`` / ``placement`` / ``style`` / ``events`` / ``animations``. See their respective pages.

.. _gui-interface-json_ui-styling-props-index-sec-02:

Related Documents
----------------------------------

- :doc:`../index`
- :doc:`../../index`
- :doc:`../../view/index`
- :doc:`../../interaction/bindings`
- :doc:`../placement`

.. _gui-interface-json_ui-styling-props-index-sec-03:

General Rules
--------------------------

Public JSON uses explicit props fields:

.. list-table::
   :header-rows: 1

   * - Props field
     - Applicable control types
     - Subdocument
   * - ``commonProps``
     - All control types
     - :doc:`common`
   * - ``labelProps``
     - ``label`` / ``checkbox``
     - :doc:`label`
   * - ``imageProps``
     - ``image``
     - :doc:`image`
   * - ``textInputProps``
     - ``textInput``
     - :doc:`text_input`
   * - ``rangeProps``
     - ``slider`` / ``progressBar`` / ``arc``
     - :doc:`range`
   * - ``toggleProps``
     - ``switch`` / ``checkbox``
     - :doc:`toggle`
   * - ``dropdownProps``
     - ``dropdown``
     - :doc:`dropdown`
   * - ``tableProps``
     - ``table``
     - :doc:`table`
   * - ``lineProps``
     - ``line``
     - :doc:`line`
   * - ``keyboardProps``
     - ``keyboard``
     - :doc:`keyboard`
   * - ``canvasProps``
     - ``canvas``
     - :doc:`canvas`
   * - ``frameViewProps``
     - ``frameView``
     - :doc:`frame_view`

.. _gui-interface-json_ui-styling-props-index-sec-04:

Subdocument
----------------------

- :doc:`common`
- :doc:`label`
- :doc:`image`
- :doc:`text_input`
- :doc:`range`
- :doc:`toggle`
- :doc:`dropdown`
- :doc:`table`
- :doc:`line`
- :doc:`keyboard`
- :doc:`canvas`
- :doc:`frame_view`

.. _gui-interface-json_ui-styling-props-index-sec-05:

Binding Path
------------------------

To bind props, ``bindings`` must use a public ``camelCase`` dotted path, for example:

.. code-block:: json

   "bindings": {
       "labelProps.text": "status",
       "textInputProps.placeholder": "placeholder",
       "rangeProps.value": "value"
   }

Each subdocument's field table lists whether a field supports binding.

.. _gui-interface-json_ui-styling-props-index-sec-06:

Control Types That Do Not Use Proprietary Props
----------------------------------------------------------------------------------------------

The following control types currently have no exclusive props field and usually use only ``commonProps`` and fields from other modules:

- ``screen``
- ``container``
- ``button``
- ``spinner``

Where:

- ``button`` still recommends adding an explicit child ``label`` when text is needed
- ``spinner`` has no exclusive JSON props yet

.. toctree::
   :maxdepth: 1

   common
   image
   label
   line
   range
   toggle
   dropdown
   text_input
   keyboard
   canvas
   frame_view
   table
