.. _gui-interface-json-ui-interaction-animations-sec-00:

Animations
====================================

:link_to_translation:`zh_CN:[中文]`

Overview
--------------------

This page explains the semantics of the ``animations`` fields. Public JSON uses ``camelCase``, and enum values follow the spellings in this page.

This page covers only ``animations``. For event-trigger semantics see :doc:`events`; for node structure see :doc:`../view/index`.

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../index`
- :doc:`../view/index`
- :doc:`events`

Summary
--------------------

``animations`` is an optional array that can be attached to any view node (such as ``container`` / ``label`` / ``viewScreen``); there is no generic
``type = "view"`` node type. Each array item describes one basic declarative animation.

The current implementation supports:

- triggers: ``manual`` / ``mount`` / ``show`` / ``hide``
- animation properties: ``opacity`` / ``x`` / ``y`` / ``width`` / ``height`` / ``scale`` / ``angle`` / ``offsetX`` / ``offsetY``
- integer ``from`` / ``to``
- ``fromMode = current``: start from the backend's current property value on launch
- ``toMode = relative``: compute the end value relative to the resolved ``from``
- basic easing, delay, repeat, playback

When the backend creates a node, the apply order is: props fields, ``layout``, ``placement`` (then the ``commonProps`` transform such as ``zoom`` / ``angle`` / ``pivot`` is applied once more), ``style``, debug outline, ``animations``, ``events``, then ``bindings`` are subscribed. So when a ``mount`` animation starts, the node's initial props, layout, placement, and style are already applied.

Field Table
----------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - UI Effect
     - Backend Behavior / Limits
   * - ``id``
     - string
     - ``""``
     - animation identifier, for resource readability and troubleshooting
     - the current backend does not look up, cancel, or dedupe animations by ``id``
   * - ``trigger``
     - string enum: ``manual`` / ``mount`` / ``show`` / ``hide``
     - JSON parsing default ``mount``; runtime object default ``manual``
     - decides when to start the animation
     - ``manual`` is not auto-started by declarative triggers; used by the runtime API
   * - ``property``
     - string enum, see Property
     - ``opacity``
     - decides which UI property the animation changes
     - maps to the backend's corresponding property updater
   * - ``fromMode``
     - string enum: ``absolute`` / ``current``
     - ``absolute``
     - how the start value is resolved
     - ``current`` reads the backend's current property value when the animation starts
   * - ``from``
     - integer
     - ``0``
     - start value
     - used as the backend animation start value; ``dp`` / ``sp`` strings are not currently supported
   * - ``toMode``
     - string enum: ``absolute`` / ``relative``
     - ``absolute``
     - how the end value is resolved
     - ``relative`` means ``resolvedTo = resolvedFrom + to``
   * - ``to``
     - integer
     - ``0``
     - end value
     - same as ``from``
   * - ``duration``
     - integer ms
     - ``150``
     - forward animation duration
     - validator requires non-negative; the backend runs with a non-negative duration
   * - ``delay``
     - integer ms
     - ``0``
     - start delay
     - validator requires non-negative; the backend runs with a non-negative delay
   * - ``easing``
     - string enum, see Easing
     - ``linear``
     - animation curve
     - maps to a backend-supported easing
   * - ``repeat``
     - integer
     - ``0``
     - repeat count
     - validator requires non-negative
   * - ``playback``
     - bool
     - ``false``
     - whether to play in reverse
     - when true, sets a reverse duration equal to ``duration``

Trigger Time (Trigger)
--------------------------------------------

.. list-table::
   :header-rows: 1

   * - Trigger
     - Trigger time
     - Current behavior
   * - ``manual``
     - not triggered by the declarative lifecycle
     - for runtime APIs such as ``Runtime::start_view_animation*()``, ``SystemGui.StartViewAnimation``, ``SystemGui.ExecuteBatch``
   * - ``mount``
     - when the backend receives and stores the node's animation list
     - ``apply_animations()`` immediately calls ``run_animations(..., Mount)``
   * - ``show``
     - after the node's animation list is applied and the node is not currently hidden; or later when ``commonProps.hidden`` changes from true to false
     - an initially non-hidden node triggers ``show`` once right after ``mount``
   * - ``hide``
     - later, when ``commonProps.hidden`` changes from false to true
     - a node that is hidden from the start does not auto-trigger ``hide`` when the animation list is applied

Notes:

- if the same node declares both ``mount`` and ``show`` animations and the node is not initially hidden, both triggers fire one after another during the initial apply phase.
- ``show`` / ``hide`` depend on ``commonProps.hidden`` state changes; they do not auto-fire when external code directly changes the backend object's visibility.
- events such as ``click``, ``pressed``, ``valueChanged`` can trigger a ``manual`` animation via ``startAnimation`` in
  ``events[].effects``; the app can also keep calling the runtime API in the action callback.

Animation Properties (Property)
--------------------------------------------------------------

.. list-table::
   :header-rows: 1

   * - Property
     - ``from`` / ``to`` unit
     - UI Effect
     - Description
   * - ``opacity``
     - ``0..255``
     - change the node's overall opacity
     - ``0`` transparent, ``255`` opaque
   * - ``x``
     - px
     - change the object's X coordinate
     - animation displacement beyond absolute/relative is performed by the backend
   * - ``y``
     - px
     - change the object's Y coordinate
     - same as ``x``
   * - ``width``
     - px
     - change the object's width
     - may affect subsequent layout
   * - ``height``
     - px
     - change the object's height
     - may affect subsequent layout
   * - ``scale``
     - backend transform scale
     - change the node's transform scale
     - the current numeric semantics use ``256`` for 100%
   * - ``angle``
     - degree
     - change the general node transform rotation
     - unit is degrees
   * - ``offsetX``
     - px
     - change the image content's horizontal offset
     - mainly for image nodes, often for scrolling backgrounds
   * - ``offsetY``
     - px
     - change the image content's vertical offset
     - mainly for image nodes

Currently ``from`` / ``to`` parse only integers. To use ``dp`` or ``sp``, convert them to a bare integer in a constant first and make sure the parser reads a JSON number; the string ``"24dp"`` does not work for animation fields.

Value Mode
--------------------

``fromMode`` and ``toMode`` reduce the synchronous readback an app would need before starting an animation:

- ``fromMode = "absolute"``: use the ``from`` field.
- ``fromMode = "current"``: read the backend's current property value when the animation starts and use it as the actual start point.
- ``toMode = "absolute"``: use the ``to`` field.
- ``toMode = "relative"``: compute ``resolvedTo = resolvedFrom + to`` from the actual start point.

``fromMode = "relative"`` and ``toMode = "current"`` are currently invalid and raise an error during validator or service parameter parsing.

For example, to move a node up 48px from its current Y coordinate:

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

Easing
--------------------

.. list-table::
   :header-rows: 1

   * - JSON value
     - Meaning
   * - ``linear``
     - linear curve
   * - ``easeIn``
     - ease in
   * - ``easeOut``
     - ease out
   * - ``easeInOut``
     - ease in and out
   * - ``overshoot``
     - overshoot
   * - ``bounce``
     - bounce
   * - ``step``
     - step

An illegal enum raises an error during parsing or validation; ``AnimationEasing::Max`` is not allowed through the validator.

Example
--------------------

The example below demonstrates common entrance animations: the title fades in on mount, and the content container slides in from below to its current position on mount.

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

Show/hide animation example:

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

Current Restrictions
----------------------------------------

- an event effect can reference an animation ``id`` in the node's own ``animations[]`` via ``animationId``; the referenced animation should use
  ``trigger = "manual"``.
- ``id`` still does not take part in declarative-lifecycle trigger dedup; it is only a reference identifier in event effect / runtime calls.
- multiple animations with the same trigger start in array order; if several animations change the same property at once, the final effect depends on backend animation scheduling.
- ``from`` / ``to``, ``duration``, ``delay``, ``repeat`` are all integers; no unit-string parsing.
- ``playback`` only sets a reverse duration; it does not separately expose reverse delay, reverse easing, or playback repeat.
- ``scale`` uses the backend transform scale value, not a percentage or floating-point ratio.
