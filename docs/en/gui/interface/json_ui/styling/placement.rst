.. _gui-interface-json-ui-styling-placement-sec-00:

Placement
==================================

:link_to_translation:`zh_CN:[中文]`

Overview
--------------------

This page explains the semantics of the ``placement`` fields. Public JSON uses ``camelCase``.

This page covers only ``placement``. For how the current node lays out its children, see :doc:`layout`.

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../index`
- :doc:`layout`
- :doc:`props/index`
- :doc:`../runtime`

Mental Model
------------------------

``placement`` only controls how the current node itself is placed by its parent, including:

- width and height
- absolute coordinates
- anchor positioning relative to the parent or to other controls in the current file
- cell info when placed into a parent grid

``placement`` does not control how the current node lays out its children. Child layout lives entirely in ``layout``, see :doc:`layout`.

Dimension
--------------------

``placement.width`` / ``placement.height`` use dimensions:

.. list-table::
   :header-rows: 1

   * - JSON value
     - Parse result
     - Backend semantics
   * - ``120``
     - fixed ``120px``
     - fixed pixel size
   * - ``"120dp"``
     - ``round(120 * Environment.density)``
     - fixed pixels
   * - ``"50%"``
     - ``SizeMode::Percent``, value ``50``
     - use a percentage of the parent's available size
   * - ``"match"``
     - ``SizeMode::Match``
     - match the parent's available size
   * - ``"wrap"`` or default
     - ``SizeMode::Wrap``
     - the size required by content

A ``dp`` string supports only the ``dp`` suffix; a percentage string supports only the ``%`` suffix; a bare number is an already-converted backend pixel and is not multiplied by ``density``.

``placement.x``, ``placement.y``, ``placement.width``, and ``placement.height`` may produce numeric values via constant or env expressions, for example:

.. code-block:: json

   {
       "placement": {
           "x": "${constant.ui.overlay.metric.railWidth}",
           "y": "${constant.ui.overlay.metric.topBarHeight}",
           "width": "${expr(${env.widthDp} - ${constant.ui.overlay.metric.railWidth})}",
           "height": "${expr(${env.heightDp} - ${constant.ui.overlay.metric.topBarHeight})}"
       }
   }

The result of ``${expr(...)}`` must fit the type the field originally accepts: ``x/y`` is usually an integer, ``"Ndp"``, or ``"N%"``,
and ``width/height`` is usually a dimension. For expression syntax, ``${env.*}`` fields, and unit rules, see
:doc:`../assets/constant`.

Placement
--------------------

``placement`` is an optional object. When omitted, the ``gui_interface`` built-in placement baseline is applied first, then overridden by explicit JSON fields.

The general node baseline is:

- ``mode``: ``"absolute"``
- ``width`` / ``height``: ``"wrap"``
- ``x`` / ``y``: ``0``
- ``align``: ``"topLeft"``
- ``relativeTo``: ``""``
- ``gridColumn`` / ``gridRow``: ``0``
- ``gridColumnSpan`` / ``gridRowSpan``: ``1`` (fill in an integer ``>= 1``)
- ``alignSelf``: ``"start"``
- ``flexGrow``: ``0``

``screen`` is the exception:

- if ``screen`` omits ``placement.width`` / ``placement.height`` entirely
- or the ``placement`` object exists but leaves these two fields unset
- the runtime fills the default size to ``"match"``

In other words, this difference is itself a baseline rule, not an extra patch:

- ordinary nodes still default to ``wrap``
- ``screen`` fills the parent display area by default
- if you explicitly write ``"wrap"`` or a fixed size on ``screen``, the explicit value wins

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - UI Effect
     - Backend Behavior / Limits
   * - ``mode``
     - ``flow`` / ``absolute`` / ``relative``
     - ``absolute``
     - How the current node is positioned within its parent
     - ``flow`` does not position actively; ``absolute`` uses x/y; ``relative`` uses an anchor and an optional target node
   * - ``width``
     - dimension
     - ``"wrap"`` for general nodes; ``"match"`` for screen by default
     - current node width
     - the backend applies the final width semantics
   * - ``height``
     - dimension
     - ``"wrap"`` for general nodes; ``"match"`` for screen by default
     - current node height
     - the backend applies the final height semantics
   * - ``aspectRatio``
     - string or number
     - none
     - outer-frame aspect-ratio constraint
     - uses fit short-side semantics; available only when neither ``width`` nor ``height`` is ``wrap``
   * - ``x``
     - number px, ``"Ndp"``, or ``"N%"``
     - ``0``
     - absolute coordinate or relative alignment offset
     - percentage is relative to parent content width
   * - ``y``
     - number px, ``"Ndp"``, or ``"N%"``
     - ``0``
     - absolute coordinate or relative alignment offset
     - percentage is relative to parent content height
   * - ``align``
     - placement align enum, see placement align enum
     - ``topLeft``
     - relative anchor
     - backend anchor enum
   * - ``relativeTo``
     - string
     - ``""``
     - ``${view.<path>}``
     - empty string means relative to the parent; when non-empty it must use a ``${view.*}`` dotted path relative to the current file root
   * - ``gridColumn``
     - integer
     - ``0``
     - the start column when placing the current node into the parent grid
     - participates in grid cell placement when the parent is a grid
   * - ``gridRow``
     - integer
     - ``0``
     - the start row when placing the current node into the parent grid
     - same as ``gridColumn``
   * - ``gridColumnSpan``
     - integer
     - ``1``
     - number of columns the current node spans
     - passed to the backend as is; the component layer does not clamp the range; fill in ``>= 1``
   * - ``gridRowSpan``
     - integer
     - ``1``
     - number of rows the current node spans
     - passed to the backend as is; the component layer does not clamp the range; fill in ``>= 1``
   * - ``alignSelf``
     - align enum, see ``alignSelf``
     - ``start``
     - the current node's self-alignment within the parent grid cell
     - used for both horizontal and vertical grid cell alignment
   * - ``flexGrow``
     - integer
     - ``0``
     - the share of remaining space the current node takes in the parent flex track
     - ``0`` disables it; ``1+`` distributes the remaining space proportionally

Value Description
----------------------------------

Mode
^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1

   * - Value
     - Meaning
   * - ``flow``
     - only sets the size, does not set coordinates; the position is left to the parent flex/grid or the backend default flow position
   * - ``absolute``
     - uses ``x/y`` as absolute coordinates relative to the parent's top-left corner
   * - ``relative``
     - uses ``align`` and an optional ``relativeTo`` for anchor positioning

Placement Align Enum
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``align`` is used only when ``mode = "relative"``.

.. list-table::
   :header-rows: 1

   * - Value
     - Meaning
   * - ``topLeft``
     - top-left corner inside the target area
   * - ``topMid``
     - midpoint of the top edge inside the target area
   * - ``topRight``
     - top-right corner inside the target area
   * - ``bottomLeft``
     - bottom-left corner inside the target area
   * - ``bottomMid``
     - midpoint of the bottom edge inside the target area
   * - ``bottomRight``
     - bottom-right corner inside the target area
   * - ``leftMid``
     - midpoint of the left edge inside the target area
   * - ``rightMid``
     - midpoint of the right edge inside the target area
   * - ``center``
     - center of the target area
   * - ``outTopLeft``
     - outside above the target area, left-aligned
   * - ``outTopMid``
     - outside above the target area, horizontally centered
   * - ``outTopRight``
     - outside above the target area, right-aligned
   * - ``outBottomLeft``
     - outside below the target area, left-aligned
   * - ``outBottomMid``
     - outside below the target area, horizontally centered
   * - ``outBottomRight``
     - outside below the target area, right-aligned
   * - ``outLeftTop``
     - outside to the left of the target area, top-aligned
   * - ``outLeftMid``
     - outside to the left of the target area, vertically centered
   * - ``outLeftBottom``
     - outside to the left of the target area, bottom-aligned
   * - ``outRightTop``
     - outside to the right of the target area, top-aligned
   * - ``outRightMid``
     - outside to the right of the target area, vertically centered
   * - ``outRightBottom``
     - outside to the right of the target area, bottom-aligned

Alignself
^^^^^^^^^^^^^^^^^^^^

``alignSelf`` uses the align enum from :doc:`layout`: ``start``, ``center``, ``end``, ``spaceBetween``,
``spaceAround``, ``spaceEvenly``, ``stretch``. The most common in grid child placement are ``start``, ``center``, ``end``, and
``stretch``; ``stretch`` means fill the grid cell the node sits in.

Flow
--------------------

``flow`` is not the default placement. The backend only sets the size and does not apply coordinates or anchors. If the parent is
flex/grid, the node participates in the parent layout; if the parent has ``layout.type = "none"``, the backend default position is used.

Percent Size
------------------------

``placement.width`` / ``placement.height`` may use percentage strings such as ``"50%"``. The percentage is computed against the parent's available size.
``"100%"`` and ``"match"`` are equivalent for regular placement size, but a percentage can express a finer ratio.

``placement.x`` / ``placement.y`` may also use percentage strings such as ``"50%"``. A percentage offset is computed against the
content area of the current node's parent: ``x`` uses parent content width and ``y`` uses parent content height. ``relativeTo`` only changes the anchor target; the
base for the percentage offset is still the current node's parent.

Aspectratio
----------------------

``placement.aspectRatio`` constrains the node's outer-frame aspect ratio and supports:

.. list-table::
   :header-rows: 1

   * - JSON value
     - Meaning
   * - ``"16:9"``
     - aspect ratio ``16 / 9``
   * - ``"1024:600"``
     - aspect ratio ``1024 / 600``
   * - ``1.7067``
     - use the numeric ratio directly

When the bounds given by ``width`` / ``height`` do not satisfy the ratio, the runtime shrinks one side using **fit short-side** semantics,
ensuring the final outer frame fits entirely within the given bounds. For example, with ``width = 100``, ``height = 80``, ``aspectRatio = "1:1"``,
the final outer frame is ``80 x 80``.

Limits:

- ``aspectRatio`` constrains only the current view's outer frame and does not control child layout.
- ``aspectRatio`` requires that neither ``width`` nor ``height`` is ``wrap``.
- No cover/crop mode is provided; to crop or fill image content, use ``imageProps.innerAlign``.

Absolute
--------------------

``absolute`` uses ``x/y`` as coordinates relative to the parent's top-left corner.

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

Corresponding backend behavior: apply fixed width and height, and position by adding the ``x/y`` offset to the parent's top-left corner.

Relative
--------------------

``relative`` uses anchor positioning. When ``relativeTo`` is empty it is relative to the parent; when non-empty it is relative to a target node in the current file.

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

Limits:

- ``out*`` alignment must set ``relativeTo``, because parent-object alignment on some backends does not support an outside anchor.
- ``relativeTo`` must use ``${view.<path>}``, for example ``${view.anchor}``, ``${view.host.anchor}``.
- ``path`` is relative to the root node of the current JSON file, not to the current node.
- path segments are separated by ``.``; the ``/`` separator is not supported.
- referencing ancestor, sibling, and collateral nodes in the current file is allowed.
- referencing the node itself or its descendants is not allowed.

Grid Child Placement
----------------------------------------

When the parent has ``layout.type = "grid"``, a child decides how it lands in a cell via ``placement.gridColumn`` / ``placement.gridRow`` /
``placement.gridColumnSpan`` / ``placement.gridRowSpan`` / ``placement.alignSelf``.

.. code-block:: json

   {
       "placement": {
           "height": "40dp",
           "gridColumn": 1,
           "gridRow": 0,
           "alignSelf": "stretch"
       }
   }

To make a grid child fill its cell, prefer ``placement.alignSelf: "stretch"``. Do not treat
``placement.width: "match"`` as cell-stretch semantics.

Flex Grow
--------------------

``placement.flexGrow`` controls remaining-space distribution when the parent has ``layout.type = "flex"``; it maps to LVGL
``lv_obj_set_flex_grow()``:

- the default is ``0``, meaning it does not participate in grow.
- ``1`` or a larger integer distributes the remaining space of the current flex track proportionally.
- it is not a size field and does not change the meaning of ``placement.width: "match"``.
- in Figma Auto Layout, main-axis ``Fill container`` should be exported as ``placement.flexGrow: 1``, not as
  ``placement.width: "match"`` or ``placement.height: "match"``.

.. code-block:: json

   {
       "placement": {
           "mode": "flow",
           "width": "wrap",
           "height": "1dp",
           "flexGrow": 1
       }
   }

Image Size
--------------------

A ``type = "image"`` node has extra sizing behavior. The image resource must be referenced via ``imageProps.src: "${image.<id>}"``, and the
``width`` / ``height`` declared in the resource are passed to the backend.

.. list-table::
   :header-rows: 1

   * - Placement size
     - Current behavior
   * - ``width`` and ``height`` not fixed
     - use the image asset's actual ``width`` / ``height`` as the image view size
   * - both ``width`` and ``height`` fixed
     - use the fixed outer-frame size; image content alignment/scaling is decided by ``imageProps.innerAlign``
   * - only ``width`` fixed
     - derive the height by ratio from the source image width
   * - only ``height`` fixed
     - derive the width by ratio from the source image height
   * - using a percentage or ``match``
     - treated as an explicit outer-frame size; not overridden by the image asset's actual width/height

Limits:

- single-side derivation applies only when ``placement.width/height`` is a fixed size greater than 0; ``match`` / percentage / ``wrap`` do not participate in single-side derivation.
- ``aspectRatio`` constrains the image view's outer frame; image content is still decided by ``imageProps.innerAlign``.
- ``innerAlign`` can choose image content layouts such as ``contain``, ``cover``, ``stretch``, ``center``, ``tile``. A small icon with a fixed frame should usually set ``imageProps.innerAlign: "contain"`` explicitly.
- if the image resource has no valid actual width/height, the backend performs no special sizing.
