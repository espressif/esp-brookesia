.. _gui-interface-json-ui-styling-placement-sec-00:

定位
==================================

:link_to_translation:`en:[English]`

概览
--------------------

本文档说明 ``placement`` 的字段语义。公开 JSON 使用 ``camelCase``。

本文档只负责 ``placement``。当前节点对子节点的布局方式请查看 :doc:`layout`。

相关文档
--------------------

- :doc:`index`
- :doc:`../index`
- :doc:`layout`
- :doc:`props/index`
- :doc:`../runtime`

心智模型
--------------------

``placement`` 只负责当前节点自身如何被父节点摆放，包括：

- 宽高
- 绝对坐标
- 相对父节点或当前文件内其它控件的锚点定位
- 放入父 grid 时的 cell 信息

``placement`` 不负责当前节点对子节点的布局。子节点布局统一写在 ``layout``，详见 :doc:`layout`。

Dimension
--------------------

``placement.width`` / ``placement.height`` 使用 dimension：

.. list-table::
   :header-rows: 1

   * - JSON 值
     - 解析结果
     - Backend 语义
   * - ``120``
     - 固定 ``120px``
     - 固定像素尺寸
   * - ``"120dp"``
     - ``round(120 * Environment.density)``
     - 固定像素
   * - ``"50%"``
     - ``SizeMode::Percent``，值为 ``50``
     - 使用父级可用尺寸的百分比
   * - ``"match"``
     - ``SizeMode::Match``
     - 匹配父级可用尺寸
   * - ``"wrap"`` 或缺省
     - ``SizeMode::Wrap``
     - 按内容所需尺寸

``dp`` 字符串只支持 ``dp`` 后缀；百分比字符串只支持 ``%`` 后缀；裸数字表示已经换算好的 backend 像素，不会乘以 ``density``。

``placement.x``、``placement.y``、``placement.width`` 和 ``placement.height`` 可使用 constant 或 env 表达式生成数值，例如：

.. code-block:: json

   {
       "placement": {
           "x": "${constant.ui.overlay.metric.railWidth}",
           "y": "${constant.ui.overlay.metric.topBarHeight}",
           "width": "${expr(${env.widthDp} - ${constant.ui.overlay.metric.railWidth})}",
           "height": "${expr(${env.heightDp} - ${constant.ui.overlay.metric.topBarHeight})}"
       }
   }

``${expr(...)}`` 的结果必须能落到字段原本支持的类型：``x/y`` 通常是 integer、``"Ndp"`` 或 ``"N%"``，
``width/height`` 通常是 dimension。表达式语法、``${env.*}`` 字段和单位规则见
:doc:`../assets/constant`。

placement
--------------------

``placement`` 是可选 object。缺省时会先应用 ``gui_interface`` 内建 placement baseline，再由 JSON 显式字段覆盖。

一般节点 baseline 为：

- ``mode``: ``"absolute"``
- ``width`` / ``height``: ``"wrap"``
- ``x`` / ``y``: ``0``
- ``align``: ``"topLeft"``
- ``relativeTo``: ``""``
- ``gridColumn`` / ``gridRow``: ``0``
- ``gridColumnSpan`` / ``gridRowSpan``: ``1``（建议填写 ``>= 1`` 的整数）
- ``alignSelf``: ``"start"``
- ``flexGrow``: ``0``

``screen`` 是例外：

- 若 ``screen`` 完全省略 ``placement.width`` / ``placement.height``
- 或 ``placement`` object 存在但未填写这两个字段
- runtime 会把缺省尺寸补成 ``"match"``

也就是说，这条差异本身也是 baseline 规则，而不是额外补丁：

- 普通节点默认仍然是 ``wrap``
- ``screen`` 默认会撑满父显示区域
- 如果你在 ``screen`` 上显式写了 ``"wrap"`` 或固定尺寸，则以显式值为准

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - UI 影响
     - Backend 行为 / 限制
   * - ``mode``
     - ``flow`` / ``absolute`` / ``relative``
     - ``absolute``
     - 当前节点如何在父对象中定位
     - ``flow`` 不主动定位；``absolute`` 使用 x/y；``relative`` 使用锚点和可选目标节点
   * - ``width``
     - dimension
     - 一般节点为 ``"wrap"``；screen 缺省时为 ``"match"``
     - 当前节点宽度
     - backend 应用最终宽度语义
   * - ``height``
     - dimension
     - 一般节点为 ``"wrap"``；screen 缺省时为 ``"match"``
     - 当前节点高度
     - backend 应用最终高度语义
   * - ``aspectRatio``
     - string 或 number
     - 无
     - 外框横纵比约束
     - 使用 fit 短边语义；只在 ``width`` / ``height`` 都不是 ``wrap`` 时可用
   * - ``x``
     - number px、``"Ndp"`` 或 ``"N%"``
     - ``0``
     - absolute 坐标或 relative 对齐偏移
     - 百分比相对父内容宽度
   * - ``y``
     - number px、``"Ndp"`` 或 ``"N%"``
     - ``0``
     - absolute 坐标或 relative 对齐偏移
     - 百分比相对父内容高度
   * - ``align``
     - placement align enum，见 placement align enum
     - ``topLeft``
     - relative 锚点
     - backend 锚点枚举
   * - ``relativeTo``
     - string
     - ``""``
     - ``${view.<path>}``
     - 空字符串表示相对父对象；非空时必须使用当前文件根相对的 ``${view.*}`` 点路径
   * - ``gridColumn``
     - integer
     - ``0``
     - 当前节点放入父 grid 的起始列
     - 父节点是 grid 时参与 grid cell placement
   * - ``gridRow``
     - integer
     - ``0``
     - 当前节点放入父 grid 的起始行
     - 同 ``gridColumn``
   * - ``gridColumnSpan``
     - integer
     - ``1``
     - 当前节点横跨的列数
     - 原样传给 backend，组件层不做范围钳制；应填写 ``>= 1``
   * - ``gridRowSpan``
     - integer
     - ``1``
     - 当前节点横跨的行数
     - 原样传给 backend，组件层不做范围钳制；应填写 ``>= 1``
   * - ``alignSelf``
     - align enum，见 ``alignSelf``
     - ``start``
     - 当前节点在父 grid cell 内的自对齐
     - 同时用于横向和纵向 grid cell 对齐
   * - ``flexGrow``
     - integer
     - ``0``
     - 当前节点在父 flex track 内占用剩余空间的比例
     - ``0`` 禁用；``1+`` 按比例分配剩余空间

取值说明
--------------------

mode
^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1

   * - 值
     - 含义
   * - ``flow``
     - 只设置尺寸，不主动设置坐标；位置交给父 flex/grid 或 backend 默认流式位置
   * - ``absolute``
     - 使用 ``x/y`` 作为相对父对象左上角的绝对坐标
   * - ``relative``
     - 使用 ``align`` 和可选 ``relativeTo`` 做锚点定位

placement align enum
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``align`` 仅在 ``mode = "relative"`` 时使用。

.. list-table::
   :header-rows: 1

   * - 值
     - 含义
   * - ``topLeft``
     - 目标区域内左上角
   * - ``topMid``
     - 目标区域内上边中点
   * - ``topRight``
     - 目标区域内右上角
   * - ``bottomLeft``
     - 目标区域内左下角
   * - ``bottomMid``
     - 目标区域内下边中点
   * - ``bottomRight``
     - 目标区域内右下角
   * - ``leftMid``
     - 目标区域内左边中点
   * - ``rightMid``
     - 目标区域内右边中点
   * - ``center``
     - 目标区域中心
   * - ``outTopLeft``
     - 目标区域外侧上方，左边对齐
   * - ``outTopMid``
     - 目标区域外侧上方，水平居中
   * - ``outTopRight``
     - 目标区域外侧上方，右边对齐
   * - ``outBottomLeft``
     - 目标区域外侧下方，左边对齐
   * - ``outBottomMid``
     - 目标区域外侧下方，水平居中
   * - ``outBottomRight``
     - 目标区域外侧下方，右边对齐
   * - ``outLeftTop``
     - 目标区域外侧左方，上边对齐
   * - ``outLeftMid``
     - 目标区域外侧左方，垂直居中
   * - ``outLeftBottom``
     - 目标区域外侧左方，下边对齐
   * - ``outRightTop``
     - 目标区域外侧右方，上边对齐
   * - ``outRightMid``
     - 目标区域外侧右方，垂直居中
   * - ``outRightBottom``
     - 目标区域外侧右方，下边对齐

alignSelf
^^^^^^^^^^^^^^^^^^^^

``alignSelf`` 使用 :doc:`layout` 中的 align enum：``start``、``center``、``end``、``spaceBetween``、
``spaceAround``、``spaceEvenly``、``stretch``。在 grid child placement 中最常用的是 ``start``、``center``、``end`` 和
``stretch``；其中 ``stretch`` 表示填满所在 grid cell。

flow
--------------------

``flow`` 不是默认 placement。backend 只设置尺寸，不主动应用坐标或锚点。如果父节点是
flex/grid，节点会参与父布局；如果父节点是 ``layout.type = "none"``，则沿用 backend 默认位置。

百分比尺寸
--------------------

``placement.width`` / ``placement.height`` 可使用百分比字符串，例如 ``"50%"``。百分比按父对象可用尺寸计算。
``"100%"`` 与 ``"match"`` 在常规 placement size 上等价，但百分比可以表达更细的比例。

``placement.x`` / ``placement.y`` 也可使用百分比字符串，例如 ``"50%"``。百分比 offset 按当前节点父对象的
内容区域计算：``x`` 使用父内容宽度，``y`` 使用父内容高度。``relativeTo`` 只改变锚点目标，百分比 offset 的
计算基准仍是当前节点父对象。

aspectRatio
----------------------

``placement.aspectRatio`` 用于约束节点外框横纵比，支持：

.. list-table::
   :header-rows: 1

   * - JSON 值
     - 含义
   * - ``"16:9"``
     - 宽高比 ``16 / 9``
   * - ``"1024:600"``
     - 宽高比 ``1024 / 600``
   * - ``1.7067``
     - 直接使用数字 ratio

当 ``width`` / ``height`` 给出的边界不满足该比例时，runtime 会按 **fit 短边** 语义缩小其中一边，
保证最终外框完整落在给定边界内。例如 ``width = 100``、``height = 80``、``aspectRatio = "1:1"`` 时，
最终外框为 ``80 x 80``。

限制：

- ``aspectRatio`` 只约束当前 view 外框，不控制子节点布局。
- ``aspectRatio`` 需要 ``width`` 和 ``height`` 都不是 ``wrap``。
- 不提供 cover/crop 模式；需要图片内容裁剪或填充时，使用 ``imageProps.innerAlign``。

absolute
--------------------

``absolute`` 使用 ``x/y`` 作为相对父对象左上角的坐标。

.. code-block:: json

   {
       "placement": {
           "mode": "absolute",
           "x": "24dp",
           "y": "16dp",
           "width": "160dp",
           "height": "48dp"
       }
   }

对应后端行为：应用固定宽高，并按父对象左上角加上 ``x/y`` 偏移定位。

relative
--------------------

``relative`` 使用锚点定位。``relativeTo`` 为空时相对父对象，非空时相对当前文件内目标节点。

.. code-block:: json

   {
       "placement": {
           "mode": "relative",
           "align": "center",
           "x": 0,
           "y": 0
       }
   }

.. code-block:: json

   {
       "placement": {
           "mode": "relative",
           "relativeTo": "${view.host.anchor}",
           "align": "outRightMid",
           "x": "12dp",
           "y": 0
       }
   }

限制：

- ``out*`` 对齐必须设置 ``relativeTo``，因为部分 backend 的父对象对齐不支持 outside anchor。
- ``relativeTo`` 必须使用 ``${view.<path>}``，例如 ``${view.anchor}``、``${view.host.anchor}``。
- ``path`` 相对于当前 JSON 文件根节点，不是相对于当前节点。
- 路径段使用 ``.`` 分隔，不支持 ``/`` 分隔。
- 允许引用当前文件内祖先、同级和旁系节点。
- 不允许引用自身或自身后代。

Grid 子项放置
--------------------

当父节点 ``layout.type = "grid"`` 时，子节点通过 ``placement.gridColumn`` / ``placement.gridRow`` /
``placement.gridColumnSpan`` / ``placement.gridRowSpan`` / ``placement.alignSelf`` 决定自己如何落入 cell。

.. code-block:: json

   {
       "placement": {
           "height": "40dp",
           "gridColumn": 1,
           "gridRow": 0,
           "alignSelf": "stretch"
       }
   }

如果希望 grid 子项填满所在 cell，推荐使用 ``placement.alignSelf: "stretch"``。不要把
``placement.width: "match"`` 当作 cell 拉伸语义。

Flex grow
--------------------

``placement.flexGrow`` 用于父节点 ``layout.type = "flex"`` 时的剩余空间分配，语义对应 LVGL
``lv_obj_set_flex_grow()``：

- 默认值为 ``0``，表示不参与 grow。
- ``1`` 或更大的整数表示按比例分配当前 flex track 的剩余空间。
- 它不是尺寸字段，不会改变 ``placement.width: "match"`` 的含义。
- 在 Figma Auto Layout 中，主轴 ``Fill container`` 应导出为 ``placement.flexGrow: 1``，而不是
  ``placement.width: "match"`` 或 ``placement.height: "match"``。

.. code-block:: json

   {
       "placement": {
           "mode": "flow",
           "width": "wrap",
           "height": "1dp",
           "flexGrow": 1
       }
   }

Image 尺寸
--------------------

``type = "image"`` 节点有额外 sizing 行为。图片资源必须通过 ``imageProps.src: "${image.<id>}"`` 引用，资源中声明的
``width`` / ``height`` 会传到后端。

.. list-table::
   :header-rows: 1

   * - Placement 尺寸
     - 当前行为
   * - 未固定 ``width`` 和 ``height``
     - 使用 image asset 的实际 ``width`` / ``height`` 作为 image view 尺寸
   * - 同时固定 ``width`` 和 ``height``
     - 使用固定外框尺寸；图片内容对齐/缩放由 ``imageProps.innerAlign`` 决定
   * - 只固定 ``width``
     - 根据源图宽度计算比例，推导高度
   * - 只固定 ``height``
     - 根据源图高度计算比例，推导宽度
   * - 使用百分比或 ``match``
     - 作为显式外框尺寸处理；不会被 image asset 实际宽高覆盖

限制：

- 只有 ``placement.width/height`` 是固定尺寸且大于 0 时才参与单边推导；``match`` / 百分比 / ``wrap`` 不参与单边推导。
- ``aspectRatio`` 约束 image view 外框；图片内容仍由 ``imageProps.innerAlign`` 决定。
- ``innerAlign`` 可选择 ``contain``、``cover``、``stretch``、``center``、``tile`` 等图片内容布局方式。小图标固定外框通常应显式使用 ``imageProps.innerAlign: "contain"``。
- 如果图片资源没有有效实际宽高，后端不会执行特殊 sizing。
