.. _gui-interface-json-ui-styling-layout-sec-00:

Layout
============================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-styling-layout-sec-01:

Overview
--------------------

This page explains the semantics of the ``layout`` fields. Public JSON uses ``camelCase``.

This page covers only ``layout``. For the node's own size, coordinates, anchors, and grid-child placement semantics, see
:doc:`placement`.

.. _gui-interface-json_ui-styling-layout-sec-02:

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../index`
- :doc:`../view/index`
- :doc:`placement`

.. _gui-interface-json_ui-styling-layout-sec-03:

Mental Model
------------------------

``layout`` only controls how the current node lays out its direct children, not how the node itself is placed by its parent. The node's own size,
coordinates, anchors, grid cell, and similar info all live in ``placement``, see :doc:`placement`.

When Runtime view debug is on, the backend only draws an extra debug outline to make control boundaries visible; this layer does not participate in ``layout`` computation.

.. list-table::
   :header-rows: 1

   * - Object
     - Responsibility
     - Backend behavior
   * - ``layout``
     - How the current node lays out its direct children
     - the backend applies auto layout, flex, or grid semantics
   * - ``placement``
     - How the current node is placed by its parent
     - the backend applies size, coordinates, relative anchors, and grid cell semantics

``style`` only handles visual presentation such as color, corner radius, fonts, and spacing. Do not write ``placement`` size or coordinates into ``layout``.

.. _gui-interface-json_ui-styling-layout-sec-04:

Dimension
--------------------

``layout.gridTemplateColumns`` / ``layout.gridTemplateRows`` use dimensions:

.. list-table::
   :header-rows: 1

   * - JSON value
     - Parse result
     - Backend semantics
   * - ``120``
     - fixed ``120px``
     - fixed grid track
   * - ``"120dp"``
     - ``round(120 * Environment.density)``
     - fixed pixels
   * - ``"50%"``
     - ``SizeMode::Percent``, value ``50``
     - the backend can map it to relative track weight
   * - ``"match"``
     - ``SizeMode::Match``
     - fill the remaining space allocated to the track
   * - ``"wrap"`` or default
     - ``SizeMode::Wrap``
     - the size required by content

A ``dp`` string supports only the ``dp`` suffix; a percentage string supports only the ``%`` suffix; a bare number is an already-converted backend pixel and is not multiplied by ``density``.

.. _gui-interface-json_ui-styling-layout-sec-05:

Layout
--------------------

``layout`` is an optional object. When omitted, the ``gui_interface`` built-in layout baseline is applied first, then overridden by explicit JSON fields. The current baseline is equivalent to:

- ``type = "none"``
- ``flexFlow = "column"``
- ``mainAlign = "start"``
- ``crossAlign = "start"``
- ``gap = 0``
- ``gridTemplateColumns = []``
- ``gridTemplateRows = []``

That is, by default the current node does not enable backend auto layout.

.. _gui-interface-json_ui-styling-layout-sec-06:

Field Table
^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - UI Effect
     - Backend Behavior / Limits
   * - ``type``
     - ``none`` / ``flex`` / ``grid``
     - ``none``
     - Decides how the current node lays out its children
     - ``none`` disables auto layout; ``flex`` / ``grid`` enable the corresponding layout
   * - ``gap``
     - number px or ``"Ndp"``
     - ``0``
     - flex/grid item spacing
     - meaningful only for ``flex`` / ``grid``
   * - ``flexFlow``
     - ``row`` / ``column`` / ``rowWrap`` / ``columnWrap``
     - ``column``
     - flex main-axis direction and wrapping
     - meaningful only for ``type=flex``
   * - ``mainAlign``
     - align enum, see align enum
     - ``start``
     - flex main-axis alignment; overall horizontal alignment of grid tracks
     - the backend maps it to main-axis or track alignment by layout type
   * - ``crossAlign``
     - align enum, see align enum
     - ``start``
     - flex cross-axis/track alignment; overall vertical alignment of grid tracks
     - the backend maps it to cross-axis or track alignment by layout type
   * - ``gridTemplateColumns``
     - dimension[]
     - ``[]``
     - grid column track definition
     - each item maps to a backend grid track descriptor
   * - ``gridTemplateRows``
     - dimension[]
     - ``[]``
     - grid row track definition
     - same as ``gridTemplateColumns``

``align enum`` supports ``start``, ``center``, ``end``, ``spaceBetween``, ``spaceAround``, ``spaceEvenly``, ``stretch``.
Among them ``stretch`` is mainly for ``placement.alignSelf``; used in flex alignment it is treated as ``start``.

.. _gui-interface-json_ui-styling-layout-sec-07:

Value Description
----------------------------------

.. _gui-interface-json_ui-styling-layout-sec-08:

Type
^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1

   * - Value
     - Meaning
   * - ``none``
     - Backend auto layout is not enabled; children are positioned by their own ``placement``
   * - ``flex``
     - Enables flex layout, arranging direct children by ``flexFlow``, ``mainAlign``, ``crossAlign``, ``gap``
   * - ``grid``
     - Enables grid layout, building a grid by ``gridTemplateColumns`` / ``gridTemplateRows``

.. _gui-interface-json_ui-styling-layout-sec-09:

Flexflow
^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1

   * - Value
     - Meaning
   * - ``row``
     - Main axis horizontal, left to right, no wrap
   * - ``column``
     - Main axis vertical, top to bottom, no wrap
   * - ``rowWrap``
     - Main axis horizontal; wraps to the next row when space runs out
   * - ``columnWrap``
     - Main axis vertical; wraps to the next column when space runs out

.. _gui-interface-json_ui-styling-layout-sec-10:

Align Enum
^^^^^^^^^^^^^^^^^^^^

``mainAlign``, ``crossAlign``, and ``placement.alignSelf`` share the same set of align values:

.. list-table::
   :header-rows: 1

   * - Value
     - Meaning
   * - ``start``
     - Align to the start edge
   * - ``center``
     - Center alignment
   * - ``end``
     - Align to the end edge
   * - ``spaceBetween``
     - First and last items hug the edges; remaining space is split evenly between middle items
   * - ``spaceAround``
     - Equal outer space on both sides of each item
   * - ``spaceEvenly``
     - Spaces between items and at both ends are all equal
   * - ``stretch``
     - Stretch to fill available space; mainly for grid children's ``placement.alignSelf``

.. _gui-interface-json_ui-styling-layout-sec-11:

None
^^^^^^^^^^^^^^^^^^^^

``none`` disables auto layout for the current node. How children are positioned is entirely decided by their own ``placement``.

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

Flex
^^^^^^^^^^^^^^^^^^^^

``flex`` makes the current node a flex container, affecting its direct children.

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

Corresponding backend behavior: enable flex layout and apply ``flexFlow``, ``mainAlign``, ``crossAlign``, and ``gap``.

If a flex child needs to occupy the remaining space of the current track, use
``flexGrow`` in that child's ``placement``. Do not use ``placement.width: "match"`` to mimic the main-axis ``Fill container`` of Figma Auto Layout;
``match`` is 100% of the parent's size, not flex remaining-space semantics.

.. _gui-interface-json_ui-styling-layout-sec-13:

Grid
^^^^^^^^^^^^^^^^^^^^

``grid`` makes the current node a grid container; ``gridTemplateColumns`` / ``gridTemplateRows`` define tracks. Children go into cells via
``placement.gridColumn`` / ``placement.gridRow``.

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

Corresponding backend behavior: enable grid layout, apply the columns/rows tracks and overall align, and place children into cells via their ``placement.grid*``
fields.

To make a grid child fill its cell, prefer ``placement.alignSelf: "stretch"``. Do not use
``placement.width: "match"`` to mimic cell width; ``match`` is 100% of the parent, not equivalent to the current grid cell.
