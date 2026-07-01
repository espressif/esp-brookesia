.. _gui-interface-json-ui-styling-props-keyboard-sec-00:

keyboardProps
==========================

:link_to_translation:`en:[English]`

概览
--------------------

``keyboardProps`` 适用于 ``keyboard``。

相关文档
--------------------

- :doc:`index`
- :doc:`../../view/keyboard`

字段表
--------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - Binding
     - UI 影响 / 限制
   * - ``mode``
     - string
     - ``"text"``
     - ``keyboardProps.mode``
     - 键盘模式；backend 按字符串映射到支持的键盘模式
   * - ``allowedModes``
     - string[]
     - ``[]``
     - ``keyboardProps.allowedModes``
     - 允许切换到的模式；binding 使用逗号字符串，如 ``"text,upper,number,special"``
   * - ``popovers``
     - bool
     - ``false``
     - ``keyboardProps.popovers``
     - 是否启用按键弹出提示
   * - ``iconSize``
     - dimension
     - ``0``
     - ``keyboardProps.iconSize``
     - 带 ``image`` 按键的目标图标尺寸；``0`` 表示按图片原始尺寸绘制
   * - ``targetTextInput``
     - string
     - ``""``
     - 否
     - 目标 ``textInput`` 路径；为空时 keyboard 不自动输入到文本框
   * - ``layouts``
     - object
     - ``{}``
     - 否
     - 自定义布局表；由 backend 映射到 native keyboard map
   * - ``keyStyles``
     - object
     - ``{}``
     - 否
     - 按键样式类；支持 ``default`` / ``special`` / ``mode`` / ``action`` / ``disabled``
   * - ``keyStyleRefs``
     - object
     - ``{}``
     - 否
     - 按键样式类到 theme style id 的映射；支持同 ``keyStyles`` 的样式类

mode
--------------------

当前公开取值：

.. list-table::
   :header-rows: 1

   * - 值
     - 含义
   * - ``text``
     - 文本小写键盘
   * - ``upper``
     - 文本大写键盘
   * - ``number``
     - 数字键盘
   * - ``special``
     - 符号/特殊字符键盘

仅支持 ``text``、``upper``、``number``、``special`` 四种 mode；未知 mode 会在 parser/validator
阶段报错。如果 ``allowedModes`` 非空，``role=mode`` 的目标模式不在列表中时按键会使用 disabled 样式并忽略点击。

targetTextInput
------------------------------

``targetTextInput`` 支持同 screen 绝对路径，也支持相对当前 screen root 的点路径。示例：

.. code-block:: json

   "keyboardProps": {
       "targetTextInput": "/keyboard_input/panel/input"
   }

layouts
--------------------

``layouts`` 用 JSON 描述按键行，不是运行时控件。支持 ``text``、``upper``、``number``、``special`` 四种布局。每个按键可以是字符串，也可以是对象：

.. code-block:: json

   {
       "text": "q",
       "width": 1,
       "role": "",
       "mode": "",
       "styleClass": "",
       "image": "${image.icon.backspace}"
   }

带 ``role`` 的按键也可以设置 ``text``。此时 ``role`` 决定按键行为，``text`` 是文字 fallback。若设置 ``image``，backend
会在按键 draw-task 中居中绘制对应 image resource，并跳过 label 绘制；不会创建 per-key 子控件，也不会把图标文本写入
``textInput``。图标按 ``keyboardProps.iconSize`` 缩放；image 缺失或加载失败时回退显示 ``text``。

支持的 ``role``：

.. list-table::
   :header-rows: 1

   * - Role
     - 行为
   * - ``backspace``
     - 删除前一个字符
   * - ``left`` / ``right``
     - 移动光标
   * - ``space``
     - 空格
   * - ``ok``
     - 触发 ``ready``
   * - ``cancel``
     - 触发 ``cancel``
   * - ``mode``
     - 切换到 ``mode`` 字段指定的布局

``width`` 范围为 ``1..15``，用于设置按键相对宽度。

``keyStyles`` 可为按键样式类配置：

.. code-block:: json

   "keyStyles": {
       "action": {
           "bgColor": "#2563eb",
           "textColor": "#ffffff",
           "pressedBgColor": "#1d4ed8",
           "pressedTextColor": "#ffffff",
           "radius": "8dp"
       }
   }

未显式写 ``styleClass`` 时，backend 会按 role 推断：普通字符为 ``default``，控制键为 ``special``，
模式切换键为 ``mode``，确认/取消为 ``action``，不可用模式键为 ``disabled``。

``keyStyleRefs`` 可引用当前 theme 中的命名样式。runtime 会从该 style 的 ``bgColor``、``textColor``、``radius`` 以及
``stateStyles.pressed.bgColor``、``stateStyles.pressed.textColor`` 解析出键盘按键颜色和圆角。``keyStyles`` 仍可作为
内联 fallback；当同一个样式类同时出现在两处时，``keyStyleRefs`` 的解析结果优先生效。

示例
--------------------

.. code-block:: json

   "keyboardProps": {
       "mode": "text",
       "allowedModes": ["text", "upper", "number", "special"],
       "popovers": true,
       "iconSize": "${constant.ui.keyboard.iconSize}",
       "targetTextInput": "/keyboard_input/panel/input",
       "keyStyles": {
           "default": {"bgColor": "#ffffff", "textColor": "#111827"},
           "action": {"bgColor": "#2563eb", "textColor": "#ffffff"},
           "disabled": {"bgColor": "#e5e7eb", "textColor": "#9ca3af"}
       },
       "layouts": {
           "text": [
               ["q", "w", "e", {"text": "ABC", "role": "mode", "mode": "upper", "width": 2}],
               [
                   {"text": "Cancel", "role": "cancel", "width": 2, "image": "${image.icon.cancel}"},
                   {"text": "Space", "role": "space", "width": 6, "image": "${image.icon.space}"},
                   {"text": "OK", "role": "ok", "width": 2, "image": "${image.icon.ok}"}
               ]
           ],
           "upper": [
               ["Q", "W", "E", {"role": "backspace", "width": 2}]
           ],
           "number": [
               ["1", "2", "3", {"role": "ok", "width": 2}]
           ]
       }
   }
