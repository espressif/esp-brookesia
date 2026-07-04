.. _gui-interface-json-ui-styling-props-common-sec-00:

Commonprops
======================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-styling-props-common-sec-01:

Overview
--------------------

``commonProps`` keeps only the state fields common to all nodes.

A node's outer-frame size is not part of ``commonProps``. Size/box constraints such as ``width``, ``height``, ``aspectRatio`` all live in
:doc:`placement <../placement>`; ``commonProps.zoom`` / ``pivotX`` / ``pivotY`` only handle the transform applied after layout.

.. _gui-interface-json_ui-styling-props-common-sec-02:

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../../view/index`
- :doc:`../placement`

.. _gui-interface-json_ui-styling-props-common-sec-03:

Field Table
----------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - Binding
     - UI Effect / Limits
   * - ``hidden``
     - bool
     - ``false``
     - ``commonProps.hidden``
     - controls the node's hidden state; the show/hide animation triggers on changes to this field
   * - ``disabled``
     - bool
     - ``false``
     - ``commonProps.disabled``
     - controls the disabled state
   * - ``clickable``
     - bool
     - control-type baseline
     - ``commonProps.clickable``
     - controls whether the node receives click-type events
   * - ``scrollable``
     - bool
     - control-type baseline
     - ``commonProps.scrollable``
     - controls whether scrolling is allowed
   * - ``pressLock``
     - bool
     - ``true``
     - ``commonProps.pressLock``
     - controls whether the node stays locked as the pressed target when the press slides outside the node bounds
   * - ``angle``
     - integer degrees
     - ``0``
     - ``commonProps.angle``
     - general node rotation angle
   * - ``zoom``
     - integer
     - ``256``
     - ``commonProps.zoom``
     - general node scale value; ``256`` means 1x
   * - ``pivotX``
     - integer px or percent string
     - ``0``
     - ``commonProps.pivotX``
     - X coordinate of the general rotation/scale pivot; e.g. ``"50%"`` is half the node width
   * - ``pivotY``
     - integer px or percent string
     - ``0``
     - ``commonProps.pivotY``
     - Y coordinate of the general rotation/scale pivot; e.g. ``"50%"`` is half the node height

.. _gui-interface-json_ui-styling-props-common-sec-04:

Baseline Default Value
--------------------------------------------

The default of ``clickable`` comes from the built-in baseline:

- ``label`` defaults to ``false``
- other regular nodes such as ``screen``, ``container``, ``button``, ``image`` default to ``true``

The default of ``scrollable`` also comes from the built-in baseline:

- ``screen``, ``container`` default to ``true``
- other regular nodes such as ``label``, ``button``, ``image`` default to ``false``

.. _gui-interface-json_ui-styling-props-common-sec-05:

Example
--------------------

.. code-block:: json

   "commonProps": {
       "hidden": true,
       "disabled": false,
       "clickable": true,
       "scrollable": false,
       "pressLock": true,
       "angle": 15,
       "zoom": 256,
       "pivotX": "50%",
       "pivotY": "50%"
   }

``scrollable: true`` asks the backend to enable scrolling; ``scrollable: false`` asks the backend to disable scrolling.

``pressLock: true`` means that even if the pointer leaves the node bounds after a press, the backend tries to keep this node as the current pressed target.
If a control needs to receive ``pressLost`` when the press slides out of bounds and cancel the subsequent effective click, set ``pressLock: false``.

``pivotX`` / ``pivotY`` accept a bare integer px or a percentage string. A percentage is computed against the node's current size: ``pivotX`` uses node width, ``pivotY`` uses node height.
