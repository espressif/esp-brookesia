.. _gui-interface-json-ui-assets-theme-sec-00:

theme
====================

:link_to_translation:`en:[English]`

概览
--------------------

``theme`` descriptor 描述 Runtime 全局主题入口。主题入口负责组合 token 片段并生成最终
``colors`` 与全局 ``styles``；应用或 Shell 的专属命名样式应放在文档自己的 ``styleSet`` 资源中。

相关文档
--------------------

- :doc:`index`
- :doc:`../index`
- :doc:`../styling/style`
- :doc:`../runtime`

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
     - 固定为 ``"theme"``
   * - ``id``
     - string
     - 无
     - 是
     - 主题 id
   * - ``assets``
     - array
     - ``[]``
     - 否
     - theme 私有 ``constant`` token 片段路径或 inline constant
   * - ``variants``
     - array
     - ``[]``
     - 否
     - 按环境条件追加的 theme 私有 ``constant`` token 片段
   * - ``styles``
     - object
     - ``{}``
     - 否
     - 入口文件内的最终全局 style 覆盖

``theme`` 文件不支持 ``colors`` 字段。颜色别名必须放在 ``constant`` asset 的 ``data.colors`` 中。

主题 token 常量
----------------------

``assets`` / ``variants.assets`` 只接受 ``type: "constant"`` 资源。Theme parser 会先按顺序合并
base assets，再合并命中的 variant assets，然后从最终 constants 中读取：

- ``colors``: ``${color.xxx}`` 可引用的语义色表
- ``styles``: 全局 theme styles
- 其它 token：可通过 ``${constant.xxx}`` 供 theme styles 使用，例如字体、圆角、间距

``colors`` value 必须是真实颜色字符串或空字符串，不能再引用其它资源或颜色别名。

示例：

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

Style Key 约定
------------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 说明
   * - ``all``
     - 所有节点通用 style 层
   * - ``screen`` / ``container`` / ``label`` 等控件类型
     - 按节点控件类型追加覆盖
   * - 包含 ``.`` 的自定义 key
     - 全局通用命名样式，例如 ``app.card``、``app.pageTitle``

全局 theme 不应定义具体 UI 的样式，如 ``shell.*``、``settings.*``。这些样式应放在相应文档的
``styleSet`` 中。

document 本地 styleSet
----------------------------------------

应用文档可以加载 ``type: "styleSet"`` 资源维护自己的命名样式：

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

本地 style key 必须包含 ``.``。``styleRefs`` 解析顺序为：

1. 内建默认 theme 的 ``all`` 和控件类型层
2. 当前全局 theme 的 ``all`` 和控件类型层
3. document-local ``styleSet`` 中的命名样式
4. 当前全局 theme 中的命名样式 fallback
5. 节点自身 ``style`` / ``stateStyles`` / ``partStyles``

colors
--------------------

``colors`` 用于给主题色建立语义别名，view、theme style 和 document-local styleSet 的颜色字段可以通过
``${color.<name>}`` 引用当前 theme 的颜色值。``${color...}`` 只支持颜色类 style 字段，例如
``bgColor``、``textColor``、``borderColor``、``lineColor``、``arcColor``、``shadowColor``、``imageRecolor``
以及对应的 ``stateStyles`` / ``partStyles`` 字段。

包含 ``${color...}`` 的 file-backed document 会被标记为 theme-sensitive，``set_theme(theme_id, true)``
时会重新解析并更新颜色。
