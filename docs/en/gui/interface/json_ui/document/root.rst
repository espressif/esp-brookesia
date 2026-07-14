.. _gui-interface-json-ui-document-root-sec-00:

Root
========================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-document-root-sec-01:

Overview
--------------------

This page covers the entry structure of the document loading layer:

- ``root.json`` top-level structure
- ``variants[]``
- ``when`` expression
- ``Environment``
- Unit and font-size conversion

This page does not cover asset field details, nor view/layout/style/props. See those separately in
:doc:`../assets/index`, :doc:`../view/index`, :doc:`../styling/layout`, :doc:`../styling/placement`.

.. _gui-interface-json_ui-document-root-sec-02:

Related Documents
----------------------------------

- :doc:`../index`
- :doc:`index`
- :doc:`../assets/index`
- :doc:`../styling/placement`

.. _gui-interface-json_ui-document-root-sec-03:

Root Document Top-Level Structure
--------------------------------------------------------

``root.json`` must be a JSON object.

Example:

.. code-block:: json

   {
       "version": "0.1.1",
       "assets": [
           "../../shared/constants/base.json",
           "flows/main.json",
           {
               "type": "viewScreen",
               "id": "home",
               "children": []
           }
       ],
       "variants": [
           {
               "when": "${expr(${env.language} == \"zh\")}",
               "assets": [
                   "../../shared/constants/i18n_zh.json",
                   {
                       "type": "constant",
                       "data": {
                           "hero": {
                               "title": "设置"
                           }
                       }
                   }
               ]
           },
           {
               "when": "${expr(${env.widthDp} < 600dp)}",
               "assets": [
                   "../../shared/constants/layout_phone.json",
                   "nodes/about.json",
                   "nodes/about_action_item.json"
               ]
           }
       ]
   }

.. _gui-interface-json_ui-document-root-sec-04:

Top-Level Field Table
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - Required
     - Description
   * - ``version``
     - string
     - ``"0.1.1"``
     - No
     - Protocol version; currently ``"0.1.1"`` is supported
   * - ``assets``
     - array<string | object>
     - ``[]``
     - No
     - Shared resource list, processed in original order
   * - ``variants``
     - object[]
     - ``[]``
     - No
     - Condition-matched resource overlay list

.. _gui-interface-json_ui-document-root-sec-05:

Top-Level Field Description
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. _gui-interface-json_ui-document-root-sec-06:

Version
~~~~~~~~~~~~~~~~~~~~

- type: ``string``
- Required: No
- Current value: ``"0.1.1"``
- ``0.1.1`` adds ``imageSet.images[].preload``; old resources that omit this field behave as ``false`` and keep on-demand loading

.. _gui-interface-json_ui-document-root-sec-07:

Assets
~~~~~~~~~~~~~~~~~~~~

- type: ``array<string | object>``
- Required: No
- Default: empty array

Notes:

- Represents the shared resource list of the document
- Each item can be:
  - a file path string
  - an embedded asset JSON object
- The path resolves relative to the current ``root.json`` directory
- Relative paths inside an embedded object also resolve against the current ``root.json`` directory
- May mix ``constant`` / ``styleSet`` / ``imageSet`` / ``viewScreen`` / ``viewTemplate`` / ``interactionTemplate`` / ``screenFlow`` assets
- When mixed, the array's original order applies strictly; ``constants`` / ``nodes`` / ``screenFlow`` are no longer root-level fields, put them in ``assets``

.. _gui-interface-json_ui-document-root-sec-08:

Variants
~~~~~~~~~~~~~~~~~~~~

- type: ``object[]``
- Required: No
- Default: empty array

Notes:

- ``variants`` may be omitted or empty
- When empty, only the top-level ``assets`` are loaded
- All matched variants are overlaid; matching does not stop at the first hit

.. _gui-interface-json_ui-document-root-sec-09:

Variants[]
--------------------

Each variant is a JSON object.

Example:

.. code-block:: json

   {
       "when": "${expr(${env.language} == \"zh\" && ${env.widthDp} < 600dp)}",
       "assets": [
           "../../shared/constants/i18n_zh.json",
           "../../shared/constants/layout_phone.json"
       ]
   }

Field table:

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - Required
     - Description
   * - ``when``
     - string
     - ``"${expr(true)}"``
     - No
     - Variant match condition; must be written as ``${expr(...)}``
   * - ``assets``
     - array<string | object>
     - ``[]``
     - No
     - Resources to overlay when the variant matches

.. _gui-interface-json_ui-document-root-sec-10:

Assets Entry Forms
----------------------------------------------

``root.json.assets`` and ``variants[].assets`` both support two entry forms:

- a path string
- an embedded asset object

Example:

.. code-block:: json

   {
       "version": "0.1.1",
       "assets": [
           "./constants/base.json",
           {
               "type": "viewScreen",
               "id": "home_screen",
               "children": []
           }
       ]
   }

Rules:

- ``string`` entry:
  - still denotes an external asset file path
- ``object`` entry:
  - must be a valid asset object
  - must contain ``type``
  - document standard types are ``constant`` / ``styleSet`` / ``imageSet`` / ``viewScreen`` / ``viewTemplate`` / ``interactionTemplate`` / ``screenFlow``
  - ``fontSet`` / ``theme`` are not document ``assets`` standard types; register or load them through the Runtime global API
- The two entry forms can be mixed
- The parser processes the array in original order and does not reorder by entry type
- For relative paths inside ``imageSet`` / ``viewScreen`` / ``viewTemplate`` / ``interactionTemplate`` embedded objects:
  - they resolve relative to the current ``root.json`` directory

.. _gui-interface-json_ui-document-root-sec-11:

When Expression
----------------------------

.. _gui-interface-json_ui-document-root-sec-12:

Supported Environment References
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ``${env.widthDp}``
- ``${env.heightDp}``
- ``${env.widthPx}``
- ``${env.heightPx}``
- ``${env.language}``
- ``${env.theme}``

.. _gui-interface-json_ui-document-root-sec-13:

Supported Operators
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- Comparison: ``>``, ``<``, ``==``, ``!=``, ``>=``, ``<=``
- Logic: ``&&``, ``||``, ``!``
- Grouping: ``(``, ``)``
- Arithmetic: ``+``, ``-``, ``*``, ``/``
- String literal: ``"zh"``
- Numeric unit: ``dp``

.. _gui-interface-json_ui-document-root-sec-14:

Example
^^^^^^^^^^^^^^^^^^^^

.. code-block:: json

   { "when": "${expr(${env.language} == \"zh\")}" }
   { "when": "${expr(${env.theme} == \"shell.dark\")}" }
   { "when": "${expr(${env.widthDp} < 600dp)}" }
   { "when": "${expr(${env.widthDp} >= 600dp && ${env.heightDp} >= 360dp)}" }
   { "when": "${expr((${env.widthDp} >= 600dp && ${env.heightDp} >= 400dp) || ${env.widthPx} >= 1280)}" }

.. _gui-interface-json_ui-document-root-sec-15:

Runtime Environment
------------------------------------------------------------

``when`` must be written as ``${expr(...)}``, and the expression result must be a boolean, e.g. ``"${expr(${env.widthDp} < 600dp)}"``.

``when`` evaluation relies on the ``Environment`` passed in by the caller.

See the current structure in
``gui/brookesia_gui_interface/include/brookesia/gui_interface/document.hpp``.

Main fields (defaults from ``document.hpp``):

- ``width_px``: default ``320``
- ``height_px``: default ``480``
- ``density``: default ``1.0``
- ``font_scale``: default ``1.0``
- ``language``: default ``"zh"``
- ``theme_id``: default ``"default"``
- ``colors``: ``map<string, string>`` color table for ``${color.*}`` resolution; note it **cannot** be read via ``${env.*}``

Conversion:

- ``${env.widthDp} = width_px / density``
- ``${env.heightDp} = height_px / density``

These fields can also be referenced via ``${env.*}`` in JSON resources. Currently supported: ``${env.widthPx}``, ``${env.heightPx}``,
``${env.widthDp}``, ``${env.heightDp}``, ``${env.density}``, ``${env.fontScale}``, ``${env.language}`` and
``${env.theme}``. ``${env.widthDp}`` / ``${env.heightDp}`` output an ``"Ndp"`` string, suitable for use within ``${expr(...)}``.

``Environment`` is only responsible for:

- ``when`` expression evaluation
- ``dp/sp`` size conversion
- Protocol inputs such as language and viewport
- Recording the initial theme id; ``${env.theme}`` in ``when`` uses this value
- The Runtime global ``set_theme(...)`` is still the official entry for switching themes

If a document's ``variants[].when`` uses ``${env.theme}`` or ``${env.language}``, the Runtime marks it as
environment-sensitive. ``set_theme(theme_id, true)`` / ``set_language(language, true)`` re-parse the root for file-backed documents,
so constants, images, and screen resources follow the variant update; non-file-backed documents can only reapply style and log a warning.

For theme resources and theme switching, see :doc:`../runtime`.

The view debug frame is not part of ``Environment`` but a Runtime-level debugging capability; see :doc:`../runtime`.

.. _gui-interface-json_ui-document-root-sec-16:

Unit and Font-Size Conversion
----------------------------------------------------------

All sizes in JSON UI are converted to backend pixel values during parsing.

Basic concepts:

- ``px``
  - backend physical pixels
  - written as a JSON number in the protocol, e.g. ``24``
  - the ``"24px"`` string suffix is not currently supported
- ``dp``
  - density-independent pixel
  - used for layout size, spacing, corner radius, borders, and other non-text sizes
  - formula: ``px = round(dp * Environment.density)``
- ``sp``
  - scale-independent pixel
  - used for font size
  - formula: ``px = round(sp * Environment.density * Environment.font_scale)``
- ``fontSize``
  - the public field name of ``style.fontSize``
  - semantically equivalent to the ``sp`` font size
  - the font size finally passed to the backend is the converted integer pixel value

Field rules:

- ``layout.gap`` takes a ``dp`` string or a bare number ``px``
- ``placement.x`` / ``placement.y`` take a ``dp`` string, percentage, or bare number ``px``
- ``placement.width`` / ``placement.height`` accept ``dp`` strings, percentages, bare numbers ``px``, ``match``, ``wrap``
- ``style.borderWidth`` / ``style.radius`` / ``style.padding`` take a ``dp`` string or a bare number ``px``
- ``style.fontSize`` takes an ``sp`` string or a bare number ``px``
- A bare number always means an already-converted ``px``; it is not multiplied by ``density`` or ``font_scale``

Example:

.. code-block:: json

   {
       "placement": {
           "width": "120dp",
           "height": 48
       },
       "style": {
           "padding": "12dp",
           "fontSize": "20sp"
       }
   }

When ``Environment.density = 2.0`` and ``Environment.font_scale = 1.2``:

- ``"120dp"`` -> ``round(120 * 2.0) = 240px``
- ``48`` -> ``48px``
- ``"12dp"`` -> ``round(12 * 2.0) = 24px``
- ``"20sp"`` -> ``round(20 * 2.0 * 1.2) = 48px``

Relationship between ``fontSize`` and native font resources:

- a native font's ``native_size`` is the pixel size of the generated font
- the backend selects a native font by the converted ``fontSize`` pixel value
- if there is no exact match, it picks the closest ``native_size`` and logs a warning
