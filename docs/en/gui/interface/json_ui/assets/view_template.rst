.. _gui-interface-json-ui-assets-view-template-sec-00:

Viewtemplate
============

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-assets-view_template-sec-01:

Overview
--------------------

``viewTemplate`` is a reusable template, instantiable dynamically at runtime via ``create_view(...)`` or statically in ``children[]`` via ``templateRef``.

.. _gui-interface-json_ui-assets-view_template-sec-02:

Related Documents
--------------------

- :doc:`index`
- :doc:`../index`
- :doc:`../view/index`

.. _gui-interface-json_ui-assets-view_template-sec-03:

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
     - Fixed to ``"viewTemplate"``
   * - ``id``
     - string
     - none
     - Yes
     - Template id
   * - ``node``
     - object
     - none
     - Yes
     - Template root control; cannot contain ``id`` — the parser injects the root id from the template id or instance id

.. code-block:: json

   {
       "type": "viewTemplate",
       "id": "menu_item",
       "node": {
           "type": "button",
           "children": [
               {"type": "label", "id": "title", "labelProps": {"text": "Item"}}
           ]
       }
   }

.. _gui-interface-json_ui-assets-view_template-sec-04:

Templateref
----------------------

``templateRef`` may only appear in ``children[]``; it is a parser directive, not a runtime control type.

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
     - Fixed to ``"templateRef"``
   * - ``id``
     - string
     - none
     - Yes
     - Instance root node id
   * - ``templateId``
     - string
     - none
     - Yes
     - The ``viewTemplate.id`` to instantiate
   * - ``slots``
     - object
     - ``{}``
     - No
     - Replaces slot content by the template's internal slot id
   * - ``overrides``
     - object
     - ``{}``
     - No
     - Overrides local fields by the template's internal relative path

.. code-block:: json

   {
       "type": "templateRef",
       "id": "wifi_item",
       "templateId": "menu_item",
       "overrides": {
           "title": {
               "labelProps": {"text": "Wi-Fi"}
           }
       }
   }

A template can expose replaceable subtrees with the ``slot`` parser directive in ``children[]``:

.. code-block:: json

   {
       "type": "viewTemplate",
       "id": "settings_card",
       "node": {
           "type": "container",
           "children": [
               {"type": "slot", "id": "content", "children": []}
           ]
       }
   }

``slot.id`` must be non-empty. An instance fills the matching content via ``slots``. When a slot is not provided, the slot's own
``children`` is used as default content; it is empty if there is no default. ``slot`` exists only in the parser stage, not as a runtime control type.
An unknown slot, a duplicate slot id, or a ``slot`` left in the normal view tree causes an error.

``overrides`` keys use a template-internal relative path; an empty string or ``"."`` denotes the template root node. Overrides can only replace local fields;
overriding ``id``, ``type``, ``children``, ``node``, ``templateId``, ``slots``, or ``overrides`` is not allowed. When both ``slots`` and
``overrides`` are present, slots are filled first, then overrides are applied.
