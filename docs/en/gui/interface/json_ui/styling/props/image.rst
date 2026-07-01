.. _gui-interface-json-ui-styling-props-image-sec-00:

Imageprops
====================

:link_to_translation:`zh_CN:[中文]`

Overview
--------------------

``imageProps`` applies to ``image``.

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../../view/image`
- :doc:`../../assets/image`

Field Table
----------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - Binding
     - UI Effect / Limits
   * - ``src``
     - string, must be ``"${image.<id>}"``
     - ``""``
     - ``imageProps.src``
     - the parser resolves it to an image resource id; when written via binding, the store should hold the image resource id directly
   * - ``innerAlign``
     - string enum, see ``innerAlign``
     - ``"default"``
     - no
     - internal image alignment/layout; ``"contain"`` shows the image fully and proportionally within a fixed outer frame, ``"tile"`` works with ``offsetX/offsetY`` for tiled scrolling
   * - ``recolor``
     - string, ``#RRGGBB``
     - ``""``
     - ``imageProps.recolor``
     - image recoloring; an empty string disables recoloring
   * - ``recolorOpacity``
     - integer, ``0..255``
     - ``255``
     - ``imageProps.recolorOpacity``
     - recolor blend strength; ``0`` disables blending, ``255`` uses the target color entirely
   * - ``angle``
     - integer degrees
     - ``0``
     - ``imageProps.angle``
     - the image's local rotation angle, in degrees; for rotation that applies to all nodes, prefer ``commonProps.angle``
   * - ``offsetX``
     - integer px
     - ``0``
     - ``imageProps.offsetX``
     - horizontal offset of the image content; often used for tiled background scrolling
   * - ``offsetY``
     - integer px
     - ``0``
     - ``imageProps.offsetY``
     - vertical offset of the image content
   * - ``zoom``
     - integer
     - ``256``
     - ``imageProps.zoom``
     - the image's local scale value; ``256`` means 1x; for scaling that applies to all nodes, prefer ``commonProps.zoom``
   * - ``pivotX``
     - integer px or percent string
     - ``0``
     - ``imageProps.pivotX``
     - X coordinate of the image's local rotation/scale pivot; for a pivot that applies to all nodes, prefer ``commonProps.pivotX``
   * - ``pivotY``
     - integer px or percent string
     - ``0``
     - ``imageProps.pivotY``
     - Y coordinate of the image's local rotation/scale pivot; for a pivot that applies to all nodes, prefer ``commonProps.pivotY``

Example
--------------------

.. code-block:: json

   "imageProps": {
       "src": "${image.logo}",
       "innerAlign": "tile",
       "recolor": "#2563eb",
       "recolorOpacity": 160,
       "angle": 15,
       "offsetX": -12,
       "zoom": 320,
       "pivotX": "50%",
       "pivotY": "50%"
   }

``recolor`` is independent of ``style.imageOpacity`` / ``style.imageRecolor``: ``imageProps.recolor``
is a node-local override, while ``style.imageRecolor`` lets theme/styleRefs manage icon color uniformly.
If only ``recolor`` is set, ``recolorOpacity`` defaults to ``255``. Writing an empty string restores the original image while keeping the current
``recolorOpacity`` value for later dynamic restoration; if the node also has theme/style-layer recoloring, an empty string clears the local override and falls back to the style-layer effect.

``imageProps.angle`` / ``zoom`` / ``pivotX`` / ``pivotY`` affect only the current image node. For a transform that must apply to all nodes, use ``commonProps.angle`` / ``zoom`` / ``pivotX`` / ``pivotY``.

Inneralign
--------------------

The public values are:

.. list-table::
   :header-rows: 1

   * - Value
     - Behavior
   * - ``"default"``
     - use the backend default image alignment
   * - ``"center"``
     - image content is centered within the image view
   * - ``"contain"``
     - the image is scaled proportionally to fit fully inside the image view; suitable for fixed small frames such as launcher and navigation icons
   * - ``"cover"``
     - the image is scaled proportionally to cover the image view; the overflow may be cropped
   * - ``"stretch"``
     - the image stretches to fill the image view without keeping the original aspect ratio
   * - ``"tile"``
     - the image is tiled and can work with ``offsetX`` / ``offsetY`` for a scrolling background

If the image view fixes both ``placement.width`` and ``placement.height`` but does not set ``innerAlign``,
the backend only sets the outer-frame size and does not perform contain scaling automatically. To show a small icon fully, write it explicitly:

.. code-block:: json

   "imageProps": {
       "src": "${image.icon}",
       "innerAlign": "contain"
   }

For image size behavior, see "Image Size" in :doc:`../placement`.
If the image view uses ``placement.aspectRatio``, that field constrains only the image view's outer frame; how the content is scaled,
cropped, or stretched inside the frame is still decided by ``innerAlign``.
