.. _gui-interface-json-ui-view-index-sec-00:

视图
========================

:link_to_translation:`en:[English]`

.. _gui-interface-json_ui-view-index-sec-01:

概览
--------------------

本文档负责：

- ``viewScreen`` / ``viewTemplate`` 中普通 view 节点的通用字段
- ``id``
- ``mountMode``
- ``children``
- 控件类型导航与语义入口

本文档不负责字段级细节，例如 ``layout`` / ``placement`` / ``style`` / ``props`` / ``events`` / ``animations``。这些内容请查看对应文档。

.. _gui-interface-json_ui-view-index-sec-02:

相关文档
--------------------

- :doc:`../index`
- :doc:`../assets/index`
- :doc:`../styling/layout`
- :doc:`../styling/placement`
- :doc:`../styling/style`
- :doc:`../styling/props/index`
- :doc:`../interaction/bindings`
- :doc:`../interaction/events`
- :doc:`../interaction/animations`

.. _gui-interface-json_ui-view-index-sec-03:

通用字段表
--------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - 是否必填
     - 说明
   * - ``type``
     - view type enum，见 ``type``
     - 无
     - 是
     - 控件类型，例如 ``container``、``label``、``button``
   * - ``id``
     - string
     - 无
     - 是
     - 当前节点 id
   * - ``mountMode``
     - ``eager`` / ``dynamic``
     - ``eager``
     - 否
     - 仅 ``viewScreen`` 有意义
   * - ``layout``
     - object
     - baseline
     - 否
     - 当前节点如何布局直接子节点
   * - ``placement``
     - object
     - baseline
     - 否
     - 当前节点如何被父节点摆放
   * - ``style``
     - object
     - theme 合并结果
     - 否
     - 视觉样式
   * - ``stateStyles``
     - object
     - ``{}``
     - 否
     - 交互状态下的局部 style 覆盖，见 :ref:`State Styles <gui-interface-json-ui-styling-style-state-styles>`
   * - ``partStyles``
     - object
     - ``{}``
     - 否
     - 控件内部 part（如 ``indicator`` / ``knob``）的局部 style 覆盖，见 :ref:`Part Styles <gui-interface-json-ui-styling-style-part-styles>`
   * - ``styleRefs``
     - string array
     - ``[]``
     - 否
     - 引用当前 Runtime theme 中的命名样式，按数组顺序叠加
   * - ``interactionRefs``
     - string array
     - ``[]``
     - 否
     - 引用 document 内 ``interactionTemplate``，用于复用事件和动画（缺失模板会导致解析失败）
   * - ``events``
     - array
     - ``[]``
     - 否
     - 事件声明
   * - ``animations``
     - array
     - ``[]``
     - 否
     - 声明式动画
   * - ``bindings``
     - object
     - ``{}``
     - 否
     - store 绑定
   * - ``children``
     - array
     - ``[]``
     - 否
     - 子节点列表

完整 props 域请查看 :doc:`../styling/props/index`。

.. _gui-interface-json_ui-view-index-sec-04:

styleRefs
--------------------

``styleRefs`` 用于复用 Runtime theme 中的命名样式。命名样式 key 必须包含 ``.``，例如
``shell.card`` 或 ``shell.pageTitle``。

合成顺序（由低到高优先级）为：内建 theme、当前 theme 的 ``all/控件 type``、``styleRefs``、节点自身
``style``、``stateStyles``、``partStyles``。因此节点自身 ``style`` / ``stateStyles`` / ``partStyles`` 总是拥有更高优先级。
完整分层合成规则见 :doc:`../styling/style`。缺失的 style ref 会被跳过并输出 warning。

.. code-block:: json

   {
       "type": "label",
       "id": "title",
       "styleRefs": ["shell.pageTitle"],
       "labelProps": {
           "text": "Apps"
       }
   }

.. _gui-interface-json_ui-view-index-sec-05:

interactionRefs
------------------------------

``interactionRefs`` 用于复用 document asset 中声明的 ``interactionTemplate``。模板中的 ``events`` / ``animations``
会追加到节点本地声明之前，``commonProps`` / ``stateStyles`` 作为默认值合入；节点本地字段优先。

与 ``styleRefs`` 不同，引用一个不存在的 ``interactionTemplate`` 是 **解析硬错误** （document 解析会失败），而不是跳过并 warning。

.. code-block:: json

   {
       "type": "button",
       "id": "install",
       "interactionRefs": ["press.scale"],
       "events": [{"type": "clicked", "action": "store.install"}]
   }

.. _gui-interface-json_ui-view-index-sec-06:

id
--------------------

- 同一父节点下必须唯一。
- 顶层 ``screen`` 和顶层模板 id 在同一 document 内必须唯一。

.. _gui-interface-json_ui-view-index-sec-07:

mountMode
--------------------

当前只对 ``viewScreen`` 有意义。

.. list-table::
   :header-rows: 1

   * - 值
     - 含义
   * - ``eager``
     - document load 时创建 screen 子树；适合常驻页面或希望启动后立即可见的页面
   * - ``dynamic``
     - 首次 mount 时再创建 screen 子树；适合不常用页面，减少 document 初始加载开销

.. _gui-interface-json_ui-view-index-sec-08:

type
--------------------

普通 view 节点的 ``type`` 决定控件类型，也决定可使用的专属 props 域。screen 和 template 使用 asset type
``viewScreen`` / ``viewTemplate`` 表达，不写作普通子节点。

.. list-table::
   :header-rows: 1

   * - 值
     - 含义
     - 专属 props
   * - ``container``
     - 普通容器
     - 无
   * - ``label``
     - 文本标签
     - ``labelProps``
   * - ``button``
     - 按钮容器
     - 无，按钮文案推荐添加子 ``label``
   * - ``image``
     - 图片控件
     - ``imageProps``
   * - ``textInput``
     - 文本输入框
     - ``textInputProps``
   * - ``slider``
     - 滑条
     - ``rangeProps``
   * - ``switch``
     - 开关
     - ``toggleProps``
   * - ``checkbox``
     - 复选框
     - ``labelProps`` + ``toggleProps``
   * - ``dropdown``
     - 下拉选择
     - ``dropdownProps``
   * - ``progressBar``
     - 进度条
     - ``rangeProps``
   * - ``spinner``
     - 加载指示器
     - 无
   * - ``arc``
     - 弧形进度/旋钮控件
     - ``rangeProps``
   * - ``line``
     - 折线/线段控件
     - ``lineProps``
   * - ``table``
     - 表格控件
     - ``tableProps``
   * - ``keyboard``
     - 屏幕键盘
     - ``keyboardProps``
   * - ``canvas``
     - 画布控件
     - ``canvasProps``
   * - ``frameView``
     - 离屏渲染帧缓冲控件
     - ``frameViewProps``

``type`` 统一使用上表中的 ``camelCase`` 写法。

.. _gui-interface-json_ui-view-index-sec-09:

children
--------------------

``children`` 是可选数组。每个子节点本身仍然是一个普通控件节点，或一个 ``templateRef`` parser 指令。

约束：

- ``children`` 中不允许出现 ``viewScreen``。
- 同一父节点下子节点 ``id`` 必须唯一。
- 子节点可以继续包含自己的 ``children``。
- 可以使用 ``templateRef`` 静态实例化 ``viewTemplate``；模板内部可用 ``slot`` 暴露可替换子树，详见
  :doc:`../assets/view_template`。
- ``placement.relativeTo`` 必须使用 ``${view.*}`` 文件根相对点路径，例如 ``${view.anchor}`` 或 ``${view.host.anchor}``。

.. _gui-interface-json_ui-view-index-sec-10:

Subtype 导航
--------------------

- :doc:`screen`
- :doc:`container`
- :doc:`label`
- :doc:`button`
- :doc:`image`
- :doc:`textInput <text_input>`
- :doc:`slider`
- :doc:`switch`
- :doc:`checkbox`
- :doc:`dropdown`
- :doc:`progressBar <progress_bar>`
- :doc:`spinner`
- :doc:`arc`
- :doc:`line`
- :doc:`table`
- :doc:`keyboard`
- :doc:`canvas`
- :doc:`frameView <frame_view>`

.. toctree::
   :maxdepth: 1

   screen
   container
   frame_view
   label
   image
   button
   checkbox
   switch
   slider
   progress_bar
   arc
   spinner
   dropdown
   text_input
   keyboard
   line
   canvas
   table
