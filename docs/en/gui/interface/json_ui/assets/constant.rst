.. _gui-interface-json-ui-assets-constant-sec-00:

Constant
====================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-assets-constant-sec-01:

Overview
--------------------

The ``constant`` asset defines the constant tree available to the current document. Public JSON should use ``camelCase``.

.. _gui-interface-json_ui-assets-constant-sec-02:

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../index`
- :doc:`../runtime`

.. _gui-interface-json_ui-assets-constant-sec-03:

Field Table
----------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - Required
     - Description
   * - ``type``
     - string
     - none
     - Yes
     - Fixed to ``"constant"``
   * - ``data``
     - object
     - ``{}``
     - Yes
     - Constant tree; may nest JSON values such as object, string, number, bool, and array

.. _gui-interface-json_ui-assets-constant-sec-04:

Reference Rules
----------------------

- Use ``${constant.<path>}`` to reference a constant.
- ``<path>`` uses a dot path, e.g. ``${constant.colors.pageBg}``.
- Constant references are resolved in the parser stage in the order of the currently matched assets.
- Use ``${env.<field>}`` to reference the current document parsing environment; env is not part of the constant tree and is not
  queryable via ``Runtime::get_constant_value()``.
- For simple numeric math, use ``${expr(...)}``. Expressions are still resolved in the parser stage, not as a runtime binding.
- After the Runtime loads a document it keeps the final merged constant tree, which can be read via
  ``Runtime::get_constant_value(document_id, "<path>")``, e.g.
  ``get_constant_value(id, "colors.pageBg")``.
- The query path contains neither ``${}`` nor the ``constant.`` prefix; a missing or empty path returns an error.
- The query returns a ``boost::json::value``, so string, number, bool, object, array, and null keep their original JSON type.
- If root variants match and override a default constant, the runtime query returns the final overridden value.

.. _gui-interface-json_ui-assets-constant-sec-05:

Expression
--------------------

``${expr(...)}`` performs static arithmetic over constants. Inside an expression it supports:

- ``${constant.<path>}``; the reference must resolve to a number or an ``"Ndp"`` string.
- ``${env.<field>}``; the reference must resolve to a number or an ``"Ndp"`` string.
- Numeric literals, e.g. ``0``, ``12``, ``1.5``.
- ``dp`` literals, e.g. ``44dp``.
- Operators ``+``, ``-``, ``*``, ``/`` and parentheses.

Unit rules:

.. list-table::
   :header-rows: 1

   * - Operation
     - Rule
   * - ``+`` / ``-``
     - Both sides must both be ``dp`` or both be unitless numbers
   * - ``*``
     - Allows ``dp * number``, ``number * dp``, or ``number * number``; ``dp * dp`` is not allowed
   * - ``/``
     - The right-hand side must be a unitless number; the divisor cannot be 0

When the result is ``dp`` it is written back as an ``"Ndp"`` string; a unitless integer is written back as a JSON integer; a unitless decimal as a JSON number.
Non-numeric constants, incompatible units, division by zero, and mismatched parentheses make document parsing fail.

.. _gui-interface-json_ui-assets-constant-sec-06:

Environment Reference
------------------------------------------

``${env.*}`` is an independent namespace for reading the current parsing environment; copying these values into constants is discouraged. Currently supported:

.. list-table::
   :header-rows: 1

   * - Field
     - Type
     - Description
   * - ``widthPx``
     - integer
     - Current default display width, in px
   * - ``heightPx``
     - integer
     - Current default display height, in px
   * - ``widthDp``
     - string
     - Current default display width, in dp, e.g. ``"1024dp"``
   * - ``heightDp``
     - string
     - Current default display height, in dp, e.g. ``"600dp"``
   * - ``density``
     - number
     - Current density
   * - ``fontScale``
     - number
     - Current font scale
   * - ``language``
     - string
     - Current language id
   * - ``theme``
     - string
     - Current theme id

.. code-block:: json

   {
       "placement": {
           "x": "${constant.layout.leftRailWidth}",
           "width": "${expr(${env.widthDp} - ${constant.layout.leftRailWidth})}",
           "height": "${expr((${env.heightDp} - 44dp) / 2)}"
       }
   }

.. _gui-interface-json_ui-assets-constant-sec-07:

Example
--------------------

.. code-block:: json

   {
       "type": "constant",
       "data": {
           "colors": {
               "pageBg": "#f7f2e8"
           },
           "sizes": {
               "match": "match",
               "wrap": "wrap",
               "buttonHeight": "48dp",
               "panelWidth": "${expr(320dp - 24dp)}"
           }
       }
   }
