.. _gui-interface-json-ui-assets-font-sec-00:

fontSet
====================

:link_to_translation:`en:[English]`

.. _gui-interface-json_ui-assets-font-sec-01:

概览
--------------------

``fontSet`` descriptor 描述一组 Runtime 全局字体资源。字体不作为 document ``assets`` 标准类型，应通过 Runtime 注册接口加载。

字体资源统一组织为 ``fontSet.fonts[]``，即使只有一个字体。

.. _gui-interface-json_ui-assets-font-sec-02:

相关文档
--------------------

- :doc:`index`
- :doc:`../index`
- :doc:`../runtime`

.. _gui-interface-json_ui-assets-font-sec-03:

字段表
--------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - 是否必填
     - 说明
   * - ``type``
     - string
     - 无
     - 是
     - 固定为 ``"fontSet"``
   * - ``fonts``
     - object[]
     - 无
     - 是
     - 字体 descriptor 列表，不能为空
   * - ``fonts[].id``
     - string
     - 无
     - 是
     - 字体资源 id
   * - ``fonts[].kind``
     - string
     - ``"file"``
     - 否
     - 字体类型；支持 ``"file"`` / ``"imageFont"``
   * - ``fonts[].src``
     - string
     - ``""``
     - file 是
     - 字体文件路径；相对 ``fontSet`` 文件所在目录解析
   * - ``fonts[].languages``
     - string[]
     - ``[]``
     - file 是
     - 支持的语言列表，用于 ``list_supported_fonts(language)`` 和 ``list_supported_languages()``
   * - ``fonts[].fallbacks``
     - string[]
     - ``[]``
     - 否
     - fallback 字体引用，元素使用 ``${font.<id>}``
   * - ``fonts[].height``
     - int
     - ``0``
     - imageFont 单尺寸是
     - image font glyph 高度
   * - ``fonts[].glyphs``
     - object[]
     - ``[]``
     - imageFont 单尺寸是
     - image font glyph 列表
   * - ``fonts[].glyphs[].codepoint``
     - string
     - 无
     - 是
     - glyph Unicode 码点，例如 ``"U+F801"``
   * - ``fonts[].glyphs[].src``
     - string
     - 无
     - 是
     - glyph 图片路径；相对 ``fontSet`` 文件所在目录解析
   * - ``fonts[].sizes``
     - object[]
     - ``[]``
     - imageFont 多尺寸是
     - 多档 image font glyph 尺寸；不能和 ``height/glyphs`` 同时出现
   * - ``fonts[].sizes[].height``
     - int
     - 无
     - 是
     - 当前档 glyph 高度
   * - ``fonts[].sizes[].glyphs``
     - object[]
     - 无
     - 是
     - 当前档 glyph 列表

``fonts[]`` entry 不包含 ``type``。同一个 ``fontSet`` 内不能出现重复 id。

``imageFont`` 用于把图片作为字体 glyph 使用，常用于 label 内联图标。``imageFont`` 不参与
``list_supported_languages()``，也不应作为某个语言的默认字体；需要通过 ``style.font: "${font.<id>}"`` 显式引用。
``imageFont`` 可用 ``height + glyphs`` 描述单尺寸，也可用 ``sizes[]`` 描述多尺寸。多尺寸时每档必须包含相同
codepoint 集合；backend 根据 ``style.imageFontSize`` 选择最接近的一档。``style.imageFontSize`` 只影响 image font
glyph，不改变 fallback 普通文字的 ``fontSize``。

.. _gui-interface-json_ui-assets-font-sec-04:

Runtime API
----------------------

- ``register_font(...)``
- ``register_font_json(...)``
- ``register_font_file(...)``
- ``list_supported_fonts(language = "")``
- ``list_supported_languages()``
- ``set_default_font_for_language(language, font_id)``
- ``get_default_font_for_language(language)``

``register_font_json/file(...)`` 会注册 ``fontSet`` 中的所有字体；若其中任意一项注册失败，本次已注册的字体会回滚。

``style.font`` 使用 ``${font.<id>}`` 引用字体 id。未显式设置 ``style.font`` 的节点会跟随 Runtime 当前语言选择默认字体。应用可先注册字体，再调用 ``set_default_font_for_language(language, font_id)`` 建立语言到字体的映射；``set_language(...)`` 会重新应用已加载 document 的样式。

.. _gui-interface-json_ui-assets-font-sec-05:

示例
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
