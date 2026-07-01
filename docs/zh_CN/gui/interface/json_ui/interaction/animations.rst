.. _gui-interface-json-ui-interaction-animations-sec-00:

动画
====================================

:link_to_translation:`en:[English]`

.. _gui-interface-json_ui-interaction-animations-sec-01:

概览
--------------------

本文档说明 ``animations`` 的字段语义。公开 JSON 使用 ``camelCase``，enum 值按本文档写法书写。

本文档只负责 ``animations``。事件触发语义请查看 :doc:`events`，节点结构请查看 :doc:`../view/index`。

.. _gui-interface-json_ui-interaction-animations-sec-02:

相关文档
--------------------

- :doc:`index`
- :doc:`../index`
- :doc:`../view/index`
- :doc:`events`

.. _gui-interface-json_ui-interaction-animations-sec-03:

总览
--------------------

``animations`` 是可选数组，可挂在任意 view 节点上（如 ``container`` / ``label`` / ``viewScreen`` 等）；不存在通用的
``type = "view"`` 节点类型。每个数组项描述一个基础声明式动画。

当前实现支持：

- 触发时机：``manual`` / ``mount`` / ``show`` / ``hide``
- 动画属性：``opacity`` / ``x`` / ``y`` / ``width`` / ``height`` / ``scale`` / ``angle`` / ``offsetX`` / ``offsetY``
- 整数 ``from`` / ``to``
- ``fromMode = current``：启动时从 backend 当前属性值开始
- ``toMode = relative``：以解析后的 ``from`` 为基准计算结束值
- 基础 easing、delay、repeat、playback

后端创建节点时的应用顺序为：props 域、``layout``、``placement``（其后还会再应用一次 ``commonProps`` 的 transform，如 ``zoom`` / ``angle`` / ``pivot``）、``style``、debug outline、``animations``、``events``，随后订阅 ``bindings``。因此 ``mount`` 动画开始时，节点的初始 props、布局、摆放和样式已经应用。

.. _gui-interface-json_ui-interaction-animations-sec-04:

字段表
--------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - UI 影响
     - Backend 行为 / 限制
   * - ``id``
     - string
     - ``""``
     - 动画标识，便于资源阅读和排查
     - 当前 backend 不按 ``id`` 查找、取消或去重动画
   * - ``trigger``
     - string enum: ``manual`` / ``mount`` / ``show`` / ``hide``
     - JSON 解析默认 ``mount``；运行时对象默认 ``manual``
     - 决定何时启动动画
     - ``manual`` 不会被声明式 trigger 自动启动，用于 runtime API
   * - ``property``
     - string enum，见 Property
     - ``opacity``
     - 决定被动画修改的 UI 属性
     - 映射到 backend 对应属性更新器
   * - ``fromMode``
     - string enum: ``absolute`` / ``current``
     - ``absolute``
     - 起始值解析方式
     - ``current`` 在启动动画时读取 backend 当前属性值
   * - ``from``
     - integer
     - ``0``
     - 起始值
     - 作为 backend 动画起始值；当前不支持 ``dp`` / ``sp`` 字符串
   * - ``toMode``
     - string enum: ``absolute`` / ``relative``
     - ``absolute``
     - 结束值解析方式
     - ``relative`` 表示 ``resolvedTo = resolvedFrom + to``
   * - ``to``
     - integer
     - ``0``
     - 结束值
     - 同 ``from``
   * - ``duration``
     - integer ms
     - ``150``
     - 正向动画时长
     - validator 要求非负；backend 按非负时长执行
   * - ``delay``
     - integer ms
     - ``0``
     - 启动延迟
     - validator 要求非负；backend 按非负延迟执行
   * - ``easing``
     - string enum，见 Easing
     - ``linear``
     - 动画曲线
     - 映射到 backend 支持的 easing
   * - ``repeat``
     - integer
     - ``0``
     - 重复次数
     - validator 要求非负
   * - ``playback``
     - bool
     - ``false``
     - 是否反向播放
     - true 时设置 reverse duration，时长等于 ``duration``

.. _gui-interface-json_ui-interaction-animations-sec-05:

触发时机（trigger）
--------------------------

.. list-table::
   :header-rows: 1

   * - Trigger
     - 触发时机
     - 当前行为
   * - ``manual``
     - 不由声明式生命周期触发
     - 供 ``Runtime::start_view_animation*()``、``SystemGui.StartViewAnimation``、``SystemGui.ExecuteBatch`` 这类运行时 API 使用
   * - ``mount``
     - backend 接收并保存节点动画列表时
     - ``apply_animations()`` 中立即调用 ``run_animations(..., Mount)``
   * - ``show``
     - 节点动画列表应用后且节点当前不是 hidden；或后续 ``commonProps.hidden`` 从 true 变为 false
     - 初始未隐藏节点会在 ``mount`` 后立即触发一次 ``show``
   * - ``hide``
     - 后续 ``commonProps.hidden`` 从 false 变为 true
     - 初始就是 hidden 的节点不会在动画列表应用时自动触发 ``hide``

注意：

- 如果同一节点同时声明 ``mount`` 和 ``show`` 动画，且节点初始不是 hidden，两个 trigger 会在初始应用阶段先后启动。
- ``show`` / ``hide`` 依赖 ``commonProps.hidden`` 状态变化，不会因为 backend 对象可见性被外部代码直接修改而自动触发。
- ``click``、``pressed``、``valueChanged`` 等事件可以通过 ``events[].effects`` 中的 ``startAnimation``
  触发 ``manual`` 动画；也可以继续由 app 在 action 回调中调用 runtime API。

.. _gui-interface-json_ui-interaction-animations-sec-06:

动画属性（property）
----------------------------

.. list-table::
   :header-rows: 1

   * - Property
     - ``from`` / ``to`` 单位
     - UI 影响
     - 说明
   * - ``opacity``
     - ``0..255``
     - 修改节点整体透明度
     - ``0`` 透明，``255`` 不透明
   * - ``x``
     - px
     - 修改对象 X 坐标
     - absolute/relative 之外的动画位移由 backend 执行
   * - ``y``
     - px
     - 修改对象 Y 坐标
     - 同 ``x``
   * - ``width``
     - px
     - 修改对象宽度
     - 可能影响后续布局
   * - ``height``
     - px
     - 修改对象高度
     - 可能影响后续布局
   * - ``scale``
     - backend transform scale
     - 修改节点 transform 缩放
     - 当前数值语义以 ``256`` 表示 100%
   * - ``angle``
     - degree
     - 修改通用节点 transform 旋转
     - 单位为度
   * - ``offsetX``
     - px
     - 修改 image 内容横向偏移
     - 主要用于 image 节点，常用于滚动背景
   * - ``offsetY``
     - px
     - 修改 image 内容纵向偏移
     - 主要用于 image 节点

当前 ``from`` / ``to`` 只解析 integer。若希望使用 ``dp`` 或 ``sp``，需要先在常量中换算为裸整数并保证 parser 读到的是 JSON number；字符串 ``"24dp"`` 不适用于动画字段。

.. _gui-interface-json_ui-interaction-animations-sec-07:

取值模式（value mode）
--------------------------------

``fromMode`` 和 ``toMode`` 用于减少 app 在启动动画前的同步回读：

- ``fromMode = "absolute"``：使用 ``from`` 字段。
- ``fromMode = "current"``：启动动画时读取 backend 当前属性值，并把它作为实际起点。
- ``toMode = "absolute"``：使用 ``to`` 字段。
- ``toMode = "relative"``：以实际起点为基准，计算 ``resolvedTo = resolvedFrom + to``。

``fromMode = "relative"`` 和 ``toMode = "current"`` 当前无效，会在 validator 或 service 参数解析阶段报错。

例如让节点从当前 Y 坐标向上移动 48px：

.. code-block:: json

   {
       "property": "y",
       "fromMode": "current",
       "from": 0,
       "toMode": "relative",
       "to": -48,
       "duration": 200,
       "easing": "linear"
   }

.. _gui-interface-json_ui-interaction-animations-sec-08:

缓动（easing）
--------------------

.. list-table::
   :header-rows: 1

   * - JSON 值
     - 含义
   * - ``linear``
     - 线性曲线
   * - ``easeIn``
     - 缓入
   * - ``easeOut``
     - 缓出
   * - ``easeInOut``
     - 缓入缓出
   * - ``overshoot``
     - 过冲
   * - ``bounce``
     - 弹跳
   * - ``step``
     - 阶跃

非法 enum 会在解析或校验阶段报错；``AnimationEasing::Max`` 不允许通过 validator。

.. _gui-interface-json_ui-interaction-animations-sec-09:

示例
--------------------

下面示例演示常见的入场动画：标题在 mount 时淡入，内容容器在 mount 时从下方向当前位置滑入。

.. code-block:: json

   {
       "type": "viewScreen",
       "id": "controls",
       "layout": {
           "type": "flex",
           "gap": "${constant.metrics.pageGap}"
       },
       "placement": {
           "width": "${constant.sizes.match}",
           "height": "${constant.sizes.match}"
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
               },
               "animations": [
                   {
                       "id": "title_fade",
                       "trigger": "mount",
                       "property": "opacity",
                       "from": 0,
                       "to": 255,
                       "duration": 350,
                       "easing": "easeOut"
                   }
               ]
           },
           {
               "type": "container",
               "id": "content",
               "animations": [
                   {
                       "id": "content_slide",
                       "trigger": "mount",
                       "property": "y",
                       "from": 24,
                       "to": 0,
                       "duration": 260,
                       "easing": "easeOut"
                   }
               ]
           }
       ]
   }

显示/隐藏动画示例：

.. code-block:: json

   {
       "type": "container",
       "id": "panel",
       "commonProps": {
           "hidden": true
       },
       "animations": [
           {
               "id": "panel_show",
               "trigger": "show",
               "property": "opacity",
               "from": 0,
               "to": 255,
               "duration": 180,
               "easing": "easeOut"
           },
           {
               "id": "panel_hide",
               "trigger": "hide",
               "property": "opacity",
               "from": 255,
               "to": 0,
               "duration": 120,
               "easing": "easeIn"
           }
       ]
   }

.. _gui-interface-json_ui-interaction-animations-sec-10:

当前限制
--------------------

- 事件 effect 可以通过 ``animationId`` 引用节点自己的 ``animations[]`` 中的动画 ``id``，推荐被引用动画使用
  ``trigger = "manual"``。
- ``id`` 仍不参与声明式 lifecycle trigger 的去重；只作为 event effect / runtime 调用中的引用标识。
- 多个同 trigger 动画会按数组顺序启动；如果多个动画同时修改同一 property，最终效果取决于 backend 动画调度。
- ``from`` / ``to``、``duration``、``delay``、``repeat`` 都是 integer；没有单位字符串解析。
- ``playback`` 只设置 reverse duration，不单独暴露 reverse delay、reverse easing 或 playback repeat。
- ``scale`` 使用 backend transform scale 数值，不是百分比或浮点比例。
