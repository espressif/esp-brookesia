.. _gui-interface-json-ui-assets-constant-sec-00:

constant
====================

:link_to_translation:`en:[English]`

概览
--------------------

``constant`` asset 定义当前 document 可用的常量树。公开 JSON 推荐使用 ``camelCase``。

相关文档
--------------------

- :doc:`index`
- :doc:`../index`
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
     - 固定为 ``"constant"``
   * - ``data``
     - object
     - ``{}``
     - 是
     - 常量树；可嵌套 object、string、number、bool、array 等 JSON 值

引用规则
--------------------

- 使用 ``${constant.<path>}`` 引用常量。
- ``<path>`` 使用点路径，例如 ``${constant.colors.pageBg}``。
- 常量引用会在 parser 阶段按当前命中的 assets 顺序解析。
- 使用 ``${env.<field>}`` 引用当前 document 解析环境；env 不是 constant 树的一部分，不会被
  ``Runtime::get_constant_value()`` 查询到。
- 需要简单数值计算时，使用 ``${expr(...)}``。表达式仍在 parser 阶段解析，不是运行时 binding。
- Runtime 加载 document 后会保留最终合并后的常量树，可通过
  ``Runtime::get_constant_value(document_id, "<path>")`` 读取，例如
  ``get_constant_value(id, "colors.pageBg")``。
- 查询 path 不包含 ``${}``，也不包含 ``constant.`` 前缀；找不到 path 或 path 为空会返回错误。
- 查询返回 ``boost::json::value``，因此 string、number、bool、object、array 和 null 会保留原始 JSON 类型。
- 若 root variants 命中并覆盖默认常量，Runtime 查询返回的是覆盖后的最终值。

表达式
--------------------

``${expr(...)}`` 用于基于 constant 做静态四则运算。表达式内支持：

- ``${constant.<path>}``，引用必须解析为 number 或 ``"Ndp"`` 字符串。
- ``${env.<field>}``，引用必须解析为 number 或 ``"Ndp"`` 字符串。
- 数字字面量，例如 ``0``、``12``、``1.5``。
- ``dp`` 字面量，例如 ``44dp``。
- 运算符 ``+``、``-``、``*``、``/`` 和括号。

单位规则：

.. list-table::
   :header-rows: 1

   * - 运算
     - 规则
   * - ``+`` / ``-``
     - 两侧必须同为 ``dp`` 或同为无单位 number
   * - ``*``
     - 允许 ``dp * number``、``number * dp`` 或 ``number * number``；不允许 ``dp * dp``
   * - ``/``
     - 右侧必须是无单位 number；除数不能为 0

表达式结果为 ``dp`` 时会写回 ``"Ndp"`` 字符串；无单位整数会写回 JSON integer；无单位小数会写回 JSON number。
非数值 constant、单位不兼容、除零、括号不匹配都会让 document 解析失败。

Environment 引用
----------------------------

``${env.*}`` 是独立命名空间，用于读取当前解析环境，不建议把这些值复制进 constants。当前支持：

.. list-table::
   :header-rows: 1

   * - Field
     - 类型
     - 说明
   * - ``widthPx``
     - integer
     - 当前默认 display 宽度，单位 px
   * - ``heightPx``
     - integer
     - 当前默认 display 高度，单位 px
   * - ``widthDp``
     - string
     - 当前默认 display 宽度，单位 dp，例如 ``"1024dp"``
   * - ``heightDp``
     - string
     - 当前默认 display 高度，单位 dp，例如 ``"600dp"``
   * - ``density``
     - number
     - 当前 density
   * - ``fontScale``
     - number
     - 当前 font scale
   * - ``language``
     - string
     - 当前语言 id
   * - ``theme``
     - string
     - 当前 theme id

.. code-block:: json

   {
       "placement": {
           "x": "${constant.layout.leftRailWidth}",
           "width": "${expr(${env.widthDp} - ${constant.layout.leftRailWidth})}",
           "height": "${expr((${env.heightDp} - 44dp) / 2)}"
       }
   }

示例
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
