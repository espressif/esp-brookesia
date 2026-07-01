.. _gui-interface-json-ui-document-root-sec-00:

root.json 文档入口
========================

:link_to_translation:`en:[English]`

.. _gui-interface-json_ui-document-root-sec-01:

概览
--------------------

本文档负责 document 加载层的入口结构：

- ``root.json`` 顶层结构
- ``variants[]``
- ``when`` 表达式
- ``Environment``
- 单位与字号换算

本文档不负责 asset 字段细节，也不负责 view/layout/style/props。那些内容请分别查看
:doc:`../assets/index`、:doc:`../view/index`、:doc:`../styling/layout`、:doc:`../styling/placement`。

.. _gui-interface-json_ui-document-root-sec-02:

相关文档
--------------------

- :doc:`../index`
- :doc:`index`
- :doc:`../assets/index`
- :doc:`../styling/placement`

.. _gui-interface-json_ui-document-root-sec-03:

root.json 顶层结构
----------------------------

``root.json`` 必须是一个 JSON object。

示例：

.. code-block:: json

   {
       "version": "0.1.0",
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

顶层字段表
^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - 是否必填
     - 说明
   * - ``version``
     - string
     - ``"0.1.0"``
     - 否
     - 协议版本；当前支持 ``"0.1.0"``
   * - ``assets``
     - array<string | object>
     - ``[]``
     - 否
     - 公共资源列表，按原始顺序处理
   * - ``variants``
     - object[]
     - ``[]``
     - 否
     - 条件命中的资源叠加列表

.. _gui-interface-json_ui-document-root-sec-05:

顶层字段说明
^^^^^^^^^^^^^^^^^^^^

.. _gui-interface-json_ui-document-root-sec-06:

version
~~~~~~~~~~~~~~~~~~~~

- 类型：``string``
- 是否必填：否
- 当前值：``"0.1.0"``

.. _gui-interface-json_ui-document-root-sec-07:

assets
~~~~~~~~~~~~~~~~~~~~

- 类型：``array<string | object>``
- 是否必填：否
- 默认值：空数组

说明：

- 表示 document 的公共资源列表
- 每一项都可以是：
  - 文件路径字符串
  - 内嵌 asset JSON object
- 路径相对于当前 ``root.json`` 所在目录
- 内嵌 object 中的相对路径也相对于当前 ``root.json`` 所在目录
- 可以混合包含 ``constant`` / ``styleSet`` / ``imageSet`` / ``viewScreen`` / ``viewTemplate`` / ``interactionTemplate`` / ``screenFlow`` asset
- 混写时严格按数组原始顺序生效；``constants`` / ``nodes`` / ``screenFlow`` 不再作为 root 顶层字段，请放入 ``assets``

.. _gui-interface-json_ui-document-root-sec-08:

variants
~~~~~~~~~~~~~~~~~~~~

- 类型：``object[]``
- 是否必填：否
- 默认值：空数组

说明：

- ``variants`` 可以缺省或为空
- 为空时只加载顶层 ``assets``
- 所有命中的 variant 都会叠加，不是命中一个就停止

.. _gui-interface-json_ui-document-root-sec-09:

variants[]
--------------------

每个 variant 是一个 JSON object。

示例：

.. code-block:: json

   {
       "when": "${expr(${env.language} == \"zh\" && ${env.widthDp} < 600dp)}",
       "assets": [
           "../../shared/constants/i18n_zh.json",
           "../../shared/constants/layout_phone.json"
       ]
   }

字段表：

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - 是否必填
     - 说明
   * - ``when``
     - string
     - ``"${expr(true)}"``
     - 否
     - variant 命中条件，必须写成 ``${expr(...)}``
   * - ``assets``
     - array<string | object>
     - ``[]``
     - 否
     - variant 命中后需要叠加的资源

.. _gui-interface-json_ui-document-root-sec-10:

assets entry 双形态
--------------------------------

``root.json.assets`` 和 ``variants[].assets`` 都支持两种 entry：

- 路径字符串
- 内嵌 asset object

示例：

.. code-block:: json

   {
       "version": "0.1.0",
       "assets": [
           "./constants/base.json",
           {
               "type": "viewScreen",
               "id": "home_screen",
               "children": []
           }
       ]
   }

规则：

- ``string`` entry：
  - 继续表示外部 asset 文件路径
- ``object`` entry：
  - 必须是合法 asset object
  - 必须包含 ``type``
  - document 标准类型为 ``constant`` / ``styleSet`` / ``imageSet`` / ``viewScreen`` / ``viewTemplate`` / ``interactionTemplate`` / ``screenFlow``
  - ``fontSet`` / ``theme`` 不是 document ``assets`` 标准类型，应通过 Runtime 全局 API 注册或加载
- 两种 entry 可以混写
- parser 会按数组原始顺序处理，不会因为 entry 类型不同而重新排序
- 对 ``imageSet`` / ``viewScreen`` / ``viewTemplate`` / ``interactionTemplate`` 内嵌 object 中出现的相对路径：
  - 统一相对当前 ``root.json`` 所在目录解析

.. _gui-interface-json_ui-document-root-sec-11:

when 表达式
--------------------

.. _gui-interface-json_ui-document-root-sec-12:

支持的环境引用
^^^^^^^^^^^^^^^^^^^^

- ``${env.widthDp}``
- ``${env.heightDp}``
- ``${env.widthPx}``
- ``${env.heightPx}``
- ``${env.language}``
- ``${env.theme}``

.. _gui-interface-json_ui-document-root-sec-13:

支持的运算符
^^^^^^^^^^^^^^^^^^^^

- 比较：``>``, ``<``, ``==``, ``!=``, ``>=``, ``<=``
- 逻辑：``&&``, ``||``, ``!``
- 分组：``(``, ``)``
- 算术：``+``, ``-``, ``*``, ``/``
- 字符串字面量：``"zh"``
- 数值单位：``dp``

.. _gui-interface-json_ui-document-root-sec-14:

示例
^^^^^^^^^^^^^^^^^^^^

.. code-block:: json

   { "when": "${expr(${env.language} == \"zh\")}" }
   { "when": "${expr(${env.theme} == \"shell.dark\")}" }
   { "when": "${expr(${env.widthDp} < 600dp)}" }
   { "when": "${expr(${env.widthDp} >= 600dp && ${env.heightDp} >= 360dp)}" }
   { "when": "${expr((${env.widthDp} >= 600dp && ${env.heightDp} >= 400dp) || ${env.widthPx} >= 1280)}" }

.. _gui-interface-json_ui-document-root-sec-15:

运行时环境 Environment
----------------------------------

``when`` 必须写成 ``${expr(...)}``，且表达式结果必须是 boolean，例如 ``"${expr(${env.widthDp} < 600dp)}"``。

``when`` 求值依赖调用方传入的 ``Environment``。

当前结构见
``gui/brookesia_gui_interface/include/brookesia/gui_interface/document.hpp``。

主要字段（默认值见 ``document.hpp``）：

- ``width_px``：默认 ``320``
- ``height_px``：默认 ``480``
- ``density``：默认 ``1.0``
- ``font_scale``：默认 ``1.0``
- ``language``：默认 ``"zh"``
- ``theme_id``：默认 ``"default"``
- ``colors``：``map<string, string>`` 颜色表，供 ``${color.*}`` 解析使用；注意 **不能** 通过 ``${env.*}`` 读取

换算关系：

- ``${env.widthDp} = width_px / density``
- ``${env.heightDp} = height_px / density``

这些字段也可以通过 ``${env.*}`` 在 JSON 资源中引用。当前支持 ``${env.widthPx}``、``${env.heightPx}``、
``${env.widthDp}``、``${env.heightDp}``、``${env.density}``、``${env.fontScale}``、``${env.language}`` 和
``${env.theme}``。``${env.widthDp}`` / ``${env.heightDp}`` 会输出 ``"Ndp"`` 字符串，适合与 ``${expr(...)}`` 组合使用。

``Environment`` 只负责：

- ``when`` 表达式求值
- ``dp/sp`` 尺寸换算
- 语言和视口等协议输入
- 初始 theme id 记录；``when`` 中的 ``${env.theme}`` 使用该值
- Runtime 全局 ``set_theme(...)`` 仍是切换主题的正式入口

如果 document 的 ``variants[].when`` 使用了 ``${env.theme}`` 或 ``${env.language}``，Runtime 会把它标记为
environment-sensitive。``set_theme(theme_id, true)`` / ``set_language(language, true)`` 对 file-backed document 会重新解析
root，以便常量、图片和 screen 资源跟随 variant 更新；非 file-backed document 只能重新应用 style，并记录 warning。

主题资源与主题切换请查看 :doc:`../runtime`。

view 调试外框不属于 ``Environment``，而是 Runtime 级调试能力；详见 :doc:`../runtime`。

.. _gui-interface-json_ui-document-root-sec-16:

单位与字号换算
--------------------

JSON UI 中的尺寸最终都会在解析阶段换算成 backend 使用的像素值。

基础概念：

- ``px``
  - backend 物理像素
  - 协议中用 JSON number 表示，例如 ``24``
  - 当前不支持 ``"24px"`` 字符串后缀
- ``dp``
  - density independent pixel
  - 用于布局尺寸、间距、圆角、边框等非文字尺寸
  - 换算公式：``px = round(dp * Environment.density)``
- ``sp``
  - scale independent pixel
  - 用于字号
  - 换算公式：``px = round(sp * Environment.density * Environment.font_scale)``
- ``fontSize``
  - ``style.fontSize`` 的公开字段名
  - 语义等价于字号 ``sp``
  - 最终传给 backend 的字号为换算后的整数像素值

字段规则：

- ``layout.gap`` 使用 ``dp`` 字符串或裸数字 ``px``
- ``placement.x`` / ``placement.y`` 使用 ``dp`` 字符串、百分比或裸数字 ``px``
- ``placement.width`` / ``placement.height`` 支持 ``dp`` 字符串、百分比、裸数字 ``px``、``match``、``wrap``
- ``style.borderWidth`` / ``style.radius`` / ``style.padding`` 使用 ``dp`` 字符串或裸数字 ``px``
- ``style.fontSize`` 使用 ``sp`` 字符串或裸数字 ``px``
- 裸数字始终表示已经换算好的 ``px``，不会再乘以 ``density`` 或 ``font_scale``

示例：

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

若 ``Environment.density = 2.0`` 且 ``Environment.font_scale = 1.2``：

- ``"120dp"`` -> ``round(120 * 2.0) = 240px``
- ``48`` -> ``48px``
- ``"12dp"`` -> ``round(12 * 2.0) = 24px``
- ``"20sp"`` -> ``round(20 * 2.0 * 1.2) = 48px``

``fontSize`` 与 native 字体资源的关系：

- native font 的 ``native_size`` 表示已生成字体的像素字号
- backend 按换算后的 ``fontSize`` 像素值选择 native font
- 若没有精确匹配，则选择最接近的 ``native_size`` 并输出 warning
