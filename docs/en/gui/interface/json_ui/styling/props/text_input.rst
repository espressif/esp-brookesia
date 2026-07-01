.. _gui-interface-json-ui-styling-props-text-input-sec-00:

Textinputprops
============================

:link_to_translation:`zh_CN:[中文]`

Overview
--------------------

``textInputProps`` applies to ``textInput``.

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../../view/text_input`

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
     - ``textInputProps.text``
     - current input text
   * - ``placeholder``
     - string
     - ``""``
     - ``textInputProps.placeholder``
     - placeholder text
   * - ``password``
     - bool
     - ``false``
     - ``textInputProps.password``
     - whether to enable password display mode
   * - ``multiline``
     - bool
     - ``false``
     - ``textInputProps.multiline``
     - whether to allow multi-line input
   * - ``maxLength``
     - integer
     - ``0``
     - ``textInputProps.maxLength``
     - maximum input length; ``0`` means no extra limit

Example
--------------------

.. code-block:: json

   "textInputProps": {
       "text": "Hello",
       "placeholder": "Type here",
       "password": false,
       "multiline": false,
       "maxLength": 32
   }
