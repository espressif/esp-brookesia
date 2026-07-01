.. _gui-interface-json-ui-interaction-events-sec-00:

Events
============================

:link_to_translation:`zh_CN:[中文]`

Overview
--------------------

``events`` is an optional array. Each event item maps a backend event to a runtime action.

Related Documents
----------------------------------

- :doc:`index`
- :doc:`../index`
- :doc:`animations`
- :doc:`../runtime`

Event Item Field Table
--------------------------------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - Required
     - Description
   * - ``type``
     - event type enum
     - ``clicked``
     - no
     - backend event type
   * - ``action``
     - string
     - ``""``
     - no
     - the runtime dispatch action
   * - ``effects``
     - object[]
     - ``[]``
     - no
     - local event effects, executed in declaration order

Event Type
--------------------

.. list-table::
   :header-rows: 1

   * - JSON value
     - Description
   * - ``pressed``
     - pressed
   * - ``pressing``
     - during pressing
   * - ``pressLost``
     - after pressing and sliding off the control; the current press sequence is treated as an invalid click
   * - ``released``
     - released
   * - ``clicked``
     - clicked
   * - ``longPressed``
     - long press
   * - ``longPressedRepeat``
     - long-press repeat
   * - ``focused``
     - focused
   * - ``defocused``
     - lost focus
   * - ``valueChanged``
     - value changed
   * - ``ready``
     - input completed
   * - ``cancel``
     - cancel
   * - ``scroll``
     - scroll
   * - ``gesture``
     - gesture

Payload
--------------------

Runtime events uniformly use a ``boost::json::object payload`` to carry extra data.

.. list-table::
   :header-rows: 1

   * - Source
     - Common event types
     - payload
   * - ``textInput``
     - ``valueChanged``
     - ``{ "text": "..." }``
   * - ``textInput``
     - ``ready``
     - ``{}`` (``ready`` does not carry ``text``)
   * - ``slider`` / ``progressBar`` / ``arc``
     - ``valueChanged``
     - ``{ "value": 45 }``
   * - ``switch`` / ``checkbox``
     - ``valueChanged``
     - ``{ "checked": true }``
   * - ``dropdown``
     - ``valueChanged``
     - ``{ "selectedIndex": 1 }``
   * - nodes that support gestures
     - ``gesture``
     - ``{ "direction": "left" }``, value ``left`` / ``right`` / ``up`` / ``down``
   * - other events without extra data
     - ``clicked``, etc.
     - ``{}``

Subscription Portal
--------------------------------------

- ``Runtime::subscribe_event_action(document_id, action, ...)``: exact match by ``document_id + action``.
- ``Runtime::subscribe_event_action_with_id(document_id, action, ...)``: returns a stable ``subscription_id`` with the same routing semantics.
- ``Runtime::unsubscribe_subscription(subscription_id)``: actively disconnects, by id, an event subscription obtained via ``with_id``.
- ``View::on_event(...)``: subscribe by ``EventType`` once you have the concrete instance; no ``action`` filtering.

Event Effects (Effects)
----------------------------------------------

``effects`` run inside the Runtime and need no app action callback. A failing effect logs a warning and execution continues with subsequent effects;
an illegal schema raises an error during parsing.

> Tip: if a node needs to receive ``pressLost`` when the press slides out of bounds, set ``commonProps.pressLock=false`` on that node.
> The default ``pressLock=true`` tries to keep the pressed target when the finger slides out of the node bounds.

Emitaction
^^^^^^^^^^^^^^^^^^^^

Dispatch a runtime action. When ``requireValidPress=true``, dispatch is skipped if the same press sequence already received ``pressLost``.

.. code-block:: json

   {
       "type": "emitAction",
       "action": "launcher.open",
       "requireValidPress": true
   }

Setproperty
^^^^^^^^^^^^^^^^^^^^^^

Modify a single field of the target view. ``field`` reuses the binding target syntax, for example ``commonProps.hidden``,
``labelProps.text``, ``imageProps.src``, ``imageProps.recolor``, ``imageProps.recolorOpacity``, ``style.opacity``,
``stateStyles.pressed.bgColor``,
``style.imageFontSize``, ``partStyles.indicator.bgGradientColor``, ``partStyles.knob.stateStyles.pressed.bgColor``,
``layout.gap``, ``placement.x``.
The event value of ``placement.x`` / ``placement.y`` can use a ``"50%"`` percentage offset, following the placement page rules.

.. code-block:: json

   {
       "type": "setProperty",
       "target": "self",
       "field": "style.opacity",
       "value": 180
   }

Setproperties
^^^^^^^^^^^^^^^^^^^^^^^^^^

Modify multiple fields in batch. Each ``updates[]`` entry contains ``target``, ``field``, ``value``.

.. code-block:: json

   {
       "type": "setProperties",
       "updates": [
           {"target": "self", "field": "commonProps.zoom", "value": 236},
           {"target": "./label", "field": "style.opacity", "value": 220}
       ]
   }

Startanimation / Stopanimation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Start or stop an animation. ``startAnimation`` can reference a ``trigger=manual`` animation in the target view's ``animations[]`` via
``animationId``, or embed an ``animation`` object inline. ``stopAnimation`` stops, by ``target + animationId``,
an animation that was started and recorded by an event effect.

.. code-block:: json

   {
       "type": "startAnimation",
       "target": "self",
       "animationId": "press_down"
   }

Target
--------------------

.. list-table::
   :header-rows: 1

   * - Form
     - Meaning
   * - ``self`` or omitted
     - current event source view
   * - ``./child/path``
     - relative to the current event source view
   * - ``/absolute/path``
     - absolute path within the current document

Notes:

- within the same document, a statically declared non-empty ``action`` must be globally unique.
- multiple instances generated from the same template definition can share the same ``action``.
- when template instances share an ``action``, prefer ``event.path`` to tell instances apart; ``event.node_id`` distinguishes the triggering node inside the template.
- ``subscribe_event_action(...)`` returns a ``ScopedConnection``; the connection disconnects automatically on destruction.
- ``subscribe_event_action_with_id(...)`` returns a ``SubscriptionId``; it is only a subscription identity and does not affect the ``document_id + action`` routing model.

Example
--------------------

.. code-block:: json

   "events": [
       {
           "type": "valueChanged",
           "action": "preferences.language"
       }
   ]

Press animation example:

.. code-block:: json

   "events": [
       {"type": "pressed", "effects": [{"type": "startAnimation", "animationId": "press_down"}]},
       {"type": "pressLost", "effects": [{"type": "startAnimation", "animationId": "press_up"}]},
       {"type": "clicked", "effects": [{"type": "emitAction", "action": "launcher.open", "requireValidPress": true}]}
   ]
