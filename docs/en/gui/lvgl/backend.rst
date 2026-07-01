.. _gui-lvgl-backend-sec-00:

LVGL Json UI Backend Notes
====================================================

:link_to_translation:`zh_CN:[中文]`

This page records how ``brookesia_gui_lvgl`` implements the resolved JSON UI model. The JSON UI protocol fields themselves are still governed by
``gui/brookesia_gui_interface/docs/json_ui``; this page only explains how the LVGL backend realizes those fields.

Backend Pump
------------------------

``gui::Runtime::process_backend()`` forwards to the LVGL backend timer processing. In standalone examples or scenarios without a system main loop, call this interface periodically to drive LVGL animations, events, and internal timers.

Image .Bin
--------------------

LVGL backend supports LVGL v9 image ``.bin``:

- ``RuntimeImageResource.primary_src`` or ``imageSet.images[].src`` can point to a ``.bin``.
- ``Runtime::register_image(...)``, when width/height are missing, asks the LVGL backend through the backend metadata hook to read the ``.bin`` header and complete ``width`` / ``height``.
- the ``.bin`` is read into an in-memory descriptor through the backend preload hook during document load.
- the backend caches ``.bin`` content by image path and keeps a reference count; the same path is read only once even when referenced by multiple documents or global images.
- binding updates, ``set_view_src(...)``, and props apply only reuse the preloaded descriptor and do not read files on the dynamic path.

For build-time PNG-to-``.bin`` conversion, see :doc:`image_pack`.

Layout
--------------------

``layout.type`` mapping:

.. list-table::
   :header-rows: 1

   * - JSON value
     - LVGL behavior
   * - ``none``
     - ``lv_obj_set_layout(obj, 0)``
   * - ``flex``
     - ``LV_LAYOUT_FLEX``, and apply ``lv_obj_set_flex_flow()`` / ``lv_obj_set_flex_align()``
   * - ``grid``
     - ``LV_LAYOUT_GRID``, and apply grid descriptor and grid align

In ``layout.gridTemplateColumns`` / ``layout.gridTemplateRows``:

.. list-table::
   :header-rows: 1

   * - Dimension
     - LVGL descriptor
   * - fixed px
     - fixed track
   * - ``match``
     - ``LV_GRID_FR(1)``
   * - percent
     - ``LV_GRID_FR(percent)``, treated as a relative weight
   * - ``wrap``
     - ``LV_GRID_CONTENT``

The backend does not currently expose the LVGL track cross-place parameter separately; ``crossAlign`` applies to both cross place and track cross place.

Placement
--------------------

``placement`` mapping:

.. list-table::
   :header-rows: 1

   * - Field / Mode
     - LVGL behavior
   * - ``width`` / ``height`` fixed
     - ``lv_obj_set_width()`` / ``lv_obj_set_height()``
   * - ``width`` / ``height`` ``match``
     - ``lv_pct(100)``
   * - ``width`` / ``height`` percent
     - ``lv_pct(percent)``
   * - ``width`` / ``height`` ``wrap``
     - ``LV_SIZE_CONTENT``
   * - ``aspectRatio``
     - after sizes are applied, the outer frame is corrected a second time using fit short-edge semantics
   * - ``mode = absolute``
     - ``lv_obj_set_pos()``; percentage ``x/y`` is first converted to px against the parent content area
   * - ``mode = relative``
     - ``lv_obj_align()`` / ``lv_obj_align_to()``; percentage ``x/y`` is still converted against the current parent content area
   * - grid child fields
     - ``lv_obj_set_grid_cell()``

``out*`` placement align requires ``relativeTo``, because LVGL parent-object alignment itself has no outside anchor.

Image sizing:

.. list-table::
   :header-rows: 1

   * - Placement size
     - LVGL backend behavior
   * - ``width`` and ``height`` not fixed
     - use the image asset width/height to set object size and keep the original image ratio
   * - both ``width`` and ``height`` fixed
     - use the fixed outer frame; image content is controlled by ``imageProps.innerAlign``
   * - use percent / ``match``
     - keep the explicit frame size; not overridden by the image asset width/height

Props
--------------------

Commonprops
^^^^^^^^^^^^^^^^^^^^^^

- ``scrollable = true``: adds ``LV_OBJ_FLAG_SCROLLABLE`` and sets scrollbar mode to ``AUTO``.
- ``scrollable = false``: removes ``LV_OBJ_FLAG_SCROLLABLE`` and sets scrollbar mode to ``OFF``.
- ``pressLock = true``: adds ``LV_OBJ_FLAG_PRESS_LOCK`` and keeps locking the current pressed target when the press slides outside the node bounds.
- ``pressLock = false``: removes ``LV_OBJ_FLAG_PRESS_LOCK`` and lets LVGL re-hit the target and fire ``pressLost`` when the press slides out of range.
- ``pivotX`` / ``pivotY`` percentage values refresh the object layout once before applying the transform, avoiding pivot calculation at ``(0, 0)`` when early sizes are not yet updated.

Image Props Inner Align
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1

   * - JSON value
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

Image Props Recolor
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ``recolor = "#RRGGBB"``: calls ``lv_obj_set_style_image_recolor()``.
- ``recolorOpacity = 0..255``: calls ``lv_obj_set_style_image_recolor_opa()``.
- ``recolor = ""``: removes the image node's local image-recolor property and falls back to the style/theme layer effect.

Style Image Recolor
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ``imageRecolor = "#RRGGBB"``: calls ``lv_style_set_image_recolor()``.
- ``imageRecolorOpacity = 0..255``: calls ``lv_style_set_image_recolor_opa()``.
- ``imageRecolor = ""`` or unset: removes the style-layer image-recolor property.

Keyboard Props Mode
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1

   * - JSON value
     - LVGL mode
   * - ``text``
     - ``LV_KEYBOARD_MODE_TEXT_LOWER``
   * - ``upper`` / ``textUpper``
     - ``LV_KEYBOARD_MODE_TEXT_UPPER``
   * - ``number``
     - ``LV_KEYBOARD_MODE_NUMBER``
   * - ``special``
     - ``LV_KEYBOARD_MODE_SPECIAL``

The JSON UI parser/validator rejects unknown modes; on an invalid mode, the backend keeps the currently available layout.

Keyboard Props Target Text Input / Layouts
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``targetTextInput`` resolves to a target ``textInput`` within the same document. The backend does not hand the target to the LVGL default keyboard
handler; instead it caches the target object and writes manually by JSON key role on ``LV_EVENT_VALUE_CHANGED``, so control-key display text or
image resource ids are not inserted as plain characters.

``keyboardProps.layouts`` is converted into the LVGL keyboard map:

- the ``text``, ``upper``, ``number``, and ``special`` layouts each bind to the corresponding LVGL keyboard mode.
- a string key is used directly as the key text.
- a ``role=mode`` key uses the ``mode`` field to switch layout; when the target mode is not in ``allowedModes`` it is drawn in the disabled style and clicks are ignored.
- a key with ``role`` but no ``text`` maps to an LVGL symbol or control key, such as ``backspace``, ``left``, ``right``, ``space``, ``ok``, ``cancel``.
- a key with ``role`` and a set ``text`` uses ``text`` as the fallback display, while ``role`` still determines behavior.
- a key with ``image`` hides its label and centers the registered image resource on ``LV_EVENT_DRAW_TASK_ADDED``;
  ``keyboardProps.iconSize`` controls the target icon size and creates no per-key child object.
- ``width`` maps to the relative width in ``lv_keyboard_set_ctrl_map()``; its range is guaranteed by the JSON UI parser/validator.
- ``keyStyles`` modifies the fill/label color of ``LV_PART_ITEMS`` through a draw-task hook, supporting the normal, control, mode, confirm/cancel, and disabled classes.

Canvas Props Commands
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The current LVGL backend supports:

.. list-table::
   :header-rows: 1

   * - ``type``
     - Behavior
   * - ``fill``
     - fills the canvas background with ``color``
   * - ``pixel``
     - sets the pixel at ``(x, y)``

The ``width`` / ``height`` fields are accepted by the parser but unused by the current LVGL canvas command execution.

Style
--------------------

During style apply, the LVGL backend maintains each node's ``lv_style_t`` and updates the relevant fields by dirty mask. Typical mapping:

.. list-table::
   :header-rows: 1

   * - JSON style fields
     - LVGL operations
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

Font resolution priority:

1. the native LVGL font in ``ResolvedFontSpec.native_fonts`` closest to the requested size.
2. the ``primary_src`` of the JSON/backend font resource, creating a FreeType font along the fallback chain.
3. a built-in font; used as fallback when FreeType or the file is unavailable.

Animations
--------------------

Animations run through LVGL anim. Main property mapping:

.. list-table::
   :header-rows: 1

   * - Property
     - LVGL operations
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

Easing mapping:

.. list-table::
   :header-rows: 1

   * - JSON value
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
