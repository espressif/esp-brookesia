.. _gui-interface-json-ui-view-index-sec-00:

View
========================

:link_to_translation:`zh_CN:[中文]`

.. _gui-interface-json_ui-view-index-sec-01:

Overview
--------------------

This page covers:

- Common fields of ordinary view nodes inside ``viewScreen`` / ``viewTemplate``
- ``id``
- ``mountMode``
- ``children``
- Control type navigation and semantic entry

This page does not cover field-level details such as ``layout`` / ``placement`` / ``style`` / ``props`` / ``events`` / ``animations``. See the corresponding documents for those.

.. _gui-interface-json_ui-view-index-sec-02:

Related Documents
----------------------------------

- :doc:`../index`
- :doc:`../assets/index`
- :doc:`../styling/layout`
- :doc:`../styling/placement`
- :doc:`../styling/style`
- :doc:`../styling/props/index`
- :doc:`../interaction/bindings`
- :doc:`../interaction/events`
- :doc:`../interaction/animations`

.. _gui-interface-json_ui-view-index-sec-03:

Common Field Table
------------------------------------

.. list-table::
   :header-rows: 1

   * - Key
     - Type
     - Default
     - Required
     - Description
   * - ``type``
     - view type enum, see ``type``
     - none
     - Yes
     - Control type, e.g. ``container``, ``label``, ``button``
   * - ``id``
     - string
     - none
     - Yes
     - Current node id
   * - ``mountMode``
     - ``eager`` / ``dynamic``
     - ``eager``
     - No
     - Meaningful only for ``viewScreen``
   * - ``layout``
     - object
     - baseline
     - No
     - How the current node lays out its direct children
   * - ``placement``
     - object
     - baseline
     - No
     - How the current node is placed by its parent
   * - ``style``
     - object
     - theme merge result
     - No
     - Visual style
   * - ``stateStyles``
     - object
     - ``{}``
     - No
     - Local style overrides for interactive states, see :ref:`State Styles <gui-interface-json-ui-styling-style-state-styles>`
   * - ``partStyles``
     - object
     - ``{}``
     - No
     - Local style overrides for control parts (such as ``indicator`` / ``knob``), see :ref:`Part Styles <gui-interface-json-ui-styling-style-part-styles>`
   * - ``styleRefs``
     - string array
     - ``[]``
     - No
     - References named styles in the current Runtime theme, overlaid in array order
   * - ``interactionRefs``
     - string array
     - ``[]``
     - No
     - References an ``interactionTemplate`` in the document, to reuse events and animations (a missing template fails parsing)
   * - ``events``
     - array
     - ``[]``
     - No
     - Event declarations
   * - ``animations``
     - array
     - ``[]``
     - No
     - Declarative animations
   * - ``bindings``
     - object
     - ``{}``
     - No
     - Store bindings
   * - ``children``
     - array
     - ``[]``
     - No
     - Child node list

For the full props fields, see :doc:`../styling/props/index`.

.. _gui-interface-json_ui-view-index-sec-04:

Stylerefs
--------------------

``styleRefs`` reuses named styles in the Runtime theme. A named style key must contain ``.``, e.g.
``shell.card`` or ``shell.pageTitle``.

The composition order (low to high priority) is: built-in theme, the current theme's ``all``/control type, ``styleRefs``, then the node's own
``style``, ``stateStyles``, ``partStyles``. So a node's own ``style`` / ``stateStyles`` / ``partStyles`` always has higher priority.
For the full layered composition rules, see :doc:`../styling/style`. A missing style ref is skipped with a warning.

.. code-block:: json

   {
       "type": "label",
       "id": "title",
       "styleRefs": ["shell.pageTitle"],
       "labelProps": {
           "text": "Apps"
       }
   }

.. _gui-interface-json_ui-view-index-sec-05:

Interactionrefs
------------------------------

``interactionRefs`` reuses an ``interactionTemplate`` declared in a document asset. The template's ``events`` / ``animations``
are prepended to the node's local declarations, while ``commonProps`` / ``stateStyles`` merge as defaults; node-local fields take precedence.

Unlike ``styleRefs``, referencing a non-existent ``interactionTemplate`` is a **hard parse error** (document parsing fails) rather than a skip-and-warn.

.. code-block:: json

   {
       "type": "button",
       "id": "install",
       "interactionRefs": ["press.scale"],
       "events": [{"type": "clicked", "action": "store.install"}]
   }

.. _gui-interface-json_ui-view-index-sec-06:

Id
--------------------

- Must be unique under the same parent node.
- Top-level ``screen`` and top-level template ids must be unique within the same document.

.. _gui-interface-json_ui-view-index-sec-07:

Mountmode
--------------------

Currently meaningful only for ``viewScreen``.

.. list-table::
   :header-rows: 1

   * - Value
     - Meaning
   * - ``eager``
     - Creates the screen subtree at document load; suitable for resident pages or pages that should be visible right after startup
   * - ``dynamic``
     - Creates the screen subtree on first mount; suitable for rarely used pages to reduce the document's initial load cost

.. _gui-interface-json_ui-view-index-sec-08:

Type
--------------------

For an ordinary view node, ``type`` decides the control type and the dedicated props fields available. Screens and templates are expressed with the asset types
``viewScreen`` / ``viewTemplate`` and are not written as ordinary child nodes.

.. list-table::
   :header-rows: 1

   * - Value
     - Meaning
     - Dedicated props
   * - ``container``
     - Ordinary container
     - none
   * - ``label``
     - Text label
     - ``labelProps``
   * - ``button``
     - Button container
     - None; add a child ``label`` for button text
   * - ``image``
     - Image control
     - ``imageProps``
   * - ``textInput``
     - Text input box
     - ``textInputProps``
   * - ``slider``
     - Slider
     - ``rangeProps``
   * - ``switch``
     - Switch
     - ``toggleProps``
   * - ``checkbox``
     - Checkbox
     - ``labelProps`` + ``toggleProps``
   * - ``dropdown``
     - Dropdown selection
     - ``dropdownProps``
   * - ``progressBar``
     - Progress bar
     - ``rangeProps``
   * - ``spinner``
     - Loading indicator
     - none
   * - ``arc``
     - Arc progress / knob control
     - ``rangeProps``
   * - ``line``
     - Polyline / line-segment control
     - ``lineProps``
   * - ``table``
     - Table control
     - ``tableProps``
   * - ``keyboard``
     - On-screen keyboard
     - ``keyboardProps``
   * - ``canvas``
     - Canvas control
     - ``canvasProps``
   * - ``frameView``
     - Off-screen render framebuffer control
     - ``frameViewProps``

``type`` uses the ``camelCase`` spelling from the table above.

.. _gui-interface-json_ui-view-index-sec-09:

Children
--------------------

``children`` is an optional array. Each child node is itself an ordinary control node or a ``templateRef`` parser directive.

Constraints:

- ``viewScreen`` is not allowed inside ``children``.
- Child node ``id`` must be unique under the same parent.
- A child node can keep its own ``children``.
- Use ``templateRef`` to statically instantiate a ``viewTemplate``; inside a template, ``slot`` exposes replaceable subtrees, see
  :doc:`../assets/view_template`.
- ``placement.relativeTo`` must use a ``${view.*}`` file-root-relative path, e.g. ``${view.anchor}`` or ``${view.host.anchor}``.

.. _gui-interface-json_ui-view-index-sec-10:

Subtype Navigation
------------------------------------

- :doc:`screen`
- :doc:`container`
- :doc:`label`
- :doc:`button`
- :doc:`image`
- :doc:`textInput <text_input>`
- :doc:`slider`
- :doc:`switch`
- :doc:`checkbox`
- :doc:`dropdown`
- :doc:`progressBar <progress_bar>`
- :doc:`spinner`
- :doc:`arc`
- :doc:`line`
- :doc:`table`
- :doc:`keyboard`
- :doc:`canvas`
- :doc:`frameView <frame_view>`

.. toctree::
   :maxdepth: 1

   screen
   container
   frame_view
   label
   image
   button
   checkbox
   switch
   slider
   progress_bar
   arc
   spinner
   dropdown
   text_input
   keyboard
   line
   canvas
   table
