.. _gui-interface-json-ui-styling-layout-sec-00:

布局
============================

:link_to_translation:`en:[English]`

.. _gui-interface-json_ui-styling-layout-sec-01:

概览
--------------------

本文档说明 ``layout`` 的字段语义。公开 JSON 使用 ``camelCase``。

本文档只负责 ``layout``。节点自身尺寸、坐标、锚点和 grid child placement 语义请查看
:doc:`placement`。

.. _gui-interface-json_ui-styling-layout-sec-02:

相关文档
--------------------

- :doc:`index`
- :doc:`../index`
- :doc:`../view/index`
- :doc:`placement`

.. _gui-interface-json_ui-styling-layout-sec-03:

心智模型
--------------------

``layout`` 只负责当前节点如何布局自己的直接子节点，不负责当前节点自身如何被父节点摆放。节点自身尺寸、
坐标、锚点、grid cell 等信息统一写在 ``placement`` 中，详见 :doc:`placement`。

若开启 Runtime view debug，backend 只会额外绘制 debug outline 方便观察控件边界；这层可视化不参与 ``layout`` 计算。

.. list-table::
   :header-rows: 1

   * - 对象
     - 负责内容
     - Backend 行为
   * - ``layout``
     - 当前节点如何布局自己的直接子节点
     - backend 应用自动布局、flex 或 grid 语义
   * - ``placement``
     - 当前节点如何被父节点摆放
     - backend 应用尺寸、坐标、相对锚点和 grid cell 语义

``style`` 只负责视觉表现，例如颜色、圆角、字体和间距。不要把 ``placement`` 的尺寸、坐标写进 ``layout``。

.. _gui-interface-json_ui-styling-layout-sec-04:

Dimension
--------------------

``layout.gridTemplateColumns`` / ``layout.gridTemplateRows`` 使用 dimension：

.. list-table::
   :header-rows: 1

   * - JSON 值
     - 解析结果
     - Backend 语义
   * - ``120``
     - 固定 ``120px``
     - 固定 grid track
   * - ``"120dp"``
     - ``round(120 * Environment.density)``
     - 固定像素
   * - ``"50%"``
     - ``SizeMode::Percent``，值为 ``50``
     - backend 可映射为相对 track 权重
   * - ``"match"``
     - ``SizeMode::Match``
     - 填充分配到该 track 的剩余空间
   * - ``"wrap"`` 或缺省
     - ``SizeMode::Wrap``
     - 按内容所需尺寸

``dp`` 字符串只支持 ``dp`` 后缀；百分比字符串只支持 ``%`` 后缀；裸数字表示已经换算好的 backend 像素，不会乘以 ``density``。

.. _gui-interface-json_ui-styling-layout-sec-05:

layout
--------------------

``layout`` 是可选 object。缺省时会先应用 ``gui_interface`` 内建 layout baseline，再由 JSON 显式字段覆盖。当前 baseline 等价于：

- ``type = "none"``
- ``flexFlow = "column"``
- ``mainAlign = "start"``
- ``crossAlign = "start"``
- ``gap = 0``
- ``gridTemplateColumns = []``
- ``gridTemplateRows = []``

也就是说，缺省时当前节点不启用 backend 自动布局。

.. _gui-interface-json_ui-styling-layout-sec-06:

字段表
^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - UI 影响
     - Backend 行为 / 限制
   * - ``type``
     - ``none`` / ``flex`` / ``grid``
     - ``none``
     - 决定当前节点对子节点的布局方式
     - ``none`` 关闭自动布局；``flex`` / ``grid`` 启用对应布局
   * - ``gap``
     - number px 或 ``"Ndp"``
     - ``0``
     - flex/grid 子项间距
     - 仅 ``flex`` / ``grid`` 有意义
   * - ``flexFlow``
     - ``row`` / ``column`` / ``rowWrap`` / ``columnWrap``
     - ``column``
     - flex 主轴方向与是否换行
     - 仅 ``type=flex`` 有意义
   * - ``mainAlign``
     - align enum，见 align enum
     - ``start``
     - flex 主轴对齐；grid tracks 整体横向对齐
     - backend 按布局类型映射到主轴或 track 对齐
   * - ``crossAlign``
     - align enum，见 align enum
     - ``start``
     - flex 交叉轴/track 对齐；grid tracks 整体纵向对齐
     - backend 按布局类型映射到交叉轴或 track 对齐
   * - ``gridTemplateColumns``
     - dimension[]
     - ``[]``
     - grid 列轨道定义
     - 每项映射到 backend grid track descriptor
   * - ``gridTemplateRows``
     - dimension[]
     - ``[]``
     - grid 行轨道定义
     - 同 ``gridTemplateColumns``

``align enum`` 支持 ``start``、``center``、``end``、``spaceBetween``、``spaceAround``、``spaceEvenly``、``stretch``。
其中 ``stretch`` 主要用于 ``placement.alignSelf``，用在 flex 对齐时会按 ``start`` 处理。

.. _gui-interface-json_ui-styling-layout-sec-07:

取值说明
--------------------

.. _gui-interface-json_ui-styling-layout-sec-08:

type
^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1

   * - 值
     - 含义
   * - ``none``
     - 不启用 backend 自动布局；子节点由各自 ``placement`` 决定位置
   * - ``flex``
     - 启用 flex 布局，按 ``flexFlow``、``mainAlign``、``crossAlign``、``gap`` 排列直接子节点
   * - ``grid``
     - 启用 grid 布局，按 ``gridTemplateColumns`` / ``gridTemplateRows`` 建立网格

.. _gui-interface-json_ui-styling-layout-sec-09:

flexFlow
^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1

   * - 值
     - 含义
   * - ``row``
     - 主轴横向从左到右，不换行
   * - ``column``
     - 主轴纵向从上到下，不换行
   * - ``rowWrap``
     - 主轴横向，空间不足时换到下一行
   * - ``columnWrap``
     - 主轴纵向，空间不足时换到下一列

.. _gui-interface-json_ui-styling-layout-sec-10:

align enum
^^^^^^^^^^^^^^^^^^^^

``mainAlign``、``crossAlign`` 和 ``placement.alignSelf`` 共享同一组 align 值：

.. list-table::
   :header-rows: 1

   * - 值
     - 含义
   * - ``start``
     - 靠起始边对齐
   * - ``center``
     - 居中对齐
   * - ``end``
     - 靠结束边对齐
   * - ``spaceBetween``
     - 首尾贴边，中间项目均分剩余空间
   * - ``spaceAround``
     - 每个项目两侧分配相等外侧空间
   * - ``spaceEvenly``
     - 项目之间以及首尾外侧空间完全相等
   * - ``stretch``
     - 拉伸填满可用空间；主要用于 grid 子项的 ``placement.alignSelf``

.. _gui-interface-json_ui-styling-layout-sec-11:

none
^^^^^^^^^^^^^^^^^^^^

``none`` 关闭当前节点的自动布局。子节点如何定位完全由各自的 ``placement`` 决定。

.. code-block:: json

   {
       "layout": {
           "type": "none"
       },
       "children": [
           {
               "type": "label",
               "id": "badge",
               "placement": {
                   "mode": "relative",
                   "align": "bottomRight",
                   "x": "-12dp",
                   "y": "-12dp"
               }
           }
       ]
   }

.. _gui-interface-json_ui-styling-layout-sec-12:

flex
^^^^^^^^^^^^^^^^^^^^

``flex`` 让当前节点成为 flex 容器，影响其直接子节点。

.. code-block:: json

   {
       "layout": {
           "type": "flex",
           "flexFlow": "column",
           "mainAlign": "start",
           "crossAlign": "center",
           "gap": "${constant.metrics.pageGap}"
       },
       "placement": {
           "width": "${constant.sizes.match}",
           "height": "${constant.sizes.match}"
       }
   }

对应后端行为：启用 flex 布局，应用 ``flexFlow``、``mainAlign``、``crossAlign`` 和 ``gap``。

如果某个 flex 子节点需要占用当前 track 的剩余空间，应在该子节点的 ``placement`` 中使用
``flexGrow``。不要用 ``placement.width: "match"`` 模拟 Figma Auto Layout 的主轴 ``Fill container``；
``match`` 是相对父对象的 100% 尺寸，不是 flex 剩余空间语义。

.. _gui-interface-json_ui-styling-layout-sec-13:

grid
^^^^^^^^^^^^^^^^^^^^

``grid`` 让当前节点成为 grid 容器，``gridTemplateColumns`` / ``gridTemplateRows`` 定义 tracks。子节点通过
``placement.gridColumn`` / ``placement.gridRow`` 放入 cell。

.. code-block:: json

   {
       "layout": {
           "type": "grid",
           "gap": "10dp",
           "gridTemplateColumns": ["match", "match"],
           "gridTemplateRows": ["wrap", "wrap"]
       },
       "children": [
           {
               "type": "slider",
               "id": "slider",
               "placement": {
                   "height": "40dp",
                   "gridColumn": 1,
                   "gridRow": 0,
                   "alignSelf": "stretch"
               }
           }
       ]
   }

对应后端行为：启用 grid 布局，应用 columns/rows track、整体 align，并用子节点 ``placement.grid*``
字段放入对应 cell。

如果希望 grid 子项填满所在 cell，推荐使用 ``placement.alignSelf: "stretch"``。不要用
``placement.width: "match"`` 模拟 cell 宽度；``match`` 表示相对父对象的 100%，不等价于当前 grid cell。
