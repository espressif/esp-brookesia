.. _gui-interface-json-ui-styling-style-sec-00:

样式
==========================

:link_to_translation:`en:[English]`

概览
--------------------

本文档说明 ``style`` 的字段语义。公开 JSON 使用 ``camelCase``。

本文档只负责 ``style``。控件语义请查看 :doc:`../view/index`，字段级 ``props`` 请查看 :doc:`props/index`。

相关文档
--------------------

- :doc:`index`
- :doc:`../index`
- :doc:`../assets/index`
- :doc:`../view/index`
- :doc:`props/index`

样式合成顺序
--------------------

``style`` 是可选 object。当前最终样式合并顺序固定为：

1. 内建默认 ``theme.styles.all``
2. 内建默认 ``theme.styles.<节点 type>``
3. 当前 Runtime theme ``styles.all``
4. 当前 Runtime theme ``styles.<节点 type>``
5. 当前 Runtime theme 中 ``styleRefs`` 引用的命名样式
6. 节点显式 ``style``
7. backend 最小技术兜底

其中：

- ``gui_interface`` 总会先应用一份内建默认 theme
- Runtime 外部 theme 是可选覆盖层，不是默认值来源
- 节点显式 ``style`` 永远按字段覆盖内建默认 theme 和外部 theme
- 未被 theme 和节点显式设置的字段，最后才回落到 backend 最小技术兜底

backend 每次应用样式时会把最终 ``ResolvedStyle`` 映射到自己的原生样式对象。``style``
缺省不等于 backend 默认值；多数正式默认值来自内建默认 theme，backend 只负责实现层安全兜底。

Runtime view debug 叠加的是 backend debug outline：

- 这层可视化不是 ``style.margin``
- 它不参与布局，不改变 ``placement``、``layout``、真实 margin 或最终 frame
- 目标是帮助观察控件大小和位置，而不是表达正式 UI 样式

字段表
--------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - UI 影响
     - Backend 行为 / 限制
   * - ``bgColor``
     - string, ``"#RRGGBB"``
     - ``""``
     - 主体背景色
     - 非空且可解析时应用背景色；最终为空时背景透明
   * - ``bgGradientColor``
     - string, ``"#RRGGBB"``
     - ``""``
     - 主体背景过渡色
     - 与 ``bgColor``、``bgGradientDirection`` 配合；适合 bar / slider indicator
   * - ``bgGradientDirection``
     - enum
     - ``none``
     - 背景渐变方向
     - 可选值：``none``、``horizontal``、``vertical``
   * - ``bgMainStop``
     - integer, ``0..255``
     - backend 默认
     - 主背景色 stop
     - LVGL 背景渐变 stop；仅在配置渐变时有意义
   * - ``bgGradientStop``
     - integer, ``0..255``
     - backend 默认
     - 过渡色 stop
     - LVGL 背景渐变 stop；仅在配置渐变时有意义
   * - ``bgGradientOpacity``
     - integer, ``0..255``
     - ``255``
     - 过渡色透明度
     - LVGL 背景渐变 opacity
   * - ``textColor``
     - string, ``"#RRGGBB"`` 或 ``"${color.*}"``
     - ``""``
     - 文本颜色
     - 应用文本颜色
   * - ``textAlign``
     - string enum，``auto`` / ``left`` / ``center`` / ``right``
     - ``auto``
     - 文本水平对齐
     - 映射到 backend 文本对齐
   * - ``borderColor``
     - string, ``"#RRGGBB"``
     - ``""``
     - 边框颜色
     - 若 ``borderWidth`` 为 0，通常看不到边框
   * - ``lineColor``
     - string, ``"#RRGGBB"``
     - ``""``
     - line 控件线条颜色
     - 应用线条颜色
   * - ``arcColor``
     - string, ``"#RRGGBB"``
     - ``""``
     - arc 弧线颜色
     - 应用到 LVGL arc style；indicator 通常写在 ``partStyles.indicator``
   * - ``arcGradientColor``
     - string, ``"#RRGGBB"``
     - ``""``
     - arc 弧线过渡色
     - LVGL backend 对 arc 使用分段绘制近似两色渐变
   * - ``font``
     - string, 必须为 ``"${font.<id>}"``
     - 内建默认 theme 为 ``default``
     - 文本字体资源
     - parser 展开为 font id；runtime 解析为 backend 可用字体资源；未命中字库时允许对 ``default`` 回退到 backend built-in font
   * - ``fontSize``
     - number px 或 ``"Nsp"``
     - 内建默认 theme 为 ``16``
     - 文本字号
     - 字段存在时 ``"Nsp"`` 按 ``round(N * Environment.density * Environment.font_scale)`` 转 px
   * - ``imageFontSize``
     - number px 或 ``"Nsp"``
     - imageFont 第一档尺寸
     - imageFont glyph 尺寸选择
     - 只对 ``kind=imageFont`` 生效；选择最接近的 ``fontSet.fonts[].sizes[].height``，不改变普通 fallback 文本字号
   * - ``borderWidth``
     - number px 或 ``"Ndp"``
     - 内建默认 theme 为 ``0``
     - 边框宽度
     - 字段显式存在时覆盖 theme；最终无值时 backend 用 ``0`` 兜底
   * - ``radius``
     - number px 或 ``"Ndp"``
     - 内建默认 theme 为 ``0``
     - 圆角半径
     - 字段显式存在时覆盖 theme；最终无值时 backend 用 ``0`` 兜底
   * - ``padding``
     - number px 或 ``"Ndp"``
     - ``0``
     - 四边内边距
     - 作为四边默认值，分方向字段可覆盖
   * - ``paddingLeft``
     - number px 或 ``"Ndp"``
     - ``0``
     - 左内边距
     - 若未写则回退到 ``padding``，再回退到 ``0``
   * - ``paddingRight``
     - number px 或 ``"Ndp"``
     - ``0``
     - 右内边距
     - 若未写则回退到 ``padding``，再回退到 ``0``
   * - ``paddingTop``
     - number px 或 ``"Ndp"``
     - ``0``
     - 上内边距
     - 若未写则回退到 ``padding``，再回退到 ``0``
   * - ``paddingBottom``
     - number px 或 ``"Ndp"``
     - ``0``
     - 下内边距
     - 若未写则回退到 ``padding``，再回退到 ``0``
   * - ``margin``
     - number px 或 ``"Ndp"``
     - ``0``
     - 四边外边距
     - 作为四边默认值，分方向字段可覆盖
   * - ``marginLeft``
     - number px 或 ``"Ndp"``
     - ``0``
     - 左外边距
     - 若未写则回退到 ``margin``，再回退到 ``0``
   * - ``marginRight``
     - number px 或 ``"Ndp"``
     - ``0``
     - 右外边距
     - 若未写则回退到 ``margin``，再回退到 ``0``
   * - ``marginTop``
     - number px 或 ``"Ndp"``
     - ``0``
     - 上外边距
     - 若未写则回退到 ``margin``，再回退到 ``0``
   * - ``marginBottom``
     - number px 或 ``"Ndp"``
     - ``0``
     - 下外边距
     - 若未写则回退到 ``margin``，再回退到 ``0``
   * - ``shadowWidth``
     - number px 或 ``"Ndp"``
     - 内建默认 theme 为 ``0``
     - 阴影尺寸
     - 节点省略时继承 theme 层
   * - ``shadowOffsetX``
     - number px 或 ``"Ndp"``
     - 内建默认 theme 为 ``0``
     - 阴影 X 偏移
     - 应用阴影 X 偏移
   * - ``shadowOffsetY``
     - number px 或 ``"Ndp"``
     - 内建默认 theme 为 ``0``
     - 阴影 Y 偏移
     - 应用阴影 Y 偏移
   * - ``shadowColor``
     - string, ``"#RRGGBB"``
     - ``""``
     - 阴影颜色
     - 最终颜色非空且可解析时应用
   * - ``opacity``
     - integer, ``0..255``
     - 内建默认 theme 为 ``255``
     - 整体透明度
     - 字段显式存在时覆盖 theme；最终无值时 backend 用 ``255`` 兜底
   * - ``lineWidth``
     - number px 或 ``"Ndp"``
     - 内建默认 theme 为 ``0``
     - line 控件线宽
     - 应用线宽
   * - ``imageOpacity``
     - integer, ``0..255``
     - 内建默认 theme 为 ``255``
     - image 绘制透明度
     - 字段显式存在时覆盖 theme；最终无值时 backend 用 ``255`` 兜底
   * - ``imageRecolor``
     - string, ``"#RRGGBB"``
     - ``""``
     - image 绘制重着色
     - 空字符串关闭 style 层重着色
   * - ``imageRecolorOpacity``
     - integer, ``0..255``
     - ``255``
     - image 重着色混合强度
     - 仅在 ``imageRecolor`` 有效时产生视觉效果
   * - ``arcWidth``
     - number px 或 ``"Ndp"``
     - backend 默认
     - arc 弧线宽度
     - 可写在 ``style`` 或 ``partStyles.indicator.style``
   * - ``arcOpacity``
     - integer, ``0..255``
     - ``255``
     - arc 弧线透明度
     - 可写在 ``style`` 或 ``partStyles.indicator.style``
   * - ``arcGradientSegments``
     - integer, ``2..128``
     - ``32``
     - arc 渐变分段数量
     - 段数越大越平滑，绘制成本也越高
   * - ``arcRounded``
     - boolean
     - backend 默认
     - arc 端点圆角
     - 映射到 LVGL arc rounded
   * - ``clipCorner``
     - boolean
     - backend 默认
     - 是否按圆角裁剪子内容
     - 映射到 LVGL clip corner；通常配合 ``radius`` 使用

.. _gui-interface-json-ui-styling-style-state-styles:

stateStyles
----------------------

节点可通过 ``stateStyles`` 为单个交互状态提供局部 style 覆盖。``stateStyles`` 的字段规则与 ``style`` 相同，
但不支持 ``font`` / ``fontSize`` / ``imageFontSize``，避免状态切换时重新创建字体缓存。

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

支持的状态名：

.. list-table::
   :header-rows: 1

   * - 状态
     - 含义
   * - ``pressed``
     - 正在按下
   * - ``checked``
     - 已选中；例如 switch / checkbox 或手动设置 checked state 的控件
   * - ``focused``
     - 获得焦点
   * - ``focusKey``
     - 键盘/方向键焦点
   * - ``edited``
     - 编辑状态
   * - ``hovered``
     - 指针悬停
   * - ``scrolled``
     - 滚动中
   * - ``disabled``
     - 禁用状态
   * - ``user1`` / ``user2`` / ``user3`` / ``user4``
     - 预留用户状态

合成规则：

- base ``style`` 的合成顺序不变。
- 每个 state 的覆盖样式按同样层级合成：内建 theme、当前 theme、``styleRefs``、节点自身 ``stateStyles``。
- state style 只写显式字段；未写字段继承普通状态的最终样式。
- 只支持单一状态选择器，不支持 ``pressed+checked`` 这种组合状态。
- binding / event effect 可写入 ``stateStyles.<state>.<styleField>``，例如
  ``stateStyles.pressed.bgColor``。

.. _gui-interface-json-ui-styling-style-part-styles:

partStyles
--------------------

``partStyles`` 用于给控件内部部件设置样式。现有 ``style`` 始终表示 ``main`` 部分；``partStyles``
表示同一个 view 的其他部件。当前支持：

.. list-table::
   :header-rows: 1

   * - Part
     - 常见控件
     - 含义
   * - ``indicator``
     - ``progressBar`` / ``slider`` / ``arc``
     - 已填充进度、slider 选中轨道、arc 指示弧
   * - ``knob``
     - ``slider`` / ``arc``
     - 拖动手柄

写法支持直接 style shorthand，也支持 ``style + stateStyles``：

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

合成规则：

- ``style`` 仍按 ``main`` 部分合成。
- 每个 part 的 style 按相同层级合成：内建 theme、当前 theme、``styleRefs``、节点自身 ``partStyles``。
- ``partStyles.<part>.stateStyles`` 同样支持 state 覆盖。
- ``partStyles`` 不支持 ``font`` / ``fontSize`` / ``imageFontSize``。
- binding / event effect 可写入 ``partStyles.indicator.bgGradientColor``、
  ``partStyles.knob.stateStyles.pressed.bgColor`` 等字段。

Range 渐变
^^^^^^^^^^^^^^^^^^^^

``progressBar`` 和 ``slider`` 的 ``partStyles.indicator`` 使用 LVGL 背景渐变，适合横向/纵向进度色：

.. code-block:: json

   "partStyles": {
       "indicator": {
           "bgColor": "#22c55e",
           "bgGradientColor": "#3b82f6",
           "bgGradientDirection": "horizontal"
       }
   }

``arc`` 没有原生 arc gradient；LVGL backend 在 ``partStyles.indicator.arcGradientColor`` 存在时使用分段绘制近似：

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

主题默认样式层
--------------------

Runtime 在解析节点最终 style 前总会先应用一份内建默认 theme。若 Runtime 已额外加载全局 ``theme``，并通过 ``set_theme(...)`` 选中当前主题，还会继续自动应用：

- ``styles.all``
- ``styles.<节点 type>``

示例：

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

若某个 ``label`` 节点又显式写了：

.. code-block:: json

   "style": {
       "textColor": "#ef4444"
   }

则最终 ``textColor`` 以节点显式值为准。

颜色字段接受两种写法：字面量 ``#RRGGBB``，或颜色引用 ``${color.<path>}``（从当前 theme 的 ``colors`` 解析）。
``${color.*}`` 仅可用于颜色类 style 字段，例如 ``bgColor``、``textColor``、``borderColor``、``lineColor``、``arcColor``、
``shadowColor``、``imageRecolor`` 及其对应的 ``stateStyles`` / ``partStyles`` 字段。空字符串表示不设置该颜色；
非法颜色或其他 CSS 写法会在 document validator 阶段报错，不会进入 backend 应用流程。

字体与字号
--------------------

``font`` 必须使用资源引用：

.. code-block:: json

   {
       "style": {
           "font": "${font.title}",
           "fontSize": "${constant.metrics.titleFont}",
           "textColor": "${constant.colors.primaryText}"
       }
   }

资源引用规则：

- ``font: "${font.title}"`` 在 parser 中保留为字体资源引用，不会按普通常量替换。
- runtime 会合并 JSON 字体资源和 backend 动态字体资源。
- 若 JSON 和 backend 都提供同名字体，backend 动态资源优先。
- 不支持 ``font: "title"``、``${font:title}`` 或把字体文件路径直接写到 ``style.font``。

字号规则：

- ``fontSize`` 推荐使用 ``"Nsp"`` 或 ``${constant.*}`` 中解析出的 ``"Nsp"``。
- ``sp`` 公式为 ``round(N * Environment.density * Environment.font_scale)``。
- 裸数字表示已经换算好的 px，不受 ``font_scale`` 影响。
- 未显式设置 ``fontSize`` 时，内建默认 theme 提供 ``16``。
- 若需要应用字体但最终 ``fontSize`` 为 ``0``，backend 字体选择逻辑会使用默认字号 ``16``。
- ``imageFontSize`` 推荐用于 ``style.font`` 指向 ``kind=imageFont`` 的节点，例如 label 内联图标。
  它按 ``sp`` 转 px 后只选择 imageFont 的 glyph 图片尺寸；普通字符仍由 ``fontSize`` 控制。

后端字体优先级：

1. ``ResolvedFontSpec.native_fonts`` 中最接近请求字号的 native font。
2. JSON/backend 字体资源的 ``primary_src``，并按 fallback 链创建 FreeType 字体。
3. 内置 Montserrat 字体；FreeType 不可用或文件不可用时也会回退到内置字体。

尺寸与间距
--------------------

尺寸类 style 字段接受裸数字 px 或 ``"Ndp"``：

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

当前规则：

- 每一层 style 只有字段显式存在时才覆盖前序层；节点省略字段会继承内建 theme 或当前 Runtime theme 的最终合并值。
- 内建默认 theme 的 ``all`` 层提供 ``borderWidth`` / ``radius`` / ``padding`` / ``margin`` / ``shadowWidth`` / ``lineWidth`` 等数值 baseline。
- ``padding`` 和分方向 ``padding*`` 同时存在时，backend 以最终 ``padding`` 作为回退值，再由分方向字段逐项覆盖。
- ``margin`` 和分方向 ``margin*`` 同理。
- 颜色和字体 id 仍保留“空字符串表示不设置具体资源/颜色”的语义。

透明度
--------------------

.. code-block:: json

   {
       "style": {
           "opacity": 220,
           "imageOpacity": 180,
           "imageRecolor": "#0f172a",
           "imageRecolorOpacity": 255
       }
   }

说明：

- ``opacity`` 影响对象整体透明度。
- ``imageOpacity`` 影响 image 绘制透明度。
- ``imageRecolor`` / ``imageRecolorOpacity`` 影响 image 绘制颜色，可放在 theme style 中供 icon 统一适配主题。
- 内建默认 theme 的默认值都是 ``255``，表示完全不透明；外部 theme 或节点显式字段可覆盖该值。
- 当前 parser 只要求 integer，validator 会进一步限制为 ``0..255``；超出范围会导致 document 校验失败。

示例
--------------------

下面示例展示典型的样式与资源引用写法，资源引用保持严格 namespace：

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
