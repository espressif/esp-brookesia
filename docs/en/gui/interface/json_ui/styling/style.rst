.. _gui-interface-json-ui-styling-style-sec-00:

Style
==========================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-styling-style-sec-01:

Overview
--------------------

This page explains the semantics of the ``style`` fields. Public JSON uses ``camelCase``.

This page covers only ``style``. For control semantics see :doc:`../view/index`; for field-level ``props`` see :doc:`props/index`.

.. _gui-interface-json_ui-styling-style-sec-02:

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../index`
- :doc:`../assets/index`
- :doc:`../view/index`
- :doc:`props/index`

.. _gui-interface-json_ui-styling-style-sec-03:

Style Composition Order
----------------------------------------------

``style`` is an optional object. The final style merge order is currently fixed as:

1. built-in default ``theme.styles.all``
2. built-in default ``theme.styles.<node type>``
3. current Runtime theme ``styles.all``
4. current Runtime theme ``styles.<node type>``
5. named styles referenced by ``styleRefs`` in the current Runtime theme
6. the node's explicit ``style``
7. the backend's minimal technical fallback

Where:

- ``gui_interface`` always applies a built-in default theme first
- the Runtime external theme is an optional overlay, not a source of defaults
- the node's explicit ``style`` always overrides the built-in default theme and the external theme field by field
- fields not set by any theme or by the node fall back, last, to the backend's minimal technical fallback

Each time the backend applies a style it maps the final ``ResolvedStyle`` to its own native style object. An omitted
``style`` field does not equal the backend default; most formal defaults come from the built-in default theme, and the backend only provides an implementation-level safety fallback.

Runtime view debug overlays the backend debug outline:

- this visualization is not ``style.margin``
- it does not participate in layout and does not change ``placement``, ``layout``, the real margin, or the final frame
- its goal is to help observe control size and position, not to express formal UI style

.. _gui-interface-json_ui-styling-style-sec-04:

Field Table
----------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - UI Effect
     - Backend Behavior / Limits
   * - ``bgColor``
     - string, ``"#RRGGBB"``
     - ``""``
     - main background color
     - applies the background color when non-empty and parsable; transparent when finally empty
   * - ``bgGradientColor``
     - string, ``"#RRGGBB"``
     - ``""``
     - main background transition color
     - works with ``bgColor`` and ``bgGradientDirection``; suitable for a bar / slider indicator
   * - ``bgGradientDirection``
     - enum
     - ``none``
     - background gradient direction
     - allowed values: ``none``, ``horizontal``, ``vertical``
   * - ``bgMainStop``
     - integer, ``0..255``
     - backend default
     - main background color stop
     - LVGL background gradient stop; meaningful only when a gradient is configured
   * - ``bgGradientStop``
     - integer, ``0..255``
     - backend default
     - transition color stop
     - LVGL background gradient stop; meaningful only when a gradient is configured
   * - ``bgGradientOpacity``
     - integer, ``0..255``
     - ``255``
     - transition color opacity
     - LVGL background gradient opacity
   * - ``textColor``
     - string, ``"#RRGGBB"`` or ``"${color.*}"``
     - ``""``
     - text color
     - applies the text color
   * - ``textAlign``
     - string enum, ``auto`` / ``left`` / ``center`` / ``right``
     - ``auto``
     - horizontal text alignment
     - maps to backend text alignment
   * - ``borderColor``
     - string, ``"#RRGGBB"``
     - ``""``
     - border color
     - if ``borderWidth`` is 0, the border is usually not visible
   * - ``lineColor``
     - string, ``"#RRGGBB"``
     - ``""``
     - line control line color
     - applies the line color
   * - ``arcColor``
     - string, ``"#RRGGBB"``
     - ``""``
     - arc line color
     - applied to the LVGL arc style; the indicator is usually written in ``partStyles.indicator``
   * - ``arcGradientColor``
     - string, ``"#RRGGBB"``
     - ``""``
     - arc line transition color
     - the LVGL backend approximates a two-color gradient for arc using segmented drawing
   * - ``font``
     - string, must be ``"${font.<id>}"``
     - ``default`` in the built-in default theme
     - text font resource
     - the parser expands it to a font id; the runtime resolves it to a backend-usable font resource; when the font set has no match, ``default`` may fall back to the backend built-in font
   * - ``fontSize``
     - number px or ``"Nsp"``
     - ``16`` in the built-in default theme
     - text size
     - when the field exists, ``"Nsp"`` converts to px by ``round(N * Environment.density * Environment.font_scale)``
   * - ``imageFontSize``
     - number px or ``"Nsp"``
     - the imageFont first-tier size
     - imageFont glyph size selection
     - effective only for ``kind=imageFont``; picks the closest ``fontSet.fonts[].sizes[].height`` and does not change the normal fallback text size
   * - ``borderWidth``
     - number px or ``"Ndp"``
     - ``0`` in the built-in default theme
     - border width
     - overrides the theme when the field is explicitly present; the backend falls back to ``0`` when finally unset
   * - ``radius``
     - number px or ``"Ndp"``
     - ``0`` in the built-in default theme
     - corner radius
     - overrides the theme when the field is explicitly present; the backend falls back to ``0`` when finally unset
   * - ``padding``
     - number px or ``"Ndp"``
     - ``0``
     - padding on all four sides
     - used as the four-side default; per-side fields can override it
   * - ``paddingLeft``
     - number px or ``"Ndp"``
     - ``0``
     - left padding
     - if unset, falls back to ``padding``, then to ``0``
   * - ``paddingRight``
     - number px or ``"Ndp"``
     - ``0``
     - right padding
     - if unset, falls back to ``padding``, then to ``0``
   * - ``paddingTop``
     - number px or ``"Ndp"``
     - ``0``
     - top padding
     - if unset, falls back to ``padding``, then to ``0``
   * - ``paddingBottom``
     - number px or ``"Ndp"``
     - ``0``
     - bottom padding
     - if unset, falls back to ``padding``, then to ``0``
   * - ``margin``
     - number px or ``"Ndp"``
     - ``0``
     - margin on all four sides
     - used as the four-side default; per-side fields can override it
   * - ``marginLeft``
     - number px or ``"Ndp"``
     - ``0``
     - left margin
     - if unset, falls back to ``margin``, then to ``0``
   * - ``marginRight``
     - number px or ``"Ndp"``
     - ``0``
     - right margin
     - if unset, falls back to ``margin``, then to ``0``
   * - ``marginTop``
     - number px or ``"Ndp"``
     - ``0``
     - top margin
     - if unset, falls back to ``margin``, then to ``0``
   * - ``marginBottom``
     - number px or ``"Ndp"``
     - ``0``
     - bottom margin
     - if unset, falls back to ``margin``, then to ``0``
   * - ``shadowWidth``
     - number px or ``"Ndp"``
     - ``0`` in the built-in default theme
     - shadow size
     - inherits the theme layer when omitted on the node
   * - ``shadowOffsetX``
     - number px or ``"Ndp"``
     - ``0`` in the built-in default theme
     - shadow X offset
     - applies the shadow X offset
   * - ``shadowOffsetY``
     - number px or ``"Ndp"``
     - ``0`` in the built-in default theme
     - shadow Y offset
     - applies the shadow Y offset
   * - ``shadowColor``
     - string, ``"#RRGGBB"``
     - ``""``
     - shadow color
     - applied when the final color is non-empty and parsable
   * - ``opacity``
     - integer, ``0..255``
     - ``255`` in the built-in default theme
     - overall opacity
     - overrides the theme when the field is explicitly present; the backend falls back to ``255`` when finally unset
   * - ``lineWidth``
     - number px or ``"Ndp"``
     - ``0`` in the built-in default theme
     - line control line width
     - applies the line width
   * - ``imageOpacity``
     - integer, ``0..255``
     - ``255`` in the built-in default theme
     - image draw opacity
     - overrides the theme when the field is explicitly present; the backend falls back to ``255`` when finally unset
   * - ``imageRecolor``
     - string, ``"#RRGGBB"``
     - ``""``
     - image draw recolor
     - an empty string disables style-layer recoloring
   * - ``imageRecolorOpacity``
     - integer, ``0..255``
     - ``255``
     - image recolor blend strength
     - has a visual effect only when ``imageRecolor`` is effective
   * - ``arcWidth``
     - number px or ``"Ndp"``
     - backend default
     - arc line width
     - may be written in ``style`` or ``partStyles.indicator.style``
   * - ``arcOpacity``
     - integer, ``0..255``
     - ``255``
     - arc line opacity
     - may be written in ``style`` or ``partStyles.indicator.style``
   * - ``arcGradientSegments``
     - integer, ``2..128``
     - ``32``
     - number of arc gradient segments
     - more segments give a smoother result at a higher drawing cost
   * - ``arcRounded``
     - boolean
     - backend default
     - arc endpoint rounding
     - maps to LVGL arc rounded
   * - ``clipCorner``
     - boolean
     - backend default
     - whether to clip child content to the corner radius
     - maps to LVGL clip corner; usually used with ``radius``

.. _gui-interface-json-ui-styling-style-state-styles:

State Styles
----------------------

A node can use ``stateStyles`` to provide local style overrides for a single interaction state. ``stateStyles`` follows the same field rules as ``style``,
but does not support ``font`` / ``fontSize`` / ``imageFontSize``, to avoid recreating the font cache on state changes.

.. code-block:: json

   {
       "type": "button",
       "id": "open",
       "styleRefs": ["app.card"],
       "stateStyles": {
           "pressed": {"bgColor": "#e2e8f0"},
           "disabled": {"opacity": 120},
           "checked": {"borderColor": "#2563eb", "borderWidth": "2dp"}
       }
   }

Supported state names:

.. list-table::
   :header-rows: 1

   * - State
     - Meaning
   * - ``pressed``
     - being pressed
   * - ``checked``
     - checked; e.g. switch / checkbox, or a control with a manually set checked state
   * - ``focused``
     - focused
   * - ``focusKey``
     - keyboard/arrow-key focus
   * - ``edited``
     - editing state
   * - ``hovered``
     - pointer hover
   * - ``scrolled``
     - scrolling
   * - ``disabled``
     - disabled state
   * - ``user1`` / ``user2`` / ``user3`` / ``user4``
     - reserved user states

Composition rules:

- the composition order of the base ``style`` is unchanged.
- each state's override style is composed at the same layers: built-in theme, current theme, ``styleRefs``, the node's own ``stateStyles``.
- a state style writes only explicit fields; unset fields inherit the final style of the normal state.
- only a single-state selector is supported; combined states like ``pressed+checked`` are not.
- binding / event effects can write ``stateStyles.<state>.<styleField>``, for example
  ``stateStyles.pressed.bgColor``.

.. _gui-interface-json-ui-styling-style-part-styles:

Part Styles
--------------------

``partStyles`` sets styles for a control's internal parts. The existing ``style`` always refers to the ``main`` part; ``partStyles``
refers to other parts of the same view. Currently supported:

.. list-table::
   :header-rows: 1

   * - Part
     - Common controls
     - Meaning
   * - ``indicator``
     - ``progressBar`` / ``slider`` / ``arc``
     - filled progress, the slider's selected track, the arc indicator
   * - ``knob``
     - ``slider`` / ``arc``
     - drag handle

Both a direct style shorthand and ``style + stateStyles`` are supported:

.. code-block:: json

   {
       "type": "slider",
       "id": "brightness",
       "partStyles": {
           "indicator": {
               "bgColor": "#2563eb",
               "bgGradientColor": "#38bdf8",
               "bgGradientDirection": "horizontal"
           },
           "knob": {
               "style": {
                   "bgColor": "#ffffff",
                   "radius": "14dp"
               },
               "stateStyles": {
                   "pressed": {"bgColor": "#e0f2fe"}
               }
           }
       }
   }

Composition rules:

- ``style`` is still composed as the ``main`` part.
- each part's style is composed at the same layers: built-in theme, current theme, ``styleRefs``, the node's own ``partStyles``.
- ``partStyles.<part>.stateStyles`` also supports state overrides.
- ``partStyles`` does not support ``font`` / ``fontSize`` / ``imageFontSize``.
- binding / event effects can write fields such as ``partStyles.indicator.bgGradientColor`` and
  ``partStyles.knob.stateStyles.pressed.bgColor``.

.. _gui-interface-json_ui-styling-style-sec-07:

Range Gradient
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ``partStyles.indicator`` of ``progressBar`` and ``slider`` uses an LVGL background gradient, suitable for horizontal/vertical progress colors:

.. code-block:: json

   "partStyles": {
       "indicator": {
           "bgColor": "#22c55e",
           "bgGradientColor": "#3b82f6",
           "bgGradientDirection": "horizontal"
       }
   }

``arc`` has no native arc gradient; the LVGL backend approximates one with segmented drawing when ``partStyles.indicator.arcGradientColor`` is present:

.. code-block:: json

   "partStyles": {
       "indicator": {
           "arcColor": "#22c55e",
           "arcGradientColor": "#3b82f6",
           "arcWidth": "10dp",
           "arcRounded": true,
           "arcGradientSegments": 48
       }
   }

.. _gui-interface-json_ui-styling-style-sec-08:

Theme Default Style Layer
--------------------------------------------------

The runtime always applies a built-in default theme before resolving a node's final style. If the Runtime has additionally loaded a global ``theme`` and selected the current theme via ``set_theme(...)``, it also continues to auto-apply:

- ``styles.all``
- ``styles.<node type>``

Example:

.. code-block:: json

   {
       "id": "dark",
       "styles": {
           "all": {
               "bgColor": "#020617"
           },
           "label": {
               "textColor": "#f8fafc"
           }
       }
   }

If a ``label`` node then explicitly writes:

.. code-block:: json

   "style": {
       "textColor": "#ef4444"
   }

then the final ``textColor`` uses the node's explicit value.

A color field accepts two forms: a literal ``#RRGGBB``, or a color reference ``${color.<path>}`` (resolved from the current theme's ``colors``).
``${color.*}`` is allowed only on color style fields such as ``bgColor``, ``textColor``, ``borderColor``, ``lineColor``, ``arcColor``,
``shadowColor``, ``imageRecolor`` and their corresponding ``stateStyles`` / ``partStyles`` fields. An empty string means the color is not set;
illegal colors or other CSS-style forms raise an error in the document validator stage and never reach the backend application flow.

.. _gui-interface-json_ui-styling-style-sec-09:

Font and Font Size
------------------------------------

``font`` must use a resource reference:

.. code-block:: json

   {
       "style": {
           "font": "${font.title}",
           "fontSize": "${constant.metrics.titleFont}",
           "textColor": "${constant.colors.primaryText}"
       }
   }

Resource reference rules:

- ``font: "${font.title}"`` is kept as a font resource reference in the parser and is not substituted like an ordinary constant.
- the runtime merges JSON font resources and backend dynamic font resources.
- if both JSON and backend provide a font with the same name, the backend dynamic resource wins.
- ``font: "title"``, ``${font:title}``, or writing a font file path directly into ``style.font`` are not supported.

Font size rules:

- ``fontSize`` should use ``"Nsp"`` or an ``"Nsp"`` resolved from ``${constant.*}``.
- the ``sp`` formula is ``round(N * Environment.density * Environment.font_scale)``.
- a bare number is an already-converted px and is not affected by ``font_scale``.
- when ``fontSize`` is not set explicitly, the built-in default theme provides ``16``.
- if a font must be applied but the final ``fontSize`` is ``0``, the backend font selection logic uses the default size ``16``.
- ``imageFontSize`` is recommended for nodes whose ``style.font`` points to ``kind=imageFont``, such as inline icons in a label.
  After converting ``sp`` to px it only selects the imageFont glyph image size; ordinary characters are still controlled by ``fontSize``.

Backend font priority:

1. the native font in ``ResolvedFontSpec.native_fonts`` closest to the requested size.
2. the ``primary_src`` of the JSON/backend font resource, creating FreeType fonts along the fallback chain.
3. the built-in Montserrat font; it also falls back to the built-in font when FreeType or the file is unavailable.

.. _gui-interface-json_ui-styling-style-sec-10:

Size and Spacing
--------------------------------

Size-class style fields accept a bare number px or ``"Ndp"``:

.. code-block:: json

   {
       "style": {
           "padding": "${constant.metrics.sectionPadding}",
           "borderWidth": "1dp",
           "radius": "${constant.radii.card}",
           "shadowWidth": "8dp",
           "shadowOffsetY": "2dp"
       }
   }

Current rules:

- each style layer overrides the previous one only when the field is explicitly present; a field omitted on the node inherits the final merged value of the built-in theme or the current Runtime theme.
- the ``all`` layer of the built-in default theme provides numeric baselines such as ``borderWidth`` / ``radius`` / ``padding`` / ``margin`` / ``shadowWidth`` / ``lineWidth``.
- when ``padding`` and per-side ``padding*`` both exist, the backend uses the final ``padding`` as the fallback and then overrides it per side.
- ``margin`` and per-side ``margin*`` work the same way.
- color and font ids still keep the semantics that an empty string means no specific resource/color is set.

.. _gui-interface-json_ui-styling-style-sec-11:

Transparency
------------------------

.. code-block:: json

   {
       "style": {
           "opacity": 220,
           "imageOpacity": 180,
           "imageRecolor": "#0f172a",
           "imageRecolorOpacity": 255
       }
   }

Notes:

- ``opacity`` affects the overall opacity of the object.
- ``imageOpacity`` affects image draw opacity.
- ``imageRecolor`` / ``imageRecolorOpacity`` affect the image draw color and can be placed in a theme style so icons adapt to the theme uniformly.
- the built-in default theme defaults are all ``255``, meaning fully opaque; an external theme or an explicit node field can override this.
- the current parser only requires an integer; the validator further restricts it to ``0..255``; values out of range fail document validation.

.. _gui-interface-json_ui-styling-style-sec-12:

Example
--------------------

The example below shows typical style and resource-reference usage, keeping resource references in strict namespaces:

.. code-block:: json

   {
       "type": "container",
       "id": "content",
       "style": {
           "padding": "${constant.metrics.sectionPadding}",
           "bgColor": "${constant.colors.cardBg}",
           "borderColor": "${constant.colors.border}",
           "borderWidth": "1dp",
           "radius": "${constant.radii.card}"
       },
       "children": [
           {
               "type": "label",
               "id": "title",
               "labelProps": {
                   "text": "Controls Gallery"
               },
               "style": {
                   "font": "${font.title}",
                   "fontSize": "${constant.metrics.titleFont}",
                   "textColor": "${constant.colors.primaryText}"
               }
           },
           {
               "type": "line",
               "id": "line",
               "style": {
                   "lineColor": "${constant.colors.accent}",
                   "lineWidth": "3dp"
               }
           }
       ]
   }
