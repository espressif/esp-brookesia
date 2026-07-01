.. _gui-interface-json-ui-assets-font-sec-00:

Fontset
====================

:link_to_translation:`zh_CN:[中文]`

Overview
--------------------

The ``fontSet`` descriptor describes a set of Runtime global font resources. Fonts are not a document ``assets`` standard type; load them through the Runtime registration interface.

Font resources are always organized into ``fontSet.fonts[]``, even for a single font.

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../index`
- :doc:`../runtime`

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
     - Fixed to ``"fontSet"``
   * - ``fonts``
     - object[]
     - none
     - Yes
     - Font descriptor list; cannot be empty
   * - ``fonts[].id``
     - string
     - none
     - Yes
     - Font resource id
   * - ``fonts[].kind``
     - string
     - ``"file"``
     - No
     - Font kind; supports ``"file"`` / ``"imageFont"``
   * - ``fonts[].src``
     - string
     - ``""``
     - file: yes
     - Font file path; resolved relative to the ``fontSet`` file directory
   * - ``fonts[].languages``
     - string[]
     - ``[]``
     - file: yes
     - Supported languages, used by ``list_supported_fonts(language)`` and ``list_supported_languages()``
   * - ``fonts[].fallbacks``
     - string[]
     - ``[]``
     - No
     - Fallback font references; each element uses ``${font.<id>}``
   * - ``fonts[].height``
     - int
     - ``0``
     - imageFont single-size: yes
     - Image-font glyph height
   * - ``fonts[].glyphs``
     - object[]
     - ``[]``
     - imageFont single-size: yes
     - Image-font glyph list
   * - ``fonts[].glyphs[].codepoint``
     - string
     - none
     - Yes
     - Glyph Unicode code point, e.g. ``"U+F801"``
   * - ``fonts[].glyphs[].src``
     - string
     - none
     - Yes
     - Glyph image path; resolved relative to the ``fontSet`` file directory
   * - ``fonts[].sizes``
     - object[]
     - ``[]``
     - imageFont multi-size: yes
     - Multiple image-font glyph sizes; cannot coexist with ``height/glyphs``
   * - ``fonts[].sizes[].height``
     - int
     - none
     - Yes
     - Glyph height of the current size
   * - ``fonts[].sizes[].glyphs``
     - object[]
     - none
     - Yes
     - Glyph list of the current size

A ``fonts[]`` entry does not contain ``type``. Duplicate ids cannot appear within one ``fontSet``.

``imageFont`` uses images as font glyphs, typically for inline icons in a label. ``imageFont`` does not participate in
``list_supported_languages()`` and should not be the default font of a language; reference it explicitly via ``style.font: "${font.<id>}"``.
``imageFont`` can describe a single size with ``height + glyphs`` or multiple sizes with ``sizes[]``. With multiple sizes, every size must contain the same
codepoint set; the backend picks the closest one by ``style.imageFontSize``. ``style.imageFontSize`` only affects image-font
glyphs and does not change the fallback regular-text ``fontSize``.

Runtime API
----------------------

- ``register_font(...)``
- ``register_font_json(...)``
- ``register_font_file(...)``
- ``list_supported_fonts(language = "")``
- ``list_supported_languages()``
- ``set_default_font_for_language(language, font_id)``
- ``get_default_font_for_language(language)``

``register_font_json/file(...)`` registers all fonts in a ``fontSet``; if any one fails to register, the fonts registered this time are rolled back.

``style.font`` uses ``${font.<id>}`` to reference a font id. A node without an explicit ``style.font`` follows the Runtime current language to pick the default font. An app can register fonts first and then call ``set_default_font_for_language(language, font_id)`` to map languages to fonts; ``set_language(...)`` reapplies the styles of loaded documents.

Example
--------------------

.. code-block:: json

   {
       "type": "fontSet",
       "fonts": [
           {
               "id": "title",
               "kind": "file",
               "src": "./fonts/title.ttf",
               "languages": ["en"],
               "fallbacks": [
                   "${font.default}"
               ]
           },
           {
               "id": "symbol.icons",
               "kind": "imageFont",
               "fallbacks": [
                   "${font.default}"
               ],
               "sizes": [
                   {
                       "height": 20,
                       "glyphs": [
                           {"codepoint": "U+F801", "src": "./fonts/icon/20/info.png"},
                           {"codepoint": "U+F802", "src": "./fonts/icon/20/warning.png"}
                       ]
                   },
                   {
                       "height": 28,
                       "glyphs": [
                           {"codepoint": "U+F801", "src": "./fonts/icon/28/info.png"},
                           {"codepoint": "U+F802", "src": "./fonts/icon/28/warning.png"}
                       ]
                   }
               ]
           }
       ]
   }
