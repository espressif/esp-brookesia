.. _gui-interface-json-ui-assets-theme-sec-00:

Theme
====================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-assets-theme-sec-01:

Overview
--------------------

The ``theme`` descriptor describes the Runtime global theme entry. The theme entry combines token fragments and produces the final
``colors`` and global ``styles``; application- or Shell-specific named styles should live in the document's own ``styleSet`` resources.

.. _gui-interface-json_ui-assets-theme-sec-02:

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../index`
- :doc:`../styling/style`
- :doc:`../runtime`

.. _gui-interface-json_ui-assets-theme-sec-03:

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
     - Fixed to ``"theme"``
   * - ``id``
     - string
     - none
     - Yes
     - Theme id
   * - ``assets``
     - array
     - ``[]``
     - No
     - Theme-private ``constant`` token fragment paths or inline constants
   * - ``variants``
     - array
     - ``[]``
     - No
     - Theme-private ``constant`` token fragments appended by environment condition
   * - ``styles``
     - object
     - ``{}``
     - No
     - Final global style overrides inside the entry file

A ``theme`` file does not support a ``colors`` field. Color aliases must live in a ``constant`` asset's ``data.colors``.

.. _gui-interface-json_ui-assets-theme-sec-04:

Theme Token Constant
----------------------------------------

``assets`` / ``variants.assets`` accept only ``type: "constant"`` resources. The theme parser first merges the
base assets in order, then the matched variant assets, and then reads from the final constants:

- ``colors``: the semantic color table referenced by ``${color.xxx}``
- ``styles``: global theme styles
- Other tokens: usable by theme styles via ``${constant.xxx}``, such as fonts, corner radius, spacing

A ``colors`` value must be a real color string or an empty string; it cannot reference other resources or color aliases.

Example:

.. code-block:: json

   {
       "type": "theme",
       "id": "shell.light",
       "assets": [
           "color/light.json",
           "font/default.json",
           "size/default.json",
           "style/default.json"
       ],
       "variants": [
           {
               "when": "${expr(${env.widthDp} == 800dp && ${env.heightDp} == 480dp)}",
               "assets": [
                   "font/800x480.json",
                   "size/800x480.json"
               ]
           }
       ],
       "styles": {
           "app.pageTitle": {
               "fontSize": "48sp"
           }
       }
   }

``color/light.json``:

.. code-block:: json

   {
       "type": "constant",
       "data": {
           "colors": {
               "text": {
                   "default": "#38393a",
                   "muted": "#7a7a7a"
               },
               "surface": {
                   "base": "#fafbfc"
               },
               "primary": {
                   "fill": "#E8362D",
                   "on": "#fafbfc"
               }
           }
       }
   }

``style/default.json``:

.. code-block:: json

   {
       "type": "constant",
       "data": {
           "styles": {
               "label": {
                   "textColor": "${color.text.default}",
                   "fontSize": "${constant.font.body}"
               },
               "app.card": {
                   "bgColor": "${color.surface.base}",
                   "borderColor": "${color.border.default}",
                   "borderWidth": "1dp",
                   "radius": "${constant.radius.card}"
               }
           }
       }
   }

.. _gui-interface-json_ui-assets-theme-sec-05:

Style Key Convention
----------------------------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Description
   * - ``all``
     - Common style layer for all nodes
   * - ``screen`` / ``container`` / ``label`` and other control types
     - Override appended by node control type
   * - Custom key containing ``.``
     - Global named style, e.g. ``app.card``, ``app.pageTitle``

A global theme should not define styles for specific UI, such as ``shell.*``, ``settings.*``. Those styles belong in the corresponding document's
``styleSet``.

.. _gui-interface-json_ui-assets-theme-sec-06:

Document Local Styleset
----------------------------------------------

An application document can load a ``type: "styleSet"`` resource to maintain its own named styles:

.. code-block:: json

   {
       "type": "styleSet",
       "styles": {
           "shell.card": {
               "bgColor": "${color.surface.base}",
               "borderColor": "${color.border.default}",
               "borderWidth": "1dp",
               "radius": "12dp"
           }
       }
   }

A local style key must contain ``.``. The ``styleRefs`` resolution order is:

1. The built-in default theme ``all`` and control-type layers
2. The current global theme ``all`` and control-type layers
3. Named styles in the document-local ``styleSet``
4. Named style fallback in the current global theme
5. The node's own ``style`` / ``stateStyles`` / ``partStyles``

.. _gui-interface-json_ui-assets-theme-sec-07:

Colors
--------------------

``colors`` creates semantic aliases for theme colors. Color fields of view, theme styles, and document-local styleSet can reference
the current theme's color values via ``${color.<name>}``. ``${color...}`` supports only color style fields, e.g.
``bgColor``, ``textColor``, ``borderColor``, ``lineColor``, ``arcColor``, ``shadowColor``, ``imageRecolor``,
and the corresponding ``stateStyles`` / ``partStyles`` fields.

A file-backed document containing ``${color...}`` is marked theme-sensitive; ``set_theme(theme_id, true)``
re-parses and updates the colors.
