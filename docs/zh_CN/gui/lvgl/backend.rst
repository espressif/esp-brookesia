.. _gui-lvgl-backend-sec-00:

LVGL JSON UI Backend Notes
====================================================

:link_to_translation:`en:[English]`

本文档记录 ``brookesia_gui_lvgl`` 对 JSON UI resolved model 的实现细节。JSON UI 协议字段本身仍以
``gui/brookesia_gui_interface/docs/json_ui`` 为准；这里仅说明 LVGL backend 如何落地这些字段。

Backend Pump
------------------------

``gui::Runtime::process_backend()`` 会转发到 LVGL backend 的 timer 处理。独立 examples 或没有系统主循环的场景，可以周期调用该接口驱动 LVGL 动画、事件和内部 timer。

Image .bin
--------------------

LVGL backend 支持 LVGL v9 image ``.bin``：

- ``RuntimeImageResource.primary_src`` 或 ``imageSet.images[].src`` 可指向 ``.bin``。
- ``Runtime::register_image(...)`` 在宽高缺失时会通过 backend metadata hook 请求 LVGL backend 读取 ``.bin`` header 并补全 ``width`` / ``height``。
- ``.bin`` 会在 document load 阶段通过 backend preload hook 读取到内存 descriptor。
- backend 按 image path 缓存 ``.bin`` 内容并维护引用计数；同一路径被多个 document 或 global image 引用时只读一次。
- binding 更新、``set_view_src(...)`` 和 props apply 只复用已预加载 descriptor，不在动态路径补读文件。

构建期 PNG 转 ``.bin`` 请看 :doc:`image_pack`。

Layout
--------------------

``layout.type`` 映射：

.. list-table::
   :header-rows: 1

   * - JSON 值
     - LVGL 行为
   * - ``none``
     - ``lv_obj_set_layout(obj, 0)``
   * - ``flex``
     - ``LV_LAYOUT_FLEX``，并应用 ``lv_obj_set_flex_flow()`` / ``lv_obj_set_flex_align()``
   * - ``grid``
     - ``LV_LAYOUT_GRID``，并应用 grid descriptor 与 grid align

``layout.gridTemplateColumns`` / ``layout.gridTemplateRows`` 中：

.. list-table::
   :header-rows: 1

   * - Dimension
     - LVGL descriptor
   * - fixed px
     - 固定 track
   * - ``match``
     - ``LV_GRID_FR(1)``
   * - percent
     - ``LV_GRID_FR(percent)``，作为相对权重处理
   * - ``wrap``
     - ``LV_GRID_CONTENT``

当前 backend 没有单独暴露 LVGL track cross place 参数，``crossAlign`` 会同时用于 cross place 和 track cross place。

Placement
--------------------

``placement`` 映射：

.. list-table::
   :header-rows: 1

   * - 字段 / 模式
     - LVGL 行为
   * - ``width`` / ``height`` fixed
     - ``lv_obj_set_width()`` / ``lv_obj_set_height()``
   * - ``width`` / ``height`` ``match``
     - ``lv_pct(100)``
   * - ``width`` / ``height`` percent
     - ``lv_pct(percent)``
   * - ``width`` / ``height`` ``wrap``
     - ``LV_SIZE_CONTENT``
   * - ``aspectRatio``
     - 在尺寸应用后按 fit 短边语义二次修正外框
   * - ``mode = absolute``
     - ``lv_obj_set_pos()``；百分比 ``x/y`` 先按父内容区域换算为 px
   * - ``mode = relative``
     - ``lv_obj_align()`` / ``lv_obj_align_to()``；百分比 ``x/y`` 仍按当前父内容区域换算
   * - grid child fields
     - ``lv_obj_set_grid_cell()``

``out*`` placement align 需要 ``relativeTo``，因为 LVGL 父对象对齐本身没有 outside anchor。

Image sizing：

.. list-table::
   :header-rows: 1

   * - Placement 尺寸
     - LVGL backend 行为
   * - 未固定 ``width`` 和 ``height``
     - 使用 image asset 宽高设置对象尺寸，并使用原始图片比例
   * - 同时固定 ``width`` 和 ``height``
     - 使用固定外框；图片内容由 ``imageProps.innerAlign`` 控制
   * - 使用 percent / ``match``
     - 保留显式外框尺寸；不会被 image asset 宽高覆盖

Props
--------------------

commonProps
^^^^^^^^^^^^^^^^^^^^^^

- ``scrollable = true``：添加 ``LV_OBJ_FLAG_SCROLLABLE``，scrollbar mode 设为 ``AUTO``。
- ``scrollable = false``：移除 ``LV_OBJ_FLAG_SCROLLABLE``，scrollbar mode 设为 ``OFF``。
- ``pressLock = true``：添加 ``LV_OBJ_FLAG_PRESS_LOCK``，按住后滑出节点范围时继续锁定当前 pressed target。
- ``pressLock = false``：移除 ``LV_OBJ_FLAG_PRESS_LOCK``，按住滑出范围时允许 LVGL 重新命中目标并触发 ``pressLost``。
- ``pivotX`` / ``pivotY`` 百分比值在应用 transform 前会刷新一次对象 layout，避免早期尺寸未更新时按 ``(0, 0)`` 计算 pivot。

imageProps.innerAlign
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1

   * - JSON 值
     - LVGL align
   * - ``default``
     - ``LV_IMAGE_ALIGN_DEFAULT``
   * - ``center``
     - ``LV_IMAGE_ALIGN_CENTER``
   * - ``contain``
     - ``LV_IMAGE_ALIGN_CONTAIN``
   * - ``cover``
     - ``LV_IMAGE_ALIGN_COVER``
   * - ``stretch``
     - ``LV_IMAGE_ALIGN_STRETCH``
   * - ``tile``
     - ``LV_IMAGE_ALIGN_TILE``

imageProps.recolor
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ``recolor = "#RRGGBB"``：调用 ``lv_obj_set_style_image_recolor()``。
- ``recolorOpacity = 0..255``：调用 ``lv_obj_set_style_image_recolor_opa()``。
- ``recolor = ""``：移除 image 节点本地 image recolor 属性，回退到 style/theme 层效果。

style.imageRecolor
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ``imageRecolor = "#RRGGBB"``：调用 ``lv_style_set_image_recolor()``。
- ``imageRecolorOpacity = 0..255``：调用 ``lv_style_set_image_recolor_opa()``。
- ``imageRecolor = ""`` 或未设置：移除 style 层 image recolor 属性。

keyboardProps.mode
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1

   * - JSON 值
     - LVGL mode
   * - ``text``
     - ``LV_KEYBOARD_MODE_TEXT_LOWER``
   * - ``upper`` / ``textUpper``
     - ``LV_KEYBOARD_MODE_TEXT_UPPER``
   * - ``number``
     - ``LV_KEYBOARD_MODE_NUMBER``
   * - ``special``
     - ``LV_KEYBOARD_MODE_SPECIAL``

JSON UI parser/validator 会拒绝未知 mode；backend 收到异常 mode 时会保持当前可用布局。

keyboardProps.targetTextInput / layouts
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``targetTextInput`` 会在同一 document 内解析为目标 ``textInput``。Backend 不把目标交给 LVGL 默认 keyboard
handler，而是缓存目标对象并在 ``LV_EVENT_VALUE_CHANGED`` 中按 JSON key role 手动写入，避免控制键显示文本或
图片资源 id 被当作普通字符插入。

``keyboardProps.layouts`` 会被转换为 LVGL keyboard map：

- ``text``、``upper``、``number``、``special`` 四种 layout 分别绑定到对应 LVGL keyboard mode。
- 字符串 key 直接作为按键文本。
- ``role=mode`` 的 key 使用 ``mode`` 字段切换 layout；目标 mode 不在 ``allowedModes`` 时会按 disabled 样式绘制并忽略点击。
- 带 ``role`` 且没有 ``text`` 的 key 会映射到 LVGL symbol 或控制键，例如 ``backspace``、``left``、``right``、``space``、``ok``、``cancel``。
- 带 ``role`` 且设置了 ``text`` 的 key 使用 ``text`` 作为 fallback 显示，``role`` 仍决定行为。
- 带 ``image`` 的 key 会在 ``LV_EVENT_DRAW_TASK_ADDED`` 中隐藏 label 并居中绘制已注册 image resource；
  ``keyboardProps.iconSize`` 控制目标图标尺寸，不创建 per-key 子对象。
- ``width`` 映射到 ``lv_keyboard_set_ctrl_map()`` 的相对宽度，范围由 JSON UI parser/validator 保证。
- ``keyStyles`` 通过 draw-task hook 修改 ``LV_PART_ITEMS`` 的 fill/label 颜色，支持普通、控制、模式、确认/取消和 disabled 类。

canvasProps.commands
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

当前 LVGL backend 支持：

.. list-table::
   :header-rows: 1

   * - ``type``
     - 行为
   * - ``fill``
     - 用 ``color`` 填充 canvas 背景
   * - ``pixel``
     - 在 ``(x, y)`` 设置像素

``width`` / ``height`` 字段会被 parser 接收，但当前 LVGL canvas command 执行未使用。

Style
--------------------

LVGL backend 在 style apply 时维护每个节点的 ``lv_style_t``，并按 dirty mask 更新相关字段。典型映射：

.. list-table::
   :header-rows: 1

   * - JSON style 字段
     - LVGL 操作
   * - ``bgColor``
     - ``lv_style_set_bg_color()`` + ``lv_style_set_bg_opa()``
   * - ``textColor``
     - ``lv_style_set_text_color()``
   * - ``borderColor`` / ``borderWidth``
     - ``lv_style_set_border_color()`` / ``lv_style_set_border_width()``
   * - ``lineColor`` / ``lineWidth``
     - ``lv_style_set_line_color()`` / ``lv_style_set_line_width()``
   * - ``padding*``
     - ``lv_style_set_pad_*()``
   * - ``margin*``
     - ``lv_style_set_margin_*()``
   * - ``shadow*``
     - ``lv_style_set_shadow_*()``
   * - ``opacity``
     - ``lv_style_set_opa()``
   * - ``imageOpacity``
     - ``lv_style_set_image_opa()``

字体解析优先级：

1. ``ResolvedFontSpec.native_fonts`` 中最接近请求字号的 native LVGL font。
2. JSON/backend 字体资源的 ``primary_src``，并按 fallback 链创建 FreeType 字体。
3. 内置字体；FreeType 不可用或文件不可用时回退。

Animations
--------------------

动画通过 LVGL anim 执行。主要 property 映射：

.. list-table::
   :header-rows: 1

   * - Property
     - LVGL 操作
   * - ``opacity``
     - ``lv_obj_set_style_opa()``
   * - ``x`` / ``y``
     - ``lv_obj_set_x()`` / ``lv_obj_set_y()``
   * - ``width`` / ``height``
     - ``lv_obj_set_width()`` / ``lv_obj_set_height()``
   * - ``scale``
     - ``lv_obj_set_style_transform_scale()``
   * - ``angle``
     - ``lv_obj_set_style_transform_rotation(value * 10)``
   * - ``offsetX`` / ``offsetY``
     - ``lv_image_set_offset_x()`` / ``lv_image_set_offset_y()``

Easing 映射：

.. list-table::
   :header-rows: 1

   * - JSON 值
     - LVGL path
   * - ``linear``
     - ``lv_anim_path_linear``
   * - ``easeIn``
     - ``lv_anim_path_ease_in``
   * - ``easeOut``
     - ``lv_anim_path_ease_out``
   * - ``easeInOut``
     - ``lv_anim_path_ease_in_out``
   * - ``overshoot``
     - ``lv_anim_path_overshoot``
   * - ``bounce``
     - ``lv_anim_path_bounce``
   * - ``step``
     - ``lv_anim_path_step``
