.. _gui-interface-json-ui-assets-interaction-template-sec-00:

Interactiontemplate
===================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-assets-interaction_template-sec-01:

Overview
--------------------

``interactionTemplate`` reuses events, animations, state styles, and some common props without producing a runtime view; it expands only in the parser stage. After a node references it via ``interactionRefs``, the template's ``commonProps``, ``stateStyles``, ``events``, and ``animations`` are merged into the node.

.. _gui-interface-json_ui-assets-interaction_template-sec-02:

Related Documents
--------------------

- :doc:`index`
- :doc:`../index`
- :doc:`../view/index`

.. _gui-interface-json_ui-assets-interaction_template-sec-03:

Fields
--------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - Required
     - Description
   * - ``type``
     - string
     - none
     - Yes
     - Fixed to ``"interactionTemplate"``
   * - ``id``
     - string
     - none
     - Yes
     - Template id
   * - ``commonProps``
     - object
     - ``{}``
     - No
     - Default common props; node-local fields take precedence
   * - ``stateStyles``
     - object
     - ``{}``
     - No
     - Default state styles; node-local fields take precedence
   * - ``events``
     - object[]
     - ``[]``
     - No
     - Prepended to the node's events
   * - ``animations``
     - object[]
     - ``[]``
     - No
     - Prepended to the node's animations

.. code-block:: json

   {
       "type": "interactionTemplate",
       "id": "press.scale",
       "commonProps": {"pressLock": false, "pivotX": "50%", "pivotY": "50%"},
       "events": [
           {"type": "pressed", "effects": [{"type": "startAnimation", "animationId": "press_down"}]},
           {"type": "released", "effects": [{"type": "startAnimation", "animationId": "press_up"}]}
       ],
       "animations": [
           {"id": "press_down", "trigger": "manual", "property": "scale", "fromMode": "current", "to": 236},
           {"id": "press_up", "trigger": "manual", "property": "scale", "fromMode": "current", "to": 256}
       ]
   }
